#pragma once

#include "common.h"
#include "TaskTimer.h"

// SA CMentalState: what a ped remembers about recent provocation. The
// anger fields drive CEventHighAngerAtPlayer; the health fields snapshot
// the ped/vehicle health so changes can be noticed frame to frame.
class CMentalState
{
public:
	uint8 m_AngerAtPlayer;
	uint8 m_LastAngerAtPlayer;

	CTaskTimer m_AngerTimer;

	uint8 m_pedHealth;
	uint8 m_oldPedHealth;

	uint8 m_vehicleHealth;
	uint8 m_oldVehicleHealth;

public:
	CMentalState() : m_AngerAtPlayer(0), m_LastAngerAtPlayer(0), m_pedHealth(0), m_oldPedHealth(0), m_vehicleHealth(0), m_oldVehicleHealth(0) {}

	void IncrementAnger(int32 anger);
	void Process();
};

VALIDATE_SIZE(CMentalState, 0x14);
