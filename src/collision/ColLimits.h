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
//   'numBoxesB <= MAXNUMBOXES'            0x65  -> 101
//
// The port was running 128, 600 and 32. A model whose bounding sphere overlaps
// between 600 and 1600 triangles therefore had its candidate list cut short and
// hits were lost silently, because the assert that would have caught it is
// compiled out in release. The sphere count was one short, which drops the last
// sphere of any model that has exactly 129.
//
// Boxes were the worst of the three: real maps ship collision models with more
// than 32 boxes - a log from a full map shows 510 of them, up to 274 - and the
// port examined only the first 32 of each, so a third or more of a building's
// collision was invisible to the checks.
//
// Lines keep the port's value: the analysis confirms that the assert name
// 'effectiveLinesA <= MAXNUMLINES' exists, but does not pin the number.
//
// This header is deliberately dependency-free: the limits are also checked by a
// unit test that does not build the game, and it must see the real values rather
// than a copy of them.
enum {
	MAXNUMSPHERES = 129,
	MAXNUMBOXES = 101,
	MAXNUMLINES = 16,
	MAXNUMTRIS = 1600
};

#endif // __GTA_COLLIMITS_H__
