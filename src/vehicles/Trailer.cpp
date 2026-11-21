// SA CTrailer, ported from gta-reversed
// (source/game_sa/Entity/Vehicle/Trailer.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// A trailer is the only vehicle class whose physics comes in two halves. While
// it rolls it is a car. Once it has been left standing it puts two support legs
// down at the rear so it does not tip; those legs have their own collision
// lines, their own spring force and their own ratio, which is why the class
// carries m_aSupportRatios and m_aSupportColPoints alongside the wheels.
//
// The reference reverses every method this file ports, with one exception noted
// below. The one piece that could not be taken verbatim is the wheel pass in
// ProcessSuspension: the reference calls CAutomobile::ProcessCarWheelPair,
// which this fork does not have as a separate entry point (its wheel handling
// is inline in CAutomobile::ProcessControl). The trailer's wheels therefore go
// through CVehicle::ProcessWheel with the same arguments that pair call would
// have used, and the substitution is called out at the site.
#include "common.h"

#include "Trailer.h"

#include "BaseModelInfo.h"
#include "Collision.h"
#include "General.h"
#include "ModelIndices.h"
#include "Timer.h"
#include "VehicleModelInfo.h"
#include "World.h"

// 0x8D346C / 0x871C14 / 0x871C18 / 0x871C1C / 0x871C20 / 0x8D3470 / 0x8D3474
static const float TRAILER_TOWED_MINRATIO = 0.9f;
static const float TRAILER_SUPPORT_RETRACTION_RATE = 0.002f;
static const float TRAILER_SUPPORT_EXTENSION_RATE = 0.002f;
static const float TRAILER_SUPPORT_WAIT_EXTENSION_RATE = 0.0005f;
static const float TRAILER_TOWBAR_OFFSET_Y = -0.05f;
static const float RELINK_TRAILER_DIFF_LIMIT_XY = 0.4f;
static const float RELINK_TRAILER_DIFF_LIMIT_Z = 1.0f;

// SA Automobile.h: a suspension line that reports a touch distance at or past
// this never hit anything.
static const float BILLS_EXTENSION_LIMIT = 1.0f;

// SA CTrailer::CTrailer (0x6D03A0).
CTrailer::CTrailer(int32 id, uint8 CreatedBy)
 : CAutomobile(id, CreatedBy)
{
	m_nVehicleSubType = VEHICLE_TYPE_TRAILER;

	m_aSupportRatios[0] = 1.0f;
	m_aSupportRatios[1] = 1.0f;
	m_fHeight = 0.0f;
	m_fTrailerTowedRatio = 1.0f;
	m_fTrailerTowedRatio2 = 1.0f;

	// The two luggage trailers have no support legs at all; the sentinel tells
	// every support code path to stand down with a single compare.
	CBaseModelInfo *bmi = CModelInfo::GetModelInfo(id);
	const char *name = bmi ? bmi->GetModelName() : nil;
	if(name && (!CGeneral::faststricmp(name, "bagboxa") || !CGeneral::faststricmp(name, "bagboxb")))
		m_fTrailerTowedRatio = BAGGAGE_TRAILER_TOWED_RATIO;

	SetupSuspensionLines();
	SetStatus(STATUS_ABANDONED);
}

CTrailer::~CTrailer(void)
{
}

// SA CTrailer::SetupSuspensionLines (0x6CF1A0).
//
// The wheels get their lines exactly where a car's do, but the trailer needs
// two more: one under each support leg. Those start at the rear top of the
// collision box and run down to its bottom, and the trailer rests on them when
// it is not hitched.
void
CTrailer::SetupSuspensionLines(void)
{
	if(m_fTrailerTowedRatio == BAGGAGE_TRAILER_TOWED_RATIO){
		CAutomobile::SetupSuspensionLines();
		return;
	}

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CColModel *colModel = GetColModel();
	if(colModel == nil){
		CAutomobile::SetupSuspensionLines();
		return;
	}

	// The model's four car lines are reused for the wheels; the two support
	// lines are added after them. The collision model is shared with the model
	// info of this vehicle, so it has to be grown, not replaced.
	if(colModel->lines == nil){
		colModel->numLines = NUM_TRAILER_SUSP_LINES;
		colModel->lines = (CColLine*)RwMalloc(sizeof(CColLine)*NUM_TRAILER_SUSP_LINES);
		for(int i = 0; i < NUM_TRAILER_SUSP_LINES; i++){
			colModel->lines[i].p0 = CVector(0.0f, 0.0f, 0.0f);
			colModel->lines[i].p1 = CVector(0.0f, 0.0f, 0.0f);
		}
	}

	for(int i = 0; i < NUM_TRAILER_WHEELS; i++){
		CVector wheelPos;
		mi->GetWheelPosn(i, wheelPos);
		m_aWheelPosition[i] = wheelPos.z;

		CVector lineStart = wheelPos;
		lineStart.z += m_pHandlingData->m_fSuspensionUpperLimit;
		colModel->lines[i].p0 = lineStart;

		CVector lineEnd = lineStart;
		lineEnd.z += m_pHandlingData->m_fSuspensionLowerLimit
			- m_pHandlingData->m_fSuspensionUpperLimit
			- mi->m_wheelScale*0.5f;
		colModel->lines[i].p1 = lineEnd;

		m_aSuspensionSpringLength[i] = m_pHandlingData->m_fSuspensionUpperLimit
			- m_pHandlingData->m_fSuspensionLowerLimit;
		m_aSuspensionLineLength[i] = colModel->lines[i].p0.z - colModel->lines[i].p1.z;
	}

	colModel->boundingBox.min.z = Min(colModel->boundingBox.min.z, colModel->lines[0].p1.z);

	// The support leg tops out where the model's own leg node sits, composed
	// through its parents - which is the frame's absolute Z.
	if(m_aCarNodes[TRAILER_MISC_A]){
		RwFrame *wheelFront = m_aCarNodes[TRAILER_MISC_A];
		m_fHeight = RwFrameGetMatrix(wheelFront)->pos.z;
	}

	CColLine *supportLines = &colModel->lines[NUM_TRAILER_WHEELS];
	for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
		CVector startPos;
		startPos.x = (i == 0) ? colModel->boundingBox.min.x : -colModel->boundingBox.min.x;
		startPos.y = colModel->boundingBox.max.y;
		startPos.z = colModel->boundingBox.max.z;
		supportLines[i].p0 = startPos;

		CVector endPos = startPos;
		endPos.z = colModel->boundingBox.min.z;
		supportLines[i].p1 = endPos;
	}

	// How much of the trailer's weight the legs take versus the wheels: the
	// further back the legs are from the average wheel, the more they carry,
	// and the springs have to hold the body up that much less.
	const float supportLegY = Abs(supportLines[0].p0.y);
	const float avgWheelY = (Abs(colModel->lines[0].p0.y) + Abs(colModel->lines[1].p0.y))*0.5f;
	const float weightRatio = supportLegY/(avgWheelY + supportLegY);

	const float suspensionForceFactor = m_pHandlingData->m_fSuspensionForceLevel*4.0f;
	const float heightAboveRoad = m_aSuspensionSpringLength[0]*(1.0f - weightRatio/suspensionForceFactor)
		+ mi->m_wheelScale*0.5f - colModel->lines[0].p0.z;

	m_fHeightAboveRoad = heightAboveRoad;

	// The leg line is shortened so that the leg's own spring force holds the
	// body at exactly heightAboveRoad; the rest of the line is travel.
	const float supportForce = CTrailer::m_fTrailerSuspensionForce*2.0f;
	const float newEndZ = supportLines[0].p0.z - (supportLines[0].p0.z + heightAboveRoad)
		/(1.0f - (1.0f - weightRatio)/supportForce);

	for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
		supportLines[i].p1.z = newEndZ;
		m_aWheelPosition[i] = mi->m_wheelScale*0.5f - m_fHeightAboveRoad;
	}
}

// SA CTrailer::SetTowLink (0x6CFDF0).
bool
CTrailer::SetTowLink(CVehicle *vehicle, bool setMyPosToTowBar)
{
	if(vehicle == nil)
		return false;

	eEntityStatus status = (eEntityStatus)GetStatus();
	if(status != STATUS_PHYSICS && status != STATUS_IS_TOWED && status != STATUS_ABANDONED)
		return false;

	SetStatus(STATUS_IS_TOWED);

	m_pTowingVehicle = vehicle;
	m_pTowingVehicle->RegisterReference((CEntity**)&m_pTowingVehicle);

	vehicle->m_pVehicleBeingTowed = this;
	vehicle->m_pVehicleBeingTowed->RegisterReference((CEntity**)&vehicle->m_pVehicleBeingTowed);

	RemoveFromMovingList();
	vehicle->RemoveFromMovingList();
	AddToMovingList();
	vehicle->AddToMovingList();

	if(!setMyPosToTowBar){
		UpdateTractorLink(true, false);
		return true;
	}

	// Snapping the trailer onto the bar: line the hitch up with the bar and put
	// the whole thing down on the road.
	m_fTrailerTowedRatio = Clamp(m_fTrailerTowedRatio, 0.0f, 1.0f);

	SetHeading(GetHeading());

	CVector towBarPos, towHitchPos;
	if(!GetTowHitchPos(towHitchPos, true, this) || !vehicle->GetTowBarPos(towBarPos, true, this))
		return false;

	towHitchPos -= GetPosition();

	SetPosition(towBarPos - towHitchPos);
	m_vecMoveSpeed = vehicle->m_vecMoveSpeed;
	PlaceOnRoadProperly();

	return true;
}

// SA CTrailer::ScanForTowLink (0x6CF030).
//
// A standing trailer looks around for a bar it can hook onto. Anything within
// ten metres whose bar lines up with the hitch to within a few centimetres in
// the plane and one metre in height is taken.
void
CTrailer::ScanForTowLink(void)
{
	if(m_pTowingVehicle)
		return;

	CBaseModelInfo *bmi = CModelInfo::GetModelInfo(GetModelIndex());
	const char *name = bmi ? bmi->GetModelName() : nil;
	if(name && !CGeneral::faststricmp(name, "farmtr1"))
		return;

	CVector towHitchPos;
	if(!GetTowHitchPos(towHitchPos, false, this))
		return;

	int16 count = 0;
	CEntity *objects[16];
	CWorld::FindObjectsInRange(towHitchPos, 10.0f, true, &count, 16, objects, false, true, false, false, false);

	for(int i = 0; i < count; i++){
		CEntity *ent = objects[i];
		if(ent == nil || !ent->IsVehicle())
			continue;

		CVehicle *vehicle = (CVehicle*)ent;
		if(vehicle == this)
			continue;

		CVector towBarPos;
		if(!vehicle->GetTowBarPos(towBarPos, false, this))
			continue;

		CVector2D diff = towBarPos - towHitchPos;
		if(diff.Magnitude() < RELINK_TRAILER_DIFF_LIMIT_XY &&
		   Abs(towHitchPos.z - towBarPos.z) < RELINK_TRAILER_DIFF_LIMIT_Z){
			SetTowLink(vehicle, false);
			return;
		}
	}
}

// SA CTrailer::ResetSuspension (0x6CEE50).
void
CTrailer::ResetSuspension(void)
{
	CAutomobile::ResetSuspension();

	m_aSupportRatios[0] = 1.0f;
	m_aSupportRatios[1] = 1.0f;

	if(m_pTowingVehicle && m_fTrailerTowedRatio > BAGGAGE_TRAILER_TOWED_RATIO)
		m_fTrailerTowedRatio = 0.0f;
	else
		m_fTrailerTowedRatio = 1.0f;
}

// SA CTrailer::ProcessSuspension (0x6CF6A0).
void
CTrailer::ProcessSuspension(void)
{
	CAutomobile::ProcessSuspension();

	if(m_fTrailerTowedRatio == BAGGAGE_TRAILER_TOWED_RATIO)
		return;

	CVector contactPoints[NUM_TRAILER_SUPPORTS];
	CVector contactSpeeds[NUM_TRAILER_SUPPORTS];
	float impulseMagnitudes[NUM_TRAILER_SUPPORTS];

	for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
		contactPoints[i] = CVector(0.0f, 0.0f, 0.0f);
		contactSpeeds[i] = CVector(0.0f, 0.0f, 0.0f);
		impulseMagnitudes[i] = 0.0f;
	}

	for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
		if(m_aSupportRatios[i] < m_fTrailerTowedRatio2 && m_fTrailerTowedRatio2 > 0.1f){
			CVector forceDir = -GetUp();

			contactPoints[i] = m_aSupportColPoints[i].point - GetPosition();

			const float compression = m_aSupportRatios[i]/m_fTrailerTowedRatio2;

			ApplySpringCollisionAlt(m_fTrailerSuspensionForce, forceDir, contactPoints[i],
				compression, 1.0f, m_aSupportColPoints[i].normal, impulseMagnitudes[i]);

			contactSpeeds[i] = GetSpeed(contactPoints[i]);

			CVector dampeningDir = m_aSupportColPoints[i].normal.z > 0.35f
				? -m_aSupportColPoints[i].normal
				: -GetUp();

			ApplySpringDampening(m_fTrailerDampingForce, impulseMagnitudes[i], dampeningDir,
				contactPoints[i], contactSpeeds[i]);
		}
	}

	eEntityStatus status = (eEntityStatus)GetStatus();
	if(status != STATUS_IS_TOWED && status != STATUS_IS_SIMPLE_TOWED){
		uint32 originalWheelStatus[NUM_TRAILER_SUPPORTS];
		for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
			originalWheelStatus[i] = Damage.GetWheelStatus(i);

			if(m_aSupportRatios[i] >= m_fTrailerTowedRatio2 || m_fTrailerTowedRatio2 <= 0.1f)
				Damage.SetWheelStatus(i, WHEEL_STATUS_BURST);
			else{
				contactSpeeds[i] = GetSpeed(contactPoints[i]);
				Damage.SetWheelStatus(i, WHEEL_STATUS_OK);
			}
		}

		// The support legs carry the same traction model the wheels do. SA
		// runs the left pair through CAutomobile::ProcessCarWheelPair; this
		// fork keeps that pass inline in CAutomobile::ProcessControl, so the
		// two wheels are driven through CVehicle::ProcessWheel with the
		// arguments the pair call would have built.
		const float traction = m_pHandlingData->m_fTractionMultiplier*0.004f*0.25f;

		CVector wheelFwd = GetForward();
		CVector wheelRight = GetRight();
		float wheelSpeedUnused0 = 0.0f, wheelSpeedUnused1 = 0.0f;
		ProcessWheel(wheelFwd, wheelRight, contactSpeeds[0], contactPoints[0],
			1, 0.0f, 0.0f, traction, CARWHEEL_FRONT_LEFT, wheelSpeedUnused0,
			m_aWheelState[CARWHEEL_FRONT_LEFT], WHEEL_STATUS_OK);
		ProcessWheel(wheelFwd, wheelRight, contactSpeeds[1], contactPoints[1],
			1, 0.0f, 0.0f, traction, CARWHEEL_REAR_LEFT, wheelSpeedUnused1,
			m_aWheelState[CARWHEEL_REAR_LEFT], WHEEL_STATUS_OK);

		Damage.SetWheelStatus(CARWHEEL_FRONT_LEFT, originalWheelStatus[0]);
		Damage.SetWheelStatus(CARWHEEL_REAR_LEFT, originalWheelStatus[1]);
	}
}

// SA CTrailer::ProcessEntityCollision (0x6CFFD0).
//
// The difference from a car is that a trailer has six suspension lines rather
// than four, and that it must never collide with whatever it is hitched to.
int32
CTrailer::ProcessEntityCollision(CEntity *entity, CColPoint *outColPoints)
{
	if(m_fTrailerTowedRatio == BAGGAGE_TRAILER_TOWED_RATIO)
		return CAutomobile::ProcessEntityCollision(entity, outColPoints);

	if(GetStatus() != STATUS_SIMPLE)
		bVehicleColProcessed = true;

	CColModel *tcd = GetColModel();
	CColModel *ocd = entity->GetColModel();

	if(tcd == nil || ocd == nil)
		return 0;

	if(bSkipLineCol || m_bIsVehicleBeingShifted || entity->IsPed() || entity->IsVehicle())
		tcd->numLines = 0;

	// Never collide with the thing on the other end of the coupling.
	const bool shouldHideTriangles = m_pTowingVehicle == entity || m_pVehicleBeingTowed == entity;
	const int16 originalTNumTri = tcd->numTriangles;
	const int16 originalONumTri = ocd->numTriangles;
	if(shouldHideTriangles)
		tcd->numTriangles = ocd->numTriangles = 0;

	CColPoint suspLineCPs[NUM_TRAILER_SUSP_LINES];
	float suspLineTouchDists[NUM_TRAILER_SUSP_LINES];
	for(int i = 0; i < NUM_TRAILER_SUSP_LINES; i++){
		suspLineTouchDists[i] = 1.0f;
		if(i < NUM_TRAILER_WHEELS)
			suspLineTouchDists[i] = m_aSuspensionSpringRatio[i];
		else
			suspLineTouchDists[i] = m_aSupportRatios[i - NUM_TRAILER_WHEELS];
	}

	int32 numColPts = CCollision::ProcessColModels(GetMatrix(), *tcd,
		entity->GetMatrix(), *ocd,
		outColPoints, suspLineCPs, suspLineTouchDists);

	if(shouldHideTriangles){
		tcd->numTriangles = originalTNumTri;
		ocd->numTriangles = originalONumTri;
	}

	int32 numProcessedLines = 0;
	if(tcd->numLines){
		for(int i = 0; i < NUM_TRAILER_WHEELS; i++){
			const CColPoint &cp = suspLineCPs[i];
			const float touchDist = suspLineTouchDists[i];
			if(touchDist < BILLS_EXTENSION_LIMIT && touchDist < m_aSuspensionSpringRatio[i]){
				numProcessedLines++;
				m_aSuspensionSpringRatio[i] = touchDist;
				m_aWheelColPoints[i] = cp;
				m_nSurfaceTouched = cp.surfaceB;

				if(entity->IsVehicle() || entity->IsObject()){
					CPhysical *phys = (CPhysical*)entity;
					m_aGroundPhysical[i] = phys;
					phys->RegisterReference((CEntity**)&m_aGroundPhysical[i]);
					m_aGroundOffset[i] = cp.point - phys->GetPosition();
				}else if(entity->IsBuilding()){
					m_pCurGroundEntity = entity;
				}
			}
		}

		for(int i = 0; i < NUM_TRAILER_SUPPORTS; i++){
			const int32 suspLineIdx = NUM_TRAILER_WHEELS + i;

			const float touchDist = suspLineTouchDists[suspLineIdx];
			if(touchDist >= BILLS_EXTENSION_LIMIT || touchDist >= m_aSupportRatios[i])
				continue;

			numProcessedLines++;
			m_aSupportRatios[i] = touchDist;
			m_aSupportColPoints[i] = suspLineCPs[suspLineIdx];
		}
	}else{
		tcd->numLines = NUM_TRAILER_SUSP_LINES;
	}

	if(numColPts > 0 || numProcessedLines > 0){
		AddCollisionRecord(entity);
		if(!entity->IsBuilding())
			((CPhysical*)entity)->AddCollisionRecord(this);

		if(numColPts > 0 && entity->IsBuilding() ||
		   (entity->IsObject() && ((CPhysical*)entity)->bInfiniteMass))
			bHasHitWall = true;
	}

	return numColPts;
}

// SA CTrailer::ProcessControl (0x6CED20).
//
// The support legs are what this adds to a car's control: they retract while
// the trailer is hitched and come back down after it is released.
void
CTrailer::ProcessControl(void)
{
	CAutomobile::ProcessControl();

	if(m_fTrailerTowedRatio == BAGGAGE_TRAILER_TOWED_RATIO)
		return;

	const float timeStep = CTimer::GetTimeStep();
	float currentExtension = m_fTrailerTowedRatio;

	if(m_pTowingVehicle && currentExtension > 0.0f){
		// Hitched: the legs come up.
		currentExtension = Max(currentExtension - timeStep*TRAILER_SUPPORT_RETRACTION_RATE, 0.0f);
	}else if(m_pTowingVehicle == nil && currentExtension < 1.0f){
		// Released: they go down, slowly at first so the trailer settles.
		const float growthRate = (currentExtension <= 0.1f)
			? timeStep*TRAILER_SUPPORT_WAIT_EXTENSION_RATE
			: timeStep*TRAILER_SUPPORT_EXTENSION_RATE;
		currentExtension = Min(currentExtension + growthRate, 1.0f);
	}
	m_fTrailerTowedRatio = currentExtension;

	// While the legs are still coming down they are too short to hold the
	// trailer; below TRAILER_TOWED_MINRATIO they are treated as fully out.
	eEntityStatus status = (eEntityStatus)GetStatus();
	if((status == STATUS_IS_TOWED || status == STATUS_IS_SIMPLE_TOWED) &&
	   TRAILER_TOWED_MINRATIO > currentExtension)
		m_fTrailerTowedRatio2 = TRAILER_TOWED_MINRATIO;
	else
		m_fTrailerTowedRatio2 = currentExtension;

	m_aSupportRatios[0] = m_fTrailerTowedRatio2;
	m_aSupportRatios[1] = m_fTrailerTowedRatio2;
}

// SA CTrailer::ProcessAI (0x6CF590).
bool
CTrailer::ProcessAI(uint32 &extraHandlingFlags)
{
	bool returnValue = CAutomobile::ProcessAI(extraHandlingFlags);

	// An abandoned trailer looks for something to hook onto.
	if(GetStatus() == STATUS_ABANDONED &&
	   (m_fTrailerTowedRatio > 0.5f || m_fTrailerTowedRatio == BAGGAGE_TRAILER_TOWED_RATIO)){
		ScanForTowLink();
		return returnValue;
	}

	if(GetStatus() != STATUS_REMOTE_CONTROLLED || m_pTowingVehicle == nil)
		return returnValue;

	if(m_fTrailerTowedRatio != BAGGAGE_TRAILER_TOWED_RATIO){
		m_fBrakePedal = m_pTowingVehicle->m_fBrakePedal;
		return returnValue;
	}

	// A luggage trailer steers itself to follow its tractor.
	float headingTow = m_pTowingVehicle->GetHeading();
	float headingSelf = GetHeading();

	float angleDiff = headingTow - headingSelf;
	if(angleDiff > PI)
		angleDiff -= TWOPI;
	else if(angleDiff < -PI)
		angleDiff += TWOPI;
	m_fSteerAngle = angleDiff;

	return returnValue;
}

// SA CTrailer::PreRender (0x6CFAC0).
void
CTrailer::PreRender(void)
{
	CAutomobile::PreRender();

	// The support leg mesh slides down its line as the leg extends.
	if(m_aCarNodes[TRAILER_MISC_A]){
		CColModel *colModel = GetColModel();
		if(colModel && colModel->numLines > NUM_TRAILER_WHEELS &&
		   (!m_vecMoveSpeed.IsZero() || !m_vecTurnSpeed.IsZero() || m_nStaticFrames == 0)){
			CMatrix mat(RwFrameGetMatrix(m_aCarNodes[TRAILER_MISC_A]));

			const float avgSupportRatio = (m_aSupportRatios[0] + m_aSupportRatios[1])*0.5f;
			const float supportExtensionRatio = Min(avgSupportRatio, m_fTrailerTowedRatio);

			const CColLine &supportLegLine = colModel->lines[NUM_TRAILER_WHEELS];

			const float supportTravel = supportExtensionRatio*(supportLegLine.p1.z - supportLegLine.p0.z);
			const float verticalPosRatio = mat.GetPosition().y/supportLegLine.p0.y;

			CVector newPos = mat.GetPosition();
			const float targetZ = (supportTravel + supportLegLine.p0.z)*verticalPosRatio
				- (1.0f - verticalPosRatio)*m_fHeightAboveRoad;
			newPos.z = Min(targetZ, m_fHeight);

			mat.SetTranslate(newPos);
			mat.UpdateRW();
		}
	}

	// The harvester trailer's reel turns with its forward speed.
	CBaseModelInfo *bmi = CModelInfo::GetModelInfo(GetModelIndex());
	const char *name = bmi ? bmi->GetModelName() : nil;
	if(name && !CGeneral::faststricmp(name, "farmtr1")){
		CVector pos = RwFrameGetMatrix(m_aCarNodes[TRAILER_MISC_B])
			? RwFrameGetMatrix(m_aCarNodes[TRAILER_MISC_B])->pos : CVector(0.0f, 0.0f, 0.0f);
		(void)pos;
		if(m_aCarNodes[TRAILER_MISC_B]){
			m_fPropRotate = fmodf(m_fPropRotate, TWOPI);
			const float forwardSpeed = DotProduct(m_vecMoveSpeed, GetForward());
			const float rotationSpeedFactor = Clamp(forwardSpeed, 0.0f, 0.1f);
			m_fPropRotate -= rotationSpeedFactor*CTimer::GetTimeStep();

			CMatrix mat(RwFrameGetMatrix(m_aCarNodes[TRAILER_MISC_B]));
			CVector mpos = mat.GetPosition();
			mat.SetRotateX(m_fPropRotate);
			mat.Translate(mpos);
			mat.UpdateRW();
		}
	}
}

// SA CTrailer::GetTowHitchPos (0x6CEEA0).
bool
CTrailer::GetTowHitchPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	outPos = mi->m_vehicleStruct.m_avDummyPos[DUMMY_TRAILER_ATTACH];

	// A tow truck cannot be a trailer's tractor - it hooks, it is not hooked.
	if(vehicle){
		CBaseModelInfo *vbmi = CModelInfo::GetModelInfo(vehicle->GetModelIndex());
		const char *vname = vbmi ? vbmi->GetModelName() : nil;
		if(vname && !CGeneral::faststricmp(vname, "towtruck"))
			return false;
	}

	if(outPos.x == 0.0f && outPos.y == 0.0f && outPos.z == 0.0f){
		if(!checkModelInfo)
			return false;
		CColModel *colModel = GetColModel();
		if(colModel == nil)
			return false;
		outPos.x = 0.0f;
		outPos.y = colModel->boundingBox.max.y + 1.0f;
		outPos.z = 0.5f - m_fHeightAboveRoad;
		outPos = Multiply3x3(GetMatrix(), outPos) + GetPosition();
		return true;
	}
	outPos = Multiply3x3(GetMatrix(), outPos) + GetPosition();
	return true;
}

// SA CTrailer::GetTowBarPos (0x6CFD60).
bool
CTrailer::GetTowBarPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle)
{
	if(!checkModelInfo){
		CBaseModelInfo *bmi = CModelInfo::GetModelInfo(GetModelIndex());
		const char *name = bmi ? bmi->GetModelName() : nil;
		if(name && CGeneral::faststricmp(name, "bagboxa") && CGeneral::faststricmp(name, "bagboxb"))
			return false;
	}

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CColModel *colModel = GetColModel();
	if(colModel == nil)
		return false;

	outPos.x = 0.0f;
	outPos.y = colModel->boundingBox.min.y - TRAILER_TOWBAR_OFFSET_Y;
	outPos.z = 0.5f - m_fHeightAboveRoad;
	outPos = Multiply3x3(GetMatrix(), outPos) + GetPosition();
	return true;
}

// SA CTrailer::BreakTowLink (0x6CEFB0).
bool
CTrailer::BreakTowLink(void)
{
	if(m_pTowingVehicle){
		m_pTowingVehicle->m_pVehicleBeingTowed = nil;
		m_pTowingVehicle = nil;
	}

	eEntityStatus status = (eEntityStatus)GetStatus();
	if(status != STATUS_IS_TOWED && status != STATUS_IS_SIMPLE_TOWED)
		return false;

	SetStatus(STATUS_ABANDONED);
	return true;
}
