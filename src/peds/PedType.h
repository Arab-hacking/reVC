#ifndef __GTA_PEDTYPE_H__
#define __GTA_PEDTYPE_H__

#include "eBoneTag.h"
#include "PedNode.h"
#include "PedNode.h"

// Index into the PedType array. The San Andreas set differs from the Vice City
// one from the third slot on: the two extra players become the network player
// and an unused slot, and the tail carries dealers, medics, bums and the eight
// mission types. This is the numbering `ped.dat` uses, and the bit of value N
// here is what `CPedType::GetPedFlag` returns.
enum ePedType
{
	PEDTYPE_PLAYER1 = 0,
	PEDTYPE_PLAYER2,
	PEDTYPE_PLAYER_NETWORK,
	PEDTYPE_PLAYER_UNUSED,
	PEDTYPE_CIVMALE,
	PEDTYPE_CIVFEMALE,

	PEDTYPE_COP,

	PEDTYPE_GANG1,		// Ballas
	PEDTYPE_GANG2,		// Grove Street Families
	PEDTYPE_GANG3,		// Los Santos Vagos
	PEDTYPE_GANG4,		// San Fierro Rifa
	PEDTYPE_GANG5,		// Da Nang Boys
	PEDTYPE_GANG6,		// Mafia
	PEDTYPE_GANG7,		// Mountain Cloud Triad
	PEDTYPE_GANG8,		// Varrio Los Aztecas
	PEDTYPE_GANG9,		// Russian Mafia
	PEDTYPE_GANG10,		// Bikers

	PEDTYPE_DEALER,
	PEDTYPE_MEDIC,
	PEDTYPE_FIREMAN,
	PEDTYPE_CRIMINAL,
	PEDTYPE_BUM,
	PEDTYPE_PROSTITUTE,
	PEDTYPE_SPECIAL,
	PEDTYPE_MISSION1,
	PEDTYPE_MISSION2,
	PEDTYPE_MISSION3,
	PEDTYPE_MISSION4,
	PEDTYPE_MISSION5,
	PEDTYPE_MISSION6,
	PEDTYPE_MISSION7,
	PEDTYPE_MISSION8,

	NUM_PEDTYPES		// 32
};

// A ped type flag is the bit of its index, and the threat and avoid masks are
// 32 bits wide, which is exactly how many types San Andreas defines.
static_assert(NUM_PEDTYPES == 32, "ped type flags are 32 bits wide");

enum
{
	PED_FLAG_PLAYER1 = 1 << 0,
	PED_FLAG_PLAYER2 = 1 << 1,
	PED_FLAG_PLAYER_NETWORK = 1 << 2,
	PED_FLAG_PLAYER_UNUSED = 1 << 3,
	PED_FLAG_CIVMALE = 1 << 4,
	PED_FLAG_CIVFEMALE = 1 << 5,
	PED_FLAG_COP = 1 << 6,
	PED_FLAG_GANG1 = 1 << 7,
	PED_FLAG_GANG2 = 1 << 8,
	PED_FLAG_GANG3 = 1 << 9,
	PED_FLAG_GANG4 = 1 << 10,
	PED_FLAG_GANG5 = 1 << 11,
	PED_FLAG_GANG6 = 1 << 12,
	PED_FLAG_GANG7 = 1 << 13,
	PED_FLAG_GANG8 = 1 << 14,
	PED_FLAG_GANG9 = 1 << 15,
	PED_FLAG_GANG10 = 1 << 16,
	PED_FLAG_DEALER = 1 << 17,
	PED_FLAG_MEDIC = 1 << 18,
	PED_FLAG_FIREMAN = 1 << 19,
	PED_FLAG_CRIMINAL = 1 << 20,
	PED_FLAG_BUM = 1 << 21,
	PED_FLAG_PROSTITUTE = 1 << 22,
	PED_FLAG_SPECIAL = 1 << 23,
	PED_FLAG_MISSION1 = 1 << 24,
	PED_FLAG_MISSION2 = 1 << 25,
	PED_FLAG_MISSION3 = 1 << 26,
	PED_FLAG_MISSION4 = 1 << 27,
	PED_FLAG_MISSION5 = 1 << 28,
	PED_FLAG_MISSION6 = 1 << 29,
	PED_FLAG_MISSION7 = 1 << 30,
	PED_FLAG_MISSION8 = (int32)1 << 31,
};

// What a ped is afraid of. These are not ped type flags: they are the names
// `ped.dat` uses on a Threat line, and the bit of each is the value a threat
// scan returns.
enum ePedThreatFlags
{
	PED_THREAT_GUN = 1 << 0,
	PED_THREAT_COP_CAR = 1 << 1,
	PED_THREAT_FAST_CAR = 1 << 2,
	PED_THREAT_EXPLOSION = 1 << 3,
	PED_THREAT_DEADPEDS = 1 << 4,
};

// The race a ped is spawned as, derived from the first letter of its model.
enum ePedRace
{
	RACE_DEFAULT = 0,
	RACE_BLACK = 1,		// 'B'
	RACE_WHITE = 2,		// 'W'
	RACE_ORIENTAL = 3,	// 'O', 'I'
	RACE_HISPANIC = 4,	// 'H'
};

class CPedType
{
	uint32 m_flag;
	float unknown1;
	float unknown2;
	float unknown3;
	float unknown4;
	float unknown5;
	uint32 m_threats;
	uint32 m_avoid;

	static CPedType *ms_apPedType[NUM_PEDTYPES];
public:

	static void Initialise(void);
	static void Shutdown(void);
	static void LoadPedData(void);
	static ePedType FindPedType(const char *type);
	static uint32 FindPedFlag(const char *type);
	// The flag of a type is the bit of its index; used by the relation lists.
	static uint32 GetPedFlag(ePedType type);

	static uint32 GetFlag(int type) { return ms_apPedType[type]->m_flag; }
	static uint32 GetAvoid(int type) { return ms_apPedType[type]->m_avoid; }
	static uint32 GetThreats(int type) { return ms_apPedType[type]->m_threats; }
	static void SetThreats(int type, uint32 threat) { ms_apPedType[type]->m_threats = threat; }
	static void AddThreat(int type, int threat) { ms_apPedType[type]->m_threats |= threat; }
	static void RemoveThreat(int type, int threat) { ms_apPedType[type]->m_threats &= ~threat; }
	static bool IsThreat(int type, int threat) { return ms_apPedType[type]->m_threats & threat; }
};

VALIDATE_SIZE(CPedType, 0x20);

// San Andreas stats, in the order `pedstats.dat` lists them. The set is the
// Vice City one with the three extra gang types inserted after GANG7, which
// pushes every entry from GANG8 on down by three.
enum ePedStats
{
	// Never the value of a real row: a model whose stat name is unknown gets
	// this, so a reader can tell "no personality" from PEDSTAT_PLAYER.
	PEDSTAT_NONE = -1,
	PEDSTAT_PLAYER,
	PEDSTAT_COP,
	PEDSTAT_MEDIC,
	PEDSTAT_FIREMAN,
	PEDSTAT_GANG1,
	PEDSTAT_GANG2,
	PEDSTAT_GANG3,
	PEDSTAT_GANG4,
	PEDSTAT_GANG5,
	PEDSTAT_GANG6,
	PEDSTAT_GANG7,
	PEDSTAT_GANG8,
	PEDSTAT_GANG9,
	PEDSTAT_GANG10,
	PEDSTAT_STREET_GUY,
	PEDSTAT_SUIT_GUY,
	PEDSTAT_SENSIBLE_GUY,
	PEDSTAT_GEEK_GUY,
	PEDSTAT_OLD_GUY,
	PEDSTAT_TOUGH_GUY,
	PEDSTAT_STREET_GIRL,
	PEDSTAT_SUIT_GIRL,
	PEDSTAT_SENSIBLE_GIRL,
	PEDSTAT_GEEK_GIRL,
	PEDSTAT_OLD_GIRL,
	PEDSTAT_TOUGH_GIRL,
	PEDSTAT_TRAMP_MALE,
	PEDSTAT_TRAMP_FEMALE,
	PEDSTAT_TOURIST,
	PEDSTAT_PROSTITUTE,
	PEDSTAT_CRIMINAL,
	PEDSTAT_BUSKER,
	PEDSTAT_TAXIDRIVER,
	PEDSTAT_PSYCHO,
	PEDSTAT_STEWARD,
	PEDSTAT_SPORTSFAN,
	PEDSTAT_SHOPPER,
	PEDSTAT_OLDSHOPPER,
	PEDSTAT_BEACH_GUY,
	PEDSTAT_BEACH_GIRL,
	PEDSTAT_SKATER,
	PEDSTAT_STD_MISSION,
	PEDSTAT_COWARD,

	NUM_PEDSTATS
};

// flags
enum
{
	STAT_PUNCH_ONLY = 1,
	STAT_CAN_KNEE_HEAD = 2,
	STAT_CAN_KICK = 4,
	STAT_CAN_ROUNDHOUSE = 8,
	STAT_NO_DIVE = 0x10,
	STAT_ONE_HIT_KNOCKDOWN = 0x20,
	STAT_SHOPPING_BAGS = 0x40,
	STAT_GUN_PANIC = 0x80
};

// A row of pedstats.dat. The layout and the field names are San Andreas':
// the file grew an eleventh column at the end (the default decision maker),
// and the id of the row is stored rather than implied, so a reader can tell
// which type the row landed in.
struct CPedStat
{
	uint32 m_nId;
	char m_acName[24];
	float m_fFleeDistance;
	float m_fHeadingChangeRate;
	uint8 m_nFear;
	uint8 m_nTemper;
	uint8 m_nLawfulness;
	uint8 m_nSexiness;
	float m_fAttackStrength;
	float m_fDefendWeakness;
	uint16 m_flags;
	int8 m_nDefaultDecisionMaker;
};

VALIDATE_SIZE(CPedStat, 0x34);

class CPedStats
{
public:
	static CPedStat *ms_apPedStats[NUM_PEDSTATS];

	static void Initialise(void);
	static void Shutdown(void);
	static void LoadPedStats(void);
	static ePedStats GetPedStatType(const char *name);
	// The row itself, for callers that need the stats and not an index. Both
	// return nil when the name is not in the table.
	static CPedStat *GetPedStatInfo(const char *name);
	static CPedStat *GetPedStatByArrayIndex(uint32 statIndex);
};

enum eMoveState
{
	PEDMOVE_NONE = 0,
	PEDMOVE_STILL,
	PEDMOVE_TURN_L,
	PEDMOVE_TURN_R,
	PEDMOVE_WALK,
	PEDMOVE_JOG,
	PEDMOVE_RUN,
	PEDMOVE_SPRINT
};

#endif // __GTA_PEDTYPE_H__
