#ifndef __GTA_BOAT_H__
#define __GTA_BOAT_H__

#include "Vehicle.h"
#include "Door.h"

// SA eBoatNodes (Boat.h), verbatim. SA adds the two propeller pairs.
enum eBoatNodes
{
	BOAT_NODE_NONE      = 0,
	BOAT_MOVING         = 1,
	BOAT_WINDSCREEN     = 2,
	BOAT_RUDDER         = 3,
	BOAT_FLAP_LEFT      = 4,
	BOAT_FLAP_RIGHT     = 5,
	BOAT_REARFLAP_LEFT  = 6,
	BOAT_REARFLAP_RIGHT = 7,
	BOAT_STATIC_PROP    = 8,
	BOAT_MOVING_PROP    = 9,
	BOAT_STATIC_PROP_2  = 10,
	BOAT_MOVING_PROP_2  = 11,

	NUM_BOAT_NODES
};

class CBoat : public CVehicle
{
public:
	float m_fMovingRotation;
	float m_fMovingSpeed;
	int32 m_boat_unused1;
	RwFrame *m_aBoatNodes[NUM_BOAT_NODES];
	CDoor m_boom;
	// SA CBoat::m_OldMoveSpeed / m_OldTurnSpeed: the chassis motion from before
	// this frame's step. CDoor::Process needs the difference to work out the
	// acceleration at the hinge, which is what actually swings the flap.
	CVector m_OldMoveSpeed;
	CVector m_OldTurnSpeed;
	// SA CBoat::m_EngineSpeed is the propeller speed in radians per second and
	// m_PropellerAngle the accumulated rotation PreRender feeds the propeller
	// nodes. Both live on the vehicle, not in a static: every boat in the world
	// has its own.
	float m_EngineSpeed;
	float m_PropellerAngle;
	// Smoothed up/down deflection of the rear flaps, from the pad's vertical
	// steering. A file static in the reference, i.e. shared by every boat;
	// going per instance keeps one boat's flaps from being driven by another's
	// input.
	float m_fRearFlapSteering;
	void ProcessBoatNodeRendering(eBoatNodes node, float rotation, uint8 alpha);
	tBoatHandlingData *m_pBoatHandlingData;
	uint8 bBoatInWater : 1;
	uint8 bPropellerInWater : 1;
	bool m_bIsAnchored;
	float m_fOrientation;
	uint32 m_nPoliceShoutTimer;
	int32 m_boat_unused2;
	float m_fDamage;
	CEntity *m_pSetOnFireEntity;
	float m_skimmerThingTimer;
	bool m_boat_unused3;
	uint8 m_nPadID;
	int32 m_boat_unused4;
	float m_fVolumeUnderWater;
	CVector m_vecBuoyancePoint;
	float m_fPrevVolumeUnderWater;
	int16 m_nDeltaVolumeUnderWater;
	uint16 m_NumWakeCoords;
	CVector2D m_WakeCoords[32];
	uint8 m_WakeBoatSpeed[32];
	float m_WakePtCounters[32];

	static const float MAX_WAKE_LENGTH;
	static const float MIN_WAKE_INTERVAL;
	static const float WAKE_LIFETIME;

	CBoat(int, uint8);

	virtual void SetModelIndex(uint32 id);
	virtual void ProcessControl();
	virtual void Teleport(CVector v);
	virtual void PreRender(void);
	virtual void Render(void);
	virtual void ProcessControlInputs(uint8);
	virtual void GetComponentWorldPosition(int32 component, CVector &pos);
	virtual bool IsComponentPresent(int32 component) { return true; }
	virtual void BlowUpCar(CEntity *ent);

	void RenderWaterOutPolys(void);
	void ApplyWaterResistance(void);
	void SetupModelNodes();
	void PruneWakeTrail(void);
	void AddWakePoint(CVector point);
	void RenderWakePoints(void);
	static void RenderAllWakePointBoats(void);
	void DoDriveByShootings(void);

	static CBoat *apFrameWakeGeneratingBoats[4];
	
	static bool IsSectorAffectedByWake(CVector2D sector, float fSize, CBoat **apBoats);
	static float IsVertexAffectedByWake(CVector vecVertex, CBoat *pBoat);
	static void FillBoatList(void);


	// ---- State API (see the block in Vehicle.h) -------------------------
	//
	// A boat has no wheels, doors or wings; its damage state is the hull value
	// and the engine. The buoyancy and propeller state below is what a peer
	// needs to show a boat that is bobbing without simulating the hull.
	bool IsInWater(void) const { return bBoatInWater; }
	void SetInWater(bool in) { bBoatInWater = in; }
	float GetEngineSpeed(void) const { return m_EngineSpeed; }
	void SetEngineSpeed(float speed) { m_EngineSpeed = speed; }
	float GetPropellerAngle(void) const { return m_PropellerAngle; }
	void SetPropellerAngle(float angle) { m_PropellerAngle = angle; }
	// How far the hull is under the surface, and where the buoyant force acts.
	float GetVolumeUnderWater(void) const { return m_fVolumeUnderWater; }
	void SetVolumeUnderWater(float v) { m_fVolumeUnderWater = v; }
	CVector GetBuoyancyPoint(void) const { return m_vecBuoyancePoint; }
	void SetBuoyancyPoint(const CVector &p) { m_vecBuoyancePoint = p; }
	// Hull damage, separate from the vehicle's m_fHealth: it is what the boat
	// takes from hitting the water hard, and it is what makes it sink.
	float GetHullDamage(void) const { return m_fDamage; }
	void SetHullDamage(float damage) { m_fDamage = damage; }
	bool IsAnchored(void) const { return m_bIsAnchored; }
	void SetAnchored(bool anchored) { m_bIsAnchored = anchored; }

};

#endif // __GTA_BOAT_H__
