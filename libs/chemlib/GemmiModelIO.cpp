#include "GemmiModelIO.h"

#include <gemmi/mmread_gz.hpp>
#include <gemmi/gz.hpp>
#include <gemmi/model.hpp>
#include <gemmi/metadata.hpp>
#include <gemmi/symmetry.hpp>

#include <ui/Logger.h>
#include "MIAtom.h"
#include "MIMolIOBase.h"
#include "Monomer.h"
#include "model.h"
#include "mol_util.h"
#include "atom_util.h"

#include <algorithm>
#include <cctype>
#include <cstring>
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
        bool sawRefln = false;
        int linesRead = 0;
        const int maxLines = 20000;
        while (linesRead < maxLines && stream->gets(line, sizeof(line)) != nullptr)
        {
            ++linesRead;
            const char *p = line;
            while (*p && std::isspace(static_cast<unsigned char>(*p)))
                ++p;
            if (*p != '_')
                continue;

            if (strncmp(p, "_atom_site.", 11) == 0 ||
                strncmp(p, "_chem_comp_atom.", 16) == 0)
            {
                return CifContentKind::Coordinates;
            }
            if (strncmp(p, "_refln.", 7) == 0 ||
                strncmp(p, "_refln_", 7) == 0 ||
                strncmp(p, "_diffrn_refln.", 14) == 0)
            {
                sawRefln = true;
            }
        }
        if (sawRefln)
            return CifContentKind::Reflections;
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
    if (endsWithIgnoreCase(path, ".mmcif") ||
        endsWithIgnoreCase(path, ".mmcif.gz") ||
        endsWithIgnoreCase(path, ".cif.gz") ||
        endsWithIgnoreCase(path, ".pdb.gz") ||
        endsWithIgnoreCase(path, ".ent.gz"))
    {
        return !IsCifReflectionFile(path);
    }
    if (endsWithIgnoreCase(path, ".cif"))
    {
        return IsCifCoordinateFile(path);
    }
    return false;
}

Residue *LoadGemmiStructure(const std::string &path,
                            std::vector<Bond> *connects,
                            GemmiStructureMetadata *out_meta)
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

    const gemmi::Model &model = st.first_model();

    Residue *first_res = nullptr;
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
                if (gat.pos.x >= 9999.0 || gat.pos.y >= 9999.0 || gat.pos.z >= 9999.0)
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
                atom->setMass(serial);
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
            if (conn.asu == gemmi::Asu::Different || conn.type == gemmi::Connection::Hydrog)
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
                out_meta->author += ", ";
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

} // namespace chemlib
