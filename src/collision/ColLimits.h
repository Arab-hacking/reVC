#ifndef __GTA_COLLIMITS_H__
#define __GTA_COLLIMITS_H__

// Ceilings for collision models and for the per-pair candidate lists built in
// CCollision::ProcessColModels.
//
// The client states these as asserts, and the assert text carries the number:
//
//   'effectiveSpheresA <= MAXNUMSPHERES'  0x81  -> 129
//   'numSpheresB <= MAXNUMSPHERES'        0x81  -> 129
//   'numTrianglesB <= MAXNUMTRIS'         0x640 -> 1600
//
// The port was running 128 and 600. A model whose bounding sphere overlaps
// between 600 and 1600 triangles therefore had its candidate list cut short and
// hits were lost silently, because the assert that would have caught it is
// compiled out in release. The sphere count was one short, which drops the last
// sphere of any model that has exactly 129.
//
// Boxes and lines keep the port's values: the analysis confirms that the assert
// names 'numBoxesB <= MAXNUMBOXES' and 'effectiveLinesA <= MAXNUMLINES' exist,
// but does not pin their numbers, so changing those would be guesswork.
//
// The MAX_COL* aliases are what CFileLoader::LoadCollisionModel accepts from a
// .col record. The counts come from the file and decide how much is allocated
// and how far the read pointer advances, so they use the same ceilings.
//
// This header is deliberately dependency-free: the limits are also checked by a
// unit test that does not build the game, and it must see the real values rather
// than a copy of them.
enum {
	MAXNUMSPHERES = 129,
	MAXNUMBOXES = 32,
	MAXNUMLINES = 16,
	MAXNUMTRIS = 1600
};

#define MAX_COLSPHERES MAXNUMSPHERES
#define MAX_COLBOXES   MAXNUMBOXES
#define MAX_COLLINES   MAXNUMLINES
#define MAX_COLTRIS    MAXNUMTRIS
// Vertices have no assert of their own in the client; the only hard numbers the
// analysis gives are the 12-byte step and the 1 800 000-byte collision buffer,
// so this is the largest count that fits in one record under that bound.
#define MAX_COLVERTS   150000

#endif // __GTA_COLLIMITS_H__
