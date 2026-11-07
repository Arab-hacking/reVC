#ifndef __GTA_MODELLOADDISPATCH_H__
#define __GTA_MODELLOADDISPATCH_H__

#include "BaseModelInfo.h"

// Dispatch by the IDE model info, never by the DFF's filename or root node.
// In particular, a ped DFF must go through CPedModelInfo::SetClump and a
// vehicle through CVehicleModelInfo::SetClump (the latter uses a two-stage read).
enum ModelLoadKind {
	MODELLOAD_UNSUPPORTED,
	MODELLOAD_ATOMIC,
	MODELLOAD_CLUMP,
	MODELLOAD_VEHICLE,
	MODELLOAD_PED
};

inline ModelLoadKind
GetModelLoadKind(int type)
{
	switch(type){
	case MITYPE_SIMPLE:
	case MITYPE_TIME:
	case MITYPE_WEAPON:
		return MODELLOAD_ATOMIC;
	case MITYPE_CLUMP:
		return MODELLOAD_CLUMP;
	case MITYPE_VEHICLE:
		return MODELLOAD_VEHICLE;
	case MITYPE_PED:
		return MODELLOAD_PED;
	default:
		return MODELLOAD_UNSUPPORTED;
	}
}

// Both VC and SA ped hit/animation code require a skin and HAnim hierarchy.
// Their *bone layouts* differ: do not impose SA-specific bone tags on VC peds.
// Other clump/vehicle models may legitimately be unskinned.
inline bool
IsModelClumpCompatible(ModelLoadKind kind, bool hasRoot, bool hasAtomic,
                       bool hasSkin, bool hasHAnimHierarchy)
{
	return (kind == MODELLOAD_CLUMP || kind == MODELLOAD_VEHICLE || kind == MODELLOAD_PED) &&
	       hasRoot && hasAtomic && (!hasSkin || hasHAnimHierarchy) &&
	       (kind != MODELLOAD_PED || hasSkin);
}

#endif
