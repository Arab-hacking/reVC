#include "TaskSimpleCarDrive.h"

#include "AnimBlendAssociation.h"
#include "Ped.h"
#include "Timer.h"
#include "Vehicle.h"

// SA CTaskSimpleCarDrive (0x63C340). ProcessPed/MakeAbortable are not
// reversed upstream (binary calls), and most of what they do in SA -
// telling the vehicle's autopilot what to do each tick - happens in this
// codebase inside the vehicle's own processing (the driver's vehicle AI
// runs off the vehicle's autopilot state, exactly as VC's
// CAutomobile::ProcessAI does). The reconstruction below therefore keeps
// the task's contract: stay alive while the ped is seated in the
// vehicle, end when he is not. The head/arm bopping functions and
// TriggerIK are likewise unreversed/IK-bound and remain empty shells so
// callers link; their state fields still tick in UpdateBopping.

// 0x63C340
CTaskSimpleCarDrive::CTaskSimpleCarDrive(CVehicle *vehicle, CTaskUtilityLineUpPedWithCar *utilityTask, bool updateCurrentVehicle)
{
	m_pVehicle = vehicle;
	m_pAnimCloseDoorRolling = nil;
	m_pTaskUtilityLineUpPedWithCar = nil;
	m_TaskTimer = nil;
	field_18 = 0;
	field_1C = 0;
	field_1D = 0;
	m_nTimePassedSinceCarUpSideDown = 0;
	m_nCarDriveFlags = 0;
	m_bUpdateCurrentVehicle = updateCurrentVehicle;
	m_b08 = true;

	if(m_pVehicle)
		m_pVehicle->RegisterReference((CEntity**)&m_pVehicle);

	// SA deep-copies the utility task; it is not ported yet and all
	// current callers pass nil.
	(void)utilityTask;

	m_fHeadBoppingFactor = 0.0f;
	m_fHeadBoppingOrientation = 0.0f;
	m_fRandomHeadBoppingMultiplier = 0.0f;
	m_nBoppingStartTime = -1;
	m_nBoppingEndTime = 0;
	m_fBoppingProgress = 0.0f;
	m_nBoppingCompletedTimes = 0;
	m_nHeadBoppingStartTime = 0;
	m_nHeadBoppingDirection = 0;
	m_nArmBoppingStartTime = 0;
}

// 0x63C460
CTaskSimpleCarDrive::~CTaskSimpleCarDrive()
{
	// The vehicle reference is released with the task: RegisterReference
	// clears m_pVehicle itself if the vehicle dies first.
	if(m_pTaskUtilityLineUpPedWithCar){
		// delete m_pTaskUtilityLineUpPedWithCar; // not ported yet, never set
		m_pTaskUtilityLineUpPedWithCar = nil;
	}

	if(m_b20 && m_pAnimCloseDoorRolling){
		m_pAnimCloseDoorRolling->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
		if(m_pVehicle)
			m_pVehicle->ClearGettingOutFlags(1);
	}
}

// 0x63C500: needs the IK chain manager (not ported).
void
CTaskSimpleCarDrive::TriggerIK(CPed *ped) const
{
	(void)ped;
}

// 0x63C900
void
CTaskSimpleCarDrive::UpdateBopping()
{
	int32 timeDelta = (int32)CTimer::GetTimeInMilliseconds() - m_nBoppingStartTime;
	if(m_nBoppingEndTime > 0){
		m_fBoppingProgress = (float)(timeDelta % m_nBoppingEndTime) / (float)m_nBoppingEndTime;
		m_nBoppingCompletedTimes = (timeDelta / m_nBoppingEndTime) % 2;
	}
}

// 0x642760: not reversed upstream (plugin call).
void
CTaskSimpleCarDrive::StartBopping(CPed *ped)
{
	(void)ped;
}

// 0x6428C0: not reversed upstream (plugin call).
void
CTaskSimpleCarDrive::ProcessHeadBopping(CPed *ped, bool bBopping, float fDistSq)
{
	(void)ped; (void)bBopping; (void)fDistSq;
}

// 0x642AE0: not reversed upstream (plugin call).
void
CTaskSimpleCarDrive::ProcessArmBopping(CPed *ped, bool bBopping, float fDistSq)
{
	(void)ped; (void)bBopping; (void)fDistSq;
}

// 0x642E70
void
CTaskSimpleCarDrive::ProcessBopping(CPed *ped, bool bBopping)
{
	(void)bBopping;

	if(ped->m_pMyVehicle == nil)
		return;

	// SA skips bopping for the player's car, cops and dragged-out peds;
	// with the bopping bodies unreversed only the state tick remains.
	if(m_nBoppingStartTime != -1)
		UpdateBopping();
}

// 0x63DC20
CTask*
CTaskSimpleCarDrive::Clone() const
{
	CTaskSimpleCarDrive *task = new CTaskSimpleCarDrive(m_pVehicle);
	task->m_bUpdateCurrentVehicle = m_bUpdateCurrentVehicle;
	return task;
}

// 0x63C670: not reversed upstream. A driving task can always be aborted;
// the ped-side state is owned by the vehicle code.
bool
CTaskSimpleCarDrive::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)ped; (void)priority; (void)event;
	return true;
}

// 0x644470: not reversed upstream; reconstructed (see file header).
bool
CTaskSimpleCarDrive::ProcessPed(CPed *ped)
{
	if(m_bUpdateCurrentVehicle)
		m_pVehicle = ped->m_pMyVehicle;

	if(ped->m_pMyVehicle == nil || ped->m_pMyVehicle != m_pVehicle)
		return true;

	ProcessBopping(ped, true);
	return false;
}

// SA keeps a driving ped on his seat from the task itself (0x63C770,
// unreversed, but every sibling task does exactly this); also inherited
// by CTaskSimpleCarDriveTimed, the first phase of leaving a car.
bool
CTaskSimpleCarDrive::SetPedPosition(CPed *ped)
{
	if (m_pVehicle == nil || ped->m_pMyVehicle != m_pVehicle)
		return false;
	ped->SetPedPositionInCar();
	return true;
}
