#include "common.h"

#include "Camera.h"
#include "Game.h"
#include "Remote.h"
#include "Timer.h"
#include "World.h"
#include "PlayerInfo.h"
#include "Vehicle.h"

void
CRemote::GivePlayerRemoteControlledCar(float x, float y, float z, float rot, uint16 model)
{
	CVehicle *car = CGame::SpawnVehicle(model, CVector(x, y, z), rot);
	if(car == nil)
		return;

	bool found;
	z = car->GetDistanceFromCentreOfMassToBaseOfModel() + CWorld::FindGroundZFor3DCoord(x, y, z + 2.0f, &found);
	car->GetMatrix().GetPosition().z = z;
	car->SetStatus(STATUS_PLAYER_REMOTE);
	car->bIsLocked = true;

	car->bEngineOn = true;
	if (FindPlayerVehicle() != nil) 
		FindPlayerVehicle()->SetStatus(STATUS_PLAYER_DISABLED);

	CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle = car;
	CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle->RegisterReference((CEntity**)&CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle);
	if (car->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE || car->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI) {
		TheCamera.TakeControl(car, CCam::MODE_CAM_ON_A_STRING, INTERPOLATION, CAMCONTROL_SCRIPT);
		TheCamera.SetZoomValueCamStringScript(0);
	} else
		TheCamera.TakeControl(car, CCam::MODE_BEHINDCAR, INTERPOLATION, CAMCONTROL_SCRIPT);
}

void
CRemote::TakeRemoteControlledCarFromPlayer(bool blowUp)
{
	CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle->bIsLocked = false;
	CWorld::Players[CWorld::PlayerInFocus].m_nTimeLostRemoteCar = CTimer::GetTimeInMilliseconds();
	CWorld::Players[CWorld::PlayerInFocus].m_bInRemoteMode = true;
	CWorld::Players[CWorld::PlayerInFocus].field_D5 = blowUp;
	CWorld::Players[CWorld::PlayerInFocus].field_D6 = true;
}
