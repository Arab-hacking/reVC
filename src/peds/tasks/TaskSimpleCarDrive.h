#pragma once

// SA CTaskSimpleCarDrive: marks the ped as "driving this vehicle". The
// actual steering in SA shares code with the vehicle AI; here the task
// holds the vehicle reference and stays alive while the ped is in the
// car (the vehicle's own autopilot does the driving, as in VC). The SA
// head/arm-bopping-while-driving machinery and TriggerIK ride on the
// unported ProcessPed body and the IK chain manager; their fields are
// kept so the layout matches SA.

#include "TaskSimple.h"
#include "TaskTimer.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

class CTaskSimpleCarDrive : public CTaskSimple
{
public:
	CVehicle *m_pVehicle;
	CAnimBlendAssociation *m_pAnimCloseDoorRolling;
	CTaskUtilityLineUpPedWithCar *m_pTaskUtilityLineUpPedWithCar;
	CTaskTimer *m_TaskTimer;
	int32 field_18;
	int8 field_1C;
	int8 field_1D;
	int32 m_nBoppingStartTime;
	int32 field_24;
	int32 m_nBoppingEndTime;
	float m_fBoppingProgress;
	int32 m_nBoppingCompletedTimes;
	int32 m_nHeadBoppingStartTime;
	int32 m_nHeadBoppingDirection;
	float m_fHeadBoppingOrientation;
	float m_fRandomHeadBoppingMultiplier;
	float m_fHeadBoppingFactor;
	int32 m_nArmBoppingStartTime;
	int32 m_nTimePassedSinceCarUpSideDown;
	CTaskTimer m_copCarStolenTimer;

	union {
		uint8 m_nCarDriveFlags;
		struct {
			uint8 m_b01 : 1;
			uint8 m_b02 : 1;
			uint8 m_bUpdateCurrentVehicle : 1; // keep m_pVehicle pointing at the occupied vehicle
			uint8 m_b08 : 1;
			uint8 m_b10 : 1;
			uint8 m_b20 : 1;
		};
	};

public:
	CTaskSimpleCarDrive(CVehicle *vehicle, CTaskUtilityLineUpPedWithCar *utilityTask = nil, bool updateCurrentVehicle = false);
	~CTaskSimpleCarDrive();

	CTask *Clone() const;
	eTaskType GetTaskType() const { return TASK_SIMPLE_CAR_DRIVE; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);

	void TriggerIK(CPed *ped) const;
	void UpdateBopping();
	void StartBopping(CPed *ped);
	void ProcessHeadBopping(CPed *ped, bool bBopping, float fDistSq);
	void ProcessArmBopping(CPed *ped, bool bBopping, float fDistSq);
	void ProcessBopping(CPed *ped, bool bBopping);

	CVehicle *GetVehicle() const { return m_pVehicle; }
};

VALIDATE_SIZE(CTaskSimpleCarDrive, 0x60);
