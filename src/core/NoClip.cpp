#include "common.h"

#include "NoClip.h"
#include "NoClipMovement.h"
#include "Camera.h"
#include "CutsceneMgr.h"
#include "Frontend.h"
#include "Pad.h"
#include "Physical.h"
#include "PlayerPed.h"
#include "Replay.h"
#include "Timer.h"
#include "Vehicle.h"
#include "World.h"

static CPhysical *noclipEntity;
static bool savedCollision, savedGravity;

static CPhysical*
FindNoClipTarget(void)
{
	CPlayerPed *ped = FindPlayerPed();
	if(!ped || ped->DyingOrDead() || ped->m_fHealth <= 0.0f ||
	   ped->EnteringCar() || ped->GetPedState() == PED_EXIT_CAR ||
	   ped->GetPedState() == PED_DRAG_FROM_CAR ||
	   ped->GetPedState() == PED_ENTER_TRAIN || ped->GetPedState() == PED_EXIT_TRAIN)
		return nil;

	if(ped->InVehicle()){
		CVehicle *vehicle = ped->m_pMyVehicle;
		// Do not take control of another driver's vehicle or a moving train.
		return vehicle && vehicle->pDriver == ped && !vehicle->IsTrain() ? vehicle : nil;
	}
	return ped;
}

static void
StopNoClipMotion(CPhysical *entity)
{
	entity->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	entity->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
	entity->m_vecMoveFriction = CVector(0.0f, 0.0f, 0.0f);
	entity->m_vecTurnFriction = CVector(0.0f, 0.0f, 0.0f);
}

static void
ApplyNoClipFlags(void)
{
	noclipEntity->bUsesCollision = false;
	noclipEntity->bAffectedByGravity = false;
	StopNoClipMotion(noclipEntity);
}

bool
CNoClip::IsActive(void)
{
	return noclipEntity != nil;
}

void
CNoClip::Reset(void)
{
	CPad::GetPad(PAD1)->SetEnablePlayerControls(PLAYERCONTROL_NOCLIP);
	if(noclipEntity){
		noclipEntity->bUsesCollision = savedCollision;
		noclipEntity->bAffectedByGravity = savedGravity;
		noclipEntity->bIsInSafePosition = false; // recheck physics on the next frame
		StopNoClipMotion(noclipEntity);
		noclipEntity->CleanUpOldReference((CEntity**)&noclipEntity);
		noclipEntity = nil;
		REVC_LOG_INFO("F10 noclip off (collision and gravity restored)");
	}
}

void
CNoClip::ProcessInput(void)
{
#ifdef GTA_PC_CONTROLS
	CPad *pad = CPad::GetPad(PAD1);
	// RegisterReference clears our pointer if the ped/vehicle is destroyed.
	// The control bit must be cleared as well or the new player cannot move.
	if(!noclipEntity && pad->IsPlayerControlsDisabledBy(PLAYERCONTROL_NOCLIP))
		Reset();
	if(noclipEntity && (noclipEntity != FindNoClipTarget() ||
	                    CCutsceneMgr::IsRunning() || CReplay::IsPlayingBack()))
		Reset();

	if(FrontEndMenuManager.m_bMenuActive || CReplay::IsPlayingBack() ||
	   CCutsceneMgr::IsRunning())
		return;

	if(pad->GetFJustDown(9)){
		if(noclipEntity){
			Reset();
			return;
		}
		CPhysical *target = FindNoClipTarget();
		if(!target || !target->bUsesCollision || pad->ArePlayerControlsDisabled()){
			REVC_LOG_WARNING("F10 noclip: only available on foot or while driving, with normal controls");
			return;
		}
		noclipEntity = target;
		savedCollision = target->bUsesCollision;
		savedGravity = target->bAffectedByGravity;
		target->RegisterReference((CEntity**)&noclipEntity);
		pad->SetDisablePlayerControls(PLAYERCONTROL_NOCLIP);
		REVC_LOG_INFO("F10 noclip on (%s model=%d)", target->IsVehicle() ? "vehicle" : "ped",
			target->GetModelIndex());
	}
	if(noclipEntity)
		ApplyNoClipFlags(); // ped/vehicle control can restore these during a frame
#endif
}

void
CNoClip::Move(void)
{
#ifdef GTA_PC_CONTROLS
	if(!noclipEntity)
		return;
	if(noclipEntity != FindNoClipTarget() || CCutsceneMgr::IsRunning() ||
	   CReplay::IsPlayingBack()){
		Reset();
		return;
	}

	CPad *pad = CPad::GetPad(PAD1);
	ApplyNoClipFlags();
	if(pad->DisablePlayerControls & ~PLAYERCONTROL_NOCLIP)
		return; // a script/camera has taken control; do not move it

	float forward = (pad->GetChar('W') || pad->GetChar('w')) -
	                (pad->GetChar('S') || pad->GetChar('s'));
	float strafe = (pad->GetChar('D') || pad->GetChar('d')) -
	               (pad->GetChar('A') || pad->GetChar('a'));
	float vertical = (pad->GetChar(' ') || pad->GetChar('E') || pad->GetChar('e')) -
	                 (pad->GetLeftCtrl() || pad->GetRightCtrl() || pad->GetChar('Q') || pad->GetChar('q'));
	if(forward == 0.0f && strafe == 0.0f && vertical == 0.0f)
		return;

	const CVector &view = TheCamera.Cams[TheCamera.ActiveCam].Front;
	NoClipVector direction = NoClipMoveDirection(view.x, view.y, view.z, forward, strafe, vertical);
	float seconds = NoClipClamp(CTimer::GetTimeStepInSeconds(), 0.0f, 0.1f);
	float speed = (pad->GetLeftShift() || pad->GetRightShift()) ? 36.0f : 12.0f;
	CVector pos = noclipEntity->GetPosition();
	// RemoveAndAdd uses sparse sectors beyond the original island grid.
	pos.x += direction.x*speed*seconds;
	pos.y += direction.y*speed*seconds;
	pos.z += direction.z*speed*seconds; // F10 must not impose a second height boundary

	// Teleport() on vehicles resets their orientation; reposition the physical
	// entity directly and update sector membership without pinning any RW object.
	noclipEntity->SetPosition(pos);
	noclipEntity->RemoveAndAdd();
	noclipEntity->GetMatrix().UpdateRW();
	noclipEntity->UpdateRwFrame();
	if(noclipEntity->IsVehicle()){
		CPlayerPed *ped = FindPlayerPed();
		ped->SetPedPositionInCar();
		ped->RemoveAndAdd();
		ped->GetMatrix().UpdateRW();
		ped->UpdateRwFrame();
	}
	ApplyNoClipFlags();
#endif
}
