#ifndef __REVC_VEHICLE_SPAWN_KEY_H__
#define __REVC_VEHICLE_SPAWN_KEY_H__

// Walk registered vehicle model IDs in either direction, the same way
// FindNextPlayerModel walks the ped skins. The caller checks the IDE type and
// whether the model can actually be streamed, then spawns the one it finds.
template<typename Eligible>
int FindNextVehicleModel(int current, int direction, int maxModels, Eligible eligible)
{
	if(current < 0 || current >= maxModels || (direction != 1 && direction != -1))
		return -1;
	int i = current;
	for(int checked = 1; checked < maxModels; checked++) {
		i += direction;
		if(i < 0) i = maxModels - 1;
		else if(i >= maxModels) i = 0;
		if(eligible(i)) return i;
	}
	return -1;
}

#endif
