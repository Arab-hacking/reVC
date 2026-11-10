#ifndef __GTA_BONES_H__
#define __GTA_BONES_H__

enum BoneTag
{
	BONE_root = 0,
	BONE_pelvis = 1,
	BONE_spine = 2,
	BONE_spine1 = 3,
	BONE_neck = 4,
	BONE_head = 5,
	BONE_l_clavicle = 31,
	BONE_l_upperarm = 32,
	BONE_l_forearm = 33,
	BONE_l_hand = 34,
	BONE_l_finger = 35,
	BONE_l_finger01 = 36,
	BONE_r_clavicle = 21,
	BONE_r_upperarm = 22,
	BONE_r_forearm = 23,
	BONE_r_hand = 24,
	BONE_r_finger = 25,
	BONE_r_finger01 = 26,
	BONE_l_thigh = 41,
	BONE_l_calf = 42,
	BONE_l_foot = 43,
	BONE_l_toe = 44,
	BONE_r_thigh = 51,
	BONE_r_calf = 52,
	BONE_r_foot = 53,
	BONE_r_toe = 54,
};

int ConvertPedNode2BoneTag(int node);
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
	for(;;){
		while(*s == ' ' || *s == '\t') s++;
		while(*t == ' ' || *t == '\t') t++;
		char a = *s++, b = *t++;
		if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
		if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
		if(a != b) return false;
		if(a == '\0') return true;
	}
}

#endif // __GTA_BONES_H__
