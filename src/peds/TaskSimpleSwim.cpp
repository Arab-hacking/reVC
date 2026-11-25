#include "common.h"

#include "TaskSimpleSwim.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Pad.h"
#include "World.h"
#include "Timer.h"
#include "General.h"
#include "Stats.h"
#include "AnimManager.h"
#include "RpAnimBlend.h"
#include "WaterLevel.h"
#include "soundlist.h"
#include "Particle.h"
#include "ParticleObject.h"
#include "AnimBlendAssociation.h"
#include "Camera.h"

// The strokes' root-motion speeds, measured from swim.ifp: Swim_Breast covers
// 3.011 m in 1.3 s and SWIM_crawl 2.505 m in 0.9 s. SA drives a swimming ped
// with the animation's own shift rather than with an input speed, which is why
// these two numbers are what a swimmer actually moves at.
#define SWIM_BREAST_SPEED 2.316f
#define SWIM_CRAWL_SPEED 2.783f

// A ped's origin sits a unit above its feet: the COL bounds run from z -1 to
// 0.95. Every water depth below is measured against that origin, as SA does.
#define PED_ORIGIN_HEIGHT 1.0f

// SA CTaskSimpleSwim::SWIM_STOP_TIME: how long a ped that is standing in water
// too shallow to swim keeps the swim task before it gives up.
#define SWIM_STOP_TIME 15.0f

namespace {

// Where a swimming ped's origin settles when the swim servo is not driving it.
// Buoyancy immersion (water - origin + 1) / 1.9 times the ped's buoyancy
// factor balances gravity, which for a live ped puts the origin 0.727 under
// the surface.
float
PedFloatDepth(bool alive)
{
	return 1.9f / (alive ? 1.1f : 1.8f) - 1.0f;
}

// SA CPed::ProcessBuoyancy starts the in-water task once the ped's origin is
// 0.6 under the surface. This is deliberately shallower than the buoyancy
// force's own trigger, because by the time buoyancy lifts a ped he is already
// up to his chest and the task is late.
bool
SwimEntry(float feetZ, float waterLevel)
{
	return feetZ + PED_ORIGIN_HEIGHT + 0.6f < waterLevel;
}

} // namespace

CTaskSimpleSwim::CTaskSimpleSwim()
{
	m_vecSwimTarget = CVector2D(0.0f, 0.0f);
	m_fRotationX = 0.0f;
	m_fSwimProgress = 0.0f;
	m_fAnimSpeed = -1.0f;
	m_nSwimState = SWIM_TREAD;
	m_bDiving = false;
	m_bHasTarget = false;
	m_pSwimAnim = nil;
}

CTaskSimpleSwim::~CTaskSimpleSwim()
{
	if(m_pSwimAnim){
		m_pSwimAnim->SetDeleteCallback(nil, nil);
		m_pSwimAnim = nil;
	}
}

// SA CTaskSimpleSwim::StartAnim: blend the stroke for the current state. The
// ped's swimming move state follows the clip so that ProcessControl and the
// animation code can tell a breaststroke from a crawl without asking the task.
void
CTaskSimpleSwim::StartAnim(CPed *ped, AssocGroupId animGroup, AnimationId animId)
{
	if(m_pSwimAnim)
		m_pSwimAnim->SetDeleteCallback(nil, nil);

	m_pSwimAnim = CAnimManager::BlendAnimation(ped->GetClump(), animGroup, animId, 8.0f);
	if(m_pSwimAnim == nil)
		return;

	switch(animId){
	case ANIM_ID_SWIM_TREAD:
		m_pSwimAnim->m_Speed = 0.5f;
		ped->m_nSwimmingMoveState = PEDMOVE_STILL;
		break;
	case ANIM_ID_SWIM_BREAST:
		m_pSwimAnim->m_Speed = m_fAnimSpeed > 0.0f ? m_fAnimSpeed : 1.0f;
		ped->m_nSwimmingMoveState = PEDMOVE_WALK;
		break;
	case ANIM_ID_SWIM_CRAWL:
		m_pSwimAnim->m_Speed = m_fAnimSpeed > 1.0f ? Min(1.5f, m_fAnimSpeed - 1.0f) : 1.0f;
		ped->m_nSwimmingMoveState = PEDMOVE_RUN;
		break;
	default:
		// The dive and glide clips are one-shots; they run once and the state
		// machine takes over from there.
		ped->m_nSwimmingMoveState = PEDMOVE_STILL;
		break;
	}
}

void
CTaskSimpleSwim::StopTask(CPed *ped)
{
	if(m_pSwimAnim){
		m_pSwimAnim->SetDeleteCallback(nil, nil);
		m_pSwimAnim->m_BlendDelta = -1000.0f;
		m_pSwimAnim = nil;
	}
	ped->m_nSwimmingMoveState = PEDMOVE_STILL;
	ped->SetMoveAnim();
}

bool
CTaskSimpleSwim::ProcessPed(CPed *ped)
{
	CPlayerPed *player = ped->IsPlayer() ? (CPlayerPed *)ped : nil;

	if(player){
		ProcessControlInput(ped, player);
		ProcessSwimmingResistance(ped);
		ProcessEffects(ped);
	}else{
		ProcessControlAI(ped);
		ProcessSwimmingResistance(ped);
	}

	return true;
}

// SA CTaskSimpleSwim::ProcessControlInput (0x689FD0): what the pad asks for.
//
// Swimming is a move state, so the input does not pick a ped state; it picks a
// stroke. The dive and the jump-out are edges, and both are only available from
// the two surface states - once the ped is underwater, the surface is reached by
// swimming up, not by pressing jump again.
void
CTaskSimpleSwim::ProcessControlInput(CPed *ped, CPlayerPed *player)
{
	CPad *pad = CPad::GetPad(0);

	// The move blend ramps toward the pad's move vector at 0.07 a frame, exactly
	// as the on-foot blend does, and it is the blend rather than the raw input
	// that picks between treading and swimming. player->MoveSpeed() mirrors it
	// so that the rest of the game (the HUD, the AI, the camera) sees a player
	// who is moving at the speed he is visibly moving at.
	float leftRight = pad->GetPedWalkLeftRight();
	float upDown = pad->GetPedWalkUpDown();
	float inputMag = Min(1.0f, CVector2D(leftRight, upDown).Magnitude() / 128.0f);
	float step = CTimer::GetTimeStep() * 0.07f;
	m_fSwimProgress = (inputMag - m_fSwimProgress <= step) ? inputMag : m_fSwimProgress + step;
	player->MoveSpeed() = m_fSwimProgress;

	if(pad->GetJump() && (m_nSwimState == SWIM_TREAD || m_nSwimState == SWIM_SPRINT))
		m_nSwimState = SWIM_BACK_TO_SURFACE;
	else if(pad->WeaponJustDown() && (m_nSwimState == SWIM_TREAD || m_nSwimState == SWIM_SPRINT)){
		m_nSwimState = SWIM_DIVE_UNDERWATER;
		m_bDiving = true;
		if(m_pSwimAnim){
			m_pSwimAnim->SetDeleteCallback(nil, nil);
			m_pSwimAnim = nil;
		}
		StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_DIVE_UNDER);
	}else{
		switch(m_nSwimState){
		case SWIM_TREAD:
		case SWIM_SPRINT:
			if(player->ControlButtonSprint(SPRINT_WATER) > 1.0f)
				m_nSwimState = SWIM_SPRINTING;
			else
				m_nSwimState = m_fSwimProgress > 0.5f ? SWIM_SPRINT : SWIM_TREAD;
			break;
		case SWIM_SPRINTING:
			if(!pad->GetSprint())
				m_nSwimState = m_fSwimProgress > 0.5f ? SWIM_SPRINT : SWIM_TREAD;
			break;
		case SWIM_DIVE_UNDERWATER:
			if(m_pSwimAnim == nil || m_pSwimAnim->m_BlendAmount == 1.0f){
				m_nSwimState = SWIM_UNDERWATER_SPRINTING;
				StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_UNDER);
			}
			break;
		case SWIM_UNDERWATER_SPRINTING:
			break;
		case SWIM_BACK_TO_SURFACE:
			if(m_pSwimAnim == nil || m_pSwimAnim->m_BlendAmount == 1.0f){
				m_nSwimState = SWIM_TREAD;
				StartAnim(ped, ANIM_GROUP_DEFAULT, ANIM_ID_SWIM_TREAD);
			}
			break;
		}
	}

	// The stroke that matches the state, started only when the ped has none.
	if(m_pSwimAnim == nil){
		switch(m_nSwimState){
		case SWIM_TREAD: StartAnim(ped, ANIM_GROUP_DEFAULT, ANIM_ID_SWIM_TREAD); break;
		case SWIM_SPRINT: StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_BREAST); break;
		case SWIM_SPRINTING: StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_CRAWL); break;
		case SWIM_BACK_TO_SURFACE: StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_JUMPOUT); break;
		default: break;
		}
	}else{
		// Same family of clip: keep the blend but swap the stroke when the state
		// moved between the surface strokes.
		AnimationId wanted = ANIM_ID_SWIM_TREAD;
		bool swap = false;
		switch(m_nSwimState){
		case SWIM_TREAD: wanted = ANIM_ID_SWIM_TREAD; swap = true; break;
		case SWIM_SPRINT: wanted = ANIM_ID_SWIM_BREAST; swap = true; break;
		case SWIM_SPRINTING: wanted = ANIM_ID_SWIM_CRAWL; swap = true; break;
		default: break;
		}
		if(swap && m_pSwimAnim->m_AnimId != wanted)
			StartAnim(ped, ANIM_GROUP_SWIM, wanted);
	}

	// Heading: a swimmer always turns to face where he is going, and while
	// treading water he turns to where the camera is looking.
	if(m_nSwimState == SWIM_TREAD){
		m_fRotationX = CGeneral::LimitRadianAngle(m_fRotationX + leftRight / 128.0f * 0.05f * CTimer::GetTimeStep());
	}else{
		if(leftRight != 0.0f || upDown != 0.0f){
			float padHeading = CGeneral::GetRadianAngleBetweenPoints(0.0f, 0.0f, -leftRight, upDown);
			float neededTurn = CGeneral::LimitRadianAngle(padHeading - TheCamera.Orientation);
			m_fRotationX = neededTurn;
		}
	}
	ped->m_fRotationDest = ped->m_fRotationCur = m_fRotationX;
}

// SA CTaskSimpleSwim::ProcessSwimmingResistance (0x68A310): the ped is in a
// medium, not on a surface.
//
// The horizontal speed is not set from the input; it is dragged toward the
// stroke's root-motion speed. That is why a swimmer keeps gliding for a moment
// after the pad is released, and why a sprint in water is a different stroke
// rather than the same stroke faster.
void
CTaskSimpleSwim::ProcessSwimmingResistance(CPed *ped)
{
	float ts = CTimer::GetTimeStep();
	float keep = pow(0.9f, ts);

	float strokeSpeed = 0.0f;
	switch(m_nSwimState){
	case SWIM_SPRINT: strokeSpeed = SWIM_BREAST_SPEED; break;
	case SWIM_SPRINTING: strokeSpeed = SWIM_CRAWL_SPEED; break;
	default: strokeSpeed = 0.0f; break;
	}

	// The stroke's speed is in the ped's own forward direction.
	CVector forward = ped->GetForward();
	forward.z = 0.0f;
	if(forward.MagnitudeSqr() > 0.0f)
		forward.Normalise();

	CVector target = forward * strokeSpeed;
	ped->m_vecMoveSpeed.x = ped->m_vecMoveSpeed.x * keep + target.x * (1.0f - keep);
	ped->m_vecMoveSpeed.y = ped->m_vecMoveSpeed.y * keep + target.y * (1.0f - keep);

	// Vertical: the servo that holds a swimmer at his swimming depth.
	float waterLevel = 0.0f;
	bool hasWater = CWaterLevel::GetWaterLevel(ped->GetPosition(), &waterLevel, false);

	if(hasWater){
		float submerge = 0.55f;
		if(m_nSwimState == SWIM_SPRINT) submerge = 0.4f;
		else if(m_nSwimState == SWIM_SPRINTING) submerge = 0.2f;

		float targetZ = (waterLevel - submerge - ped->GetPosition().z) / ts;
		targetZ = Clamp(targetZ, -ts * 0.1f, ts * 0.1f) - ped->m_vecMoveSpeed.z;
		targetZ = Clamp(targetZ, -ts, ts);
		ped->m_vecMoveSpeed.z += targetZ;

		// Below the deepest water in the level: nothing to swim in.
		if(ped->GetPosition().z < -69.0f)
			ped->SetPosition(ped->GetPosition().x, ped->GetPosition().y, -69.0f);
	}

	ped->bIsStanding = false;
	ped->bIsInTheAir = false;

	// The dive's glide ends when the clip is over.
	if(m_nSwimState == SWIM_DIVE_UNDERWATER && m_pSwimAnim
	   && (m_pSwimAnim->m_BlendAmount == 1.0f || m_pSwimAnim->m_AnimId != ANIM_ID_SWIM_DIVE_UNDER)){
		m_nSwimState = SWIM_UNDERWATER_SPRINTING;
		StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_UNDER);
	}
}

// SA CTaskSimpleSwim::ProcessEffects (0x68ABB0): breath, the splash under a
// swimmer's hands and the sound of the stroke.
void
CTaskSimpleSwim::ProcessEffects(CPed *ped)
{
	CPlayerPed *player = ped->IsPlayer() ? (CPlayerPed *)ped : nil;
	if(player == nil)
		return;

	bool underwater = m_nSwimState == SWIM_DIVE_UNDERWATER
	                  || m_nSwimState == SWIM_UNDERWATER_SPRINTING;

	// The lung-capacity stat scales how long the player can hold his breath;
	// the same 2x/1x/3x rates SA uses apply on the way down, on the way up and
	// once there is no air left.
	float capacity = CStats::GetFatAndMuscleModifier(CStats::STAT_MOD_AIR_IN_LUNG);
	float ts = CTimer::GetTimeStep();

	if(underwater && ped->bDrownsInWater){
		if(player->Breath() > 0.0f)
			player->Breath() = Max(0.0f, player->Breath() - ts);
	} else if(capacity > player->Breath()){
		player->Breath() = Min(capacity, player->Breath() + ts * 2.0f);
	}

	// Out of air while under: SA starts drowning damage here, and the ped sinks.
	if(underwater && player->Breath() <= 0.0f && ped->bDrownsInWater && !ped->DyingOrDead()){
		ped->bIsDrowning = true;
		ped->InflictDamage(nil, WEAPONTYPE_DROWNING, 3.0f * ts, PED_PIECE_TORSO, 0);
	}
}

// SA CTaskSimpleSwim::ProcessControlAI (0x689640): a ped that swims without a
// pad heads for its target point and picks its stroke from the distance to it.
void
CTaskSimpleSwim::ProcessControlAI(CPed *ped)
{
	if(!m_bHasTarget){
		// Nowhere to go: tread water, which is what SA leaves a ped doing when
		// its seek position was never set.
		if(m_nSwimState != SWIM_TREAD){
			m_nSwimState = SWIM_TREAD;
			StartAnim(ped, ANIM_GROUP_DEFAULT, ANIM_ID_SWIM_TREAD);
		}
		return;
	}

	if(ped->m_nPedType == PEDTYPE_CIVMALE || ped->m_nPedType == PEDTYPE_CIVFEMALE){
		float distance = (m_vecSwimTarget - CVector2D(ped->GetPosition())).Magnitude();

		eSwimState newState;
		if(distance > 1.0f){
			if(m_fAnimSpeed > 1.0f && distance > 2.0f)
				newState = SWIM_SPRINTING;
			else
				newState = SWIM_SPRINT;
		}else{
			newState = SWIM_TREAD;
		}

		if(newState != m_nSwimState){
			m_nSwimState = newState;
			switch(m_nSwimState){
			case SWIM_TREAD: StartAnim(ped, ANIM_GROUP_DEFAULT, ANIM_ID_SWIM_TREAD); break;
			case SWIM_SPRINT: StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_BREAST); break;
			case SWIM_SPRINTING: StartAnim(ped, ANIM_GROUP_SWIM, ANIM_ID_SWIM_CRAWL); break;
			default: break;
			}
		}

		if(distance > 0.1f){
			CVector2D dir = m_vecSwimTarget - CVector2D(ped->GetPosition());
			ped->m_fRotationDest = ped->m_fRotationCur = CGeneral::GetRadianAngleBetweenPoints(0.0f, 0.0f, dir.x, dir.y);
		}
	}
}
