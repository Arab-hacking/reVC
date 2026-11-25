#ifndef __GTA_DOOR_H__
#define __GTA_DOOR_H__

class CVehicle;

// SA eDoorAxis (Door.h). The door's hinge direction is one of six signed axes,
// not a bare 0/1/2 index: a left door swings about -X and its right-hand twin
// about -Y, and the sign is what makes them open away from the car instead of
// into it.
enum eDoorAxis : uint8 {
	DOOR_AXIS_X     = 0,
	DOOR_AXIS_Y     = 1,
	DOOR_AXIS_Z     = 2,

	DOOR_AXIS_NEG_X = 3,
	DOOR_AXIS_NEG_Y = 4,
	DOOR_AXIS_NEG_Z = 5,
};

// SA eDoorExtraFlags. m_dirn packs the axis in its low nibble and these bits
// above it; the old int8 could only carry one polarity bit and had nowhere to
// put the spring, chassis and firetruck cases.
enum eDoorExtraFlags : uint16 {
	DOOR_EXTRA_NONE        = 0x0,

	DOOR_EXTRA_DIRN_MASK   = 0x0F,

	DOOR_EXTRA_BASED       = 0x10,
	DOOR_EXTRA_LOW_GRAVITY = 0x20,
	DOOR_EXTRA_CHASSIS     = 0x40,
	DOOR_EXTRA_FIXEDSTATE  = 0x80,
	DOOR_EXTRA_FIRETRUCK   = 0x100,
};

// SA eDoorState. The VC enum only distinguished swinging/open/closed, which
// cannot express "slammed shut" - the state the damage code tests for.
enum eDoorState : uint8 {
	DOOR_NOTHING,
	DOOR_HIT_MAX_END,
	DOOR_HIT_MIN_END,
	DOOR_POP_OPEN,
	DOOR_SLAM_SHUT
};

// SA CDoor globals (Door.cpp, 0x872304..0x87233C).
extern float DOOR_SPEED_MIN_LIMIT;
extern float DOOR_DAMP_RATE;
extern float DOOR_DAMP_RATE_BOAT;
extern float DOOR_DAMP_RATE_FIRETRUCK;
extern float DOOR_DAMP_RATE_BONNET;
extern float DOOR_DAMP_RATE_CHASSIS;
extern float DOOR_DAMP_RATE_CHASSIS_CUTSCENE;
extern float DOOR_SPRING_RATE_CHASSIS;
extern float DOOR_APPLY_RATE_CHASSIS;
extern float DOOR_APPLY_RATE_CHASSIS_CUTSCENE;
extern float DOOR_SPEED_MAX_CAPPED_CHASSIS;
extern float DOOR_SPEED_MAX_CAPPED_FIRETRUCK;
extern float DOOR_SPRING_RATE_FIRETRUCK;
extern float DOOR_SLAM_SHUT_SPEED;

class CDoor
{
public:
	// SA field layout: the two limit angles are named for what they are, and
	// m_dirn is a bitfield carrying the axis plus the extra flags.
	float m_openAngle;
	float m_closedAngle;

	uint16     m_dirn;
	eDoorAxis  m_axis;
	eDoorState m_nDoorState;

	float m_fAngle;
	float m_fPrevAngle;
	float m_fAngVel;    // angular velocity

	static const float DOOR_SPEED_MAX_CAPPED;

	CDoor(void);
	// SA CDoor::Init(openAngle, closedAngle, dirn, axis, flags). The VC signature
	// took (min, max, dirn, axis); Init makes the mapping explicit so a caller
	// can no longer transpose the two limits by accident.
	void Init(float openAngle, float closedAngle, int8 dirn, int8 axis, uint16 flags = DOOR_EXTRA_BASED) {
		m_openAngle = openAngle;
		m_closedAngle = closedAngle;
		m_dirn = (uint16)(uint8)dirn | flags;
		m_axis = (eDoorAxis)axis;
	}
	void Open(float ratio);
	bool Process(CVehicle *veh, const CVector &oldMoveSpeed, const CVector &oldTurnSpeed, const CVector &offset);
	bool ProcessImpact(CVehicle *veh, const CVector &oldMoveSpeed, const CVector &oldTurnSpeed, const CVector &offset);
	float RetAngleWhenClosed(void);
	float RetAngleWhenOpen(void);
	float GetAngleOpenRatio(void);
	bool IsFullyOpen(void);
	bool IsClosed(void);

	// SA helpers.
	CVector GetRotation(void) const;
	void UpdateFrameMatrix(CMatrix &mat);

	// The Rhino stores the suspension lengths of its eight extra track wheels
	// in the angles of its two rear doors, hence these accessors.
	void SetExtraWheelPositions(float wheel1, float wheel2, float wheel3, float wheel4);
	void GetExtraWheelPositions(float &wheel1, float &wheel2, float &wheel3, float &wheel4);
};


#endif // __GTA_DOOR_H__
