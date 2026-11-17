#include "common.h"

#include "main.h"
#include "FileMgr.h"
#include "Physical.h"
#include "HandlingMgr.h"
#include "SAHandlingData.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif
#include <algorithm>

cHandlingDataMgr mod_HandlingManager;

const char *HandlingFilename = "HANDLING.CFG";

const char VehicleNames[NUMHANDLINGS][14] = {
	"LANDSTAL",
	"IDAHO",
	"STINGER",
	"LINERUN",
	"PEREN",
	"SENTINEL",
	"PATRIOT",
	"FIRETRUK",
	"TRASH",
	"STRETCH",
	"MANANA",
	"INFERNUS",
	"PONY",
	"MULE",
	"CHEETAH",
	"AMBULAN",
	"FBICAR",
	"MOONBEAM",
	"ESPERANT",
	"TAXI",
	"KURUMA",
	"BOBCAT",
	"MRWHOOP",
	"BFINJECT",
	"POLICE",
	"ENFORCER",
	"SECURICA",
	"BANSHEE",
	"BUS",
	"RHINO",
	"BARRACKS",
	"TRAIN",
	"HELI",
	"DODO",
	"COACH",
	"CABBIE",
	"STALLION",
	"RUMPO",
	"RCBANDIT",
	"MAFIA",
	"AIRTRAIN",
	"DEADDODO",
	"FLATBED",
	"YANKEE",
	"GOLFCART",
	"VOODOO",
	"WASHING",
	"CUBAN",
	"ROMERO",
	"PACKER",
	"ADMIRAL",
	"GANGBUR",
	"ZEBRA",
	"TOPFUN",
	"GLENDALE",
	"OCEANIC",
	"HERMES",
	"SABRE1",
	"SABRETUR",
	"PHEONIX",
	"WALTON",
	"REGINA",
	"COMET",
	"DELUXO",
	"BURRITO",
	"SPAND",
	"BAGGAGE",
	"KAUFMAN",
	"RANCHER",
	"FBIRANCH",
	"VIRGO",
	"GREENWOO",
	"HOTRING",
	"SANDKING",
	"BLISTAC",
	"BOXVILLE",
	"BENSON",
	"DESPERAD",
	"LOVEFIST",
	"BLOODRA",
	"BLOODRB",
	"BIKE",
	"MOPED",
	"DIRTBIKE",
	"ANGEL",
	"FREEWAY",
	"PREDATOR",
	"SPEEDER",
	"REEFER",
	"RIO",
	"SQUALO",
	"TROPIC",
	"COASTGRD",
	"DINGHY",
	"MARQUIS",
	"CUPBOAT",
	"SEAPLANE",
	"SPARROW",
	"SEASPAR",
	"MAVERICK",
	"COASTMAV",
	"POLMAV",
	"HUNTER",
	"RCBARON",
	"RCGOBLIN",
	"RCCOPTER"
};

cHandlingDataMgr::cHandlingDataMgr(void)
{
	field_0 = fWheelFriction = field_8 = field_C = field_10 = 0.0f;
	for(auto &entry : HandlingData) entry = tHandlingData{};
	for(auto &entry : BikeHandlingData) entry = tBikeHandlingData{};
	for(auto &entry : BoatHandlingData) entry = tBoatHandlingData{};
	for(auto &entry : FlyingHandlingData) entry = tFlyingHandlingData{};
}

void
cHandlingDataMgr::Initialise(void)
{
	LoadHandlingData();
#ifdef CUSTOM_MODELS
	std::vector<uint8> saBytes;
	std::string error;
	if(CCustomModels::ReadDataFile("data/handling.cfg", saBytes)){
		if(!LoadSAHandlingData(saBytes.data(), saBytes.size(), &error))
			REVC_LOG_WARNING("SA handling not installed: %s", error.c_str());
	}
#endif
	field_0 = 0.1f;
	fWheelFriction = 0.9f;
	field_8 = 1.0f;
	field_C = 0.8f;
	field_10 = 0.98f;
}

void
cHandlingDataMgr::LoadHandlingData(void)
{
	char *start, *end;
	char line[201];	// weird value
	char delim[4];	// not sure
	char *word;
	int field, handlingId;
	int keepGoing;
	tHandlingData *handling;
	tFlyingHandlingData *flyingHandling;
	tBoatHandlingData *boatHandling;
	tBikeHandlingData *bikeHandling;

	CFileMgr::SetDir("DATA");
	CFileMgr::LoadFile(HandlingFilename, work_buff, sizeof(work_buff), "r");
	CFileMgr::SetDir("");

	start = (char*)work_buff;
	end = start+1;
	handling = nil;
	flyingHandling = nil;
	boatHandling = nil;
	bikeHandling = nil;
	keepGoing = 1;

	while(keepGoing){
		// find end of line
		while(*end != '\n') end++;

		// get line
		int32 lineLength = end - start;
#ifdef GTA_PS2
		// CRs are left untouched. we assume every line ends in one
		lineLength--;
#endif
		strncpy(line, start, lineLength);
		line[lineLength] = '\0';
		start = end+1;
		end = start+1;

		// yeah, this is kinda crappy
		if(strcmp(line, ";the end") == 0)
			keepGoing = 0;
		else if(line[0] != ';'){
			if(line[0] == '!'){
				// Bike data
				field = 0;
				strcpy(delim, " \t");
				// FIX: game seems to use a do-while loop here
				for(word = strtok(line, delim); word; word = strtok(nil, delim)){
					switch(field){
					case  0: break;
					case  1:
						handlingId = FindExactWord(word, (const char*)VehicleNames, 14, NUMHANDLINGS);
						assert(handlingId >= 0 && handlingId < NUMHANDLINGS);
						bikeHandling = GetBikePointer(handlingId);
						bikeHandling->nIdentifier = (tVehicleType)handlingId;
						break;
					case  2: bikeHandling->fLeanFwdCOM = atof(word); break;
					case  3: bikeHandling->fLeanFwdForce = atof(word); break;
					case  4: bikeHandling->fLeanBakCOM = atof(word); break;
					case  5: bikeHandling->fLeanBackForce = atof(word); break;
					case  6: bikeHandling->fMaxLean = atof(word); break;
					case  7: bikeHandling->fFullAnimLean = atof(word); break;
					case  8: bikeHandling->fDesLean = atof(word); break;
					case  9: bikeHandling->fSpeedSteer = atof(word); break;
					case 10: bikeHandling->fSlipSteer = atof(word); break;
					case 11: bikeHandling->fNoPlayerCOMz = atof(word); break;
					case 12: bikeHandling->fWheelieAng = atof(word); break;
					case 13: bikeHandling->fStoppieAng = atof(word); break;
					case 14: bikeHandling->fWheelieSteer = atof(word); break;
					case 15: bikeHandling->fWheelieStabMult = atof(word); break;
					case 16: bikeHandling->fStoppieStabMult = atof(word); break;
					}
					field++;
				}
				ConvertBikeDataToGameUnits(bikeHandling);
			}else if(line[0] == '$'){
				// Flying data
				field = 0;
				strcpy(delim, " \t");
				// FIX: game seems to use a do-while loop here
				for(word = strtok(line, delim); word; word = strtok(nil, delim)){
					switch(field){
					case  0: break;
					case  1:
						handlingId = FindExactWord(word, (const char*)VehicleNames, 14, NUMHANDLINGS);
						assert(handlingId >= 0 && handlingId < NUMHANDLINGS);
						flyingHandling = GetFlyingPointer(handlingId);
						flyingHandling->nIdentifier = (tVehicleType)handlingId;
						break;
					case  2: flyingHandling->fThrust = atof(word); break;
					case  3: flyingHandling->fThrustFallOff = atof(word); break;
					case  4: flyingHandling->fYaw = atof(word); break;
					case  5: flyingHandling->fYawStab = atof(word); break;
					case  6: flyingHandling->fSideSlip = atof(word); break;
					case  7: flyingHandling->fRoll = atof(word); break;
					case  8: flyingHandling->fRollStab = atof(word); break;
					case  9: flyingHandling->fPitch = atof(word); break;
					case 10: flyingHandling->fPitchStab = atof(word); break;
					case 11: flyingHandling->fFormLift = atof(word); break;
					case 12: flyingHandling->fAttackLift = atof(word); break;
					case 13: flyingHandling->fMoveRes = atof(word); break;
					case 14: flyingHandling->vecTurnRes.x = atof(word); break;
					case 15: flyingHandling->vecTurnRes.y = atof(word); break;
					case 16: flyingHandling->vecTurnRes.z = atof(word); break;
					case 17: flyingHandling->vecSpeedRes.x = atof(word); break;
					case 18: flyingHandling->vecSpeedRes.y = atof(word); break;
					case 19: flyingHandling->vecSpeedRes.z = atof(word); break;
					}
					field++;
				}
			}else if(line[0] == '%'){
				// Boat data
				field = 0;
				strcpy(delim, " \t");
				// FIX: game seems to use a do-while loop here
				for(word = strtok(line, delim); word; word = strtok(nil, delim)){
					switch(field){
					case  0: break;
					case  1:
						handlingId = FindExactWord(word, (const char*)VehicleNames, 14, NUMHANDLINGS);
						assert(handlingId >= 0 && handlingId < NUMHANDLINGS);
						boatHandling = GetBoatPointer(handlingId);
						boatHandling->nIdentifier = (tVehicleType)handlingId;
						break;
					case  2: boatHandling->fThrustY = atof(word); break;
					case  3: boatHandling->fThrustZ = atof(word); break;
					case  4: boatHandling->fThrustAppZ = atof(word); break;
					case  5: boatHandling->fAqPlaneForce = atof(word); break;
					case  6: boatHandling->fAqPlaneLimit = atof(word); break;
					case  7: boatHandling->fAqPlaneOffset = atof(word); break;
					case  8: boatHandling->fWaveAudioMult = atof(word); break;
					case  9: boatHandling->vecMoveRes.x = atof(word); break;
					case 10: boatHandling->vecMoveRes.y = atof(word); break;
					case 11: boatHandling->vecMoveRes.z = atof(word); break;
					case 12: boatHandling->vecTurnRes.x = atof(word); break;
					case 13: boatHandling->vecTurnRes.y = atof(word); break;
					case 14: boatHandling->vecTurnRes.z = atof(word); break;
					case 15: boatHandling->fLook_L_R_BehindCamHeight = atof(word); break;
					}
					field++;
				}
			}else{
				field = 0;
				strcpy(delim, " \t");
				// FIX: game seems to use a do-while loop here
				for(word = strtok(line, delim); word; word = strtok(nil, delim)){
					switch(field){
					case  0:
						handlingId = FindExactWord(word, (const char*)VehicleNames, 14, NUMHANDLINGS);
						assert(handlingId >= 0 && handlingId < NUMHANDLINGS);
						handling = &HandlingData[handlingId];
						handling->nIdentifier = (tVehicleType)handlingId;
						break;
					case  1: handling->fMass = atof(word); break;
					case  2: handling->Dimension.x = atof(word); break;
					case  3: handling->Dimension.y = atof(word); break;
					case  4: handling->Dimension.z = atof(word); break;
					case  5: handling->CentreOfMass.x = atof(word); break;
					case  6: handling->CentreOfMass.y = atof(word); break;
					case  7: handling->CentreOfMass.z = atof(word); break;
					case  8: handling->nPercentSubmerged = atoi(word); break;
					case  9: handling->fTractionMultiplier = atof(word); break;
					case 10: handling->fTractionLoss = atof(word); break;
					case 11: handling->fTractionBias = atof(word); break;
					case 12: handling->Transmission.nNumberOfGears = atoi(word); break;
					case 13: handling->Transmission.fMaxVelocity = atof(word); break;
					case 14: handling->Transmission.fEngineAcceleration = atof(word) * 0.4; break;
					case 15: handling->Transmission.nDriveType = word[0]; break;
					case 16: handling->Transmission.nEngineType = word[0]; break;
					case 17: handling->fBrakeDeceleration = atof(word); break;
					case 18: handling->fBrakeBias = atof(word); break;
					case 19: handling->bABS = !!atoi(word); break;
					case 20: handling->fSteeringLock = atof(word); break;
					case 21: handling->fSuspensionForceLevel = atof(word); break;
					case 22: handling->fSuspensionDampingLevel = atof(word); break;
					case 23: handling->fSeatOffsetDistance = atof(word); break;
					case 24: handling->fCollisionDamageMultiplier = atof(word); break;
					case 25: handling->nMonetaryValue = atoi(word); break;
					case 26: handling->fSuspensionUpperLimit = atof(word); break;
					case 27: handling->fSuspensionLowerLimit = atof(word); break;
					case 28: handling->fSuspensionBias = atof(word); break;
					case 29: handling->fSuspensionAntidiveMultiplier = atof(word); break;
					case 30:
						sscanf(word, "%x", &handling->Flags);
						handling->Transmission.Flags = handling->Flags;
						break;
					case 31: handling->FrontLights = atoi(word); break;
					case 32: handling->RearLights = atoi(word); break;
					}
					field++;
				}
				ConvertDataToGameUnits(handling);
			}
		}
	}
}

int
cHandlingDataMgr::FindExactWord(const char *word, const char *words, int wordLen, int numWords)
{
	int i;

	for(i = 0; i < numWords; i++){
		// BUG: the game does something really stupid here, it's fixed here
		if(strncmp(word, words, wordLen) == 0)
			return i;
		words += wordLen;
	}
	return numWords;
}


void
cHandlingDataMgr::ConvertDataToGameUnits(tHandlingData *handling)
{
	// convert distance to m, time to 1/50s
	float velocity, a, b;

	handling->Transmission.fEngineAcceleration *= 1.0f/(50.0f*50.0f);
	handling->Transmission.fMaxVelocity *= 1000.0f/(60.0f*60.0f * 50.0f);
	handling->fBrakeDeceleration *= 1.0f/(50.0f*50.0f);
	handling->fTurnMass = (sq(handling->Dimension.x) + sq(handling->Dimension.y)) * handling->fMass / 12.0f;
	if(handling->fTurnMass < 10.0f)
		handling->fTurnMass *= 5.0f;
	handling->fInvMass = 1.0f/handling->fMass;
	handling->fCollisionDamageMultiplier *= 2000.0f/handling->fMass;
	handling->fBuoyancy = 100.0f/handling->nPercentSubmerged * GRAVITY*handling->fMass;

	// Don't quite understand this. What seems to be going on is that
	// we calculate a drag (air resistance) deceleration for a given velocity and
	// find the intersection between that and the max engine acceleration.
	// at that point the car cannot accelerate any further and we've found the max velocity.
	a = 0.0f;
	b = 100.0f;
	velocity = handling->Transmission.fMaxVelocity;
	while(a < b && velocity > 0.0f){
		velocity -= 0.01f;
		// what's the 1/6?
		a = handling->Transmission.fEngineAcceleration/6.0f;
		// no density or drag coefficient here...
		float a_drag = 0.5f*SQR(velocity) * handling->Dimension.x*handling->Dimension.z / handling->fMass;
		// can't make sense of this... maybe  v - v/(drag + 1)  ? but that doesn't make so much sense either
		b = -velocity * (1.0f/(a_drag + 1.0f) - 1.0f);
	}

	if(handling->nIdentifier == HANDLING_RCBANDIT){
		handling->Transmission.fMaxCruiseVelocity = handling->Transmission.fMaxVelocity;
		handling->Transmission.fMaxReverseVelocity = -handling->Transmission.fMaxVelocity;
	}else if(handling->nIdentifier >= HANDLING_BIKE && handling->nIdentifier <= HANDLING_FREEWAY){
		handling->Transmission.fMaxCruiseVelocity = velocity;
		handling->Transmission.fMaxVelocity = velocity * 1.2f;
		handling->Transmission.fMaxReverseVelocity = -0.05f;
	}else{
		handling->Transmission.fMaxCruiseVelocity = velocity;
		handling->Transmission.fMaxVelocity = velocity * 1.2f;
		handling->Transmission.fMaxReverseVelocity = -0.2f;
	}

	if(handling->Transmission.nDriveType == '4')
		handling->Transmission.fEngineAcceleration /= 4.0f;
	else
		handling->Transmission.fEngineAcceleration /= 2.0f;

	handling->Transmission.InitGearRatios();
}

void
cHandlingDataMgr::ConvertBikeDataToGameUnits(tBikeHandlingData *handling)
{
	handling->fMaxLean = Sin(DEGTORAD(handling->fMaxLean));
	handling->fFullAnimLean = DEGTORAD(handling->fFullAnimLean);
	handling->fWheelieAng = Sin(DEGTORAD(handling->fWheelieAng));
	handling->fStoppieAng = Sin(DEGTORAD(handling->fStoppieAng));
}

int32
cHandlingDataMgr::GetHandlingId(const char *name)
{
	if(!name) return -1;
	auto sa = m_saNames.find(name);
	if(sa != m_saNames.end()) return sa->second;
	int i;
	for(i = 0; i < NUMHANDLINGS; i++)
		if(strncmp(VehicleNames[i], name, 14) == 0)
			break;
	return i == NUMHANDLINGS && !m_saNames.empty() ? -1 : i;
}

tFlyingHandlingData*
cHandlingDataMgr::GetFlyingPointer(int32 id)
{
	auto sa = m_saFlying.find(id);
	if(sa != m_saFlying.end()) return &sa->second;
	if(id >= HANDLING_SEAPLANE && id <= HANDLING_RCCOPTER)
		return &FlyingHandlingData[id-HANDLING_SEAPLANE];
	return &FlyingHandlingData[0];
}

tBoatHandlingData*
cHandlingDataMgr::GetBoatPointer(int32 id)
{
	auto sa = m_saBoats.find(id);
	if(sa != m_saBoats.end()) return &sa->second;
	if(id >= HANDLING_PREDATOR && id <= HANDLING_SEAPLANE)
		return &BoatHandlingData[id-HANDLING_PREDATOR];
	return &BoatHandlingData[0];
}


tBikeHandlingData *
cHandlingDataMgr::GetBikePointer(int32 id)
{
	auto sa = m_saBikes.find(id);
	if(sa != m_saBikes.end()) return &sa->second;
	if(id >= HANDLING_BIKE && id <= HANDLING_FREEWAY)
		return &BikeHandlingData[id-HANDLING_BIKE];
	return &BikeHandlingData[0];
}

namespace {
// Transitional view for consumers not yet migrated to SA's TWO flag words.
// Never put the raw SA word in Flags: its bits have different meanings.
uint32 SACompatibilityFlags(uint32 model, uint32 handling)
{
	uint32 result = handling & 3;
	const uint32 modelBits[][2] = {
		{1u<<0,HANDLING_IS_VAN},{1u<<1,HANDLING_IS_BUS},{1u<<2,HANDLING_IS_LOW},
		{1u<<3,HANDLING_IS_BIG},{1u<<4,HANDLING_REV_BONNET},{1u<<5,HANDLING_HANGING_BOOT},
		{1u<<6,HANDLING_TAILGATE_BOOT},{1u<<7,HANDLING_NOSWING_BOOT},{1u<<8,HANDLING_NO_DOORS},
		{1u<<10,HANDLING_SIT_IN_BOAT},{1u<<11,HANDLING_HAS_NO_ROOF},{1u<<12,HANDLING_NO_EXHAUST},
		{1u<<13,HANDLING_DBL_EXHAUST},{1u<<24,HANDLING_IS_BIKE},{1u<<25,HANDLING_IS_HELI},
		{1u<<26,HANDLING_IS_PLANE},{1u<<27,HANDLING_IS_BOAT}
	};
	for(const auto &bit : modelBits) if(model & bit[0]) result |= bit[1];
	if(handling & (1u<<2)) result |= HANDLING_NONPLAYER_STABILISER;
	if(handling & (1u<<3)) result |= HANDLING_NEUTRALHANDLING;
	if(handling & (1u<<22)) result |= HANDLING_HALOGEN_LIGHTS;
	if(handling & (1u<<23)) result |= HANDLING_REARWHEEL_1ST;
	if(handling & ((1u<<20)|(1u<<21))) result |= HANDLING_GOOD_INSAND;
	return result;
}
void ConvertSAUnits(tHandlingData &h, bool isBike, bool rcBandit)
{
	// gta-reversed cHandlingDataMgr::ConvertDataToGameUnits, SA 0x6F5080.
	auto &t = h.Transmission;
	t.fEngineAcceleration *= 0.4f/(50.0f*50.0f);
	t.fMaxVelocity *= 0.277778f/50.0f;
	h.fBrakeDeceleration /= 50.0f*50.0f;
	h.fInvMass = 1.0f/h.fMass;
	h.fBuoyancy = h.fMass*0.8f/h.nPercentSubmerged;
	h.fCollisionDamageMultiplier *= h.fInvMass*2000.0f;
	const float limit = t.fEngineAcceleration/6.0f;
	float velocity = t.fMaxVelocity;
	while(velocity > 0.0f){
		velocity -= 0.01f;
		if(h.fDragMult >= 0.01f){
			if(velocity*velocity*(h.fDragMult/2.0f)/1000.0f <= limit) break;
		}else{
			const float reciprocal = 1.0f/(velocity*velocity*h.fDragMult+1.0f);
			if((reciprocal-1.0f)*-velocity <= limit) break;
		}
	}
	if(rcBandit){
		t.fMaxCruiseVelocity = velocity;
		t.fMaxReverseVelocity = -velocity;
	}else if(h.nHandlingFlags & (1u<<24)){
		t.fMaxCruiseVelocity = velocity/1.2f;
		t.fMaxReverseVelocity = std::min(-t.fMaxCruiseVelocity/4.0f,-0.2f);
	}else{
		t.fMaxVelocity = velocity*1.2f;
		t.fMaxCruiseVelocity = velocity;
		t.fMaxReverseVelocity = isBike ? -0.05f : std::min(-velocity*0.3f,-0.2f);
	}
	t.fEngineAcceleration /= t.nDriveType=='4' ? 4.0f : 2.0f;
	t.InitGearRatios();
}
}

bool
cHandlingDataMgr::LoadSAHandlingData(const uint8 *data, size_t size, std::string *error)
{
	sahandling::Table table;
	if(!sahandling::Parse(data,size,table,error)) return false;
	// Do not relocate data after vehicles have taken pointers into the registry.
	if(!m_saCars.empty()){
		if(error) *error = "SA handling registry is already installed; runtime reload is not supported";
		return false;
	}
	std::map<std::string,int32> names;
	std::map<int32,tHandlingData> cars;
	std::map<int32,tBikeHandlingData> bikes;
	std::map<int32,tBoatHandlingData> boats;
	std::map<int32,tFlyingHandlingData> flying;
	int32 next = NUMHANDLINGS;
	for(const auto &source : table.cars){
		int32 id = FindExactWord(source.name.c_str(),(const char*)VehicleNames,14,NUMHANDLINGS);
		if(id < 0 || id >= NUMHANDLINGS) id = next++;
		names[source.name] = id;
		tHandlingData h = {};
		h.nIdentifier = (tVehicleType)id;
		h.fMass=source.mass; h.fTurnMass=source.turnMass; h.fDragMult=source.drag;
		h.CentreOfMass=CVector(source.centre[0],source.centre[1],source.centre[2]);
		h.nPercentSubmerged=source.submerged;
		h.fTractionMultiplier=source.traction; h.fTractionLoss=source.tractionLoss; h.fTractionBias=source.tractionBias;
		auto &t=h.Transmission;
		t.nNumberOfGears=source.gears; t.nDriveType=source.drive; t.nEngineType=source.engine;
		t.fMaxVelocity=source.velocity; t.fEngineAcceleration=source.acceleration; t.fEngineInertia=source.inertia;
		t.Flags=source.handlingFlags;
		h.fBrakeDeceleration=source.brake; h.fBrakeBias=source.brakeBias; h.bABS=source.abs;
		h.fSteeringLock=source.steering; h.fSuspensionForceLevel=source.spring;
		h.fSuspensionDampingLevel=source.damping; h.fSuspensionHighSpdComDamp=source.highSpeedDamping;
		h.fSuspensionUpperLimit=source.upper; h.fSuspensionLowerLimit=source.lower;
		h.fSuspensionBias=source.suspensionBias; h.fSuspensionAntidiveMultiplier=source.antiDive;
		h.fSeatOffsetDistance=source.seatOffset; h.fCollisionDamageMultiplier=source.collisionDamage;
		h.nMonetaryValue=source.money; h.FrontLights=source.frontLights; h.RearLights=source.rearLights;
		h.nModelFlags=source.modelFlags; h.nHandlingFlags=source.handlingFlags; h.nAnimGroup=source.animGroup;
		h.Flags=SACompatibilityFlags(source.modelFlags,source.handlingFlags); h.bSAData=true;
		bool isBike=(source.modelFlags & (1u<<24))!=0;
		for(const auto &special : table.bikes) if(special.name==source.name) isBike=true;
		ConvertSAUnits(h,isBike,source.name=="RCBANDIT");
		cars[id]=h;
	}
	for(const auto &source : table.bikes){
		auto id=names.find(source.name); if(id==names.end()) continue;
		tBikeHandlingData h={}; h.nIdentifier=(tVehicleType)id->second;
		float *fields[]={&h.fLeanFwdCOM,&h.fLeanFwdForce,&h.fLeanBakCOM,&h.fLeanBackForce,
			&h.fMaxLean,&h.fFullAnimLean,&h.fDesLean,&h.fSpeedSteer,&h.fSlipSteer,
			&h.fNoPlayerCOMz,&h.fWheelieAng,&h.fStoppieAng,&h.fWheelieSteer,&h.fWheelieStabMult,&h.fStoppieStabMult};
		for(size_t i=0;i<15;++i) *fields[i]=source.values[i];
		ConvertBikeDataToGameUnits(&h); bikes[id->second]=h;
	}
	for(const auto &source : table.boats){
		auto id=names.find(source.name); if(id==names.end()) continue;
		tBoatHandlingData h={}; h.nIdentifier=(tVehicleType)id->second;
		float *fields[]={&h.fThrustY,&h.fThrustZ,&h.fThrustAppZ,&h.fAqPlaneForce,&h.fAqPlaneLimit,
			&h.fAqPlaneOffset,&h.fWaveAudioMult,&h.vecMoveRes.x,&h.vecMoveRes.y,&h.vecMoveRes.z,
			&h.vecTurnRes.x,&h.vecTurnRes.y,&h.vecTurnRes.z,&h.fLook_L_R_BehindCamHeight};
		for(size_t i=0;i<14;++i) *fields[i]=source.values[i];
		boats[id->second]=h;
	}
	for(const auto &source : table.flying){
		auto id=names.find(source.name); if(id==names.end()) continue;
		tFlyingHandlingData h={}; h.nIdentifier=(tVehicleType)id->second;
		float *fields[]={&h.fThrust,&h.fThrustFallOff,&h.fYaw,&h.fYawStab,&h.fSideSlip,&h.fRoll,&h.fRollStab,
			&h.fPitch,&h.fPitchStab,&h.fFormLift,&h.fAttackLift,&h.fGearUpR,&h.fGearDownL,&h.fWindMult,
			&h.fMoveRes,&h.vecTurnRes.x,&h.vecTurnRes.y,&h.vecTurnRes.z,&h.vecSpeedRes.x,&h.vecSpeedRes.y,&h.vecSpeedRes.z};
		for(size_t i=0;i<21;++i) *fields[i]=source.values[i];
		flying[id->second]=h;
	}
	m_saNames.swap(names); m_saCars.swap(cars); m_saBikes.swap(bikes);
	m_saBoats.swap(boats); m_saFlying.swap(flying);
	REVC_LOG_INFO("Installed SA handling: %u cars, %u bike, %u boat, %u flying records",
		(unsigned)m_saCars.size(),(unsigned)m_saBikes.size(),(unsigned)m_saBoats.size(),(unsigned)m_saFlying.size());
	return true;
}
