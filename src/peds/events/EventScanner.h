#pragma once

#include "common.h"
#include "TaskTimer.h"
#include "AttractorScanner.h"

class CPed;
class CEntity;

// SA CPedAcquaintanceScanner: decides which nearby peds this ped forms an
// opinion about (like/dislike/hate/respect events come from here).
class CPedAcquaintanceScanner
{
public:
	static float ms_fThresholdDotProduct;

	CTaskTimer m_timer;
	bool m_bScanAllowedScriptPed;
	bool m_bScanAllowedInVehicle;
	bool m_bScanAllowedScriptedTask;

	CPedAcquaintanceScanner() : m_bScanAllowedScriptPed(false), m_bScanAllowedInVehicle(false), m_bScanAllowedScriptedTask(false) {}

	void SetOnlyScriptPedAllowed()
	{
		m_bScanAllowedScriptPed = true;
		m_bScanAllowedInVehicle = false;
		m_bScanAllowedScriptedTask = false;
	}

	void TurnOffAllScanners()
	{
		m_bScanAllowedScriptPed = false;
		m_bScanAllowedInVehicle = false;
		m_bScanAllowedScriptedTask = false;
	}
};

// SA CVehiclePotentialCollisionScanner
class CVehiclePotentialCollisionScanner
{
public:
	CTaskTimer m_timer;
	void ScanForVehiclePotentialCollisionEvents(const CPed &ped, CEntity **entities, int32 count);
};

// SA CObjectPotentialCollisionScanner
class CObjectPotentialCollisionScanner
{
public:
	CTaskTimer m_timer;
};

// SA CSexyPedScanner
class CSexyPedScanner
{
public:
	CTaskTimer m_timer;
};

// SA CNearbyFireScanner
class CNearbyFireScanner
{
public:
	CTaskTimer m_timer;
};

// SA CEventScanner (0x600D60): runs the sub-scanners that turn the ped's
// surroundings (from the entity scanners) into events on its event group.
class CEventScanner
{
public:
	static uint32 m_sDeadPedWalkingTimer;

public:
	CEventScanner();
	~CEventScanner() {}

	void Clear();
	void ScanForEvents(CPed &ped);
	void ScanForEventsNow(const CPed &ped, bool bDontScan);

	CPedAcquaintanceScanner &GetAcquaintanceScanner() { return m_pedAcquaintanceScanner; }
	CVehiclePotentialCollisionScanner &GetVehiclePotentialCollisionScanner() { return m_vehiclePotentialCollisionScanner; }
	CObjectPotentialCollisionScanner &GetObjectPotentialCollisionScanner() { return m_objectPotentialCollisionScanner; }
	CAttractorScanner &GetAttractorScanner() { return m_attractorScanner; }
	CPedAcquaintanceScanner &GetPedAcquaintanceScanner() { return m_pedAcquaintanceScanner; }
	CSexyPedScanner &GetSexyPedScanner() { return m_sexyPedScanner; }
	CNearbyFireScanner &GetNearbyFireScanner() { return m_nearbyFireScanner; }

private:
	uint32 m_nNextScanTime;
	CVehiclePotentialCollisionScanner m_vehiclePotentialCollisionScanner;
	CObjectPotentialCollisionScanner m_objectPotentialCollisionScanner;
	CAttractorScanner m_attractorScanner;
	CPedAcquaintanceScanner m_pedAcquaintanceScanner;
	CSexyPedScanner m_sexyPedScanner;
	CNearbyFireScanner m_nearbyFireScanner;
};

VALIDATE_SIZE(CEventScanner, 0xD4);
