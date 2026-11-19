#include "GemmiModelIO.h"

#include <gemmi/mmread_gz.hpp>
#include <gemmi/gz.hpp>
#include <gemmi/model.hpp>
#include <gemmi/metadata.hpp>
#include <gemmi/symmetry.hpp>
#include <gemmi/polyheur.hpp>
#include <gemmi/align.hpp>
#include <gemmi/to_mmcif.hpp>
#include <gemmi/to_cif.hpp>

#include <ui/Logger.h>
#include "MIAtom.h"
#include "MIMolIOBase.h"
#include "Monomer.h"
#include "model.h"
#include "mol_util.h"
#include "atom_util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>

namespace chemlib
{

namespace
{

std::string toLowerCopy(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool endsWithIgnoreCase(const std::string &str, const char *suffix)
{
    std::string lstr = toLowerCopy(str);
    std::string lsuf = toLowerCopy(suffix);
    if (lsuf.size() > lstr.size())
        return false;
    return lstr.compare(lstr.size() - lsuf.size(), lsuf.size(), lsuf) == 0;
}

bool hasCifExtension(const std::string &path)
{
    return endsWithIgnoreCase(path, ".cif")
        || endsWithIgnoreCase(path, ".mmcif")
        || endsWithIgnoreCase(path, ".cif.gz")
        || endsWithIgnoreCase(path, ".mmcif.gz");
}

enum class CifContentKind
{
    Unknown,
    Coordinates,
    Reflections
};

CifContentKind inspectCifContent(const std::string &path)
{
    if (!hasCifExtension(path))
        return CifContentKind::Unknown;

    try
    {
        gemmi::MaybeGzipped input(path);
        std::unique_ptr<gemmi::AnyStream> stream = input.create_stream();
        if (!stream)
            return CifContentKind::Unknown;

        char line[1024];
        while (stream->gets(line, sizeof(line)) != nullptr)
        {
            const char *p = line;
            while (*p && std::isspace(static_cast<unsigned char>(*p)))
                ++p;
            if (*p != '_')
                continue;

            // Ligand dictionaries (_chem_comp_atom.) are intentionally not coordinates.
            if (strncmp(p, "_atom_site.", 11) == 0)
            {
                return CifContentKind::Coordinates;
            }
            if (strncmp(p, "_refln.", 7) == 0 ||
                strncmp(p, "_refln_", 7) == 0 ||
                strncmp(p, "_diffrn_refln.", 14) == 0)
            {
                return CifContentKind::Reflections;
            }
        }
    }
    catch (...)
    {
        return CifContentKind::Unknown;
    }
    return CifContentKind::Unknown;
}

} // anonymous namespace

bool IsCifCoordinateFile(const std::string &path)
{
    if (!hasCifExtension(path))
        return false;
    CifContentKind kind = inspectCifContent(path);
    return kind == CifContentKind::Coordinates;
}

bool IsCifReflectionFile(const std::string &path)
{
    if (!hasCifExtension(path))
        return false;
    CifContentKind kind = inspectCifContent(path);
    return kind == CifContentKind::Reflections;
}

bool IsGemmiCoordinateFile(const std::string &path)
{
    if (endsWithIgnoreCase(path, ".pdb.gz") ||
        endsWithIgnoreCase(path, ".ent.gz"))
    {
        return true;
    }
    return IsCifCoordinateFile(path);
}

namespace
{

Residue *loadGemmiStructureImpl(const std::string &path,
                                std::vector<Bond> *connects,
                                GemmiStructureMetadata *out_meta,
                                Residue *&first_res)
{
    if (connects)
        connects->clear();

    gemmi::Structure st;
    try
    {
        st = gemmi::read_structure_gz(path);
    }
    catch (const std::exception &e)
    {
        Logger::log("Gemmi error reading %s: %s", path.c_str(), e.what());
        Logger::message(::format("Error reading structure file %s:\n%s", path.c_str(), e.what()));
        return nullptr;
    }

    if (st.models.empty())
    {
        Logger::message("Error: No models found in file!");
        return nullptr;
    }

    if (st.models.size() > 1)
    {
        Logger::log("%s contains %zu models; loading only the first", path.c_str(), st.models.size());
    }
    const gemmi::Model &model = st.first_model();

    Residue *curr_res = nullptr;
    int atom_serial = 0;

    std::map<const gemmi::Atom*, MIAtom*> atom_map;
    std::map<int, MIAtom*> serial_map;
    std::map<const gemmi::Residue*, Residue*> res_map;

    std::map<char, int> char_segment_counts;

    for (const gemmi::Chain &chain : model.chains)
    {
        char chain_char = chain.name.empty() ? ' ' : chain.name[0];
        int seg_idx = char_segment_counts[chain_char]++;
        int bonding_group = ((seg_idx & 0xFF) << 8) | (static_cast<unsigned char>(chain_char));

        for (const gemmi::Residue &gres : chain.residues)
        {
            if (gres.atoms.empty())
                continue;

            std::vector<MIAtom*> res_atoms;
            res_atoms.reserve(gres.atoms.size());

            for (const gemmi::Atom &gat : gres.atoms)
            {
                if (!std::isfinite(gat.pos.x) || !std::isfinite(gat.pos.y) || !std::isfinite(gat.pos.z))
                    continue;

                MIAtom *atom = new MIAtom();
                atom->setName(gat.name.c_str());

                if (MIGetColorSetter())
                {
                    (*MIGetColorSetter())(atom);
                }

                atom->setPosition(static_cast<float>(gat.pos.x),
                                  static_cast<float>(gat.pos.y),
                                  static_cast<float>(gat.pos.z));
                atom->setOcc(gat.occ);
                float bval = (gat.b_iso < 0.0f) ? 15.0f : gat.b_iso;
                atom->setBValue(bval);
                atom->set_radius_type(0);
                atom->setType(MIAtom::MIGetAtomTypeFromName(atom->name()));
                atom->setAltloc(gat.altloc ? gat.altloc : ' ');

                if (gat.element != gemmi::El::X)
                {
                    atom->setAtomicnumber(static_cast<short>(gat.element.atomic_number()));
                }
                else
                {
                    atom->setAtomicnumber(static_cast<short>(Atomic_Number(gat.name.c_str())));
                }

                ++atom_serial;
                int serial = (gat.serial > 0) ? gat.serial : atom_serial;
                atom->setAtomnumber(serial);
                atom->setSymmop(0);
                atom->setCharge(static_cast<float>(gat.charge));
                atom->set_formal_charge(static_cast<int>(gat.charge));

                if (gat.aniso.nonzero())
                {
                    atom->newAnisotropicity();
                    atom->U(0, gat.aniso.u11);
                    atom->U(1, gat.aniso.u22);
                    atom->U(2, gat.aniso.u33);
                    atom->U(3, gat.aniso.u12);
                    atom->U(4, gat.aniso.u13);
                    atom->U(5, gat.aniso.u23);
                }

                res_atoms.push_back(atom);
                atom_map[&gat] = atom;
                serial_map[serial] = atom;
            }

            if (res_atoms.empty())
                continue;

            Residue *new_res = new Residue();
            if (!first_res)
            {
                first_res = new_res;
                curr_res = new_res;
                curr_res->set_linkage_type(NTERMINUS);
            }
            else
            {
                curr_res = curr_res->insertResidue(new_res);
                curr_res->set_linkage_type(MIDDLE);
            }

            std::string seq_str = gres.seqid.str();
            if (seq_str.empty())
                seq_str = "1";
            curr_res->setName(seq_str);
            curr_res->setType(gres.name);
            curr_res->set_chain_id_str(chain.name.empty() ? std::string(" ") : chain.name);
            curr_res->set_chain_id(static_cast<unsigned short>(bonding_group));
            curr_res->setSecstr('U');

            for (MIAtom *a : res_atoms)
            {
                curr_res->addAtom(a);
            }

            res_map[&gres] = curr_res;
        }
    }

    if (!first_res || atom_serial == 0)
    {
        Logger::message("Error: No atoms found in file!");
        return nullptr;
    }

    curr_res->set_linkage_type(CTERMINUS);

    // Populate connectivity from _struct_conn (st.connections) and CONECT (st.conect_map)
    if (connects)
    {
        std::set<std::pair<MIAtom*, MIAtom*>> seen_bonds;
        auto addUniqueConnect = [&](MIAtom *a1, MIAtom *a2) {
            if (!a1 || !a2 || a1 == a2)
                return;
            std::pair<MIAtom*, MIAtom*> key = (a1 < a2) ? std::make_pair(a1, a2) : std::make_pair(a2, a1);
            if (seen_bonds.insert(key).second)
            {
                Bond connect;
                connect.setAtom1(a1);
                connect.setAtom2(a2);
                connects->push_back(connect);
            }
        };

        for (const gemmi::Connection &conn : st.connections)
        {
            if (conn.asu == gemmi::Asu::Different || (conn.type != gemmi::Connection::Covale && conn.type != gemmi::Connection::Disulf))
                continue;

            gemmi::const_CRA cra1 = model.find_cra(conn.partner1, true);
            gemmi::const_CRA cra2 = model.find_cra(conn.partner2, true);
            if (cra1.atom && cra2.atom)
            {
                auto it1 = atom_map.find(cra1.atom);
                auto it2 = atom_map.find(cra2.atom);
                if (it1 != atom_map.end() && it2 != atom_map.end())
                {
                    addUniqueConnect(it1->second, it2->second);
                }
            }
        }

        for (const auto &kv : st.conect_map)
        {
            auto it1 = serial_map.find(kv.first);
            if (it1 == serial_map.end())
                continue;
            for (int target_serial : kv.second)
            {
                auto it2 = serial_map.find(target_serial);
                if (it2 != serial_map.end())
                {
                    addUniqueConnect(it1->second, it2->second);
                }
            }
        }
    }

    // Secondary structure: Helices (_struct_conf) and Sheets (_struct_sheet_range)
    int nsecstr = 0;
    auto markSecStrRange = [&](const gemmi::AtomAddress &startAddr, const gemmi::AtomAddress &endAddr, char secType) {
        gemmi::const_CRA startCra = model.find_cra(startAddr, true);
        gemmi::const_CRA endCra = model.find_cra(endAddr, true);
        if (!startCra.residue || !endCra.residue)
            return;

        auto itStart = res_map.find(startCra.residue);
        auto itEnd = res_map.find(endCra.residue);
        if (itStart == res_map.end() || itEnd == res_map.end())
            return;

        Residue *r = itStart->second;
        Residue *rEnd = itEnd->second;
        while (r != nullptr)
        {
            r->setSecstr(secType);
            if (r == rEnd)
                break;
            r = r->next();
        }
        ++nsecstr;
    };

    for (const gemmi::Helix &helix : st.helices)
    {
        markSecStrRange(helix.start, helix.end, 'H');
    }

    for (const gemmi::Sheet &sheet : st.sheets)
    {
        for (const gemmi::Sheet::Strand &strand : sheet.strands)
        {
            markSecStrRange(strand.start, strand.end, 'S');
        }
    }

    if (out_meta)
    {
        *out_meta = GemmiStructureMetadata();
        out_meta->has_secstr = (nsecstr > 0);

        std::string title = st.get_info("_struct.title");
        if (title.empty())
            title = st.get_info("_entry.id");
        if (title.empty())
            title = st.name;
        out_meta->compound = title;

        for (size_t i = 0; i < st.meta.authors.size(); ++i)
        {
            if (i > 0)
                out_meta->author += "; ";
            out_meta->author += st.meta.authors[i];
        }

        if (st.cell.is_crystal())
        {
            out_meta->is_crystal = true;
            out_meta->a = static_cast<float>(st.cell.a);
            out_meta->b = static_cast<float>(st.cell.b);
            out_meta->c = static_cast<float>(st.cell.c);
            out_meta->alpha = static_cast<float>(st.cell.alpha);
            out_meta->beta = static_cast<float>(st.cell.beta);
            out_meta->gamma = static_cast<float>(st.cell.gamma);
            out_meta->spacegroup_hm = st.spacegroup_hm;

            if (const gemmi::SpaceGroup *sg = st.find_spacegroup())
            {
                out_meta->spacegroup_number = sg->number;
                out_meta->spacegroup_pdb_name = sg->pdb_name();
                if (out_meta->spacegroup_hm.empty())
                    out_meta->spacegroup_hm = sg->hm;
            }

            if (st.cell.explicit_matrices)
            {
                out_meta->has_explicit_frac = true;
                for (int r = 0; r < 3; ++r)
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        out_meta->ctof[r][c] = static_cast<float>(st.cell.frac.mat[r][c]);
                    }
                }
            }
        }
    }

    return first_res;
}

} // anonymous namespace

Residue *LoadGemmiStructure(const std::string &path,
                            std::vector<Bond> *connects,
                            GemmiStructureMetadata *out_meta)
{
    Residue *first_res = nullptr;
    try
    {
        return loadGemmiStructureImpl(path, connects, out_meta, first_res);
    }
    catch (const std::exception &e)
    {
        Logger::log("Gemmi error processing %s: %s", path.c_str(), e.what());
        Logger::message(::format("Error processing structure file %s:\n%s", path.c_str(), e.what()));
        if (first_res)
            FreeResidueList(first_res);
        if (connects)
            connects->clear();
        return nullptr;
    }
}

namespace
{

std::string cifBlockNameFromPath(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");
    std::string stem = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = stem.find('.');
    if (dot != std::string::npos)
        stem.erase(dot);
    std::string out;
    for (char ch : stem)
    {
        unsigned char c = static_cast<unsigned char>(ch);
        out += (std::isalnum(c) || ch == '_' || ch == '-') ? ch : '_';
    }
    return out.empty() ? std::string("MIFIT") : out;
}

// Residue::name() holds the sequence number with an optional trailing insertion code, e.g. "12A".
gemmi::SeqId parseSeqId(const std::string &name)
{
    const char *s = name.c_str();
    char *end = nullptr;
    long num = std::strtol(s, &end, 10);
    if (end == s)
        return gemmi::SeqId(1, ' ');
    char icode = ' ';
    while (*end == ' ')
        ++end;
    if (*end != '\0')
        icode = *end;
    return gemmi::SeqId(static_cast<int>(num), icode);
}

std::string gemmiChainName(const Residue *res)
{
    std::string name = res->chain_id_str();
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    if (name.empty())
    {
        char c = static_cast<char>(res->chain_id() & 255);
        if (c != ' ' && c != '\0')
            name = std::string(1, c);
    }
    return name.empty() ? std::string("A") : name;
}

} // anonymous namespace

bool SaveGemmiStructure(const std::string &path,
                        Residue *res,
                        const std::vector<Bond> *connects,
                        const GemmiStructureMetadata *meta,
                        bool visibleOnly)
{
    try
    {
        gemmi::Structure st;
        st.name = cifBlockNameFromPath(path);

        if (meta)
        {
            if (meta->is_crystal)
            {
                auto round4 = [](double v) { return std::round(v * 10000.0) / 10000.0; };
                st.cell.set(round4(meta->a), round4(meta->b), round4(meta->c),
                            round4(meta->alpha), round4(meta->beta), round4(meta->gamma));
                st.spacegroup_hm = !meta->spacegroup_pdb_name.empty() ? meta->spacegroup_pdb_name
                                                                      : meta->spacegroup_hm;
            }
            if (!meta->compound.empty())
                st.info["_struct.title"] = meta->compound;
            if (!meta->author.empty())
            {
                size_t start = 0;
                while (start <= meta->author.size())
                {
                    size_t comma = meta->author.find(';', start);
                    if (comma == std::string::npos)
                        comma = meta->author.size();
                    std::string a = meta->author.substr(start, comma - start);
                    size_t b = a.find_first_not_of(' ');
                    size_t e = a.find_last_not_of(' ');
                    if (b != std::string::npos)
                        st.meta.authors.push_back(a.substr(b, e - b + 1));
                    start = comma + 1;
                }
            }
        }

        st.models.emplace_back(1);
        gemmi::Model &model = st.models.back();

        std::map<std::string, size_t> chain_index;
        std::map<const MIAtom*, gemmi::AtomAddress> addresses;
        std::map<const MIAtom*, std::string> restype;
        int serial = 0;

        for (Residue *r = res; r != nullptr; r = r->next())
        {
            std::string cname = gemmiChainName(r);
            auto it = chain_index.find(cname);
            if (it == chain_index.end())
            {
                model.chains.emplace_back(cname);
                it = chain_index.emplace(cname, model.chains.size() - 1).first;
            }

            gemmi::Residue gres;
            gres.seqid = parseSeqId(r->name());
            gres.name = r->type();
            gres.het_flag = (IsPeptide(*r) || IsNucleic(r)) ? 'A' : 'H';

            std::vector<std::pair<const MIAtom*, gemmi::Atom*>> pending;
            for (int i = 0; i < r->atomCount(); ++i)
            {
                MIAtom *a = r->atom(i);
                if (visibleOnly && a->isHidden())
                    continue;

                gemmi::Atom gat;
                gat.name = a->name();
                gat.altloc = (a->altloc() == ' ') ? '\0' : a->altloc();
                int z = a->atomicnumber();
                gat.element = (z > 0) ? gemmi::Element(z) : gemmi::Element(gemmi::El::X);
                int charge = a->formal_charge();
                gat.charge = static_cast<signed char>(std::max(-8, std::min(8, charge)));
                gat.serial = ++serial;
                gat.pos = gemmi::Position(a->x(), a->y(), a->z());
                gat.occ = a->occ();
                gat.b_iso = a->BValue();
                if (a->hasAnisotropicity())
                {
                    gat.aniso.u11 = a->U(0);
                    gat.aniso.u22 = a->U(1);
                    gat.aniso.u33 = a->U(2);
                    gat.aniso.u12 = a->U(3);
                    gat.aniso.u13 = a->U(4);
                    gat.aniso.u23 = a->U(5);
                }
                addresses[a] = gemmi::AtomAddress(cname, gres.seqid, gres.name, gat.name, gat.altloc);
                restype[a] = gres.name;
                gres.atoms.push_back(std::move(gat));
            }
            if (gres.atoms.empty())
                continue;
            model.chains[it->second].residues.push_back(std::move(gres));
        }

        if (serial == 0)
        {
            Logger::message("Save failed! No atoms to write.");
            return false;
        }

        if (connects)
        {
            int ncovale = 0;
            int ndisulf = 0;
            for (const Bond &b : *connects)
            {
                auto it1 = addresses.find(b.getAtom1());
                auto it2 = addresses.find(b.getAtom2());
                if (it1 == addresses.end() || it2 == addresses.end())
                    continue;
                gemmi::Connection conn;
                bool disulf = std::strcmp(b.getAtom1()->name(), "SG") == 0
                              && std::strcmp(b.getAtom2()->name(), "SG") == 0
                              && restype[b.getAtom1()] == "CYS" && restype[b.getAtom2()] == "CYS";
                conn.type = disulf ? gemmi::Connection::Disulf : gemmi::Connection::Covale;
                conn.name = disulf ? ::format("disulf%d", ++ndisulf) : ::format("covale%d", ++ncovale);
                conn.partner1 = it1->second;
                conn.partner2 = it2->second;
                st.connections.push_back(conn);
            }
        }

        gemmi::setup_entities(st);
        gemmi::assign_label_seq_id(st, false);

        gemmi::cif::Document doc = gemmi::make_mmcif_document(st);
        std::ofstream os(path.c_str(), std::ios::out | std::ios::trunc);
        if (!os)
        {
            Logger::message("Save failed! Unable to open output file. Disk full?");
            return false;
        }
        gemmi::cif::write_cif_to_stream(os, doc);
        os.close();
        if (os.fail())
        {
            Logger::message("Save failed! Error writing output file. Disk full?");
            return false;
        }
        return true;
    }
    catch (const std::exception &e)
    {
        Logger::log("Gemmi error writing %s: %s", path.c_str(), e.what());
        Logger::message(::format("Error writing structure file %s:\n%s", path.c_str(), e.what()));
        return false;
    }
}

} // namespace chemlib
