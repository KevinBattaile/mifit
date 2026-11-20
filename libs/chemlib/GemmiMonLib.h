#ifndef chemlib_GemmiMonLib_h
#define chemlib_GemmiMonLib_h

#include <cstring>
#include <string>
#include <vector>

#include "Residue.h"
#include "Bond.h"
#include "ANGLE.h"
#include "TORSION.h"
#include "PLANE.h"
#include "CHIRALDICT.h"

namespace chemlib
{

/**
 * One monomer converted from a CCP4/Refmac/AceDRG/Grade2 monomer-library
 * data block. Bond and angle atom pointers refer to atoms owned by `res`.
 * ANGLE::ideal_angle holds the 1-3 distance (MIFit's dictionary convention),
 * not the angle in degrees.
 */
struct GemmiMonLibEntry
{
    GemmiMonLibEntry() : res(NULL)
    {
    }

    Residue *res; // owned by the enclosing GemmiMonLibResult until released
    std::vector<Bond> bonds;
    std::vector<ANGLE> angles;
    std::vector<TORSDICT> torsions;
    std::vector<PLANEDICT> planes;
    std::vector<CHIRALDICT> chirals;
    bool hasCoordinates = false;
};

struct GemmiMonLibResult
{
    GemmiMonLibResult()
    {
    }

    ~GemmiMonLibResult();
    void clear();

    std::vector<GemmiMonLibEntry> entries;

private:
    GemmiMonLibResult(const GemmiMonLibResult &);
    GemmiMonLibResult &operator=(const GemmiMonLibResult &);
};

/**
 * Reads every monomer block (.cif, .cif.gz) of a monomer-library style file
 * via gemmi. Blocks without _chem_comp_atom (e.g. comp_list) are skipped.
 * Returns true if at least one monomer was loaded.
 */
bool LoadMonomerCif(const std::string &path, GemmiMonLibResult &out);

/**
 * Returns true if the path ends in .cif/.mmcif (optionally .gz) or its
 * content contains a _chem_comp_atom category.
 */
bool IsMonomerCifFile(const std::string &path);

} // namespace chemlib

#endif // chemlib_GemmiMonLib_h
