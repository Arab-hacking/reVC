#include "common.h"

#include "EventGunShot.h"

#include "Entity.h"
#include "Ped.h"
#include "PedGroups.h"
#include "PedIntelligence.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Vehicle.h"
#include "Wanted.h"
#include "World.h"

// SA CEventGunShot, adapted to this codebase (see the header).

float CEventGunShot::ms_fGunShotSenseRangeForRiot2 = 0.0f;

// 0x4AC610
CEventGunShot::CEventGunShot(CEntity *entity, CVector startPoint, CVector endPoint, bool bHasNoSound)
: CEventEditableResponse()
{
	m_startPoint = startPoint;
	m_endPoint = endPoint;
	m_firedBy = entity;
	m_bHasNoSound = bHasNoSound;
	if(m_firedBy)
		m_firedBy->RegisterReference(&m_firedBy);
}

CEventGunShot::~CEventGunShot()
{
	if(m_firedBy)
		m_firedBy->CleanUpOldReference(&m_firedBy);
}

// 0x4B2CD0
bool
CEventGunShot::AffectsPed(CPed *ped)
{
	if(!m_firedBy)
		return false;

	if(m_firedBy->IsPed() && CPedGroups::AreInSameGroup(ped, (CPed*)m_firedBy))
		return false;

	if(!(ped->bInVehicle && ped->m_pMyVehicle && ped->m_pMyVehicle->pDriver)) // !ped->IsInVehicleThatHasADriver()
	{
		CWanted *playerWanted = FindPlayerPed()->Wanted();
		if(ped->m_nPedType == PEDTYPE_COP && playerWanted->GetWantedLevel() > 0){
			// SA also requires the cop to be in (or able to join) the
			// pursuit of the player; the local wanted system cannot
			// answer that yet.
			if(m_firedBy != FindPlayerPed())
				return false;
		}
		if(ped->m_fHealth > 0.0f && ped != m_firedBy && !ped->IsPlayer()){ // ped->IsAlive()
			float fGunShotRange = 45.0f;
			if(ms_fGunShotSenseRangeForRiot2 <= 0.0f){
				if(ped->CharCreatedBy == MISSION_CHAR) // ped->IsCreatedByMission()
					fGunShotRange = ped->GetIntelligence()->GetPedFOVRange();
			}else if(ped->CharCreatedBy == MISSION_CHAR){
				fGunShotRange = ms_fGunShotSenseRangeForRiot2;
			}

			const CVector &pedPos = ped->GetPosition();
			if((m_firedBy->GetPosition() - pedPos).MagnitudeSqr() <= fGunShotRange * fGunShotRange){
				if(!m_bHasNoSound)
					return true;

				if(DotProduct(m_startPoint - pedPos, ped->GetForward()) > 0.0f){
					if(CWorld::GetIsLineOfSightClear(m_startPoint, pedPos, true, true, true, true, true, false, false))
						return true;
				}
			}
		}
	}
	return false;
}

// 0x4AC810
bool
CEventGunShot::IsCriminalEvent()
{
	return m_firedBy && m_firedBy->IsPed() && ((CPed*)m_firedBy)->IsPlayer();
}

// 0x4AC780
bool
CEventGunShot::TakesPriorityOver(const CEvent &refEvent)
{
	if(refEvent.GetEventType() == GetEventType()){
		bool bIsPlayer = false;
		bool otherPedIsPlayer = false;
		const CEventGunShot *refEventGunShot = (const CEventGunShot*)&refEvent;
		if(m_firedBy && m_firedBy->IsPed())
			bIsPlayer = ((CPed*)m_firedBy)->IsPlayer();

		CPed *otherPed = refEventGunShot->m_firedBy && refEventGunShot->m_firedBy->IsPed()
			? (CPed*)refEventGunShot->m_firedBy : nil;
		if(otherPed)
			otherPedIsPlayer = otherPed->IsPlayer();

		return bIsPlayer && !otherPedIsPlayer;
	}
	return GetEventPriority() >= refEvent.GetEventPriority();
}

// 0x4B6B20
CEventEditableResponse*
CEventGunShot::CloneEditable() const
{
	return new CEventGunShot(m_firedBy, m_startPoint, m_endPoint, m_bHasNoSound);
}
