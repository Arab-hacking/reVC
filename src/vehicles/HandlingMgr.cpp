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
			break;
	}

	// SA's tHandlingData::InitFromData ends with two things the client loader
	// also does: the handling flags are copied onto the transmission block, and
	// the engine acceleration is scaled by 0.4 before the unit conversion.
	h->Transmission.HandlingFlags = h->HandlingFlags;
	h->Transmission.fEngineAcceleration *= 0.4f;
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
	NumCars = NumBikes = NumFlying = NumBoats = NumCranes = 0;
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

		// Terminator test first, then the dispatcher: the client compares the
		// first eight bytes of the line with ";the end" at 0x1804e58 and only
		// then looks at the type byte at 0x1804e64. The order matters — the
		// terminator starts with ';', so a dispatcher-first parser treats it as
		// an ordinary comment and keeps reading past it.
		if(strncmp(line, HandlingEnd, strlen(HandlingEnd)) == 0)
			break;

		// Type is the first byte of the line, exactly like the client's
		// dispatcher at 0x1804e64. ';' and '^' skip the line — '^' is the
		// vehicle anim group table (30 lines), which the client skips as well:
		// nothing on the vehicle side reads it, so it is not stored.
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
	// SA's conversion (gta-reversed cHandlingDataMgr::ConvertDataToGameUnits,
	// 0x6F5080). Everything the physics touches is brought to the port's units
	// here, once: acceleration and braking to "per tick per tick", velocities to
	// "metres per tick".
	#define ACCEL_CONST    (1.0f/(50.0f*50.0f))
	#define VELOCITY_CONST (0.277778f/50.0f)

	cTransmission *t = &handling->Transmission;

	// tHandlingData::InitFromData already scaled the engine acceleration by 0.4
	// (see ParseCarLine); SA applies its unit conversion on top of that.
	t->fEngineAcceleration *= ACCEL_CONST;
	t->fMaxVelocity *= VELOCITY_CONST;
	handling->fBrakeDeceleration *= ACCEL_CONST;

	handling->fInvMass = 1.0f/handling->fMass;
	handling->fBuoyancy = handling->fMass * 0.8f / (float)Max(handling->nPercentSubmerged, 1);
	handling->fCollisionDamageMultiplier *= handling->fInvMass * 2000.0f;

	// SA finds the speed at which the engine acceleration and the drag balance
	// out. Note the two different drag formulas: above 0.01 the coefficient is
	// applied as mass-scaled drag, below it through the reciprocal form.
	float engineAccelLimit = t->fEngineAcceleration / 6.0f;
	float maxVelocity = t->fMaxVelocity;
	while(maxVelocity > 0.0f){
		maxVelocity -= 0.01f;
		if(handling->fDragMult >= 0.01f){
			if(SQR(maxVelocity) * (handling->fDragMult/2.0f) / 1000.0f <= engineAccelLimit)
				break;
			continue;
		}
		float fRecip = 1.0f/(SQR(maxVelocity) * handling->fDragMult + 1.0f);
		if((fRecip - 1.0f) * -maxVelocity <= engineAccelLimit)
			break;
	}

	t->fMaxFlatVelocity = maxVelocity;
	t->fMaxCruiseVelocity = maxVelocity;
	t->fMaxVelocity = maxVelocity * 1.2f;
	// Two-wheelers reverse at a fixed crawl; everything else at a quarter of
	// the flat speed, but never slower than 0.2 (m/s in port units).
	if(handling->ModelFlags & MODEL_IS_BIKE)
		t->fMaxReverseVelocity = -0.05f;
	else
		t->fMaxReverseVelocity = Min(-maxVelocity * 0.3f, -0.2f);

	t->fEngineAcceleration /= (t->nDriveType == '4') ? 4.0f : 2.0f;

	// The gearbox reads the 1G/2G boost bits; SA keeps the handling flags on the
	// transmission block.
	t->HandlingFlags = handling->HandlingFlags;

	t->InitGearRatios();

	// The client file has no Dimensions (VC kept x/y/z there), but the port's
	// physics still needs the two values VC derived from them: the air-resistance
	// factor (was Dimensions.x*Dimensions.z/mass) and the body height used to
	// offset the stabiliser (was Dimensions.z). fDragMult carries the drag and
	// the centre of mass the height, as before.
	handling->fAirResistanceFactor = handling->fDragMult / Max(handling->fMass, 1.0f);
	handling->fApproxHeight = 2.0f*Abs(handling->CentreOfMass.z) + 1.0f;

	#undef ACCEL_CONST
	#undef VELOCITY_CONST
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
