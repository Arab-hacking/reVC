#include "common.h"

#include "EventDamage.h"

#include "Crime.h"
#include "Entity.h"
#include "General.h"
#include "Ped.h"
#include "PedDamageResponseCalculator.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Vehicle.h"
#include "Wanted.h"
#include "WeaponInfo.h"

// SA CEventDamage, adapted to this codebase (see the header): VC weapon
// and crime enums, VC's CEntity proof bitfields in place of SA's
// physicalFlags/CanPhysicalBeDamaged, registered references instead of
// SA's SafeRegisterRef. The GANG2/stealth branches of AffectsPed,
// AffectsPedGroup and the damage/death animation computation are not
// ported yet.

// 0x4B33B0
CEventDamage::CEventDamage(const CEventDamage &event)
: CEventEditableResponse()
{
	From(event);
}

// 0x4AD830
CEventDamage::CEventDamage(CEntity *source, uint32 startTime, eWeaponType weaponType, ePedPieceTypes pieceHit, uint8 direction, bool bJumpedOutOfMovingCar, bool bPedInVehicle)
: CEventEditableResponse()
{
	m_pSourceEntity         = source;
	m_nStartTime            = startTime;
	m_weaponType            = weaponType;
	m_pedPieceType          = pieceHit;
	m_ucDirection           = direction;
	m_bJumpedOutOfMovingCar = bJumpedOutOfMovingCar;
	m_bFallDown             = false;
	m_bAnimAdded            = false;
	m_bWitnessedInVehicle   = bPedInVehicle;
	m_bStealthMode          = false;
	m_nAnimGroup            = ANIM_GROUP_DEFAULT;
	m_nAnimID               = ANIM_ID_NO_ANIMATION_SET;
	m_fAnimBlend            = 8.0f;
	m_fAnimSpeed            = 1.0f;

	if(m_pSourceEntity)
		m_pSourceEntity->RegisterReference(&m_pSourceEntity);
	m_bWitnessedInVehicle = true; // if we're setting this to true, then why do we have bPedInVehicle parameter in this constructor? bug?
}

CEventDamage::~CEventDamage()
{
	if(m_pSourceEntity)
		m_pSourceEntity->CleanUpOldReference(&m_pSourceEntity);
}

// 0x4B35A0 (adapted: no stealth/silenced/group branches; the proof
// bitfields stand in for SA's CanPhysicalBeDamaged)
bool
CEventDamage::AffectsPed(CPed *ped)
{
	if(m_damageResponse.m_bDamageCalculated && !m_damageResponse.m_bCheckIfAffectsPed)
		return true;

	if(ped->m_fHealth <= 0.0f) // !ped->IsAlive()
		return false;

	if(m_weaponType == WEAPONTYPE_DROWNING && !ped->bDrownsInWater)
		return false;

	if(ped->bExplosionProof && m_weaponType == WEAPONTYPE_EXPLOSION)
		return false;

	if(ped->bFireProof && (m_weaponType == WEAPONTYPE_MOLOTOV || m_weaponType == WEAPONTYPE_FLAMETHROWER))
		return false;

	if(ped->bBulletProof && CWeaponInfo::GetWeaponInfo(m_weaponType)->m_eWeaponFire == WEAPON_FIRE_INSTANT_HIT)
		return false;

	if(ped->bCollisionProof && m_weaponType == WEAPONTYPE_FALL)
		return false;

	return true;
}

// 0x4ADA90
bool
CEventDamage::IsCriminalEvent()
{
	if(m_pSourceEntity){
		if(m_pSourceEntity->IsPed())
			return ((CPed*)m_pSourceEntity)->IsPlayer();

		if(m_pSourceEntity->IsVehicle())
			return ((CVehicle*)m_pSourceEntity)->pDriver == FindPlayerPed();
	}
	return false;
}

// 0x4B3440 (VC crime types; SA's PoliceDontCareAboutCrimesAgainstPedType
// has no local counterpart, police always care here)
void
CEventDamage::ReportCriminalEvent(CPed *ped)
{
	if(!IsCriminalEvent() || !m_pSourceEntity)
		return;

	const bool bVictimCop = ped->m_nPedType == PEDTYPE_COP;
	eCrimeType crimeType;
	if(m_weaponType == WEAPONTYPE_RAMMEDBYCAR || m_weaponType == WEAPONTYPE_RUNOVERBYCAR)
		crimeType = bVictimCop ? CRIME_RUNOVER_COP : CRIME_RUNOVER_PED;
	else if(m_weaponType <= WEAPONTYPE_CHAINSAW)
		crimeType = bVictimCop ? CRIME_HIT_COP : CRIME_HIT_PED;
	else
		crimeType = bVictimCop ? CRIME_SHOOT_COP : CRIME_SHOOT_PED;

	FindPlayerPed()->Wanted()->RegisterCrime(crimeType, m_pSourceEntity->GetPosition(), (uint32)(uintptr_t)ped, false);
}

// 0x4ADB00
bool
CEventDamage::TakesPriorityOver(const CEvent &refEvent)
{
	if(refEvent.GetEventType() == EVENT_IN_WATER && m_damageResponse.m_bHealthZero && m_bAddToEventGroup
	|| refEvent.GetEventType() == EVENT_ON_FIRE && m_damageResponse.m_bHealthZero && m_bAddToEventGroup
	|| refEvent.GetEventType() == EVENT_KNOCK_OFF_BIKE && m_damageResponse.m_bHealthZero && m_bAddToEventGroup){
		return true;
	}

	if(m_pSourceEntity && m_pSourceEntity->IsPed() && ((CPed*)m_pSourceEntity)->IsPlayer() && refEvent.GetEventType() == EVENT_DAMAGE){
		if(refEvent.GetSourceEntity() == m_pSourceEntity && (!m_damageResponse.m_bHealthZero || !m_bAddToEventGroup))
			return CEvent::TakesPriorityOver(refEvent);
		return true;
	}

	if(refEvent.GetEventType() == EVENT_DAMAGE){
		if(GetSourceEntity() != refEvent.GetSourceEntity())
			return true;
	}
	return CEvent::TakesPriorityOver(refEvent);
}

// 0x4ADAE0
bool
CEventDamage::DoInformVehicleOccupants(CPed *ped)
{
	if(GetSourceEntity())
		return m_weaponType != WEAPONTYPE_DROWNING;
	return false;
}

// 0x4B5D40
CEventEditableResponse*
CEventDamage::CloneEditable() const
{
	CEventDamage *clonedEvent = new CEventDamage(*this);
	clonedEvent->m_nAnimID = m_nAnimID;
	clonedEvent->m_bStealthMode = m_bStealthMode;
	return clonedEvent;
}

// 0x4AD9C0
void
CEventDamage::From(const CEventDamage &event)
{
	m_pSourceEntity = event.m_pSourceEntity;
	if(m_pSourceEntity)
		m_pSourceEntity->RegisterReference(&m_pSourceEntity);

	m_nStartTime            = event.m_nStartTime;
	m_weaponType            = event.m_weaponType;
	m_pedPieceType          = event.m_pedPieceType;
	m_ucDirection           = event.m_ucDirection;
	m_bJumpedOutOfMovingCar = event.m_bJumpedOutOfMovingCar;
	m_bFallDown             = event.m_bFallDown;
	m_bAnimAdded            = event.m_bAnimAdded;
	m_bWitnessedInVehicle   = event.m_bWitnessedInVehicle;
	m_bStealthMode          = event.m_bStealthMode;
	m_nAnimGroup            = event.m_nAnimGroup;
	m_nAnimID               = event.m_nAnimID;
	m_fAnimBlend            = event.m_fAnimBlend;
	m_fAnimSpeed            = event.m_fAnimSpeed;
	m_damageResponse        = event.m_damageResponse;
}

// 0x4B3A20
void
CEventDamage::ProcessDamage(CPed *ped)
{
	int32 pedNode = 0;
	ComputeBodyPartToRemove(pedNode);
	if(pedNode)
		ped->RemoveBodyPart((ePedNode)pedNode, m_ucDirection);
}

// 0x4ADC10 (VC weapon types)
void
CEventDamage::ComputeBodyPartToRemove(int32 &pedNode)
{
	pedNode = 0;
	switch(m_weaponType){
	case WEAPONTYPE_UNARMED:
	case WEAPONTYPE_BRASSKNUCKLE:
	case WEAPONTYPE_GOLFCLUB:
	case WEAPONTYPE_NIGHTSTICK:
	case WEAPONTYPE_KNIFE:
	case WEAPONTYPE_BASEBALLBAT:
	case WEAPONTYPE_KATANA:
	case WEAPONTYPE_CHAINSAW:
		if(m_damageResponse.m_bForceDeath)
			pedNode = PED_NODE_HEAD;
		break;
	case WEAPONTYPE_GRENADE:
	case WEAPONTYPE_ROCKETLAUNCHER:
	case WEAPONTYPE_EXPLOSION: {
		if(m_damageResponse.m_bForceDeath){
			int32 boneArray[5] = {
				PED_NODE_LEFT_ARM,
				PED_NODE_RIGHT_LEG,
				PED_NODE_HEAD,
				PED_NODE_RIGHT_ARM,
				PED_NODE_LEFT_LEG
			};
			pedNode = boneArray[CGeneral::GetRandomNumberInRange(0, 5)];
		}
		break;
	}
	case WEAPONTYPE_COLT45:
	case WEAPONTYPE_PYTHON:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_SPAS12_SHOTGUN:
	case WEAPONTYPE_STUBBY_SHOTGUN:
	case WEAPONTYPE_TEC9:
	case WEAPONTYPE_UZI:
	case WEAPONTYPE_SILENCED_INGRAM:
	case WEAPONTYPE_MP5:
	case WEAPONTYPE_M4:
	case WEAPONTYPE_RUGER:
	case WEAPONTYPE_SNIPERRIFLE:
	case WEAPONTYPE_LASERSCOPE:
	case WEAPONTYPE_M60:
	case WEAPONTYPE_MINIGUN:
		if(m_damageResponse.m_bForceDeath){
			switch(m_pedPieceType){
			case PED_PIECE_LEFT_ARM:
				pedNode = PED_NODE_LEFT_ARM;
				break;
			case PED_PIECE_RIGHT_ARM:
				pedNode = PED_NODE_RIGHT_ARM;
				break;
			case PED_PIECE_LEFT_LEG:
				pedNode = PED_NODE_LEFT_LEG;
				break;
			case PED_PIECE_RIGHT_LEG:
				pedNode = PED_NODE_RIGHT_LEG;
				break;
			case PED_PIECE_HEAD:
				pedNode = PED_NODE_HEAD;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}
}

// SA 0x4B5CE0
void
CEventDamage::ComputeDamageResponseIfAffectsPed(CPed *ped, CPedDamageResponseCalculator &calculator, bool bSpeak)
{
	if(AffectsPed(ped)){
		calculator.ComputeDamageResponse(ped, m_damageResponse, bSpeak);
	}else{
		m_damageResponse.SetDamageAsCalculated();
	}
}

// 0x4ADD60 (melee part; gun part checks the hit piece)
bool
CEventDamage::WasHeadShot() const
{
	switch(m_weaponType){
	case WEAPONTYPE_UNARMED:
	case WEAPONTYPE_BRASSKNUCKLE:
	case WEAPONTYPE_GOLFCLUB:
	case WEAPONTYPE_NIGHTSTICK:
	case WEAPONTYPE_KNIFE:
	case WEAPONTYPE_BASEBALLBAT:
	case WEAPONTYPE_KATANA:
	case WEAPONTYPE_CHAINSAW:
		return m_damageResponse.m_bForceDeath;
	default:
		return m_pedPieceType == PED_PIECE_HEAD;
	}
}
