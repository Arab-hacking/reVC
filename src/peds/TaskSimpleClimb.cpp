#include "common.h"

#include "TaskSimpleClimb.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "World.h"
#include "Collision.h"
#include "AnimManager.h"
#include "AnimBlendHierarchy.h"
#include "RpAnimBlend.h"
#include "Stats.h"
#include "General.h"
#include "Timer.h"
#include "ModelInfo.h"
#include "Physical.h"
#include "Vehicle.h"
#include "Object.h"
#include "Pad.h"
#include "soundlist.h"

// The four collision models SA builds in CTaskSimpleClimb::CreateColModel
// (0x67A890). Each sphere's piece index is what ScanToGrab reads back out of
// the first collision point: the low indices are the spheres nearest the
// ground, and a hit on one of those means the wall is too low to grab. The
// sphere set and every piece index are SA's, sphere for sphere.
namespace {

struct ClimbSphereDef {
	CVector center;
	float radius;
	uint8 piece;
};

const ClimbSphereDef ClimbSpheres[] = {
	{ CVector(0.0f, -0.06f, 1.2f), 0.4f, 0 },
	{ CVector(0.0f, -0.06f, 1.7f), 0.4f, 1 },
	{ CVector(0.0f, -0.06f, 2.2f), 0.4f, 2 },
	{ CVector(0.0f, -0.06f, 2.5f), 0.4f, 3 },
	{ CVector(0.0f, +0.5f, 2.9f), 0.4f, 4 },
	{ CVector(0.0f, +0.5f, 2.4f), 0.4f, 5 },
	{ CVector(0.0f, +0.5f, 1.9f), 0.4f, 6 },
	{ CVector(0.0f, +0.5f, 1.4f), 0.4f, 7 },
	{ CVector(0.0f, +0.5f, 0.9f), 0.4f, 8 },
	{ CVector(0.0f, +0.5f, 0.4f), 0.4f, 9 },
	{ CVector(0.0f, +1.0f, 2.9f), 0.4f, 10 },
	{ CVector(0.0f, +1.0f, 2.4f), 0.4f, 11 },
	{ CVector(0.0f, +1.0f, 1.9f), 0.4f, 12 },
	{ CVector(0.0f, +1.0f, 1.4f), 0.4f, 13 },
	{ CVector(0.0f, +1.0f, 0.9f), 0.4f, 14 },
	{ CVector(0.0f, +1.0f, 0.4f), 0.4f, 15 },
	{ CVector(0.0f, +1.5f, 2.9f), 0.4f, 16 },
	{ CVector(0.0f, +1.5f, 2.4f), 0.4f, 17 },
	{ CVector(0.0f, +1.5f, 1.9f), 0.4f, 18 },
	{ CVector(0.0f, +1.5f, 1.4f), 0.4f, 19 },
	{ CVector(0.0f, +1.5f, 0.9f), 0.4f, 20 },
	{ CVector(0.0f, +1.5f, 0.4f), 0.4f, 21 },
};

const ClimbSphereDef StandUpSpheres[] = {
	{ CVector(0.0f, 0.6f, 1.75f), 0.35f, 0 },
	{ CVector(0.0f, 0.6f, 1.30f), 0.35f, 1 },
	{ CVector(0.0f, 0.6f, 0.85f), 0.35f, 2 },
	{ CVector(0.0f, 0.3f, 0.65f), 0.35f, 3 },
	{ CVector(0.0f, 0.0f, 0.50f), 0.35f, 4 },
	{ CVector(0.0f, 0.0f, 0.10f), 0.35f, 5 },
	{ CVector(0.0f, 0.0f, -0.30f), 0.35f, 6 },
};

const ClimbSphereDef VaultSpheres[] = {
	{ CVector(0.0f, 1.10f, -0.2f), 0.3f, 0 },
	{ CVector(0.0f, 1.10f, +0.2f), 0.3f, 1 },
	{ CVector(0.0f, 1.10f, +0.6f), 0.3f, 2 },
	{ CVector(0.0f, 0.70f, +0.6f), 0.3f, 3 },
	{ CVector(0.0f, 0.35f, +0.6f), 0.3f, 4 },
	{ CVector(0.0f, 0.00f, +0.6f), 0.3f, 5 },
};

const ClimbSphereDef FindEdgeSpheres[] = {
	{ CVector(0.0f, -0.5f, +0.5f), 0.3f, 0 },
	{ CVector(0.0f, -0.5f, +0.3f), 0.3f, 1 },
	{ CVector(0.0f, -0.5f, +0.1f), 0.3f, 2 },
	{ CVector(0.0f, -0.5f, -0.1f), 0.3f, 3 },
	{ CVector(0.0f, -0.3f, +0.5f), 0.3f, 4 },
	{ CVector(0.0f, -0.3f, +0.3f), 0.3f, 5 },
	{ CVector(0.0f, -0.3f, +0.1f), 0.3f, 6 },
	{ CVector(0.0f, -0.3f, -0.1f), 0.3f, 7 },
	{ CVector(0.0f, -0.1f, +0.5f), 0.3f, 8 },
	{ CVector(0.0f, -0.1f, +0.3f), 0.3f, 9 },
	{ CVector(0.0f, -0.1f, +0.1f), 0.3f, 10 },
	{ CVector(0.0f, -0.1f, -0.1f), 0.3f, 11 },
	{ CVector(0.0f, +0.1f, +0.5f), 0.3f, 12 },
	{ CVector(0.0f, +0.1f, +0.3f), 0.3f, 13 },
	{ CVector(0.0f, +0.1f, +0.1f), 0.3f, 14 },
	{ CVector(0.0f, +0.1f, -0.1f), 0.3f, 15 },
};

// The bound sphere and box of each model, SA's numbers.
struct ClimbModelDef {
	const ClimbSphereDef *spheres;
	int32 numSpheres;
	CSphere boundSphere;
	CBox boundBox;
};

CColModel ms_ClimbColModel;
CColModel ms_StandUpColModel;
CColModel ms_VaultColModel;
CColModel ms_FindEdgeColModel;
bool ms_bColModelsCreated = false;

// Allocate a collision model from a sphere table. reVC's CColModel is a
// plain struct with an owned sphere array, so the allocation is explicit
// rather than SA's AllocateData call.
void
BuildColModel(CColModel &model, const ClimbSphereDef *defs, int32 count,
              float boundRadius, CVector boundCenter, CVector boxMin, CVector boxMax)
{
	model.RemoveCollisionVolumes();

	model.numSpheres = (int16)count;
	model.numLines = 0;
	model.numBoxes = 0;
	model.numVertices = 0;
	model.numTriangles = 0;
	model.spheres = (CColSphere *)RwMalloc(sizeof(CColSphere) * count);
	model.sphereW = nil;
	model.lines = nil;
	model.boxes = nil;
	model.vertices = nil;
	model.triangles = nil;
	model.planes = nil;
	model.tree = nil;
	model.ownsCollisionVolumes = true;

	for(int32 i = 0; i < count; i++){
		// reVC's CColSphere calls SA's piece index "material": it is the field
		// Collision writes into CColPoint::pieceA, which is what the scan reads
		// back. Any surface works for a probe model - nothing uses its
		// friction - so it stays SURFACE_DEFAULT.
		model.spheres[i].Set(defs[i].radius, defs[i].center, 0, SURFACE_DEFAULT, defs[i].piece);
	}

	model.boundingSphere.radius = boundRadius;
	model.boundingSphere.center = boundCenter;
	model.boundingBox.min = boxMin;
	model.boundingBox.max = boxMax;
}

} // namespace

bool
CTaskSimpleClimb::CreateColModel()
{
	BuildColModel(ms_ClimbColModel, ClimbSpheres, ARRAY_SIZE(ClimbSpheres),
	              2.02f, CVector(0.0f, 0.75f, 1.65f),
	              CVector(-0.4f, -0.46f, 0.0f), CVector(0.4f, 1.9f, 3.3f));

	BuildColModel(ms_StandUpColModel, StandUpSpheres, ARRAY_SIZE(StandUpSpheres),
	              1.5f, CVector(0.0f, 0.3f, 0.7f),
	              CVector(-0.35f, -0.35f, -0.65f), CVector(0.35f, 0.95f, 2.1f));

	BuildColModel(ms_VaultColModel, VaultSpheres, ARRAY_SIZE(VaultSpheres),
	              1.15f, CVector(0.0f, 0.5f, 0.1f),
	              CVector(-0.35f, -0.35f, -0.6f), CVector(0.35f, 1.3f, 0.85f));

	BuildColModel(ms_FindEdgeColModel, FindEdgeSpheres, ARRAY_SIZE(FindEdgeSpheres),
	              0.9f, CVector(0.0f, -0.2f, 0.2f),
	              CVector(-0.3f, -0.8f, -0.4f), CVector(0.3f, 0.4f, 0.8f));

	ms_bColModelsCreated = true;
	return true;
}

void
CTaskSimpleClimb::Shutdown()
{
	ms_ClimbColModel.RemoveCollisionVolumes();
	ms_StandUpColModel.RemoveCollisionVolumes();
	ms_VaultColModel.RemoveCollisionVolumes();
	ms_FindEdgeColModel.RemoveCollisionVolumes();
	ms_bColModelsCreated = false;
}

// The climb offsets are given in the ped's local space: horiz along the
// handhold's right axis, vert straight up. SA builds them from the handhold
// angle so that the offset stays perpendicular to the wall.
CVector
CTaskSimpleClimb::GetClimbOffset3D(CVector2D offset2D, float angle)
{
	// (cos, sin) * horiz, rotated a quarter turn left, then the vertical part.
	CVector2D dir(Cos(angle), Sin(angle));
	CVector2D perpLeft(dir.x * 0.0f - dir.y * 1.0f, dir.y * 0.0f + dir.x * 1.0f);
	return CVector(perpLeft.x * offset2D.x, perpLeft.y * offset2D.x, offset2D.y);
}

void
CTaskSimpleClimb::GetHandholdPosAndAngleForEntity(CEntity *entity, const CVector &handPos,
                                                  float handAngle, CVector &outPos, float &outAngle)
{
	if(entity && (entity->IsVehicle() || entity->IsObject() || entity->IsPed())){
		// The hold was stored in the entity's own space for a moving carrier.
		RwV3d in, out;
		in.x = handPos.x; in.y = handPos.y; in.z = handPos.z;
		RwV3dTransformPoints(&out, &in, 1, entity->GetMatrix().m_attachment);
		outPos = CVector(out.x, out.y, out.z);
		outAngle = handAngle + entity->GetHeading();
	}else{
		outPos = handPos;
		outAngle = handAngle;
	}
}

CTaskSimpleClimb::CTaskSimpleClimb(CEntity *climbEntity, const CVector &vecTarget, float fHeading,
                                   eSurfaceType nSurfaceType, eClimbHeights nHeight, bool bForceClimb)
{
	m_HasFinished = false;
	m_HasToChangeAnimation = false;
	m_HasToChangePosition = false;
	m_HasToForceClimb = bForceClimb;
	m_IsInvalidClimb = false;
	m_HeightForAnim = nHeight;
	m_HeightForPos = CLIMB_NOT_READY;
	m_SurfaceType = nSurfaceType;
	m_FallAfterVault = -1;
	m_HandholdHeading = fHeading;
	m_HandholdPos = vecTarget;
	m_ClimbEntity = climbEntity;
	m_GetToPosCounter = 0;
	m_Anim = nil;
}

CTaskSimpleClimb::~CTaskSimpleClimb()
{
	if(m_Anim){
		m_Anim->SetDeleteCallback(nil, nil);
		m_Anim = nil;
	}
}

// SA CTaskSimpleClimb::StartAnim (0x67DBE0): blend the clip for the phase that
// is about to play. The pull-up, stand and vault clips run at the speed the
// player's body stats allow, which is what STAT_MOD_1 is for.
void
CTaskSimpleClimb::StartAnim(CPed *ped)
{
	switch(m_HeightForAnim){
	case CLIMB_GRAB:
		if(m_Anim){
			m_Anim->SetDeleteCallback(nil, nil);
			m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_IDLE, 4.0f);
		}else{
			m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_JUMP, 8.0f);
		}
		m_HeightForPos = CLIMB_GRAB;
		m_HasToChangePosition = false;
		m_HasToChangeAnimation = false;
		break;

	case CLIMB_PULLUP:
		if(m_HeightForPos == CLIMB_NOT_READY){
			m_HeightForAnim = CLIMB_STANDUP;
			m_HeightForPos = CLIMB_STANDUP;
			m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_STAND, 4.0f);
			if(m_Anim)
				m_Anim->m_Flags &= ~ANIMATION_IS_PLAYING;
		}else{
			if(m_Anim)
				m_Anim->SetDeleteCallback(nil, nil);
			m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_PULL, 1000.0f);
			m_HeightForPos = CLIMB_PULLUP;
		}
		m_HasToChangePosition = false;
		m_HasToChangeAnimation = false;
		break;

	case CLIMB_STANDUP:
		if(m_Anim)
			m_Anim->SetDeleteCallback(nil, nil);
		m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_STAND, 1000.0f);
		m_HasToChangePosition = true;
		m_HasToChangeAnimation = false;
		break;

	case CLIMB_FINISHED:
	case CLIMB_FINISHED_V:
		CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, 1000.0f);
		ped->SetMoveState(PEDMOVE_STILL);
		ped->SetMoveAnim();
		if(ped->IsPlayer())
			((CPlayerPed *)ped)->MoveBlendRatio() = 0.0f;
		if(m_Anim)
			m_Anim->SetDeleteCallback(nil, nil);
		m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT,
			m_HeightForAnim == CLIMB_FINISHED_V ? ANIM_ID_CLIMB_JUMP2FALL : ANIM_ID_CLIMB_STAND_FINISH, 1000.0f);
		m_HasToChangePosition = true;
		m_HasToChangeAnimation = false;
		break;

	case CLIMB_VAULT:
		if(m_Anim)
			m_Anim->SetDeleteCallback(nil, nil);
		m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CLIMB_JUMP_B,
			m_Anim && m_Anim->m_AnimId == ANIM_ID_CLIMB_STAND ? 16.0f : 1000.0f);
		m_HasToChangePosition = true;
		m_HasToChangeAnimation = false;
		break;

	default:
		break;
	}

	if(m_Anim){
		m_Anim->SetDeleteCallback(DeleteAnimCB, this);
		if(ped->IsPlayer() && (m_Anim->m_Flags & ANIMATION_IS_PLAYING)
		   && (m_Anim->m_AnimId == ANIM_ID_CLIMB_PULL || m_Anim->m_AnimId == ANIM_ID_CLIMB_STAND
		       || m_Anim->m_AnimId == ANIM_ID_CLIMB_JUMP_B)){
			m_Anim->m_Speed = CStats::GetFatAndMuscleModifier(CStats::STAT_MOD_1);
		}
	}
}

void
CTaskSimpleClimb::StartSpeech(CPed *ped) const
{
	if(ped->IsPlayer()){
		if(m_HeightForAnim == CLIMB_PULLUP)
			ped->Say(SOUND_PED_ATTACK);
		else if(m_HeightForAnim == CLIMB_STANDUP)
			ped->Say(SOUND_PED_ATTACK);
	}
}

void
CTaskSimpleClimb::DeleteAnimCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleClimb *self = (CTaskSimpleClimb *)data;
	if(self)
		self->m_Anim = nil;
}

// SA CTaskSimpleClimb::ProcessPed (0x680DC0). Returns true once the climb is
// over; the ped then drops this object.
bool
CTaskSimpleClimb::ProcessPed(CPed *ped)
{
	if(m_HasFinished)
		return true;

	if(m_ClimbEntity == nil)
		return true;

	// A carrier that is moving too fast (a train under power) cannot be
	// climbed, and neither can an entity whose collision has just been
	// switched off.
	if(m_ClimbEntity->IsVehicle()){
		CVehicle *vehicle = (CVehicle *)m_ClimbEntity;
		if(vehicle->IsTrain() && vehicle->GetMoveSpeed().Magnitude() > 0.1f){
			m_HasFinished = true;
			return true;
		}
	}

	if(m_Anim == nil || m_HasToChangeAnimation){
		StartAnim(ped);
		StartSpeech(ped);
	}

	CVector handPos;
	float handAngle;
	GetHandholdPosAndAngleForEntity(m_ClimbEntity, m_HandholdPos, m_HandholdHeading, handPos, handAngle);

	bool newHeightForPos = m_HasToChangePosition && m_Anim && m_Anim->m_BlendAmount == 1.0f;
	if(newHeightForPos){
		m_HeightForPos = m_HeightForAnim;
		m_HasToChangePosition = false;
	}

	if(m_HeightForPos > CLIMB_NOT_READY){
		CVector2D offset;
		switch(m_HeightForPos){
		case CLIMB_GRAB:
		case CLIMB_PULLUP: offset = CVector2D(ms_fHangingOffsetHorz, ms_fHangingOffsetVert); break;
		case CLIMB_STANDUP:
		case CLIMB_VAULT: offset = CVector2D(ms_fAtEdgeOffsetHorz, ms_fAtEdgeOffsetVert); break;
		case CLIMB_FINISHED: offset = CVector2D(ms_fStandUpOffsetHorz, ms_fStandUpOffsetVert); break;
		case CLIMB_FINISHED_V: offset = CVector2D(ms_fVaultOffsetHorz, ms_fVaultOffsetVert); break;
		default: offset = CVector2D(0.0f, 0.0f); break;
		}

		CVector targetPt = handPos + GetClimbOffset3D(offset, handAngle);

		CVector climbEntSpeed(0.0f, 0.0f, 0.0f);
		CVector delta = targetPt - ped->GetPosition();
		if((m_ClimbEntity->IsVehicle() || m_ClimbEntity->IsObject()))
			climbEntSpeed = ((CPhysical *)m_ClimbEntity)->GetSpeed(targetPt - m_ClimbEntity->GetPosition());

		if(newHeightForPos){
			ped->SetPosition(targetPt);
			ped->m_vecMoveSpeed = climbEntSpeed;
		}else if(delta.MagnitudeSqr() >= 0.1f){
			ped->bIsStanding = false;
			delta *= 0.25f;
			ped->m_vecMoveSpeed = delta / CTimer::GetTimeStep();

			// SA bounds the settle speed at 0.2: the position correction is not
			// a speed, and dividing a few centimetres by the frame time turns
			// it into one that the landing check reads as a fall.
			float speed = ped->m_vecMoveSpeed.Magnitude();
			if(speed > 0.2f)
				ped->m_vecMoveSpeed *= 0.2f / speed;

			ped->m_vecMoveSpeed += climbEntSpeed;

			if(m_Anim && (!(m_Anim->m_Flags & ANIMATION_IS_PLAYING) || m_Anim->m_AnimId == ANIM_ID_CLIMB_IDLE)){
				m_GetToPosCounter += (uint16)CTimer::GetTimeStepInMilliseconds();
				if(m_GetToPosCounter > 1000
				   || (m_Anim->m_AnimId == ANIM_ID_CLIMB_IDLE && m_GetToPosCounter > 500)){
					m_IsInvalidClimb = true;
					m_Anim->m_BlendDelta = -1000.0f;
					m_Anim = nil;
					m_HasFinished = true;
					ped->ApplyMoveForce(ped->GetForward() * (ped->m_fMass * -0.1f));
					return true;
				}
			}
		}else{
			ped->bIsStanding = false;
			ped->m_vecMoveSpeed = delta / CTimer::GetTimeStep();

			float speed = ped->m_vecMoveSpeed.Magnitude();
			if(speed > 0.2f)
				ped->m_vecMoveSpeed *= 0.2f / speed;

			ped->m_vecMoveSpeed += climbEntSpeed;

			if(m_Anim && !(m_Anim->m_Flags & ANIMATION_IS_PLAYING) && m_HeightForAnim == CLIMB_STANDUP){
				if(TestForVault(ped, handPos, handAngle)){
					m_HeightForAnim = CLIMB_VAULT;
					m_HasToChangeAnimation = true;
				}else{
					m_Anim->m_Flags |= ANIMATION_IS_PLAYING;
				}
			}
		}
	}

	if(newHeightForPos){
		if(m_HeightForPos == CLIMB_FINISHED || m_HeightForPos == CLIMB_FINISHED_V){
			m_HasFinished = true;
			ped->bIsInTheAir = false;
			ped->bIsLanding = false;

			if(m_HeightForPos == CLIMB_FINISHED_V){
				ped->m_vecMoveSpeed = ped->GetForward() / 50.0f + CVector(0.0f, 0.0f, -0.05f);
				ped->bIsStanding = false;
				ped->bWasStanding = true;
			}else{
				ped->m_vecMoveSpeed = ped->GetForward() * 0.05f + CVector(0.0f, 0.0f, -0.01f);
				ped->bIsStanding = true;
				ped->bWasStanding = true;
			}

			if(m_Anim){
				m_Anim->m_BlendDelta = -8.0f;
				m_Anim = nil;
			}

			return true;
		}
	}

	CPad *pad = ped->IsPlayer() ? CPad::GetPad(0) : nil;

	// Advance the phase when the current clip has played out.
	if(m_Anim && m_Anim->m_BlendAmount == 1.0f){
		bool advance = false;
		float totalTime = m_Anim->m_BlendHier ? m_Anim->m_BlendHier->m_fTotalTime : 0.0f;
		float timeLeft = totalTime - (m_Anim->m_CurrentTime + m_Anim->m_TimeStep);

		switch(m_Anim->m_AnimId){
		case ANIM_ID_CLIMB_JUMP:
			if(timeLeft <= 0.0f){
				if(m_HasToForceClimb || (pad && pad->GetJump()))
					advance = true;
				StartAnim(ped);
			}
			break;
		case ANIM_ID_CLIMB_IDLE:
			if(m_HasToForceClimb || pad == nil || pad->GetJump())
				advance = true;
			break;
		case ANIM_ID_CLIMB_PULL:
		case ANIM_ID_CLIMB_STAND:
			if(totalTime <= m_Anim->m_CurrentTime)
				advance = true;
			break;
		case ANIM_ID_CLIMB_JUMP_B:
			if(timeLeft <= 0.0f)
				advance = true;
			break;
		default:
			if(totalTime <= m_Anim->m_CurrentTime){
				if(m_HasToForceClimb || (pad && pad->JumpJustDown()))
					advance = true;
			}
			break;
		}

		if(advance){
			m_HeightForAnim = (eClimbHeights)(m_HeightForAnim + 1);
			m_HasToChangeAnimation = true;
		}
	}

	if(pad && pad->GetExitVehicle()){
		m_HasFinished = true;
		return true;
	}

	if(m_HeightForPos != CLIMB_STANDUP && m_HeightForPos != CLIMB_VAULT
	   || m_Anim == nil || !(m_Anim->m_Flags & ANIMATION_IS_PLAYING)){
		if(m_HeightForAnim == CLIMB_STANDUP && m_HeightForPos < CLIMB_STANDUP
		   && TestForVault(ped, handPos, handAngle))
			m_HeightForAnim = CLIMB_VAULT;
	}else if(!TestForStandUp(ped, handPos, handAngle)){
		m_HasFinished = true;
		return true;
	}

	ped->m_fRotationDest = ped->m_fRotationCur = handAngle;
	ped->SetOrientation(0.0f, 0.0f, handAngle);

	return false;
}

// SA CTaskSimpleClimb::TestForClimb (0x6803A0).
CEntity *
CTaskSimpleClimb::TestForClimb(CPed *ped, CVector &outClimbPos, float &outClimbHeading,
                               eSurfaceType &outSurfaceType, bool bLaunch)
{
	if(CEntity *entity = ScanToGrab(ped, outClimbPos, outClimbHeading, outSurfaceType, bLaunch, false, false, nil)){
		CVector pos;
		float angle;
		GetHandholdPosAndAngleForEntity(entity, outClimbPos, outClimbHeading, pos, angle);
		pos += GetClimbOffset3D(CVector2D(ms_fAtEdgeOffsetHorz, ms_fAtEdgeOffsetVert), angle);

		CVector grabPos;
		eSurfaceType grabSurface;
		float grabAngle = outClimbHeading;
		// If a second hold is found just behind the first, the ped is standing
		// in a niche and SA refuses the climb.
		if(!ScanToGrab(ped, grabPos, grabAngle, grabSurface, false, true, false, &pos))
			return entity;
		outClimbHeading = -9999.9f;
	}
	return nil;
}

// SA CTaskSimpleClimb::ScanToGrabSectorList (0x67DE10). Returns the grabbed
// entity, or the sentinel (CEntity*)1 when the hit was on a sphere that must
// cancel the whole scan (the ped is already at a wall it cannot grab).
static CEntity *
ScanToGrabSectorList(CPtrList &list, CPed *ped, CColModel &colModel, CVector &outTargetPos,
                     float &outAngle, eSurfaceType &outSurfaceType, bool isLaunch,
                     bool hasToTestStandup, bool hasToTestDropOtherSide)
{
	CEntity *collidedEntity = nil;
	CVector pedPos = ped->GetPosition();

	for(CPtrNode *node = list.first; node; node = node->next){
		CEntity *entity = (CEntity *)node->item;

		if(entity->m_scanCode == CWorld::GetCurrentScanCode())
			continue;
		entity->m_scanCode = CWorld::GetCurrentScanCode();

		if(!entity->bUsesCollision)
			continue;

		bool eligible = entity->IsBuilding()
			|| (entity->IsObject() && (entity->GetIsStatic() || ((CPhysical *)entity)->bHasContacted)
			    && !((CPhysical *)entity)->bInfiniteMass)
			|| (entity->IsVehicle() && (hasToTestStandup || hasToTestDropOtherSide
			    || (((CVehicle *)entity)->IsBoat())));

		if(!eligible)
			continue;

		CColModel *entityCol = entity->GetColModel();
		if(entityCol == nil)
			continue;

		// Broad phase: bound spheres first.
		CVector modelCenter = colModel.boundingSphere.center;
		RwV3d center;
		RwV3d in;
		in.x = modelCenter.x; in.y = modelCenter.y; in.z = modelCenter.z;
		RwV3dTransformPoints(&center, &in, 1, ped->GetMatrix().m_attachment);
		CVector worldCenter(center.x, center.y, center.z);

		if((entity->GetBoundCentre() - worldCenter).Magnitude()
		   >= entity->GetBoundRadius() + colModel.boundingSphere.radius)
			continue;

		if(CCollision::ProcessColModels(ped->GetMatrix(), colModel, entity->GetMatrix(), *entityCol,
		                                CWorld::m_aTempColPts, nil, nil) <= 0)
			continue;

		if(hasToTestStandup || hasToTestDropOtherSide)
			return entity;

		CColPoint &colPoint = CWorld::m_aTempColPts[0];
		uint8 sphereIndex = colPoint.pieceA;

		// The four lowest climb spheres and the 1.0m row cancel the scan: the
		// ped is touching a wall at hip height, not a ledge it can hang from.
		if(sphereIndex <= 4 || sphereIndex == 10)
			return (CEntity *)1;

		CVector relPos = colPoint.point - outTargetPos;
		if(sphereIndex == 16
		   || (colPoint.point.z <= outTargetPos.z && DotProduct(relPos, ped->GetForward()) >= 0.0f)){
			continue;
		}

		// The fat/muscle modifier widens the reachable band: a heavier ped can
		// grab the higher rows.
		int32 fatModifier = (ped->IsPlayer()) ? (int32)CStats::GetFatAndMuscleModifier(CStats::STAT_MOD_0) : 0;

		if(!isLaunch || fatModifier >= 1){
			if(sphereIndex == 5 || sphereIndex == 11 || sphereIndex == 17)
				continue;

			if(fatModifier >= 1 && (!isLaunch || fatModifier >= 2)){
				if(sphereIndex == 6 || sphereIndex == 12 || sphereIndex == 18)
					continue;
			}
		}

		if(Abs(colPoint.normal.x) <= 0.05f && Abs(colPoint.normal.y) <= 0.05f){
			outTargetPos = colPoint.point;
			outAngle = ped->m_fRotationCur;
			outSurfaceType = (eSurfaceType)colPoint.surfaceB;
			collidedEntity = entity;
		}else{
			CVector normal = colPoint.normal;

			if(DotProduct2D(normal, colPoint.point - pedPos) < 0.0f)
				normal = -normal;

			if(DotProduct(normal, ped->GetForward()) > 0.3f){
				outTargetPos = colPoint.point;
				outAngle = Atan2(-normal.x, normal.y);
				outSurfaceType = (eSurfaceType)colPoint.surfaceB;
				collidedEntity = entity;
			}
		}

		// Second pass with the edge model at the point just found: this is what
		// turns a wall contact into an actual ledge.
		CMatrix matrix(ped->GetMatrix());
		matrix.SetTranslateOnly(colPoint.point);
		if(CCollision::ProcessColModels(matrix, ms_FindEdgeColModel, entity->GetMatrix(), *entityCol,
		                                CWorld::m_aTempColPts, nil, nil) > 0){
			CColPoint &edgePoint = CWorld::m_aTempColPts[0];
			if(Abs(edgePoint.normal.x) <= 0.05f && Abs(edgePoint.normal.y) <= 0.05f){
				outTargetPos = edgePoint.point;
				outSurfaceType = (eSurfaceType)edgePoint.surfaceB;
			}else{
				CVector normal = edgePoint.normal;
				if(DotProduct2D(normal, edgePoint.point - pedPos) < 0.0f)
					normal = -normal;
				if(DotProduct(normal, ped->GetForward()) > 0.3f){
					outTargetPos = edgePoint.point;
					outAngle = Atan2(-normal.x, normal.y);
					outSurfaceType = (eSurfaceType)edgePoint.surfaceB;
					collidedEntity = entity;
				}
			}
		}
	}

	return collidedEntity;
}

// SA CTaskSimpleClimb::ScanToGrab (0x67FD30): sweep the sectors around the
// ped's climb model. Buildings first, then objects, then vehicles.
CEntity *
CTaskSimpleClimb::ScanToGrab(CPed *ped, CVector &outClimbPos, float &outClimbAngle,
                             eSurfaceType &surfaceType, bool bLaunch, bool bStandUp,
                             bool bVault, CVector *pedPosition)
{
	if(!ms_bColModelsCreated)
		CreateColModel();

	CColModel &colModel = bVault ? ms_VaultColModel : (bStandUp ? ms_StandUpColModel : ms_ClimbColModel);

	CEntity *collidedEntity = nil;
	CVector originalPedPosition = ped->GetPosition();

	if(pedPosition)
		ped->SetPosition(*pedPosition);

	outClimbPos = ped->GetPosition() + ped->GetForward() * 10.0f;

	// Where the model's bound sphere sits in the world.
	RwV3d center;
	RwV3d in;
	in.x = colModel.boundingSphere.center.x;
	in.y = colModel.boundingSphere.center.y;
	in.z = colModel.boundingSphere.center.z;
	RwV3dTransformPoints(&center, &in, 1, ped->GetMatrix().m_attachment);

	float radius = colModel.boundingSphere.radius;
	int32 startSectorX = (int32)CWorld::GetSectorX(center.x - radius);
	int32 startSectorY = (int32)CWorld::GetSectorY(center.y - radius);
	int32 endSectorX = (int32)CWorld::GetSectorX(center.x + radius);
	int32 endSectorY = (int32)CWorld::GetSectorY(center.y + radius);

	CWorld::AdvanceCurrentScanCode();

	for(int32 y = startSectorY; y <= endSectorY; y++){
		for(int32 x = startSectorX; x <= endSectorX; x++){
			CSector *sector = CWorld::GetSector(x, y);
			if(sector == nil)
				continue;

			CEntity *result1 = ScanToGrabSectorList(sector->m_lists[ENTITYLIST_BUILDINGS], ped, colModel,
			                                        outClimbPos, outClimbAngle, surfaceType, bLaunch,
			                                        bStandUp, bVault);
			CEntity *result2 = ScanToGrabSectorList(sector->m_lists[ENTITYLIST_OBJECTS], ped, colModel,
			                                        outClimbPos, outClimbAngle, surfaceType, bLaunch,
			                                        bStandUp, bVault);
			if(result2 == nil)
				result2 = ScanToGrabSectorList(sector->m_lists[ENTITYLIST_VEHICLES], ped, colModel,
				                               outClimbPos, outClimbAngle, surfaceType, bLaunch,
				                               bStandUp, bVault);

			if(result1 == (CEntity *)1 || result2 == (CEntity *)1){
				if(pedPosition)
					ped->SetPosition(originalPedPosition);
				return nil;
			}

			CEntity *entity = result2 ? result2 : result1;
			if(entity){
				if(bStandUp || bVault){
					if(pedPosition)
						ped->SetPosition(originalPedPosition);
					return entity;
				}
				collidedEntity = entity;
			}
		}
	}

	if(pedPosition)
		ped->SetPosition(originalPedPosition);

	// The hold was stored in world space; a physical carrier moves, so the
	// hold has to be expressed in the carrier's space to survive that.
	if(collidedEntity && (collidedEntity->IsVehicle() || collidedEntity->IsObject())){
		RwV3d in;
		in.x = outClimbPos.x; in.y = outClimbPos.y; in.z = outClimbPos.z;
		RwV3d out;
		// Inverse of the carrier's matrix.
		CMatrix inv;
		Invert(collidedEntity->GetMatrix(), inv);
		RwV3dTransformPoints(&out, &in, 1, inv.m_attachment);
		outClimbPos = CVector(out.x, out.y, out.z);
		outClimbAngle -= collidedEntity->GetHeading();
	}

	return collidedEntity;
}

// SA CTaskSimpleClimb::TestForStandUp (0x680570): is there room to stand at the
// edge? True when nothing blocks the stand-up model.
bool
CTaskSimpleClimb::TestForStandUp(CPed *ped, const CVector &point, float fAngle) const
{
	CVector climbPos;
	float angle;
	eSurfaceType surfaceType;
	CVector pedPos = point + GetClimbOffset3D(CVector2D(ms_fAtEdgeOffsetHorz, ms_fAtEdgeOffsetVert), fAngle);
	return ScanToGrab(ped, climbPos, angle, surfaceType, false, true, false, &pedPos) == nil;
}

// SA CTaskSimpleClimb::TestForVault (0x6804D0): a low obstacle the ped can
// swing over instead of pulling up onto.
bool
CTaskSimpleClimb::TestForVault(CPed *ped, const CVector &point, float fAngle) const
{
	if(m_ClimbEntity && m_ClimbEntity->IsVehicle())
		return false;

	CVector climbPos;
	float angle;
	eSurfaceType surfaceType;
	CVector pedPos = point + GetClimbOffset3D(CVector2D(ms_fAtEdgeOffsetHorz, ms_fAtEdgeOffsetVert), fAngle);
	return ScanToGrab(ped, climbPos, angle, surfaceType, false, false, true, &pedPos) == nil;
}

// SA CTaskSimpleClimb::GetCameraTargetPos (0x67A390).
void
CTaskSimpleClimb::GetCameraTargetPos(CPed *ped, CVector &vecTarget) const
{
	vecTarget = ped->GetPosition();

	if(m_Anim == nil)
		return;

	CVector2D offset;
	switch(m_Anim->m_AnimId){
	case ANIM_ID_FALL_GLIDE:
	case ANIM_ID_CLIMB_JUMP2FALL:
		offset = CVector2D(ms_fVaultOffsetHorz, ms_fVaultOffsetVert);
		break;
	case ANIM_ID_CLIMB_PULL:
	case ANIM_ID_CLIMB_STAND:
	case ANIM_ID_CLIMB_STAND_FINISH:
	case ANIM_ID_CLIMB_JUMP_B:
		offset = CVector2D(ms_fAtEdgeOffsetHorz, ms_fAtEdgeOffsetVert);
		break;
	default:
		return;
	}

	CVector point;
	float angle;
	GetHandholdPosAndAngleForEntity(m_ClimbEntity, m_HandholdPos, m_HandholdHeading, point, angle);
	vecTarget = point + GetClimbOffset3D(offset, angle);
}
