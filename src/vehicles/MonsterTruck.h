#ifndef __GTA_MONSTERTRUCK_H__
#define __GTA_MONSTERTRUCK_H__

#include "Automobile.h"

// SA eMonsterTruckNodes (MonsterTruck.h), verbatim. A monster truck is a car
// whose wheels are so big that the whole suspension had to be re-authored: the
// front and rear axles are visible as separate nodes, the wheels sit on their
// own stubs, and the dump bed is its own moving part.
enum eMonsterTruckNodes {
	MONSTER_NODE_NONE      = 0,
	MONSTER_CHASSIS        = 1,
	MONSTER_WHEEL_RF       = 2,
	MONSTER_WHEEL_RM       = 3,
	MONSTER_WHEEL_RB       = 4,
	MONSTER_WHEEL_LF       = 5,
	MONSTER_WHEEL_LM       = 6,
	MONSTER_WHEEL_LB       = 7,
	MONSTER_DOOR_RF        = 8,
	MONSTER_DOOR_RR        = 9,
	MONSTER_DOOR_LF        = 10,
	MONSTER_DOOR_LR        = 11,
	MONSTER_BUMP_FRONT     = 12,
	MONSTER_BUMP_REAR      = 13,
	MONSTER_WING_RF        = 14,
	MONSTER_WING_LF        = 15,
	MONSTER_BONNET         = 16,
	MONSTER_BOOT           = 17,
	MONSTER_WINDSCREEN     = 18,
	MONSTER_TRANSMISSION_F = 19,
	MONSTER_TRANSMISSION_R = 20,
	MONSTER_LOADBAY        = 21,
	MONSTER_MISC_A         = 22,

	MONSTER_NUM_NODES
};

// SA CMonsterTruck (0x99C bytes).
class CMonsterTruck : public CAutomobile
{
public:
	float m_afWheelRatios[4];	// unused by the game, kept for layout parity
	float m_fSuspensionRadius;
	bool m_bIsBigDumper;

	CMonsterTruck(int32 id, uint8 CreatedBy);
	~CMonsterTruck(void);

	// from CPhysical
	int32 ProcessEntityCollision(CEntity *ent, CColPoint *colpoint);

	// from CAutomobile
	void ProcessSuspension(void);
	void ProcessControl(void);
	void SetupSuspensionLines(void);
	void PreRender(void);
	void BurstTyre(uint8 tyre, bool bPhysicalEffect);
	bool SetUpWheelColModel(CColModel *colModel);
	void ResetSuspension(void);
	void ExtendSuspension(void);
};

extern float fWheelExtensionRate;	// 0.1

#endif // __GTA_MONSTERTRUCK_H__
