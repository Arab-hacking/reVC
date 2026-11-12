#ifndef __GTA_COLLIMITS_H__
#define __GTA_COLLIMITS_H__

// Sizes of the per-pair candidate lists in CCollision::ProcessColModels, taken
// from the loop bounds in the client's own code (PUNCT ANALYSE v45, section
// 10.4). These are not "how many volumes a model may have" - they are how many
// candidates one pass keeps, and each is one byte inside the assert that
// follows the loop:
//
//   loop stops at   assert fires at   name
//   -------------   --------------   -------------------------------------
//   spheres A  0x80 (128)    0x81    'effectiveSpheresA <= MAXNUMSPHERES'
//   lines A    0x10 (16)     0x11    'effectiveLinesA <= MAXNUMLINES'
//   spheres B  0x80 (128)    0x81    'numSpheresB <= MAXNUMSPHERES'
//   boxes   B  0x64 (100)    0x65    'numBoxesB <= MAXNUMBOXES'
//   tris    B  0x640 (1600) 0x641    'numTrianglesB <= MAXNUMTRIS'
//
// Both halves of that are visible in the disassembly: the loops leave at
// 0xdc1178 (`cmp w8,#0x80`), 0xdc1244 (`cmp w11,#0x64`), 0xdc12e8
// (`cmp w8,#0x640`) and 0xdc0d64 (`cmp w21,#0x11`), and the asserts are
// `cmp #0x81` (0xdc0d50, 0xdc12fc), `cmp #0x65` (0xdc130c) and `cmp #0x641`
// (0xdc1318). The assert is compiled out in release builds, so the loop bound is
// the behaviour that matters: the client itself never keeps more than these
// numbers of candidates, and the port has to stop at the same place or it
// processes hits in an order the client does not.
//
// Note that the older value 0x640 for triangles and 0x65 for boxes were being
// read as the limits themselves; they are the "@+1" assertion, one above the
// bound. That off-by-one is harmless for triangles (1601 candidates never
// happen) but it is wrong as a stated limit, and it is what made an earlier
// revision of the analysis call 101 triangles.
//
// This header is deliberately dependency-free: a unit test checks these values
// without building the game, so it must see the real numbers, not a copy.
enum {
	MAXNUMSPHERES = 128,
	MAXNUMBOXES = 100,
	MAXNUMLINES = 16,
	MAXNUMTRIS = 1600
};

#endif // __GTA_COLLIMITS_H__
