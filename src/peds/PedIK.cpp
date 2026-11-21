#include "common.h"

#include "Bones.h"
#include "Camera.h"
#include "PedIK.h"
#include "Ped.h"
#include "General.h"
#include "RwHelper.h"
#include "RpAnimBlend.h"

LimbMovementInfo CPedIK::ms_torsoInfo = { DEGTORAD(50.0f), DEGTORAD(-50.0f), DEGTORAD(8.0f), DEGTORAD(45.0f), DEGTORAD(-45.0f), DEGTORAD(5.0f) };
LimbMovementInfo CPedIK::ms_headInfo = { DEGTORAD(90.0f), DEGTORAD(-90.0f), DEGTORAD(15.0f), DEGTORAD(45.0f), DEGTORAD(-45.0f), DEGTORAD(8.0f) };
LimbMovementInfo CPedIK::ms_headRestoreInfo = { DEGTORAD(90.0f), DEGTORAD(-90.0f), DEGTORAD(10.0f), DEGTORAD(45.0f), DEGTORAD(-45.0f), DEGTORAD(5.0f) };
LimbMovementInfo CPedIK::ms_upperArmInfo = { DEGTORAD(5.0f), DEGTORAD(-120.0f), DEGTORAD(20.0f), DEGTORAD(70.0f), DEGTORAD(-70.0f), DEGTORAD(20.0f) };
LimbMovementInfo CPedIK::ms_lowerArmInfo = { DEGTORAD(60.0f), DEGTORAD(0.0f), DEGTORAD(15.0f), DEGTORAD(90.0f), DEGTORAD(-90.0f), DEGTORAD(10.0f) };

const RwV3d XaxisIK = { 1.0f, 0.0f, 0.0f};
const RwV3d YaxisIK = { 0.0f, 1.0f, 0.0f};
const RwV3d ZaxisIK = { 0.0f, 0.0f, 1.0f};

CPedIK::CPedIK(CPed *ped) : m_ped(ped)
{
	assert(ped != nil);
	m_flags = 0;
	m_headOrient.yaw = 0.0f;
	m_headOrient.pitch = 0.0f;
	m_torsoOrient.yaw = 0.0f;
	m_torsoOrient.pitch = 0.0f;
	m_upperArmOrient.yaw = 0.0f;
	m_upperArmOrient.pitch = 0.0f;
	m_lowerArmOrient.yaw = 0.0f;
	m_lowerArmOrient.pitch = 0.0f;
	// SA CPedIK constructor zeroes the slope-lean state as well.
	m_fSlopePitch = 0.0f;
	m_fSlopePitchLimitMult = 0.0f;
	m_fSlopeRoll = 0.0f;
	m_fBodyRoll = 0.0f;
}

// A skeleton need not carry the bone an IK request names: a Vice City ped has
// no SA spine or clavicle, and a clump that is not skinned has no hierarchy at
// all. RpHAnimIDGetIndex answers -1 for a bone that is not there, and indexing
// the matrix array with it reads before the array, so the callers below get
// nothing to aim from instead.
inline RwMatrix*
GetBoneMatrix(CPed *ped, int32 bone)
{
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(ped->GetClump());
	if(hier == nil)
		return nil;
	int idx = RpHAnimIDGetIndex(hier, bone);
	if(idx < 0)
		return nil;
	RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
	if(mats == nil)
		return nil;
	return &mats[idx];
}
inline RwMatrix*
GetComponentMatrix(CPed *ped, int32 node)
{
	if(node < 0 || node >= PED_NODE_MAX || ped->m_pFrames[node] == nil)
		return nil;
	return GetBoneMatrix(ped, ped->m_pFrames[node]->nodeID);
}

inline void
RotateQuatIfPresent(RtQuat *q, const RwV3d *axis, float angle, RwOpCombineType mode)
{
	if(q == nil)
		return;
	RtQuatRotate(q, axis, angle, mode);
}

// A ped's frame table is filled from the frame names the clump carries, so a
// node the model does not have stays empty; the IK code writes the rotation
// straight into the animated frame.
inline RtQuat*
GetFrameQuat(CPed *ped, int32 node)
{
	if(ped == nil || node < 0 || node >= PED_NODE_MAX)
		return nil;
	AnimBlendFrameData *frame = ped->m_pFrames[node];
	if(frame == nil || frame->hanimFrame == nil)
		return nil;
	return &frame->hanimFrame->q;
}

void
CPedIK::RotateTorso(AnimBlendFrameData *node, LimbOrientation *limb, bool changeRoll)
{
	RtQuat *q = &node->hanimFrame->q;
	RotateQuatIfPresent(q, &XaxisIK, RADTODEG(limb->yaw), rwCOMBINEREPLACE);
	RotateQuatIfPresent(q, &ZaxisIK, RADTODEG(limb->pitch), rwCOMBINEPRECONCAT);
	m_ped->bDontAcceptIKLookAts = true;
}

// Ported from gta-reversed CPedIK::PitchForSlope (0x5FD...). SA leans the body
// on a slope: the spine is pitched back by part of the slope angle, both legs
// are re-pitched so the feet stay planted, and the whole ped is shifted along
// the ground normal. Without it a ped on a hill stands bolt upright, which is
// what made the skeleton look unlike SA.
void
CPedIK::PitchForSlope(void)
{
	const float CLAMP_EPS = 0.01f;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(m_ped->GetClump());

	if(fabsf(m_fBodyRoll) > CLAMP_EPS){
		if(m_fBodyRoll < -DEGTORAD(30.0f)) m_fBodyRoll = -DEGTORAD(30.0f);
		if(m_fBodyRoll >  DEGTORAD(30.0f)) m_fBodyRoll =  DEGTORAD(30.0f);
	}

	if(!m_ped->DyingOrDead()){
		if(fabsf(m_fSlopePitch) > CLAMP_EPS){
			// A walking ped leans a little less than one that is standing.
			float offset = (m_ped->m_nMoveState != PEDMOVE_WALK) ? 0.1f : 0.085f;
			m_fSlopePitchLimitMult = (m_fSlopePitchLimitMult * 0.9f) + offset;

			float lowBound  = m_fSlopePitchLimitMult * DEGTORAD(-35.0f);
			float highBound = m_fSlopePitchLimitMult * DEGTORAD(30.0f);
			if(m_fSlopePitch < lowBound)  m_fSlopePitch = lowBound;
			if(m_fSlopePitch > highBound) m_fSlopePitch = highBound;
		}

		if(fabsf(m_fSlopeRoll) > CLAMP_EPS){
			if(m_fSlopeRoll < -DEGTORAD(45.0f)) m_fSlopeRoll = -DEGTORAD(45.0f);
			if(m_fSlopeRoll >  DEGTORAD(45.0f)) m_fSlopeRoll =  DEGTORAD(45.0f);
		}

		// Rotation goes into the animated key frame of the named node, the same
		// place RotateTorso writes to.
		struct BoneRotator {
			CAnimBlendClumpData *data;
			CPed *ped;
			void operator()(int32 node, float degrees, const RwV3d &axis) const {
				if(node < 0 || node >= PED_NODE_MAX || ped->m_pFrames[node] == nil)
					return;
				RotateQuatIfPresent(&data->frames[ped->m_pFrames[node]->nodeID].hanimFrame->q,
					&axis, degrees, rwCOMBINEPRECONCAT);
			}
			void operator()(int32 node, float degrees) const {
				static const RwV3d zAxis = { 0.0f, 0.0f, 1.0f };
				(*this)(node, degrees, zAxis);
			}
		};
		BoneRotator rotate = { clumpData, m_ped };

		// Up vector of a node, as the reference implementation reads it.
		struct UpZ {
			CPed *ped;
			float operator()(int32 node) const {
				RwMatrix *m = GetComponentMatrix(ped, node);
				return m ? m->up.z : 0.0f;
			}
		};
		UpZ upZ = { m_ped };

		if(fabsf(m_fSlopePitch) > CLAMP_EPS){
			rotate(PED_MID, RADTODEG(-m_fSlopePitch) * 0.75f);

			// Re-pitch the legs so the feet follow the ground instead of
			// sinking into it. The sign of the thigh up-vector decides which
			// way each joint has to bend.
			rotate(PED_UPPERLEGR, asinf(upZ(PED_UPPERLEGR)) * 30.0f * m_fSlopePitch);
			rotate(PED_LOWERLEGR, Max(-asinf(upZ(PED_UPPERLEGR)), 0.0f) * -30.0f * m_fSlopePitch);
			rotate(PED_FOOTR,     Max( asinf(upZ(PED_FOOTR)),     0.0f) * -30.0f * m_fSlopePitch);
			rotate(PED_UPPERLEGL, asinf(upZ(PED_UPPERLEGL)) * 30.0f * m_fSlopePitch);
			rotate(PED_LOWERLEGL, Max(-asinf(upZ(PED_UPPERLEGL)), 0.0f) * -30.0f * m_fSlopePitch);
			rotate(PED_FOOTL,     Max(-asinf(upZ(PED_FOOTL)),     0.0f) * -30.0f * m_fSlopePitch);

			// Shift the ped so the tilted body still meets the ground.
			CMatrix result(m_ped->GetMatrix());
			CMatrix translate;
			translate.SetTranslate(CVector(0.0f, 0.0f, 0.0f));
			translate.RotateX(-m_fSlopePitch / 2.0f);
			result *= translate;

			result.GetPosition() += m_ped->GetForward() * 0.75f * sinf(m_fSlopePitch);
			result.GetPosition().z += 0.3f / cosf(m_fSlopeRoll) - 0.3f;
			result.UpdateRW();
			m_ped->UpdateRwFrame();
		}else{
			m_fSlopePitch = 0.0f;
		}

		if(fabsf(m_fSlopeRoll) > CLAMP_EPS){
			// Roll each foot around the axis it is actually pointing along.
			for(int32 side = 0; side < 2; side++){
				int32 node = side ? PED_FOOTR : PED_FOOTL;
				RwMatrix *hm = GetComponentMatrix(m_ped, node);
				if(hm == nil)
					continue;
				float angle = CGeneral::LimitRadianAngle(atan2f(hm->at.y, hm->at.x) - m_ped->m_fRotationCur);
				RwV3d axis = { 0.0f, Cos(angle), Sin(angle) };
				rotate(node, RADTODEG(m_fSlopeRoll), axis);
			}
		}else{
			m_fSlopeRoll = 0.0f;
		}
	}else{
		// Dying or dead: lay the whole body over instead of posing bones.
		CMatrix result(m_ped->GetMatrix());
		CMatrix translate;
		translate.SetTranslate(ZaxisIK);
		translate.RotateX(-m_fSlopePitch);
		translate.RotateY(m_fSlopeRoll);
		result *= translate;
		result.GetPosition().z -= 1.0f;
		result.UpdateRW();
		m_ped->UpdateRwFrame();
	}

	m_flags &= ~SLOPE_PITCH;
}

void
CPedIK::GetComponentPosition(RwV3d &pos, uint32 node)
{
	RwMatrix *m = GetComponentMatrix(m_ped, node);
	if (m)
		pos = m->pos;
}

LimbMoveStatus
CPedIK::MoveLimb(LimbOrientation &limb, float targetYaw, float targetPitch, LimbMovementInfo &moveInfo)
{
	LimbMoveStatus result = ONE_ANGLE_COULDNT_BE_SET_EXACTLY;

	// yaw

	if(Abs(limb.yaw-targetYaw) < moveInfo.yawD){
		limb.yaw = targetYaw;
		result = ANGLES_SET_EXACTLY;
	}else{
		if (limb.yaw > targetYaw) {
			limb.yaw -= moveInfo.yawD;
		} else if (limb.yaw < targetYaw) {
			limb.yaw += moveInfo.yawD;
		}
	}

	if (limb.yaw > moveInfo.maxYaw || limb.yaw < moveInfo.minYaw) {
		limb.yaw = Clamp(limb.yaw, moveInfo.minYaw, moveInfo.maxYaw);
		result = ANGLES_SET_TO_MAX;
	}

	// pitch

	if (Abs(limb.pitch - targetPitch) < moveInfo.pitchD){
		limb.pitch = targetPitch;
	}else{
		if (limb.pitch > targetPitch) {
			limb.pitch -= moveInfo.pitchD;
		} else if (limb.pitch < targetPitch) {
			limb.pitch += moveInfo.pitchD;
		}
		result = ONE_ANGLE_COULDNT_BE_SET_EXACTLY;
	}

	if (limb.pitch > moveInfo.maxPitch || limb.pitch < moveInfo.minPitch) {
		limb.pitch = Clamp(limb.pitch, moveInfo.minPitch, moveInfo.maxPitch);
		result = ANGLES_SET_TO_MAX;
	}
	return result;
}

bool
CPedIK::RestoreGunPosn(void)
{
	LimbMoveStatus limbStatus = MoveLimb(m_torsoOrient, 0.0f, 0.0f, ms_torsoInfo);
	RotateTorso(m_ped->m_pFrames[PED_MID], &m_torsoOrient, false);
	return limbStatus == ANGLES_SET_EXACTLY;
}

bool
CPedIK::LookInDirection(float targetYaw, float targetPitch)
{
	bool success = true;
	float yaw, pitch;
	if (m_ped->m_pFrames[PED_HEAD] == nil)
		return false;
	if (!(m_ped->m_pFrames[PED_HEAD]->flag & AnimBlendFrameData::IGNORE_ROTATION)) {
		m_ped->m_pFrames[PED_HEAD]->flag |= AnimBlendFrameData::IGNORE_ROTATION;
		RwMatrix *m = GetComponentMatrix(m_ped, PED_NECK);
		if (m == nil)
			return false;
		m_headOrient.yaw = Atan2(-m->at.y, -m->at.x);
		m_headOrient.yaw -= m_ped->m_fRotationCur;
		m_headOrient.yaw = CGeneral::LimitRadianAngle(m_headOrient.yaw);
		float up = Clamp(m->up.z, -1.0f, 1.0f);
		m_headOrient.pitch = Atan2(-up, Sqrt(1.0f - SQR(-up)));
	}

	// parent of head is neck
	RwMatrix *m = GetComponentMatrix(m_ped, PED_NECK);
	if (m == nil)
		return false;
	yaw = CGeneral::LimitRadianAngle(Atan2(-m->at.y, -m->at.x));
	float up = Clamp(m->up.z, -1.0f, 1.0f);
	pitch = Atan2(-up, Sqrt(1.0f - SQR(-up)));
	float headYaw = CGeneral::LimitRadianAngle(targetYaw - (yaw + m_torsoOrient.yaw));
	float headPitch = CGeneral::LimitRadianAngle(targetPitch - pitch) * Cos(Min(Abs(headYaw), HALFPI));

	LimbMoveStatus headStatus = MoveLimb(m_headOrient, headYaw, headPitch, ms_headInfo);
	if (headStatus == ANGLES_SET_TO_MAX)
		success = false;

	if (headStatus != ANGLES_SET_EXACTLY && !(m_flags & LOOKAROUND_HEAD_ONLY))
		if (MoveLimb(m_torsoOrient, CGeneral::LimitRadianAngle(targetYaw-m_ped->m_fRotationCur), targetPitch, ms_torsoInfo))
			success = true;

	// This was RotateHead
	RtQuat *q = GetFrameQuat(m_ped, PED_HEAD);
	RotateQuatIfPresent(q, &ZaxisIK, RADTODEG(m_headOrient.pitch), rwCOMBINEREPLACE);
	RotateQuatIfPresent(q, &XaxisIK, RADTODEG(m_headOrient.yaw), rwCOMBINEPRECONCAT);
	m_ped->bDontAcceptIKLookAts = true;

	if (!(m_flags & LOOKAROUND_HEAD_ONLY))
		RotateTorso(m_ped->m_pFrames[PED_MID], &m_torsoOrient, false);
	return success;
}

bool
CPedIK::LookAtPosition(CVector const &pos)
{
	RwMatrix *midMatrix = GetComponentMatrix(m_ped, PED_MID);
	if (midMatrix == nil)
		return false;
	RwV3d *pedpos = &midMatrix->pos;
	float yawToFace = CGeneral::GetRadianAngleBetweenPoints(
		pos.x, pos.y,
		pedpos->x, pedpos->y);

	float pitchToFace = CGeneral::GetRadianAngleBetweenPoints(
		// BUG? not using pedpos here
		pos.z, (m_ped->GetPosition() - pos).Magnitude2D(),
		pedpos->z, 0.0f);

	return LookInDirection(yawToFace, pitchToFace);
}

bool
CPedIK::PointGunInDirection(float targetYaw, float targetPitch)
{
	bool result = true;
	bool armPointedToGun = false;
	targetYaw = CGeneral::LimitRadianAngle(targetYaw - m_ped->GetForward().Heading());
	m_flags &= ~GUN_POINTED_SUCCESSFULLY;
	m_flags |= LOOKAROUND_HEAD_ONLY;
	if (m_flags & AIMS_WITH_ARM) {
		armPointedToGun = PointGunInDirectionUsingArm(targetYaw, targetPitch);
		targetYaw = CGeneral::LimitRadianAngle(targetYaw - (m_upperArmOrient.yaw + m_lowerArmOrient.yaw));
	}
	if (armPointedToGun) {
		if (m_flags & AIMS_WITH_ARM && m_torsoOrient.yaw * m_upperArmOrient.yaw < 0.0f)
			MoveLimb(m_torsoOrient, 0.0f, m_torsoOrient.pitch, ms_torsoInfo);
	} else {
		// Unused code
		RwMatrix *matrix;
		float yaw, pitch;
		matrix = RwMatrixCreate();
		RwMatrix *clavicle = GetComponentMatrix(m_ped, PED_CLAVICLER);
		if (clavicle == nil){
			RwMatrixDestroy(matrix);
			return false;
		}
		*matrix = *clavicle;
		ExtractYawAndPitchWorld(matrix, &yaw, &pitch);
		RwMatrixDestroy(matrix);

		if(m_flags & AIMS_WITH_ARM){
			if(targetPitch > 0.0f)
				targetPitch = Max(targetPitch - Abs(targetYaw), 0.0f);
			else
				targetPitch = Min(targetPitch + Abs(targetYaw), 0.0f);
		}
		LimbMoveStatus status = MoveLimb(m_torsoOrient, targetYaw, targetPitch, ms_torsoInfo);
		if (status == ANGLES_SET_TO_MAX)
			result = false;
		else if (status == ANGLES_SET_EXACTLY)
			m_flags |= GUN_POINTED_SUCCESSFULLY;
	}

	// Game uses index 2 directly, which happens to be identical to BONE_spine
#ifdef FIX_BUGS
	RwMatrix *m = GetBoneMatrix(m_ped, BONE_spine);
	if (m == nil)
		return false;
#else
	RpHAnimHierarchy* hier = GetAnimHierarchyFromSkinClump(m_ped->GetClump());
	RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
	RwMatrix *m = &mats[2];
#endif
	RwV3d axis = { 0.0f, 0.0f, 0.0f };
	float axisangle = -CGeneral::LimitRadianAngle(Atan2(-m->at.y, -m->at.x) - m_ped->m_fRotationCur);
	axis.y = -Sin(axisangle);
	axis.z = Cos(axisangle);

	// this was RotateTorso
	RtQuat *q = GetFrameQuat(m_ped, PED_MID);
	RotateQuatIfPresent(q, &axis, RADTODEG(m_torsoOrient.pitch), rwCOMBINEPOSTCONCAT);
	RotateQuatIfPresent(q, &XaxisIK, RADTODEG(m_torsoOrient.yaw), rwCOMBINEPOSTCONCAT);
	m_ped->bDontAcceptIKLookAts = true;

	return result;
}

bool
CPedIK::PointGunInDirectionUsingArm(float targetYaw, float targetPitch)
{
	bool result = false;
	RwMatrix *matrix;
	float yaw, pitch;

	float uaRoll = 45.0f;
	float handRoll = 30.0f;

	matrix = GetComponentMatrix(m_ped, PED_CLAVICLER);
	if (matrix == nil)
		return false;
	yaw = CGeneral::LimitRadianAngle(Atan2(matrix->right.y, matrix->right.x) - m_ped->m_fRotationCur);
	pitch = Atan2(matrix->up.z, Sqrt(1.0f - SQR(matrix->up.z)));

	float uaYaw, uaPitch;
	uaYaw = CGeneral::LimitRadianAngle(targetYaw - yaw - DEGTORAD(15.0f));
	uaPitch = CGeneral::LimitRadianAngle(targetPitch - pitch + DEGTORAD(10.0f));
	LimbMoveStatus uaStatus = MoveLimb(m_upperArmOrient, uaYaw, uaPitch, ms_upperArmInfo);
	if (uaStatus == ANGLES_SET_EXACTLY) {
		m_flags |= GUN_POINTED_SUCCESSFULLY;
		result = true;
	}

	if (uaStatus == ANGLES_SET_TO_MAX) {
		float laYaw = uaYaw - m_upperArmOrient.yaw;

		LimbMoveStatus laStatus;
		if (laYaw > 0.0f){
			float rollReduce = laYaw/DEGTORAD(30.0f);
			uaRoll *= 1.0f - Min(rollReduce, 1.0f);
			handRoll *= 1.0f - Min(rollReduce, 1.0f);

			laYaw *= 1.9f;
			laStatus = MoveLimb(m_lowerArmOrient, laYaw, 0.0f, ms_lowerArmInfo);

			// some unused statics here
			float uaPitchAmount = 1.0f - (m_lowerArmOrient.yaw + m_upperArmOrient.yaw) * 0.34f;
			float f1 = ms_upperArmInfo.maxPitch * Max(uaPitchAmount, 0.0f);
			float f2 = 0.2f*m_lowerArmOrient.yaw + m_upperArmOrient.pitch;
			m_upperArmOrient.pitch = Min(f1, f2);
		}else
			laStatus = MoveLimb(m_lowerArmOrient, laYaw, 0.0f, ms_lowerArmInfo);

		if (laStatus == ANGLES_SET_EXACTLY) {
			m_flags |= GUN_POINTED_SUCCESSFULLY;
			result = true;
		}

		// game does this stupidly by going through the clump extension...
		RtQuat *q = GetFrameQuat(m_ped, PED_FOREARMR);
		RotateQuatIfPresent(q, &ZaxisIK, -RADTODEG(m_lowerArmOrient.yaw), rwCOMBINEREPLACE);
		RotateQuatIfPresent(q, &XaxisIK, -RADTODEG(m_lowerArmOrient.pitch), rwCOMBINEPOSTCONCAT);
		m_ped->bDontAcceptIKLookAts = true;
	}

	RtQuat *q = GetFrameQuat(m_ped, PED_UPPERARMR);
	RotateQuatIfPresent(q, &XaxisIK, uaRoll, rwCOMBINEREPLACE);
	RotateQuatIfPresent(q, &YaxisIK, -RADTODEG(m_upperArmOrient.pitch), rwCOMBINEPOSTCONCAT);
	RotateQuatIfPresent(q, &ZaxisIK, -RADTODEG(m_upperArmOrient.yaw+HALFPI), rwCOMBINEPOSTCONCAT);
	m_ped->bDontAcceptIKLookAts = true;

	q = GetFrameQuat(m_ped, PED_HANDR);
	RotateQuatIfPresent(q, &XaxisIK, handRoll, rwCOMBINEPRECONCAT);

	return result;
}

bool
CPedIK::PointGunAtPosition(CVector const& position)
{
	CVector startPoint;
	if (m_ped->GetWeapon()->m_eWeaponType == WEAPONTYPE_SPAS12_SHOTGUN || m_ped->GetWeapon()->m_eWeaponType == WEAPONTYPE_STUBBY_SHOTGUN)
		startPoint = m_ped->GetPosition();
	else {
		RwV3d armPos;
		GetComponentPosition(armPos, PED_UPPERARMR);
		startPoint.x = m_ped->GetPosition().x;
		startPoint.y = m_ped->GetPosition().y;
		startPoint.z = armPos.z;
	}

	return PointGunInDirection(
		CGeneral::GetRadianAngleBetweenPoints(position.x, position.y, startPoint.x, startPoint.y),
		CGeneral::GetRadianAngleBetweenPoints(position.z, Distance2D(m_ped->GetPosition(), position.x, position.y), startPoint.z, 0.0f));
}

bool
CPedIK::RestoreLookAt(void)
{
	bool result = false;
	float yaw, pitch;

	if (m_ped->m_pFrames[PED_HEAD]->flag & AnimBlendFrameData::IGNORE_ROTATION) {
		m_ped->m_pFrames[PED_HEAD]->flag &= (~AnimBlendFrameData::IGNORE_ROTATION);
	} else {
		ExtractYawAndPitchLocalSkinned(m_ped->m_pFrames[PED_HEAD], &yaw, &pitch);
		if (MoveLimb(m_headOrient, yaw, pitch, ms_headRestoreInfo) == ANGLES_SET_EXACTLY)
			result = true;
	}

	// This was RotateHead
	RtQuat *q = GetFrameQuat(m_ped, PED_HEAD);
	RotateQuatIfPresent(q, &XaxisIK, RADTODEG(m_headOrient.yaw), rwCOMBINEREPLACE);
	RotateQuatIfPresent(q, &ZaxisIK, RADTODEG(m_headOrient.pitch), rwCOMBINEPRECONCAT);
	m_ped->bDontAcceptIKLookAts = true;

	if (!(m_flags & LOOKAROUND_HEAD_ONLY))
		MoveLimb(m_torsoOrient, 0.0f, 0.0f, ms_torsoInfo);
	if (!(m_flags & LOOKAROUND_HEAD_ONLY))
		RotateTorso(m_ped->m_pFrames[PED_MID], &m_torsoOrient, false);
	return result;
}

void
CPedIK::ExtractYawAndPitchWorld(RwMatrix *mat, float *yaw, float *pitch)
{
	float f = Clamp(DotProduct(mat->up, CVector(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
	*yaw = Acos(f);
	if (mat->up.x > 0.0f) *yaw = -*yaw;

	f = Clamp(DotProduct(mat->right, CVector(0.0f, 0.0f, 1.0f)), -1.0f, 1.0f);
	*pitch = Acos(f);
	if (mat->up.z > 0.0f) *pitch = -*pitch;
}

void
CPedIK::ExtractYawAndPitchLocal(RwMatrix *mat, float *yaw, float *pitch)
{
	float f = Clamp(DotProduct(mat->at, CVector(0.0f, 0.0f, 1.0f)), -1.0f, 1.0f);
	*yaw = Acos(f);
	if (mat->at.y > 0.0f) *yaw = -*yaw;

	f = Clamp(DotProduct(mat->right, CVector(1.0f, 0.0f, 0.0f)), -1.0f, 1.0f);
	*pitch = Acos(f);
	if (mat->up.x > 0.0f) *pitch = -*pitch;
}

void
CPedIK::ExtractYawAndPitchLocalSkinned(AnimBlendFrameData *node, float *yaw, float *pitch)
{
	RwMatrix *mat = RwMatrixCreate();
	RtQuatConvertToMatrix(&node->hanimFrame->q, mat);
	ExtractYawAndPitchLocal(mat, yaw, pitch);
	RwMatrixDestroy(mat);
}
