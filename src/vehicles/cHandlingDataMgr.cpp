#include "common.h"
#include "main.h"

#include "cHandlingDataMgr.h"
#include "FileMgr.h"
#include "VehicleAnimGroupData.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif

cHandlingDataMgr mod_HandlingManager;

// File -> game unit conversions. handling.cfg is written in km/h and seconds
// while the game works in metres per 1/50 s frame, so these are the factors the
// reference applies on load. Both live here because every caller must use the
// same pair.
static const float ACCEL_CONST = 1.0f / (50.0f * 50.0f);
static const float VELOCITY_CONST = 0.277778f / 50.0f;

// Longest handling line we accept. The SA records are 36 tab separated columns;
// a padded line with a long name still fits comfortably.
enum {
	HANDLING_LINE_SIZE = 512,
	HANDLING_LINES_MAX = 1024,
};

// Counts whitespace separated tokens so a malformed line can be reported
// without depending on the number of successfully converted fields.
static int
CountTokens(const char *line)
{
	int tokens = 0;
	bool inToken = false;

	for(const char *p = line; *p; p++){
		if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'){
			inToken = false;
		}else if(!inToken){
			inToken = true;
			tokens++;
		}
	}
	return tokens;
}

// Reads the next non-empty line of handling.cfg into `line`. Returns false at
// end of data (";the end"), end of file or when the line does not fit.
static bool
ReadHandlingLine(const uint8 *data, uint32 size, uint32 &pos, char *line, uint32 lineSize)
{
	while(pos < size){
		uint32 start = pos;
		while(pos < size && data[pos] != '\n')
			pos++;
		uint32 length = pos - start;
		if(pos < size)
			pos++;	// skip '\n'

		// strip CR and leading blanks
		while(length > 0 && (data[start + length - 1] == '\r' || data[start + length - 1] == ' '))
			length--;
		uint32 first = 0;
		while(first < length && (data[start + first] == ' ' || data[start + first] == '\t' || data[start + first] == '\r'))
			first++;

		if(first >= length)
			continue;	// blank line
		if(length - first >= lineSize)
			return false;	// overlong line: reject rather than truncate a record

		memcpy(line, data + start + first, length - first);
		line[length - first] = '\0';
		return true;
	}
	return false;
}

// Splits `line` into at most `maxTokens` NUL terminated fields in `tokens`
// storage. Fields are separated by any run of blanks/tabs.
static int
Tokenize(char *line, char *storage, uint32 storageSize, char **tokens, int maxTokens)
{
	int count = 0;
	uint32 used = 0;
	char *p = line;

	while(*p && count < maxTokens){
		while(*p == ' ' || *p == '\t')
			p++;
		if(*p == '\0')
			break;

		char *start = p;
		while(*p && *p != ' ' && *p != '\t')
			p++;
		uint32 length = p - start;

		if(used + length + 1 > storageSize)
			return -1;
		memcpy(storage + used, start, length);
		storage[used + length] = '\0';
		tokens[count++] = storage + used;
		used += length + 1;
	}
	return count;
}

static bool
ParseFloat(const char *token, float &out)
{
	char *end;
	out = strtof(token, &end);
	// strtof also accepts "nan"/"inf" and saturates overflow to HUGE_VALF;
	// none of those may reach the physics conversions below.
	return end != token && *end == '\0' && isfinite(out);
}

static bool
ParseInt(const char *token, int32 &out)
{
	char *end;
	long value = strtol(token, &end, 10);
	if(end == token || *end != '\0')
		return false;
	out = (int32)value;
	return true;
}

static bool
ParseHex(uint32 &out, const char *token)
{
	char *end;
	unsigned long value = strtoul(token, &end, 16);
	if(end == token || *end != '\0')
		return false;
	out = (uint32)value;
	return true;
}

static bool
ParseChar(char &out, const char *token)
{
	if(token[0] == '\0' || token[1] != '\0')
		return false;
	out = token[0];
	return true;
}

// MB (model flags) and HB (handling flags) columns: either a hex word or an
// empty column meaning "no flags". The reference writes them as 0 for most
// vehicles, but accepting a missing column keeps hand-written files usable.
static bool
ParseFlags(uint32 &out, const char *token)
{
	if(token[0] == '\0')
		return false;
	return ParseHex(out, token);
}

cHandlingDataMgr::cHandlingDataMgr(void)
{
	memset(HandlingData, 0, sizeof(HandlingData));
	memset(BikeHandlingData, 0, sizeof(BikeHandlingData));
	memset(FlyingHandlingData, 0, sizeof(FlyingHandlingData));
	memset(BoatHandlingData, 0, sizeof(BoatHandlingData));

	field_0 = 0.1f;
	fWheelFriction = 0.9f;
	field_8 = 1.0f;
	field_C = 0.8f;
	field_10 = 0.98f;

	for(int i = 0; i < NUM_HANDLINGS; i++)
		HandlingData[i].m_nVehicleId = i;
	for(int i = 0; i < NUM_BIKE_HANDLINGS; i++)
		BikeHandlingData[i].m_nVehicleId = HANDLING_BIKE + i;
	for(int i = 0; i < NUM_FLYING_HANDLINGS; i++)
		FlyingHandlingData[i].m_nVehicleId = HANDLING_SEAPLANE + i;
	for(int i = 0; i < NUM_BOAT_HANDLINGS; i++)
		BoatHandlingData[i].m_nVehicleId = HANDLING_PREDATOR + i;
}

void
cHandlingDataMgr::Initialise(void)
{
	LoadHandlingData();
}

// Parses one vehicle handling line. Columns, in file order:
// name mass turnMass dragMult centreOfMass(3) percentSubmerged tractionMultiplier
// tractionLoss tractionBias nGears maxVelocity engineAcceleration engineInertia
// driveType engineType brakeDeceleration brakeBias abs steeringLock
// suspensionForceLevel suspensionDampingLevel suspensionHighSpdComDamp
// suspensionUpperLimit suspensionLowerLimit suspensionBias antidive
// seatOffsetDistance collisionDamageMultiplier monetaryValue modelFlags
// handlingFlags frontLights rearLights animGroup
static bool
ParseHandlingData(tHandlingData *h, char **tokens, int count)
{
	// name + 35 columns
	if(count < 36)
		return false;

	int i = 1;	// skip the name, it was resolved to the id already
	int32 value;

	if(!ParseFloat(tokens[i++], h->m_fMass) || h->m_fMass <= 0.0f)
		return false;
	if(!ParseFloat(tokens[i++], h->m_fTurnMass))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fDragMult))
		return false;
	if(!ParseFloat(tokens[i++], h->m_vecCentreOfMass.x))
		return false;
	if(!ParseFloat(tokens[i++], h->m_vecCentreOfMass.y))
		return false;
	if(!ParseFloat(tokens[i++], h->m_vecCentreOfMass.z))
		return false;
	if(!ParseInt(tokens[i++], value) || value < 0 || value > 255)
		return false;
	h->m_nPercentSubmerged = (uint8)value;
	if(!ParseFloat(tokens[i++], h->m_fTractionMultiplier))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fTractionLoss))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fTractionBias))
		return false;
	if(!ParseInt(tokens[i++], value) || value < 1 || value > MAX_TRANSMISSION_GEARS)
		return false;
	h->m_Transmission.m_nNumberOfGears = (uint8)value;
	// The conversion below solves the top speed by stepping down in 0.01
	// increments, so an absurd file value would hang the load; the fastest
	// real record is a few hundred. Zero is legitimate (the crane records).
	if(!ParseFloat(tokens[i++], h->m_Transmission.m_fMaxVelocity) ||
	   h->m_Transmission.m_fMaxVelocity < 0.0f || h->m_Transmission.m_fMaxVelocity > 10000.0f)
		return false;
	if(!ParseFloat(tokens[i++], h->m_Transmission.m_fEngineAcceleration))
		return false;
	// SA pre-scales the engine acceleration column by 0.4 on load. The engine
	// model multiplies by 0.4 again in CalculateDriveAcceleration, so dropping
	// this factor would silently give every car 2.5x its real acceleration.
	// See the reference's tHandlingData::InitFromData.
	h->m_Transmission.m_fEngineAcceleration *= 0.4f;
	if(!ParseFloat(tokens[i++], h->m_Transmission.m_fEngineInertia))
		return false;
	if(!ParseChar((char &)h->m_Transmission.m_nDriveType, tokens[i++]))
		return false;
	if(!ParseChar((char &)h->m_Transmission.m_nEngineType, tokens[i++]))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fBrakeDeceleration))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fBrakeBias))
		return false;
	if(!ParseInt(tokens[i++], value) || value < 0 || value > 1)
		return false;
	h->m_bABS = (int8)value;
	if(!ParseFloat(tokens[i++], h->m_fSteeringLock))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionForceLevel))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionDampingLevel))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionHighSpdComDamp))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionUpperLimit))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionLowerLimit))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionBiasBetweenFrontAndRear))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSuspensionAntiDiveMultiplier))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fSeatOffsetDistance))
		return false;
	if(!ParseFloat(tokens[i++], h->m_fCollisionDamageMultiplier))
		return false;
	if(!ParseInt(tokens[i++], h->m_nMonetaryValue))
		return false;

	// Extended SA/BR fields (39 columns)
	if(count >= 39){
		if(!ParseFloat(tokens[i++], h->m_fSuspensionBiasFront))
			return false;
		if(!ParseFloat(tokens[i++], h->m_fSuspensionBiasRear))
			return false;
		if(!ParseFloat(tokens[i++], h->m_fSuspensionAntiDive))
			return false;
	}else{
		h->m_fSuspensionBiasFront = 0.0f;
		h->m_fSuspensionBiasRear = 0.0f;
		h->m_fSuspensionAntiDive = 0.0f;
	}

	uint32 flags;
	if(!ParseFlags(flags, tokens[i++]))
		return false;
	h->m_nModelFlags = (eVehicleHandlingModelFlags)flags;
	if(!ParseFlags(flags, tokens[i++]))
		return false;
	h->m_Transmission.m_nHandlingFlags = flags;
	h->m_nHandlingFlags = (eVehicleHandlingFlags)flags;

	if(!ParseInt(tokens[i++], value) || value < LIGHTS_LONG || value > LIGHTS_TALL)
		return false;
	h->m_nFrontLights = (eVehicleLightsSize)value;
	if(!ParseInt(tokens[i++], value) || value < LIGHTS_LONG || value > LIGHTS_TALL)
		return false;
	h->m_nRearLights = (eVehicleLightsSize)value;
	if(!ParseInt(tokens[i++], value) || value < 0 || value > 255)
		return false;
	h->m_nAnimGroup = (uint8)value;

	return true;
}

static bool
ParseBikeHandlingData(tBikeHandlingData *b, char **tokens, int count)
{
	if(count < 16)
		return false;

	int i = 1;
	float *fields[] = {
		&b->m_fLeanFwdCOM, &b->m_fLeanFwdForce, &b->m_fLeanBakCOM, &b->m_fLeanBakForce,
		&b->m_fMaxLean, &b->m_fFullAnimLean, &b->m_fDesLean, &b->m_fSpeedSteer,
		&b->m_fSlipSteer, &b->m_fNoPlayerCOMz, &b->m_fWheelieAng, &b->m_fStoppieAng,
		&b->m_fWheelieSteer, &b->m_fWheelieStabMult, &b->m_fStoppieStabMult,
	};
	for(uint32 f = 0; f < sizeof(fields) / sizeof(fields[0]); f++)
		if(!ParseFloat(tokens[i++], *fields[f]))
			return false;
	return true;
}

static bool
ParseBoatHandlingData(tBoatHandlingData *b, char **tokens, int count)
{
	if(count < 15)
		return false;

	int i = 1;
	float *fields[] = {
		&b->m_fThrustY, &b->m_fThrustZ, &b->m_fThrustAppZ, &b->m_fAqPlaneForce,
		&b->m_fAqPlaneLimit, &b->m_fAqPlaneOffset, &b->m_fWaveAudioMult,
		&b->m_vecMoveRes.x, &b->m_vecMoveRes.y, &b->m_vecMoveRes.z,
		&b->m_vecTurnRes.x, &b->m_vecTurnRes.y, &b->m_vecTurnRes.z,
		&b->m_fLook_L_R_BehindCamHeight,
	};
	for(uint32 f = 0; f < sizeof(fields) / sizeof(fields[0]); f++)
		if(!ParseFloat(tokens[i++], *fields[f]))
			return false;
	return true;
}

static bool
ParseFlyingHandlingData(tFlyingHandlingData *f, char **tokens, int count)
{
	if(count < 22)
		return false;

	int i = 1;
	float *fields[] = {
		&f->m_fThrust, &f->m_fThrustFallOff, &f->m_fYaw, &f->m_fYawStab, &f->m_fSideSlip,
		&f->m_fRoll, &f->m_fRollStab, &f->m_fPitch, &f->m_fPitchStab, &f->m_fFormLift,
		&f->m_fAttackLift, &f->m_fGearUpR, &f->m_fGearDownL, &f->m_fWindMult, &f->m_fMoveRes,
		&f->m_vecTurnRes.x, &f->m_vecTurnRes.y, &f->m_vecTurnRes.z,
		&f->m_vecSpeedRes.x, &f->m_vecSpeedRes.y, &f->m_vecSpeedRes.z,
	};
	for(uint32 k = 0; k < sizeof(fields) / sizeof(fields[0]); k++)
		if(!ParseFloat(tokens[i++], *fields[k]))
			return false;
	return true;
}

// Parses `data` as handling.cfg. Returns the number of records accepted; a
// malformed line is skipped and logged rather than aborting the load, because
// custom archives may carry a hand-edited table.
uint32
ParseHandlingFile(const uint8 *data, uint32 size);

uint32
ParseHandlingFile(const uint8 *data, uint32 size)
{
	char line[HANDLING_LINE_SIZE];
	char storage[HANDLING_LINE_SIZE];
	char *tokens[40];
	uint32 pos = 0;
	uint32 parsed = 0;
	uint32 skipped = 0;
	uint32 lineNumber = 0;

	while(ReadHandlingLine(data, size, pos, line, sizeof(line))){
		lineNumber++;

		if(strcmp(line, ";the end") == 0)
			break;
		if(line[0] == ';')
			continue;
		if(line[0] == '^'){
			// Vehicle animation group lines fill CVehicleAnimGroupData, the table
			// the get-in/get-out code asks for a clip's animation group.
			CVehicleAnimGroupData::LoadAGroupFromData(line);
			continue;
		}

		char recordKind = ' ';
		char *record = line;
		if(line[0] == '!' || line[0] == '$' || line[0] == '%'){
			recordKind = line[0];
			record = line + 1;
			while(*record == ' ' || *record == '\t')
				record++;
		}

		int count = Tokenize(record, storage, sizeof(storage), tokens, 40);
		if(count < 2){
			skipped++;
			continue;
		}

		// SA resolves the record by an exact name hit against VehicleNames. A
		// name that is not in the table is a data error, not a different format:
		// the archive carries the SA table, so there is nothing to translate.
		int32 id = mod_HandlingManager.FindExactWord(tokens[0], &VehicleNames[0][0],
			VEHICLE_HANDLING_NAME_LEN, NUM_HANDLINGS);
		if(id < 0){
			REVC_LOG_WARNING("handling.cfg line %u: unknown vehicle name %s", lineNumber, tokens[0]);
			skipped++;
			continue;
		}

		bool ok;
		switch(recordKind){
		case '!':
			ok = ParseBikeHandlingData(mod_HandlingManager.GetBikeHandlingPointer(id), tokens, count);
			if(ok)
				mod_HandlingManager.ConvertBikeDataToGameUnits(mod_HandlingManager.GetBikeHandlingPointer(id));
			break;
		case '$':
			ok = ParseFlyingHandlingData(mod_HandlingManager.GetFlyingPointer(id), tokens, count);
			break;
		case '%':
			ok = ParseBoatHandlingData(mod_HandlingManager.GetBoatPointer(id), tokens, count);
			break;
		default:
			ok = ParseHandlingData(mod_HandlingManager.GetVehiclePointer(id), tokens, count);
			if(ok)
				mod_HandlingManager.ConvertDataToGameUnits(mod_HandlingManager.GetVehiclePointer(id));
			break;
		}

		if(!ok){
			REVC_LOG_WARNING("handling.cfg line %u: malformed record for %s", lineNumber, tokens[0]);
			skipped++;
			continue;
		}
		parsed++;
	}

	if(skipped)
		REVC_LOG_WARNING("handling.cfg: %u records parsed, %u skipped", parsed, skipped);
	else
		REVC_LOG_INFO("handling.cfg: %u records parsed", parsed);

	return parsed;
}

void
cHandlingDataMgr::LoadHandlingData(void)
{
	static uint8 fileData[HANDLING_LINES_MAX * HANDLING_LINE_SIZE];

	// The handling table ships inside the game's own data archive, in the SA
	// column layout. That archive table is the one the game runs on; the loose
	// file read below is only the fallback for builds without the archive.
	ssize_t size = 0;
#ifdef CUSTOM_MODELS
	{
		std::vector<uint8> archived;
		if(CCustomModels::ReadCommonDataFile("data/handling.cfg", archived) &&
		   archived.size() <= sizeof(fileData)){
			memcpy(fileData, archived.data(), archived.size());
			size = (ssize_t)archived.size();
		}
	}
#endif
	if(size <= 0){
		CFileMgr::SetDir("DATA");
		size = CFileMgr::LoadFile("HANDLING.CFG", fileData, sizeof(fileData), "rb");
		CFileMgr::SetDir("");
	}

	if(size <= 0){
		// No silent fall back to a different data set: without handling the
		// vehicle simulation has nothing to work with, so this is fatal.
		REVC_LOG_ERROR("HANDLING.CFG is missing or unreadable (%d bytes)", (int)size);
		return;
	}

	// The vehicle animation groups start from SA's stock table; `^` lines in
	// the file parsed below replace the groups they name. A Vice City
	// handling.cfg names none, and an all-zero table sends every car-door
	// animation to the default ped group and zeroes every door timing.
	CVehicleAnimGroupData::InitialiseDefaults();
	ParseHandlingFile(fileData, (uint32)size);

	// Apply physics.json overrides (BR-specific collision response parameters)
#ifdef CUSTOM_MODELS
	{
		std::vector<uint8> physData;
		if(CCustomModels::ReadCommonDataFile("data/physics.json", physData)){
			// Parse and apply physics.json to handling records
			// This applies BR-specific angular/linear collision response parameters
			// and speed range to all vehicle handling records
			ApplyPhysicsJson(physData);
		}
	}
#endif
}

void
cHandlingDataMgr::ApplyPhysicsJson(const uint8 *data, uint32 size)
{
	// BR physics.json structure:
	// {
	//   "vehicles": {
	//     "angularCollisionResponseXY": [1.0, 0.4],
	//     "angularCollisionResponseZ": [1.0, 0.2],
	//     "collisionTorqueRateZ": [1.0, 0.4],
	//     "linearCollisionResponseXY": [1.0, 0.4],
	//     "linearCollisionResponseZ": [1.0, 0.2],
	//     "speedRange": [30.0, 60.0]
	//   }
	// }
	// Apply to all vehicle handling records

	// Simple JSON parse for the fields we need
	const char *p = (const char*)data;
	const char *end = (const char*)data + size;

	auto skipWs = [&](const char **pp){
		while(*pp < end && (**pp == ' ' || **pp == '\t' || **pp == '\r' || **pp == '\n'))
			(*pp)++;
	};

	auto findKey = [&](const char *key) -> const char* {
		const char *p = (const char*)data;
		size_t keyLen = strlen(key);
		while(p < end){
			skipWs(&p);
			if(p + keyLen + 2 <= end && p[0] == '"' && memcmp(p + 1, key, keyLen) == 0 && p[keyLen + 1] == '"'){
				p += keyLen + 2;
				skipWs(&p);
				if(*p == ':'){
					p++;
					skipWs(&p);
					return p;
				}
			}
			p++;
		}
		return nullptr;
	};

	auto parseFloatArray = [&](const char *p, float *out, int count) -> bool {
		skipWs(&p);
		if(*p != '[') return false;
		p++;
		for(int i = 0; i < count; i++){
			skipWs(&p);
			if(p >= end) return false;
			char *endPtr;
			float val = strtof(p, (char**)&p);
			if(p == (const char*)end) return false;
			out[i] = val;
			skipWs(&p);
			if(i < count - 1){
				if(*p != ',') return false;
				p++;
			}else{
				skipWs(&p);
				if(*p != ']') return false;
			}
		}
		return true;
	};

	const char *vehicles = findKey("vehicles");
	if(!vehicles) return;

	// Apply to all vehicle handling records
	for(uint32 i = 0; i < NUM_HANDLINGS; i++){
		tHandlingData *h = &HandlingData[i];
		if(h->m_nVehicleId == i){
			const char *ptr;
			if((ptr = findKey("angularCollisionResponseXY")) && parseFloatArray(ptr, h->m_fAngularCollisionResponseXY, 2)) {}
			if((ptr = findKey("angularCollisionResponseZ")) && parseFloatArray(ptr, h->m_fAngularCollisionResponseZ, 2)) {}
			if((ptr = findKey("collisionTorqueRateZ")) && parseFloatArray(ptr, h->m_fCollisionTorqueRateZ, 2)) {}
			if((ptr = findKey("linearCollisionResponseXY")) && parseFloatArray(ptr, h->m_fLinearCollisionResponseXY, 2)) {}
			if((ptr = findKey("linearCollisionResponseZ")) && parseFloatArray(ptr, h->m_fLinearCollisionResponseZ, 2)) {}
			if((ptr = findKey("speedRange")) && parseFloatArray(ptr, h->m_fSpeedRange, 2)) {}
		}
	}
}

int32
cHandlingDataMgr::FindExactWord(const char *word, const char *names, uint32 entrySize, uint32 numEntries)
{
	for(uint32 i = 0; i < numEntries; i++){
		const char *entry = names + entrySize * i;
		if(strncmp(word, entry, strlen(entry)) == 0)
			return (int32)i;
	}
	return -1;
}

int32
cHandlingDataMgr::GetHandlingId(const char *name)
{
	return FindExactWord(name, &VehicleNames[0][0], VEHICLE_HANDLING_NAME_LEN, NUM_HANDLINGS);
}

tHandlingData *
cHandlingDataMgr::GetVehiclePointer(uint32 handlingId)
{
	return &HandlingData[handlingId < NUM_HANDLINGS ? handlingId : 0];
}

tBikeHandlingData *
cHandlingDataMgr::GetBikeHandlingPointer(uint32 handlingId)
{
	if(handlingId >= HANDLING_BIKE && handlingId < HANDLING_BIKE + NUM_BIKE_HANDLINGS)
		return &BikeHandlingData[handlingId - HANDLING_BIKE];
	return &BikeHandlingData[0];
}

tFlyingHandlingData *
cHandlingDataMgr::GetFlyingPointer(uint32 handlingId)
{
	if(handlingId >= HANDLING_SEAPLANE && handlingId < HANDLING_SEAPLANE + NUM_FLYING_HANDLINGS)
		return &FlyingHandlingData[handlingId - HANDLING_SEAPLANE];
	return &FlyingHandlingData[0];
}

tBoatHandlingData *
cHandlingDataMgr::GetBoatPointer(uint32 handlingId)
{
	if(handlingId >= HANDLING_PREDATOR && handlingId < HANDLING_PREDATOR + NUM_BOAT_HANDLINGS)
		return &BoatHandlingData[handlingId - HANDLING_PREDATOR];
	return &BoatHandlingData[0];
}
// File units -> game units. This is where the SA engine model is established:
// the acceleration is split between the drive wheels, the top speed is solved
// against the drag curve, and the gear ratios are derived from the result.
void
cHandlingDataMgr::ConvertDataToGameUnits(tHandlingData *h)
{
	cTransmission *t = &h->m_Transmission;

	t->m_fEngineAcceleration *= ACCEL_CONST;
	t->m_fMaxVelocity *= VELOCITY_CONST;
	h->m_fBrakeDeceleration *= ACCEL_CONST;

	h->m_fMassRecip = 1.0f / h->m_fMass;
	// Records that never enter water (the cranes) carry percentSubmerged 0;
	// their buoyancy is no buoyancy at all, not a divide by zero.
	h->m_fBuoyancyConstant = h->m_nPercentSubmerged != 0
		? h->m_fMass * 0.8f / (float)h->m_nPercentSubmerged : 0.0f;
	h->m_fCollisionDamageMultiplier *= h->m_fMassRecip * 2000.0f;

	float engineAccelLimit = t->m_fEngineAcceleration / 6.0f;
	float maxVelocity = t->m_fMaxVelocity;

	while(maxVelocity > 0.0f){
		maxVelocity -= 0.01f;
		if(h->m_fDragMult >= 0.01f){
			if((maxVelocity * maxVelocity * (h->m_fDragMult / 2.0f) / 1000.0f) <= engineAccelLimit)
				break;
			continue;
		}

		float recip = 1.0f / (maxVelocity * maxVelocity * h->m_fDragMult + 1.0f);
		if((recip - 1.0f) * -maxVelocity <= engineAccelLimit)
			break;
	}

	if(h->m_nVehicleId == HANDLING_RCBANDIT){
		t->m_fMaxFlatVelocity = maxVelocity;
		t->m_fMaxCruiseVelocity = t->m_fMaxVelocity;
		t->m_fMaxReverseVelocity = -maxVelocity;
	}else if(h->m_bUseMaxspLimit){
		t->m_fMaxFlatVelocity = maxVelocity / 1.2f;
		t->m_fMaxCruiseVelocity = t->m_fMaxFlatVelocity;
		t->m_fMaxReverseVelocity = Min(-t->m_fMaxFlatVelocity / 4.0f, -0.2f);
	}else{
		t->m_fMaxVelocity = maxVelocity * 1.2f;
		t->m_fMaxFlatVelocity = maxVelocity;
		// AI traffic never drives at the player's overspeed allowance
		t->m_fMaxCruiseVelocity = maxVelocity;
		if(h->m_nVehicleId >= HANDLING_BIKE && h->m_nVehicleId <= HANDLING_FREEWAY){
			// two wheelers barely reverse
			t->m_fMaxReverseVelocity = -0.05f;
		}else{
			t->m_fMaxReverseVelocity = Min(-maxVelocity * 0.3f, -0.2f);
		}
	}

	t->m_fEngineAcceleration /= (t->m_nDriveType == '4') ? 4.0f : 2.0f;
	t->InitGearRatios();
}
// The exact inverse of ConvertDataToGameUnits: turns a live record back into
// file units, so a record can be written out in handling.cfg form again. The
// reference installs it at 0x6F5010 next to the forward conversion.
void
cHandlingDataMgr::ConvertDataToWorldUnits(tHandlingData *h)
{
	cTransmission *t = &h->m_Transmission;

	t->m_fMaxVelocity /= VELOCITY_CONST;
	h->m_fBrakeDeceleration /= ACCEL_CONST;
	t->m_fEngineAcceleration *= (t->m_nDriveType == '4' ? 4.0f : 2.0f) / ACCEL_CONST;
	h->m_fCollisionDamageMultiplier *= h->m_fMass / 2000.0f;
}

void
cHandlingDataMgr::ConvertBikeDataToGameUnits(tBikeHandlingData *b)
{
	b->m_fMaxLean = Sin(b->m_fMaxLean * (PI / 180.0f));
	b->m_fFullAnimLean = b->m_fFullAnimLean * (PI / 180.0f);
	b->m_fWheelieAng = Sin(b->m_fWheelieAng * (PI / 180.0f));
	b->m_fStoppieAng = Sin(b->m_fStoppieAng * (PI / 180.0f));
}

// The inverse of ConvertBikeDataToGameUnits (0x6F5240).
void
cHandlingDataMgr::ConvertBikeDataToWorldUnits(tBikeHandlingData *b)
{
	b->m_fMaxLean = Asin(b->m_fMaxLean) * (180.0f / PI);
	b->m_fFullAnimLean = b->m_fFullAnimLean * (180.0f / PI);
	b->m_fWheelieAng = Asin(b->m_fWheelieAng) * (180.0f / PI);
	b->m_fStoppieAng = Asin(b->m_fStoppieAng) * (180.0f / PI);
}
