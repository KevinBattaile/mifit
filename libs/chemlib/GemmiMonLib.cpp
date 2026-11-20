#include "GemmiMonLib.h"

#include <gemmi/read_cif.hpp>
#include <gemmi/chemcomp.hpp>

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
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace chemlib
{

GemmiMonLibResult::~GemmiMonLibResult()
{
    clear();
}

void GemmiMonLibResult::clear()
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        delete entries[i].res;
    }
    entries.clear();
}

namespace
{

const float DEFAULT_BOND_ESD = 0.015f;
const float DEFAULT_ANGLE_ESD = 0.03f;
const double PI_D = 3.14159265358979323846;

std::string lowerCopy(const std::string &s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

bool endsWith(const std::string &s, const char *suffix)
{
    size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

void copyName(char *dst, size_t dstSize, const std::string &src)
{
    std::strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

bool validEsd(double esd)
{
    return std::isfinite(esd) && esd > 0.0;
}

unsigned char bondOrderFromGemmi(gemmi::BondType t)
{
    switch (t)
    {
    case gemmi::BondType::Single:
        return SINGLEBOND;
    case gemmi::BondType::Double:
        return DOUBLEBOND;
    case gemmi::BondType::Triple:
        return TRIPLEBOND;
    case gemmi::BondType::Aromatic:
    case gemmi::BondType::Deloc:
        return PARTIALDOUBLEBOND;
    default:
        return NORMALBOND;
    }
}

typedef std::pair<std::string, std::string> NamePair;

NamePair makePair(const std::string &a, const std::string &b)
{
    return (a < b) ? NamePair(a, b) : NamePair(b, a);
}

float normalizeTorsion(double v)
{
    double r = std::fmod(v, 360.0);
    if (r < 0.0)
    {
        r += 360.0;
    }
    return static_cast<float>(r);
}

struct CoordRow
{
    double x, y, z;
};

// Fallback coordinate reader for _chem_comp_atom columns gemmi does not pick up
bool readBlockCoords(gemmi::cif::Block &block, std::map<std::string, CoordRow> &coords)
{
    static const char *const triples[][3] = {
        {"pdbx_model_Cartn_x_ideal", "pdbx_model_Cartn_y_ideal", "pdbx_model_Cartn_z_ideal"},
        {"model_Cartn_x", "model_Cartn_y", "model_Cartn_z"},
        {"x", "y", "z"},
        {"cartn_x", "cartn_y", "cartn_z"}
    };
    const std::string prefix = "_chem_comp_atom.";
    gemmi::cif::Column ids = block.find_values(prefix + "atom_id");
    const int n = ids.length();
    if (n <= 0)
    {
        return false;
    }
    for (size_t t = 0; t < sizeof(triples) / sizeof(triples[0]); ++t)
    {
        gemmi::cif::Column cx = block.find_values(prefix + triples[t][0]);
        gemmi::cif::Column cy = block.find_values(prefix + triples[t][1]);
        gemmi::cif::Column cz = block.find_values(prefix + triples[t][2]);
        if (cx.length() != n || cy.length() != n || cz.length() != n)
        {
            continue;
        }
        std::map<std::string, CoordRow> found;
        for (int i = 0; i < n; ++i)
        {
            const double nan = std::numeric_limits<double>::quiet_NaN();
            CoordRow r = {gemmi::cif::as_number(cx[i], nan),
                          gemmi::cif::as_number(cy[i], nan),
                          gemmi::cif::as_number(cz[i], nan)};
            if (std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.z))
            {
                found[gemmi::cif::as_string(ids[i])] = r;
            }
        }
        if (!found.empty())
        {
            coords.swap(found);
            return true;
        }
    }
    return false;
}

bool convertChemComp(const gemmi::ChemComp &cc, gemmi::cif::Block &block,
                     GemmiMonLibEntry &entry)
{
    const std::string &blockName = block.name;
    std::string type = cc.name;
    if (type.empty() && blockName.compare(0, 5, "comp_") == 0)
    {
        type = blockName.substr(5);
    }
    if (type.empty() || type.size() >= MAXNAME)
    {
        Logger::log("Skipping monomer '%s': invalid residue name length", type.c_str());
        return false;
    }
    if (cc.atoms.empty())
    {
        return false;
    }

    std::map<std::string, CoordRow> blockCoords;
    bool useCoords = cc.has_coordinates;
    if (useCoords)
    {
        bool anyFinite = false;
        for (size_t i = 0; i < cc.atoms.size() && !anyFinite; ++i)
        {
            const gemmi::Position &p = cc.atoms[i].xyz;
            anyFinite = std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }
        useCoords = anyFinite;
    }
    const bool useBlockCoords = !useCoords && readBlockCoords(block, blockCoords);
    std::map<std::string, MIAtom*> atomMap;
    Residue *res = new Residue();
    res->setType(type);
    res->setName("1");
    res->reserveAtoms(static_cast<int>(cc.atoms.size()));

    int serial = 0;
    for (size_t i = 0; i < cc.atoms.size(); ++i)
    {
        const gemmi::ChemComp::Atom &ca = cc.atoms[i];
        if (atomMap.find(ca.id) != atomMap.end())
        {
            Logger::log("Monomer %s: duplicate atom name %s ignored", type.c_str(), ca.id.c_str());
            continue;
        }

        MIAtom *atom = new MIAtom();
        atom->setName(ca.id.c_str());
        if (MIGetColorSetter())
        {
            (*MIGetColorSetter())(atom);
        }

        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (useCoords && std::isfinite(ca.xyz.x) && std::isfinite(ca.xyz.y) && std::isfinite(ca.xyz.z))
        {
            x = static_cast<float>(ca.xyz.x);
            y = static_cast<float>(ca.xyz.y);
            z = static_cast<float>(ca.xyz.z);
        }
        else if (useBlockCoords)
        {
            std::map<std::string, CoordRow>::const_iterator it = blockCoords.find(ca.id);
            if (it != blockCoords.end())
            {
                x = static_cast<float>(it->second.x);
                y = static_cast<float>(it->second.y);
                z = static_cast<float>(it->second.z);
            }
        }
        atom->setPosition(x, y, z);
        atom->setOcc(1.0f);
        atom->setBValue(15.0f);
        atom->set_radius_type(0);
        atom->setType(MIAtom::MIGetAtomTypeFromName(atom->name()));
        atom->setAltloc(' ');
        if (ca.el != gemmi::El::X)
        {
            atom->setAtomicnumber(static_cast<short>(ca.el.atomic_number()));
        }
        else
        {
            atom->setAtomicnumber(static_cast<short>(Atomic_Number(ca.id.c_str())));
        }
        atom->setAtomnumber(++serial);
        atom->setSymmop(0);
        if (std::isfinite(ca.charge))
        {
            atom->setCharge(static_cast<float>(ca.charge));
        }

        res->addAtom(atom);
        atomMap[ca.id] = atom;
    }

    entry.res = res;
    entry.hasCoordinates = useCoords || useBlockCoords;

    // Bonds: also remember ideal lengths so angles can be expressed as 1-3 distances
    std::map<NamePair, float> bondLengths;
    for (size_t i = 0; i < cc.rt.bonds.size(); ++i)
    {
        const gemmi::Restraints::Bond &rb = cc.rt.bonds[i];
        std::map<std::string, MIAtom*>::iterator a1 = atomMap.find(rb.id1.atom);
        std::map<std::string, MIAtom*>::iterator a2 = atomMap.find(rb.id2.atom);
        if (a1 == atomMap.end() || a2 == atomMap.end() || a1->second == a2->second)
        {
            continue;
        }
        Bond bond;
        bond.setAtom1(a1->second);
        bond.setAtom2(a2->second);
        bond.setOrder(bondOrderFromGemmi(rb.type));
        double length = rb.value;
        if (!std::isfinite(length) || length <= 0.0)
        {
            length = rb.value_nucleus;
        }
        if (!std::isfinite(length) || length <= 0.0)
        {
            length = useCoords ? a1->second->distance(a2->second) : 1.5;
        }
        bond.ideal_length = static_cast<float>(length);
        bond.tolerance = validEsd(rb.esd) ? static_cast<float>(rb.esd) : DEFAULT_BOND_ESD;
        bondLengths[makePair(rb.id1.atom, rb.id2.atom)] = bond.ideal_length;
        entry.bonds.push_back(bond);
    }

    // Angles: MIFit stores the ideal as the 1-3 distance (law of cosines)
    for (size_t i = 0; i < cc.rt.angles.size(); ++i)
    {
        const gemmi::Restraints::Angle &ra = cc.rt.angles[i];
        std::map<std::string, MIAtom*>::iterator a1 = atomMap.find(ra.id1.atom);
        std::map<std::string, MIAtom*>::iterator a2 = atomMap.find(ra.id2.atom);
        std::map<std::string, MIAtom*>::iterator a3 = atomMap.find(ra.id3.atom);
        if (a1 == atomMap.end() || a2 == atomMap.end() || a3 == atomMap.end() || !std::isfinite(ra.value))
        {
            continue;
        }
        double d12 = 0.0, d23 = 0.0;
        std::map<NamePair, float>::iterator b12 = bondLengths.find(makePair(ra.id1.atom, ra.id2.atom));
        std::map<NamePair, float>::iterator b23 = bondLengths.find(makePair(ra.id2.atom, ra.id3.atom));
        if (b12 != bondLengths.end())
        {
            d12 = b12->second;
        }
        else if (useCoords)
        {
            d12 = a1->second->distance(a2->second);
        }
        if (b23 != bondLengths.end())
        {
            d23 = b23->second;
        }
        else if (useCoords)
        {
            d23 = a2->second->distance(a3->second);
        }
        if (d12 <= 0.0 || d23 <= 0.0)
        {
            continue;
        }
        double theta = ra.value * PI_D / 180.0;
        double d13sq = d12 * d12 + d23 * d23 - 2.0 * d12 * d23 * std::cos(theta);
        if (d13sq <= 0.0)
        {
            continue;
        }
        double d13 = std::sqrt(d13sq);

        ANGLE angle;
        angle.setAtom1(a1->second);
        angle.setAtom2(a2->second);
        angle.setAtom3(a3->second);
        angle.ideal_angle = static_cast<float>(d13);
        angle.tolerance = DEFAULT_ANGLE_ESD;
        if (validEsd(ra.esd))
        {
            // propagate the angular esd (degrees) into a 1-3 distance esd
            double tol = d12 * d23 * std::sin(theta) / d13 * ra.esd * PI_D / 180.0;
            if (validEsd(tol))
            {
                angle.tolerance = static_cast<float>(tol);
            }
        }
        angle.res = res;
        entry.angles.push_back(angle);
    }

    // Torsions
    std::set<std::string> usedTorsionNames;
    for (size_t i = 0; i < cc.rt.torsions.size(); ++i)
    {
        const gemmi::Restraints::Torsion &rt = cc.rt.torsions[i];
        if (!std::isfinite(rt.value)
            || atomMap.find(rt.id1.atom) == atomMap.end()
            || atomMap.find(rt.id2.atom) == atomMap.end()
            || atomMap.find(rt.id3.atom) == atomMap.end()
            || atomMap.find(rt.id4.atom) == atomMap.end())
        {
            continue;
        }
        TORSDICT t;
        copyName(t.restype, sizeof(t.restype), type);
        std::string label = rt.label;
        if (label.empty() || label.size() >= sizeof(t.type) || usedTorsionNames.count(label))
        {
            label = "T" + std::to_string(i + 1);
        }
        usedTorsionNames.insert(label);
        copyName(t.type, sizeof(t.type), label);
        copyName(t.name[0], sizeof(t.name[0]), rt.id1.atom);
        copyName(t.name[1], sizeof(t.name[1]), rt.id2.atom);
        copyName(t.name[2], sizeof(t.name[2]), rt.id3.atom);
        copyName(t.name[3], sizeof(t.name[3]), rt.id4.atom);

        // Same rule as the legacy reader: fixed (improper-like) torsions carry
        // ideal values; periodic ones are flexible and carry none.
        bool fixed = (rt.esd == 0.0) || rt.period <= 0;
        if (fixed)
        {
            float v = normalizeTorsion(rt.value);
            t.ideal[0] = v;
            t.nideal = 1;
            if (rt.period == 2)
            {
                t.ideal[1] = normalizeTorsion(v + 180.0);
                t.nideal = 2;
            }
            else if (rt.period == 3)
            {
                t.ideal[1] = normalizeTorsion(v + 120.0);
                t.ideal[2] = normalizeTorsion(v + 240.0);
                t.nideal = 3;
            }
        }
        entry.torsions.push_back(t);
    }

    // Planes
    for (size_t i = 0; i < cc.rt.planes.size(); ++i)
    {
        const gemmi::Restraints::Plane &rp = cc.rt.planes[i];
        PLANEDICT p;
        copyName(p.restype, sizeof(p.restype), type);
        bool truncated = false;
        for (size_t j = 0; j < rp.ids.size(); ++j)
        {
            if (atomMap.find(rp.ids[j].atom) == atomMap.end())
            {
                continue;
            }
            if (!p.AddAtom(rp.ids[j].atom.c_str()))
            {
                truncated = true;
                break;
            }
        }
        if (truncated)
        {
            Logger::log("Monomer %s: plane %s has more than %d atoms; truncated",
                        type.c_str(), rp.label.c_str(), MAXPLANE);
        }
        if (p.natoms >= 4)
        {
            entry.planes.push_back(p);
        }
    }

    // Chirality
    for (size_t i = 0; i < cc.rt.chirs.size(); ++i)
    {
        const gemmi::Restraints::Chirality &rc = cc.rt.chirs[i];
        if (atomMap.find(rc.id_ctr.atom) == atomMap.end()
            || atomMap.find(rc.id1.atom) == atomMap.end()
            || atomMap.find(rc.id2.atom) == atomMap.end()
            || atomMap.find(rc.id3.atom) == atomMap.end())
        {
            continue;
        }
        CHIRALDICT c;
        if (rc.sign == gemmi::ChiralityType::Positive)
        {
            c.order = CLOCKWISE;
        }
        else if (rc.sign == gemmi::ChiralityType::Negative)
        {
            c.order = COUNTERCLOCKWISE;
        }
        else
        {
            continue; // "both" imposes no handedness
        }
        copyName(c.restype, sizeof(c.restype), type);
        copyName(c.center, sizeof(c.center), rc.id_ctr.atom);
        c.AddAtom(rc.id1.atom.c_str(), 0);
        c.AddAtom(rc.id2.atom.c_str(), 1);
        c.AddAtom(rc.id3.atom.c_str(), 2);
        entry.chirals.push_back(c);
    }

    if (!useCoords && !useBlockCoords)
    {
        Logger::log("Monomer %s has no coordinates; atoms placed at the origin", type.c_str());
    }
    return true;
}

} // anonymous namespace

bool LoadMonomerCif(const std::string &path, GemmiMonLibResult &out)
{
    try
    {
        gemmi::cif::Document doc = gemmi::read_cif_gz(path);
        for (gemmi::cif::Block &block : doc.blocks)
        {
            if (block.name == "comp_list")
            {
                continue;
            }
            if (block.find_values("_chem_comp_atom.atom_id").length() == 0)
            {
                continue;
            }
            try
            {
                gemmi::ChemComp cc = gemmi::make_chemcomp_from_block(block);
                GemmiMonLibEntry entry;
                if (convertChemComp(cc, block, entry))
                {
                    out.entries.push_back(entry);
                    entry.res = NULL; // ownership moved to `out`
                }
                else
                {
                    delete entry.res;
                }
            }
            catch (const std::exception &e)
            {
                Logger::log("Gemmi error reading monomer block %s in %s: %s",
                            block.name.c_str(), path.c_str(), e.what());
            }
        }
    }
    catch (const std::exception &e)
    {
        Logger::log("Gemmi error reading monomer file %s: %s", path.c_str(), e.what());
        return false;
    }
    return !out.entries.empty();
}

bool IsMonomerCifFile(const std::string &path)
{
    std::string lower = lowerCopy(path);
    if (endsWith(lower, ".cif") || endsWith(lower, ".mmcif")
        || endsWith(lower, ".cif.gz") || endsWith(lower, ".mmcif.gz"))
    {
        return true;
    }
    if (endsWith(lower, ".gz"))
    {
        return false;
    }

    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::string buf(262144, '\0');
    in.read(&buf[0], static_cast<std::streamsize>(buf.size()));
    buf.resize(static_cast<size_t>(in.gcount()));
    return buf.find("_chem_comp_atom.") != std::string::npos;
}

} // namespace chemlib
