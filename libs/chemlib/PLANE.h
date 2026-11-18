#ifndef MI_PLANE_H
#define MI_PLANE_H

#include <cstring>
#include "model.h"

namespace chemlib
{
    class MIAtom;
    class Residue;

    class PLANE
    {
    public:
        PLANE() : atoms(NULL), natoms(0), tolerance(0.005f), res(NULL)
        {
        }

        void Clear()
        {
            PLANE p;
            *this = p;
        }                              // note: doesn't free old mem, if any

        MIAtom **atoms;
        int natoms;
        float tolerance;
        /**
         * normal vector to plane.
         */
        float vm[3];
        /**
         * distance from origin of normal vector.
         */
        float d;
        /**
         * the residue the bond is from.
         */
        const Residue *res;

    };
#define MAXPLANE 20
//@{
// A plane dictionary entry.
//@}
    class PLANEDICT
    {
    public:
        PLANEDICT()
        {
            Clear();
        }

        void Clear()
        {
            memset(restype, 0, sizeof(restype));
            natoms = 0;
            memset(name, 0, sizeof(name));
        }

        bool AddAtom(const char *atom_name)
        {
            if (natoms + 1 <= MAXPLANE)
            {
                strncpy(name[natoms], atom_name, MAXNAME);
                name[natoms][MAXNAME - 1] = '\0';
                natoms++;
                return true;
            }
            else
            {
                return false;
            }
        }

        char restype[MAXNAME];
        int natoms;
        char name[MAXPLANE][MAXNAME]; /* if more than 20 use overlapping planes*/
    };

}


#endif // ifndef MI_PLANE_H
