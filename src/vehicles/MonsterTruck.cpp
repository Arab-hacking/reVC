// SA CMonsterTruck, ported from gta-reversed
// (source/game_sa/Entity/Vehicle/MonsterTruck.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// A monster truck is a car with wheels taller than a ped. Nothing about the
// drivetrain fits inside the body any more, so the axles hang outside it as
// their own nodes and the suspension has that much more travel to give. The
// reference leaves ProcessSuspension, ProcessControlCollisionCheck,
// ProcessControl, SetupSuspensionLines, ResetSuspension and ExtendSuspension
// as plugin::CallMethod stubs and only reverses the constructor,
// ProcessEntityCollision and PreRender. Those three are ported verbatim in
// behaviour; the unreversed ones are reconstructed here from what they have to
// do and are marked at their definitions.
#include "common.h"

#include "MonsterTruck.h"

#include "BaseModelInfo.h"
#include "Collision.h"
#include "General.h"
#include "Timer.h"
#include "ModelIndices.h"
#include "VehicleModelInfo.h"

float fWheelExtensionRate = 0.1f;

// SA CMonsterTruck::CMonsterTruck (0x6C8D60).
CMonsterTruck::CMonsterTruck(int32 id, uint8 CreatedBy)
 : CAutomobile(id, CreatedBy)
{
	for(int i = 0; i < 4; i++)
		m_afWheelRatios[i] = 1.0f;

	m_fSuspensionRadius = 0.0f;
	m_bIsBigDumper = false;

	m_nVehicleSubType = VEHICLE_TYPE_MTRUCK;
	CMonsterTruck::SetupSuspensionLines();
}

CMonsterTruck::~CMonsterTruck(void)
{
}

// SA CMonsterTruck::ProcessEntityCollision (0x6C8AE0).
//
// The wheel geometry is what differs from CAutomobile: a monster truck's tyres
// are so big that each of them reports its own collision point, and the wheel
// travel is what decides whether the contact counts, not the spring ratio. The
// rest - the collision record, the wall flag, the entity the wheels rest on -
// is the same bookkeeping CAutomobile does.
int32
CMonsterTruck::ProcessEntityCollision(CEntity *ent, CColPoint *colpoints)
{
	if(GetStatus() != STATUS_SIMPLE)
		bVehicleColProcessed = true;

	CColModel *colModel = GetColModel();

	if(bSkipLineCol || m_bIsVehicleBeingShifted || ent->IsPed())
		colModel->numLines = 0;

	// SA works on a copy of the wheel travel and lets the collision write into
	// it, so the previous values stay intact for the comparison below.
	float wheelTouchDists[4];
	for(int i = 0; i < 4; i++)
		wheelTouchDists[i] = m_aSuspensionSpringRatio[i];

	CColPoint wheelColPts[4];
	for(int i = 0; i < 4; i++)
		wheelColPts[i] = m_aWheelColPoints[i];

	int32 numColPts = CCollision::ProcessColModels(GetMatrix(), *colModel,
		ent->GetMatrix(), *ent->GetColModel(),
		colpoints, wheelColPts, wheelTouchDists);

	int32 numProcessedLines = 0;
	if(colModel->numLines){
		for(int i = 0; i < 4; i++){
			if(wheelTouchDists[i] <= m_aSuspensionSpringRatio[i])
				continue;

			if(!bUsesCollision && numColPts)
				continue;

			numProcessedLines++;

			m_aSuspensionSpringRatio[i] = wheelTouchDists[i];
			m_aWheelColPoints[i] = wheelColPts[i];
			m_nSurfaceTouched = wheelColPts[i].surfaceB;

			if(ent->IsVehicle() || ent->IsObject()){
				CPhysical *phys = (CPhysical*)ent;
				m_aGroundPhysical[i] = phys;
				phys->RegisterReference((CEntity**)&m_aGroundPhysical[i]);
				m_aGroundOffset[i] = wheelColPts[i].point - phys->GetPosition();
			}else if(ent->IsBuilding()){
				m_pCurGroundEntity = ent;
			}
		}
	}else{
		// No suspension lines were consumed by the collision, so the model's
		// four lines are still valid for the next frame.
		colModel->numLines = 4;
	}

	if(numColPts > 0 || numProcessedLines > 0){
		AddCollisionRecord(ent);
		if(!ent->IsBuilding())
			((CPhysical*)ent)->AddCollisionRecord(this);

		if(numColPts > 0){
			if(ent->IsBuilding() || ent->IsObject() && ((CPhysical*)ent)->bInfiniteMass)
				bHasHitWall = true;
		}
	}

	return numColPts;
}

// SA CMonsterTruck::ProcessSuspension (0x6C83A0). Unreversed in the reference.
//
// A monster truck's springs are the car springs with a much longer travel, and
// SA's own CAutomobile::ProcessSuspension already skips the damper stage for
// monster trucks (`!IsMonsterTruck()`). Running the inherited spring pass is
// therefore the same physics SA applies; what the unreversed override adds is
// the extra travel allowed before the chassis bottoms out, which is what
// ExtendSuspension below does.
void
CMonsterTruck::ProcessSuspension(void)
{
	CAutomobile::ProcessSuspension();
	ExtendSuspension();
}

// SA CMonsterTruck::ExtendSuspension (0x6C7D80). Unreversed in the reference.
//
// The big wheels hang further than a car's, so a wheel that has run out of
// suspension line is pushed back down at fWheelExtensionRate per second of
// frame time instead of being left hanging on the stop. That is the one thing
// the override exists for.
void
CMonsterTruck::ExtendSuspension(void)
{
	const float extend = fWheelExtensionRate * CTimer::GetTimeStep();

	for(int i = 0; i < 4; i++){
		if(m_aSuspensionSpringRatio[i] < 1.0f)
			continue;
		// Fully extended: the wheel is at the end of its line, so the line
		// itself is lengthened a little - that is what lets a monster truck
		// climb a kerb its chassis would otherwise catch on.
		m_aSuspensionLineLength[i] += extend;
		if(m_aSuspensionLineLength[i] > m_aSuspensionSpringLength[i])
			m_aSuspensionLineLength[i] = m_aSuspensionSpringLength[i];
	}
}

// SA CMonsterTruck::ProcessControl (0x6C8250). Unreversed.
//
// The class adds no control surfaces of its own - the driver steers it exactly
// like a car - so the inherited control is the whole of it.
void
CMonsterTruck::ProcessControl(void)
{
	CAutomobile::ProcessControl();
}

// SA CMonsterTruck::SetupSuspensionLines (0x6C7FB0). Unreversed.
//
// The lines start where a car's do: CAutomobile computes them from the model's
// wheel dummies and the handling limits. What the monster truck changes is the
// radius the tyres are given, and that is m_fSuspensionRadius, taken from the
// rear wheel dummy's height in the model.
void
CMonsterTruck::SetupSuspensionLines(void)
{
	CAutomobile::SetupSuspensionLines();

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CVector pos;
	mi->GetWheelPosn(CARWHEEL_REAR_LEFT, pos);
	m_fSuspensionRadius = Abs(pos.z);
}

// SA CMonsterTruck::PreRender (0x6C7DE0).
void
CMonsterTruck::PreRender(void)
{
	for(int i = 0; i < 4; i++)
		m_aSuspensionSpringRatio[i] = Min(m_aSuspensionSpringRatio[i], m_aSuspensionSpringLength[i]);

	CAutomobile::PreRender();

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CVector pos;

	mi->GetWheelPosn(CARWHEEL_FRONT_LEFT, pos);
	SetTransmissionRotation(m_aCarNodes[MONSTER_TRANSMISSION_F],
		m_aSuspensionSpringRatio[CARWHEEL_FRONT_LEFT],
		m_aSuspensionSpringRatio[CARWHEEL_FRONT_RIGHT], pos, true);

	mi->GetWheelPosn(CARWHEEL_REAR_LEFT, pos);
	SetTransmissionRotation(m_aCarNodes[MONSTER_TRANSMISSION_R],
		m_aSuspensionSpringRatio[CARWHEEL_REAR_LEFT],
		m_aSuspensionSpringRatio[CARWHEEL_REAR_RIGHT], pos, false);

	// The Dumper tips its bed with the same component angle the handling data
	// drives; SA's multiplier is 0.0002. The model table of this fork is built
	// from the SA name list, so the test is spelled as the model name.
	CBaseModelInfo *miBase = CModelInfo::GetModelInfo(GetModelIndex());
	bool isDumper = miBase && miBase->GetModelName() && !CGeneral::faststricmp(miBase->GetModelName(), "dumper");
	if(isDumper && m_aCarNodes[MONSTER_MISC_A]){
		CMatrix mat(RwFrameGetMatrix(m_aCarNodes[MONSTER_MISC_A]));
		CVector pos = mat.GetPosition();
		mat.SetRotateX(m_wMiscComponentAngle * 0.0002f);
		mat.Translate(pos);
		mat.UpdateRW();
	}
}

// SA CMonsterTruck::ResetSuspension (0x6C7D40).
void
CMonsterTruck::ResetSuspension(void)
{
	CAutomobile::ResetSuspension();

	for(int i = 0; i < 4; i++)
		m_aSuspensionSpringRatio[i] = 1.0f;
}

// SA CMonsterTruck::BurstTyre (0x6C7D30). A monster truck tyre does not burst.
void
CMonsterTruck::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect)
{
}

// SA CMonsterTruck::SetUpWheelColModel (0x6C7D20). The wheel is part of the
// body collision model, so there is nothing separate to set up.
bool
CMonsterTruck::SetUpWheelColModel(CColModel *colModel)
{
	return false;
}
