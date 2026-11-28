#ifndef __GTA_CARGEN_H__
#define __GTA_CARGEN_H__
#include "common.h"
#include "config.h"
#include "SpecialPlateHandler.h"

// SA's car generator table size. The VC constant (185 slots) is gone from
// config.h together with the rest of the VC car generator layout.
enum {
	NUM_CAR_GENERATORS = 500
};

// SA's on-disk/IDE form of a car generator, as read from cargen IPL sections.
// Kept because the loader fills this and then calls Setup().
class CFileCarGenerator
{
public:
	CVector m_vecPosn;
	float m_fAngle;
	int32 m_nModelId;
	int32 m_nPrimaryColor;
	int32 m_nSecondaryColor;
	union {
		struct {
			uint32 m_bForceSpawn : 1;
			uint32 m_bIgnorePopulationLimit : 1;
		};
		uint32 m_nFlags;
	};
	int32 m_nAlarmChance;
	int32 m_nDoorLockChance;
	int32 m_nMinDelay;
	int32 m_nMaxDelay;
};

// SA CCarGenerator (0x20 bytes in the original). The layout changed a lot from
// VC: the position is compressed to 1/8-unit int16s, the angle is a single byte
// in 1/256 turn units, and the flags became a bitfield. The VC fields
// m_nColor1/m_nColor2 became int8, and the "uses remaining" counter became
// m_nGenerateCount where 0 means off and 0xFFFF means infinite.
class CCarGenerator
{
public:
	int16 m_nModelId;		// >= 0: fixed model, <= -2: random, -1: none
	int8 m_nPrimaryColor;
	int8 m_nSecondaryColor;
	int16 m_nPosX;			// 1/8 unit
	int16 m_nPosY;
	int16 m_nPosZ;
	char m_nAngle;			// 1/256 turn
	uint8 m_nAlarmChance;		// percentage 0-100
	uint8 m_nDoorLockChance;	// percentage 0-100

	union {
		struct {
			uint8 bWaitUntilFarFromPlayer : 1;
			uint8 bHighPriority : 1;
			uint8 bActive : 1;
			uint8 bPlayerHasAlreadyOwnedCar : 1;
			uint8 bIgnorePopulationLimit : 1;
		};
		uint8 m_nFlags;
	};
	uint16 m_nMinDelay;
	uint16 m_nMaxDelay;
	uint32 m_nNextGenTime;
	int16 m_nVehicleHandle;
	// 0: never spawn, >0: spawn this many times, 0xFFFF: always spawn.
	uint16 m_nGenerateCount;
	uint8 m_nIplId;
	bool m_bIsUsed;

	// Helper accessors for the compressed position. SA stores the position as
	// int16 at 1/8 unit precision rather than as three floats.
	void SetPosn(CVector const &posn);
	CVector GetPosn(void) const;

	bool CheckForBlockage(int32 modelId);
	bool CheckIfWithinRangeOfAnyPlayers();
	void DoInternalProcessing();
	void Process();
	void Setup(CVector posn, float angle, int32 modelId, int16 color1, int16 color2, uint8 forceSpawn,
		uint8 alarmChance, uint8 doorLockChance, uint16 minDelay, uint16 maxDelay,
		uint8 iplId, bool ignorePopulationLimit);
	void SwitchOff();
	void SwitchOn();

	static uint32 CalcNextGen();
};

class CTheCarGenerators
{
public:
	static uint8 ProcessCounter;
	static uint32 NumOfCarGenerators;
	static CCarGenerator CarGeneratorArray[NUM_CAR_GENERATORS];
	static uint8 GenerateEvenIfPlayerIsCloseCounter;
	static CSpecialPlateHandler m_SpecialPlateHandler;

	static void Process();
	static int32 CreateCarGenerator(CVector posn, float angle, int32 modelId, int16 color1, int16 color2, uint8 forceSpawn,
		uint8 alarmChance, uint8 doorLockChance, uint16 minDelay, uint16 maxDelay,
		uint8 iplId, bool ignorePopulationLimit);
	static void Init();
	static void RemoveCarGenerators(uint8 iplId);

	static CCarGenerator *Get(uint16 index);
	static int32 GetIndex(CCarGenerator *pCarGen);
};

#endif // __GTA_CARGEN_H__
