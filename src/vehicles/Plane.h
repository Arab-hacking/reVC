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
	void Render(void);
	void FlagToDestroyWhenNextProcessed() { bRemoveFromWorld = true; }

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
