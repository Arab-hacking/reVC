// Checks cHandlingDataMgr against the client's handling.cfg: the record
// counters, the column map of every record type, and the small format details
// the client relies on (UTF-8 BOM, ';the end' terminator, the type markers,
// '^' anim-group lines that are skipped, short lines that must not crash).
//
// Built outside the engine: tests/vehicles_stub holds the few engine headers
// HandlingMgr.cpp and Transmission.cpp need (common.h, FileMgr.h, ...), so the
// real sources are compiled unmodified against the stub.
#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "HandlingMgr.h"

static uint8 *gFileData = nil;
static int32 gFileSize = 0;

int32
CFileMgr::LoadFile(const char *name, uint8 *buf, int32 maxSize, const char *mode)
{
	(void)name; (void)mode;
	if(gFileData == nil || gFileSize > maxSize)
		return -1;
	memcpy(buf, gFileData, gFileSize);
	return gFileSize;
}

static int gProblems = 0;

#define CHECK(cond, ...) do { if(!(cond)){ printf("FAIL: " __VA_ARGS__); printf("\n"); gProblems++; } } while(0)
// Values that went through a conversion are compared with a relative epsilon:
// the file is read as float and then scaled, so 12/2500/2 and 12/2500*0.5 are
// not always the same bit pattern.
static bool
CloseEnough(double a, double b)
{
	double m = a < 0 ? -a : a;
	double n = b < 0 ? -b : b;
	double scale = m > n ? m : n;
	if(scale < 1e-6)
		return true;
	return (a > b ? a - b : b - a) / scale < 1e-5;
}

#define CHECKF(a, b, what) do { if(!CloseEnough((double)(a), (double)(b))){ \
	printf("FAIL: " what " %.9g != %.9g\n", (double)(a), (double)(b)); gProblems++; } } while(0)

// Feeds `size` bytes of `data` to the manager, the way the game does at boot.
static void
LoadFrom(const char *data, int32 size, const char *what)
{
	gFileData = (uint8*)data;
	gFileSize = size;
	mod_HandlingManager.Initialise();
	printf("%s: %u cars, %u bikes, %u flying, %u boats, %u cranes\n", what,
	       mod_HandlingManager.NumCars, mod_HandlingManager.NumBikes,
	       mod_HandlingManager.NumFlying, mod_HandlingManager.NumBoats,
	       mod_HandlingManager.NumCranes);
}

static void
DumpCar(const char *name)
{
	int32 id = mod_HandlingManager.GetHandlingId(name);
	if(id < 0){ printf("%s: not found\n", name); return; }
	tHandlingData *h = mod_HandlingManager.GetHandlingData(id);
	printf("%-10s id=%3d mass=%.0f turn=%.0f drag=%.2f com=(%.2f %.2f %.2f) sub=%d tract=%.2f/%.2f/%.2f\n",
	       name, id, h->fMass, h->fTurnMass, h->fDragMult,
	       h->CentreOfMass.x, h->CentreOfMass.y, h->CentreOfMass.z,
	       h->nPercentSubmerged, h->fTractionMultiplier, h->fTractionLoss, h->fTractionBias);
	printf("%-10s gears=%d maxv=%.2f accel=%.4f inertia=%.1f drive=%c eng=%c brake=%.4f bias=%.2f abs=%d steer=%.1f\n",
	       "", h->Transmission.nNumberOfGears, h->Transmission.fMaxVelocity,
	       h->Transmission.fEngineAcceleration, h->Transmission.fEngineInertia,
	       h->Transmission.nDriveType, h->Transmission.nEngineType,
	       h->fBrakeDeceleration, h->fBrakeBias, (int)h->bABS, h->fSteeringLock);
}

// ---------------------------------------------------------------------------
// Synthetic files. These pin the parser's behaviour, not the shipped data.
// ---------------------------------------------------------------------------

// A car line with a distinct value in every column so a shifted column map
// shows up as the wrong number landing in the wrong field.
static const char *kFullCarLine =
	"TESTCAR 1000 2000 0.5 0.1 0.2 0.3 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 1 25 "
	"1.5 0.25 0 0.4 -0.2 0.6 0.7 0.15 0.8 12345 800 4000000 2 3 0 0.75 7 0.125\n";

static void
BuildSynthetic(char *dst, size_t size, const char *prefix, const char *body, const char *suffix)
{
	snprintf(dst, size, "%s%s%s", prefix ? prefix : "", body, suffix ? suffix : "");
}

static void
TestFormatDetails(void)
{
	char buf[4096];

	// 1. The UTF-8 BOM must not turn the first comment line into a record: the
	// shipped file starts with one, and without the skip a ';' line parses as a
	// car named "\xEF\xBB\xBF;" with mass 0.
	BuildSynthetic(buf, sizeof(buf), "\xEF\xBB\xBF", ";comment line\n"
	               "TESTCAR 1000 2000 0.5 0 0 0 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 0 25\n", nil);
	LoadFrom(buf, strlen(buf), "bom+comment");
	CHECK(mod_HandlingManager.NumCars == 1, "BOM: %u cars instead of 1", mod_HandlingManager.NumCars);
	CHECK(mod_HandlingManager.GetHandlingId("TESTCAR") == 0, "BOM: TESTCAR not record 0");
	CHECK(mod_HandlingManager.GetHandlingData(0)->Name[0] == 'T', "BOM: first record is not TESTCAR");

	// 2. ";the end" stops the file: anything after it is not read.
	BuildSynthetic(buf, sizeof(buf), nil,
	               "TESTCAR 1000 2000 0.5 0 0 0 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 0 25\n"
	               ";the end\n"
	               "AFTEREND 1000 2000 0.5 0 0 0 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 0 25\n", nil);
	LoadFrom(buf, strlen(buf), "terminator");
	CHECK(mod_HandlingManager.NumCars == 1, "terminator: %u cars", mod_HandlingManager.NumCars);
	CHECK(mod_HandlingManager.GetHandlingId("AFTEREND") < 0, "terminator: read past the end");

	// 3. Type markers pick the array, and '^' anim-group lines are skipped
	// (the client skips them; the port needs the counts to stay right).
	BuildSynthetic(buf, sizeof(buf), nil,
	               "^ 0 1 2 3\n"
	               "TESTCAR 1000 2000 0.5 0 0 0 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 0 25\n"
	               "!TESTBIKE 0.35 0.15 0.34 0.10 45.0 38.0 0.93 0.70 0.5 0.1 35.0 -40.0 -0.009 0.7 0.6\n"
	               "$TESTFLY 0.5 0.40 -0.00015 0.002 0.10 0.003 -0.006 0.0003 0.0020 0.020 0.15 1.0 1.0 0.2 1.0 0.998 0.998 0.995 20.0 50.0 20.0\n"
	               "%TESTBOAT 0.50 0.6 0.5 6.0 0.60 -0.8 4.0 0.7 0.998 0.999 0.85 0.98 0.96 4.0\n"
	               "& TESTCRANE 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 9\n"
	               ";the end\n", nil);
	LoadFrom(buf, strlen(buf), "markers");
	CHECK(mod_HandlingManager.NumCars == 1, "markers: %u cars", mod_HandlingManager.NumCars);
	CHECK(mod_HandlingManager.NumBikes == 1, "markers: %u bikes", mod_HandlingManager.NumBikes);
	CHECK(mod_HandlingManager.NumFlying == 1, "markers: %u flying", mod_HandlingManager.NumFlying);
	CHECK(mod_HandlingManager.NumBoats == 1, "markers: %u boats", mod_HandlingManager.NumBoats);
	CHECK(mod_HandlingManager.NumCranes == 1, "markers: %u cranes", mod_HandlingManager.NumCranes);
	CHECK(strcmp(mod_HandlingManager.GetBikePointer(0)->Name, "TESTBIKE") == 0, "markers: bike name");
	CHECK(strcmp(mod_HandlingManager.GetBoatPointer(0)->Name, "TESTBOAT") == 0, "markers: boat name");
	// Crane layout: name, 27 floats at +0x1c.., then the int at +0x88.
	CHECK(mod_HandlingManager.GetCranePointer(0)->Values[0] == 1.0f, "crane value 0");
	CHECK(mod_HandlingManager.GetCranePointer(0)->Values[26] == 27.0f, "crane value 26");
	CHECK(mod_HandlingManager.GetCranePointer(0)->nValue == 9, "crane int %d",
	      mod_HandlingManager.GetCranePointer(0)->nValue);

	// 4. A short line stops at the last token and leaves the remaining fields
	// zero, without reading past them (the shipped car section holds short
	// boat/RC rows).
	BuildSynthetic(buf, sizeof(buf), nil,
	               "SHORT 1000 2000 0.5\n;the end\n", nil);
	LoadFrom(buf, strlen(buf), "short line");
	CHECK(mod_HandlingManager.NumCars == 1, "short: %u cars", mod_HandlingManager.NumCars);
	{
		tHandlingData *h = mod_HandlingManager.GetHandlingData(0);
		CHECK(h->fMass == 1000.0f && h->fTurnMass == 2000.0f, "short: mass/turn");
		CHECK(h->Transmission.nNumberOfGears == 0, "short: gears %d", h->Transmission.nNumberOfGears);
		CHECK(h->ModelFlags == 0 && h->HandlingFlags == 0, "short: flags not zero");
	}
}

// The column map itself: every column of a car record must land in its own
// field. Values are distinct so a shift is visible immediately.
static void
TestColumnMap(void)
{
	char buf[2048];
	BuildSynthetic(buf, sizeof(buf), nil, kFullCarLine, ";the end\n");
	LoadFrom(buf, strlen(buf), "column map");
	CHECK(mod_HandlingManager.NumCars == 1, "column map: %u cars", mod_HandlingManager.NumCars);

	tHandlingData *h = mod_HandlingManager.GetHandlingData(0);

	// Columns 1..11.
	CHECKF(h->fMass, 1000.0f, "col 1 fMass");
	CHECKF(h->fTurnMass, 2000.0f, "col 2 fTurnMass");
	CHECKF(h->fDragMult, 0.5f, "col 3 fDragMult");
	CHECKF(h->CentreOfMass.x, 0.1f, "col 4 CentreOfMass.x");
	CHECKF(h->CentreOfMass.y, 0.2f, "col 5 CentreOfMass.y");
	CHECKF(h->CentreOfMass.z, 0.3f, "col 6 CentreOfMass.z");
	CHECK(h->nPercentSubmerged == 60, "col 7 nPercentSubmerged %d", h->nPercentSubmerged);
	CHECKF(h->fTractionMultiplier, 0.7f, "col 8 fTractionMultiplier");
	CHECKF(h->fTractionLoss, 0.9f, "col 9 fTractionLoss");
	CHECKF(h->fTractionBias, 0.4f, "col 10 fTractionBias");
	CHECK(h->Transmission.nNumberOfGears == 5, "col 11 gears %d", h->Transmission.nNumberOfGears);

	// Columns 12..16: the transmission block. fMaxVelocity is replaced by the
	// drag/acceleration equilibrium during load, so it is not compared. The
	// engine acceleration goes through SA's chain: x0.4 in the parser, then
	// x1/50^2 for the tick conversion, then /2 (or /4 for four-wheel drive).
	CHECKF(h->Transmission.fEngineInertia, 40.0f, "col 14 fEngineInertia");
	CHECKF(h->Transmission.fEngineAcceleration, 12.0f*0.4f/(50.0f*50.0f)/2.0f, "col 13 fEngineAcceleration");
	CHECK(h->Transmission.nDriveType == 'R', "col 15 drive '%c'", h->Transmission.nDriveType);
	CHECK(h->Transmission.nEngineType == 'P', "col 16 engine '%c'", h->Transmission.nEngineType);
	// The gear box reads the handling flags from the transmission block, where
	// SA keeps its own copy.
	CHECK(h->Transmission.HandlingFlags == 0x4000000, "transmission flags %08x", h->Transmission.HandlingFlags);

	// Columns 17..20.
	CHECKF(h->fBrakeDeceleration, 7.0f/(50.0f*50.0f), "col 17 fBrakeDeceleration");
	CHECKF(h->fBrakeBias, 0.55f, "col 18 fBrakeBias");
	CHECK(h->bABS == true, "col 19 bABS %d", (int)h->bABS);
	CHECKF(h->fSteeringLock, 25.0f, "col 20 fSteeringLock");

	// Columns 21..27, skipping the ignored 23.
	CHECKF(h->fSuspensionForceLevel, 1.5f, "col 21 fSuspensionForceLevel");
	CHECKF(h->fSuspensionDampingLevel, 0.25f, "col 22 fSuspensionDampingLevel");
	CHECKF(h->fSuspensionUpperLimit, 0.4f, "col 24 fSuspensionUpperLimit");
	CHECKF(h->fSuspensionLowerLimit, -0.2f, "col 25 fSuspensionLowerLimit");
	CHECKF(h->fSuspensionBias, 0.6f, "col 26 fSuspensionBias");
	CHECKF(h->fSuspensionAntidiveMultiplier, 0.7f, "col 27 fSuspensionAntidiveMultiplier");

	// Columns 28..30.
	CHECKF(h->fSeatOffsetDistance, 0.15f, "col 28 fSeatOffsetDistance");
	CHECKF(h->fCollisionDamageMultiplier, 0.8f*2000.0f/1000.0f, "col 29 fCollisionDamageMultiplier");
	CHECK(h->nMonetaryValue == 12345, "col 30 money %d", h->nMonetaryValue);

	// Columns 31..34: two hex flag masks and the two light types.
	CHECK(h->ModelFlags == 0x800, "col 31 ModelFlags %08x", h->ModelFlags);
	CHECK(h->HandlingFlags == 0x4000000, "col 32 HandlingFlags %08x", h->HandlingFlags);
	CHECK(h->FrontLights == 2, "col 33 front lights %d", h->FrontLights);
	CHECK(h->RearLights == 3, "col 34 rear lights %d", h->RearLights);

	// Columns 36..38, skipping the ignored 35.
	CHECKF(h->fHighLightsAngle, 0.75f, "col 36 fHighLightsAngle");
	CHECK(h->nAnimGroup == 7, "col 37 nAnimGroup %d", h->nAnimGroup);
	CHECKF(h->fSuspensionHighSpdComDamp, 0.125f, "col 38 fSuspensionHighSpdComDamp");

	// Derived values must be consistent with the columns they come from (SA's
	// ConvertDataToGameUnits: buoyancy is mass*0.8/percentSubmerged).
	CHECKF(h->fInvMass, 1.0f/1000.0f, "fInvMass");
	CHECKF(h->fBuoyancy, 1000.0f*0.8f/60.0f, "fBuoyancy");
	CHECKF(h->fAirResistanceFactor, 0.5f/1000.0f, "fAirResistanceFactor");
	CHECKF(h->fApproxHeight, 2.0f*0.3f + 1.0f, "fApproxHeight");

	// The gear table follows SA's InitGearRatios: evenly spaced over the top
	// speed, first gear at half a gear's worth, last gear on fMaxVelocity.
	cTransmission *t = &h->Transmission;
	float halfGear = 0.5f * t->fMaxVelocity / t->nNumberOfGears;
	float maxGear = t->fMaxVelocity - halfGear;
	CHECKF(t->Gears[1].MaxVelocity, 1.0f*maxGear/5.0f + halfGear, "gear 1 top");
	CHECKF(t->Gears[5].MaxVelocity, 5.0f*maxGear/5.0f + halfGear, "gear 5 top");
	CHECKF(t->Gears[5].MaxVelocity, t->fMaxVelocity, "top gear reaches fMaxVelocity");
	CHECKF(t->Gears[0].MaxVelocity, t->fMaxReverseVelocity, "reverse gear top");
	// The gears are strictly increasing, which is what the shift logic needs.
	for(int i = 1; i < 5; i++)
		CHECK(t->Gears[i].MaxVelocity < t->Gears[i+1].MaxVelocity,
		      "gear %d top %.4f is not below gear %d top %.4f",
		      i, t->Gears[i].MaxVelocity, i+1, t->Gears[i+1].MaxVelocity);
}

// A name that is only in a secondary section must not resolve to a car record:
// the model info stores a car-record index, and the type-specific helpers find
// their record through that name.
static void
TestLookup(void)
{
	char buf[1024];
	BuildSynthetic(buf, sizeof(buf), nil,
	               "TESTCAR 1000 2000 0.5 0 0 0 60 0.7 0.9 0.4 5 200 12 40 R P 7 0.55 0 25\n"
	               "!ONLYBIKE 0.35 0.15 0.34 0.10 45.0 38.0 0.93 0.70 0.5 0.1 35.0 -40.0 -0.009 0.7 0.6\n"
	               ";the end\n", nil);
	LoadFrom(buf, strlen(buf), "lookup");
	CHECK(mod_HandlingManager.GetHandlingId("TESTCAR") == 0, "lookup: TESTCAR");
	CHECK(mod_HandlingManager.GetHandlingId("ONLYBIKE") < 0, "lookup: bike-only name resolved as a car");
	CHECK(mod_HandlingManager.GetBikeHandlingId("ONLYBIKE") == 0, "lookup: ONLYBIKE bike record");
	CHECK(mod_HandlingManager.GetBikeHandlingId("TESTCAR") < 0, "lookup: TESTCAR has no bike record");
	// The pointer helpers fall back to the first record instead of reading out
	// of bounds; that is what the callers rely on.
	tBikeHandlingData *b = mod_HandlingManager.GetBikePointer(-1);
	CHECK(b == mod_HandlingManager.GetBikePointer(0), "lookup: bike fallback");
	tFlyingHandlingData *f = mod_HandlingManager.GetFlyingPointer(9999);
	CHECK(f == mod_HandlingManager.GetFlyingPointer(0), "lookup: flying fallback");
	tBoatHandlingData *o = mod_HandlingManager.GetBoatPointer(-1);
	CHECK(o == mod_HandlingManager.GetBoatPointer(0), "lookup: boat fallback");
}

// ---------------------------------------------------------------------------
// The shipped file.
// ---------------------------------------------------------------------------
static void
TestShippedFile(void)
{
	mod_HandlingManager.Initialise();

	printf("shipped: %u cars, %u bikes, %u flying, %u boats, %u cranes\n",
	       mod_HandlingManager.NumCars, mod_HandlingManager.NumBikes,
	       mod_HandlingManager.NumFlying, mod_HandlingManager.NumBoats,
	       mod_HandlingManager.NumCranes);

	// The client file has 567 car records, 23 bikes, 24 flying, 16 boats and 2
	// cranes (the 597 in the loader chapter counts a different build and was
	// polluted by the BOM line).
	CHECK(mod_HandlingManager.NumCars == 567, "car count %u != 567", mod_HandlingManager.NumCars);
	CHECK(mod_HandlingManager.NumBikes == 23, "bike count %u != 23", mod_HandlingManager.NumBikes);
	CHECK(mod_HandlingManager.NumFlying == 24, "flying count %u != 24", mod_HandlingManager.NumFlying);
	CHECK(mod_HandlingManager.NumBoats == 16, "boat count %u != 16", mod_HandlingManager.NumBoats);
	CHECK(mod_HandlingManager.NumCranes == 2, "crane count %u != 2", mod_HandlingManager.NumCranes);

	// Every car record has to be self-consistent: these are the columns the
	// physics reads, and a shifted map shows up here immediately.
	for(uint32 i = 0; i < mod_HandlingManager.NumCars; i++){
		tHandlingData *h = mod_HandlingManager.GetHandlingData(i);
		CHECK(h->fMass >= 100.0f && h->fMass < 200000.0f, "%s: mass %.1f", h->Name, h->fMass);
		CHECK(h->fTurnMass > 0.0f, "%s: turn mass %.1f", h->Name, h->fTurnMass);
		CHECK(h->Transmission.nDriveType == 'F' || h->Transmission.nDriveType == 'R' ||
		      h->Transmission.nDriveType == '4', "%s: drive '%c'", h->Name, h->Transmission.nDriveType);
		CHECK(h->Transmission.nEngineType == 'P' || h->Transmission.nEngineType == 'D' ||
		      h->Transmission.nEngineType == 'E', "%s: engine '%c'", h->Name, h->Transmission.nEngineType);
		CHECK(h->Transmission.nNumberOfGears >= 0 && h->Transmission.nNumberOfGears <= 6,
		      "%s: gears %d", h->Name, h->Transmission.nNumberOfGears);
		// Bounds only: the file itself has rows where the "lower" column is
		// above the "upper" one (CUPBOAT 0.10/0.30), so ordering is not a
		// property the parser can be held to. The values must be the file's.
		CHECK(h->fSuspensionUpperLimit > -50.0f && h->fSuspensionUpperLimit < 50.0f &&
		      h->fSuspensionLowerLimit > -50.0f && h->fSuspensionLowerLimit < 50.0f,
		      "%s: suspension limits %.2f/%.2f", h->Name, h->fSuspensionUpperLimit, h->fSuspensionLowerLimit);
		CHECK(h->FrontLights <= 3 && h->RearLights <= 3,
		      "%s: lights %d/%d", h->Name, h->FrontLights, h->RearLights);
		CHECK(h->fBrakeDeceleration >= 0.0f, "%s: brake %.4f", h->Name, h->fBrakeDeceleration);
		CHECK(h->fSteeringLock == 0.0f || (h->fSteeringLock >= 20.0f && h->fSteeringLock <= 55.0f),
		      "%s: steer lock %.1f", h->Name, h->fSteeringLock);
	}

	// Families: the same name exists in the car section (physics) and in its own
	// section (behaviour), which is what the client's Get*Pointer relies on.
	static const char *families[] = { "BIKE", "FREEWAY", "NRG500", "PREDATOR", "SPEEDER",
	                                  "DINGHY", "MAVERICK", "DODO", "SPARROW", "SEAPLANE" };
	for(uint32 i = 0; i < sizeof(families)/sizeof(families[0]); i++){
		const char *n = families[i];
		CHECK(mod_HandlingManager.GetHandlingId(n) >= 0, "%s: no car record", n);
		CHECK(mod_HandlingManager.GetBikeHandlingId(n) >= 0 ||
		      mod_HandlingManager.GetFlyingHandlingId(n) >= 0 ||
		      mod_HandlingManager.GetBoatHandlingId(n) >= 0, "%s: no type record", n);
	}

	// Names that are not in the stock file must NOT resolve (custom worlds).
	CHECK(mod_HandlingManager.GetHandlingId("NOSUCHCAR") < 0, "NOSUCHCAR resolved");

	DumpCar("ADMIRAL");
	DumpCar("LANDSTAL");
	DumpCar("INFERNUS");
	DumpCar("DINGHY");
	DumpCar("BUS");

	// ADMIRAL's row, straight from the file: mass 1670, 5 gears, F/P, brake
	// 8.5 and 30 degrees of steering lock.
	{
		tHandlingData *h = mod_HandlingManager.GetHandlingData(mod_HandlingManager.GetHandlingId("ADMIRAL"));
		CHECKF(h->fMass, 1670.0f, "ADMIRAL mass");
		CHECK(h->Transmission.nNumberOfGears == 5, "ADMIRAL gears %d", h->Transmission.nNumberOfGears);
		CHECK(h->Transmission.nDriveType == 'F', "ADMIRAL drive '%c'", h->Transmission.nDriveType);
		CHECKF(h->fSteeringLock, 30.0f, "ADMIRAL steering lock");
		CHECKF(h->fBrakeDeceleration, 8.5f/(50.0f*50.0f), "ADMIRAL brake");
		CHECKF(h->CentreOfMass.z, -0.05f, "ADMIRAL CentreOfMass.z");
		// Column 23 of this row is 0 and column 38 is 0: the record has 36
		// columns, so the fields after it stay zero.
	}

	// The boat record is found through the car record's name, and the last
	// column (14) lands at +0x38 — inside the block, not after it.
	{
		int32 id = mod_HandlingManager.GetHandlingId("PREDATOR");
		tBoatHandlingData *b = mod_HandlingManager.GetBoatPointer(id);
		CHECKF(b->fThrustY, 0.50f, "PREDATOR thrustY");
		CHECKF(b->fLook_L_R_BehindCamHeight, 4.0f, "PREDATOR cam height");
	}

	// Bike columns are linear; the four angle columns are converted to radians
	// (or their sine) once at load, as the client's 0x1804d9c block does.
	{
		int32 id = mod_HandlingManager.GetHandlingId("BIKE");
		tBikeHandlingData *bk = mod_HandlingManager.GetBikePointer(id);
		CHECKF(bk->fLeanFwdCOM, 0.35f, "BIKE leanFwdCOM");
		CHECKF(bk->fMaxLean, Sin(DEGTORAD(45.0f)), "BIKE maxLean");
		CHECKF(bk->fFullAnimLean, DEGTORAD(38.0f), "BIKE fullAnimLean");
	}

	// Flying: columns 12..14 are parsed and dropped, 15..21 land at +0x48..+0x60
	// (the last one is what the flight code reads as its damping).
	{
		int32 id = mod_HandlingManager.GetHandlingId("DODO");
		tFlyingHandlingData *fl = mod_HandlingManager.GetFlyingPointer(id);
		CHECKF(fl->fThrust, 0.5f, "DODO thrust");
		CHECKF(fl->fThrustFallOff, 0.4f, "DODO falloff");
		CHECKF(fl->fMoveRes, 20.0f, "DODO fMoveRes");
	}

	for(uint32 i = 0; i < mod_HandlingManager.NumCranes; i++){
		tCraneHandlingData *c = mod_HandlingManager.GetCranePointer(i);
		printf("crane %u: %s value=%d last=%.2f\n", i, c->Name, c->nValue, c->Values[26]);
	}
}


// ---------------------------------------------------------------------------
// The gearbox itself: SA's CalculateDriveAcceleration (gta-reversed 0x6D05E0).
// ---------------------------------------------------------------------------
static void
TestDriveAcceleration(void)
{
	cTransmission t;
	memset(&t, 0, sizeof(t));
	t.nNumberOfGears = 5;
	t.nDriveType = 'R';
	t.fMaxVelocity = 1.0f;          // metres per tick, arbirary but consistent
	t.fMaxFlatVelocity = 1.0f / 1.2f;
	t.fMaxReverseVelocity = -0.2f;
	t.fEngineAcceleration = 0.004f;
	t.fEngineInertia = 20.0f;
	t.HandlingFlags = 0;
	t.InitGearRatios();

	const float ts = CTimer::GetTimeStep();

	// 1. A single gear with the same numbers: speedMultiplier is 1, so the force
	// is engineAcceleration * 0.4 * gas * timestep, with no inertia damping
	// (a6/a7 are null). This is the amount the old VC code multiplied by a
	// per-gear constant instead.
	{
		cTransmission one;
		memset(&one, 0, sizeof(one));
		one.nNumberOfGears = 1;
		one.fMaxVelocity = 1.0f;
		one.fMaxReverseVelocity = -0.2f;
		one.fEngineAcceleration = 0.004f;
		one.fEngineInertia = 20.0f;
		one.InitGearRatios();

		uint8 gear = 1;
		float velocity = 0.0f;
		float a = one.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(a, 0.004f * 0.4f * 1.0f * ts, "one-gear drive acceleration");
		CHECK(gear == 1, "one-gear: gear moved to %d", gear);
	}

	// 2. The pull falls off with the gear: first gear has gearNumber = 1, so
	// multiplier 3+1; the top gear has gearNumber = 0, multiplier 1.
	{
		uint8 gear = 1;
		float velocity = 0.0f;
		float a1 = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(a1, 0.004f * 4.0f * 0.4f * ts, "first gear drive acceleration");

		// Sit inside the top gear's own span: above its ChangeDownVelocity and
		// below its ChangeUpVelocity, otherwise the loop would shift down first.
		gear = 5;
		velocity = 0.85f;
		CHECK(t.Gears[5].ChangeDownVelocity < velocity && velocity <= t.Gears[5].ChangeUpVelocity,
		      "test velocity is not inside the top gear's span");
		float a5 = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECK(gear == 5, "top gear test left gear %d", gear);
		CHECKF(a5, 0.004f * 1.0f * 0.4f * ts, "fifth gear drive acceleration");
		CHECK(a5 < a1, "top gear must pull less than first");
	}

	// 3. 1G and 2G boost raise the multiplier (5 and 4 instead of 3).
	{
		cTransmission boosted;
		memset(&boosted, 0, sizeof(boosted));
		boosted.nNumberOfGears = 5;
		boosted.fMaxVelocity = 1.0f;
		boosted.fMaxReverseVelocity = -0.2f;
		boosted.fEngineAcceleration = 0.004f;
		boosted.fEngineInertia = 20.0f;
		boosted.HandlingFlags = HANDLING_1G_BOOST;
		boosted.InitGearRatios();

		uint8 gear = 1;
		float velocity = 0.0f;
		float a = boosted.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(a, 0.004f * 6.0f * 0.4f * ts, "1G boost first gear");

		boosted.HandlingFlags = HANDLING_2G_BOOST;
		gear = 1;
		velocity = 0.0f;
		a = boosted.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(a, 0.004f * 5.0f * 0.4f * ts, "2G boost first gear");
	}

	// 4. The inertia history damps the first frames: with a6/a7 the acceleration
	// starts well below the undamped value and climbs, which is what stops a car
	// from pulling at full engine force from a standstill.
	{
		float a6 = 0.0f, a7 = 0.0f;
		uint8 gear = 1;
		float velocity = 0.0f;
		float first = t.CalculateDriveAcceleration(1.0f, gear, velocity, &a6, &a7, 4, false);
		float undamped = 0.004f * 4.0f * 0.4f * ts;
		CHECK(first < undamped, "inertia did not damp the first frame (%.6f vs %.6f)", first, undamped);
		CHECK(a7 > 0.0f && a7 <= 1.0f, "a7 out of range: %f", a7);

		// Let it settle at a fixed velocity and the multiplier must climb toward
		// the undamped value.
		float prev = first;
		for(int i = 0; i < 60; i++){
			float a = t.CalculateDriveAcceleration(1.0f, gear, velocity, &a6, &a7, 4, false);
			CHECK(a <= undamped + 1e-6f, "accelerated past the undamped value");
			prev = a;
		}
		CHECK(prev > first, "acceleration did not rise as the gear settled");
	}

	// 5. No wheels on the ground: the engine spins up freely and the multiplier
	// is pinned to 0.1.
	{
		float a6 = 0.0f, a7 = 0.5f;
		uint8 gear = 1;
		float velocity = 0.0f;
		t.CalculateDriveAcceleration(1.0f, gear, velocity, &a6, &a7, 0, false);
		CHECKF(a7, 0.1f, "airborne a7");
		CHECK(a6 > 0.0f, "airborne a6 did not grow");
	}

	// 6. At the top of the gearbox the engine still pulls (the top gear's own
	// force), and above fMaxVelocity there is nothing left to give.
	{
		uint8 gear = 5;
		float velocity = t.fMaxVelocity;
		float a = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(a, 0.004f * 1.0f * 0.4f * ts, "force at the gearbox top is not the top gear's");

		gear = 5;
		velocity = t.fMaxVelocity + 0.025f;
		float over = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(over, 0.0f, "force above fMaxVelocity");

		// Below the reverse limit the engine is cut as well.
		gear = 0;
		velocity = t.fMaxReverseVelocity - 0.01f;
		float rev = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		CHECKF(rev, 0.0f, "force below the reverse limit");
	}

	// 7. Nitros doubles the drive force.
	{
		uint8 gear = 1;
		float velocity = 0.0f;
		float plain = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, false);
		gear = 1;
		float nitro = t.CalculateDriveAcceleration(1.0f, gear, velocity, nil, nil, 4, true);
		CHECKF(nitro, 2.0f * plain, "nitros multiplier");
	}
}

int
main(int argc, char **argv)
{
	TestFormatDetails();
	TestColumnMap();
	TestLookup();

	const char *path = argc > 1 ? argv[1] : "gamefiles/data/handling.cfg";
	FILE *f = fopen(path, "rb");
	if(f == nil){
		printf("cannot open %s (run from the repository root, or pass the path)\n", path);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	gFileSize = (int32)ftell(f);
	fseek(f, 0, SEEK_SET);
	gFileData = (uint8*)malloc(gFileSize);
	if(fread(gFileData, 1, gFileSize, f) != (size_t)gFileSize){ printf("short read\n"); return 2; }
	fclose(f);

	TestShippedFile();
	TestDriveAcceleration();

	printf("\n%s (%d problems)\n", gProblems == 0 ? "PASS" : "FAIL", gProblems);
	return gProblems == 0 ? 0 : 1;
}
