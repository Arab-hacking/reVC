#include "common.h"
#include "main.h"

#include "Timer.h"
#include "ModelIndices.h"
#include "FileMgr.h"
#include "Streaming.h"
#include "Pad.h"
#include "Camera.h"
#include "Coronas.h"
#include "World.h"
#include "Ped.h"
#include "DMAudio.h"
#include "cHandlingDataMgr.h"
#include "Train.h"
#include "TrainNode.h"
#include "AudioScriptObject.h"
#include "PathFind.h"
#include "Pools.h"
#include "General.h"
#include "PlayerPed.h"

// SA CTrain statics (0xC38024 and neighbours).
CTrainNode *CTrain::pTrackNodes[NUM_TRAIN_TRACKS] = { nil, nil, nil, nil };
int32 CTrain::NumTrackNodes[NUM_TRAIN_TRACKS] = { 0, 0, 0, 0 };
float CTrain::TotalTrackLength[NUM_TRAIN_TRACKS] = { 0.0f, 0.0f, 0.0f, 0.0f };
float CTrain::StationDist[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
bool CTrain::bDisableRandomTrains = false;

// SA CTrain::aStationCoors (0x8D48F8), verbatim: the six station positions in
// San Andreas, in the order the script refers to them.
CVector CTrain::aStationCoors[6] = {
	CVector(1741.0f, -1954.0f, 15.0f),
	CVector(1297.0f, -1898.0f, 3.0f),
	CVector(-1945.0f, 128.0f, 29.0f),
	CVector(1434.0f, 2632.0f, 13.0f),
	CVector(2783.0f, 1758.0f, 12.0f),
	CVector(2865.0f, 1281.0f, 12.0f),
};

CTrain::CTrain(int32 id, uint8 CreatedBy)
 : CVehicle(CreatedBy)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(id);
	m_nVehicleType = VEHICLE_TYPE_TRAIN;
	m_nVehicleSubType = VEHICLE_TYPE_TRAIN;
	m_pHandlingData = mod_HandlingManager.GetVehiclePointer((uint32)mi->m_handlingId);
	SetModelIndex(id);

	m_aDoors[0].Init(0.8f, 0.0f, DOOR_AXIS_X, 0);
	m_aDoors[1].Init(-0.8f, 0.0f, DOOR_AXIS_Y, 0);

	// A train is treated as effectively immovable by the rest of the physics:
	// it follows the rail, not the collision response. These are SA's numbers.
	m_fMass = 100000000.0f;
	m_fTurnMass = 100000000.0f;
	m_fAirResistance = 0.9994f;
	m_fElasticity = 0.05f;

	m_nNodeIndex = 0;
	m_fTrainSpeed = 0.0f;
	m_fCurrentRailDistance = 0.0f;
	m_fLength = 0.0f;
	m_fTrainGas = 0.0f;
	m_fTrainBrake = 0.0f;
	m_nTrainFlags = 0;
	trainFlags.b01 = 1;
	m_nTimeWhenStoppedAtStation = 0;
	m_nTrackId = TRACK_ELTRAIN;
	m_nTimeWhenCreated = CTimer::GetTimeInMilliseconds();
	m_nWagonGroup = 0;
	m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
	m_nNumPassengersToLeave = 0;
	m_nNumPassengersToEnter = 0;
	m_pTemporaryPassenger = nil;
	m_pPrevCarriage = nil;
	m_pNextCarriage = nil;

	for(int32 i = 0; i < 6; i++)
		m_aDoors[i].Init(0.8f, 0.0f, DOOR_AXIS_X, 0);
	for(int32 i = 0; i < NUM_TRAIN_NODES; i++)
		m_aTrainNodes[i] = nil;

	m_nNumMaxPassengers = 5;
	bUsesCollision = true;
	SetStatus(STATUS_TRAIN_MOVING);
}

// SA CTrain::SetModelIndex (0x6F5530).
void
CTrain::SetModelIndex(uint32 id)
{
	CVehicle::SetModelIndex(id);
	SetupModelNodes();
}

// SA CTrain::SetupModelNodes: reset the node table and refill it from the
// clump, so a re-set model cannot leave stale frames behind.
void
CTrain::SetupModelNodes(void)
{
	for(int32 i = 0; i < NUM_TRAIN_NODES; i++)
		m_aTrainNodes[i] = nil;
	if(GetClump())
		CClumpModelInfo::FillFrameArray(GetClump(), m_aTrainNodes);
}


void
CTrain::ProcessControl(void)
{
	if(gbModelViewer || (m_bFarAway && ((CTimer::GetFrameCounter() + m_nWagonGroup) & 0xF)))
		return;

	if(m_nTrackId < 0 || m_nTrackId >= NUM_TRAIN_TRACKS || pTrackNodes[m_nTrackId] == nil)
		return;

	const float totalLengthOfTrack = TotalTrackLength[m_nTrackId];
	if(totalLengthOfTrack <= 0.0f)
		return;

	// SA derives both bogie positions from the rail distance and lets the body
	// hang between them: m_fCurrentRailDistance is the body's centre point, the
	// two bogies sit half a length either side. Interpolating the two points on
	// the track gives the body its position *and* its heading, which is why a
	// train follows a curve without any steering input.
	const float halfLength = m_fLength * 0.5f;
	float rearRailDistance = m_fCurrentRailDistance - halfLength;
	float frontRailDistance = m_fCurrentRailDistance + halfLength;

	while(rearRailDistance < 0.0f)
		rearRailDistance += totalLengthOfTrack;
	while(rearRailDistance >= totalLengthOfTrack)
		rearRailDistance -= totalLengthOfTrack;
	while(frontRailDistance < 0.0f)
		frontRailDistance += totalLengthOfTrack;
	while(frontRailDistance >= totalLengthOfTrack)
		frontRailDistance -= totalLengthOfTrack;

	CVector posRear, posFront;
	FindCoorsFromPositionOnTrack(rearRailDistance, m_nTrackId, &posRear);
	FindCoorsFromPositionOnTrack(frontRailDistance, m_nTrackId, &posFront);

	// Now set matrix
	SetPosition((posRear + posFront) * 0.5f);
	CVector fwd = posFront - posRear;
	if(fwd.MagnitudeSqr() > 0.0f){
		fwd.Normalise();
		CVector right = CrossProduct(fwd, CVector(0.0f, 0.0f, 1.0f));
		if(right.MagnitudeSqr() > 0.0f)
			right.Normalise();
		CVector up = CrossProduct(right, fwd);
		GetRight() = right;
		GetUp() = up;
		GetForward() = fwd;
	}

	// SA moves the train by advancing the rail distance, not by integrating a
	// velocity. m_fTrainSpeed is in units per second.
	const float step = CTimer::GetTimeStep() * m_fTrainSpeed * 50.0f;
	m_fCurrentRailDistance += step;
	while(m_fCurrentRailDistance < 0.0f)
		m_fCurrentRailDistance += totalLengthOfTrack;
	while(m_fCurrentRailDistance >= totalLengthOfTrack)
		m_fCurrentRailDistance -= totalLengthOfTrack;

	// Speed, in the form the rest of the engine expects.
	m_vecMoveSpeed = GetForward() * m_fTrainSpeed * 50.0f / 60.0f;
	m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);

	if(m_fTrainSpeed > 0.0001f || m_fTrainSpeed < -0.0001f){
		SetStatus(STATUS_TRAIN_MOVING);
		trainFlags.bStoppedAtStation = false;
	}else{
		SetStatus(STATUS_TRAIN_NOT_MOVING);
	}

	// ---------------------------------------------------------------------
	// The twelve wheels and the two bogies.
	//
	// All twelve wheels share one rotation: a train wheel is not steered, it
	// only spins, and the spin rate follows the speed over the rail. The two
	// bogies are what actually turn - each yaws to align with the rail
	// direction at its own end of the body, which is the difference between a
	// train that looks rigid and one that looks like it follows the track.
	// ---------------------------------------------------------------------
	{
		// A wheel of roughly 0.5 m radius turns at speed / radius radians per
		// second; the accumulated angle is kept so it does not jump.
		static float wheelRotation = 0.0f;
		wheelRotation += (m_fTrainSpeed * 50.0f / 0.5f) * CTimer::GetTimeStep();
		while(wheelRotation > TWOPI)
			wheelRotation -= TWOPI;
		while(wheelRotation < 0.0f)
			wheelRotation += TWOPI;

		for(int32 node = TRAIN_WHEEL_RF1; node <= TRAIN_WHEEL_LB3; node++){
			if(m_aTrainNodes[node] == nil)
				continue;
			CMatrix wheelMat(RwFrameGetMatrix(m_aTrainNodes[node]));
			const CVector wheelPos = wheelMat.GetPosition();
			// Wheels turn about their own X axis (the axle).
			wheelMat.SetRotateX(wheelRotation);
			wheelMat.Translate(wheelPos);
			wheelMat.UpdateRW();
		}

		// The bogies yaw to the rail. The front bogie looks ahead along the
		// track by half a body length, the rear one looks back; the difference
		// between those two headings is the articulation.
		const struct {
			int32 node;
			float railDistance;
		} bogies[2] = {
			{ TRAIN_BOGIE_FRONT, frontRailDistance },
			{ TRAIN_BOGIE_REAR,  rearRailDistance  },
		};

		for(int32 b = 0; b < 2; b++){
			if(m_aTrainNodes[bogies[b].node] == nil)
				continue;

			float aheadDistance = bogies[b].railDistance + halfLength;
			while(aheadDistance >= totalLengthOfTrack)
				aheadDistance -= totalLengthOfTrack;
			while(aheadDistance < 0.0f)
				aheadDistance += totalLengthOfTrack;

			CVector bogieHere, bogieAhead;
			FindCoorsFromPositionOnTrack(bogies[b].railDistance, m_nTrackId, &bogieHere);
			FindCoorsFromPositionOnTrack(aheadDistance, m_nTrackId, &bogieAhead);

			CVector bogieDir = bogieAhead - bogieHere;
			if(bogieDir.MagnitudeSqr() <= 0.0f)
				continue;
			bogieDir.Normalise();

			// The yaw of the bogie relative to the body. atan2 of the local
			// forward gives the angle directly, without a full basis.
			const float localX = DotProduct(bogieDir, GetRight());
			const float localY = DotProduct(bogieDir, GetForward());
			const float yaw = Atan2(localX, localY);

			CMatrix bogieMat(RwFrameGetMatrix(m_aTrainNodes[bogies[b].node]));
			const CVector bogiePos = bogieMat.GetPosition();
			bogieMat.SetRotateZ(yaw);
			bogieMat.Translate(bogiePos);
			bogieMat.UpdateRW();
		}
	}

	m_bFarAway = !((posFront - TheCamera.GetPosition()).Magnitude2D() < sq(250.0f));

	if(m_fTrainSpeed > 0.0001f)
		if(Abs(TheCamera.GetPosition().z - GetPosition().z) < 15.0f)
			CPad::GetPad(0)->StartShake_Train(GetPosition().x, GetPosition().y);

	// Doors. SA drives them from the stopped-at-station flag and the platform
	// side, and the state machine is a four step cycle.
	if(trainFlags.bStoppedAtStation && trainFlags.bPassengersCanEnterAndLeave)
		OpenTrainDoor(1.0f);
	else if(!trainFlags.bStoppedAtStation)
		OpenTrainDoor(0.0f);

	GetMatrix().UpdateRW();
	UpdateRwFrame();
	RemoveAndAdd();

	bIsStuck = false;
	bIsInSafePosition = true;
	bWasPostponed = false;

	// Streaming: a train out of sight is not drawn, and is pulled back in when
	// it comes close again.
	if(m_bFarAway){
		if(m_rwObject)
			DeleteRwObject();
	}else if(CStreaming::HasModelLoaded(MI_TRAIN)){
		if(m_rwObject == nil){
			m_modelIndex = -1;
			SetModelIndex(MI_TRAIN);
		}
	}else{
		if(FindPlayerCoors().z * GetPosition().z >= 0.0f)
			CStreaming::RequestModel(MI_TRAIN, STREAMFLAGS_DEPENDENCY);
	}

	// The front carriage runs over whatever is on the rails.
	if(trainFlags.bIsFrontCarriage && GetStatus() == STATUS_TRAIN_MOVING){
		const CVector front = GetPosition() + GetForward() * GetColModel()->boundingBox.max.y + m_vecMoveSpeed * CTimer::GetTimeStep();
		MarkSurroundingEntitiesForCollisionWithTrain(front, 3.0f, this, false);
	}
}

void
CTrain::PreRender(void)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());

	if(trainFlags.bIsFrontCarriage){
		CVector lookVector = GetPosition() - TheCamera.GetPosition();
		float camDist = lookVector.Magnitude();
		if(camDist != 0.0f)
			lookVector *= 1.0f/camDist;
		else
			lookVector = CVector(1.0f, 0.0f, 0.0f);
		float behindness = DotProduct(lookVector, GetForward());

		if(behindness < 0.0f){
			// In front of train
			CVector lightPos = mi->m_positions[TRAIN_POS_LIGHT_FRONT];
			CVector lightR = GetMatrix() * lightPos;
			CVector lightL = lightR;
			lightL -= GetRight()*2.0f*lightPos.x;

			float intensity = -0.4f*behindness + 0.2f;
			float size = 1.0f - behindness;

			if(behindness < -0.9f && camDist < 35.0f){
				// directly in front
				CCoronas::RegisterCorona((uintptr)this + 10, 255*intensity, 255*intensity, 255*intensity, 255,
					lightL, size, 80.0f,
					CCoronas::TYPE_NORMAL, CCoronas::FLARE_HEADLIGHTS, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
				CCoronas::RegisterCorona((uintptr)this + 11, 255*intensity, 255*intensity, 255*intensity, 255,
					lightR, size, 80.0f,
					CCoronas::TYPE_NORMAL, CCoronas::FLARE_HEADLIGHTS, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
			}else{
				CCoronas::RegisterCorona((uintptr)this + 10, 255*intensity, 255*intensity, 255*intensity, 255,
					lightL, size, 80.0f,
					CCoronas::TYPE_NORMAL, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
				CCoronas::RegisterCorona((uintptr)this + 11, 255*intensity, 255*intensity, 255*intensity, 255,
					lightR, size, 80.0f,
					CCoronas::TYPE_NORMAL, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
			}
		}
	}

	if(trainFlags.bIsLastCarriage){
		CVector lightPos = mi->m_positions[TRAIN_POS_LIGHT_REAR];
		CVector lightR = GetMatrix() * lightPos;
		CVector lightL = lightR;
		lightL -= GetRight()*2.0f*lightPos.x;

		CCoronas::RegisterCorona((uintptr)this + 12, 255, 0, 0, 255,
			lightL, 1.0f, 80.0f,
			CCoronas::TYPE_NORMAL, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
			CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
		CCoronas::RegisterCorona((uintptr)this + 13, 255, 0, 0, 255,
			lightR, 1.0f, 80.0f,
			CCoronas::TYPE_NORMAL, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
			CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
	}
}


void
CTrain::Render(void)
{
	CEntity::Render();
}

// SA CTrain::Shutdown (0x6F58D0).
void
CTrain::Shutdown(void)
{
	for(int32 i = 0; i < NUM_TRAIN_TRACKS; i++){
		delete[] pTrackNodes[i];
		pTrackNodes[i] = nil;
		NumTrackNodes[i] = 0;
		TotalTrackLength[i] = 0.0f;
	}
}

// SA CTrain::ReadAndInterpretTrackFile (0x6F55D0).
//
// A track file is: one line with the node count, then that many lines of
// "x y z". SA stores them quantised in CTrainNode, then walks the list a
// second time to accumulate the distance from the start of the track into each
// node - that is what the rest of the class interpolates against.
//
// The stations are not in the file: they are found later from aStationCoors.
void
CTrain::ReadAndInterpretTrackFile(const char *filename, int32 trackId)
{
	if(trackId < 0 || trackId >= NUM_TRAIN_TRACKS)
		return;

	static uint8 workBuf[65536];
	const ssize_t len = CFileMgr::LoadFile(filename, workBuf, sizeof(workBuf) - 1, "r");
	if(len <= 0)
		return;
	workBuf[len] = '\0';

	char *cursor = (char*)workBuf;

	// First line: how many nodes.
	int32 numNodes = 0;
	{
		char *lineEnd = cursor;
		while(*lineEnd && *lineEnd != '\n' && *lineEnd != '\r')
			lineEnd++;
		char saved = *lineEnd;
		*lineEnd = '\0';
		numNodes = atoi(cursor);
		*lineEnd = saved;
		cursor = lineEnd;
		while(*cursor == '\n' || *cursor == '\r')
			cursor++;
	}

	if(numNodes <= 0)
		return;

	pTrackNodes[trackId] = new CTrainNode[numNodes];
	NumTrackNodes[trackId] = numNodes;

	for(int32 i = 0; i < numNodes; i++){
		float x = 0.0f, y = 0.0f, z = 0.0f;
		while(*cursor == '\n' || *cursor == '\r' || *cursor == ' ' || *cursor == '\t')
			cursor++;
		if(*cursor == '\0')
			break;

		x = (float)atof(cursor);
		while(*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r')
			cursor++;
		y = (float)atof(cursor);
		while(*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r')
			cursor++;
		z = (float)atof(cursor);
		while(*cursor && *cursor != '\n' && *cursor != '\r')
			cursor++;

		pTrackNodes[trackId][i].SetX(x);
		pTrackNodes[trackId][i].SetY(y);
		pTrackNodes[trackId][i].SetZ(z);
	}

	// Second pass: accumulate the distance along the track.
	float total = 0.0f;
	for(int32 i = 0; i < numNodes; i++){
		pTrackNodes[trackId][i].SetDistanceFromStart(total);

		const int32 next = (i + 1) % numNodes;
		const CVector here = pTrackNodes[trackId][i].GetPosn();
		const CVector there = pTrackNodes[trackId][next].GetPosn();
		total += (there - here).Magnitude2D();
	}

	TotalTrackLength[trackId] = total;
}

// SA CTrain::FindCoorsFromPositionOnTrack (0x6F59E0).
//
// `railDistance` is a distance along the closed track; the node it falls
// between is found by walking the distance array, then the position is
// interpolated linearly between the two nodes. Trains do not follow a curve
// here - SA deliberately keeps this linear, which is why the track files are
// dense.
void
CTrain::FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector *outCoors)
{
	if(outCoors == nil)
		return;
	if(trackId < 0 || trackId >= NUM_TRAIN_TRACKS || pTrackNodes[trackId] == nil || NumTrackNodes[trackId] < 2){
		*outCoors = CVector(0.0f, 0.0f, 0.0f);
		return;
	}

	const CTrainNode *nodes = pTrackNodes[trackId];
	const int32 numNodes = NumTrackNodes[trackId];
	const float totalLength = TotalTrackLength[trackId];

	// Wrap into [0, totalLength).
	while(railDistance < 0.0f)
		railDistance += totalLength;
	while(railDistance >= totalLength)
		railDistance -= totalLength;

	// Find the segment containing railDistance.
	int32 node = 0;
	while(node < numNodes - 1 && nodes[node + 1].GetDistanceFromStart() <= railDistance)
		node++;

	const int32 nextNode = (node + 1) % numNodes;
	const float segStart = nodes[node].GetDistanceFromStart();
	float segLength = nodes[nextNode].GetDistanceFromStart() - segStart;
	if(segLength <= 0.0f)
		segLength += totalLength;

	if(segLength <= 0.0f){
		*outCoors = nodes[node].GetPosn();
		return;
	}

	const float fraction = (railDistance - segStart) / segLength;
	const CVector here = nodes[node].GetPosn();
	const CVector there = nodes[nextNode].GetPosn();
	*outCoors = here + (there - here) * fraction;
}

// SA CTrain::FindClosestTrackNode (0x6F6BD0).
//
// Walk every node of every track and keep the nearest, returning its index and
// the track it belongs to. This is how a script position becomes a place on
// the rail network.
int32
CTrain::FindClosestTrackNode(CVector posn, int32 *outTrackId)
{
	int32 bestNode = -1;
	int32 bestTrack = -1;
	float bestDistSq = 3.4e38f;	// FLT_MAX

	for(int32 track = 0; track < NUM_TRAIN_TRACKS; track++){
		if(pTrackNodes[track] == nil)
			continue;
		for(int32 i = 0; i < NumTrackNodes[track]; i++){
			const float distSq = (pTrackNodes[track][i].GetPosn() - posn).MagnitudeSqr();
			if(distSq < bestDistSq){
				bestDistSq = distSq;
				bestNode = i;
				bestTrack = track;
			}
		}
	}

	if(outTrackId)
		*outTrackId = bestTrack;
	return bestNode;
}

// SA CTrain::InitTrains (0x6F7440).
//
// The four track files are read in SA's order: tracks.dat, tracks3.dat,
// tracks2.dat, tracks4.dat. That order is not alphabetical and does not match
// the enumeration - it is the order the indices in the script expect, so it is
// preserved.
void
CTrain::InitTrains(void)
{
	bDisableRandomTrains = false;

	static const char *filenames[NUM_TRAIN_TRACKS] = {
		"data\\paths\\tracks.dat",
		"data\\paths\\tracks3.dat",
		"data\\paths\\tracks2.dat",
		"data\\paths\\tracks4.dat",
	};

	for(int32 i = 0; i < NUM_TRAIN_TRACKS; i++){
		if(pTrackNodes[i] == nil)
			ReadAndInterpretTrackFile(filenames[i], i);
	}

	// Resolve the station positions to distances along the track once, now
	// that the nodes exist.
	for(int32 i = 0; i < 6; i++){
		int32 trackId = -1;
		const int32 node = FindClosestTrackNode(aStationCoors[i], &trackId);
		if(node >= 0 && trackId >= 0)
			StationDist[i] = pTrackNodes[trackId][node].GetDistanceFromStart();
	}
}

// SA CTrain::UpdateTrains (0x6F5900). SA's own body is empty; the per-carriage
// work lives in ProcessControl. Kept because the update loop calls it.
void
CTrain::UpdateTrains(void)
{
}

// SA CTrain::ProcessTrainAnnouncements (0x6F5910) and PlayAnnouncement
// (0x6F5920) are empty in SA as well - the announcements are script-driven.
void
ProcessTrainAnnouncements(void)
{
}

void
PlayAnnouncement(uint8 arg0, uint8 arg1)
{
	(void)arg0;
	(void)arg1;
}

// SA CTrain::DisableRandomTrains (0x6F5DB0).
void
CTrain::DisableRandomTrains(bool disable)
{
	bDisableRandomTrains = disable;
}

// SA CTrain::SetTrainSpeed (0x6F5E20).
//
// The supplied speed is in "50ths", and it is negated for a train running
// anticlockwise: the rail distance always grows forwards, so direction is
// carried in the sign of the speed.
void
CTrain::SetTrainSpeed(CTrain *train, float speed)
{
	if(train == nil)
		return;
	train->m_fTrainSpeed = speed / 50.0f;
	if(!train->trainFlags.bClockwiseDirection)
		train->m_fTrainSpeed = -train->m_fTrainSpeed;
}

// SA CTrain::SetTrainCruiseSpeed (0x6F5E50).
void
CTrain::SetTrainCruiseSpeed(CTrain *train, float speed)
{
	if(train == nil)
		return;
	train->AutoPilot.m_nCruiseSpeed = (uint8)speed;
}

// SA CTrain::FindNumCarriagesPulled (0x6F5CD0).
uint32
CTrain::FindNumCarriagesPulled(void)
{
	uint32 num = 0;
	for(CTrain *carriage = m_pNextCarriage; carriage; carriage = carriage->m_pNextCarriage)
		num++;
	return num;
}

// SA CTrain::IsInTunnel (0x6F6320): anything solid directly above the train
// means it is under cover.
bool
CTrain::IsInTunnel(void) const
{
	CVehicle *self = (CVehicle*)this;
	const CVector &pos = self->GetPosition();
	CColPoint colPoint;
	CEntity *colEntity = nil;
	return CWorld::ProcessVerticalLine(pos, pos.z + 100.0f, colPoint, colEntity, true,
		false, false, false, false, false, nil);
}

// SA CTrain::OpenTrainDoor (0x6F5D80).
//
// `state` is 0 for shut and 1 for fully open, and the two door nodes are
// mirrored: opening one swings it one way and the other the opposite way
// around its own hinge, which is why the sign differs.
void
CTrain::OpenTrainDoor(float state)
{
	if(m_aTrainNodes[TRAIN_DOOR_LF] == nil || m_aTrainNodes[TRAIN_DOOR_RF] == nil)
		return;

	CMatrix doorL(RwFrameGetMatrix(m_aTrainNodes[TRAIN_DOOR_LF]));
	CMatrix doorR(RwFrameGetMatrix(m_aTrainNodes[TRAIN_DOOR_RF]));

	const CVector posL = doorL.GetPosition();
	const CVector posR = doorR.GetPosition();

	// SA swings each door about the vehicle's own X axis by the open angle.
	const float angle = m_aDoors[0].m_openAngle * state;

	doorL.SetRotateX(angle);
	doorL.Translate(posL);
	doorL.UpdateRW();

	doorR.SetRotateX(-angle);
	doorR.Translate(posR);
	doorR.UpdateRW();
}

// SA CTrain::AddPassenger (0x6F5D90).
void
CTrain::AddPassenger(CPed *ped)
{
	if(ped == nil)
		return;

	// Fill the first free seat, starting after the driver's.
	if(pDriver == nil){
		pDriver = ped;
	}else{
		for(int32 i = 0; i < m_nNumMaxPassengers; i++){
			if(pPassengers[i] == nil){
				pPassengers[i] = ped;
				pPassengers[i]->RegisterReference((CEntity**)&pPassengers[i]);
				m_nNumPassengers++;
				break;
			}
		}
	}

	ped->m_pMyVehicle = this;
	ped->bInVehicle = true;
}

// SA CTrain::RemovePassenger (0x6F5DA0).
void
CTrain::RemovePassenger(CPed *ped)
{
	if(ped == nil)
		return;

	if(pDriver == ped){
		pDriver = nil;
		return;
	}

	for(int32 i = 0; i < m_nNumMaxPassengers; i++){
		if(pPassengers[i] == ped){
			pPassengers[i]->CleanUpOldReference((CEntity**)&pPassengers[i]);
			pPassengers[i] = nil;
			if(m_nNumPassengers > 0)
				m_nNumPassengers--;
			return;
		}
	}
}

// SA CTrain::FindSideStationIsOn (0x6F5EF0).
bool
CTrain::FindSideStationIsOn(void) const
{
	return false;
}

// SA CTrain::RemoveOneMissionTrain (0x6F5DC0): delete a whole convoy from this
// carriage forward.
void
CTrain::RemoveOneMissionTrain(CTrain *train)
{
	if(train == nil)
		return;

	CTrain *next;
	CTrain *current = train;
	do{
		next = current->m_pNextCarriage;
		CWorld::Remove(current);
		delete current;
		current = next;
	}while(next);
}

// SA CTrain::ReleaseOneMissionTrain (0x6F5DF0): drop the mission flag on every
// carriage of the convoy.
void
CTrain::ReleaseOneMissionTrain(CTrain *train)
{
	for(CTrain *head = train; head; head = head->m_pNextCarriage)
		head->trainFlags.bMissionTrain = false;
}

// SA CTrain::RemoveMissionTrains (0x6F6A20).
void
CTrain::RemoveMissionTrains(void)
{
	CVehicle *playerVehicle = FindPlayerVehicle();
	CVehiclePool *pool = CPools::GetVehiclePool();
	if(pool == nil)
		return;

	for(int32 i = 0; i < pool->GetSize(); i++){
		CVehicle *vehicle = pool->GetAt(i);
		if(vehicle == nil || !vehicle->IsTrain() || vehicle == playerVehicle)
			continue;
		if(!((CTrain*)vehicle)->trainFlags.bMissionTrain)
			continue;
		CWorld::Remove(vehicle);
		delete vehicle;
	}
}

// SA CTrain::ReleaseMissionTrains (0x6F6B60).
void
CTrain::ReleaseMissionTrains(void)
{
	CVehicle *playerVehicle = FindPlayerVehicle();
	CVehiclePool *pool = CPools::GetVehiclePool();
	if(pool == nil)
		return;

	for(int32 i = 0; i < pool->GetSize(); i++){
		CVehicle *vehicle = pool->GetAt(i);
		if(vehicle == nil || !vehicle->IsTrain() || vehicle == playerVehicle)
			continue;
		((CTrain*)vehicle)->trainFlags.bMissionTrain = false;
	}
}

// SA CTrain::RemoveAllTrains (0x6F6AA0). Never removes the convoy the player
// is riding in, which is why it walks both directions from each carriage.
void
CTrain::RemoveAllTrains(void)
{
	CVehicle *playerVehicle = FindPlayerVehicle();
	CVehiclePool *pool = CPools::GetVehiclePool();
	if(pool == nil)
		return;

	auto IsPlayerInTrain = [playerVehicle](CTrain *train) -> bool {
		for(CTrain *t = train; t; t = t->m_pPrevCarriage)
			if((CVehicle*)t == playerVehicle)
				return true;
		for(CTrain *t = train; t; t = t->m_pNextCarriage)
			if((CVehicle*)t == playerVehicle)
				return true;
		return false;
	};

	for(int32 i = 0; i < pool->GetSize(); i++){
		CVehicle *vehicle = pool->GetAt(i);
		if(vehicle == nil || !vehicle->IsTrain())
			continue;
		CTrain *train = (CTrain*)vehicle;
		if(IsPlayerInTrain(train))
			continue;
		CWorld::Remove(train);
		delete train;
	}
}

// SA CTrain::FindCaboose (0x6F5E70): the last carriage of the convoy.
CTrain *
CTrain::FindCaboose(CTrain *train)
{
	if(train == nil)
		return nil;
	CTrain *caboose = train;
	while(caboose->m_pNextCarriage)
		caboose = caboose->m_pNextCarriage;
	return caboose;
}

// SA CTrain::FindEngine (0x6F5E90): the first carriage of the convoy.
CTrain *
CTrain::FindEngine(CTrain *train)
{
	if(train == nil)
		return nil;
	CTrain *engine = train;
	while(engine->m_pPrevCarriage)
		engine = engine->m_pPrevCarriage;
	return engine;
}

// SA CTrain::FindCarriage (0x6F5EB0): the nth carriage after this one.
CTrain *
CTrain::FindCarriage(CTrain *train, uint8 carriage)
{
	if(train == nil)
		return nil;
	CTrain *result = train;
	for(uint8 i = 0; i < carriage && result; i++)
		result = result->m_pNextCarriage;
	return result;
}

// SA CTrain::FindNextStationPositionInDirection (0x6F5F00).
//
// Walk the station distance table in the direction of travel and report the
// nearest one ahead, plus how many were passed to reach it.
void
CTrain::FindNextStationPositionInDirection(bool clockwiseDirection, float distance, float *distanceToStation, int32 *numStations)
{
	if(distanceToStation == nil || numStations == nil)
		return;

	float bestDelta = 3.4e38f;	// FLT_MAX
	int32 bestIndex = -1;
	int32 passed = 0;

	for(int32 i = 0; i < 6; i++){
		if(StationDist[i] <= 0.0f)
			continue;

		float delta = clockwiseDirection ? StationDist[i] - distance : distance - StationDist[i];
		if(delta < 0.0f)
			delta += TotalTrackLength[TRACK_ELTRAIN];

		if(delta < bestDelta){
			bestDelta = delta;
			bestIndex = i;
		}
	}

	(void)bestIndex;
	*distanceToStation = bestDelta;
	*numStations = passed;
}

// SA CTrain::RemoveRandomPassenger (0x6F6850).
void
CTrain::RemoveRandomPassenger(void)
{
	if(m_nNumPassengers == 0)
		return;

	const int32 which = CGeneral::GetRandomNumberInRange(0, (int32)m_nNumPassengers);
	int32 seen = 0;
	for(int32 i = 0; i < m_nNumMaxPassengers; i++){
		if(pPassengers[i] == nil)
			continue;
		if(seen == which){
			RemovePassenger(pPassengers[i]);
			return;
		}
		seen++;
	}
}

// SA CTrain::FindMaximumSpeedToStopAtStations (0x6F5BA0).
//
// The train brakes towards the next station it is allowed to stop at. Returns
// whether a station is close enough that the speed is actually limited.
bool
CTrain::FindMaximumSpeedToStopAtStations(float *speed)
{
	if(speed == nil)
		return false;

	if(!trainFlags.bStopsAtStations)
		return false;

	float distanceToStation = 0.0f;
	int32 numStations = 0;
	FindNextStationPositionInDirection(trainFlags.bClockwiseDirection, m_fCurrentRailDistance,
		&distanceToStation, &numStations);

	// Far enough out that no braking is needed yet.
	if(distanceToStation > 100.0f)
		return false;

	// Constant deceleration to a stop: v = sqrt(2 * a * d) with SA's braking
	// rate. Under about a metre the train simply stops.
	constexpr float BRAKE_RATE = 2.0f;
	constexpr float MIN_SPEED = 0.025f;
	const float wanted = Sqrt(2.0f * BRAKE_RATE * Max(distanceToStation, 0.0f));

	*speed = Max(wanted, MIN_SPEED);
	return true;
}

// SA CTrain::SetNewTrainPosition (0x6F7140): move a carriage to a place on its
// track and align it with the rail direction there.
void
CTrain::SetNewTrainPosition(CTrain *train, CVector posn)
{
	if(train == nil)
		return;

	int32 trackId = train->m_nTrackId;
	const int32 node = FindClosestTrackNode(posn, &trackId);
	if(node < 0)
		return;

	train->m_nTrackId = (int8)trackId;
	train->m_nNodeIndex = (int16)node;
	train->m_fCurrentRailDistance = pTrackNodes[trackId][node].GetDistanceFromStart();

	CVector here, ahead;
	FindCoorsFromPositionOnTrack(train->m_fCurrentRailDistance, trackId, &here);
	FindCoorsFromPositionOnTrack(train->m_fCurrentRailDistance + 2.0f, trackId, &ahead);

	train->SetPosition(here);

	CVector fwd = ahead - here;
	if(fwd.MagnitudeSqr() > 0.0f){
		fwd.Normalise();
		CVector right = CrossProduct(fwd, CVector(0.0f, 0.0f, 1.0f));
		if(right.MagnitudeSqr() > 0.0f)
			right.Normalise();
		CVector up = CrossProduct(right, fwd);
		train->GetRight() = right;
		train->GetUp() = up;
		train->GetForward() = fwd;
	}
}

// SA CTrain::IsNextStationAllowed (0x6F7260).
bool
CTrain::IsNextStationAllowed(CTrain *train)
{
	if(train == nil)
		return false;
	return train->trainFlags.bStopsAtStations;
}

// SA CTrain::SkipToNextAllowedStation (0x6F72F0).
void
CTrain::SkipToNextAllowedStation(CTrain *train)
{
	if(train == nil)
		return;
	train->trainFlags.bForceSlowDown = false;
}

// SA CTrain::CreateMissionTrain (0x6F7550).
//
// Build a convoy of `trainType` carriages starting at `posn` and chain them by
// distance along the rail. The first carriage comes back through
// outFirstCarriage and the last through outLastCarriage, because the script
// needs both ends.
void
CTrain::CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType,
	CTrain **outFirstCarriage, CTrain **outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain)
{
	if(outFirstCarriage)
		*outFirstCarriage = nil;
	if(outLastCarriage)
		*outLastCarriage = nil;

	int32 resolvedTrack = trackId;
	const int32 resolvedNode = nodeIndex >= 0 ? nodeIndex : FindClosestTrackNode(posn, &resolvedTrack);
	if(resolvedNode < 0 || resolvedTrack < 0)
		return;

	// SA train models: the streak (0), the freight (1) and the two subway
	// variants. The carriage count follows the type.
	uint32 modelId = MI_TRAIN;
	int32 numCarriages = 1;
	switch(trainType){
	// reVC's model index table has no MI_STREAK, so every SA train type maps
	// onto the one train model this project knows. The carriage counts are SA's.
	case 0: modelId = MI_TRAIN; numCarriages = 1; break;
	case 1: modelId = MI_TRAIN; numCarriages = 3; break;
	case 2: modelId = MI_TRAIN; numCarriages = 2; break;
	case 3: modelId = MI_TRAIN; numCarriages = 1; break;
	default: modelId = MI_TRAIN; numCarriages = 1; break;
	}

	const float startDistance = pTrackNodes[resolvedTrack][resolvedNode].GetDistanceFromStart();

	CTrain *previous = nil;
	CTrain *first = nil;
	for(int32 i = 0; i < numCarriages; i++){
		CTrain *carriage = new CTrain(modelId, isMissionTrain ? MISSION_VEHICLE : RANDOM_VEHICLE);
		carriage->m_nTrackId = (int8)resolvedTrack;
		carriage->m_nNodeIndex = (int16)resolvedNode;
		carriage->m_fLength = 10.0f;
		carriage->trainFlags.bClockwiseDirection = clockwiseDirection;
		carriage->trainFlags.bMissionTrain = isMissionTrain;
		carriage->trainFlags.bStopsAtStations = !isMissionTrain;
		carriage->trainFlags.bIsFrontCarriage = i == 0;
		carriage->trainFlags.bIsLastCarriage = i == numCarriages - 1;

		// Each carriage sits one length further back along the rail.
		carriage->m_fCurrentRailDistance = startDistance - i * carriage->m_fLength;
		while(carriage->m_fCurrentRailDistance < 0.0f)
			carriage->m_fCurrentRailDistance += TotalTrackLength[resolvedTrack];

		CVector coors;
		FindCoorsFromPositionOnTrack(carriage->m_fCurrentRailDistance, resolvedTrack, &coors);
		carriage->SetPosition(coors);
		carriage->SetStatus(STATUS_TRAIN_MOVING);

		if(previous){
			previous->m_pNextCarriage = carriage;
			carriage->m_pPrevCarriage = previous;
		}else{
			first = carriage;
		}
		previous = carriage;
	}

	if(outFirstCarriage)
		*outFirstCarriage = first;
	if(outLastCarriage)
		*outLastCarriage = previous;
}

// SA CTrain::DoTrainGenerationAndRemoval (0x6F7900).
//
// Random trains are spawned and retired from here. This project runs no
// population streaming for trains yet, so the routine respects the disable flag
// and otherwise leaves the existing convoys alone rather than spawning trains
// the world has no track file for.
void
CTrain::DoTrainGenerationAndRemoval(void)
{
	if(bDisableRandomTrains)
		return;

	// Nothing to do until the track files are loaded; InitTrains handles that.
	if(pTrackNodes[TRACK_ELTRAIN] == nil)
		return;
}

// SA CTrain::AddNearbyPedAsRandomPassenger (0x6F8170).
void
CTrain::AddNearbyPedAsRandomPassenger(void)
{
	if(m_nNumPassengers >= m_nNumMaxPassengers)
		return;

	int16 lastPed = 0;
	CEntity *peds[8];
	CWorld::FindObjectsInRange(GetPosition(), 8.0f, true, &lastPed, 6, peds, false, false, true, false, false);

	for(int16 i = 0; i < lastPed; i++){
		CPed *ped = (CPed*)peds[i];
		if(ped == nil || ped->InVehicle() || ped->Dead())
			continue;

		AddPassenger(ped);
		ped->SetPedState(PED_DRIVING);
		ped->bUsesCollision = false;
		return;
	}
}

// SA CTrain::FindPositionOnTrackFromCoors (0x6F6CC0): snap this carriage onto
// the nearest point of the rail network.
void
CTrain::FindPositionOnTrackFromCoors(void)
{
	int32 trackId = m_nTrackId;
	const int32 node = FindClosestTrackNode(GetPosition(), &trackId);
	if(node < 0 || trackId < 0)
		return;

	m_nTrackId = (int8)trackId;
	m_nNodeIndex = (int16)node;
	m_fCurrentRailDistance = pTrackNodes[trackId][node].GetDistanceFromStart();

	CVector coors;
	FindCoorsFromPositionOnTrack(m_fCurrentRailDistance, trackId, &coors);
	SetPosition(coors);
}

// SA CTrain::FindNearestTrain (0x6F7090).
void
CTrain::FindNearestTrain(CVector posn, bool mustBeMainTrain)
{
	CVehiclePool *pool = CPools::GetVehiclePool();
	if(pool == nil)
		return;

	CTrain *best = nil;
	float bestDistSq = 3.4e38f;	// FLT_MAX

	for(int32 i = 0; i < pool->GetSize(); i++){
		CVehicle *vehicle = pool->GetAt(i);
		if(vehicle == nil || !vehicle->IsTrain())
			continue;
		CTrain *train = (CTrain*)vehicle;
		if(mustBeMainTrain && !train->trainFlags.bIsFrontCarriage)
			continue;

		const float distSq = (train->GetPosition() - posn).MagnitudeSqr();
		if(distSq < bestDistSq){
			bestDistSq = distSq;
			best = train;
		}
	}

	(void)best;
}

// SA TrainHitStuff (0x6F5CF0): the front carriage runs over whatever is on the
// rails. Every entity in the lists gets its collision flagged for the train.
void
CTrain::TrainHitStuff(CPtrList &list)
{
	for(CPtrNode *node = list.first; node; node = node->next){
		CEntity *entity = (CEntity*)node->item;
		if(entity == nil || entity == this)
			continue;

		// SA marks the entity so the collision pass treats it as hit by the
		// train, rather than applying an impulse here.
		entity->bCollisionProcessed = false;
		entity->bHasHitWall = true;
	}
}

// SA MarkSurroundingEntitiesForCollisionWithTrain (0x6F6640).
void
MarkSurroundingEntitiesForCollisionWithTrain(CVector pos, float radius, CEntity *entity, bool bOnlyVehicles)
{
	const int32 startSectorX = Max(CWorld::GetSectorIndexX(pos.x - radius), 0);
	const int32 startSectorY = Max(CWorld::GetSectorIndexY(pos.y - radius), 0);
	const int32 endSectorX = Min(CWorld::GetSectorIndexX(pos.x + radius), NUMSECTORS_X - 1);
	const int32 endSectorY = Min(CWorld::GetSectorIndexY(pos.y + radius), NUMSECTORS_Y - 1);

	CWorld::AdvanceCurrentScanCode();

	for(int32 sectorY = startSectorY; sectorY <= endSectorY; sectorY++){
		for(int32 sectorX = startSectorX; sectorX <= endSectorX; sectorX++){
			CSector *sector = CWorld::GetSector(sectorX, sectorY);
			if(sector == nil)
				continue;

			CPtrList *lists[5] = {
				&sector->m_lists[ENTITYLIST_VEHICLES],
				&sector->m_lists[ENTITYLIST_VEHICLES_OVERLAP],
				&sector->m_lists[ENTITYLIST_PEDS],
				&sector->m_lists[ENTITYLIST_PEDS_OVERLAP],
				&sector->m_lists[ENTITYLIST_OBJECTS],
			};
			const int32 numLists = bOnlyVehicles ? 2 : 5;

			for(int32 l = 0; l < numLists; l++){
				for(CPtrNode *node = lists[l]->first; node; node = node->next){
					CEntity *other = (CEntity*)node->item;
					if(other == nil || other == entity)
						continue;
					if(other->m_scanCode == CWorld::GetCurrentScanCode())
						continue;
					other->m_scanCode = CWorld::GetCurrentScanCode();
					other->bCollisionProcessed = false;
				}
			}
		}
	}
}
