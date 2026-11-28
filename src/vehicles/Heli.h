#ifndef __GTA_HELI_H__
#define __GTA_HELI_H__

#include "Vehicle.h"

class CObject;

// SA eHeliNodes (Heli.h), verbatim. SA helis carry the full wheel/door set
// plus two rotor pairs; VC only had a chassis, one rotor pair, tail and skids.
enum eHeliNodes
{
	HELI_NODE_NONE     = 0,
	HELI_CHASSIS       = 1,
	HELI_WHEEL_RF      = 2,
	HELI_WHEEL_RM      = 3,
	HELI_WHEEL_RB      = 4,
	HELI_WHEEL_LF      = 5,
	HELI_WHEEL_LM      = 6,
	HELI_WHEEL_LB      = 7,
	HELI_DOOR_RF       = 8,
	HELI_DOOR_RR       = 9,
	HELI_DOOR_LF       = 10,
	HELI_DOOR_LR       = 11,
	HELI_STATIC_ROTOR  = 12,
	HELI_MOVING_ROTOR  = 13,
	HELI_STATIC_ROTOR2 = 14,
	HELI_MOVING_ROTOR2 = 15,
	HELI_RUDDER        = 16,
	HELI_ELEVATORS     = 17,
	HELI_MISC_A        = 18,
	HELI_MISC_B        = 19,
	HELI_MISC_C        = 20,
	HELI_MISC_D        = 21,

	NUM_HELI_NODES
};

enum
{
	HELI_RANDOM0,
	HELI_RANDOM1,
	HELI_SCRIPT,
	HELI_CATALINA,	// TODO 2 in VC
	NUM_HELIS
};

enum
{
	HELI_TYPE_RANDOM,
	HELI_TYPE_SCRIPT,
	HELI_TYPE_CATALINA,
};


class CHeli : public CVehicle
{
public:
	RwFrame *m_aHeliNodes[NUM_HELI_NODES];
	int8 m_heliStatus;
	float m_fSearchLightX;
	float m_fSearchLightY;
	uint32 m_nExplosionTimer;
	float m_fRotation;
	float m_fAngularSpeed;
	float m_fTargetZ;
	float m_fSearchLightIntensity;
	int8 m_nHeliId;
	int8 m_heliType;
	int8 m_pathState;
	// Which waypoint of the current path m_pathState selects. This was a file
	// static in Heli.cpp, i.e. one cursor shared by every helicopter in the
	// world - two scripted helis would walk each other's paths.
	int8 m_pathPoint;
	int8 m_numSwat;
	uint8 m_aSwatState[4];
	float m_aSearchLightHistoryX[6];
	float m_aSearchLightHistoryY[6];
	uint32 m_nSearchLightTimer;
	uint32 m_nShootTimer;
	uint32 m_nLastShotTime;
	uint32 m_nBulletDamage;
	// SA keeps the rotor speed in CAutomobile's m_fHeliRotorSpeed union member.
	// CHeli here derives from CVehicle, not CAutomobile, so the value lives on
	// this class instead: m_fRotorSpeed is radians per second, m_fRotorRotation
	// the accumulated angle the moving rotor is drawn at.
	float m_fRotorRotation;
	float m_fRotorSpeed;
	float m_fHeliDustZ[8];
	uint32 m_nPoliceShoutTimer;
	float m_fTargetOffset;
	bool m_bTestRight;

	static CHeli *pHelis[NUM_HELIS];
	static int16 NumRandomHelis;
	static uint32 TestForNewRandomHelisTimer;
	static bool CatalinaHeliOn;
	static bool CatalinaHasBeenShotDown;
	static bool ScriptHeliOn;

	CHeli(int32 id, uint8 CreatedBy);

	// from CEntity
	void SetModelIndex(uint32 id);
	void ProcessControl(void);
	void PreRender(void);
	void Render(void);

	void PreRenderAlways(void);
	CObject *SpawnFlyingComponent(int32 component);
	CVector FindSwatPositionRelativeToHeli(int n);
	bool SendDownSwat(void);

	// ---- State API (see the block in Vehicle.h) -------------------------
	//
	// The rotor is the helicopter's whole visible state: its angle and speed are
	// what a peer needs to show a helicopter hovering without running its
	// physics. The searchlight is the other moving part and is aimed by a
	// direction, not an angle, so both components are exposed.
	float GetRotorAngle(void) const { return m_fRotorRotation; }
	void SetRotorAngle(float angle) { m_fRotorRotation = angle; }
	float GetRotorSpeed(void) const { return m_fRotorSpeed; }
	void SetRotorSpeed(float speed) { m_fRotorSpeed = speed; }
	float GetSearchLightIntensity(void) const { return m_fSearchLightIntensity; }
	void SetSearchLightIntensity(float intensity) { m_fSearchLightIntensity = intensity; }
	float GetSearchLightDirX(void) const { return m_fSearchLightX; }
	float GetSearchLightDirY(void) const { return m_fSearchLightY; }
	void SetSearchLightDir(float x, float y) { m_fSearchLightX = x; m_fSearchLightY = y; }
	// Which of the game's helicopter slots this one occupies, or -1 for a heli
	// that is not one of them. A scripted heli has to keep its slot on every
	// client, so the slot travels with the state.
	int8 GetHeliId(void) const { return m_nHeliId; }
	void SetHeliId(int8 id) { m_nHeliId = id; }

	static void InitHelis(void);
	static CHeli *GenerateHeli(bool catalina); // out of class in III PC and later because of SecuROM
	static void UpdateHelis(void);
	static void SpecialHeliPreRender(void);
	static bool TestRocketCollision(CVector *coors);
	static bool TestBulletCollision(CVector *line0, CVector *line1, CVector *bulletPos, int32 damage);
	static bool TestSniperCollision(CVector *line0, CVector *line1);

	static void StartCatalinaFlyBy(void); // out of class in III PC and later because of SecuROM
	static void RemoveCatalinaHeli(void);
	static CHeli *FindPointerToCatalinasHeli(void);
	static void CatalinaTakeOff(void);
	static void MakeCatalinaHeliFlyAway(void);
	static bool HasCatalinaBeenShotDown(void);

	static void ActivateHeli(bool activate);
};

#endif // __GTA_HELI_H__
