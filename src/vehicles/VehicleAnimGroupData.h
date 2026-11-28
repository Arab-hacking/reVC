#ifndef __GTA_VEHICLEANIMGROUPDATA_H__
#define __GTA_VEHICLEANIMGROUPDATA_H__

#include "AnimManager.h"
#include "Vector.h"

// SA CVehicleAnimGroupData, ported from gta-reversed
// (source/game_sa/VehicleAnimGroupData.{h,cpp}).
//
// SA does not give every vehicle its own in-car animation set. It gives every
// vehicle one of 30 *animation groups*, and the `^` block of handling.cfg says
// which: two candidate AssocGroupIds, eighteen yes/no flags that choose between
// them per animation, five timings the get-in/get-out blending needs, and seven
// door offsets taken from the animation itself.
//
// Reference for the field meanings: https://projectcerbera.com/gta/sa/tutorials/handling#animation

// Whether an animation uses the second of the two groups. One bit per
// animation family, in the order handling.cfg lists them.
union sVehicleAnimDataGroupFlags
{
	uint32 intValue;
	struct {
		uint32 bAlign : 1;					// 0: which group CAR_ALIGN uses
		uint32 bOpenFrontDoorsOnExit : 1;
		uint32 bOpenRearDoorsOnExit : 1;
		uint32 bCanEnterFrontDoors : 1;
		uint32 bCanEnterRearDoors : 1;
		uint32 bCanPulloutPed : 1;
		uint32 bCloseFrontDoorWhenInside : 1;
		uint32 bCloseRearDoorWhenInside : 1;

		uint32 bShuffle : 1;
		uint32 bCanExitFrontDoors : 1;
		uint32 bCanExitRearDoors : 1;
		uint32 bPlayerCanBeJacked : 1;
		uint32 bCloseFrontDoorWhenOutside : 1;
		uint32 bCloseRearDoorWhenOutside : 1;
		uint32 bCanJumpOutOfVehicle : 1;
		uint32 bRollDownWindowOnDoorClose : 1;

		uint32 bPlayerCanFallOut : 1;
		uint32 bDoorLocked : 1;
	};

	sVehicleAnimDataGroupFlags(void) { intValue = 0; }
};

// The odd behaviours a group can ask for. Unlike the flags above these are not
// about choosing a group; they change what the driving code does.
union sVehicleAnimDataSpecialFlags
{
	uint32 intValue;
	struct {
		uint32 bDontCloseDoorAfterGettingOut : 1;
		uint32 bDontCloseDoorAfterGettingIn : 1;
		uint32 bUseKartDriveAnims : 1;
		uint32 bUseTruckDriveAnims : 1;
		uint32 bUseHovercraftDriveAnims : 1;
		uint32 bRunSpecialLockedDoor : 1;
		uint32 bDontOpenDoorWhenGettingIn : 1;
	};

	sVehicleAnimDataSpecialFlags(void) { intValue = 0; }
};

// The five blend times, in seconds, that CAutomobile/CBike use to decide when
// an in-car animation has gone far enough to hand over to the next.
struct sVehAnimGroupGeneralTiming
{
	float m_fGetInTime;
	float m_fJumpOutTime;
	float m_fGetOutTime;
	float m_fCarJackTime;
	float m_fFallOutTime;

	sVehAnimGroupGeneralTiming(void)
	 : m_fGetInTime(0.0f), m_fJumpOutTime(0.0f), m_fGetOutTime(0.0f),
	   m_fCarJackTime(0.0f), m_fFallOutTime(0.0f)
	{
	}
};

enum eInOutTimingMode : uint8 {
	OPEN_START = 0,
	OPEN_STOP = 1,
	CLOST_START = 2,	// SA's own spelling of CLOSE_START
	CLOSE_STOP = 3,
};

// Where along the door animation the door starts to open and finishes opening,
// and the same for closing, as a fraction of the clip.
struct sVehAnimGroupInOutTiming
{
	float OpenOut;
	float CloseIn;
	float OpenIn;
	float CloseOut;

	sVehAnimGroupInOutTiming(void)
	 : OpenOut(0.0f), CloseIn(0.0f), OpenIn(0.0f), CloseOut(0.0f)
	{
	}

	float &operator[](eInOutTimingMode mode) { return (&OpenOut)[(int)mode]; }
	float operator[](eInOutTimingMode mode) const { return (&OpenOut)[(int)mode]; }
};

enum eInOutTiming : uint8 {
	TIMING_START = 0,
	TIMING_END = 1,
};

// The seven door-relative offsets SA computes from the animation's last key
// frame: where the ped's hand has to be for the door to look grabbed.
enum eVehAnimDoorOffset : uint8 {
	ENTER_FRONT = 0,
	ENTER_REAR = 1,
	ENTER_BIKE_FRONT = 2,
	EXIT_FRONT = 3,
	EXIT_REAR = 4,
	JACK_PED_LEFT = 5,
	JACK_PED_RIGHT = 6,
};

// SA CVehicleAnimGroup (0x94 bytes). One of these per `^` line.
class CVehicleAnimGroup
{
public:
	uint8 m_ucFirstGroup;	// see AssocGroupId
	uint8 m_ucSecondGroup;
	char _pad[2];

	AssocGroupId GetFirstGroup(void) const { return (AssocGroupId)m_ucFirstGroup; }
	AssocGroupId GetSecondGroup(void) const { return (AssocGroupId)m_ucSecondGroup; }

	sVehicleAnimDataGroupFlags m_animFlags;
	sVehicleAnimDataSpecialFlags m_specialFlags;
	sVehAnimGroupGeneralTiming m_generalTiming;
	sVehAnimGroupInOutTiming m_aInOutTiming[2];	// indexed by eInOutTiming
	CVector m_aVecDoorOffsets[7];				// indexed by eVehAnimDoorOffset

	CVehicleAnimGroup(void);

	void InitAnimGroup(uint8 firstGroup, uint8 secondGroup, int32 animFlags, int32 animSpecialFlags,
		sVehAnimGroupGeneralTiming *generalTiming,
		sVehAnimGroupInOutTiming *startTiming, sVehAnimGroupInOutTiming *endTiming);
	void CopyAnimGroup(CVehicleAnimGroup *src);
	AssocGroupId GetGroup(AnimationId animId) const;
	float ComputeCriticalBlendTime(AnimationId animId);
	CVector ComputeAnimDoorOffsets(eVehAnimDoorOffset doorId);

	// Not a SA function: parses one `^` line of handling.cfg into this group.
	int32 InitFromData(const char *line);

	sVehAnimGroupInOutTiming &GetInOutTiming(eInOutTiming timing) { return m_aInOutTiming[timing]; }
	const sVehAnimGroupInOutTiming &GetInOutTiming(eInOutTiming timing) const { return m_aInOutTiming[timing]; }
	CVector &GetDoorOffset(eVehAnimDoorOffset door) { return m_aVecDoorOffsets[door]; }
};

class CVehicleAnimGroupData
{
public:
	static const int32 NUM_VEH_ANIM_GROUPS = 30;

	// The 30 groups, in the order handling.cfg's `^` lines fill them.
	static CVehicleAnimGroup m_vehicleAnimGroups[NUM_VEH_ANIM_GROUPS];

	static void GetInOutTimings(AssocGroupId groupId, eInOutTimingMode mode, float *pfAnimStart, float *pfAnimEnd);
	static AssocGroupId GetGroupForAnim(AssocGroupId groupId, AnimationId animId);
	static CVector GetAnimDoorOffset(AssocGroupId groupId, eVehAnimDoorOffset doorId);
	static float ComputeCriticalBlendTime(AssocGroupId groupId, AnimationId animId);

	static bool UsesTruckDrivingAnims(AssocGroupId groupId);
	static bool UsesKartDrivingAnims(AssocGroupId groupId);
	static bool UsesHovercraftDrivingAnims(AssocGroupId groupId);

	// Loads one `^` line from handling.cfg. Returns -1 on success, otherwise the
	// number of tokens sscanf managed to read.
	static int32 LoadAGroupFromData(const char *line);

	// Fills the table with SA's stock `^` lines. Called before the handling
	// file is parsed so its lines override the defaults.
	static void InitialiseDefaults(void);

	// SA indexes the table without a check because every caller passes an id it
	// has already validated. This fork reads the id out of the handling record,
	// which a hand-edited handling.cfg can put out of range, so the clamp lives
	// here instead of at every call site.
	static CVehicleAnimGroup &GetVehicleAnimGroup(int32 iGroup)
	{
		if(iGroup < 0 || iGroup >= NUM_VEH_ANIM_GROUPS)
			iGroup = 0;
		return m_vehicleAnimGroups[iGroup];
	}
};

#endif // __GTA_VEHICLEANIMGROUPDATA_H__
