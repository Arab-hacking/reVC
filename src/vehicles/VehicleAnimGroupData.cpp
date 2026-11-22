// SA CVehicleAnimGroup / CVehicleAnimGroupData, ported from gta-reversed
// (source/game_sa/VehicleAnimGroupData.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// This is the in-car animation table. handling.cfg's `^` lines fill it, and the
// get-in, get-out, jack and fall-out code asks it two things: which of the two
// animation groups a given clip belongs to, and how far into the clip the
// critical blend point is. Everything here is a straight port except the
// parsing, which had to be rewritten around this fork's own line reader (see
// InitFromData).
#include "common.h"

#include "VehicleAnimGroupData.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendSequence.h"

// The 30 groups. Zero-initialised members of CVehicleAnimGroup run its
// constructor, so the whole table starts at "group 0, all flags clear".
CVehicleAnimGroup CVehicleAnimGroupData::m_vehicleAnimGroups[CVehicleAnimGroupData::NUM_VEH_ANIM_GROUPS];

// SA CVehicleAnimGroup::CVehicleAnimGroup (0x5BD420).
CVehicleAnimGroup::CVehicleAnimGroup(void)
 : m_animFlags(), m_specialFlags(), m_generalTiming(), m_aInOutTiming()
{
	m_ucFirstGroup = 0;
	m_ucSecondGroup = 0;
	_pad[0] = _pad[1] = 0;

	for(int32 i = 0; i < 7; i++)
		m_aVecDoorOffsets[i] = CVector(0.0f, 0.0f, 0.0f);
}

// SA CVehicleAnimGroup::InitAnimGroup (0x5B9EB0).
void
CVehicleAnimGroup::InitAnimGroup(uint8 firstGroup, uint8 secondGroup,
	int32 animFlags, int32 animSpecialFlags,
	sVehAnimGroupGeneralTiming *generalTiming,
	sVehAnimGroupInOutTiming *startTiming, sVehAnimGroupInOutTiming *endTiming)
{
	m_ucFirstGroup = firstGroup;
	m_ucSecondGroup = secondGroup;
	m_animFlags.intValue = (uint32)animFlags;
	m_specialFlags.intValue = (uint32)animSpecialFlags;
	m_generalTiming = *generalTiming;
	GetInOutTiming(TIMING_START) = *startTiming;
	GetInOutTiming(TIMING_END) = *endTiming;
}

// SA CVehicleAnimGroup::CopyAnimGroup (0x5B9E40).
void
CVehicleAnimGroup::CopyAnimGroup(CVehicleAnimGroup *src)
{
	m_ucFirstGroup = src->m_ucFirstGroup;
	m_ucSecondGroup = src->m_ucSecondGroup;
	m_animFlags = src->m_animFlags;
	m_specialFlags = src->m_specialFlags;
	m_generalTiming = src->m_generalTiming;
	GetInOutTiming(TIMING_START) = src->GetInOutTiming(TIMING_START);
	GetInOutTiming(TIMING_END) = src->GetInOutTiming(TIMING_END);
}

// SA CVehicleAnimGroup::GetGroup (0x6E3B00).
//
// One bit of m_animFlags per animation family decides whether the clip comes
// from the first or the second of the group's two AssocGroupIds. The switch is
// SA's own - every in-car animation the game plays is listed in it, and the
// families are grouped exactly the way gta-reversed groups them.
AssocGroupId
CVehicleAnimGroup::GetGroup(AnimationId animId) const
{
	switch(animId){
	case ANIM_ID_CAR_ALIGN_LHS:
	case ANIM_ID_CAR_ALIGN_RHS:
	case ANIM_ID_CAR_ALIGNHI_LHS:
	case ANIM_ID_CAR_ALIGNHI_RHS:
		return m_animFlags.bAlign ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_OPEN_LHS:
	case ANIM_ID_CAR_OPEN_RHS:
		return m_animFlags.bOpenFrontDoorsOnExit ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_OPEN_LHS_1:
	case ANIM_ID_CAR_OPEN_RHS_1:
		return m_animFlags.bOpenRearDoorsOnExit ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_GETIN_LHS_0:
	case ANIM_ID_CAR_GETIN_RHS_0:
	case ANIM_ID_CAR_GETIN_BIKE_FRONT:
		return m_animFlags.bCanEnterFrontDoors ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_GETIN_LHS_1:
	case ANIM_ID_CAR_GETIN_RHS_1:
		return m_animFlags.bCanEnterRearDoors ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_PULLOUT_LHS:
	case ANIM_ID_CAR_PULLOUT_RHS:
	case ANIM_ID_UNKNOWN_15:
		return m_animFlags.bCanPulloutPed ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_CLOSEDOOR_LHS_0:
	case ANIM_ID_CAR_CLOSEDOOR_RHS_0:
		return m_animFlags.bCloseFrontDoorWhenInside ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_CLOSEDOOR_LHS_1:
	case ANIM_ID_CAR_CLOSEDOOR_RHS_1:
		return m_animFlags.bCloseRearDoorWhenInside ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_SHUFFLE_RHS_0:
	case ANIM_ID_CAR_SHUFFLE_RHS_1:
		return m_animFlags.bShuffle ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_GETOUT_LHS_0:
	case ANIM_ID_CAR_GETOUT_RHS_0:
		return m_animFlags.bCanExitFrontDoors ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_GETOUT_LHS_1:
	case ANIM_ID_CAR_GETOUT_RHS_1:
		return m_animFlags.bCanExitRearDoors ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_JACKEDLHS:
	case ANIM_ID_CAR_JACKEDRHS:
		return m_animFlags.bPlayerCanBeJacked ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_CLOSE_LHS_0:
	case ANIM_ID_CAR_CLOSE_RHS_0:
		return m_animFlags.bCloseFrontDoorWhenOutside ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_CLOSE_LHS_1:
	case ANIM_ID_CAR_CLOSE_RHS_1:
		return m_animFlags.bCloseRearDoorWhenOutside ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_ROLLOUT_LHS:
	case ANIM_ID_CAR_ROLLOUT_RHS:
		return m_animFlags.bCanJumpOutOfVehicle ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_ROLLDOOR:
		return m_animFlags.bRollDownWindowOnDoorClose ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_FALLOUT_LHS:
	case ANIM_ID_CAR_FALLOUT_RHS:
		return m_animFlags.bPlayerCanFallOut ? GetSecondGroup() : GetFirstGroup();

	case ANIM_ID_CAR_DOORLOCKED_LHS:
	case ANIM_ID_CAR_DOORLOCKED_RHS:
		return m_animFlags.bDoorLocked ? GetSecondGroup() : GetFirstGroup();
	}

	// SA does not reach this: only the animations above ask for a group. A clip
	// outside the family falls back to the first group rather than reading an
	// undefined value.
	return GetFirstGroup();
}

// SA CVehicleAnimGroup::ComputeCriticalBlendTime (0x6E3C80).
float
CVehicleAnimGroup::ComputeCriticalBlendTime(AnimationId animId)
{
	switch(animId){
	case ANIM_ID_CAR_ALIGN_LHS:
	case ANIM_ID_CAR_ALIGN_RHS:
	case ANIM_ID_CAR_ALIGNHI_LHS:
	case ANIM_ID_CAR_ALIGNHI_RHS:
	case ANIM_ID_CAR_OPEN_LHS:
	case ANIM_ID_CAR_OPEN_RHS:
	case ANIM_ID_CAR_OPEN_LHS_1:
	case ANIM_ID_CAR_OPEN_RHS_1:
	case ANIM_ID_CAR_GETIN_LHS_0:
	case ANIM_ID_CAR_GETIN_RHS_0:
	case ANIM_ID_CAR_GETIN_LHS_1:
	case ANIM_ID_CAR_GETIN_RHS_1:
	case ANIM_ID_CAR_GETIN_BIKE_FRONT:
		return m_generalTiming.m_fGetInTime;

	case ANIM_ID_CAR_GETOUT_LHS_0:
	case ANIM_ID_CAR_GETOUT_RHS_0:
	case ANIM_ID_CAR_GETOUT_LHS_1:
	case ANIM_ID_CAR_GETOUT_RHS_1:
		return m_generalTiming.m_fGetOutTime;

	case ANIM_ID_CAR_JACKEDLHS:
	case ANIM_ID_CAR_JACKEDRHS:
		return m_generalTiming.m_fCarJackTime;

	case ANIM_ID_CAR_ROLLOUT_LHS:
	case ANIM_ID_CAR_ROLLOUT_RHS:
		return m_generalTiming.m_fJumpOutTime;

	case ANIM_ID_CAR_FALLOUT_LHS:
	case ANIM_ID_CAR_FALLOUT_RHS:
		return m_generalTiming.m_fFallOutTime;

	default:
		return 0.0f;
	}
}

// SA CVehicleAnimGroup::ComputeAnimDoorOffsets (0x6E3D10).
//
// The door offset is where the ped's hand ends up at the last frame of the
// get-in/get-out clip, so it is read out of the animation the first time it is
// asked for and cached in m_aVecDoorOffsets afterwards.
CVector
CVehicleAnimGroup::ComputeAnimDoorOffsets(eVehAnimDoorOffset doorId)
{
	if(GetDoorOffset(doorId).MagnitudeSqr() != 0.0f)
		return GetDoorOffset(doorId);

	AnimationId animId;
	switch(doorId){
	case ENTER_FRONT:      animId = ANIM_ID_CAR_GETIN_LHS_0; break;
	case ENTER_REAR:       animId = ANIM_ID_CAR_GETIN_LHS_1; break;
	case ENTER_BIKE_FRONT: animId = ANIM_ID_CAR_GETIN_BIKE_FRONT; break;
	case EXIT_FRONT:       animId = ANIM_ID_CAR_GETOUT_LHS_0; break;
	case EXIT_REAR:        animId = ANIM_ID_CAR_GETOUT_LHS_1; break;
	case JACK_PED_LEFT:    animId = ANIM_ID_CAR_JACKEDLHS; break;
	case JACK_PED_RIGHT:   animId = ANIM_ID_CAR_JACKEDRHS; break;
	default:               return GetDoorOffset(doorId);
	}

	const AssocGroupId groupId = GetGroup(animId);
	CAnimBlendAssociation *animAssoc = CAnimManager::GetAnimAssociation(groupId, animId);
	if(animAssoc == nil || animAssoc->hierarchy == nil)
		return GetDoorOffset(doorId);

	CAnimBlendHierarchy *hierarchy = animAssoc->hierarchy;
	CAnimBlendSequence *sequences = hierarchy->sequences;
	if(sequences == nil)
		return GetDoorOffset(doorId);

	CAnimManager::UncompressAnimation(hierarchy);

	// The offset lives in the translation of the last key frame of the first
	// sequence - the ped's root bone.
	if(sequences->numFrames > 0 && sequences->HasTranslation()){
		KeyFrameTrans *frame = (KeyFrameTrans*)sequences->GetKeyFrame(sequences->numFrames - 1);
		GetDoorOffset(doorId) = frame->translation;
	}

	return GetDoorOffset(doorId);
}

// Not a SA function: builds one group out of a `^` line of handling.cfg.
//
// handling.cfg writes the line as
//
//   ^  id  firstGroup  secondGroup  18 flags  getIn jumpOut getOut jack fallOut
//      openOutStart openOutEnd closeInStart closeInEnd openInStart openInEnd
//      closeOutStart closeOutEnd  specialFlags
//
// where each timing pair is "start end". SA counts the groups from
// ANIM_GROUP_STDCARANIMS, so the two group numbers on the line are offsets from
// the start of the car block.
//
// The reference feeds the whole line to a 35-conversion sscanf. This fork's
// handling reader has already split the line into the same fields, so the
// values are pulled with a saved-position sscanf over the raw line here, using
// the exact format the reference uses. Nothing about the field order, meaning
// or scaling is changed.
int32
CVehicleAnimGroup::InitFromData(const char *line)
{
	int32 id = 0;
	int32 firstGroup = 0, secondGroup = 0;
	int32 animSpecialFlags = 0;
	sVehAnimGroupGeneralTiming generalTiming;
	sVehAnimGroupInOutTiming startTiming, stopTiming;
	int32 flags[18] = { 0 };

	const int32 n = sscanf(line,
		// 3 fields (id, first group, second group) + the 18 per-animation flags.
		// This used to carry a 22nd %d: one conversion more than there are
		// arguments, so sscanf ran off the end of the varargs list on every ^
		// line and wrote through whatever it found there.
		"%*s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d"
		"\t%f\t%f\t%f\t%f\t%f"
		"\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f"
		"\t%d",
		&id,
		&firstGroup,
		&secondGroup,

		// openOutF, getInF, jack, closeInF, getOutF, beJacked
		&flags[0], &flags[1], &flags[2], &flags[3], &flags[4], &flags[5],

		// closeOutR, closeRoll, openLocked, align, openOutR, getInR
		&flags[6], &flags[7], &flags[8], &flags[9], &flags[10], &flags[11],

		// closeInF, shuffle, getOutR, closeOutF, jumpOut, fallDie
		&flags[12], &flags[13], &flags[14], &flags[15], &flags[16], &flags[17],

		&generalTiming.m_fGetInTime,
		&generalTiming.m_fJumpOutTime,
		&generalTiming.m_fGetOutTime,
		&generalTiming.m_fCarJackTime,
		&generalTiming.m_fFallOutTime,

		&startTiming.OpenOut, &stopTiming.OpenOut,
		&startTiming.CloseIn, &stopTiming.CloseIn,
		&startTiming.OpenIn, &stopTiming.OpenIn,
		&startTiming.CloseOut, &stopTiming.CloseOut,

		&animSpecialFlags
	);

	if(n != 35)
		return n;

	// Assemble the eighteen separate flags into the one word GetGroup reads.
	int32 animFlags = 0;
	for(int32 i = 0; i < 18; i++){
		if(flags[i])
			animFlags |= (1 << i);
	}

	// The group numbers on the line are offsets into the car animation block.
	CVehicleAnimGroup grp;
	grp.InitAnimGroup(
		(uint8)(firstGroup + (int32)ASSOCGRP_STDCARAMIMS),
		(uint8)(secondGroup + (int32)ASSOCGRP_STDCARAMIMS),
		animFlags, animSpecialFlags,
		&generalTiming,
		&startTiming, &stopTiming
	);

	CopyAnimGroup(&grp);

	return -1;
}

// SA CVehicleAnimGroupData::GetInOutTimings (0x645630).
void
CVehicleAnimGroupData::GetInOutTimings(AssocGroupId groupId, eInOutTimingMode mode,
	float *pfAnimStart, float *pfAnimEnd)
{
	CVehicleAnimGroup &group = GetVehicleAnimGroup((int32)groupId);
	*pfAnimStart = group.GetInOutTiming(TIMING_START)[mode];
	*pfAnimEnd = group.GetInOutTiming(TIMING_END)[mode];
}

// SA CVehicleAnimGroupData::GetGroupForAnim (0x639FC0).
AssocGroupId
CVehicleAnimGroupData::GetGroupForAnim(AssocGroupId groupId, AnimationId animId)
{
	return GetVehicleAnimGroup((int32)groupId).GetGroup(animId);
}

// SA CVehicleAnimGroupData::GetAnimDoorOffset (0x645600).
CVector
CVehicleAnimGroupData::GetAnimDoorOffset(AssocGroupId groupId, eVehAnimDoorOffset doorId)
{
	return GetVehicleAnimGroup((int32)groupId).ComputeAnimDoorOffsets(doorId);
}

// SA CVehicleAnimGroupData::ComputeCriticalBlendTime.
float
CVehicleAnimGroupData::ComputeCriticalBlendTime(AssocGroupId groupId, AnimationId animId)
{
	return GetVehicleAnimGroup((int32)groupId).ComputeCriticalBlendTime(animId);
}

// SA CVehicleAnimGroupData::UsesTruckDrivingAnims (0x639FE0).
bool
CVehicleAnimGroupData::UsesTruckDrivingAnims(AssocGroupId groupId)
{
	return GetVehicleAnimGroup((int32)groupId).m_specialFlags.bUseTruckDriveAnims != 0;
}

// SA CVehicleAnimGroupData::UsesKartDrivingAnims (0x6D09E0).
bool
CVehicleAnimGroupData::UsesKartDrivingAnims(AssocGroupId groupId)
{
	return GetVehicleAnimGroup((int32)groupId).m_specialFlags.bUseKartDriveAnims != 0;
}

// SA CVehicleAnimGroupData::UsesHovercraftDrivingAnims.
bool
CVehicleAnimGroupData::UsesHovercraftDrivingAnims(AssocGroupId groupId)
{
	return GetVehicleAnimGroup((int32)groupId).m_specialFlags.bUseHovercraftDriveAnims != 0;
}

// Not a SA function: pulls the group id off the front of a `^` line and hands
// the rest to CVehicleAnimGroup::InitFromData.
int32
CVehicleAnimGroupData::LoadAGroupFromData(const char *line)
{
	int32 id = 0;
	if(sscanf(line, "^\t%d", &id) != 1)
		return 0;
	if(id < 0 || id >= NUM_VEH_ANIM_GROUPS)
		return 0;
	return GetVehicleAnimGroup(id).InitFromData(line);
}
