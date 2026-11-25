#ifndef __GTA_CARCTRL_H__
#define __GTA_CARCTRL_H__

class CVehicle;

class CCarCtrl
{
public:
	// Instantiates the class a model declares. Every spawn site goes through it,
	// so a model always becomes the class its type word names.
	static CVehicle *GetNewVehicleDependingOnCarModel(int32 modelId, uint8 createdBy);
};

#endif
