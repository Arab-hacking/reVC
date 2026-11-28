#include "common.h"

#include "MentalState.h"

// 0x421050: anger only registers while the previous anger is still being
// remembered (the timer gates how often new anger can be added).
void
CMentalState::IncrementAnger(int32 anger)
{
	if(!m_AngerTimer.IsOutOfTime())
		return;
	m_AngerTimer.Start(3000);
	m_AngerAtPlayer += (uint8)anger;
}

// Not yet reversed in gta-reversed (0x6008A0); reconstructed from the field
// layout: last-frame values are snapshotted so scanners can spot changes.
void
CMentalState::Process()
{
	m_LastAngerAtPlayer = m_AngerAtPlayer;
	m_oldPedHealth = m_pedHealth;
	m_oldVehicleHealth = m_vehicleHealth;
}
