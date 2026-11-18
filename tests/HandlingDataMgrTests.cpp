// Tests of the SA handling.cfg parser and its unit conversion. The real
// cHandlingDataMgr.cpp is compiled against a file-system stub, so the numbers
// below are the ones ConvertDataToGameUnits() must produce from file units.
#include "common.h"
#include "cHandlingDataMgr.h"
#include "Timer.h"
#include "FileMgr.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>
#include <string>

// The parser is fed through the same entry point the game uses.
uint32 ParseHandlingFile(const uint8 *data, uint32 size);

// --- engine stubs --------------------------------------------------------
// The parser is linked without the rest of the engine, so the two services it
// borrows (logging and the file system) are stubbed here.
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}

float CTimer::ms_fTimeStep = 1.0f;
static std::vector<uint8> gHandlingFile;
bool gHandlingFileMissing;

void CFileMgr::SetDir(const char *) {}
int CFileMgr::OpenFile(const char *, const char *) { return -1; }
int CFileMgr::OpenFileForWriting(const char *) { return -1; }
size_t CFileMgr::Read(int, char *, ssize_t) { return 0; }
size_t CFileMgr::Write(int, const char *, ssize_t) { return 0; }
bool CFileMgr::Seek(int, int, int) { return false; }
bool CFileMgr::ReadLine(int, char *, int) { return false; }
int CFileMgr::CloseFile(int) { return 0; }
int CFileMgr::GetErrorReadWrite(int) { return 0; }
void CFileMgr::ChangeDir(const char *) {}
ssize_t CFileMgr::LoadFile(const char *file, uint8 *buf, int maxlen, const char *)
{
	if(gHandlingFileMissing || !gHandlingFile.size())
		return -1;
	ssize_t n = (ssize_t)gHandlingFile.size();
	if(n > maxlen)
		return -2;
	memcpy(buf, gHandlingFile.data(), (size_t)n);
	return n;
}

static void Check(bool ok, const char *why)
{
	if(!ok){
		std::fprintf(stderr, "cHandlingDataMgrTests: %s\n", why);
		std::abort();
	}
}

static void Near(float a, float b, const char *why)
{
	if(!(std::fabs(a - b) < 0.000001f)){
		std::fprintf(stderr, "cHandlingDataMgrTests: %s (got %.8f, want %.8f)\n", why, a, b);
		std::abort();
	}
}

// A complete SA vehicle line: name + 35 columns.
//   mass turnMass drag centreOfMass(3) submerged tractionMult tractionLoss
//   tractionBias gears maxVel engineAccel inertia driveType engineType
//   brakeDecel brakeBias abs steeringLock suspForce suspDamping suspHighSpd
//   suspUpper suspLower suspBias antidive seatOffset collisionDamage value
//   modelFlags handlingFlags frontLights rearLights animGroup
static const char *kSentinelLine =
	"SENTINEL\t1400.0\t2725.0\t0.0\t0.0\t0.25\t-0.1\t70\t0.75\t1.6\t1.0\t4\t"
	"200.0\t10.0\t20.0\tR\tP\t10.0\t0.6\t1\t35.0\t1.2\t0.1\t0.0\t0.25\t-0.15\t0.5\t1.5\n";

// Sourcing the same line with an explicit 36-column form used by the game data.
static std::string VehicleLine(const char *name, const char *body)
{
	std::string s = name;
	s += "\t";
	s += body;
	s += "\n";
	return s;
}

static const char *kVehicleBody =
	"1400.0\t2725.0\t0.0\t0.0\t0.25\t-0.1\t70\t0.75\t1.6\t1.0\t4\t"
	"200.0\t10.0\t20.0\tR\tP\t10.0\t0.6\t1\t35.0\t1.2\t0.1\t0.0\t0.25\t-0.15\t0.5\t1.5\t"
	"10.0\t1.0\t20000\t0x0\t0x0\t0\t0\t1";

static uint32 Parse(const std::string &text)
{
	return ParseHandlingFile((const uint8 *)text.data(), (uint32)text.size());
}

static void TestVehicleRecord(void)
{
	std::string text = VehicleLine("SENTINEL", kVehicleBody);
	Check(Parse(text) == 1, "one vehicle record parsed");

	tHandlingData *h = mod_HandlingManager.GetVehiclePointer(HANDLING_SENTINEL);
	Near(h->m_fMass, 1400.0f, "mass");
	Near(h->m_fMassRecip, 1.0f / 1400.0f, "reciprocal mass derived on load");
	Near(h->m_fTurnMass, 2725.0f, "turn mass");
	Near(h->m_fDragMult, 0.0f, "drag multiplier");
	Near(h->m_vecCentreOfMass.y, 0.25f, "centre of mass");
	Check(h->m_nPercentSubmerged == 70, "percent submerged");

	// tractionMultiplier keeps its file value: SA does not rescale it.
	Near(h->m_fTractionMultiplier, 0.75f, "traction multiplier");

	cTransmission &t = h->m_Transmission;
	Check(t.m_nNumberOfGears == 4, "gear count");
	Check(t.m_nDriveType == 'R', "drive type");
	Check(t.m_nEngineType == 'P', "engine type");
	Near(t.m_fEngineInertia, 20.0f, "engine inertia keeps its file value");

	// File units -> game units (the two constants the reference applies).
	// The values below are derived from the line above, not from the code:
	//   200 km/h            -> 200 * 0.277778/50 = 1.111112 m/frame
	//   10 m/s^2            -> 10 / 2500        = 0.004 m/frame^2
	// drag 0.0 takes the linear branch, whose first step satisfies the limit,
	// so the solved speed is 1.111112 - 0.01 = 1.101112 and SA then allows a
	// 1.2x overspeed on top of it.
	Near(h->m_fBrakeDeceleration, 10.0f / 2500.0f, "brake deceleration converted");
	Near(t.m_fEngineAcceleration, 0.004f / 2.0f, "acceleration converted and split over the drive axle");
	Near(t.m_fMaxFlatVelocity, 1.101112f, "solved flat speed");
	Near(t.m_fMaxVelocity, 1.101112f * 1.2f, "top speed includes the overspeed allowance");
	Near(t.m_fMaxReverseVelocity, -1.101112f * 0.3f, "reverse speed is a fraction of the flat speed");
	Near(t.m_aGears[4].m_MaxVelocity, t.m_fMaxVelocity, "gear ratios are initialised on load");

	// collisionDamageMultiplier is normalised back to a mass independent number.
	Near(h->m_fCollisionDamageMultiplier, 1.0f * (1.0f / 1400.0f) * 2000.0f,
		"collision damage multiplier normalised by mass");
	Check(h->m_fBuoyancyConstant > 0.0f, "buoyancy constant derived");

	Check(h->m_nFrontLights == LIGHTS_LONG, "front lights");
	Check(h->m_nRearLights == LIGHTS_LONG, "rear lights");
	Check(h->m_nAnimGroup == 1, "animation group");
}

static void TestHandlingFlags(void)
{
	std::string body =
		"1400.0\t2725.0\t0.0\t0.0\t0.25\t-0.1\t70\t0.75\t1.6\t1.0\t4\t"
		"200.0\t10.0\t20.0\tR\tP\t10.0\t0.6\t1\t35.0\t1.2\t0.1\t0.0\t0.25\t-0.15\t0.5\t1.5\t"
		"10.0\t1.0\t20000\t0x1000000\t0x1\t0\t0\t1";
	Check(Parse(VehicleLine("SENTINEL", body.c_str())) == 1, "flags record parsed");

	tHandlingData *h = mod_HandlingManager.GetVehiclePointer(HANDLING_SENTINEL);
	Check(h->m_bIsBike, "model flag bit reaches the bitfield view");
	Check(h->m_b1gBoost, "handling flag bit reaches the bitfield view");
	// The transmission reads the same word through its own Flags member.
	Check(h->m_Transmission.m_nHandlingFlags & VEHICLE_HANDLING_1G_BOOST,
		"handling flags are mirrored into the transmission");
}

static void TestAuxiliaryRecords(void)
{
	std::string text;
	text += "!BIKE\t1.0\t2.0\t3.0\t4.0\t5.0\t6.0\t7.0\t8.0\t9.0\t10.0\t11.0\t12.0\t13.0\t14.0\t15.0\n";
	text += "$SEAPLANE\t1.0\t2.0\t3.0\t4.0\t5.0\t6.0\t7.0\t8.0\t9.0\t10.0\t11.0\t12.0\t13.0\t14.0\t15.0\t16.0\t17.0\t18.0\t19.0\t20.0\t21.0\n";
	text += "%PREDATOR\t1.0\t2.0\t3.0\t4.0\t5.0\t6.0\t7.0\t8.0\t9.0\t10.0\t11.0\t12.0\t13.0\t14.0\n";
	Check(Parse(text) == 3, "bike, flying and boat records parsed");

	tBikeHandlingData *b = mod_HandlingManager.GetBikeHandlingPointer(HANDLING_BIKE);
	Near(b->m_fLeanFwdCOM, 1.0f, "bike first column");
	Near(b->m_fStoppieStabMult, 15.0f, "bike last column");
	// Angles are converted from degrees to sines/radians on load.
	Check(b->m_fMaxLean < 1.0f, "bike lean angle converted to a sine");

	tFlyingHandlingData *f = mod_HandlingManager.GetFlyingPointer(HANDLING_SEAPLANE);
	Near(f->m_fThrust, 1.0f, "flying first column");
	Near(f->m_vecSpeedRes.z, 21.0f, "flying last column");

	tBoatHandlingData *bo = mod_HandlingManager.GetBoatPointer(HANDLING_PREDATOR);
	Near(bo->m_fThrustY, 1.0f, "boat first column");
	Near(bo->m_fLook_L_R_BehindCamHeight, 14.0f, "boat last column");
}

static void TestMalformedRecords(void)
{
	cHandlingDataMgr before = mod_HandlingManager;

	// Too few columns.
	Check(Parse("SENTINEL\t1400.0\t2725.0\n") == 0, "short record is rejected");
	// Non numeric column.
	Check(Parse(VehicleLine("SENTINEL", "1400.0\tabc\t0.0\t0.0\t0.25\t-0.1\t70\t0.75\t1.6\t1.0\t4\t200.0\t10.0\t20.0\tR\tP\t10.0\t0.6\t1\t35.0\t1.2\t0.1\t0.0\t0.25\t-0.15\t0.5\t1.5\t10.0\t1.0\t20000\t0\t0\t0\t0\t1")) == 0,
		"non numeric column is rejected");
	// Gear count outside the transmission array.
	Check(Parse(VehicleLine("SENTINEL", "1400.0\t2725.0\t0.0\t0.0\t0.25\t-0.1\t70\t0.75\t1.6\t1.0\t9\t200.0\t10.0\t20.0\tR\tP\t10.0\t0.6\t1\t35.0\t1.2\t0.1\t0.0\t0.25\t-0.15\t0.5\t1.5\t10.0\t1.0\t20000\t0\t0\t0\t0\t1")) == 0,
		"out of range gear count is rejected");
	// Unknown name.
	Check(Parse(VehicleLine("NOTACAR", kVehicleBody)) == 0, "unknown handling name is rejected");

	// A rejected record must not have been half applied.
	Check(memcmp(&before.HandlingData[HANDLING_SENTINEL], mod_HandlingManager.GetVehiclePointer(HANDLING_SENTINEL),
		sizeof(tHandlingData)) == 0, "a rejected record leaves the table untouched");
}

static void TestTerminatorAndComments(void)
{
	std::string text;
	text += ";a comment\n";
	text += VehicleLine("SENTINEL", kVehicleBody);
	text += "^STDCARAMIMS\t1\t2\n";
	text += ";the end\n";
	text += VehicleLine("BRAVURA", kVehicleBody);
	Check(Parse(text) == 1, "comments, animation groups and the terminator are skipped");
}

static void TestNameLookup(void)
{
	Check(mod_HandlingManager.GetHandlingId("SENTINEL") == HANDLING_SENTINEL, "handling id by name");
	Check(mod_HandlingManager.GetHandlingId("BIKE") == HANDLING_BIKE, "bike handling id");
	Check(mod_HandlingManager.GetHandlingId("PREDATOR") == HANDLING_PREDATOR, "first boat id");
	Check(mod_HandlingManager.GetHandlingId("SEAPLANE") == HANDLING_SEAPLANE, "first flying id");
	Check(mod_HandlingManager.GetHandlingId("RCRAIDER") == HANDLING_RCRAIDER, "last handling id");
	Check(mod_HandlingManager.GetHandlingId("NOTACAR") == -1, "unknown names are reported");
	Check(NUM_HANDLINGS == 210, "the SA handling table has 210 entries");
}

static void TestPointerBounds(void)
{
	// Every accessor has to answer for an id outside its range without reading
	// out of bounds: this is what a corrupt model index would do.
	Check(mod_HandlingManager.GetVehiclePointer(0xFFFF) == &mod_HandlingManager.HandlingData[0],
		"out of range vehicle id clamps");
	Check(mod_HandlingManager.GetBikeHandlingPointer(0) == &mod_HandlingManager.BikeHandlingData[0],
		"out of range bike id clamps");
	Check(mod_HandlingManager.GetFlyingPointer(0) == &mod_HandlingManager.FlyingHandlingData[0],
		"out of range flying id clamps");
	Check(mod_HandlingManager.GetBoatPointer(0) == &mod_HandlingManager.BoatHandlingData[0],
		"out of range boat id clamps");

	// Drive type lookups follow the transmission's own column.
	tHandlingData *h = mod_HandlingManager.GetVehiclePointer(HANDLING_SENTINEL);
	h->m_Transmission.m_nDriveType = 'F';
	Check(mod_HandlingManager.HasFrontWheelDrive(HANDLING_SENTINEL), "front drive detected");
	Check(!mod_HandlingManager.HasRearWheelDrive(HANDLING_SENTINEL), "front drive is not rear drive");
	h->m_Transmission.m_nDriveType = '4';
	Check(mod_HandlingManager.HasFrontWheelDrive(HANDLING_SENTINEL) && mod_HandlingManager.HasRearWheelDrive(HANDLING_SENTINEL),
		"four wheel drive is both");
}

// --- Vice City data set ---------------------------------------------------
// reVC still ships the Vice City data, whose handling.cfg is the VC table: 33
// columns, no turnMass, and names the SA table does not know. Before the VC
// path existed every one of those lines was rejected and the vehicles ran on a
// zeroed handling record.
static const char *kVcAdmiral =
	"ADMIRAL\t1900.0\t1.9\t1.9\t0.7\t0.0\t0.0\t0.1\t70\t0.75\t0.65\t0.5\t5\t"
	"165.0\t26.0\tR\tP\t8.5\t0.55\t0\t30.0\t1.0\t1.0\t0.25\t2.0\t25000\t"
	"0.3\t-0.15\t0.5\t0.3\t0x0\t2\t1\n";

static void TestViceCityVehicleRecord(void)
{
	Check(Parse(kVcAdmiral) == 1, "VC vehicle record parsed");

	tHandlingData *h = mod_HandlingManager.GetVehiclePointer(HANDLING_ADMIRAL);
	Near(h->m_fMass, 1900.0f, "VC mass");
	Near(h->m_fMassRecip, 1.0f / 1900.0f, "VC reciprocal mass");
	// Derived from the collision box, which is what VC stores instead of a
	// turn mass column: (1.9^2 + 1.9^2) * 1900 / 12.
	Near(h->m_fTurnMass, (1.9f * 1.9f + 1.9f * 1.9f) * 1900.0f / 12.0f,
		"VC turn mass derived from the collision box");
	Check(h->m_nPercentSubmerged == 70, "VC percent submerged");
	Near(h->m_fTractionMultiplier, 0.75f, "VC traction multiplier");
	Near(h->m_fSeatOffsetDistance, 0.25f, "VC seat offset");
	Near(h->m_fSuspensionUpperLimit, 0.3f, "VC suspension upper limit");
	Check(h->m_nMonetaryValue == 25000, "VC monetary value");

	cTransmission &t = h->m_Transmission;
	Check(t.m_nNumberOfGears == 5, "VC gear count");
	Check(t.m_nDriveType == 'R', "VC drive type");
	// VC has no inertia column; the VC path supplies the documented default.
	Near(t.m_fEngineInertia, 15.0f, "VC engine inertia default");

	// 165 km/h -> 165 * 0.277778/50 = 0.916667 m/frame. The VC drag solve then
	// lowers the cruise speed; the player's limit is 1.2x that.
	Check(t.m_fMaxCruiseVelocity > 0.0f && t.m_fMaxCruiseVelocity < 0.916667f,
		"VC cruise speed is solved below the nominal top speed");
	Near(t.m_fMaxVelocity, t.m_fMaxCruiseVelocity * 1.2f, "VC top speed is 1.2x cruise");
	Near(t.m_fMaxReverseVelocity, -0.2f, "VC reverse speed is the fixed allowance");
	// 26 * 0.4 (VC pre-scale) / 2500 / 2 (rear wheel drive).
	Near(t.m_fEngineAcceleration, 0.00208f, "VC engine acceleration");
	Check(t.m_aGears[1].m_MaxVelocity > 0.0f, "VC gear ratios are initialised");
}

static void TestViceCityAuxiliaryRecords(void)
{
	// VC names that do not exist in the SA table still resolve.
	Check(Parse("IDAHO\t1700.0\t1.8\t1.8\t0.7\t0.0\t0.0\t0.1\t70\t0.75\t0.65\t0.5\t5\t"
	            "160.0\t25.0\tR\tP\t8.5\t0.55\t0\t30.0\t1.0\t1.0\t0.25\t2.0\t20000\t"
	            "0.3\t-0.15\t0.5\t0.3\t0x0\t2\t1\n") == 1,
		"VC-only handling name resolves");
	Near(mod_HandlingManager.GetVehiclePointer(HANDLING_SENTINEL)->m_fMass, 1700.0f,
		"VC-only name lands on the mapped SA record");

	// The VC flying line has no gearUpR/gearDownL/windMult columns.
	Check(Parse("$SEAPLANE\t0.6\t0.4\t0.4\t0.3\t0.1\t0.4\t0.3\t0.6\t0.3\t0.7\t0.6\t0.5\t"
	            "0.9\t0.9\t0.9\t0.5\t0.5\t0.5\n") == 1, "VC flying record parsed");
	tFlyingHandlingData *f = mod_HandlingManager.GetFlyingPointer(HANDLING_SEAPLANE);
	Near(f->m_fThrust, 0.6f, "VC flying thrust");
	Near(f->m_vecSpeedRes.z, 0.5f, "VC flying speed resistance");

	// A malformed VC line must not be half applied either.
	cHandlingDataMgr before = mod_HandlingManager;
	Check(Parse("ADMIRAL\t1900.0\tabc\t1.9\n") == 0, "malformed VC record is rejected");
	Check(memcmp(&before.HandlingData[HANDLING_ADMIRAL],
		mod_HandlingManager.GetVehiclePointer(HANDLING_ADMIRAL), sizeof(tHandlingData)) == 0,
		"a rejected VC record leaves the table untouched");
	// A VC record whose gear count is out of range is rejected rather than
	// clamped, and the length alone must not make it acceptable.
	Check(Parse("ADMIRAL\t1900.0\t1.9\t1.9\t0.7\t0.0\t0.0\t0.1\t70\t0.75\t0.65\t0.5\t9\t"
	            "165.0\t26.0\tR\tP\t8.5\t0.55\t0\t30.0\t1.0\t1.0\t0.25\t2.0\t25000\t"
	            "0.3\t-0.15\t0.5\t0.3\t0x0\t2\t1\n") == 0,
		"out of range VC gear count is rejected");
}

int main(void)
{
	TestVehicleRecord();
	TestHandlingFlags();
	TestAuxiliaryRecords();
	TestMalformedRecords();
	TestTerminatorAndComments();
	TestNameLookup();
	TestPointerBounds();
	TestViceCityVehicleRecord();
	TestViceCityAuxiliaryRecords();
	std::puts("cHandlingDataMgrTests: ALL PASSED");
	return 0;
}
