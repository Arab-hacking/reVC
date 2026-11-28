#include "common.h"

#include "TaskComplexTurnToFaceEntityOrCoord.h"

#include "Entity.h"
#include "General.h"
#include "Ped.h"
#include "TaskSimpleAchieveHeading.h"

// SA CTaskComplexTurnToFaceEntityOrCoord (0x66B890).

CTaskComplexTurnToFaceEntityOrCoord::CTaskComplexTurnToFaceEntityOrCoord(CEntity *entity, float changeRateMult, float maxHeading)
: m_EntityToFace(entity), m_bFaceEntity(true), m_fChangeRateMult(changeRateMult), m_fMaxHeading(maxHeading)
{
	m_CoordsToFace = CVector(0.0f, 0.0f, 0.0f);
	if(m_EntityToFace)
		m_EntityToFace->RegisterReference(&m_EntityToFace);
}

CTaskComplexTurnToFaceEntityOrCoord::CTaskComplexTurnToFaceEntityOrCoord(const CVector &coords, float changeRateMult, float maxHeading)
: m_EntityToFace(nil), m_bFaceEntity(false), m_CoordsToFace(coords), m_fChangeRateMult(changeRateMult), m_fMaxHeading(maxHeading)
{
}

CTaskComplexTurnToFaceEntityOrCoord::~CTaskComplexTurnToFaceEntityOrCoord()
{
	if(m_EntityToFace)
		m_EntityToFace->CleanUpOldReference(&m_EntityToFace);
}

// 0x66D250
CTask*
CTaskComplexTurnToFaceEntityOrCoord::Clone() const
{
	return m_bFaceEntity
		? new CTaskComplexTurnToFaceEntityOrCoord(m_EntityToFace, m_fChangeRateMult, m_fMaxHeading)
		: new CTaskComplexTurnToFaceEntityOrCoord(m_CoordsToFace, m_fChangeRateMult, m_fMaxHeading);
}

// 0x66B9D0
float
CTaskComplexTurnToFaceEntityOrCoord::ComputeTargetHeading(CPed *ped) const
{
	CVector dist = ComputeTargetPos() - ped->GetPosition();
	return CGeneral::GetRadianAngleBetweenPoints(dist.x, dist.y, 0.0f, 0.0f);
}

CVector
CTaskComplexTurnToFaceEntityOrCoord::ComputeTargetPos() const
{
	return m_bFaceEntity ? m_EntityToFace->GetPosition() : m_CoordsToFace;
}

// 0x670850
CTask*
CTaskComplexTurnToFaceEntityOrCoord::CreateFirstSubTask(CPed *ped)
{
	return m_bFaceEntity && !m_EntityToFace
		? nil
		: new CTaskSimpleAchieveHeading(ComputeTargetHeading(ped), m_fChangeRateMult, m_fMaxHeading);
}

// 0x670920
CTask*
CTaskComplexTurnToFaceEntityOrCoord::ControlSubTask(CPed *ped)
{
	if(m_bFaceEntity){
		float heading = m_EntityToFace ? ComputeTargetHeading(ped) : ped->m_fRotationCur;
		((CTaskSimpleAchieveHeading*)m_pSubTask)->SetHeading(heading, m_fMaxHeading, m_fChangeRateMult);
	}
	return m_pSubTask;
}
