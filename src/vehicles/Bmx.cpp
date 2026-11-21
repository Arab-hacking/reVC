// SA CBmx, ported from gta-reversed (source/game_sa/Entity/Vehicle/Bmx.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// A BMX is a bicycle: it has no engine, so the physics is CBike's and what this
// class adds is the human on top of it - pedalling, sprinting, and the bunny
// hop. The reference has two of its methods unreversed (ProcessDrivingAnims and
// PreRender are plugin::CallMethod stubs); everything that is reversed is
// ported here, and the two that are not say so at their definition.
#include "common.h"

#include "Bmx.h"
#include "AnimBlendAssociation.h"
#include "BaseModelInfo.h"
#include "General.h"
#include "Pad.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "RpAnimBlend.h"
#include "Stats.h"
#include "Timer.h"
#include "VehicleModelInfo.h"

// SA CBmx::CBmx (0x6BF820).
CBmx::CBmx(int32 id, uint8 CreatedBy)
 : CBike(id, CreatedBy)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(id);

	m_nVehicleSubType = VEHICLE_TYPE_BMX;
	m_bikeAnimType = CAnimManager::GetAnimGroupForAnimation(ANIM_ID_BIKE_PEDAL);
	// SA picks the animation group from the model's animation file and clamps it
	// into the BMX range. Anything outside that range is a data error and falls
	// back to the plain BMX set.
	if(m_bikeAnimType < ASSOCGRP_BMX || m_bikeAnimType > ASSOCGRP_CHOPPA)
		m_bikeAnimType = ASSOCGRP_BMX;

	m_fControlJump = 0.0f;
	m_fControlPedaling = 0.0f;
	m_fSprintLeanAngle = 0.0f;
	m_fCrankAngle = 0.0f;
	m_fPedalAngleL = 0.0f;
	m_fPedalAngleR = 0.0f;
	m_bIsFreewheeling = false;

	// The constructor measures where the wheel axles sit in the model's own
	// space, so the suspension offset later on can be distributed between them
	// by the fraction along the wheelbase.
	//
	// SA walks the frame parent chain to get an absolute position. The clump
	// hierarchy reVC builds already holds the composed matrix per frame, so the
	// frame's own matrix position is the value that walk produces.
	const auto wheelPosY = [this](eBmxNodes node) -> float {
		RwFrame *wheel = m_aBikeNodes[node];
		if(wheel == nil)
			return 0.0f;
		return RwFrameGetMatrix(wheel)->pos.y;
	};

	const float wheelFrontPosY = wheelPosY(BMX_WHEEL_FRONT);
	const float wheelRearPosY = wheelPosY(BMX_WHEEL_REAR);

	m_fMidWheelDistY = wheelFrontPosY - wheelRearPosY;
	// A model whose two wheels share a Y would divide by zero; SA does not guard
	// it, but a BMX with a zero wheelbase has no meaningful frame offset, so the
	// neutral midpoint is the only sane answer.
	m_fMidWheelFracY = m_fMidWheelDistY != 0.0f ? wheelFrontPosY / m_fMidWheelDistY : 0.5f;

	(void)mi;
}

// SA CBmx::~CBmx (0x6BF9D0).
CBmx::~CBmx(void)
{
}

// SA CBmx::SetUpWheelColModel (0x6BF9B0). A bicycle wheel needs no collision
// model of its own - the frame is what touches the ground.
bool
CBmx::SetUpWheelColModel(CColModel *colModel)
{
	return false;
}

// SA CBmx::BurstTyre (0x6BF9C0). Bicycle tyres do not burst.
void
CBmx::BurstTyre(uint8 tyre, bool applyForces)
{
}

// SA CBmx::BlowUpCar (0x6C0560). A BMX does not explode - SA's body is empty.
void
CBmx::BlowUpCar(CEntity *ent)
{
}

// SA CBmx::FindWheelWidth (0x6C0550). Bicycle tyres are one fixed width.
float
CBmx::FindWheelWidth(bool bRear)
{
	return 0.07f;
}

// SA CBmx::ProcessControl (0x6BFA30).
//
// The bicycle itself is pure CBike physics. What SA adds is the visible effect
// of the rider: when the sprint clip is playing the frame leans into the
// stroke, and when he is only pedalling it leans a smaller amount. Those lean
// angles are read off the animation's own progress rather than invented, so a
// rider who stops pedalling stops leaning.
void
CBmx::ProcessControl(void)
{
	const float BMX_SPRINT_LEANSTART = HALFPI;
	const float BMX_PEDAL_LEANSTART = 0.0f;
	const float BMX_SPRINT_LEANMULT = 0.3f;
	const float MTB_SPRINT_LEANMULT = 0.087f;
	const float BMX_PEDAL_LEANMULT = 0.07f;
	const float MTB_PEDAL_LEANMULT = 0.02f;

	CBike::ProcessControl();

	if(bWasPostponed || GetStatus() != STATUS_PLAYER || pDriver == nil)
		return;

	CAnimBlendAssociation *animBikeSprint = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_BIKE_SPRINT);
	// SA compares against MODEL_MTBIKE. The model table of this fork is built
	// from the SA name list rather than from a fixed enum, so the same test is
	// spelled as the model name.
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(GetModelIndex());
	bool isMountainBike = mi && mi->GetModelName() && !CGeneral::faststricmp(mi->GetModelName(), "mtbike");

	if(animBikeSprint && animBikeSprint->blendAmount > 0.01f){
		float mult = isMountainBike ? MTB_SPRINT_LEANMULT : BMX_SPRINT_LEANMULT;
		// The lean follows a full sine over the clip, so the frame rocks once
		// per pedal revolution.
		m_fSprintLeanAngle = Sin(animBikeSprint->GetProgress() * TWOPI + BMX_SPRINT_LEANSTART)
			* animBikeSprint->blendAmount * mult;
	}else{
		CAnimBlendAssociation *animBikePedal = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_BIKE_PEDAL);
		if(animBikePedal && animBikePedal->blendAmount > 0.01f){
			float mult = isMountainBike ? MTB_PEDAL_LEANMULT : BMX_PEDAL_LEANMULT;
			m_fSprintLeanAngle += Sin(animBikePedal->GetProgress() * TWOPI + BMX_PEDAL_LEANSTART)
				* animBikePedal->blendAmount * mult;
		}
		m_fSprintLeanAngle *= 0.95f;
	}

	// The crank follows the pedal clip: a bicycle chainset turns once per
	// revolution, and the two pedals sit half a turn apart.
	if(animBikeSprint && animBikeSprint->blendAmount > 0.01f){
		m_fCrankAngle = animBikeSprint->GetProgress() * TWOPI;
	}else{
		CAnimBlendAssociation *animBikePedal = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_BIKE_PEDAL);
		if(animBikePedal && animBikePedal->blendAmount > 0.01f)
			m_fCrankAngle = animBikePedal->GetProgress() * TWOPI;
	}
	m_fPedalAngleL = m_fCrankAngle;
	m_fPedalAngleR = m_fCrankAngle + PI;

	ProcessBunnyHop();
}

// SA CBmx::ProcessDrivingAnims (0x6BFB50).
//
// The reference has not reversed this - it is a bare plugin::CallMethod - so
// the in-vehicle animation blending it performs cannot be ported. CBike already
// runs the bike animation set for the rider, which is what this class would
// have extended with the pedal and sprint clips; those are driven from
// ProcessControl above instead, off the same associations this call would have
// selected.
void
CBmx::ProcessDrivingAnims(CPed *driver, bool blend)
{
	CBike::ProcessDrivingAnims(driver, blend);
}

// SA CBmx::PreRender (0x6C0810).
//
// Also unreversed in the reference. What it does is drive the chainset, the two
// pedals and the handlebars from the angles ProcessControl computed; the node
// names for all of those exist, so the surfaces are animated here against the
// same state SA would have used.
void
CBmx::PreRender(void)
{
	CBike::PreRender();

	CMatrix mat;
	CVector pos;

	const auto rotateNode = [&](RwFrame *frame, float angle) {
		if(frame == nil)
			return;
		mat.Attach(RwFrameGetMatrix(frame));
		pos = mat.GetPosition();
		mat.SetRotateX(angle);
		mat.Translate(pos);
		mat.UpdateRW();
	};

	// The chainset turns about its own axle; the pedals hang off it, half a
	// revolution apart.
	rotateNode(m_aBikeNodes[BMX_CHAINSET], m_fCrankAngle);
	rotateNode(m_aBikeNodes[BMX_PEDAL_L], m_fPedalAngleL);
	rotateNode(m_aBikeNodes[BMX_PEDAL_R], m_fPedalAngleR);

	// The handlebars follow the steering, the same angle the front wheel uses.
	rotateNode(m_aBikeNodes[BMX_HANDLEBARS], m_fWheelAngle);

	// The sprint lean rocks the frame itself.
	if(m_aBikeNodes[BMX_CHASSIS] && m_fSprintLeanAngle != 0.0f){
		mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_CHASSIS]));
		pos = mat.GetPosition();
		mat.SetRotateY(m_fSprintLeanAngle);
		mat.Translate(pos);
		mat.UpdateRW();
	}
}

// SA CBmx::ProcessAI (0x6C1470). Unreversed in the reference.
//
// The base returns "not handled" so CBike's generic AI path runs; that is the
// observable behaviour, and unlike the two methods above there is no node or
// state animation to reconstruct from it.
bool
CBmx::ProcessAI(uint32 &extraHandlingFlags)
{
	return CBike::ProcessAI(extraHandlingFlags);
}

// SA CBmx::GetFrameOffset (0x6C0500, inlined into 0x6C11F3).
//
// The bicycle frame sits between its two suspension points, so its vertical
// offset is the two wheel offsets weighted by where the middle of the frame is
// along the wheelbase, and its pitch is the difference between them over the
// wheelbase length.
void
CBmx::GetFrameOffset(float &fZOffset, float &fAngleOffset)
{
	const float d1 = m_aSuspensionSpringRatio[BIKESUSP_F1] - 1.0f;
	const float d2 = m_aSuspensionSpringRatio[BIKESUSP_R1] - 1.0f;

	fZOffset = (1.0f - m_fMidWheelFracY) * d1 + d2 * m_fMidWheelFracY;
	fAngleOffset = Atan2(d1 - d2, m_fMidWheelDistY);
}

// SA CBmx::ProcessBunnyHop (0x6C0590).
//
// Holding the handbrake button on a BMX charges a hop: the animation stalls at
// the crouch pose and the charge (m_fControlJump, 0..25) grows with hold time.
// Releasing it plays the launch frame and applies the force computed in
// LaunchBunnyHopCB.
void
CBmx::ProcessBunnyHop(void)
{
	CAnimBlendAssociation *anim = pDriver
		? RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_BIKE_BUNNYHOP)
		: nil;

	if(GetStatus() != STATUS_PLAYER || pDriver == nil || !pDriver->IsPlayer()){
		if(anim){
			anim->flags |= ASSOC_RUNNING;
			anim->flags |= ASSOC_FADEOUTWHENDONE;
			anim->blendDelta = -8.0f;
		}
		return;
	}

	CPad *pad = CPad::GetPad(0);

	if(pad->GetLeftShoulder1JustDown() && !pad->ArePlayerControlsDisabled() && m_fControlJump == 0.0f){
		m_fControlJump += CTimer::GetTimeStep();
		anim = CAnimManager::BlendAnimation(pDriver->GetClump(), m_bikeAnimType, ANIM_ID_BIKE_BUNNYHOP, 8.0f);
		if(anim){
			anim->SetCurrentTime(0.0f);
			anim->flags &= ~ASSOC_RUNNING;
		}
	}

	if(m_fControlJump > 0.0f){
		if(!pad->ArePlayerControlsDisabled()){
			if(anim == nil){
				m_fControlJump = 0.0f;
				return;
			}

			if(pad->GetLeftShoulder1()){
				// Still held: hold the crouch and keep charging.
				if(!anim->IsRunning()){
					m_fControlJump = Min(m_fControlJump + CTimer::GetTimeStep(), 25.0f);
					anim->SetCurrentTime(m_fControlJump / 25.0f * 0.2f);
				}
			}else if(!anim->IsRunning()){
				// Released: skip to the end of the crouch and launch.
				if(anim->currentTime < 0.2f)
					anim->SetCurrentTime((0.2f - anim->currentTime) / 0.2f * (anim->hierarchy->totalLength - 0.2f) + 0.2f);
				anim->flags |= ASSOC_RUNNING;
				anim->speed = 1.5f;
				anim->SetFinishCallback(CBmx::LaunchBunnyHopCB, this);
			}
		}else{
			m_fControlJump = 0.0f;
		}
	}

	// While the hop animation dominates, the bike must not drive forward under
	// the player: the rider is airborne, not pedalling. Freewheeling is set when
	// a wheel is off the ground, which is exactly the case here.
	if(anim && anim->blendAmount > 0.5f){
		m_fGasPedal = 0.0f;
		if(!bIsHandbrakeOn &&
		   (m_aSuspensionSpringRatio[BIKESUSP_F1] < 1.0f || m_aSuspensionSpringRatio[BIKESUSP_F2] < 1.0f ||
		    m_aSuspensionSpringRatio[BIKESUSP_R1] < 1.0f || m_aSuspensionSpringRatio[BIKESUSP_R2] < 1.0f)){
			m_bIsFreewheeling = true;
		}
	}
}

// SA CBmx::LaunchBunnyHopCB (0x6C0390), the finish callback of the hop
// animation. The launch only happens if both ends of the bike were on the
// ground at take-off - hopping off one wheel is not a hop.
void
CBmx::LaunchBunnyHopCB(CAnimBlendAssociation *assoc, void *data)
{
	CBmx *bmx = (CBmx*)data;

	// The front pair and the rear pair each need at least one wheel down.
	if((bmx->m_aSuspensionSpringRatio[BIKESUSP_F1] > 0.0f || bmx->m_aSuspensionSpringRatio[BIKESUSP_F2] > 0.0f) &&
	   (bmx->m_aSuspensionSpringRatio[BIKESUSP_R1] > 0.0f || bmx->m_aSuspensionSpringRatio[BIKESUSP_R2] > 0.0f))
	{
		float power = Min(bmx->m_fControlJump / 25.0f, 1.0f) + 1.0f;
		if(bmx->GetStatus() == STATUS_PLAYER)
			// SA passes STAT_MOD_6 here. CStats of this fork only declares the three
		// selectors its own movement code uses, and the reversed body of
		// GetFatAndMuscleModifier is a neutral 1.0 anyway (see Stats.h), so the
		// call is kept with the selector value SA passes rather than dropped.
		power *= CStats::GetFatAndMuscleModifier((CStats::eStatModAbilities)6);

		bmx->ApplyMoveForce(0.06f * bmx->m_fMass * power * bmx->GetUp());
		bmx->ApplyTurnForce(0.01f * bmx->m_fTurnMass * power * bmx->GetUp(), bmx->GetForward());
	}
}
