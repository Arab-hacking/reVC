#include "common.h"

#include "CarGen.h"

#include "Automobile.h"
#include "Bike.h"
#include "Boat.h"
#include "Camera.h"
#include "CarCtrl.h"
#include "Clock.h"
#include "General.h"
#include "Heli.h"
#include "Occlusion.h"
#include "Plane.h"
#include "Pools.h"
#include "Population.h"
#include "Replay.h"
#include "SaveBuf.h"
#include "Streaming.h"
#include "Timer.h"
#include "Train.h"
#include "Vehicle.h"
#include "VisibilityPlugins.h"
#include "World.h"
#include "Zones.h"

uint8 CTheCarGenerators::ProcessCounter;
uint32 CTheCarGenerators::NumOfCarGenerators;
CCarGenerator CTheCarGenerators::CarGeneratorArray[NUM_CAR_GENERATORS];
uint8 CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter;
CSpecialPlateHandler CTheCarGenerators::m_SpecialPlateHandler;

// SA stores the generator position as int16 at 1/8 unit precision (the same
// scale CompressedLargeVector uses), not as three floats.
void
CCarGenerator::SetPosn(CVector const &posn)
{
	m_nPosX = (int16)(posn.x * 8.0f);
	m_nPosY = (int16)(posn.y * 8.0f);
	m_nPosZ = (int16)(posn.z * 8.0f);
}

CVector
CCarGenerator::GetPosn(void) const
{
	return CVector(m_nPosX * 0.125f, m_nPosY * 0.125f, m_nPosZ * 0.125f);
}

// SA CCarGenerator::SwitchOff (0x6F2E30).
//
// SA did not keep a running count of active generators; the VC
// CurrentActiveCount bookkeeping is gone because the SA loop walks every slot
// and checks m_bIsUsed instead.
void
CCarGenerator::SwitchOff()
{
	m_nGenerateCount = 0;
}

// SA CCarGenerator::SwitchOn (0x6F32C0).
void
CCarGenerator::SwitchOn()
{
	m_nGenerateCount = (uint16)-1;
	m_nNextGenTime = CalcNextGen();
}

// SA CCarGenerator::CalcNextGen (0x6F2E40). SA ignores the min/max delay here
// and always retries in 4 ms; the delays are only used by the script opcodes.
uint32
CCarGenerator::CalcNextGen()
{
	return CTimer::GetTimeInMilliseconds() + 4;
}

// SA CCarGenerator::CheckForBlockage (0x6F32E0).
//
// Looks for anything standing where the car would appear. The z test in SA is
// asymmetric (entity max against our min, entity min against our max, each
// with 1 unit of slack) because the collision boxes are not aligned; the
// original code compares against the model's bounding box, and falls back to a
// +-1 unit slab when the model has no collision at all.
bool
CCarGenerator::CheckForBlockage(int32 modelId)
{
	CColModel *colModel = CModelInfo::GetColModel(modelId);
	float radius = colModel ? colModel->boundingSphere.radius : 2.0f;

	int16 entityCount;
	CEntity *objects[8];

	CVector posn = GetPosn();
	CWorld::FindObjectsKindaColliding(posn, radius, true, &entityCount, 8, objects, false, true, true, false, false);

	for(int32 i = 0; i < entityCount; i++){
		CEntity *obj = objects[i];
		CColModel *entityColModel = obj->GetColModel();
		CBox &entityBox = entityColModel->boundingBox;
		float objZ = obj->GetPosition().z;

		if(colModel){
			CBox &box = colModel->boundingBox;
			if(objZ + entityBox.max.z + 1.0f > posn.z + box.min.z &&
			   objZ + entityBox.min.z - 1.0f < posn.z + box.max.z){
				bWaitUntilFarFromPlayer = true;
				return true;
			}
		}else{
			if(objZ + entityBox.max.z + 1.0f > posn.z - 1.0f &&
			   objZ + entityBox.min.z - 1.0f < posn.z + 1.0f){
				bWaitUntilFarFromPlayer = true;
				return true;
			}
		}
	}

	return false;
}

// SA CCarGenerator::CheckIfWithinRangeOfAnyPlayers (0x6F2F40).
//
// The generator only fires when it is far enough away for the spawn not to be
// seen, high-priority generators skip the distance test, and the player must
// not be driving towards it (otherwise a car would pop into existence in front
// of the camera). Boats get the extra visibility test at double range.
bool
CCarGenerator::CheckIfWithinRangeOfAnyPlayers()
{
	bool bVisible = false;
	CVector relPosn = FindPlayerCentreOfWorld(CWorld::PlayerInFocus) - GetPosn();
	if(fabsf(relPosn.z) > 50.0f)
		return false;

	if(m_nModelId > 0 &&
	   CModelInfo::IsBoatModel(m_nModelId) &&
	   relPosn.Magnitude2D() < TheCamera.GenerationDistMultiplier * 240.0f)
	{
		CVector origin = GetPosn();
		if(TheCamera.IsSphereVisible(origin, 0.0f) && !COcclusion::IsPositionOccluded(origin, 0.0f))
			bVisible = true;
	}

	if(relPosn.Magnitude2D() >= TheCamera.GenerationDistMultiplier * 160.0f && !bVisible){
		bWaitUntilFarFromPlayer = false;
		return false;
	}

	if(CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter)
		return true;

	if(bWaitUntilFarFromPlayer)
		return false;

	float posnZ = GetPosn().z;
	if((CGame::CanSeeOutSideFromCurrArea() && posnZ < 950.0f || !CGame::CanSeeOutSideFromCurrArea() && posnZ >= 950.0f)
	   && (relPosn.Magnitude2D() >= TheCamera.GenerationDistMultiplier * 160.0f - 20.0f || bHighPriority)
	   && DotProduct2D(FindPlayerSpeed(), relPosn) <= 0.0f)
	{
		return true;
	}

	return false;
}

// SA CCarGenerator::DoInternalProcessing (0x6F34D0).
void
CCarGenerator::DoInternalProcessing()
{
	int32 actualModelId;
	CVehicle *vehicle = nil;
	float baseZ;
	CColPoint colPoint;
	CEntity *entity;
	char plateText[12];

	bool nightTime = CClock::GetIsTimeInRange(21, 7);
	if(!bIgnorePopulationLimit
	   && (nightTime && CCarCtrl::NumParkedCars >= 10 || !nightTime && CCarCtrl::NumParkedCars >= 5))
		return;

	if(m_nModelId >= 0){
		if(CheckForBlockage(m_nModelId)){
			m_nNextGenTime += 4;
			return;
		}

		CStreaming::RequestModel(m_nModelId, STREAMFLAGS_KEEP_IN_MEMORY);
		actualModelId = m_nModelId;
	}else{
		// A negative model id means "random car for this zone". -1 is the
		// "not yet resolved" marker; anything below that already has a model
		// cached in the negative id.
		if(m_nModelId == -1 || !CStreaming::HasModelLoaded(-m_nModelId)){
			CZoneInfo zoneInfo;
			CVector pos = FindPlayerCoors();
			CTheZones::GetZoneInfoForTimeOfDay(&pos, &zoneInfo);
			actualModelId = CCarCtrl::ChooseCarModel(CCarCtrl::ChooseCarRating(&zoneInfo));

			if(actualModelId < 0)
				return;

			CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(actualModelId);
			if(CModelInfo::IsBoatModel(actualModelId))
				return;

			if(mi->GetColModel()->boundingBox.GetSize().y > 8.0f)
				return;

			m_nModelId = -actualModelId;
			m_nPrimaryColor = -1;
			m_nSecondaryColor = -1;
		}else{
			actualModelId = -m_nModelId;
		}

		if(CheckForBlockage(actualModelId)){
			m_nNextGenTime += 4;
			return;
		}
	}

	if(!CStreaming::HasModelLoaded(actualModelId))
		return;

	if(CModelInfo::IsBoatModel(actualModelId)){
		switch(((CVehicleModelInfo*)CModelInfo::GetModelInfo(actualModelId))->m_nVehicleType){
		case VEHICLE_TYPE_HELI:
			vehicle = new CHeli(actualModelId, PARKED_VEHICLE);
			break;
		case VEHICLE_TYPE_PLANE:
			vehicle = new CPlane(actualModelId, PARKED_VEHICLE);
			break;
		default:
			vehicle = new CBoat(actualModelId, PARKED_VEHICLE);
			break;
		}

		CVector posn = GetPosn();
		baseZ = posn.z;
		if(baseZ <= MAP_Z_LOW_LIMIT)
			baseZ = CWorld::FindGroundZForCoord(posn.x, posn.y);
		vehicle->bExtendedRange = true;
	}else{
		CVector posn = GetPosn();
		if(posn.z > MAP_Z_LOW_LIMIT)
			posn.z += 1.0f;
		else
			posn.z = 1000.0f;

		if(!CWorld::ProcessVerticalLine(posn, -1000.0f, colPoint, entity, true, false, false, false, false, false, nil))
			return;

		baseZ = colPoint.point.z;

		// A script can pin a custom plate to a specific car generator. SA looks
		// it up by this generator's index before the car is even constructed.
		memset(plateText, 0, sizeof(plateText));
		CTheCarGenerators::m_SpecialPlateHandler.Find(CTheCarGenerators::GetIndex(this), plateText);

		switch(((CVehicleModelInfo*)CModelInfo::GetModelInfo(actualModelId))->m_nVehicleType){
		case VEHICLE_TYPE_MTRUCK:
		case VEHICLE_TYPE_QUAD:
		case VEHICLE_TYPE_TRAILER:
			// CMonsterTruck / CQuadBike / CTrailer are Block 2 classes; until
			// they exist SA spawns them through the same automobile path, and
			// the handling data is what makes them behave differently.
			vehicle = new CAutomobile(actualModelId, PARKED_VEHICLE);
			break;
		case VEHICLE_TYPE_HELI:
			vehicle = new CHeli(actualModelId, PARKED_VEHICLE);
			break;
		case VEHICLE_TYPE_PLANE:
			vehicle = new CPlane(actualModelId, PARKED_VEHICLE);
			break;
		case VEHICLE_TYPE_TRAIN:
			vehicle = new CTrain(actualModelId, PARKED_VEHICLE);
			break;
		case VEHICLE_TYPE_BIKE:
		case VEHICLE_TYPE_BMX:
			vehicle = new CBike(actualModelId, PARKED_VEHICLE);
			// Parked bikes rest on their side stand, as in SA.
			((CBike*)vehicle)->bIsStanding = true;
			break;
		default:
			vehicle = new CAutomobile(actualModelId, PARKED_VEHICLE);
			break;
		}

		vehicle->GetDistanceFromCentreOfMassToBaseOfModel();
		vehicle->bLightsOn = false;
	}

	vehicle->SetIsStatic(false);
	vehicle->bEngineOn = false;
	CVector posn = GetPosn();
	posn.z = vehicle->GetDistanceFromCentreOfMassToBaseOfModel() + baseZ;
	vehicle->SetPosition(posn);
	vehicle->SetOrientation(0.0f, 0.0f, m_nAngle * (TWOPI / 256));
	vehicle->SetStatus(STATUS_ABANDONED);
	vehicle->m_nDoorLock = CARLOCK_UNLOCKED;
	vehicle->bHasBeenOwnedByPlayer = bPlayerHasAlreadyOwnedCar;

	CWorld::Add(vehicle);

	if(CGeneral::GetRandomNumberInRange(0, 100) < m_nAlarmChance)
		vehicle->m_nAlarmState = -1;

	if(CGeneral::GetRandomNumberInRange(0, 100) < m_nDoorLockChance)
		vehicle->m_nDoorLock = CARLOCK_LOCKED;

	if(m_nPrimaryColor != -1 && m_nSecondaryColor != -1){
		vehicle->m_currentColour1 = m_nPrimaryColor;
		vehicle->m_currentColour2 = m_nSecondaryColor;
	}else if(m_nModelId < -1){
		// Remember the colour the chosen random model actually got, so the next
		// respawn of this generator reuses it.
		m_nPrimaryColor = vehicle->m_currentColour1;
		m_nSecondaryColor = vehicle->m_currentColour2;
	}

	CVisibilityPlugins::SetClumpAlpha(vehicle->GetClump(), 0);
	m_nVehicleHandle = CPools::GetVehiclePool()->GetIndex(vehicle);

	// SA's original comparison was between an unsigned m_nGenerateCount and the
	// signed constant -1, which after integer promotion made the decrement dead
	// code for the 0xFFFF "infinite" case. The observable behaviour is what
	// matters, and it is: finite counts decrement, 0xFFFF stays.
	if(m_nGenerateCount < (uint16)-1)
		m_nGenerateCount--;

	m_nNextGenTime = CalcNextGen();
}

// SA CCarGenerator::Process (0x6F3E90).
void
CCarGenerator::Process()
{
	if(m_nVehicleHandle == -1 &&
	   (CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter || m_nNextGenTime <= CTimer::GetTimeInMilliseconds())
	   && m_nGenerateCount != 0
	   && CheckIfWithinRangeOfAnyPlayers())
	{
		DoInternalProcessing();
	}

	if(m_nVehicleHandle == -1)
		return;

	CVehicle *vehicle = CPools::GetVehiclePool()->GetAt(m_nVehicleHandle);
	if(!vehicle){
		m_nVehicleHandle = -1;
		return;
	}

	if(vehicle->GetStatus() != STATUS_PLAYER)
		return;

	// The player took the car: stop tracking it, and hold the generator off for
	// a minute so the same car does not reappear at once.
	m_nNextGenTime += 60000;
	m_nVehicleHandle = -1;
	bWaitUntilFarFromPlayer = true;
	vehicle->bExtendedRange = false;
	if(m_nModelId < 0)
		m_nModelId = -1;
}

// SA CCarGenerator::Setup (0x6F2E50).
void
CCarGenerator::Setup(CVector posn, float angle, int32 modelId, int16 color1, int16 color2, uint8 forceSpawn,
	uint8 alarmChance, uint8 doorLockChance, uint16 minDelay, uint16 maxDelay,
	uint8 iplId, bool ignorePopulationLimit)
{
	// SA stores the angle as a byte in 1/256 turn units.
	const float magic = 256.0f / 360.0f;

	SetPosn(posn);
	m_nAngle = (char)(angle * magic);
	m_nModelId = modelId;
	m_nPrimaryColor = (int8)color1;
	m_nSecondaryColor = (int8)color2;

	bWaitUntilFarFromPlayer = false;
	bIgnorePopulationLimit = ignorePopulationLimit;
	bHighPriority = forceSpawn;
	bPlayerHasAlreadyOwnedCar = false;

	m_nAlarmChance = alarmChance;
	m_nDoorLockChance = doorLockChance;
	m_nMinDelay = minDelay;
	m_nMaxDelay = maxDelay;
	m_nIplId = iplId;

	m_nVehicleHandle = -1;
	m_nNextGenTime = CTimer::GetTimeInMilliseconds() + 1;
	m_nGenerateCount = 0;
	m_bIsUsed = true;
}

// SA CTheCarGenerators::Process (0x6F3F40).
//
// Generators are walked in four interleaved passes, one quarter of the table
// per frame, so a full sweep costs a quarter of the work every frame. The whole
// system is skipped while the player is on a train (the train would drive
// through freshly spawned cars) and during cutscenes and replay playback.
void
CTheCarGenerators::Process()
{
	if(FindPlayerTrain() || CReplay::IsPlayingBack())
		return;

	ProcessCounter++;
	if(ProcessCounter == 4)
		ProcessCounter = 0;

	for(int32 i = ProcessCounter; i < NUM_CAR_GENERATORS; i += 4){
		if(CarGeneratorArray[i].m_bIsUsed)
			CarGeneratorArray[i].Process();
	}

	if(GenerateEvenIfPlayerIsCloseCounter)
		GenerateEvenIfPlayerIsCloseCounter--;
}

// SA CTheCarGenerators::CreateCarGenerator (0x6F31A0).
//
// Returns the slot index, or -1 when the model id is out of the vehicle range
// or the table is full.
int32
CTheCarGenerators::CreateCarGenerator(CVector posn, float angle, int32 modelId, int16 color1, int16 color2,
	uint8 forceSpawn, uint8 alarmChance, uint8 doorLockChance, uint16 minDelay, uint16 maxDelay,
	uint8 iplId, bool ignorePopulationLimit)
{
	// SA's range is MODEL_LANDSTAL..MODEL_VEH_PALMKB8, the vehicle block of
	// the model table. reVC keeps VC indices, so the equivalent bounds are the
	// first and last vehicle model indices.
	if(modelId != -1 && (modelId < MI_LANDSTAL || modelId > MI_LAST_VEHICLE))
		return -1;

	int32 carGenIndex = 0;
	for(int32 i = 0; i < NUM_CAR_GENERATORS; i++){
		if(!CarGeneratorArray[i].m_bIsUsed){
			CarGeneratorArray[i].Setup(posn, angle, modelId, color1, color2, forceSpawn,
				alarmChance, doorLockChance, minDelay, maxDelay, iplId, ignorePopulationLimit);
			NumOfCarGenerators++;
			return carGenIndex;
		}
		carGenIndex++;
	}
	return -1;
}

// SA CTheCarGenerators::Init (0x6F3270).
void
CTheCarGenerators::Init()
{
	NumOfCarGenerators = 0;
	ProcessCounter = 0;
	GenerateEvenIfPlayerIsCloseCounter = 0;

	m_SpecialPlateHandler.m_nCount = 0;
	for(int32 i = 0; i < CSpecialPlateHandler::PLATES_COUNT; i++){
		m_SpecialPlateHandler.m_plateTextEntries[i].m_nCarGenId = -1;
		m_SpecialPlateHandler.m_plateTextEntries[i].m_szPlateText[0] = '\0';
	}

	for(int32 i = 0; i < NUM_CAR_GENERATORS; i++)
		CarGeneratorArray[i].m_bIsUsed = false;
}

// SA CTheCarGenerators::RemoveCarGenerators (0x6F3240). Called when an IPL
// streamed out: every generator belonging to it is released.
void
CTheCarGenerators::RemoveCarGenerators(uint8 iplId)
{
	for(int32 i = 0; i < NUM_CAR_GENERATORS; i++){
		if(CarGeneratorArray[i].m_nIplId == iplId){
			CarGeneratorArray[i].m_nIplId = 0;
			CarGeneratorArray[i].m_bIsUsed = false;
			NumOfCarGenerators--;
		}
	}
}

CCarGenerator *
CTheCarGenerators::Get(uint16 index)
{
	return &CarGeneratorArray[index];
}

int32
CTheCarGenerators::GetIndex(CCarGenerator *pCarGen)
{
	return pCarGen - CarGeneratorArray;
}

// SA CTheCarGenerators::Save (0x5D38C0) in reVC's save-buffer form.
//
// Only generators belonging to ipl 0 (i.e. not owned by a streamed IPL) are
// written, and each is preceded by its slot index so loading can restore it in
// place. The VC save format wrote a fixed-size array plus a live counter; the
// SA format is index-tagged and variable length, which is what is written here.
void
CTheCarGenerators::SaveAllCarGenerators(uint8 *buffer, uint32 *size)
{
	uint32 numToSave = 0;
	for(int32 i = 0; i < NUM_CAR_GENERATORS; i++){
		if(CarGeneratorArray[i].m_bIsUsed && CarGeneratorArray[i].m_nIplId == 0)
			numToSave++;
	}

	// header + count + counters + (index + generator) * numToSave + plates
	const uint32 platesSize = sizeof(m_SpecialPlateHandler.m_nCount)
		+ CSpecialPlateHandler::PLATES_COUNT * sizeof(tCarGenPlateText);
	*size = SAVE_HEADER_SIZE
		+ sizeof(uint32)							// numToSave
		+ sizeof(ProcessCounter)
		+ sizeof(GenerateEvenIfPlayerIsCloseCounter)
		+ numToSave * (sizeof(uint16) + sizeof(CCarGenerator))
		+ platesSize;

	INITSAVEBUF
	WriteSaveHeader(buffer, 'C','G','N','\0', *size - SAVE_HEADER_SIZE);

	WriteSaveBuf(buffer, numToSave);
	WriteSaveBuf(buffer, ProcessCounter);
	WriteSaveBuf(buffer, GenerateEvenIfPlayerIsCloseCounter);

	for(uint16 i = 0; i < NUM_CAR_GENERATORS; i++){
		CCarGenerator &carGen = CarGeneratorArray[i];
		if(carGen.m_bIsUsed && carGen.m_nIplId == 0){
			WriteSaveBuf(buffer, i);
			WriteSaveBuf(buffer, carGen);
		}
	}

	WriteSaveBuf(buffer, m_SpecialPlateHandler.m_nCount);
	for(int32 i = 0; i < CSpecialPlateHandler::PLATES_COUNT; i++)
		WriteSaveBuf(buffer, m_SpecialPlateHandler.m_plateTextEntries[i]);

	VALIDATESAVEBUF(*size)
}

// SA CTheCarGenerators::Load (0x5D39B0) in reVC's save-buffer form.
void
CTheCarGenerators::LoadAllCarGenerators(uint8 *buffer, uint32 size)
{
	Init();

	INITSAVEBUF
	CheckSaveHeader(buffer, 'C','G','N','\0', size - SAVE_HEADER_SIZE);

	uint32 numToLoad;
	ReadSaveBuf(&numToLoad, buffer);
	ReadSaveBuf(&ProcessCounter, buffer);
	ReadSaveBuf(&GenerateEvenIfPlayerIsCloseCounter, buffer);

	for(uint32 i = 0; i < numToLoad; i++){
		uint16 carGenIndex;
		ReadSaveBuf(&carGenIndex, buffer);
		if(carGenIndex < NUM_CAR_GENERATORS){
			ReadSaveBuf(&CarGeneratorArray[carGenIndex], buffer);
			NumOfCarGenerators++;
		}else{
			// Skip the entry but keep the stream aligned.
			SkipSaveBuf(buffer, sizeof(CCarGenerator));
		}
	}

	ReadSaveBuf(&m_SpecialPlateHandler.m_nCount, buffer);
	for(int32 i = 0; i < CSpecialPlateHandler::PLATES_COUNT; i++)
		ReadSaveBuf(&m_SpecialPlateHandler.m_plateTextEntries[i], buffer);

	VALIDATESAVEBUF(size)
}
