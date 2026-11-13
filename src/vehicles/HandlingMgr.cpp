#include "common.h"

#include "main.h"
#include "FileMgr.h"
#include "Physical.h"
#include "HandlingMgr.h"

cHandlingDataMgr mod_HandlingManager;

// The client keeps the literal "data/handling.cfg" in .rodata @0x465ddb.
// CFileMgr paths are relative to the game root and the port already uses the
// SetDir("DATA") + bare-name idiom for the other data files.
const char *HandlingFilename = "handling.cfg";

// The client's terminator, compared as one 64-bit value at 0x1804e5c.
static const char *HandlingEnd = ";the end";

namespace {

// Reads one whitespace-delimited token into `out` (always NUL-terminated) and
// returns the position after it. `out` may be nil to just advance.
//
// The client uses strtok(" \t") and then atof/atoi/sscanf("%x") on the token.
const char *NextToken(const char *p, char *out, int outSize)
{
	if(out != nil && outSize > 0)
		out[0] = '\0';
	while(*p && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
	if(*p == '\0' || *p == '\n')
		return p;
	int n = 0;
	while(*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
		if(out != nil && n < outSize - 1) out[n++] = *p;
		p++;
	}
	if(out != nil && outSize > 0) out[n] = '\0';
	return p;
}

float TokenFloat(const char *s) { return (float)atof(s); }
int32 TokenInt(const char *s) { return (int32)atoi(s); }
// The client sscanf's the two flag columns with "%x": leading 0x is accepted and
// parsing stops at the first non-hex digit, which is what strtoul(..., 16) does.
uint32 TokenHex(const char *s) { return (uint32)strtoul(s, nil, 16); }
// Drive type (F/R/4) and engine type (P/D/E) are single characters: the client
// stores ldrb [token] only.
char TokenChar(const char *s) { return s[0]; }

void CopyName(char *dst, int dstSize, const char *tok)
{
	strncpy(dst, tok, dstSize - 1);
	dst[dstSize - 1] = '\0';
}

// --- column maps ----------------------------------------------------------
//
// Transcribed from the four jump tables of HandlingMgr::LoadHandlingData
// (chapter 2.5 and 2.7..2.10 of the analysis). Column 0 is always the name;
// the tables below record where each following column lands.
//
// CAR — 39 columns, column -> field in the order the client's jump table has
// them (chapter 2.7; the analysis data lists the target of every column):
//   1 fMass   2 fTurnMass   3 fDragMult   4..6 CentreOfMass
//   7 nPercentSubmerged   8 fTractionMultiplier   9 fTractionLoss
//  10 fTractionBias   11 nNumberOfGears   12 fMaxVelocity
//  13 fEngineAcceleration   14 fEngineInertia   15 nDriveType   16 nEngineType
//  17 fBrakeDeceleration   18 fBrakeBias   19 bABS   20 fSteeringLock
//  21 fSuspensionForceLevel   22 fSuspensionDampingLevel   23 ignored
//  24 fSuspensionUpperLimit   25 fSuspensionLowerLimit   26 fSuspensionBias
//  27 fSuspensionAntidiveMultiplier   28 fSeatOffsetDistance
//  29 fCollisionDamageMultiplier   30 nMonetaryValue
//  31 ModelFlags (hex)   32 HandlingFlags (hex)   33 FrontLights
//  34 RearLights   35 ignored   36 fHighLightsAngle   37 nAnimGroup
//  38 fSuspensionHighSpdComDamp
static void
ParseCarLine(const char *line, tHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		switch(col){
		case 0:  CopyName(h->Name, sizeof(h->Name), tok); break;
		case 1:  h->fMass = TokenFloat(tok); break;
		case 2:  h->fTurnMass = TokenFloat(tok); break;
		case 3:  h->fDragMult = TokenFloat(tok); break;
		case 4:  h->CentreOfMass.x = TokenFloat(tok); break;
		case 5:  h->CentreOfMass.y = TokenFloat(tok); break;
		case 6:  h->CentreOfMass.z = TokenFloat(tok); break;
		case 7:  h->nPercentSubmerged = TokenInt(tok); break;
		case 8:  h->fTractionMultiplier = TokenFloat(tok); break;
		case 9:  h->fTractionLoss = TokenFloat(tok); break;
		case 10: h->fTractionBias = TokenFloat(tok); break;
		case 11: h->Transmission.nNumberOfGears = (int8)TokenInt(tok); break;
		case 12: h->Transmission.fMaxVelocity = TokenFloat(tok); break;
		case 13: h->Transmission.fEngineAcceleration = TokenFloat(tok); break;
		case 14: h->Transmission.fEngineInertia = TokenFloat(tok); break;
		case 15: h->Transmission.nDriveType = TokenChar(tok); break;
		case 16: h->Transmission.nEngineType = TokenChar(tok); break;
		case 17: h->fBrakeDeceleration = TokenFloat(tok); break;
		case 18: h->fBrakeBias = TokenFloat(tok); break;
		case 19: h->bABS = TokenInt(tok) != 0; break;
		case 20: h->fSteeringLock = TokenFloat(tok); break;
		case 21: h->fSuspensionForceLevel = TokenFloat(tok); break;
		case 22: h->fSuspensionDampingLevel = TokenFloat(tok); break;
		case 23: break;	// no record field in the client either
		case 24: h->fSuspensionUpperLimit = TokenFloat(tok); break;
		case 25: h->fSuspensionLowerLimit = TokenFloat(tok); break;
		case 26: h->fSuspensionBias = TokenFloat(tok); break;
		case 27: h->fSuspensionAntidiveMultiplier = TokenFloat(tok); break;
		case 28: h->fSeatOffsetDistance = TokenFloat(tok); break;
		case 29: h->fCollisionDamageMultiplier = TokenFloat(tok); break;
		case 30: h->nMonetaryValue = TokenInt(tok); break;
		case 31: h->ModelFlags = TokenHex(tok); break;
		case 32: h->HandlingFlags = TokenHex(tok); break;
		case 33: h->FrontLights = (uint8)TokenInt(tok); break;
		case 34: h->RearLights = (uint8)TokenInt(tok); break;
		case 35: break;	// no record field in the client either
		case 36: h->fHighLightsAngle = TokenFloat(tok); break;
		case 37: h->nAnimGroup = (uint8)TokenInt(tok); break;
		case 38: h->fSuspensionHighSpdComDamp = TokenFloat(tok); break;
		default: return;	// past the last column: the client stops there too
		}
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

// BIKE — 16 columns, fully linear: name, index, then 15 floats at +0x1c..+0x54.
static void
ParseBikeLine(const char *line, tBikeHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		switch(col){
		case 0:  CopyName(h->Name, sizeof(h->Name), tok); break;
		case 1:  h->fLeanFwdCOM = TokenFloat(tok); break;
		case 2:  h->fLeanFwdForce = TokenFloat(tok); break;
		case 3:  h->fLeanBakCOM = TokenFloat(tok); break;
		case 4:  h->fLeanBackForce = TokenFloat(tok); break;
		case 5:  h->fMaxLean = TokenFloat(tok); break;
		case 6:  h->fFullAnimLean = TokenFloat(tok); break;
		case 7:  h->fDesLean = TokenFloat(tok); break;
		case 8:  h->fSpeedSteer = TokenFloat(tok); break;
		case 9:  h->fSlipSteer = TokenFloat(tok); break;
		case 10: h->fNoPlayerCOMz = TokenFloat(tok); break;
		case 11: h->fWheelieAng = TokenFloat(tok); break;
		case 12: h->fStoppieAng = TokenFloat(tok); break;
		case 13: h->fWheelieSteer = TokenFloat(tok); break;
		case 14: h->fWheelieStabMult = TokenFloat(tok); break;
		case 15: h->fStoppieStabMult = TokenFloat(tok); break;
		default: return;
		}
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

// FLYING — 22 columns. Columns 12, 13 and 14 are parsed and thrown away by the
// client (their table entries point back at the loop head), column 21 lands in
// the trailing scalar.
static void
ParseFlyingLine(const char *line, tFlyingHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		switch(col){
		case 0:  CopyName(h->Name, sizeof(h->Name), tok); break;
		case 1:  h->fThrust = TokenFloat(tok); break;
		case 2:  h->fThrustFallOff = TokenFloat(tok); break;
		case 3:  h->fYaw = TokenFloat(tok); break;
		case 4:  h->fYawStab = TokenFloat(tok); break;
		case 5:  h->fSideSlip = TokenFloat(tok); break;
		case 6:  h->fRoll = TokenFloat(tok); break;
		case 7:  h->fRollStab = TokenFloat(tok); break;
		case 8:  h->fPitch = TokenFloat(tok); break;
		case 9:  h->fPitchStab = TokenFloat(tok); break;
		case 10: h->fFormLift = TokenFloat(tok); break;
		case 11: h->fAttackLift = TokenFloat(tok); break;
		case 12: case 13: case 14: break;	// parsed and dropped by the client
		case 15: h->vecTurnRes.x = TokenFloat(tok); break;
		case 16: h->vecTurnRes.y = TokenFloat(tok); break;
		case 17: h->vecTurnRes.z = TokenFloat(tok); break;
		case 18: h->vecSpeedRes.x = TokenFloat(tok); break;
		case 19: h->vecSpeedRes.y = TokenFloat(tok); break;
		case 20: h->vecSpeedRes.z = TokenFloat(tok); break;
		case 21: h->fMoveRes = TokenFloat(tok); break;
		default: return;
		}
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

// BOAT — 15 columns. Column 14 lands back inside the record at +0x38, between
// columns 7 and 8; fields are assigned by name so the oddity stays here.
static void
ParseBoatLine(const char *line, tBoatHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		switch(col){
		case 0:  CopyName(h->Name, sizeof(h->Name), tok); break;
		case 1:  h->fThrustY = TokenFloat(tok); break;
		case 2:  h->fThrustZ = TokenFloat(tok); break;
		case 3:  h->fThrustAppZ = TokenFloat(tok); break;
		case 4:  h->fAqPlaneForce = TokenFloat(tok); break;
		case 5:  h->fAqPlaneLimit = TokenFloat(tok); break;
		case 6:  h->fAqPlaneOffset = TokenFloat(tok); break;
		case 7:  h->fWaveAudioMult = TokenFloat(tok); break;
		case 8:  h->vecMoveRes.x = TokenFloat(tok); break;
		case 9:  h->vecMoveRes.y = TokenFloat(tok); break;
		case 10: h->vecMoveRes.z = TokenFloat(tok); break;
		case 11: h->vecTurnRes.x = TokenFloat(tok); break;
		case 12: h->vecTurnRes.y = TokenFloat(tok); break;
		case 13: h->vecTurnRes.z = TokenFloat(tok); break;
		case 14: h->fLook_L_R_BehindCamHeight = TokenFloat(tok); break;
		default: return;
		}
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

// CRANE — one sscanf of 29 fields: name, 27 floats, one int.
static void
ParseCraneLine(const char *line, tCraneHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		if(col == 0)
			CopyName(h->Name, sizeof(h->Name), tok);
		else if(col <= 27)
			h->Values[col-1] = TokenFloat(tok);
		else if(col == 28){
			h->nValue = TokenInt(tok);
			return;
		}else
			return;
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

// VEHICLE ANIM GROUPS — '^' lines, 35 columns; the port needs the two group ids
// (columns 1 and 2) to be able to name the table at all.
static void
ParseAnimGroupLine(const char *line, tAnimGroupHandlingData *h)
{
	char tok[64];
	int col = 0;
	const char *p = line;
	for(;;){
		p = NextToken(p, tok, sizeof(tok));
		if(tok[0] == '\0')
			return;
		if(col == 0)
			CopyName(h->Name, sizeof(h->Name), tok);
		else if(col == 1)
			h->Groups[0] = TokenInt(tok);
		else if(col == 2)
			h->Groups[1] = TokenInt(tok);
		else
			return;
		col++;
		if(*p == '\0' || *p == '\n' || *p == '\r')
			return;
	}
}

} // namespace

cHandlingDataMgr::cHandlingDataMgr(void)
{
	memset(this, 0, sizeof(*this));
}

void
cHandlingDataMgr::Initialise(void)
{
	LoadHandlingData();
	field_0 = 0.1f;
	fWheelFriction = 0.9f;
	field_8 = 1.0f;
	field_C = 0.8f;
	field_10 = 0.98f;
}

void
cHandlingDataMgr::LoadHandlingData(void)
{
	NumCars = NumBikes = NumFlying = NumBoats = NumCranes = NumAnimGroups = 0;
	nMissing = 0;

	// The client reads the file into a buffer capped at 0x1b773f = 1 799 999
	// bytes (movk w2, #0x1b, lsl #16) and aborts if the file is empty or does
	// not contain the terminator. The client file is 116 786 bytes.
	static uint8 *handlingFile = nil;
	if(handlingFile == nil)
		handlingFile = (uint8*)malloc(0x1b773f);
	CFileMgr::SetDir("DATA");
	int32 size = CFileMgr::LoadFile(HandlingFilename, handlingFile, 0x1b773f, "r");
	CFileMgr::SetDir("");

	if(size <= 0){
		// The client aborts here ("Couldn't load %s!", chapter 2.1). A running
		// game cannot abort at this point of its boot, but it must not silently
		// continue with zeroed handling either: the counters stay at zero, every
		// lookup falls back to record 0, and the log says why.
		REVC_LOG_ERROR("handling.cfg: could not load DATA/%s", HandlingFilename);
		return;
	}

	char line[1024];	// the client's per-line limit (__strncpy_chk, w3 = 0x400)
	const char *p = (const char*)handlingFile;
	const char *end = p + size;

	// The shipped file starts with a UTF-8 byte order mark ("\xEF\xBB\xBF").
	// The client reads the file as bytes and its first line is a comment, so
	// the mark lands on a ';' line and is never looked at. Here it would hide
	// the ';' from the dispatcher below and turn the comment into a car record,
	// so it is skipped explicitly.
	if(size >= 3 && (uint8)p[0] == 0xEF && (uint8)p[1] == 0xBB && (uint8)p[2] == 0xBF)
		p += 3;

	while(p < end){
		const char *lineEnd = p;
		while(lineEnd < end && *lineEnd != '\n') lineEnd++;
		int32 lineLength = lineEnd - p;
		if(lineLength >= (int32)sizeof(line) - 1)
			lineLength = sizeof(line) - 1;
		memcpy(line, p, lineLength);
		line[lineLength] = '\0';
		p = lineEnd < end ? lineEnd + 1 : end;
		while(lineLength > 0 && line[lineLength-1] == '\r')
			line[--lineLength] = '\0';

		// Type is the first byte of the line, exactly like the client's
		// dispatcher at 0x1804e64. ';' and '^' skip the line — '^' is the
		// vehicle anim group table, which the client skips as well.
		const char *s = line;
		switch(*s){
		case '\0': case ';': case '^':
			continue;
		case '!':
		case '$':
		case '%':
		case '&':
			s++;		// marker byte, then the name column
			break;
		}

		// Terminator test: the client compares the first eight bytes of the line
		// with ";the end" and leaves the loop on a match.
		if(strncmp(s, HandlingEnd, strlen(HandlingEnd)) == 0)
			break;

		switch(line[0]){
		case '!':	// bike
			if(NumBikes >= HANDLING_MAX_BIKES){
				REVC_LOG_ERROR("handling.cfg: more than %d bike records", HANDLING_MAX_BIKES);
				break;
			}
			memset(&BikeHandlingData[NumBikes], 0, sizeof(tBikeHandlingData));
			ParseBikeLine(s, &BikeHandlingData[NumBikes]);
			BikeHandlingData[NumBikes].nIndex = NumBikes;
			ConvertBikeDataToGameUnits(&BikeHandlingData[NumBikes]);
			NumBikes++;
			break;
		case '$':	// flying
			if(NumFlying >= HANDLING_MAX_FLYING){
				REVC_LOG_ERROR("handling.cfg: more than %d flying records", HANDLING_MAX_FLYING);
				break;
			}
			memset(&FlyingHandlingData[NumFlying], 0, sizeof(tFlyingHandlingData));
			ParseFlyingLine(s, &FlyingHandlingData[NumFlying]);
			FlyingHandlingData[NumFlying].nIndex = NumFlying;
			NumFlying++;
			break;
		case '%':	// boat
			if(NumBoats >= HANDLING_MAX_BOATS){
				REVC_LOG_ERROR("handling.cfg: more than %d boat records", HANDLING_MAX_BOATS);
				break;
			}
			memset(&BoatHandlingData[NumBoats], 0, sizeof(tBoatHandlingData));
			ParseBoatLine(s, &BoatHandlingData[NumBoats]);
			BoatHandlingData[NumBoats].nIndex = NumBoats;
			NumBoats++;
			break;
		case '&':	// crane
			if(NumCranes >= HANDLING_MAX_CARS){
				REVC_LOG_ERROR("handling.cfg: more than %d crane records", HANDLING_MAX_CARS);
				break;
			}
			memset(&CraneHandlingData[NumCranes], 0, sizeof(tCraneHandlingData));
			ParseCraneLine(s, &CraneHandlingData[NumCranes]);
			CraneHandlingData[NumCranes].nIndex = NumCranes;
			NumCranes++;
			break;
		default:	// car
			if(NumCars >= HANDLING_MAX_CARS){
				REVC_LOG_ERROR("handling.cfg: more than %d car records (hard limit)", HANDLING_MAX_CARS);
				break;
			}
			memset(&HandlingData[NumCars], 0, sizeof(tHandlingData));
			ParseCarLine(s, &HandlingData[NumCars]);
			HandlingData[NumCars].nIndex = NumCars;
			ConvertDataToGameUnits(&HandlingData[NumCars]);
			NumCars++;
			break;
		}
	}

	REVC_LOG_INFO("handling.cfg: %u cars, %u bikes, %u flying, %u boats, %u cranes",
	              NumCars, NumBikes, NumFlying, NumBoats, NumCranes);
}

void
cHandlingDataMgr::ConvertDataToGameUnits(tHandlingData *handling)
{
	float velocity, a, b;

	// Units: the client file's acceleration/deceleration is m/s^2 while the
	// port's physics runs on 1/50 s steps, so velocities are divided by 50^2.
	// The same conversion VC applied, unchanged by the file format.
	handling->Transmission.fEngineAcceleration *= 1.0f/(50.0f*50.0f);
	handling->Transmission.fMaxVelocity *= 1000.0f/(60.0f*60.0f * 50.0f);
	handling->fBrakeDeceleration *= 1.0f/(50.0f*50.0f);

	// The VC parser derived fTurnMass from Dimensions (x*y, then *5 when small).
	// The client file has fTurnMass as a column; a line that stopped before it
	// leaves zero, and only then is the VC-style fallback applied so a short
	// record still has usable inertia.
	if(handling->fTurnMass <= 0.0f)
		handling->fTurnMass = Max(handling->fMass * 0.7f, 10.0f);

	// The two places that used the VC dimensions: air resistance was
	// Dimensions.x*Dimensions.z/mass, and the stabiliser offset used
	// Dimension.z as a body height. The client file has no dimensions, so the
	// drag column stands in for the first and the centre of mass for the second.
	// Both are converted once here so the physics keeps seeing port units.
	handling->fAirResistanceFactor = handling->fDragMult / Max(handling->fMass, 1.0f);
	handling->fApproxHeight = 2.0f*Abs(handling->CentreOfMass.z) + 1.0f;

	handling->fInvMass = handling->fMass > 0.0f ? 1.0f/handling->fMass : 0.0f;
	handling->fCollisionDamageMultiplier *= 2000.0f/handling->fMass;
	handling->fBuoyancy = handling->nPercentSubmerged != 0
		? 100.0f/handling->nPercentSubmerged * GRAVITY*handling->fMass
		: 0.0f;

	// Where engine acceleration and drag balance out: the same search VC did,
	// now on the client's fMaxVelocity/fDragMult.
	a = 0.0f;
	b = 100.0f;
	velocity = handling->Transmission.fMaxVelocity;
	while(a < b && velocity > 0.0f){
		velocity -= 0.01f;
		a = handling->Transmission.fEngineAcceleration/6.0f;
		float a_drag = 0.5f*SQR(velocity) * handling->fDragMult / handling->fMass;
		b = -velocity * (1.0f/(a_drag + 1.0f) - 1.0f);
	}

	// Reverse speed: the port's transmission still works VC-style (step 3
	// replaces it with the client's gear model), so the VC per-class constants
	// stay for now.
	handling->Transmission.fMaxCruiseVelocity = velocity;
	handling->Transmission.fMaxVelocity = velocity * 1.2f;
	if(handling->ModelFlags & MODEL_IS_BIKE)
		handling->Transmission.fMaxReverseVelocity = -0.05f;
	else
		handling->Transmission.fMaxReverseVelocity = -0.2f;

	if(handling->Transmission.nDriveType == '4')
		handling->Transmission.fEngineAcceleration /= 4.0f;
	else
		handling->Transmission.fEngineAcceleration /= 2.0f;

	// The gear model reads the boost bits; the client keeps them on the handling
	// record's own flag column, and tHandlingData::Transmission::Flags mirrors
	// it so the transmission code does not need the record back.
	handling->Transmission.Flags = handling->HandlingFlags;

	handling->Transmission.InitGearRatios();
}

void
cHandlingDataMgr::ConvertBikeDataToGameUnits(tBikeHandlingData *handling)
{
	// Chapter 2.13: the four angle fields are degrees in the file and are
	// converted once at load. The client multiplies fMaxLean/fWheelieAng/
	// fStoppieAng by sin(pi/180) and fFullAnimLean by the plain factor.
	handling->fMaxLean = Sin(DEGTORAD(handling->fMaxLean));
	handling->fFullAnimLean = DEGTORAD(handling->fFullAnimLean);
	handling->fWheelieAng = Sin(DEGTORAD(handling->fWheelieAng));
	handling->fStoppieAng = Sin(DEGTORAD(handling->fStoppieAng));
}

// Name lookup. The client stores the name inside every record and searches the
// array of the requested kind; the three helpers below are that search.
int32
cHandlingDataMgr::GetHandlingId(const char *name)
{
	for(uint32 i = 0; i < NumCars; i++)
		if(strncmp(HandlingData[i].Name, name, sizeof(HandlingData[i].Name)) == 0)
			return (int32)i;
	return -1;
}

int32
cHandlingDataMgr::GetBikeHandlingId(const char *name)
{
	for(uint32 i = 0; i < NumBikes; i++)
		if(strncmp(BikeHandlingData[i].Name, name, sizeof(BikeHandlingData[i].Name)) == 0)
			return (int32)i;
	return -1;
}

int32
cHandlingDataMgr::GetFlyingHandlingId(const char *name)
{
	for(uint32 i = 0; i < NumFlying; i++)
		if(strncmp(FlyingHandlingData[i].Name, name, sizeof(FlyingHandlingData[i].Name)) == 0)
			return (int32)i;
	return -1;
}

int32
cHandlingDataMgr::GetBoatHandlingId(const char *name)
{
	for(uint32 i = 0; i < NumBoats; i++)
		if(strncmp(BoatHandlingData[i].Name, name, sizeof(BoatHandlingData[i].Name)) == 0)
			return (int32)i;
	return -1;
}

int32
cHandlingDataMgr::GetCraneHandlingId(const char *name)
{
	for(uint32 i = 0; i < NumCranes; i++)
		if(strncmp(CraneHandlingData[i].Name, name, sizeof(CraneHandlingData[i].Name)) == 0)
			return (int32)i;
	return -1;
}
