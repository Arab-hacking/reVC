#ifndef __GTA_PEDNODE_H__
#define __GTA_PEDNODE_H__

#include "Vector.h"

// San Andreas node numbering (`ePedNode`). Slot 0 is the whole-body node and
// has no bone of its own; the rest map one to one onto eBoneTag through
// ConvertPedNode2BoneTag. The SA set adds the two lower arms, the two
// clavicles and the jaw on top of what Vice City named.
enum ePedNode
{
	PED_NODE_NULL = 0,			// has no bone
	PED_NODE_UPPER_TORSO = 1,
	PED_NODE_HEAD = 2,
	PED_NODE_LEFT_ARM = 3,
	PED_NODE_RIGHT_ARM = 4,
	PED_NODE_LEFT_HAND = 5,
	PED_NODE_RIGHT_HAND = 6,
	PED_NODE_LEFT_LEG = 7,
	PED_NODE_RIGHT_LEG = 8,
	PED_NODE_LEFT_FOOT = 9,
	PED_NODE_RIGHT_FOOT = 10,
	PED_NODE_RIGHT_LOWER_LEG = 11,
	PED_NODE_LEFT_LOWER_LEG = 12,
	PED_NODE_LEFT_LOWER_ARM = 13,
	PED_NODE_RIGHT_LOWER_ARM = 14,
	PED_NODE_LEFT_CLAVICLE = 15,
	PED_NODE_RIGHT_CLAVICLE = 16,
	PED_NODE_NECK = 17,
	PED_NODE_JAW = 18,

	TOTAL_PED_NODES
};

// The piece a hit-collision sphere belongs to. These are the values that sit
// in the sphere's material field (`m_nPiece`), so the numbering is part of the
// collision contract and cannot be reordered on its own.
enum ePedPieceTypes
{
	PED_COL_SPHERE_LEG = 0,
	PED_COL_SPHERE_MID = 1,
	PED_COL_SPHERE_HEAD = 2,
	PED_PIECE_TORSO = 3,		// AKA CHEST
	PED_PIECE_ASS = 4,			// AKA MIDSECTION
	PED_PIECE_LEFT_ARM = 5,		// AKA UPPERARM_L
	PED_PIECE_RIGHT_ARM = 6,	// AKA UPPERARM_R
	PED_PIECE_LEFT_LEG = 7,		// AKA LEG_L
	PED_PIECE_RIGHT_LEG = 8,	// AKA LEG_R
	PED_PIECE_HEAD = 9
};

// One hit-collision sphere of a ped: which bone carries it, which piece of the
// body it covers, where it sits on that bone and how big it is.
struct tPedColNodeInfo
{
	char _pad[4];
	int32 m_nBoneID;		// see eBoneTag
	int32 m_nFlags;			// see ePedPieceTypes
	CVector m_vecCenter;
	float m_fRadius;
};

#endif // __GTA_PEDNODE_H__
