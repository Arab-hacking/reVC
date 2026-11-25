// SA CQuadBike, ported from gta-reversed
// (source/game_sa/Entity/Vehicle/QuadBike.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// A quad in SA is not a bike: it is a car chassis with handlebars. The wheels
// are car wheels and the suspension is a car's, but the rider sits on it like
// on a bike and leans the machine through the corners, which is what
// CRideAnimData carries. Only ProcessControl is unreversed in the reference
// (a bare plugin::CallMethod); it is reconstructed here and says so.
#include "common.h"

#include "QuadBike.h"

#include "BaseModelInfo.h"
#include "cHandlingDataMgr.h"
#include "General.h"
#include "Pad.h"
#include "Ped.h"
#include "Timer.h"
#include "VehicleModelInfo.h"

bool bDoQuadDamping = true;
float QUAD_HBSTEER_ANIM_MULT = -0.4f;
CVector vecQuadResistance(0.995f, 0.995f, 1.0f);

// SA CQuadBike::CQuadBike (0x6CE370).
CQuadBike::CQuadBike(int32 id, uint8 CreatedBy)
 : CAutomobile(id, CreatedBy)
 , m_sRideAnimData(ANIM_GROUP_QUAD)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	m_pBikeHandling = mod_HandlingManager.GetBikeHandlingPointer((uint32)mi->m_handlingId);
	m_nVehicleSubType = VEHICLE_TYPE_QUAD;

	field_9A8[0] = 1.0f;
	field_9A8[1] = 0.0f;
	field_9A8[2] = 0.0f;
	field_9A8[3] = 0.0f;

	CQuadBike::SetupSuspensionLines();

	m_nQuadFlags &= ~1u;
	m_fSteerAngle = 0.0f;
}

CQuadBike::~CQuadBike(void)
{
}

// SA CQuadBike::Fix (0x6CE2B0). A quad has no doors and no bonnet to lose, so
// a "repair" has to re-hide the pieces the model still carries and clear the
// damage flags that referred to them.
void
CQuadBike::Fix(void)
{
	for(int i = DOOR_FRONT_LEFT; i < MAX_DOORS; i++)
		Damage.SetDoorStatus((eDoors)i, DAMSTATE_NOTPRESENT);

	bIsDamaged = false;

	RpClumpForAllAtomics(GetClump(), CVehicleModelInfo::HideAllComponentsAtomicCB, (void*)2);

	for(int i = 0; i < 4; i++)
		Damage.SetWheelStatus(i, WHEEL_STATUS_OK);
}

// SA CQuadBike::GetRideAnimData (0x6CDC90).
CRideAnimData *
CQuadBike::GetRideAnimData(void)
{
	return &m_sRideAnimData;
}

// SA CQuadBike::PreRender (0x6CEAD0).
void
CQuadBike::PreRender(void)
{
	CAutomobile::PreRender();

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CVector wheelPos;

	mi->GetWheelPosn(CARWHEEL_REAR_LEFT, wheelPos);
	SetTransmissionRotation(m_aCarNodes[QUAD_REAR_AXLE],
		m_aWheelPosition[CARWHEEL_REAR_LEFT],
		m_aWheelPosition[CARWHEEL_REAR_RIGHT], wheelPos, false);

	CVector wheelFrontLeftPos;
	mi->GetWheelPosn(CARWHEEL_FRONT_LEFT, wheelFrontLeftPos);

	// SetRotateYOnly keeps the frame's own translation, which is exactly what
	// the original saves and restores around a SetRotate.
	CMatrix mat;

	if(m_aCarNodes[QUAD_SUSPENSION_LF]){
		mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_SUSPENSION_LF]), false);
		mat.SetRotateYOnly(Atan2(m_aWheelPosition[CARWHEEL_FRONT_LEFT] - wheelFrontLeftPos.z,
			Abs(wheelFrontLeftPos.x)));
		mat.UpdateRW();
	}

	if(m_aCarNodes[QUAD_SUSPENSION_RF]){
		mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_SUSPENSION_RF]), false);
		mat.SetRotateYOnly(-Atan2(m_aWheelPosition[CARWHEEL_FRONT_RIGHT] - wheelFrontLeftPos.z,
			Abs(wheelFrontLeftPos.x)));
		mat.UpdateRW();
	}

	if(m_aCarNodes[QUAD_HANDLEBARS]){
		mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_HANDLEBARS]), false);
		mat.SetRotateZOnly(QUAD_HBSTEER_ANIM_MULT * m_sRideAnimData.AnimLeanLeft);
		mat.UpdateRW();
	}
}

// SA CQuadBike::ProcessAI (0x6CE460).
//
// The player-driven half is what matters: the rider's lean is turned into a
// torque about the machine's forward axis so that leaning forward drops the
// nose and leaning back lifts it, and an airborne quad can be rotated with the
// stick. The AI half is CAutomobile's.
bool
CQuadBike::ProcessAI(uint32 &extraHandlingFlags)
{
	if(GetStatus() != STATUS_PLAYER)
		return CAutomobile::ProcessAI(extraHandlingFlags);

	GetColModel();

	if(pDriver == nil || !pDriver->IsPlayer()){
		// SA returns false without touching the vehicle; the player's lean is
		// only meaningful while he is actually on the quad.
		return false;
	}

	ProcessControlInputs((uint8)pDriver->m_nPedType);
	CPad *pad = CPad::GetPad(0);

	// The lean torque. SA keeps the same expression in both branches and only
	// changes how the magnitude is derived, so it is computed once here.
	float turnForcePerTimeStep = 0.0f;
	const float leanDirection = DotProduct(m_vecTurnSpeed, GetRight());

	if(m_sRideAnimData.LeanFwd >= 0.0f || leanDirection >= m_pBikeHandling->m_fLeanBakCOM){
		// Leaning forward: the nose drops.
		if(m_sRideAnimData.LeanFwd > 0.0f){
			if(leanDirection > -m_pBikeHandling->m_fLeanFwdCOM){
				if(m_nWheelsOnGround){
					if(m_fBrakePedal > 0.0f){
						turnForcePerTimeStep = m_pBikeHandling->m_fLeanFwdForce * m_fTurnMass
							* m_sRideAnimData.LeanFwd * Min(0.1f, m_vecMoveSpeed.Magnitude());
					}
				}else{
					turnForcePerTimeStep = m_fTurnMass * m_sRideAnimData.LeanFwd * 0.0015f;
				}
				const float turnForce = -CTimer::GetTimeStep() * turnForcePerTimeStep;
				ApplyTurnForce(GetUp() * turnForce, m_vecCentreOfMass + GetForward());
			}
		}
	}else{
		// Leaning back: the nose lifts.
		if(m_nWheelsOnGround){
			if(m_fBrakePedal == 0.0f && !bIsHandbrakeOn){
				turnForcePerTimeStep = m_pBikeHandling->m_fLeanBakForce * m_fTurnMass
					* m_sRideAnimData.LeanFwd * Min(0.1f, m_vecMoveSpeed.Magnitude());
			}
		}else{
			turnForcePerTimeStep = m_fTurnMass * m_sRideAnimData.LeanFwd * 0.0015f;
		}
		const float turnForce = -CTimer::GetTimeStep() * turnForcePerTimeStep;
		ApplyTurnForce(GetUp() * turnForce, m_vecCentreOfMass + GetForward());
	}

	// Air control: with no wheel on the ground the stick rolls and pitches the
	// quad directly instead of steering it.
	if(!GetAllWheelsOffGround() && pad){
		// (GetAllWheelsOffGround() true means all four are down - the helper
		// keeps SA's inverted naming.)
	}else if(pad){
		float steeringLeftRightProgress = (float)pad->GetSteeringLeftRight()/128.0f;

		if(bIsHandbrakeOn){
			const float turnSpeedDotUp = DotProduct(m_vecTurnSpeed, GetUp());
			if((turnSpeedDotUp < 0.029f && steeringLeftRightProgress < 0.0f) ||
			   (turnSpeedDotUp > -0.029f && steeringLeftRightProgress > 0.0f)){
				const float turnForce = CTimer::GetTimeStep() * m_fTurnMass * 0.0015f * steeringLeftRightProgress;
				ApplyTurnForce(GetRight() * turnForce, m_vecCentreOfMass + GetRight());
			}
		}else if(pad->GetAccelerate()){
			const float turnSpeedDotFwd = DotProduct(m_vecTurnSpeed, GetForward());
			if((turnSpeedDotFwd < 0.029f && steeringLeftRightProgress < 0.0f) ||
			   (turnSpeedDotFwd > -0.029f && steeringLeftRightProgress > 0.0f)){
				const float turnForce = CTimer::GetTimeStep() * m_fTurnMass * 0.0015f * steeringLeftRightProgress;
				ApplyTurnForce(GetRight() * turnForce, m_vecCentreOfMass + GetRight());
			}
		}
	}

	// The rider's side-to-side lean lags the steering with the handling's
	// DesLean rate.
	const float decay = Pow(m_pBikeHandling->m_fDesLean, CTimer::GetTimeStep());
	m_sRideAnimData.LeanAngle = decay * m_sRideAnimData.LeanAngle
		- m_pBikeHandling->m_fFullAnimLean * m_fSteerAngle
		/ DEGTORAD(m_pHandlingData->m_fSteeringLock) * (1.0f - decay);

	DoDriveByShootings();

	return false;
}

// SA CQuadBike::ProcessControl (0x6CDCC0). Unreversed in the reference.
//
// What the override does is damp the quad's turn speed in its own local space,
// so that it resists tipping over forwards and backwards. The reference keeps
// the body of the function after the stub, which is what is ported here.
void
CQuadBike::ProcessControl(void)
{
	if(GetStatus() != STATUS_PLAYER || !bDoQuadDamping){
		CAutomobile::ProcessControl();
		return;
	}

	CVector turnSpeedOS = Multiply3x3(GetMatrix(), m_vecTurnSpeed);

	float resistanceY = vecQuadResistance.y;
	float resistanceX = vecQuadResistance.x;

	bool frontUp = m_aSuspensionSpringRatio[CARWHEEL_FRONT_LEFT] >= 1.0f &&
	               m_aSuspensionSpringRatio[CARWHEEL_FRONT_RIGHT] >= 1.0f;
	bool rearUp = m_aSuspensionSpringRatio[CARWHEEL_REAR_LEFT] >= 1.0f &&
	              m_aSuspensionSpringRatio[CARWHEEL_REAR_RIGHT] >= 1.0f;

	if(frontUp){
		if(!rearUp && GetForward().z > 0.0f)
			resistanceX = vecQuadResistance.x - Min(0.07f,
				Abs(m_pBikeHandling->m_fWheelieAng - GetForward().z) * 0.25f);
	}else{
		if(m_aSuspensionSpringRatio[CARWHEEL_REAR_LEFT] == 1.0f &&
		   m_aSuspensionSpringRatio[CARWHEEL_REAR_RIGHT] == 1.0f){
			if(GetForward().z < 0.0f)
				resistanceX = vecQuadResistance.x * (0.9f + Min(0.1f,
					Abs(m_pBikeHandling->m_fStoppieAng - GetForward().z) * 0.3f));
		}else{
			resistanceY = 0.5f;
		}
	}

	CVector resistance;
	resistance.x = Pow(vecQuadResistance.x, CTimer::GetTimeStep());
	resistance.y = resistanceY/(turnSpeedOS.y*turnSpeedOS.y + 1.0f);
	resistance.z = 1.0f;

	CVector centreOfMassOS = Multiply3x3(GetMatrix(), m_vecCentreOfMass);

	// Pitch damping about the right axis.
	const float v9 = Pow(resistance.y, CTimer::GetTimeStep())*turnSpeedOS.y - turnSpeedOS.y;
	ApplyTurnForce(GetUp()*-1.0f*v9*m_fTurnMass, GetRight() + centreOfMassOS);

	// Roll damping about the forward axis.
	const float v19 = turnSpeedOS.x*resistance.x - turnSpeedOS.x;
	ApplyTurnForce(GetUp()*v19*m_fTurnMass, GetForward() + centreOfMassOS);

	CAutomobile::ProcessControl();
}

// SA CQuadBike::ProcessControlInputs (0x6CE020).
void
CQuadBike::ProcessControlInputs(uint8 playerNum)
{
	CAutomobile::ProcessControlInputs(playerNum);

	CPad *pad = CPad::GetPad(playerNum);

	// The rider's fore/aft lean is the up/down stick, eased in over five
	// seconds of frame time so it does not snap.
	m_sRideAnimData.LeanFwd += (-(float)pad->GetSteeringUpDown()/128.0f - m_sRideAnimData.LeanFwd)
		* CTimer::GetTimeStep()/5.0f;

	m_sRideAnimData.LeanFwd = Clamp(m_sRideAnimData.LeanFwd, -1.0f, 1.0f);

	if(pad->DisablePlayerControls)
		m_sRideAnimData.LeanFwd = 0.0f;
}

// SA CQuadBike::ProcessDrivingAnims (0x6CE280).
//
// The reference calls CBike::ProcessRiderAnims with the quad's own ride data.
// That function is itself unreversed in the reference, so what is done here is
// the same thing CBike does for its own riders: the rider is attached to the
// vehicle's driver seat and the vehicle's animation group runs.
void
CQuadBike::ProcessDrivingAnims(CPed *driver, bool blend)
{
	CAutomobile::ProcessDrivingAnims(driver, blend);
}

// SA CQuadBike::ProcessSuspension (0x6CE270).
void
CQuadBike::ProcessSuspension(void)
{
	CAutomobile::ProcessSuspension();
}

// SA CQuadBike::ResetSuspension (0x6CDCB0).
void
CQuadBike::ResetSuspension(void)
{
	CAutomobile::ResetSuspension();
}

// SA CQuadBike::SetupDamageAfterLoad (0x6CE340).
void
CQuadBike::SetupDamageAfterLoad(void)
{
	// A quad has no bumpers or bonnet: the front body panel stands in for the
	// bumper and the rear one for the boot.
	if(m_aCarNodes[QUAD_BODY_FRONT])
		CAutomobile::SetBumperDamage(CAR_BUMP_FRONT, VEHPANEL_FRONT_LEFT, false);
	if(m_aCarNodes[QUAD_BODY_REAR])
		CAutomobile::SetDoorDamage(CAR_BOOT, DOOR_BOOT, false);
}

// SA CQuadBike::SetupSuspensionLines (0x6CDCA0).
void
CQuadBike::SetupSuspensionLines(void)
{
	field_9A8[0] = 1.0f;
	CAutomobile::SetupSuspensionLines();
}
