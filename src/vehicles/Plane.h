#ifndef __GTA_PLANE_H__
#define __GTA_PLANE_H__

#include "Vehicle.h"

// SA ePlaneNodes (Plane.h), verbatim. The VC enum reused the heli rotor names
// here; SA uses the same frame set as a car plus propeller/control-surface
// nodes, so VC's PLANE_TOPROTOR/BACKROTOR have no SA counterpart and are gone.
enum ePlaneNodes
{
	PLANE_NODE_NONE   = 0,
	PLANE_CHASSIS     = 1,
	PLANE_WHEEL_RF    = 2,
	PLANE_WHEEL_RM    = 3,
	PLANE_WHEEL_RB    = 4,
	PLANE_WHEEL_LF    = 5,
	PLANE_WHEEL_LM    = 6,
	PLANE_WHEEL_LB    = 7,
	PLANE_DOOR_RF     = 8,
	PLANE_DOOR_RR     = 9,
	PLANE_DOOR_LF     = 10,
	PLANE_DOOR_LR     = 11,
	PLANE_STATIC_PROP = 12,
	PLANE_MOVING_PROP = 13,
	PLANE_STATIC_PROP2 = 14,
	PLANE_MOVING_PROP2 = 15,
	PLANE_RUDDER      = 16,
	PLANE_ELEVATOR_L  = 17,
	PLANE_ELEVATOR_R  = 18,
	PLANE_AILERON_L   = 19,
	PLANE_AILERON_R   = 20,
	PLANE_GEAR_L      = 21,
	PLANE_GEAR_R      = 22,
	PLANE_MISC_A      = 23,
	PLANE_MISC_B      = 24,
	PLANE_MISC_C      = 25,

	NUM_PLANE_NODES
};

struct CPlaneNode
{
	CVector p;	// position
	float t;	// xy-distance from start on path
	bool bOnGround;	// i.e. not flying
};

struct CPlaneInterpolationLine
{
	uint8 type;
	float time;	// when does this keyframe start
	// initial values at start of frame
	float position;
	float speed;
	float acceleration;
};

class CPlane : public CVehicle
{
public:
	RwFrame *m_aPlaneNodes[NUM_PLANE_NODES];
	float m_fRotorRotation;
	// SA's propeller speed constants (0x8D33CC..0x8D33D8). Boats use the same
	// pair for their propellers; the rocket-propelled case is what makes a
	// boat's propeller spin fast when gas is applied.
	static constexpr float PLANE_MIN_PROP_SPEED = 1.0f;
	static constexpr float PLANE_STD_PROP_SPEED = 2.0f;
	static constexpr float PLANE_MAX_PROP_SPEED = 32.0f;
	static constexpr float PLANE_ROC_PROP_SPEED = 1.5f;

	// SA CPlane carries the animated control state on the vehicle. The
	// reference's own names are used where they are known
	// (m_fLandingGearStatus is the gear extension, 0 retracted .. 1 down).
	float m_fLandingGearStatus;
	float m_fAileronAngle;
	float m_fElevatorAngle;
	float m_fRudderAngle;
	// Gear is retracted above this height above ground; below it the gear comes
	// down. SA uses 8 units, which is the point where the wheels would start to
	// matter for a landing.
	static constexpr float PLANE_GEAR_UPDOWN_DIST = 8.0f;

	int16 m_nPlaneId;
	int16 m_isFarAway;
	int16 m_nCurPathNode;
	float m_fSpeed;
	uint32 m_nFrameWhenHit;
	bool m_bHasBeenHit;
	bool m_bIsDrugRunCesna;
	bool m_bIsDropOffCesna;
	bool m_bTempPlane;

	CPlane(int32 id, uint8 CreatedBy);
	~CPlane(void);

	// from CEntity
	void SetModelIndex(uint32 id);
	void DeleteRwObject(void);
	void ProcessControl(void);
	void PreRender(void);
	void ProcessControlSurfaces(void);
	void Render(void);
	void FlagToDestroyWhenNextProcessed() { bRemoveFromWorld = true; }

	// ---- State API (see the block in Vehicle.h) -------------------------
	//
	// The control surfaces and the gear are the plane's visible state: a peer
	// needs them to show a plane banking or lowering its gear without running
	// the flight model. The propeller follows from the engine speed.
	float GetPropellerAngle(void) const { return m_fRotorRotation; }
	void SetPropellerAngle(float angle) { m_fRotorRotation = angle; }
	float GetLandingGear(void) const { return m_fLandingGearStatus; }
	void SetLandingGear(float gear) { m_fLandingGearStatus = Clamp(gear, 0.0f, 1.0f); }
	float GetAileronAngle(void) const { return m_fAileronAngle; }
	void SetAileronAngle(float angle) { m_fAileronAngle = angle; }
	float GetElevatorAngle(void) const { return m_fElevatorAngle; }
	void SetElevatorAngle(float angle) { m_fElevatorAngle = angle; }
	float GetRudderAngle(void) const { return m_fRudderAngle; }
	void SetRudderAngle(float angle) { m_fRudderAngle = angle; }

	static void InitPlanes(void);
	static void Shutdown(void);
	static CPlaneNode *LoadPath(char const *filename, int32 &numNodes, float &totalLength, bool loop);
	static void RemoveTemporaryPlanes(void);
	static void UpdatePlanes(void);
	static bool TestRocketCollision(CVector *rocketPos);
	static void CreateIncomingCesna(void);
	static void CreateDropOffCesna(void);
	static const CVector FindDrugPlaneCoordinates(void);
	static const CVector FindDropOffCesnaCoordinates(void);
	static bool HasCesnaLanded(void);
	static bool HasCesnaBeenDestroyed(void);
	static bool HasDropOffCesnaBeenShotDown(void);
	static void Load(void);
	static void Save(void);
};

extern float LandingPoint;
extern float TakeOffPoint;
extern float PlanePathPosition[3];

#endif // __GTA_PLANE_H__
