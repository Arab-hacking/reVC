#ifndef __GTA_BONES_H__
#define __GTA_BONES_H__

#include "PedNode.h"

// San Andreas bone tags (eBoneTag, 0x4D56F0 / 0x4D58A0). The tag is a 16-bit
// index into the hierarchy: RpHAnimIDGetIndex takes it as is and the valid
// range runs to BONE_MAX_ID, not to the last named bone. The named tags are
// the ones an IFP keyframe can carry; the rest of the range is runtime-only.
enum eBoneTag
{
	BONE_UNKNOWN = -1,

	BONE_ROOT = 0,		// called "Normal" in some exporters
	BONE_PELVIS = 1,
	BONE_SPINE = 2,
	BONE_SPINE1 = 3,
	BONE_NECK = 4,
	BONE_HEAD = 5,
	BONE_L_BROW = 6,
	BONE_R_BROW = 7,
	BONE_JAW = 8,

	BONE_R_CLAVICLE = 21,
	BONE_R_UPPER_ARM = 22,
	BONE_R_FORE_ARM = 23,
	BONE_R_HAND = 24,
	BONE_R_FINGER = 25,
	BONE_R_FINGER_01 = 26,

	BONE_L_CLAVICLE = 31,
	BONE_L_UPPER_ARM = 32,
	BONE_L_FORE_ARM = 33,
	BONE_L_HAND = 34,
	BONE_L_FINGER = 35,
	BONE_L_FINGER_01 = 36,

	BONE_L_THIGH = 41,
	BONE_L_CALF = 42,
	BONE_L_FOOT = 43,
	BONE_L_TOE_0 = 44,

	BONE_R_THIGH = 51,
	BONE_R_CALF = 52,
	BONE_R_FOOT = 53,
	BONE_R_TOE_0 = 54,

	// The body-shape bones sit far outside the skeleton range. They are
	// addressed by their ID like every other tag, never by array position.
	BONE_BELLY = 201,

	BONE_R_BREAST = 301,
	BONE_L_BREAST = 302,

	BONE_MAX_ID = 303,
	MAX_BONE_NUM = 32
};

int ConvertPedNode2BoneTag(ePedNode node);
const char *ConvertBoneTag2BoneName(int tag);

// Some SA IFP clips (including CAR_alignHI) mark every bone ID as -1.
// Match those sequences by name instead: exporter names can have leading
// spaces, an optional Bip01 prefix, and spaces before digits (" Spine 1").
// Named matching is only a fallback; valid bone IDs still take precedence.
inline bool
MatchAnimBoneNames(const char *s, const char *t)
{
	if(s == 0 || t == 0)
		return false;
	for(int n = 0; n < 2; n++){
		const char **p = n ? &t : &s;
		while(**p == ' ' || **p == '\t') ++*p;
		const char *prefix = "bip01";
		const char *q = *p;
		while(*prefix){
			char c = *q;
			if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
			if(c != *prefix) break;
			prefix++;
			q++;
		}
		if(*prefix == '\0' && (*q == ' ' || *q == '\t'))
			*p = q;
	}
	// Compare ignoring every space, tab and underscore, not just the ones at a
	// token boundary: the same bank spells one bone "Spine 1" in FightA_1 and
	// "Spine1" in FightA_block, and a clip whose sequences carry bone ID -1
	// would otherwise leave those bones unbound and bend the ped.
	for(;;){
		while(*s == ' ' || *s == '\t' || *s == '_') s++;
		while(*t == ' ' || *t == '\t' || *t == '_') t++;
		char a = *s++, b = *t++;
		if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
		if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
		if(a != b) return false;
		if(a == '\0') return true;
	}
}

#endif // __GTA_BONES_H__
