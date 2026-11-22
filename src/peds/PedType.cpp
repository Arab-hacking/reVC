#include "common.h"

#include "General.h"
#include "FileMgr.h"
#include "PedType.h"
#include "SaveBuf.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif

CPedType *CPedType::ms_apPedType[NUM_PEDTYPES];
CPedStat *CPedStats::ms_apPedStats[NUM_PEDSTATS];

void
CPedType::Initialise(void)
{
	int i;

	debug("Initialising CPedType...\n");
	for(i = 0; i < NUM_PEDTYPES; i++){
		ms_apPedType[i] = new CPedType;
		ms_apPedType[i]->m_flag = PED_FLAG_PLAYER1;
		ms_apPedType[i]->unknown1 = 0.0f;
		ms_apPedType[i]->unknown2 = 0.0f;
		// unknown3 not initialized
		ms_apPedType[i]->unknown4 = 0.0f;
		ms_apPedType[i]->unknown5 = 0.0f;
		ms_apPedType[i]->m_threats = 0;
		ms_apPedType[i]->m_avoid = 0;
	}
	debug("Loading ped data...\n");
	LoadPedData();
	debug("CPedType ready\n");
}

void
CPedType::Shutdown(void)
{
	int i;
	debug("Shutting down CPedType...\n");
	for(i = 0; i < NUM_PEDTYPES; i++)
		delete ms_apPedType[i];
	debug("CPedType shut down\n");
}

// CPedType::LoadPedData, 0x608B30. San Andreas writes the relations as four
// named lists per type - Hate, Dislike, Like, Respect - after a plain type
// table line. Every name on a list is a ped type and is turned into its bit,
// which is why FindPedFlag has to know all 32 types.
void
CPedType::LoadPedData(void)
{
	char *buf;
	char line[256];
	char word[32];
	ssize_t bp, buflen;
	int lp, linelen;
	int type;
	uint32 flags;
	float f1, f2, f3, f4, f5;

	type = NUM_PEDTYPES;
	buf = new char[16 * 1024];

	// data/ped.dat comes from the game archive like every other SA data table.
	// Threat and Avoid are the relations between ped types, so reading a VC file
	// from DATA while the SA set is loaded would describe the wrong population.
	buflen = 0;
#ifdef CUSTOM_MODELS
	{
		std::vector<uint8> archived;
		if(CCustomModels::ReadCommonDataFile("data/ped.dat", archived) &&
		   archived.size() > 0 && archived.size() < 16 * 1024){
			memcpy(buf, archived.data(), archived.size());
			buflen = (ssize_t)archived.size();
		}
	}
#endif
	if(buflen <= 0){
		// A build without the archive still reads the loose table, exactly as a
		// plain San Andreas install lays it out.
		CFileMgr::SetDir("DATA");
		buflen = CFileMgr::LoadFile("PED.DAT", (uint8*)buf, 16 * 1024, "r");
		CFileMgr::SetDir("");
	}
	if(buflen <= 0){
		REVC_LOG_WARNING("ped types: no type table in the archive or in DATA; ped relations stay empty");
		delete[] buf;
		return;
	}

	for(bp = 0; bp < buflen; ){
		// read file line by line
		for(linelen = 0; buf[bp] != '\n' && bp < buflen; bp++){
			// drop the excess instead of overrunning line[]
			if(linelen >= (int)sizeof(line) - 1)
				continue;
			if(buf[bp] == '\r' || buf[bp] == ',' || buf[bp] == '\t')
				line[linelen++] = ' ';
			else
				line[linelen++] = buf[bp];
		}
		bp++;
		line[linelen] = '\0';

		// skip white space
		for(lp = 0; line[lp] <= ' '; lp++);

		if(lp >= linelen ||		// FIX: game uses == here, but this is safer if we have empty lines
		   line[lp] == '#')
			continue;

		// Game uses just "line" here since sscanf already trims whitespace, but this is safer
		sscanf(&line[lp], "%31s", word);

		// A relation list is only readable against a type this build has. An
		// unknown type line leaves type at NUM_PEDTYPES and its lists are
		// dropped rather than written past the end of the array.
		if(strncmp(word, "Hate", 4) == 0 || strncmp(word, "Dislike", 7) == 0 ||
		   strncmp(word, "Like", 4) == 0 || strncmp(word, "Respect", 7) == 0){
			if(type >= NUM_PEDTYPES)
				continue;
			flags = 0;
			lp += (int)strlen(word);
			while(sscanf(&line[lp], "%31s", word) == 1 && lp <= linelen){
				flags |= FindPedFlag(word);
				// skip word
				while(line[lp] != ' ' && line[lp] != '\n' && line[lp] != '\0')
					lp++;
				// skip white space
				while(line[lp] == ' ')
					lp++;
			}
			// Hate and Dislike are what a ped avoids; Like and Respect are what
			// it is drawn to. The scan only ever reads threats, so the two
			// positive lists go to the avoid mask and the negative ones to the
			// threat mask, which is the way San Andreas stores them.
			if(strncmp(line, "Hate", 4) == 0 || strncmp(line, "Dislike", 7) == 0)
				ms_apPedType[type]->m_threats |= flags;
			else
				ms_apPedType[type]->m_avoid |= flags;
		}else{
			sscanf(line, "%31s %f %f %f %f %f", word, &f1, &f2, &f3, &f4, &f5);
			type = FindPedType(word);
			// A type line this build has no type for leaves nothing to fill in;
			// its relation lines are skipped by the same test.
			if(type >= NUM_PEDTYPES || type < 0)
				continue;
			ms_apPedType[type]->m_flag = FindPedFlag(word);
			// unknown values
			ms_apPedType[type]->unknown1 = f1 / 50.0f;
			ms_apPedType[type]->unknown2 = f2 / 50.0f;
			ms_apPedType[type]->unknown3 = f3 / 50.0f;
			ms_apPedType[type]->unknown4 = f4;
			ms_apPedType[type]->unknown5 = f5;
		}
	}

	delete[] buf;
}

// Every ped type name San Andreas writes in data/ped.dat, in id order.
static const struct { const char *name; ePedType type; } s_aPedTypeNames[] = {
	{ "PLAYER1",			PEDTYPE_PLAYER1 },
	{ "PLAYER2",			PEDTYPE_PLAYER2 },
	{ "PLAYER_NETWORK",		PEDTYPE_PLAYER_NETWORK },
	{ "PLAYER_UNUSED",		PEDTYPE_PLAYER_UNUSED },
	{ "CIVMALE",			PEDTYPE_CIVMALE },
	{ "CIVFEMALE",			PEDTYPE_CIVFEMALE },
	{ "COP",				PEDTYPE_COP },
	{ "GANG1",				PEDTYPE_GANG1 },
	{ "GANG2",				PEDTYPE_GANG2 },
	{ "GANG3",				PEDTYPE_GANG3 },
	{ "GANG4",				PEDTYPE_GANG4 },
	{ "GANG5",				PEDTYPE_GANG5 },
	{ "GANG6",				PEDTYPE_GANG6 },
	{ "GANG7",				PEDTYPE_GANG7 },
	{ "GANG8",				PEDTYPE_GANG8 },
	{ "GANG9",				PEDTYPE_GANG9 },
	{ "GANG10",				PEDTYPE_GANG10 },
	{ "DEALER",				PEDTYPE_DEALER },
	{ "MEDIC",				PEDTYPE_MEDIC },
	{ "FIREMAN",			PEDTYPE_FIREMAN },
	{ "CRIMINAL",			PEDTYPE_CRIMINAL },
	{ "BUM",				PEDTYPE_BUM },
	{ "PROSTITUTE",			PEDTYPE_PROSTITUTE },
	{ "SPECIAL",			PEDTYPE_SPECIAL },
	{ "MISSION1",			PEDTYPE_MISSION1 },
	{ "MISSION2",			PEDTYPE_MISSION2 },
	{ "MISSION3",			PEDTYPE_MISSION3 },
	{ "MISSION4",			PEDTYPE_MISSION4 },
	{ "MISSION5",			PEDTYPE_MISSION5 },
	{ "MISSION6",			PEDTYPE_MISSION6 },
	{ "MISSION7",			PEDTYPE_MISSION7 },
	{ "MISSION8",			PEDTYPE_MISSION8 },
};

// Looks a name up without logging; NUM_PEDTYPES means "no such type".
static ePedType
FindPedTypeQuietly(const char *type)
{
	for(uint32 i = 0; i < ARRAY_SIZE(s_aPedTypeNames); i++)
		if(strcmp(type, s_aPedTypeNames[i].name) == 0)
			return s_aPedTypeNames[i].type;
	return NUM_PEDTYPES;
}

// CPedType::FindPedType, 0x608790. Every name San Andreas puts in ped.dat is
// listed; a name that is not one of them is not a ped type at all.
ePedType
CPedType::FindPedType(const char *type)
{
	ePedType found = FindPedTypeQuietly(type);
	if(found != NUM_PEDTYPES)
		return found;
	// An SA type table can carry a name this build has no type for; an unknown
	// one must not take the whole game down, so the caller gets a value it can
	// test rather than an index past the array.
	REVC_LOG_WARNING("ped type %s is not a known type", type);
	return NUM_PEDTYPES;
}

// CPedType::GetPedFlag, 0x608830. A ped type's flag is the bit of its index.
// The threat and avoid masks are 32 bits wide, which is why the type count is
// exactly 32; anything past that has no representable flag and answers zero.
uint32
CPedType::GetPedFlag(ePedType type)
{
	if((uint32)type >= sizeof(uint32) * 8)
		return 0;
	return 1u << (uint32)type;
}

// Turns one word of a ped.dat relation list into a mask bit. A threat name
// (GUN, COP_CAR, FAST_CAR, EXPLOSION, DEADPEDS) has its own bit numbering,
// which is not a ped type index; a type name is turned into its flag.
uint32
CPedType::FindPedFlag(const char *type)
{
	static const struct { const char *name; ePedThreatFlags flag; } aThreats[] = {
		{ "GUN",		PED_THREAT_GUN },
		{ "COP_CAR",	PED_THREAT_COP_CAR },
		{ "FAST_CAR",	PED_THREAT_FAST_CAR },
		{ "EXPLOSION",	PED_THREAT_EXPLOSION },
		{ "DEADPEDS",	PED_THREAT_DEADPEDS },
	};
	for(uint32 i = 0; i < ARRAY_SIZE(aThreats); i++)
		if(strcmp(type, aThreats[i].name) == 0)
			return aThreats[i].flag;
	return GetPedFlag(FindPedTypeQuietly(type));
}

void
CPedType::Save(uint8 *buf, uint32 *size)
{
	*size = sizeof(CPedType) * NUM_PEDTYPES + SAVE_HEADER_SIZE;
INITSAVEBUF
	WriteSaveHeader(buf, 'P','T','P','\0', *size - SAVE_HEADER_SIZE);
	for(int i = 0; i < NUM_PEDTYPES; i++)
		WriteSaveBuf(buf, *ms_apPedType[i]);
VALIDATESAVEBUF(*size)
}

void
CPedType::Load(uint8 *buf, uint32 size)
{
INITSAVEBUF
	// original: SkipSaveBuf(buf, SAVE_HEADER_SIZE);
	CheckSaveHeader(buf, 'P', 'T', 'P', '\0', size - SAVE_HEADER_SIZE);

	for(int i = 0; i < NUM_PEDTYPES; i++)
		ReadSaveBuf(ms_apPedType[i], buf);
VALIDATESAVEBUF(size)
}

void
CPedStats::Initialise(void)
{
	int i;

	debug("Initialising CPedStats...\n");
	for(i = 0; i < NUM_PEDSTATS; i++){
		ms_apPedStats[i] = new CPedStat;
		ms_apPedStats[i]->m_nId = PEDSTAT_PLAYER;
		// GetPedStatType looks a table entry up by name, so the default has to
		// be a real, empty name rather than whatever the fresh allocation held.
		ms_apPedStats[i]->m_acName[0] = '\0';
		ms_apPedStats[i]->m_fFleeDistance = 20.0f;
		ms_apPedStats[i]->m_fHeadingChangeRate = 15.0f;
		ms_apPedStats[i]->m_nFear = 50;
		ms_apPedStats[i]->m_nTemper = 50;
		ms_apPedStats[i]->m_nLawfulness = 50;
		ms_apPedStats[i]->m_nSexiness = 50;
		ms_apPedStats[i]->m_fAttackStrength = 1.0f;
		ms_apPedStats[i]->m_fDefendWeakness = 1.0f;
		ms_apPedStats[i]->m_flags = 0;
		ms_apPedStats[i]->m_nDefaultDecisionMaker = 0;
	}
	debug("Loading pedstats data...\n");
	CPedStats::LoadPedStats();
	debug("CPedStats ready\n");
}

void
CPedStats::Shutdown(void)
{
	int i;
	debug("Shutting down CPedStats...\n");
	for(i = 0; i < NUM_PEDSTATS; i++)
		delete ms_apPedStats[i];
	debug("CPedStats shut down\n");
}

void
CPedStats::LoadPedStats(void)
{
	char *buf;
	char line[256];
	char name[32];
	ssize_t bp, buflen;
	int lp, linelen;
	int type;
	float fleeDist, headingChangeRate, attackStrength, defendWeakness;
	int fear, temper, lawfullness, sexiness, shootingRate, defaultDecisionMaker;

	type = 0;
	buf = new char[16 * 1024];

	// The stat table is data/pedstats.dat when the game archive carries one.
	// The archive shipped with this build does not, so the loose DATA file stays
	// as the source until it does; Initialise() has already filled the array
	// with defaults, and the loop below only overrides what it reads.
	buflen = 0;
#ifdef CUSTOM_MODELS
	{
		std::vector<uint8> archived;
		if(CCustomModels::ReadCommonDataFile("data/pedstats.dat", archived) &&
		   archived.size() < 16 * 1024){
			memcpy(buf, archived.data(), archived.size());
			buflen = (ssize_t)archived.size();
		}
	}
#endif
	if(buflen <= 0){
		CFileMgr::SetDir("DATA");
		buflen = CFileMgr::LoadFile("PEDSTATS.DAT", (uint8*)buf, 16 * 1024, "r");
		CFileMgr::SetDir("");
	}
	if(buflen <= 0){
		REVC_LOG_WARNING("ped stats: no stat table in the archive or in DATA; keeping the default stats");
		delete[] buf;
		return;
	}

	for(bp = 0; bp < buflen; ){
		// read file line by line
		for(linelen = 0; buf[bp] != '\n' && bp < buflen; bp++){
			// drop the excess instead of overrunning line[]
			if(linelen >= (int)sizeof(line) - 1)
				continue;
			if(buf[bp] == '\r' || buf[bp] == ',' || buf[bp] == '\t')
				line[linelen++] = ' ';
			else
				line[linelen++] = buf[bp];
		}
		bp++;
		line[linelen] = '\0';

		// skip white space
		for(lp = 0; line[lp] <= ' '; lp++);

		if(lp >= linelen ||		// FIX: game uses == here, but this is safer if we have empty lines
		   line[lp] == '#')
			continue;

		// SA row: name, seven stat columns, the shooting rate (which Vice City
		// already stored in m_flags) and the default decision maker.
		shootingRate = 0;
		defaultDecisionMaker = 0;
		sscanf(&line[lp], "%31s %f %f %d %d %d %d %f %f %d %d",
			name,
			&fleeDist,
			&headingChangeRate,
			&fear,
			&temper,
			&lawfullness,
			&sexiness,
			&attackStrength,
			&defendWeakness,
			&shootingRate,
			&defaultDecisionMaker);
		ms_apPedStats[type]->m_nId = (uint32)type;
		// Leave room for the terminator: GetPedStatType compares this field with
		// a whole-string comparison, so a full-width copy that fills all 24
		// bytes would run past the array.
		strncpy(ms_apPedStats[type]->m_acName, name, sizeof(ms_apPedStats[type]->m_acName) - 1);
		ms_apPedStats[type]->m_acName[sizeof(ms_apPedStats[type]->m_acName) - 1] = '\0';
		ms_apPedStats[type]->m_fFleeDistance = fleeDist;
		ms_apPedStats[type]->m_fHeadingChangeRate = headingChangeRate;
		ms_apPedStats[type]->m_nFear = (uint8)fear;
		ms_apPedStats[type]->m_nTemper = (uint8)temper;
		ms_apPedStats[type]->m_nLawfulness = (uint8)lawfullness;
		ms_apPedStats[type]->m_nSexiness = (uint8)sexiness;
		ms_apPedStats[type]->m_fAttackStrength = attackStrength;
		ms_apPedStats[type]->m_fDefendWeakness = defendWeakness;
		ms_apPedStats[type]->m_flags = (uint16)shootingRate;
		ms_apPedStats[type]->m_nDefaultDecisionMaker = (int8)defaultDecisionMaker;
		// The array is indexed by type; a table with more rows than stats must
		// stop rather than write past the end.
		if(++type >= NUM_PEDSTATS){
			REVC_LOG_WARNING("ped stats: table has more than %d entries, ignoring the rest", NUM_PEDSTATS);
			break;
		}
	}

	delete[] buf;
}

// 0x6088D0. An unknown name answers PEDSTAT_NONE: SA returns SENSIBLE_GUY
// here, which hides a typo in peds.ide by silently giving the ped a real
// personality, and every caller here already substitutes its own fallback.
ePedStats
CPedStats::GetPedStatType(const char *name)
{
	for(uint16 type = 0; type < NUM_PEDSTATS; type++)
		if(!CGeneral::faststrcmp(ms_apPedStats[type]->m_acName, name))
			return (ePedStats) type;

	return PEDSTAT_NONE;
}

CPedStat *
CPedStats::GetPedStatInfo(const char *name)
{
	for(uint16 type = 0; type < NUM_PEDSTATS; type++)
		if(!CGeneral::faststrcmp(ms_apPedStats[type]->m_acName, name))
			return ms_apPedStats[type];
	return nil;
}

CPedStat *
CPedStats::GetPedStatByArrayIndex(uint32 statIndex)
{
	return statIndex < NUM_PEDSTATS ? ms_apPedStats[statIndex] : nil;
}
