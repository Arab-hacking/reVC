#include "common.h"

#include "General.h"
#include "RwHelper.h"
#include "ModelInfo.h"
#include "ModelIndices.h"
#include "FileMgr.h"
#include "RpAnimBlend.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendAssocGroup.h"
#include "AnimManager.h"
#include "Streaming.h"

#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "SavehDiag.h"
#include "brformats.h"
#endif

CAnimBlock CAnimManager::ms_aAnimBlocks[NUMANIMBLOCKS];
CAnimBlendHierarchy CAnimManager::ms_aAnimations[NUMANIMATIONS];
int32 CAnimManager::ms_numAnimBlocks;
int32 CAnimManager::ms_numAnimations;
CAnimBlendAssocGroup *CAnimManager::ms_aAnimAssocGroups;
CAnimBlendAssocGroup *CAnimManager::ms_aSAAnimAssocGroups;
RpClump *CAnimManager::ms_pVCPedAnimationReference;
CLinkList<CAnimBlendHierarchy*> CAnimManager::ms_animCache;

AnimAssocDesc aStdAnimDescs[] = {
	{ ANIM_STD_WALK, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_RUN, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_RUNFAST, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_IDLE, ASSOC_REPEAT },
	{ ANIM_STD_STARTWALK, ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_RUNSTOP1, ASSOC_DELETEFADEDOUT | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_RUNSTOP2, ASSOC_DELETEFADEDOUT | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_IDLE_CAM, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_IDLE_HBHB, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_IDLE_TIRED, ASSOC_REPEAT },
	{ ANIM_STD_IDLE_BIGGUN, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_CHAT, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_HAILTAXI, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_KO_FRONT, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_KO_LEFT, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_KO_BACK, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_KO_RIGHT, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_KO_SHOT_FACE, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_KO_SHOT_STOMACH, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_KO_SHOT_ARM_L, ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_KO_SHOT_ARM_R, ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_KO_SHOT_LEG_L, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_KO_SHOT_LEG_R, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_SPINFORWARD_LEFT, ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_SPINFORWARD_RIGHT, ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_HIGHIMPACT_FRONT, ASSOC_PARTIAL },
	{ ANIM_STD_HIGHIMPACT_LEFT, ASSOC_PARTIAL },
	{ ANIM_STD_HIGHIMPACT_BACK, ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_HIGHIMPACT_RIGHT, ASSOC_PARTIAL },
	{ ANIM_STD_HITBYGUN_FRONT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_HITBYGUN_LEFT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_HITBYGUN_BACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_HITBYGUN_RIGHT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_HIT_FRONT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_LEFT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_HIT_BACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_RIGHT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_HIT_FLOOR, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_HIT_BODYBLOW, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_CHEST, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_HEAD, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_WALK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_WALL, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HIT_FLOOR_FRONT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_HIT_BEHIND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_IDLE, ASSOC_REPEAT },
	{ ANIM_STD_FIGHT_2IDLE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_SHUFFLE_F, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_FIGHT_BODYBLOW, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_HEAD, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_KICK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_KNEE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_LHOOK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_PUNCH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_ROUNDHOUSE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_FIGHT_LONGKICK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_PARTIAL_PUNCH, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_FIGHT_JAB, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_ELBOW_L, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_ELBOW_R, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_BKICK_L, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_BKICK_R, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_DETONATE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_PUNCH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_PARTIALPUNCH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_KICKGROUND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_THROW_UNDER, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_FIGHT_SHUFFLE_B, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_JACKEDCAR_RHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_JACKEDCAR_LO_RHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_JACKEDCAR_LHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_JACKEDCAR_LO_LHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_QUICKJACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_QUICKJACKED, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_ALIGN_DOOR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_ALIGNHI_DOOR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_OPEN_DOOR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CARDOOR_LOCKED_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_PULL_OUT_PED_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_PULL_OUT_PED_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_GET_IN_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_GET_IN_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_ROLLING_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_ROLLING_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_JUMP_IN_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_GETOUT_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_GETOUT_LO_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_ALIGN_DOOR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_ALIGNHI_DOOR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_OPEN_DOOR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CARDOOR_LOCKED_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_PULL_OUT_PED_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_PULL_OUT_PED_LO_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_GET_IN_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_GET_IN_LO_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_DOOR_LO_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_SHUFFLE_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_SHUFFLE_LO_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_SIT, ASSOC_DELETEFADEDOUT},
	{ ANIM_STD_CAR_SIT_LO, ASSOC_DELETEFADEDOUT},
	{ ANIM_STD_CAR_SIT_P, ASSOC_DELETEFADEDOUT},
	{ ANIM_STD_CAR_SIT_P_LO, ASSOC_DELETEFADEDOUT},
	{ ANIM_STD_CAR_DRIVE_LEFT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVE_RIGHT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVE_LEFT_LO, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVE_RIGHT_LO, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVEBY_LEFT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVEBY_RIGHT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVEBY_LEFT_LO, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_DRIVEBY_RIGHT_LO, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_CAR_LOOKBEHIND, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_BOAT_DRIVE, ASSOC_DELETEFADEDOUT | ASSOC_DRIVING },
	{ ANIM_STD_BOAT_DRIVE_LEFT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_BOAT_DRIVE_RIGHT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_BOAT_LOOKBEHIND, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_STD_BIKE_PICKUP_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_PICKUP_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_PULLUP_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_PULLUP_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_ELBOW_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_ELBOW_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_BIKE_FALLOFF, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_BIKE_FALLBACK, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_GETOUT_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_GETOUT_LO_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_CLOSE_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CAR_HOOKERTALK, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_TRAIN_GETIN, ASSOC_PARTIAL },
	{ ANIM_STD_TRAIN_GETOUT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CRAWLOUT_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_CRAWLOUT_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_ROLLOUT_LHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
	{ ANIM_STD_ROLLOUT_RHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
	{ ANIM_STD_GET_UP, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_GET_UP_LEFT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_GET_UP_RIGHT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_GET_UP_FRONT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_JUMP_LAUNCH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_JUMP_GLIDE, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_JUMP_LAND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_FALL, ASSOC_REPEAT | ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_FALL_GLIDE, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_FALL_LAND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_FALL_COLLAPSE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_FALL_ONBACK, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_FALL_ONFRONT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_FRONTAL },
	{ ANIM_STD_EVADE_STEP, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_EVADE_DIVE, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_FRONTAL },
	{ ANIM_STD_XPRESS_SCRATCH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_IDLE },
	{ ANIM_STD_ROADCROSS, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_STD_TURN180, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_ARREST, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_DROWN, ASSOC_PARTIAL },
	{ ANIM_STD_DUCK_DOWN, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_DUCK_LOW, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_DUCK_WEAPON, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_RBLOCK_SHOOT, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_HANDSUP, ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_HANDSCOWER, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_STD_PARTIAL_FUCKU, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_STD_PHONE_IN, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_PHONE_OUT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_PHONE_TALK, ASSOC_REPEAT | ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
	{ ANIM_STD_SEAT_DOWN, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_SEAT_UP, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_SEAT_IDLE, ASSOC_REPEAT | ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_SEAT_RVRS, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_ATM, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_ABSEIL, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL },
};
AnimAssocDesc aVanAnimDescs[] = {
	{ ANIM_STD_VAN_OPEN_DOOR_REAR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_GET_IN_REAR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_CLOSE_DOOR_REAR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_GET_OUT_REAR_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_OPEN_DOOR_REAR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_GET_IN_REAR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_CLOSE_DOOR_REAR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_VAN_GET_OUT_REAR_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aCoachAnimDescs[] = {
	{ ANIM_STD_COACH_OPEN_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_COACH_OPEN_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_COACH_GET_IN_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_COACH_GET_IN_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STD_COACH_GET_OUT_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aBikeAnimDescs[] = {
	{ ANIM_BIKE_RIDE, ASSOC_DELETEFADEDOUT},
	{ ANIM_BIKE_READY, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_LEFT, ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_RIGHT, ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_LEANB, ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_LEANF, ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_WALKBACK, ASSOC_REPEAT | ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_JUMPON_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_JUMPON_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_KICK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_HIT, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_GETOFF_LHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_GETOFF_RHS, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_BIKE_GETOFF_BACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
	{ ANIM_BIKE_DRIVEBY_LHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_DRIVEBY_RHS, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_DRIVEBY_FORWARD, ASSOC_DELETEFADEDOUT | ASSOC_PARTIAL | ASSOC_DRIVING },
	{ ANIM_BIKE_RIDE_P, ASSOC_DELETEFADEDOUT | ASSOC_DRIVING },
};
AnimAssocDesc aMeleeAnimDescs[] = {
	{ ANIM_MELEE_ATTACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_MELEE_ATTACK_2ND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_MELEE_ATTACK_START, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_NOWALK },
	{ ANIM_MELEE_IDLE_FIGHTMODE, ASSOC_REPEAT },
	{ ANIM_MELEE_ATTACK_FINISH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION },
};
AnimAssocDesc aSwingAnimDescs[] = {
	{ ANIM_MELEE_ATTACK, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_MELEE_ATTACK_2ND, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_MELEE_ATTACK_START, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_MELEE_IDLE_FIGHTMODE, ASSOC_REPEAT },
	{ ANIM_MELEE_ATTACK_FINISH, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aWeaponAnimDescs[] = {
	{ ANIM_WEAPON_FIRE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_WEAPON_CROUCHFIRE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_WEAPON_RELOAD, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_WEAPON_CROUCHRELOAD, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_WEAPON_FIRE_3RD, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aMedicAnimDescs[] = {
	{ ANIM_MEDIC_CPR, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aSunbatheAnimDescs[] = {
	{ ANIM_SUNBATHE_IDLE, ASSOC_REPEAT | ASSOC_PARTIAL },
	{ ANIM_SUNBATHE_DOWN, ASSOC_REPEAT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
	{ ANIM_SUNBATHE_UP, ASSOC_REPEAT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
	{ ANIM_SUNBATHE_ESCAPE, ASSOC_REPEAT | ASSOC_PARTIAL | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
};
AnimAssocDesc aPlayerIdleAnimDescs[] = {
	{ ANIM_PLAYER_IDLE1, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_PLAYER_IDLE2, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_PLAYER_IDLE3, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_PLAYER_IDLE4, ASSOC_DELETEFADEDOUT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aRiotAnimDescs[] = {
	{ ANIM_RIOT_ANGRY, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_ANGRY_B, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_CHANT, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_PUNCHES, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_SHOUT, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_CHALLENGE, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_RIOT_FUCKYOU, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
AnimAssocDesc aStripAnimDescs[] = {
	{ ANIM_STRIP_A, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_B, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_C, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_D, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_E, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_F, ASSOC_REPEAT | ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
	{ ANIM_STRIP_G, ASSOC_FADEOUTWHENDONE | ASSOC_PARTIAL },
};
#ifdef PC_PLAYER_CONTROLS
AnimAssocDesc aStdAnimDescsSide[] = {
	{ ANIM_STD_WALK, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_RUN, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_RUNFAST, ASSOC_REPEAT | ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION | ASSOC_WALK },
	{ ANIM_STD_IDLE, ASSOC_REPEAT },
	{ ANIM_STD_STARTWALK, ASSOC_HAS_TRANSLATION | ASSOC_HAS_X_TRANSLATION },
};
#endif
char const* aStdAnimations[] = {
	"walk_civi",
	"run_civi",
	"sprint_panic",
	"idle_stance",
	"walk_start",
	"run_stop",
	"run_stopR",
	"idle_hbhb",
	"idle_hbhb",
	"idle_tired",
	"idle_armed",
	"idle_chat",
	"idle_taxi",
	"KO_shot_front",
	"KO_shot_front",
	"KO_shot_front",
	"KO_shot_front",
	"KO_shot_face",
	"KO_shot_stom",
	"KO_shot_arml",
	"KO_shot_armR",
	"KO_shot_legl",
	"KO_shot_legR",
	"KD_left",
	"KD_right",
	"KO_skid_front",
	"KO_spin_R",
	"KO_skid_back",
	"KO_spin_L",
	"SHOT_partial",
	"SHOT_leftP",
	"SHOT_partial",
	"SHOT_rightP",
	"HIT_front",
	"HIT_L",
	"HIT_back",
	"HIT_R",
	"FLOOR_hit",
	"HIT_bodyblow",
	"HIT_chest",
	"HIT_head",
	"HIT_walk",
	"HIT_wall",
	"FLOOR_hit_f",
	"HIT_behind",
	"FIGHTIDLE",
	"FIGHT2IDLE",
	"FIGHTsh_F",
	"FIGHTbodyblow",
	"FIGHThead",
	"FIGHTkick",
	"FIGHTknee",
	"FIGHTLhook",
	"FIGHTpunch",
	"FIGHTrndhse",
	"FIGHTlngkck",
	"FIGHTppunch",
	"FIGHTjab",
	"FIGHTelbowL",
	"FIGHTelbowR",
	"FIGHTbkickL",
	"FIGHTbkickR",
	"bomber",
	"punchR",
	"FIGHTppunch",
	"KICK_floor",
	"WEAPON_throwu",
	"FIGHTsh_back",
	"car_jackedRHS",
	"car_LjackedRHS",
	"car_jackedLHS",
	"car_LjackedLHS",
	"CAR_Qjack",
	"CAR_Qjacked",
	"CAR_align_LHS",
	"CAR_alignHI_LHS",
	"CAR_open_LHS",
	"CAR_doorlocked_LHS",
	"CAR_pullout_LHS",
	"CAR_pulloutL_LHS",
	"CAR_getin_LHS",
	"CAR_getinL_LHS",
	"CAR_closedoor_LHS",
	"CAR_closedoorL_LHS",
	"CAR_rolldoor",
	"CAR_rolldoorLO",
	"CAR_jumpin_LHS",
	"CAR_getout_LHS",
	"CAR_getoutL_LHS",
	"CAR_close_LHS",
	"CAR_align_RHS",
	"CAR_alignHI_RHS",
	"CAR_open_RHS",
	"CAR_doorlocked_RHS",
	"CAR_pullout_RHS",
	"CAR_pulloutL_RHS",
	"CAR_getin_RHS",
	"CAR_getinL_RHS",
	"CAR_closedoor_RHS",
	"CAR_closedoorL_RHS",
	"CAR_shuffle_RHS",
	"CAR_Lshuffle_RHS",
	"CAR_sit",
	"CAR_Lsit",
	"CAR_sitp",
	"CAR_sitpLO",
	"DRIVE_L",
	"Drive_R",
	"Drive_LO_l",
	"Drive_LO_R",
	"Driveby_L",
	"Driveby_R",
	"DrivebyL_L",
	"DrivebyL_R",
	"CAR_LB",
	"DRIVE_BOAT",
	"DRIVE_BOAT_L",
	"DRIVE_BOAT_R",
	"DRIVE_BOAT_back",
	"BIKE_pickupR",
	"BIKE_pickupL",
	"BIKE_pullupR",
	"BIKE_pullupL",
	"BIKE_elbowR",
	"BIKE_elbowL",
	"BIKE_fall_off",
	"BIKE_fallR",
	"CAR_getout_RHS",
	"CAR_getoutL_RHS",
	"CAR_close_RHS",
	"car_hookertalk",
	"idle_stance",
	"idle_stance",
	"CAR_crawloutRHS",
	"CAR_crawloutRHS",
	"CAR_rollout_LHS",
	"CAR_rollout_LHS",
	"Getup",
	"Getup",
	"Getup",
	"Getup_front",
	"JUMP_launch",
	"JUMP_glide",
	"JUMP_land",
	"FALL_fall",
	"FALL_glide",
	"FALL_land",
	"FALL_collapse",
	"FALL_back",
	"FALL_front",
	"EV_step",
	"EV_dive",
	"XPRESSscratch",
	"roadcross",
	"TURN_180",
	"ARRESTgun",
	"DROWN",
	"DUCK_down",
	"DUCK_low",
	"WEAPON_crouch",
	"RBLOCK_Cshoot",
	"handsup",
	"handsCOWER",
	"FUCKU",
	"PHONE_in",
	"PHONE_out",
	"PHONE_talk",
	"SEAT_down",
	"SEAT_up",
	"SEAT_idle",
	"SEAT_down",
	"ATM",
	"abseil",
};
char const* aVanAnimations[] = {
	"VAN_openL",
	"VAN_getinL",
	"VAN_closeL",
	"VAN_getoutL",
	"VAN_open",
	"VAN_getin",
	"VAN_close",
	"VAN_getout",
};
char const* aCoachAnimations[] = {
	"COACH_opnL",
	"COACH_opnL",
	"COACH_inL",
	"COACH_inL",
	"COACH_outL",
};
char const* aBikesAnimations[] = {
	"BIKEs_Ride",
	"BIKEs_Still",
	"BIKEs_Left",
	"BIKEs_Right",
	"BIKEs_Back",
	"BIKEs_Fwd",
	"BIKEs_pushes",
	"BIKEs_jumponR",
	"BIKEs_jumponL",
	"BIKEs_kick",
	"BIKEs_hit",
	"BIKEs_getoffRHS",
	"BIKEs_getoffLHS",
	"BIKEs_getoffBACK",
	"BIKEs_drivebyLHS",
	"BIKEs_drivebyRHS",
	"BIKEs_drivebyFT",
	"BIKEs_passenger",
};
char const* aBikevAnimations[] = {
	"BIKEv_Ride",
	"BIKEv_Still",
	"BIKEv_Left",
	"BIKEv_Right",
	"BIKEv_Back",
	"BIKEv_Fwd",
	"BIKEv_pushes",
	"BIKEv_jumponR",
	"BIKEv_jumponL",
	"BIKEv_kick",
	"BIKEv_hit",
	"BIKEv_getoffRHS",
	"BIKEv_getoffLHS",
	"BIKEv_getoffBACK",
	"BIKEv_drivebyLHS",
	"BIKEv_drivebyRHS",
	"BIKEv_drivebyFT",
	"BIKEv_passenger",
};
char const* aBikehAnimations[] = {
	"BIKEh_Ride",
	"BIKEh_Still",
	"BIKEh_Left",
	"BIKEh_Right",
	"BIKEh_Back",
	"BIKEh_Fwd",
	"BIKEh_pushes",
	"BIKEh_jumponR",
	"BIKEh_jumponL",
	"BIKEh_kick",
	"BIKEh_hit",
	"BIKEh_getoffRHS",
	"BIKEh_getoffLHS",
	"BIKEh_getoffBACK",
	"BIKEh_drivebyLHS",
	"BIKEh_drivebyRHS",
	"BIKEh_drivebyFT",
	"BIKEh_passenger",
};
char const* aBikedAnimations[] = {
	"BIKEd_Ride",
	"BIKEd_Still",
	"BIKEd_Left",
	"BIKEd_Right",
	"BIKEd_Back",
	"BIKEd_Fwd",
	"BIKEd_pushes",
	"BIKEd_jumponR",
	"BIKEd_jumponL",
	"BIKEd_kick",
	"BIKEd_hit",
	"BIKEd_getoffRHS",
	"BIKEd_getoffLHS",
	"BIKEd_getoffBACK",
	"BIKEd_drivebyLHS",
	"BIKEd_drivebyRHS",
	"BIKEd_drivebyFT",
	"BIKEd_passenger",
};
char const* aUnarmedAnimations[] = {
	"punchR",
	"KICK_floor",
	"FIGHTppunch",
};
char const* aScrewdriverAnimations[] = {
	"FIGHTbodyblow",
	"FIGHTbodyblow",
	"FIGHTppunch",
	"FIGHTIDLE",
	"FIGHTbodyblow",
};
char const* aKnifeAnimations[] = {
	"WEAPON_knife_1",
	"WEAPON_knife_2",
	"knife_part",
	"WEAPON_knifeidle",
	"WEAPON_knife_3",
};
char const* aBaseballbatAnimations[] = {
	"WEAPON_bat_h",
	"WEAPON_bat_v",
	"BAT_PART",
	"WEAPON_bat_h",
	"WEAPON_golfclub",
};
char const* aGolfclubAnimations[] = {
	"WEAPON_bat_h",
	"WEAPON_golfclub",
	"BAT_PART",
	"WEAPON_bat_h",
	"WEAPON_bat_v",
};
char const* aChainsawAnimations[] = {
	"WEAPON_csaw",
	"WEAPON_csawlo",
	"csaw_part",
};
char const* aPythonAnimations[] = {
	"python_fire",
	"python_crouchfire",
	"python_reload",
	"python_crouchreload",
};
char const* aColtAnimations[] = {
	"colt45_fire",
	"colt45_crouchfire",
	"colt45_reload",
	"colt45_crouchreload",
	"colt45_cop",
};
char const* aShotgunAnimations[] = {
	"shotgun_fire",
	"shotgun_crouchfire",
};
char const* aBuddyAnimations[] = {
	"buddy_fire",
	"buddy_crouchfire",
};
char const* aTecAnimations[] = {
	"TEC_fire",
	"TEC_crouchfire",
	"TEC_reload",
	"TEC_crouchreload",
};
char const* aUziAnimations[] = {
	"UZI_fire",
	"UZI_crouchfire",
	"UZI_reload",
	"UZI_crouchreload",
};
char const* aRifleAnimations[] = {
	"RIFLE_fire",
	"RIFLE_crouchfire",
	"RIFLE_load",
	"RIFLE_crouchload",
};
char const* aM60Animations[] = {
	"M60_fire",
	"M60_fire",
	"M60_reload",
};
char const* aSniperAnimations[] = {
	"WEAPON_sniper",
};
char const* aThrowAnimations[] = {
	"WEAPON_throw",
	"WEAPON_throwu",
	"WEAPON_start_throw",
};
char const* aFlamethrowerAnimations[] = {
	"FLAME_fire",
};
char const* aMedicAnimations[] = {
	"CPR",
};
char const* aSunbatheAnimations[] = {
	"bather",
	"batherdown",
	"batherup",
	"batherscape",
};
char const* aPlayerIdleAnimations[] = {
	"stretch",
	"time",
	"shldr",
	"strleg",
};
char const* aRiotAnimations[] = {
	"riot_angry",
	"riot_angry_b",
	"riot_chant",
	"riot_punches",
	"riot_shout",
	"riot_challenge",
	"riot_fuku",
};
char const* aStripAnimations[] = {
	"strip_A",
	"strip_B",
	"strip_C",
	"strip_D",
	"strip_E",
	"strip_F",
	"strip_G",
};
char const* aLanceAnimations[] = {
	"lance",
};
char const* aPlayerAnimations[] = {
	"walk_player",
	"run_player",
	"SPRINT_civi",
	"IDLE_STANCE",
	"walk_start",
};
char const* aPlayerWithRocketAnimations[] = {
	"walk_rocket",
	"run_rocket",
	"run_rocket",
	"idle_rocket",
	"walk_start_rocket",
};
char const* aPlayer1ArmedAnimations[] = {
	"walk_player",
	"run_1armed",
	"SPRINT_civi",
	"IDLE_STANCE",
	"walk_start",
};
char const* aPlayer2ArmedAnimations[] = {
	"walk_armed",
	"run_armed",
	"run_armed",
	"idle_armed",
	"walk_start_armed",
};
char const* aPlayerBBBatAnimations[] = {
	"walk_player",
	"run_player",
	"run_player",
	"IDLE_STANCE",
	"walk_start",
};
char const* aPlayerChainsawAnimations[] = {
	"walk_csaw",
	"run_csaw",
	"run_csaw",
	"IDLE_csaw",
	"walk_start_csaw",
};
char const* aShuffleAnimations[] = {
	"WALK_shuffle",
	"RUN_civi",
	"SPRINT_civi",
	"IDLE_STANCE",
};
char const* aOldAnimations[] = {
	"walk_old",
	"run_civi",
	"sprint_civi",
	"idle_stance",
};
char const* aGang1Animations[] = {
	"walk_gang1",
	"run_gang1",
	"sprint_civi",
	"idle_stance",
};
char const* aGang2Animations[] = {
	"walk_gang2",
	"run_gang1",
	"sprint_civi",
	"idle_stance",
};
char const* aFatAnimations[] = {
	"walk_fat",
	"run_civi",
	"woman_runpanic",
	"idle_stance",
};
char const* aOldFatAnimations[] = {
	"walk_fatold",
	"run_fatold",
	"woman_runpanic",
	"idle_stance",
};
char const* aJoggerAnimations[] = {
	"JOG_maleA",
	"run_civi",
	"sprint_civi",
	"idle_stance",
};
char const* aStdWomanAnimations[] = {
	"woman_walknorm",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aWomanShopAnimations[] = {
	"woman_walkshop",
	"woman_run",
	"woman_run",
	"woman_idlestance",
};
char const* aBusyWomanAnimations[] = {
	"woman_walkbusy",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aSexyWomanAnimations[] = {
	"woman_walksexy",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aFatWomanAnimations[] = {
	"walk_fat",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aOldWomanAnimations[] = {
	"woman_walkold",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aJoggerWomanAnimations[] = {
	"JOG_maleB",
	"woman_run",
	"woman_runpanic",
	"woman_idlestance",
};
char const* aPanicChunkyAnimations[] = {
	"run_fatold",
	"woman_runpanic",
	"woman_runpanic",
	"idle_stance",
};
char const* aSkateAnimations[] = {
	"skate_run",
	"skate_sprint",
	"skate_sprint",
	"skate_idle",
};
#ifdef PC_PLAYER_CONTROLS
char const* aPlayerStrafeBackAnimations[] = {
	"walk_back",
	"run_back",
	"run_back",
	"IDLE_STANCE",
	"walk_start_back",
};
char const* aPlayerStrafeLeftAnimations[] = {
	"walk_left",
	"run_left",
	"run_left",
	"IDLE_STANCE",
	"walk_start_left",
};
char const* aPlayerStrafeRightAnimations[] = {
	"walk_right",
	"run_right",
	"run_right",
	"IDLE_STANCE",
	"walk_start_right",
};
char const* aRocketStrafeBackAnimations[] = {
	"walk_rocket_back",
	"run_rocket_back",
	"run_rocket_back",
	"idle_rocket",
	"walkst_rocket_back",
};
char const* aRocketStrafeLeftAnimations[] = {
	"walk_rocket_left",
	"run_rocket_left",
	"run_rocket_left",
	"idle_rocket",
	"walkst_rocket_left",
};
char const* aRocketStrafeRightAnimations[] = {
	"walk_rocket_right",
	"run_rocket_right",
	"run_rocket_right",
	"idle_rocket",
	"walkst_rocket_right",
};
char const* aChainsawStrafeBackAnimations[] = {
	"walk_csaw_back",
	"run_csaw_back",
	"run_csaw_back",
	"idle_csaw",
	"walkst_csaw_back",
};
char const* aChainsawStrafeLeftAnimations[] = {
	"walk_csaw_left",
	"run_csaw_left",
	"run_csaw_left",
	"idle_csaw",
	"walkst_csaw_left",
};
char const* aChainsawStrafeRightAnimations[] = {
	"walk_csaw_right",
	"run_csaw_right",
	"run_csaw_right",
	"idle_csaw",
	"walkst_csaw_right",
};
#endif

#define awc(a) ARRAY_SIZE(a), a
const AnimAssocDefinition CAnimManager::ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS] = {
	{ "man", "ped", MI_COP, awc(aStdAnimations), aStdAnimDescs },
	{ "van", "van", MI_COP, awc(aVanAnimations), aVanAnimDescs },
	{ "coach", "coach", MI_COP, awc(aCoachAnimations), aCoachAnimDescs },
	{ "bikes", "bikes", MI_COP, awc(aBikesAnimations), aBikeAnimDescs },
	{ "bikev", "bikev", MI_COP, awc(aBikevAnimations), aBikeAnimDescs },
	{ "bikeh", "bikeh", MI_COP, awc(aBikehAnimations), aBikeAnimDescs },
	{ "biked", "biked", MI_COP, awc(aBikedAnimations), aBikeAnimDescs },
	{ "unarmed", "ped", MI_COP, awc(aUnarmedAnimations), aMeleeAnimDescs },
	{ "screwdrv", "ped", MI_COP, awc(aScrewdriverAnimations), aMeleeAnimDescs },
	{ "knife", "knife", MI_COP, awc(aKnifeAnimations), aMeleeAnimDescs },
	{ "baseball", "baseball", MI_COP, awc(aBaseballbatAnimations), aSwingAnimDescs },
	{ "golfclub", "baseball", MI_COP, awc(aGolfclubAnimations), aSwingAnimDescs },
	{ "chainsaw", "chainsaw", MI_COP, awc(aChainsawAnimations), aMeleeAnimDescs },
	{ "python", "python", MI_COP, awc(aPythonAnimations), aWeaponAnimDescs },
	{ "colt45", "colt45", MI_COP, awc(aColtAnimations), aWeaponAnimDescs },
	{ "shotgun", "shotgun", MI_COP, awc(aShotgunAnimations), aWeaponAnimDescs },
	{ "buddy", "buddy", MI_COP, awc(aBuddyAnimations), aWeaponAnimDescs },
	{ "tec", "tec", MI_COP, awc(aTecAnimations), aWeaponAnimDescs },
	{ "uzi", "uzi", MI_COP, awc(aUziAnimations), aWeaponAnimDescs },
	{ "rifle", "rifle", MI_COP, awc(aRifleAnimations), aWeaponAnimDescs },
	{ "m60", "m60", MI_COP, awc(aM60Animations), aWeaponAnimDescs },
	{ "sniper", "sniper", MI_COP, awc(aSniperAnimations), aWeaponAnimDescs },
	{ "grenade", "grenade", MI_COP, awc(aThrowAnimations), aWeaponAnimDescs },
	{ "flame", "flame", MI_COP, awc(aFlamethrowerAnimations), aWeaponAnimDescs },
	{ "medic", "medic", MI_COP, awc(aMedicAnimations), aMedicAnimDescs },
	{ "sunbathe", "sunbathe", MI_COP, 1, aSunbatheAnimations, aSunbatheAnimDescs },	// NB: not using awc here!
	{ "playidles", "playidles", MI_COP, awc(aPlayerIdleAnimations), aPlayerIdleAnimDescs },
	{ "riot", "riot", MI_COP, awc(aRiotAnimations), aRiotAnimDescs },
	{ "strip", "strip", MI_COP, awc(aStripAnimations), aStripAnimDescs },
	{ "lance", "lance", MI_COP, awc(aLanceAnimations), aSunbatheAnimDescs },
	{ "player", "ped", MI_COP, awc(aPlayerAnimations), aStdAnimDescs },
	{ "playerrocket", "ped", MI_COP, awc(aPlayerWithRocketAnimations), aStdAnimDescs },
	{ "player1armed", "ped", MI_COP, awc(aPlayer1ArmedAnimations), aStdAnimDescs },
	{ "player2armed", "ped", MI_COP, awc(aPlayer2ArmedAnimations), aStdAnimDescs },
	{ "playerBBBat", "ped", MI_COP, awc(aPlayerBBBatAnimations), aStdAnimDescs },
	{ "playercsaw", "ped", MI_COP, awc(aPlayerChainsawAnimations), aStdAnimDescs },
	{ "shuffle", "ped", MI_COP, awc(aShuffleAnimations), aStdAnimDescs },
	{ "oldman", "ped", MI_COP, awc(aOldAnimations), aStdAnimDescs },
	{ "gang1", "ped", MI_COP, awc(aGang1Animations), aStdAnimDescs },
	{ "gang2", "ped", MI_COP, awc(aGang2Animations), aStdAnimDescs },
	{ "fatman", "ped", MI_COP, awc(aFatAnimations), aStdAnimDescs },
	{ "oldfatman", "ped", MI_COP, awc(aOldFatAnimations), aStdAnimDescs },
	{ "jogger", "ped", MI_COP, awc(aJoggerAnimations), aStdAnimDescs },
	{ "woman", "ped", MI_COP, awc(aStdWomanAnimations), aStdAnimDescs },
	{ "shopping", "ped", MI_COP, awc(aWomanShopAnimations), aStdAnimDescs },
	{ "busywoman", "ped", MI_COP, awc(aBusyWomanAnimations), aStdAnimDescs },
	{ "sexywoman", "ped", MI_COP, awc(aSexyWomanAnimations), aStdAnimDescs },
	{ "fatwoman", "ped", MI_COP, awc(aFatWomanAnimations), aStdAnimDescs },
	{ "oldwoman", "ped", MI_COP, awc(aOldWomanAnimations), aStdAnimDescs },
	{ "jogwoman", "ped", MI_COP, awc(aJoggerWomanAnimations), aStdAnimDescs },
	{ "panicchunky", "ped", MI_COP, awc(aPanicChunkyAnimations), aStdAnimDescs },
	{ "skate", "skate", MI_COP, awc(aSkateAnimations), aStdAnimDescs },
#ifdef PC_PLAYER_CONTROLS
	{ "playerback", "ped", MI_COP, awc(aPlayerStrafeBackAnimations), aStdAnimDescs },
	{ "playerleft", "ped", MI_COP, awc(aPlayerStrafeLeftAnimations), aStdAnimDescsSide },
	{ "playerright", "ped", MI_COP, awc(aPlayerStrafeRightAnimations), aStdAnimDescsSide },
	{ "rocketback", "ped", MI_COP, awc(aRocketStrafeBackAnimations), aStdAnimDescs },
	{ "rocketleft", "ped", MI_COP, awc(aRocketStrafeLeftAnimations), aStdAnimDescsSide },
	{ "rocketright", "ped", MI_COP, awc(aRocketStrafeRightAnimations), aStdAnimDescsSide },
	{ "csawback", "ped", MI_COP, awc(aChainsawStrafeBackAnimations), aStdAnimDescs },
	{ "csawleft", "ped", MI_COP, awc(aChainsawStrafeLeftAnimations), aStdAnimDescsSide },
	{ "csawright", "ped", MI_COP, awc(aChainsawStrafeRightAnimations), aStdAnimDescsSide },
#endif
};
#undef awc

void
CAnimManager::SetVCPedAnimationReference(RpClump *clump)
{
	if(ms_pVCPedAnimationReference == clump)
		return;
	if(ms_pVCPedAnimationReference)
		RpClumpDestroy(ms_pVCPedAnimationReference);
	ms_pVCPedAnimationReference = clump;
}

void
CAnimManager::Initialise(void)
{
	ms_numAnimations = 0;
	ms_numAnimBlocks = 0;
	ms_aAnimAssocGroups = nil;
	ms_aSAAnimAssocGroups = nil;
	SetVCPedAnimationReference(nil);
	ms_animCache.Init(25);
}

void
CAnimManager::Shutdown(void)
{
	int i;

	for(i = 0; i < NUMANIMBLOCKS; i++)
		CStreaming::RemoveAnim(i);

	for(i = 0; i < ms_numAnimations; i++)
		ms_aAnimations[i].Shutdown();

	ms_animCache.Shutdown();

	delete[] ms_aAnimAssocGroups;
	ms_aAnimAssocGroups = nil;
	delete[] ms_aSAAnimAssocGroups;
	ms_aSAAnimAssocGroups = nil;
	SetVCPedAnimationReference(nil);
}

void
CAnimManager::UncompressAnimation(CAnimBlendHierarchy *hier)
{
	if(hier->keepCompressed){
		if(hier->totalLength == 0.0f)
			hier->CalcTotalTimeCompressed();
	}else{
		if(!hier->compressed){
			if(hier->linkPtr){
				hier->linkPtr->Remove();
				ms_animCache.head.Insert(hier->linkPtr);
			}
		}else{
			CLink<CAnimBlendHierarchy*> *link = ms_animCache.Insert(hier);
			if(link == nil){
				CAnimBlendHierarchy *lastHier = ms_animCache.tail.prev->item;
				lastHier->RemoveUncompressedData();
				ms_animCache.Remove(ms_animCache.tail.prev);
				lastHier->linkPtr = nil;
				link = ms_animCache.Insert(hier);
			}
			hier->linkPtr = link;
			hier->Uncompress();
		}
	}
}

void
CAnimManager::RemoveFromUncompressedCache(CAnimBlendHierarchy *hier)
{
	if(hier->linkPtr){
		ms_animCache.Remove(hier->linkPtr);
		hier->linkPtr = nil;
	}
}

CAnimBlock*
CAnimManager::GetAnimationBlock(const char *name)
{
	int i;

	for(i = 0; i < ms_numAnimBlocks; i++)
		if(strcasecmp(ms_aAnimBlocks[i].name, name) == 0)
			return &ms_aAnimBlocks[i];
	return nil;
}

int32
CAnimManager::GetAnimationBlockIndex(const char *name)
{
	int i;

	for(i = 0; i < ms_numAnimBlocks; i++)
		if(strcasecmp(ms_aAnimBlocks[i].name, name) == 0)
			return i;
	return -1;
}

int32
CAnimManager::GetAnimationBlockForHierarchy(CAnimBlendHierarchy *hierarchy)
{
	if(hierarchy == nil)
		return -1;
	for(int32 i = 0; i < ms_numAnimBlocks; i++){
		CAnimBlock *block = &ms_aAnimBlocks[i];
		if(block->firstIndex < 0 || block->firstIndex > NUMANIMATIONS || block->numAnims <= 0 ||
		   block->numAnims > NUMANIMATIONS - block->firstIndex)
			continue;
		if(hierarchy >= &ms_aAnimations[block->firstIndex] &&
		   hierarchy < &ms_aAnimations[block->firstIndex + block->numAnims])
			return i;
	}
	return -1;
}

int32
CAnimManager::RegisterAnimBlock(const char *name)
{
	CAnimBlock *animBlock = GetAnimationBlock(name);
	if(animBlock == nil){
		if(ms_numAnimBlocks >= NUMANIMBLOCKS){
			debug("ANIMS: no room to register block %s\n", name);
			return -1;
		}
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, name, MAX_ANIMBLOCK_NAME - 1);
		animBlock->name[MAX_ANIMBLOCK_NAME - 1] = '\0';
		animBlock->isLoaded = false;
		animBlock->refCount = 0;
		animBlock->firstIndex = 0;
		animBlock->numAnims = 0;
		animBlock->unloadPending = false;
	}
	return animBlock - ms_aAnimBlocks;
}

int32
CAnimManager::GetNumRefsToAnimBlock(int32 block)
{
	return block >= 0 && block < ms_numAnimBlocks ? ms_aAnimBlocks[block].refCount : 0;
}

void
CAnimManager::AddAnimBlockRef(int32 block)
{
	if(block >= 0 && block < ms_numAnimBlocks)
		ms_aAnimBlocks[block].refCount++;
}

static void
RemovePendingAnimationBlock(int32 block)
{
	CAnimBlock *animBlock = CAnimManager::GetAnimationBlock(block);
	if(strncasecmp(animBlock->name, "sa_", 3) == 0 || !CStreaming::HasAnimLoaded(block))
		CAnimManager::RemoveAnimBlock(block);
	else
		CStreaming::RemoveAnim(block);
}

void
CAnimManager::RemoveAnimBlockRefWithoutDelete(int32 block)
{
	CAnimBlock *animBlock;
	if(block < 0 || block >= ms_numAnimBlocks || ms_aAnimBlocks[block].refCount <= 0)
		return;
	animBlock = &ms_aAnimBlocks[block];
	animBlock->refCount--;
	if(animBlock->refCount == 0 && animBlock->unloadPending)
		RemovePendingAnimationBlock(block);
}

void
CAnimManager::RemoveAnimBlockRef(int32 block)
{
	if(block < 0 || block >= ms_numAnimBlocks || ms_aAnimBlocks[block].refCount <= 0)
		return;
	ms_aAnimBlocks[block].refCount--;
	if(ms_aAnimBlocks[block].refCount == 0){
		if(ms_aAnimBlocks[block].unloadPending)
			RemovePendingAnimationBlock(block);
		else
			CStreaming::RemoveAnim(block);
	}
}

void
CAnimManager::RemoveAnimBlock(int32 block)
{
	int i;
	CAnimBlock *animblock;
	int32 firstAnim, numAnims;
	bool validAnimationRange;

	animblock = &ms_aAnimBlocks[block];
	firstAnim = animblock->firstIndex;
	numAnims = animblock->numAnims;
	validAnimationRange = firstAnim >= 0 && firstAnim <= NUMANIMATIONS &&
		numAnims >= 0 && numAnims <= NUMANIMATIONS - firstAnim;
	// Parallel SA banks are loaded alongside their VC block but are not a
	// separate streaming request. Release them with the VC source block so
	// custom dictionaries do not accumulate in the fixed animation table.
	if(strncasecmp(animblock->name, "sa_", 3) != 0){
		char saBlockName[MAX_ANIMBLOCK_NAME];
		CAnimBlock *saBlock;
		snprintf(saBlockName, sizeof(saBlockName), "sa_%s", animblock->name);
		saBlock = GetAnimationBlock(saBlockName);
		if(saBlock && saBlock != animblock && saBlock->isLoaded){
			if(saBlock->refCount > 0)
				saBlock->unloadPending = true;
			else
				RemoveAnimBlock(saBlock - ms_aAnimBlocks);
		}
	}
	debug("Removing ANIMS %s\n", animblock->name);
	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		CAnimBlendAssocGroup *groups[2] = {
			ms_aAnimAssocGroups ? &ms_aAnimAssocGroups[i] : nil,
			ms_aSAAnimAssocGroups ? &ms_aSAAnimAssocGroups[i] : nil
		};
		for(int g = 0; g < ARRAY_SIZE(groups); g++){
			CAnimBlendAssocGroup *group = groups[g];
			bool usesBlock = group && group->animBlock == animblock;
			if(group && !usesBlock && group->assocList){
				for(int j = 0; j < group->numAssociations; j++){
					CAnimBlendHierarchy *hier = group->assocList[j].hierarchy;
					if(validAnimationRange && hier && hier >= &ms_aAnimations[firstAnim] &&
					   hier < &ms_aAnimations[firstAnim + numAnims]){
						usesBlock = true;
						break;
					}
				}
			}
			if(usesBlock)
				group->DestroyAssociations();
		}
	}
	if(validAnimationRange){
		for(i = 0; i < numAnims; i++)
			ms_aAnimations[firstAnim + i].Shutdown();
	}else{
		debug("ANIMS %s: invalid animation table range, skipping hierarchy cleanup\n", animblock->name);
	}
	animblock->isLoaded = false;
	animblock->refCount = 0;
	animblock->unloadPending = false;
}

CAnimBlendHierarchy*
CAnimManager::GetAnimation(const char *name, CAnimBlock *animBlock)
{
	int i;
	CAnimBlendHierarchy *hier;
	if(name == nil || animBlock == nil || animBlock->firstIndex < 0 ||
	   animBlock->firstIndex >= NUMANIMATIONS || animBlock->numAnims <= 0 ||
	   animBlock->numAnims > NUMANIMATIONS - animBlock->firstIndex)
		return nil;
	hier = &ms_aAnimations[animBlock->firstIndex];

	for(i = 0; i < animBlock->numAnims; i++){
		if(strcasecmp(hier->name, name) == 0)
			return hier;
		hier++;
	}
	return nil;
}

const char*
CAnimManager::GetAnimGroupName(AssocGroupId groupId)
{
	return ms_aAnimAssocDefinitions[groupId].name;
}

CAnimBlendAssocGroup*
CAnimManager::GetAnimAssocGroup(RpClump *clump, AssocGroupId groupId)
{
	CAnimBlendClumpData *clumpData = clump ? *RPANIMBLENDCLUMPDATA(clump) : nil;
	CAnimBlendAssocGroup *group;
	if(groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	if(clumpData && clumpData->usesSAAnimations){
		if(ms_aSAAnimAssocGroups){
			group = &ms_aSAAnimAssocGroups[groupId];
			if(group->assocList && group->numAssociations > 0 &&
			   group->animBlock && group->animBlock->isLoaded)
				return group;
		}
		return nil;
	}
	return ms_aAnimAssocGroups ? &ms_aAnimAssocGroups[groupId] : nil;
}

CAnimBlendAssociation*
CAnimManager::CreateAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	if(ms_aAnimAssocGroups == nil || groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	return ms_aAnimAssocGroups[groupId].CopyAnimation(animId);
}

CAnimBlendAssociation*
CAnimManager::CreateAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	if(clump == nil)
		return nil;
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	return group ? group->CopyAnimation(animId, clump) : nil;
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	if(ms_aAnimAssocGroups == nil || groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	return ms_aAnimAssocGroups[groupId].GetAnimation(animId);
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(AssocGroupId groupId, const char *name)
{
	if(ms_aAnimAssocGroups == nil || groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	return ms_aAnimAssocGroups[groupId].GetAnimation(name);
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	return group ? group->GetAnimation(animId) : nil;
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(RpClump *clump, AssocGroupId groupId, const char *name)
{
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	return group ? group->GetAnimation(name) : nil;
}

CAnimBlendAssociation*
CAnimManager::AddAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *anim = CreateAnimAssociation(clump, groupId, animId);
	if(anim == nil || clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if(anim->IsMovement()){
		CAnimBlendAssociation *syncanim = nil;
		CAnimBlendLink *link;
		for(link = clumpData->link.next; link; link = link->next){
			syncanim = CAnimBlendAssociation::FromLink(link);
			if(syncanim->IsMovement())
				break;
		}
		if(link){
			anim->SyncAnimation(syncanim);
			anim->flags |= ASSOC_RUNNING;
		}else
			anim->Start(0.0f);
	}else
		anim->Start(0.0f);

	clumpData->link.Prepend(&anim->link);
	return anim;
}

CAnimBlendAssociation*
CAnimManager::AddAnimationAndSync(RpClump *clump, CAnimBlendAssociation *syncanim, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *anim = CreateAnimAssociation(clump, groupId, animId);
	if(anim == nil || clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if (anim->IsMovement() && syncanim){
		anim->SyncAnimation(syncanim);
		anim->flags |= ASSOC_RUNNING;
	}else
		anim->Start(0.0f);

	clumpData->link.Prepend(&anim->link);
	return anim;
}

CAnimBlendAssociation*
CAnimManager::BlendAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId, float delta)
{
	int removePrevAnim = 0;
	if(clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if(clumpData == nil)
		return nil;
	CAnimBlendAssociation *anim = GetAnimAssociation(clump, groupId, animId);
	if(anim == nil)
		return nil;
	bool isMovement = anim->IsMovement();
	bool isPartial = anim->IsPartial();
	CAnimBlendLink *link;
	CAnimBlendAssociation *found = nil, *movementAnim = nil;
	for(link = clumpData->link.next; link; link = link->next){
		anim = CAnimBlendAssociation::FromLink(link);
		if(isMovement && anim->IsMovement())
			movementAnim = anim;
		if(anim->animId == animId)
			found = anim;
		else{
			if(isPartial == anim->IsPartial()){
				if(anim->blendAmount > 0.0f){
					float blendDelta = -delta*anim->blendAmount;
					if(blendDelta < anim->blendDelta || !isPartial)
						anim->blendDelta = blendDelta;
				}else{
					anim->blendDelta = -1.0f;
				}
				anim->flags |= ASSOC_DELETEFADEDOUT;
				removePrevAnim = 1;
			}
		}
	}
	if(found){
		found->blendDelta = (1.0f - found->blendAmount)*delta;
		if(!found->IsRunning() && found->currentTime == found->hierarchy->totalLength)
			found->Start(0.0f);
	}else{
		found = AddAnimationAndSync(clump, movementAnim, groupId, animId);
		if(!removePrevAnim && !isPartial){
			found->blendAmount = 1.0f;
			return found;
		}
		found->blendAmount = 0.0f;
		found->blendDelta = delta;
	}
	UncompressAnimation(found->hierarchy);
	return found;
}

static bool
IsAnimAssocGroupReady(CAnimBlendAssocGroup *group)
{
	return group && group->assocList && group->numAssociations > 0 &&
		group->animBlock && group->animBlock->isLoaded;
}

static CPedModelInfo *
FindPedAnimationReference(int preferredModelIndex, bool wantSA)
{
	CBaseModelInfo *baseInfo;
	CPedModelInfo *pedInfo;
	int i;

	// Keep the native VC groups anchored to the same cop skeleton the original
	// game uses. For SA groups, prefer the model currently being set up, then
	// search any other loaded SA ped for the reference hierarchy.
	if(!wantSA){
		baseInfo = CModelInfo::GetModelInfo(MI_COP);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED && baseInfo->GetRwObject()){
			pedInfo = (CPedModelInfo*)baseInfo;
			if(!pedInfo->UsesSAAnimations())
				return pedInfo;
		}
	}
	if(preferredModelIndex >= MI_PLAYER && preferredModelIndex <= MI_LAST_PED){
		baseInfo = CModelInfo::GetModelInfo(preferredModelIndex);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED && baseInfo->GetRwObject()){
			pedInfo = (CPedModelInfo*)baseInfo;
			if(pedInfo->UsesSAAnimations() == wantSA)
				return pedInfo;
		}
	}

	for(i = MI_PLAYER; i <= MI_LAST_PED; i++){
		baseInfo = CModelInfo::GetModelInfo(i);
		if(baseInfo == nil || baseInfo->GetModelType() != MITYPE_PED || baseInfo->GetRwObject() == nil)
			continue;
		pedInfo = (CPedModelInfo*)baseInfo;
		if(pedInfo->UsesSAAnimations() == wantSA)
			return pedInfo;
	}
	return nil;
}

static const char *
GetSAAnimationAlias(int groupId, const char *name, const char **sourceBlock)
{
	if(sourceBlock)
		*sourceBlock = nil;
	if(name == nil)
		return nil;

	if(groupId == ASSOCGRP_FAT){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fat") == 0) return "fatwalk";
		if(strcasecmp(name, "run_civi") == 0) return "fatrun";
		if(strcasecmp(name, "sprint_panic") == 0 || strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "idle_stance") == 0) return "fatidle";
		if(strcasecmp(name, "walk_start") == 0) return "fatwalkstart";
	}
	if(groupId == ASSOCGRP_OLDFAT){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fatold") == 0) return "fatwalk";
		if(strcasecmp(name, "run_fatold") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "idle_stance") == 0) return "fatidle";
	}
	if(groupId == ASSOCGRP_FATWOMAN){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fat") == 0) return "fatwalk";
		if(strcasecmp(name, "woman_run") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "woman_idlestance") == 0) return "fatidle";
	}
	if(groupId == ASSOCGRP_PANICCHUNKY){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "run_fatold") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
	}
	if(groupId == ASSOCGRP_OLD && strcasecmp(name, "walk_old") == 0)
		return "walk_civi";
	if((groupId == ASSOCGRP_GANG1 || groupId == ASSOCGRP_GANG2) &&
	   (strcasecmp(name, "walk_gang1") == 0 || strcasecmp(name, "walk_gang2") == 0))
		return "walk_civi";
	if((groupId == ASSOCGRP_GANG1 || groupId == ASSOCGRP_GANG2) && strcasecmp(name, "run_gang1") == 0)
		return "run_civi";
	if((groupId == ASSOCGRP_JOGGER || groupId == ASSOCGRP_JOGWOMAN) &&
	   (strcasecmp(name, "JOG_maleA") == 0 || strcasecmp(name, "JOG_maleB") == 0))
		return "walk_civi";
	if(groupId == ASSOCGRP_SHUFFLE && strcasecmp(name, "WALK_shuffle") == 0)
		return "walk_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_WOMANSHOP ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   (strncasecmp(name, "woman_walk", 10) == 0 || strcasecmp(name, "JOG_maleB") == 0))
		return "walk_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_WOMANSHOP ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_run") == 0)
		return "run_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_WOMANSHOP ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_runpanic") == 0)
		return "sprint_panic";
	if(groupId == ASSOCGRP_FATWOMAN && strcasecmp(name, "woman_run") == 0)
		return "fatrun";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_WOMANSHOP ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_idlestance") == 0)
		return "idle_stance";
	if(groupId == ASSOCGRP_PLAYER1ARMED && strcasecmp(name, "run_1armed") == 0)
		return "run_armed";
	if(groupId == ASSOCGRP_TEC){
		if(sourceBlock) *sourceBlock = "sa_uzi";
		if(strcasecmp(name, "TEC_fire") == 0) return "UZI_fire";
		if(strcasecmp(name, "TEC_crouchfire") == 0) return "UZI_crouchfire";
		if(strcasecmp(name, "TEC_reload") == 0) return "UZI_reload";
		if(strcasecmp(name, "TEC_crouchreload") == 0) return "UZI_crouchreload";
	}
	if(groupId == ASSOCGRP_M60){
		if(sourceBlock) *sourceBlock = "sa_rifle";
		if(strcasecmp(name, "M60_fire") == 0) return "RIFLE_fire";
		if(strcasecmp(name, "M60_reload") == 0) return "RIFLE_load";
	}
	if(groupId == ASSOCGRP_SNIPER){
		if(sourceBlock) *sourceBlock = "sa_rifle";
		if(strcasecmp(name, "WEAPON_sniper") == 0) return "RIFLE_fire";
	}
	if(groupId == ASSOCGRP_COLT){
		if(sourceBlock) *sourceBlock = "sa_colt45";
		if(strcasecmp(name, "colt45_cop") == 0) return "colt45_fire_2hands";
	}
	if(groupId == ASSOCGRP_SUNBATHE){
		if(sourceBlock) *sourceBlock = "sa_sunbathe";
		if(strcasecmp(name, "bather") == 0) return "Lay_Bac_in";
		if(strcasecmp(name, "batherscape") == 0) return "batherup";
	}
	if(groupId == ASSOCGRP_LANCE && strcasecmp(name, "lance") == 0){
		if(sourceBlock) *sourceBlock = "sa_ped";
		return "idle_stance";
	}
	if(groupId == ASSOCGRP_STD){
		if(strcasecmp(name, "FIGHTsh_F") == 0) return "FightShF";
		if(strcasecmp(name, "FIGHTsh_back") == 0) return "FightShB";
		if(strcasecmp(name, "KO_shot_arml") == 0 || strcasecmp(name, "KO_shot_armR") == 0 ||
		   strcasecmp(name, "KO_shot_legl") == 0 || strcasecmp(name, "KO_shot_legR") == 0)
			return "KO_shot_front";
		if(strcasecmp(name, "HIT_bodyblow") == 0 || strcasecmp(name, "HIT_chest") == 0 ||
		   strcasecmp(name, "HIT_head") == 0)
			return "HIT_front";
		if(strcasecmp(name, "FIGHTIDLE") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FIGHTIDLE";
		}
		if(strcasecmp(name, "FIGHTbodyblow") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHThead") == 0 || strcasecmp(name, "FIGHTpunch") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_2";
		}
		if(strcasecmp(name, "FIGHTkick") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick";
		}
		if(strcasecmp(name, "FIGHTknee") == 0 || strcasecmp(name, "FIGHTrndhse") == 0 ||
		   strcasecmp(name, "FIGHTbkickL") == 0 || strcasecmp(name, "FIGHTbkickR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_3";
		}
		if(strcasecmp(name, "FIGHTLhook") == 0 || strcasecmp(name, "FIGHTppunch") == 0 ||
		   strcasecmp(name, "FIGHTjab") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHTlngkck") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick_B";
		}
		if(strcasecmp(name, "FIGHTelbowL") == 0 || strcasecmp(name, "FIGHTelbowR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_2";
		}
		if(strcasecmp(name, "punchR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "KICK_floor") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick_B";
		}
		if(strcasecmp(name, "WEAPON_throwu") == 0){
			if(sourceBlock) *sourceBlock = "sa_grenade";
			return "WEAPON_throwu";
		}
		if(strcasecmp(name, "DUCK_down") == 0 || strcasecmp(name, "DUCK_low") == 0)
			return "DUCK_cower";
		if(strcasecmp(name, "RBLOCK_Cshoot") == 0)
			return "WEAPON_crouch";
		if(strncasecmp(name, "CAR_", 4) == 0 || strncasecmp(name, "car_", 4) == 0){
			bool lowCar = strcasecmp(name, "CAR_pulloutL_LHS") == 0 ||
				strcasecmp(name, "CAR_pulloutL_RHS") == 0 ||
				strcasecmp(name, "CAR_getinL_LHS") == 0 ||
				strcasecmp(name, "CAR_getinL_RHS") == 0 ||
				strcasecmp(name, "CAR_closedoorL_LHS") == 0 ||
				strcasecmp(name, "CAR_closedoorL_RHS") == 0 ||
				strcasecmp(name, "CAR_Lshuffle_RHS") == 0 ||
				strcasecmp(name, "CAR_getoutL_LHS") == 0 ||
				strcasecmp(name, "CAR_getoutL_RHS") == 0 ||
				strcasecmp(name, "CAR_LjackedLHS") == 0 ||
				strcasecmp(name, "CAR_LjackedRHS") == 0 ||
				strcasecmp(name, "CAR_rolldoorLO") == 0;
			if(sourceBlock) *sourceBlock = lowCar ? "sa_lowcaramims" : "sa_stdcaramims";
			if(strcasecmp(name, "CAR_jumpin_LHS") == 0) return "CAR_getin_LHS";
			if(strcasecmp(name, "CAR_jumpin_RHS") == 0) return "CAR_getin_RHS";
			if(strcasecmp(name, "CAR_Qjack") == 0 || strcasecmp(name, "CAR_Qjacked") == 0)
				return "CAR_jackedRHS";
		}
	}
	if(groupId == ASSOCGRP_VAN){
		if(sourceBlock) *sourceBlock = "sa_vancaranims";
		if(strcasecmp(name, "VAN_openL") == 0) return "VAN_open_back_LHS";
		if(strcasecmp(name, "VAN_getinL") == 0) return "VAN_getin_back_LHS";
		if(strcasecmp(name, "VAN_closeL") == 0) return "VAN_close_back_LHS";
		if(strcasecmp(name, "VAN_getoutL") == 0) return "VAN_getout_back_LHS";
		if(strcasecmp(name, "VAN_open") == 0) return "VAN_open_back_RHS";
		if(strcasecmp(name, "VAN_getin") == 0) return "VAN_getin_back_RHS";
		if(strcasecmp(name, "VAN_close") == 0) return "VAN_close_back_RHS";
		if(strcasecmp(name, "VAN_getout") == 0) return "VAN_getout_back_RHS";
	}
	if(groupId == ASSOCGRP_COACH && sourceBlock)
		*sourceBlock = "sa_coachcaranims";
	if(groupId == ASSOCGRP_BIKE_STANDARD && sourceBlock)
		*sourceBlock = "sa_sportbikeanims";
	if(groupId == ASSOCGRP_BIKE_VESPA && sourceBlock)
		*sourceBlock = "sa_vespabikeanims";
	if(groupId == ASSOCGRP_BIKE_HARLEY && sourceBlock)
		*sourceBlock = "sa_harleybikeanims";
	if(groupId == ASSOCGRP_BIKE_DIRT && sourceBlock)
		*sourceBlock = "sa_dirtbikeanims";
	if(groupId == ASSOCGRP_UNARMED || groupId == ASSOCGRP_SCREWDRIVER){
		if(strcasecmp(name, "punchR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHTbodyblow") == 0 || strcasecmp(name, "FIGHTppunch") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHTIDLE") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FIGHTIDLE";
		}
		if(strcasecmp(name, "KICK_floor") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick_B";
		}
	}
	if(groupId == ASSOCGRP_BASEBALLBAT || groupId == ASSOCGRP_GOLFCLUB){
		if(sourceBlock)
			*sourceBlock = groupId == ASSOCGRP_GOLFCLUB ? "sa_gclub_1" : "sa_bbbat_1";
		if(strcasecmp(name, "WEAPON_bat_h") == 0 || strcasecmp(name, "WEAPON_golfclub") == 0)
			return "Bat_1";
		if(strcasecmp(name, "WEAPON_bat_v") == 0)
			return "Bat_2";
		if(strcasecmp(name, "BAT_PART") == 0)
			return "BAT_PART";
	}
	if(groupId == ASSOCGRP_KNIFE){
		if(sourceBlock) *sourceBlock = "sa_knife_1";
		if(strcasecmp(name, "WEAPON_knife_1") == 0) return "knife_1";
		if(strcasecmp(name, "WEAPON_knife_2") == 0) return "knife_2";
		if(strcasecmp(name, "WEAPON_knife_3") == 0) return "knife_3";
		if(strcasecmp(name, "WEAPON_knifeidle") == 0) return "WEAPON_knifeidle";
	}
	if(groupId == ASSOCGRP_CHAINSAW){
		if(sourceBlock) *sourceBlock = "sa_csaw_1";
		if(strcasecmp(name, "WEAPON_csaw") == 0) return "CSaw_1";
		if(strcasecmp(name, "WEAPON_csawlo") == 0) return "CSaw_2";
		if(strcasecmp(name, "csaw_part") == 0) return "CSaw_Part";
	}
	return nil;
}

static const char *
GetSAStyleFallbackName(int groupId, const char *name)
{
	if(name == nil)
		return nil;
	if(groupId == ASSOCGRP_SKATE){
		if(strcasecmp(name, "skate_run") == 0) return "run_civi";
		if(strcasecmp(name, "skate_sprint") == 0) return "sprint_panic";
		if(strcasecmp(name, "skate_idle") == 0) return "idle_stance";
	}
#ifdef PC_PLAYER_CONTROLS
	if(groupId == ASSOCGRP_PLAYERBACK || groupId == ASSOCGRP_PLAYERLEFT ||
	   groupId == ASSOCGRP_PLAYERRIGHT){
		if(strncasecmp(name, "walk_start", 10) == 0) return "walk_start";
		if(strncasecmp(name, "walk_", 5) == 0) return "walk_player";
		if(strncasecmp(name, "run_", 4) == 0) return "run_player";
	}
	if(groupId == ASSOCGRP_ROCKETBACK || groupId == ASSOCGRP_ROCKETLEFT ||
	   groupId == ASSOCGRP_ROCKETRIGHT){
		if(strncasecmp(name, "walkst_rocket", 13) == 0) return "walk_start_rocket";
		if(strncasecmp(name, "walk_rocket", 11) == 0) return "walk_rocket";
		if(strncasecmp(name, "run_rocket", 10) == 0) return "run_rocket";
	}
	if(groupId == ASSOCGRP_CHAINSAWBACK || groupId == ASSOCGRP_CHAINSAWLEFT ||
	   groupId == ASSOCGRP_CHAINSAWRIGHT){
		if(strncasecmp(name, "walkst_csaw", 11) == 0) return "walk_start_csaw";
		if(strncasecmp(name, "walk_csaw", 9) == 0) return "walk_csaw";
		if(strncasecmp(name, "run_csaw", 8) == 0) return "run_csaw";
	}
#endif
	if(groupId != ASSOCGRP_SHUFFLE && groupId != ASSOCGRP_OLD &&
	   groupId != ASSOCGRP_GANG1 && groupId != ASSOCGRP_GANG2 &&
	   groupId != ASSOCGRP_FAT && groupId != ASSOCGRP_OLDFAT &&
	   groupId != ASSOCGRP_JOGGER && groupId != ASSOCGRP_WOMAN &&
	   groupId != ASSOCGRP_WOMANSHOP && groupId != ASSOCGRP_BUSYWOMAN &&
	   groupId != ASSOCGRP_SEXYWOMAN && groupId != ASSOCGRP_FATWOMAN &&
	   groupId != ASSOCGRP_OLDWOMAN && groupId != ASSOCGRP_JOGWOMAN &&
	   groupId != ASSOCGRP_PANICCHUNKY)
		return nil;
	if(strncasecmp(name, "walk_start", 10) == 0)
		return "walk_start";
	if(strcasecmp(name, "woman_runpanic") == 0 || strncasecmp(name, "sprint", 6) == 0)
		return "sprint_panic";
	if(strncasecmp(name, "walk", 4) == 0 || strncasecmp(name, "JOG_", 4) == 0)
		return "walk_civi";
	if(strncasecmp(name, "run", 3) == 0 || strncasecmp(name, "woman_run", 9) == 0)
		return "run_civi";
	if(strncasecmp(name, "idle", 4) == 0 || strcasecmp(name, "woman_idlestance") == 0)
		return "idle_stance";
	return nil;
}

static CAnimBlendHierarchy *
FindSAAnimationHierarchy(int groupId, const char *name, CAnimBlock *primaryBlock)
{
	const char *sourceBlockName = nil;
	const char *alias = GetSAAnimationAlias(groupId, name, &sourceBlockName);
	CAnimBlock *sourceBlock = sourceBlockName ? CAnimManager::GetAnimationBlock(sourceBlockName) : primaryBlock;
	CAnimBlendHierarchy *hier;

	if(alias && sourceBlock && sourceBlock->isLoaded){
		hier = CAnimManager::GetAnimation(alias, sourceBlock);
		if(hier)
			return hier;
	}
	if(alias && primaryBlock && primaryBlock != sourceBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(alias, primaryBlock);
		if(hier)
			return hier;
	}
	if(primaryBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(name, primaryBlock);
		if(hier)
			return hier;
	}
	if(sourceBlock && sourceBlock != primaryBlock && sourceBlock->isLoaded){
		hier = CAnimManager::GetAnimation(name, sourceBlock);
		if(hier)
			return hier;
	}
	const char *styleFallback = GetSAStyleFallbackName(groupId, name);
	if(styleFallback && primaryBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(styleFallback, primaryBlock);
		if(hier)
			return hier;
	}
	return nil;
}

static bool
HasNewSAAnimation(CAnimBlendAssocGroup *saGroup, int groupId, const AnimAssocDefinition *def, CAnimBlock *primaryBlock)
{
	if(saGroup == nil || saGroup->assocList == nil || def == nil)
		return false;
	int numAssociations = saGroup->numAssociations < def->numAnims ?
		saGroup->numAssociations : def->numAnims;
	for(int i = 0; i < numAssociations; i++)
		if(saGroup->assocList[i].hierarchy == nil &&
		   FindSAAnimationHierarchy(groupId, def->animNames[i], primaryBlock))
			return true;
	return false;
}

void
CAnimManager::LoadAnimFiles(void)
{
	LoadAnimFile("ANIM\\PED.IFP");
#ifdef CUSTOM_MODELS
	// SA splits fighting, weapon, style, car and bike clips across IFP banks.
	// Load optional banks used by VC's association slots so SA peds never need
	// to borrow hierarchies from a VC skeleton.
	static const char *saSupportAnimFiles[] = {
		"ANIM\\MELEE_1.IFP",
		"ANIM\\FIGHT_E.IFP",
		"ANIM\\BBBAT_1.IFP",
		"ANIM\\GCLUB_1.IFP",
		"ANIM\\KNIFE_1.IFP",
		"ANIM\\CSAW_1.IFP",
		"ANIM\\GRENADE.IFP",
		"ANIM\\UZI.IFP",
		"ANIM\\RIFLE.IFP",
		"ANIM\\COLT45.IFP",
		"ANIM\\SUNBATHE.IFP",
		"ANIM\\FAT.IFP",
		"ANIM\\STDCARAMIMS.IFP",
		"ANIM\\LOWCARAMIMS.IFP",
		"ANIM\\VANCARANIMS.IFP",
		"ANIM\\COACHCARANIMS.IFP",
		"ANIM\\SPORTBIKEANIMS.IFP",
		"ANIM\\VESPABIKEANIMS.IFP",
		"ANIM\\HARLEYBIKEANIMS.IFP",
		"ANIM\\DIRTBIKEANIMS.IFP",
		"ANIM\\BIKES.IFP",
		"ANIM\\BIKEV.IFP",
		"ANIM\\BIKEH.IFP",
		"ANIM\\BIKED.IFP"
	};
	for(int i = 0; i < ARRAY_SIZE(saSupportAnimFiles); i++)
		LoadAnimFileFromCustom(saSupportAnimFiles[i], true);
#endif
	ms_aAnimAssocGroups = new CAnimBlendAssocGroup[NUM_ANIM_ASSOC_GROUPS];
	ms_aSAAnimAssocGroups = new CAnimBlendAssocGroup[NUM_ANIM_ASSOC_GROUPS];
	CreateAnimAssocGroups();
}

void
CAnimManager::CreateAnimAssocGroups(int preferredModelIndex)
{
	if(ms_aAnimAssocGroups == nil)
		return;
	if(preferredModelIndex >= 0 && preferredModelIndex < MODELINFOSIZE){
		CBaseModelInfo *baseInfo = CModelInfo::GetModelInfo(preferredModelIndex);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED){
			CPedModelInfo *pedInfo = (CPedModelInfo*)baseInfo;
			int groupId = (int)pedInfo->m_animGroup;
			if(groupId >= 0 && groupId < NUM_ANIM_ASSOC_GROUPS){
				bool usesSA = pedInfo->UsesSAAnimations() && ms_aSAAnimAssocGroups;
				CAnimBlendAssocGroup *group = usesSA ?
					&ms_aSAAnimAssocGroups[groupId] : &ms_aAnimAssocGroups[groupId];
				// A partial SA group may gain aliases when another SA bank loads.
				if(!usesSA && IsAnimAssocGroupReady(group))
					return;
			}
		}
	}

	int i, j;
	bool needVCClump = false;
	bool needSAClump = false;
	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		CAnimBlock *vcBlock = GetAnimationBlock(def->blockName);
		if(vcBlock && vcBlock->isLoaded && !IsAnimAssocGroupReady(&ms_aAnimAssocGroups[i]))
			needVCClump = true;
		if(ms_aSAAnimAssocGroups){
			char saBlockName[MAX_ANIMBLOCK_NAME];
			CAnimBlock *saBlock;
			snprintf(saBlockName, sizeof(saBlockName), "sa_%s", def->blockName);
			saBlock = GetAnimationBlock(saBlockName);
			if(saBlock == nil || !saBlock->isLoaded)
				saBlock = GetAnimationBlock("sa_ped");
			if(saBlock && saBlock->isLoaded &&
			   (!IsAnimAssocGroupReady(&ms_aSAAnimAssocGroups[i]) ||
			    ms_aSAAnimAssocGroups[i].animBlock != saBlock ||
			    HasNewSAAnimation(&ms_aSAAnimAssocGroups[i], i, def, saBlock)))
				needSAClump = true;
		}
	}
	CPedModelInfo *vcReference = needVCClump ? FindPedAnimationReference(preferredModelIndex, false) : nil;
	CPedModelInfo *saReference = needSAClump ? FindPedAnimationReference(preferredModelIndex, true) : nil;
	RpClump *vcClump = vcReference ? (RpClump*)vcReference->CreateInstance() : nil;
	RpClump *saClump = saReference ? (RpClump*)saReference->CreateInstance() : nil;
	bool usingStoredVCReference = false;
	if(vcClump == nil && needVCClump && ms_pVCPedAnimationReference){
		vcClump = RpClumpClone(ms_pVCPedAnimationReference);
		usingStoredVCReference = vcClump != nil;
	}
#ifdef CUSTOM_MODELS
	CUSTOM_LOG("animation groups: references VC=%s SA=%s\n",
		vcReference ? vcReference->GetModelName() : usingStoredVCReference ? "stored native cop" : "none",
		saReference ? saReference->GetModelName() : "none");
#endif
	if(vcClump)
		RpAnimBlendClumpInit(vcClump);
	if(saClump){
		RpAnimBlendClumpInit(saClump);
		(*RPANIMBLENDCLUMPDATA(saClump))->usesSAAnimations = true;
	}

	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		CAnimBlendAssocGroup *vcGroup = &ms_aAnimAssocGroups[i];
		CAnimBlendAssocGroup *saGroup = ms_aSAAnimAssocGroups ? &ms_aSAAnimAssocGroups[i] : nil;
		CAnimBlock *vcBlock = GetAnimationBlock(def->blockName);

		if(vcClump && vcBlock && vcBlock->isLoaded && !IsAnimAssocGroupReady(vcGroup)){
			vcGroup->groupId = i;
			vcGroup->firstAnimId = def->animDescs[0].animId;
			vcGroup->CreateAssociations(def->blockName, vcClump, def->animNames, def->numAnims);
			for(j = 0; j < vcGroup->numAssociations; j++){
				CAnimBlendAssociation *assoc = vcGroup->GetAnimation(def->animDescs[j].animId);
				if(assoc && assoc->hierarchy)
					assoc->flags |= def->animDescs[j].flags;
			}
		}

		if(saGroup && saClump){
			char saBlockName[MAX_ANIMBLOCK_NAME];
			CAnimBlock *saBlock;
			CAnimBlendHierarchy *saHierarchies[256];
			snprintf(saBlockName, sizeof(saBlockName), "sa_%s", def->blockName);
			saBlock = GetAnimationBlock(saBlockName);
			if(saBlock == nil || !saBlock->isLoaded)
				saBlock = GetAnimationBlock("sa_ped");
			if(saBlock && saBlock->isLoaded && def->numAnims <= ARRAY_SIZE(saHierarchies) &&
			   (!IsAnimAssocGroupReady(saGroup) || saGroup->animBlock != saBlock ||
			    HasNewSAAnimation(saGroup, i, def, saBlock))){
				for(j = 0; j < def->numAnims; j++)
					saHierarchies[j] = FindSAAnimationHierarchy(i, def->animNames[j], saBlock);
				saGroup->groupId = i;
				saGroup->firstAnimId = def->animDescs[0].animId;
#ifdef CUSTOM_MODELS
				CUSTOM_LOG("animation groups: building SA %s from %s (%d slots)\n",
					def->name, saBlock->name, def->numAnims);
#endif
				saGroup->CreateAssociations(saBlock->name, saClump, saHierarchies, def->numAnims);
#ifdef CUSTOM_MODELS
				CUSTOM_LOG("animation groups: created SA %s; applying flags\n", def->name);
#endif
				for(j = 0; j < saGroup->numAssociations; j++){
					CAnimBlendAssociation *assoc = saGroup->GetAnimation(def->animDescs[j].animId);
					if(assoc && assoc->hierarchy)
						assoc->flags |= def->animDescs[j].flags;
				}
#ifdef CUSTOM_MODELS
				CUSTOM_LOG("animation groups: flags applied to SA %s\n", def->name);
#endif
			}
		}
	}

	if(vcClump){
		if(IsClumpSkinned(vcClump))
			RpClumpForAllAtomics(vcClump, AtomicRemoveAnimFromSkinCB, nil);
		RpClumpDestroy(vcClump);
	}
	if(saClump){
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: cleaning SA reference clump\n");
#endif
		if(IsClumpSkinned(saClump))
			RpClumpForAllAtomics(saClump, AtomicRemoveAnimFromSkinCB, nil);
		RpClumpDestroy(saClump);
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: SA reference clump destroyed\n");
#endif
	}
#ifdef CUSTOM_MODELS
	CUSTOM_LOG("animation groups: completed\n");
#endif
}

static void
MakeSAAnimBlockName(const char *filename, char *blockName, size_t blockNameSize)
{
	const char *base = filename;
	const char *p;
	const char *dot;
	char stem[MAX_ANIMBLOCK_NAME];
	size_t length, i;

	for(p = filename; *p; p++)
		if(*p == '/' || *p == '\\')
			base = p + 1;
	dot = strrchr(base, '.');
	length = dot ? (size_t)(dot - base) : strlen(base);
	if(length > sizeof(stem) - 4)
		length = sizeof(stem) - 4;
	for(i = 0; i < length; i++)
		stem[i] = (char)tolower((unsigned char)base[i]);
	stem[length] = '\0';
	snprintf(blockName, blockNameSize, "sa_%s", stem);
	if(blockNameSize > 0)
		blockName[blockNameSize - 1] = '\0';
}

static bool
IsCompactAnimFile(const uint8 *data, size_t size)
{
	return size >= 4 && (memcmp(data, "ANP2", 4) == 0 || memcmp(data, "ANP3", 4) == 0);
}

#ifdef CUSTOM_MODELS
// Called for both files read directly by path and streamed IFP blocks. A VC
// ANPK replaces the requested block; an SA/BR ANP2/3 is an additional sa_<name>
// bank, so we still load the original VC file for native models and groups.
// Returns true only when the native dictionary has been replaced.
bool
CAnimManager::LoadAnimFileFromCustom(const char *filename, bool compactOnly)
{
	std::vector<uint8> customBuf;
	if(!CCustomModels::LoadAnimFileFromCustom(filename, customBuf))
		return false;

	char (*names)[24] = (char (*)[24])malloc(sizeof(char[24]) * NUMANIMATIONS);
	if(names == nil){
		CUSTOM_LOG("animation %s: no memory to validate custom IFP, using game file\n", filename);
		return false;
	}
	char declaredBlock[24];
	int numNames = 0;
	bool clean = br::SniffAnimNames(customBuf.data(), customBuf.size(), names,
		NUMANIMATIONS, numNames, declaredBlock);
	bool compact = IsCompactAnimFile(customBuf.data(), customBuf.size());
	if(!clean || numNames <= 0){
		CUSTOM_LOG("animation %s: invalid IFP structure, using game file\n", filename);
		free(names);
		return false;
	}

	if(compact){
		char blockName[MAX_ANIMBLOCK_NAME];
		MakeSAAnimBlockName(filename, blockName, sizeof(blockName));
		CAnimBlock *loadedBlock = GetAnimationBlock(blockName);
		if(loadedBlock && loadedBlock->isLoaded){
			loadedBlock->unloadPending = false;
			free(names);
			return false;
		}
		if((loadedBlock == nil && ms_numAnimBlocks >= NUMANIMBLOCKS) ||
		   ((!loadedBlock || loadedBlock->numAnims != numNames) &&
		    numNames > NUMANIMATIONS - ms_numAnimations)){
			CUSTOM_LOG("animation %s: no room for the SA dictionary (%d clips), using game file\n",
				filename, numNames);
			free(names);
			return false;
		}
		RwMemory mem;
		mem.start = customBuf.data();
		mem.length = (uint32)customBuf.size();
		RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
		if(stream){
			LoadAnimFile(stream, true, nil, blockName);
			RwStreamClose(stream, nil);
			loadedBlock = GetAnimationBlock(blockName);
		}
		CUSTOM_LOG("animation %s: %s in parallel bank %s; VC dictionary kept\n",
			filename, loadedBlock && loadedBlock->isLoaded ? "loaded" : "FAILED to load",
			blockName);
		free(names);
		return false;
	}

	if(compactOnly){
		free(names);
		return false;
	}
	// A VC file must declare the same block name as the requested file. If it
	// doesn't, the streaming slot would remain not-loaded forever.
	const char *base = filename;
	for(const char *p = filename; *p; p++)
		if(*p == '/' || *p == '\\')
			base = p + 1;
	const char *dot = strrchr(base, '.');
	size_t length = dot ? (size_t)(dot - base) : strlen(base);
	char expected[MAX_ANIMBLOCK_NAME];
	if(length == 0 || length >= sizeof(expected)){
		CUSTOM_LOG("animation %s: invalid block name, using game file\n", filename);
		free(names);
		return false;
	}
	memcpy(expected, base, length);
	expected[length] = '\0';
	if(!br::VCAnimNameEqual(declaredBlock, expected)){
		CUSTOM_LOG("animation %s: declares block %s instead of %s, using game file\n",
			filename, declaredBlock, expected);
		free(names);
		return false;
	}
	if(br::VCAnimNameEqual(expected, "ped") && !br::VCAnimHasPedCore(names, numNames)){
		CUSTOM_LOG("animation %s: missing VC walk_civi/idle_stance, using game file\n", filename);
		free(names);
		return false;
	}
	CAnimBlock *block = GetAnimationBlock(expected);
	if((block == nil && ms_numAnimBlocks >= NUMANIMBLOCKS) ||
	   ((!block || block->numAnims != numNames) && numNames > NUMANIMATIONS - ms_numAnimations)){
		CUSTOM_LOG("animation %s: no room for custom VC dictionary, using game file\n", filename);
		free(names);
		return false;
	}
	RwMemory mem;
	mem.start = customBuf.data();
	mem.length = (uint32)customBuf.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream == nil){
		CUSTOM_LOG("animation %s: cannot open custom IFP stream, using game file\n", filename);
		free(names);
		return false;
	}
	LoadAnimFile(stream, true, nil);
	RwStreamClose(stream, nil);
	block = GetAnimationBlock(expected);
	CUSTOM_LOG("animation %s: %s from custom VC IFP (%d clips)\n", filename,
		block && block->isLoaded ? "loaded" : "FAILED to load", numNames);
	free(names);
	return block && block->isLoaded;
}
#endif

void
CAnimManager::LoadAnimFile(const char *filename)
{
#ifdef CUSTOM_MODELS
	if(LoadAnimFileFromCustom(filename))
		return;
#endif
	RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	if(stream == nil){
		REVC_LOG_ERROR("animation %s: cannot open game IFP", filename);
		return;
	}
	LoadAnimFile(stream, true);
	RwStreamClose(stream, nil);
}

void
CAnimManager::LoadAnimFile(RwStream *stream, bool compress, char (*uncompressedAnims)[32], const char *blockNameOverride)
{
	#define ROUNDSIZE(x) if((x) & 3) (x) += 4 - ((x)&3)
	struct IfpHeader {
		char ident[4];
		uint32 size;
	};
	IfpHeader anpk, info, name, dgan, cpan, anim;
	char buf[256];
	int j, k, l;
	float *fbuf = (float*)buf;

	// block name
	RwStreamRead(stream, &anpk, sizeof(IfpHeader));
	// San Andreas and Black Russia packages: compact header without the
	// ANPK chunk layers (see gta-reversed CAnimManager::LoadAnimFile)
	if(memcmp(anpk.ident, "ANP3", 4) == 0 || memcmp(anpk.ident, "ANP2", 4) == 0){
		LoadAnimFile_ANP23(stream, anpk.ident, compress, anpk.size, blockNameOverride);
		return;
	}
	ROUNDSIZE(anpk.size);
	RwStreamRead(stream, &info, sizeof(IfpHeader));
	ROUNDSIZE(info.size);
	RwStreamRead(stream, buf, info.size);
	CAnimBlock *animBlock = GetAnimationBlock(buf+4);
	if(animBlock){
		// A replacement may have a different number of clips from the old
		// dictionary. Reuse its slots only when they have the same capacity.
		if(animBlock->numAnims != *(int*)buf){
			animBlock->numAnims = *(int*)buf;
			animBlock->firstIndex = ms_numAnimations;
		}
	}else{
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, buf+4, MAX_ANIMBLOCK_NAME);
		animBlock->numAnims = *(int*)buf;
		animBlock->firstIndex = ms_numAnimations;
	}

	debug("Loading ANIMS %s\n", animBlock->name);
	animBlock->isLoaded = true;

	int animIndex = animBlock->firstIndex;
	for(j = 0; j < animBlock->numAnims; j++){
		assert(animIndex < ARRAY_SIZE(ms_aAnimations));
		CAnimBlendHierarchy *hier = &ms_aAnimations[animIndex++];

		// animation name
		RwStreamRead(stream, &name, sizeof(IfpHeader));
		ROUNDSIZE(name.size);
		RwStreamRead(stream, buf, name.size);
		hier->SetName(buf);

#ifdef ANIM_COMPRESSION
		bool compressHier = compress;
#else
		bool compressHier = false;
#endif
		if (uncompressedAnims) {
			for (int i = 0; uncompressedAnims[i][0]; i++) {
				if (!CGeneral::faststricmp(uncompressedAnims[i], hier->name)){
					debug("Loading %s uncompressed\n", hier->name);
					compressHier = false;
				}
			}
		}

		hier->compressed = compressHier;
		hier->keepCompressed = false;

		// DG info has number of nodes/sequences
		RwStreamRead(stream, (char*)&dgan, sizeof(IfpHeader));
		ROUNDSIZE(dgan.size);
		RwStreamRead(stream, (char*)&info, sizeof(IfpHeader));
		ROUNDSIZE(info.size);
		RwStreamRead(stream, buf, info.size);
		hier->numSequences = *(int*)buf;
		hier->sequences = new CAnimBlendSequence[hier->numSequences];

		CAnimBlendSequence *seq = hier->sequences;
		for(k = 0; k < hier->numSequences; k++, seq++){
			// Each node has a name and key frames
			RwStreamRead(stream, &cpan, sizeof(IfpHeader));
			ROUNDSIZE(dgan.size);
			RwStreamRead(stream, &anim, sizeof(IfpHeader));
			ROUNDSIZE(anim.size);
			RwStreamRead(stream, buf, anim.size);
			int numFrames = *(int*)(buf+28);
			seq->SetName(buf);
			if(anim.size == 44)
				seq->SetBoneTag(*(int*)(buf+40));
			if(numFrames == 0)
				continue;

			bool hasScale = false;
			bool hasTranslation = false;
			RwStreamRead(stream, &info, sizeof(info));
			if(strncmp(info.ident, "KRTS", 4) == 0){
				hasScale = true;
				seq->SetNumFrames(numFrames, true, compressHier);
			}else if(strncmp(info.ident, "KRT0", 4) == 0){
				hasTranslation = true;
				seq->SetNumFrames(numFrames, true, compressHier);
			}else if(strncmp(info.ident, "KR00", 4) == 0){
				seq->SetNumFrames(numFrames, false, compressHier);
			}
			if(strstr(seq->name, "L Toe"))
				debug("anim %s has toe keyframes\n", hier->name); // BUG: seq->name

			for(l = 0; l < numFrames; l++){
				if(hasScale){
					RwStreamRead(stream, buf, 0x2C);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);

					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						// scaling ignored
						kf->SetTime(fbuf[10]);	// absolute time here
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						// scaling ignored
						kf->deltaTime = fbuf[10];	// absolute time here
					}
				}else if(hasTranslation){
					RwStreamRead(stream, buf, 0x20);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);

					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						kf->SetTime(fbuf[7]);	// absolute time here
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						kf->deltaTime = fbuf[7];	// absolute time here
					}
				}else{
					RwStreamRead(stream, buf, 0x14);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();

					if(compressHier){
						KeyFrameCompressed *kf = (KeyFrameCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTime(fbuf[4]);	// absolute time here
					}else{
						KeyFrame *kf = (KeyFrame*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->deltaTime = fbuf[4];	// absolute time here
					}
				}
			}
		}

		if(!compressHier){
			hier->RemoveQuaternionFlips();
			hier->CalcTotalTime();
		}
	}
	if(animIndex > ms_numAnimations)
		ms_numAnimations = animIndex;
}

// ANP2/ANP3: one blockName[24] + numAnims, then animations without the ANPK
// chunk headers. Key frames are the same byte layouts this game stores
// (KeyFrame 0x14, KeyFrameTrans 0x20, KeyFrameCompressed 0xA,
// KeyFrameTransCompressed 0x10 - identical in San Andreas, verified against
// gta-reversed AnimSequenceFrames.h), so frames are read where they belong:
// floats through the same rotation handling as the ANPK reader, pre-compressed
// frames (frameType 3/4) straight into the compressed arrays.
void
CAnimManager::LoadAnimFile_ANP23(RwStream *stream, const char *ident, bool compress, uint32 rootSize, const char *blockNameOverride)
{
	char buf[256];
	float *fbuf = (float*)buf;
	int j, k, l;
	bool isANP3 = ident[3] == '3';

	char blockName[24];
	RwStreamRead(stream, blockName, sizeof(blockName));
	blockName[23] = '\0';
	uint32 numAnims;
	RwStreamRead(stream, &numAnims, sizeof(numAnims));

	// The SA mobile / Black Russia packages have no root size field: the
	// block name starts right behind the magic and every block carries an
	// extra framesAllocSize. Read through the PC layout eyes that is off by
	// four bytes - but the true fields are all in the bytes already read,
	// so recover them from there (the stream then sits exactly behind the
	// mobile block header, no seeking needed).
	if(numAnims > (uint32)NUMANIMATIONS && isANP3){
		uint32 mobileNumAnims;
		memcpy(&mobileNumAnims, blockName + 20, sizeof(mobileNumAnims));
		if(mobileNumAnims > 0 && mobileNumAnims <= (uint32)NUMANIMATIONS){
			char realName[24];
			memset(realName, 0, sizeof(realName));
			memcpy(realName, &rootSize, 4);
			memcpy(realName + 4, blockName, 20);
			memcpy(blockName, realName, sizeof(blockName));
			blockName[23] = '\0';
			numAnims = mobileNumAnims;
			debug("ANP3: size-less (SA mobile) package, block %s (%d anims)\n", blockName, numAnims);
		}
	}

	// A SA compact IFP must live beside (not replace) VC's block with the same
	// name. The override is derived from the custom filename, e.g. ped -> sa_ped.
	if(blockNameOverride){
		strncpy(blockName, blockNameOverride, sizeof(blockName) - 1);
		blockName[sizeof(blockName) - 1] = '\0';
	}

	if(numAnims == 0 || numAnims > (uint32)NUMANIMATIONS){
		debug("ANP%c: invalid animation count for block %s (%u)\n", ident[3], blockName, numAnims);
		return;
	}
	CAnimBlock *animBlock = GetAnimationBlock(blockName);
	if(animBlock && (animBlock->numAnims < 0 ||
	   (animBlock->numAnims > 0 &&
	    (animBlock->firstIndex < 0 || animBlock->firstIndex > NUMANIMATIONS ||
	     animBlock->numAnims > NUMANIMATIONS - animBlock->firstIndex ||
	     animBlock->numAnims != (int32)numAnims)))){
		debug("ANP%c: block %s has an invalid animation range or count (%d at %d, file has %u)\n",
			ident[3], blockName, animBlock->numAnims, animBlock->firstIndex, numAnims);
		return;
	}
	if((animBlock == nil || animBlock->numAnims == 0) &&
	   ms_numAnimations + (int32)numAnims > NUMANIMATIONS){
		debug("ANP%c: no room for block %s (%u anims)\n", ident[3], blockName, numAnims);
		return;
	}
	if(animBlock){
		if(animBlock->numAnims == 0){
			animBlock->numAnims = numAnims;
			animBlock->firstIndex = ms_numAnimations;
		}
	}else{
		if(ms_numAnimBlocks >= NUMANIMBLOCKS){
			debug("ANP%c: no room for block %s (%u anims)\n", ident[3], blockName, numAnims);
			return;
		}
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, blockName, MAX_ANIMBLOCK_NAME - 1);
		animBlock->name[MAX_ANIMBLOCK_NAME - 1] = '\0';
		animBlock->isLoaded = false;
		animBlock->refCount = 0;
		animBlock->unloadPending = false;
		animBlock->numAnims = numAnims;
		animBlock->firstIndex = ms_numAnimations;
	}
	debug("Loading ANIMS %s (ANP%c)\n", animBlock->name, ident[3]);
	animBlock->isLoaded = true;
	animBlock->unloadPending = false;

	int animIndex = animBlock->firstIndex;
	for(j = 0; j < (int)numAnims; j++){
		if(animIndex >= NUMANIMATIONS){
			debug("ANP%c: animation table full\n", ident[3]);
			return;
		}
		CAnimBlendHierarchy *hier = &ms_aAnimations[animIndex++];

		char aname[24];
		RwStreamRead(stream, aname, sizeof(aname));
		aname[23] = '\0';
		hier->SetName(aname);

		uint32 numSeq;
		RwStreamRead(stream, &numSeq, sizeof(numSeq));

		bool fileCompressed = false;
		if(isANP3){
			uint32 framesSize, flags;
			RwStreamRead(stream, &framesSize, sizeof(framesSize));
			RwStreamRead(stream, &flags, sizeof(flags));
			fileCompressed = (flags & 1) != 0;
			(void)framesSize;
		}

		bool compressHier = false;
#ifdef ANIM_COMPRESSION
		compressHier = compress;
#endif
		if(fileCompressed)
			compressHier = false;	// frames arrive compressed already
		bool hasCompressedFrames = false;
		hier->compressed = false;
		hier->keepCompressed = false;

		if(numSeq > 0x1000){	// garbage header, do not allocate gigabytes
			debug("ANP%c: %s has an impossible sequence count\n", ident[3], hier->name);
			return;
		}
		hier->numSequences = numSeq;
		hier->sequences = new CAnimBlendSequence[numSeq];
		CAnimBlendSequence *seq = hier->sequences;
		for(k = 0; k < (int)numSeq; k++, seq++){
			char seqName[24];
			RwStreamRead(stream, seqName, sizeof(seqName));
			seqName[23] = '\0';
			uint32 frameType, numFrames;
			int32 boneTag;
			RwStreamRead(stream, &frameType, sizeof(frameType));
			RwStreamRead(stream, &numFrames, sizeof(numFrames));
			RwStreamRead(stream, &boneTag, sizeof(boneTag));
			seq->SetName(seqName);
			seq->SetBoneTag(boneTag);
			if(numFrames == 0)
				continue;
			if(numFrames > 0xFFFF){
				debug("ANP%c: %s/%s has an impossible frame count (%d)\n",
					ident[3], hier->name, seq->name, numFrames);
				return;
			}
			if(frameType < 1 || frameType > 4){
				debug("ANP%c: unknown frame type %d in %s\n", ident[3], frameType, hier->name);
				return;	// the stream is positioned wrong from here on
			}
			bool hasTrans = frameType == 2 || frameType == 4;
			if(frameType == 3 || frameType == 4){
				// pre-compressed frames: the file layout is the struct layout
				seq->SetNumFrames(numFrames, hasTrans, true);
				uint8 *dst = (uint8*)seq->GetKeyFrameCompressed(0);
				size_t stride = hasTrans ? 16 : 10;
				RwStreamRead(stream, dst, (uint32)(stride * numFrames));
				hasCompressedFrames = true;
#ifndef ANIM_COMPRESSION
				// The game normally builds without animation-cache compression.
				// Expand SA's frames now so normal update code works in every build.
				seq->Uncompress();
#endif
				continue;
			}
			// float frames: handled exactly like the ANPK reader does
			seq->SetNumFrames(numFrames, hasTrans, compressHier);
			if(compressHier)
				hasCompressedFrames = true;
			for(l = 0; l < (int)numFrames; l++){
				if(hasTrans){
					RwStreamRead(stream, buf, 0x20);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);
					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						kf->SetTime(fbuf[7]);
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						kf->deltaTime = fbuf[7];
					}
				}else{
					RwStreamRead(stream, buf, 0x14);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					if(compressHier){
						KeyFrameCompressed *kf = (KeyFrameCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTime(fbuf[4]);
					}else{
						KeyFrame *kf = (KeyFrame*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->deltaTime = fbuf[4];
					}
				}
			}
		}
#ifdef ANIM_COMPRESSION
		hier->compressed = hasCompressedFrames;
#else
		hier->compressed = false;
#endif
		if(!hier->compressed){
			hier->RemoveQuaternionFlips();
			hier->CalcTotalTime();
		}
	}
	if(animIndex > ms_numAnimations)
		ms_numAnimations = animIndex;
}

void
CAnimManager::RemoveLastAnimFile(void)
{
	int i;
	ms_numAnimBlocks--;
	ms_numAnimations = ms_aAnimBlocks[ms_numAnimBlocks].firstIndex;
	for(i = 0; i < ms_aAnimBlocks[ms_numAnimBlocks].numAnims; i++)
		ms_aAnimations[ms_aAnimBlocks[ms_numAnimBlocks].firstIndex + i].Shutdown();
	ms_aAnimBlocks[ms_numAnimBlocks].isLoaded = false;
}
