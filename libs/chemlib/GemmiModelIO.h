#ifndef chemlib_GemmiModelIO_h
#define chemlib_GemmiModelIO_h

#include <string>
#include <vector>
#include "Residue.h"
#include "Bond.h"

namespace chemlib
{

struct GemmiStructureMetadata
{
    bool is_crystal = false;
    float a = 100.0f;
    float b = 100.0f;
    float c = 100.0f;
    float alpha = 90.0f;
    float beta = 90.0f;
    float gamma = 90.0f;
    std::string spacegroup_hm;
    std::string spacegroup_pdb_name;
    int spacegroup_number = 0;
    bool has_explicit_frac = false;
    float ctof[3][3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };

    bool has_secstr = false;
    std::string compound;
    std::string source;
    std::string author;
};

/**
 * Inspects a .cif / .mmcif / .cif.gz file to determine whether it contains
 * coordinate data (_atom_site. or _chem_comp_atom.) vs. reflection data (_refln. / _diffrn_refln.).
 */
bool IsCifCoordinateFile(const std::string &path);
bool IsCifReflectionFile(const std::string &path);

/**
 * Returns true if the path has a coordinate mmCIF or gzipped coordinate extension
 * (.cif, .mmcif, .cif.gz, .mmcif.gz, .pdb.gz, .ent.gz) that should be loaded via Gemmi.
 */
bool IsGemmiCoordinateFile(const std::string &path);

/**
 * Loads a macromolecular structure file (.cif, .mmcif, .cif.gz, .mmcif.gz, .pdb, .pdb.gz)
 * using gemmi::read_structure_gz into MIFit's chemlib::Residue doubly-linked list and Bond connectivity.
 */
Residue *LoadGemmiStructure(const std::string &path,
                            std::vector<Bond> *connects,
                            GemmiStructureMetadata *out_meta = nullptr);

/**
 * Writes the residue list as an mmCIF file via gemmi. Chains are grouped by
 * Residue::chain_id_str() so multi-character chain IDs round-trip. Cell and
 * spacegroup are written only when meta->is_crystal is set. When visibleOnly
 * is true, hidden atoms are skipped.
 */
bool SaveGemmiStructure(const std::string &path,
                        Residue *res,
                        const std::vector<Bond> *connects,
                        const GemmiStructureMetadata *meta,
                        bool visibleOnly = false);

} // namespace chemlib

#endif // chemlib_GemmiModelIO_h
