#include "common.h"

#include "Vehicle.h"
#include "Automobile.h"
#include "Camera.h"
#include "General.h"
#include "Door.h"

// SA CDoor globals (source/game_sa/Door.cpp, 0x872304..0x87233C), values
// verbatim. These are what turn the door into a damped spring instead of the
// four-line hand-waver VC shipped.
float DOOR_SPEED_MIN_LIMIT = 0.001f;
float DOOR_DAMP_RATE = 0.97f;
float DOOR_DAMP_RATE_BOAT = 0.98f;
float DOOR_DAMP_RATE_FIRETRUCK = 0.92f;
float DOOR_DAMP_RATE_BONNET = 0.97f;

float DOOR_DAMP_RATE_CHASSIS = 0.98f;
float DOOR_DAMP_RATE_CHASSIS_CUTSCENE = 0.95f;
float DOOR_SPRING_RATE_CHASSIS = 0.06f;
float DOOR_APPLY_RATE_CHASSIS = 0.025f;
float DOOR_APPLY_RATE_CHASSIS_CUTSCENE = 0.0f;
float DOOR_SPEED_MAX_CAPPED_CHASSIS = 0.02f;
float DOOR_SPEED_MAX_CAPPED_FIRETRUCK = 0.05f;
float DOOR_SPRING_RATE_FIRETRUCK = 0.01f;
float DOOR_SLAM_SHUT_SPEED = 0.1f;

// Swing speed limit. Never written after initialisation.
const float CDoor::DOOR_SPEED_MAX_CAPPED = 0.5f;

CDoor::CDoor(void)
{
	memset(this, 0, sizeof(*this));
}

void
CDoor::SetExtraWheelPositions(float wheel1, float wheel2, float wheel3, float wheel4)
{
	m_openAngle = wheel1;
	m_closedAngle = wheel2;
	m_fAngle = wheel3;
	m_fPrevAngle = wheel4;
}

void
CDoor::GetExtraWheelPositions(float &wheel1, float &wheel2, float &wheel3, float &wheel4)
{
	wheel1 = m_openAngle;
	wheel2 = m_closedAngle;
	wheel3 = m_fAngle;
	wheel4 = m_fPrevAngle;
}

// SA CDoor::ProcessImpact (0x6F4540), verbatim. Decides whether an impact is
// hard enough to pop the door out of its closed state; the VC code had no such
// test, it only ever opened doors from the get-in/out animation.
bool
CDoor::ProcessImpact(CVehicle *vehicle, const CVector &oldMoveSpeed, const CVector &oldTurnSpeed, const CVector &offset)
{
	static const float POP_OPEN_DOOR_ACCEL_LIMIT = 0.1f; // 0x8D3968

	CVector velocity = vehicle->GetSpeed(offset);
	CVector oldVelocity = CrossProduct(oldTurnSpeed, offset) + oldMoveSpeed;
	CVector acceleration = velocity - oldVelocity;

	// Direction along which the impact has to act, in world space.
	CVector direction;
	switch(m_dirn & DOOR_EXTRA_DIRN_MASK){
	case DOOR_AXIS_X:     direction = vehicle->GetRight(); break;
	case DOOR_AXIS_Y:     direction = vehicle->GetForward(); break;
	case DOOR_AXIS_Z:     direction = vehicle->GetUp(); break;
	case DOOR_AXIS_NEG_X: direction = -vehicle->GetRight(); break;
	case DOOR_AXIS_NEG_Y: direction = -vehicle->GetForward(); break;
	case DOOR_AXIS_NEG_Z: direction = -vehicle->GetUp(); break;
	default:              direction = CVector(0.0f, 0.0f, 0.0f); break;
	}

	// Angular velocity along the door's hinge axis.
	CVector cross = CrossProduct(acceleration, direction);
	float ang = m_axis == DOOR_AXIS_X ? cross.x :
	            m_axis == DOOR_AXIS_Y ? cross.y : cross.z;

	const float limit = CGeneral::GetRandomNumberInRange(0.75f, 1.5f) * POP_OPEN_DOOR_ACCEL_LIMIT;

	// Is there enough acceleration to open the door? Which sign counts as
	// "opening" depends on which way the door swings.
	const bool posOpenRotation = m_openAngle >= m_closedAngle;
	if(posOpenRotation)
		return ang > limit;
	else
		return ang < -limit;
}
void
CDoor::Open(float ratio)
{
	// SA CDoor::Open (0x6F4790), verbatim.
	m_fPrevAngle = m_fAngle;
	if(ratio >= 1.0f){
		m_fAngle = m_openAngle;
		if(!(m_dirn & DOOR_EXTRA_FIXEDSTATE))
			m_nDoorState = DOOR_HIT_MAX_END;
	}else{
		m_fAngle = ratio * m_openAngle;
		if(m_fAngle == 0.0f)
			m_fAngVel = 0.0f;
	}
}

// SA CDoor::Process (0x6F4040), ported line for line.
//
// The VC routine this replaces measured one scalar: it took the speed at the
// door hinge, took the y+z (or y+x) components of it, clamped that to +-0.2,
// added it to the angular velocity when it exceeded 0.002, damped by a flat
// 0.945 and clamped to +-0.3. It had no notion of the hinge axis, no gravity,
// no spring, and no distinction between a bonnet, a boat and a fire truck.
//
// SA builds the angular acceleration properly: it takes the hinge point's
// velocity, corrects by what the old chassis motion would have given it, adds
// gravity, transforms into the car's local frame, rotates the signed hinge
// axis by the current door angle, and takes the cross product with the
// remaining acceleration along the hinge. Then it damps per door type - a
// spring for the chassis and the fire truck, plain damping for a boat, a
// bonnet or a normal door - and finally clamps the swing and reports whether
// the door slammed.
bool
CDoor::Process(CVehicle *vehicle, const CVector &oldMoveSpeed, const CVector &oldTurnSpeed, const CVector &offset)
{
	CVector velocity = vehicle->GetSpeed(offset);
	CVector oldVelocity = CrossProduct(oldTurnSpeed, offset) + oldMoveSpeed;

	CVector acceleration = velocity - oldVelocity;

	// gravity pulls the door open (or shut) depending on which way it hangs
	acceleration.z += CTimer::GetTimeStep() * ((m_dirn & DOOR_EXTRA_LOW_GRAVITY) ? 0.0016f : 0.008f);

	// Into the vehicle's local frame. For a rotation-only matrix - which the
	// vehicle matrix is - the inverse is the transpose, i.e. the rows of the
	// matrix instead of its columns. reVC has no Multiply3x3Transposed, so the
	// transposed product is written out.
	const CMatrix &vehMat = vehicle->GetMatrix();
	acceleration = CVector(vehMat.rx * acceleration.x + vehMat.fx * acceleration.y + vehMat.ux * acceleration.z,
	                       vehMat.ry * acceleration.x + vehMat.fy * acceleration.y + vehMat.uy * acceleration.z,
	                       vehMat.rz * acceleration.x + vehMat.fz * acceleration.y + vehMat.uz * acceleration.z);

	CVector vecDirn(0.0f, 0.0f, 0.0f);
	switch(m_dirn & DOOR_EXTRA_DIRN_MASK){
	case DOOR_AXIS_X:     vecDirn.x =  1.0f; break;
	case DOOR_AXIS_Y:     vecDirn.y =  1.0f; break;
	case DOOR_AXIS_Z:     vecDirn.z =  1.0f; break;
	case DOOR_AXIS_NEG_X: vecDirn.x = -1.0f; break;
	case DOOR_AXIS_NEG_Y: vecDirn.y = -1.0f; break;
	case DOOR_AXIS_NEG_Z: vecDirn.z = -1.0f; break;
	default: break;
	}

	// Rotate the hinge axis by the door's own angle so the cross product below
	// is taken against the axis the door actually hangs on right now.
	float sinA = Sin(m_fAngle);
	float cosA = Cos(m_fAngle);

	int32 axis = (int32)m_axis;
	int32 a = (axis + 1) % 3;
	int32 b = (axis + 2) % 3;

	float vec[3] = { vecDirn.x, vecDirn.y, vecDirn.z };
	float newA = sinA * vec[b] - cosA * vec[a];
	float newB = sinA * vec[a] + cosA * vec[b];
	vec[a] = newA;
	vec[b] = newB;
	vecDirn.x = vec[0];
	vecDirn.y = vec[1];
	vecDirn.z = vec[2];

	CVector cross = CrossProduct(acceleration, vecDirn);
	float ang = axis == 0 ? cross.x : (axis == 1 ? cross.y : cross.z);

	// Angular velocity update
	if(m_dirn & DOOR_EXTRA_CHASSIS){
		// A cutscene or an airborne car applies the torque far more gently.
		float rate = (TheCamera.m_WideScreenOn || (vehicle->IsAutomobile() && !((CAutomobile*)vehicle)->m_nWheelsOnGround))
			? DOOR_APPLY_RATE_CHASSIS_CUTSCENE
			: DOOR_APPLY_RATE_CHASSIS;
		m_fAngVel += rate * ang;
	}else if(Abs(ang) > DOOR_SPEED_MIN_LIMIT || Abs(m_fAngVel) > DOOR_SPEED_MIN_LIMIT){
		m_fAngVel += ang;
	}

	// Attenuation (damping + spring)
	if(m_dirn & DOOR_EXTRA_CHASSIS){
		float rate = (TheCamera.m_WideScreenOn || (vehicle->IsAutomobile() && !((CAutomobile*)vehicle)->m_nWheelsOnGround))
			? DOOR_DAMP_RATE_CHASSIS_CUTSCENE
			: DOOR_DAMP_RATE_CHASSIS;
		m_fAngVel = powf(rate, CTimer::GetTimeStep()) * m_fAngVel
			- CTimer::GetTimeStep() * m_fAngle * DOOR_SPRING_RATE_CHASSIS;
	}else if(vehicle->IsBoat()){
		m_fAngVel *= DOOR_DAMP_RATE_BOAT;
	}else if(vehicle->GetModelIndex() == MI_FIRETRUCK){
		m_fAngVel = DOOR_DAMP_RATE_FIRETRUCK * m_fAngVel
			- CTimer::GetTimeStep() * m_fAngle * DOOR_SPRING_RATE_FIRETRUCK;
	}else if(m_dirn & DOOR_EXTRA_LOW_GRAVITY){
		m_fAngVel *= DOOR_DAMP_RATE_BONNET;
	}else{
		m_fAngVel *= DOOR_DAMP_RATE;
	}

	// Speed limit
	float capSpeed = (m_dirn & DOOR_EXTRA_CHASSIS) ? DOOR_SPEED_MAX_CAPPED_CHASSIS
		: (m_dirn & DOOR_EXTRA_FIRETRUCK) ? DOOR_SPEED_MAX_CAPPED_FIRETRUCK
		: DOOR_SPEED_MAX_CAPPED;

	m_fAngVel = Clamp(m_fAngVel, -capSpeed, capSpeed);

	// Integrate the angle
	m_fAngle += m_fAngVel;

	if(!(m_dirn & DOOR_EXTRA_FIXEDSTATE))
		m_nDoorState = DOOR_NOTHING;

	// Check the stops. A door whose open angle is more positive than its closed
	// one swings the other way round, so the comparisons flip with it.
	bool posOpenRotation = m_openAngle >= m_closedAngle;
	bool hitOpen = posOpenRotation ? (m_fAngle > m_openAngle) : (m_fAngle < m_openAngle);
	bool hitClosed = posOpenRotation ? (m_fAngle < m_closedAngle) : (m_fAngle > m_closedAngle);

	if(hitOpen){
		m_fAngVel *= -0.8f;
		m_fAngle = m_openAngle;
		if(!(m_dirn & DOOR_EXTRA_FIXEDSTATE))
			m_nDoorState = DOOR_HIT_MAX_END;
	}else if(hitClosed){
		m_fAngle = m_closedAngle;

		// A door with a lock that is slammed hard enough latches shut instead of
		// bouncing back.
		if((m_dirn & DOOR_EXTRA_BASED) && !(m_dirn & DOOR_EXTRA_FIXEDSTATE) && Abs(m_fAngVel) > DOOR_SLAM_SHUT_SPEED){
			m_fAngVel = 0.0f;
			m_nDoorState = DOOR_SLAM_SHUT;
			return true;
		}

		m_fAngVel *= (m_dirn & DOOR_EXTRA_LOW_GRAVITY) ? -0.9f : -0.4f;
		if(!(m_dirn & DOOR_EXTRA_FIXEDSTATE))
			m_nDoorState = DOOR_HIT_MIN_END;
	}

	return false;
}

// SA CDoor::GetRotation / UpdateFrameMatrix (NOTSA in the reference, but the
// same three lines SA has inline in its door update).
CVector
CDoor::GetRotation(void) const
{
	CVector rotation(0.0f, 0.0f, 0.0f);
	switch(m_axis){
	case DOOR_AXIS_X: case DOOR_AXIS_NEG_X: rotation.x = m_fAngle; break;
	case DOOR_AXIS_Y: case DOOR_AXIS_NEG_Y: rotation.y = m_fAngle; break;
	case DOOR_AXIS_Z: case DOOR_AXIS_NEG_Z: rotation.z = m_fAngle; break;
	}
	return rotation;
}

void
CDoor::UpdateFrameMatrix(CMatrix &mat)
{
	CVector rot = GetRotation();
	mat.SetRotateKeepPos(rot.x, rot.y, rot.z);
	mat.UpdateRW();
}

// SA keeps the two limits as named fields, so these are one-liners. The VC
// versions guessed which of the pair was "open" by comparing magnitudes, which
// gets the answer wrong for a symmetric door (-0.4*PI and 0.4*PI have equal
// magnitude) and for a door whose closed angle is non-zero.
float
CDoor::RetAngleWhenClosed(void)
{
	return m_closedAngle;
}

float
CDoor::RetAngleWhenOpen(void)
{
	return m_openAngle;
}

// SA CDoor::GetAngleOpenRatio (0x6F47E0), verbatim.
float
CDoor::GetAngleOpenRatio(void)
{
	if(m_openAngle != 0.0f)
		return m_fAngle / m_openAngle;
	return 0.0f;
}

// SA CDoor::IsFullyOpen (0x6F4820), verbatim.
bool
CDoor::IsFullyOpen(void)
{
	return Abs(RetAngleWhenOpen()) - 0.5f <= Abs(m_fAngle);
}

// SA CDoor::IsClosed (0x6F4800), verbatim.
bool
CDoor::IsClosed(void)
{
	return m_closedAngle == m_fAngle;
}


CTrainDoor::CTrainDoor(void)
{
	memset(this, 0, sizeof(*this));
}

void
CTrainDoor::Open(float ratio)
{
	float open;

	m_fPrevPosn = m_fPosn;
	open = RetTranslationWhenOpen();
	if(ratio < 1.0f){
		m_fPosn = open*ratio;
	}else{
		m_nDoorState = DOOR_HIT_MAX_END;
		m_fPosn = open;
	}
}

float
CTrainDoor::RetTranslationWhenClosed(void)
{
	if(Abs(m_fClosedPosn) < Abs(m_fOpenPosn))
		return m_fClosedPosn;
	else
		return m_fOpenPosn;
}

float
CTrainDoor::RetTranslationWhenOpen(void)
{
	if(Abs(m_fClosedPosn) < Abs(m_fOpenPosn))
		return m_fOpenPosn;
	else
		return m_fClosedPosn;
}

bool
CTrainDoor::IsFullyOpen(void)
{
	// 0.5f again...
	if(Abs(m_fPosn) < Abs(RetTranslationWhenOpen()) - 0.5f)
		return false;
	return true;
}

bool
CTrainDoor::IsClosed(void)
{
	return m_fPosn == RetTranslationWhenClosed();
}
