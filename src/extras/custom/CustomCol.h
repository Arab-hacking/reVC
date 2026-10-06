#ifndef __GTA_CUSTOMCOL_H__
#define __GTA_CUSTOMCOL_H__

// Collision out of the game's custom folder: a .cls block holds San Andreas
// collision (COL2/COL3 - fixed point vertices, 16 bit faces, the bounding
// volumes in another order, face groups), while Vice City and reVC read the
// first version of the format (COLL - float vertices, 32 bit faces, one extra
// "lines" section). The converter of the containers turns a .cls into COL3,
// which the game's own collision reader cannot read: it would take the header
// for geometry and produce rubbish. This module rewrites the blocks into the
// version the game reads.
//
// Everything a Vice City collision model can hold is carried over - spheres,
// boxes, vertices and faces. Face groups and the shadow mesh only exist in the
// newer versions and are dropped, lines are written as zero. Surfaces the game
// does not know (San Andreas has far more materials) become the default one.

#include <vector>

namespace customcol {

struct Stats
{
	int blocks;		// collision models found
	int spheres;
	int boxes;
	int vertices;
	int faces;
	int materials;		// surfaces the game does not have, put back to the default
	int dropped;		// faces dropped because they had no vertex
	int bad;		// blocks that could not be read
};

// .col file in COL2/COL3 shape -> the same file in the shape the game reads.
// An empty vector means the file held nothing usable.
std::vector<uint8> ToGameFormat(const std::vector<uint8> &col, Stats &stats);

}

#endif // __GTA_CUSTOMCOL_H__
