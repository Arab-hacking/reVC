#ifndef __REVC_PLAYER_MODEL_CYCLE_H__
#define __REVC_PLAYER_MODEL_CYCLE_H__

// Walk registered model IDs, in either direction, without assuming ped models
// occupy a contiguous or vanilla-only range. The caller checks actual IDE
// type and streaming availability, then commits the switch after loading.
template<typename Eligible>
int FindNextPlayerModel(int current, int direction, int maxModels, Eligible eligible)
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
