#ifndef REVC_PICKUP_MODEL_VALIDATION_H
#define REVC_PICKUP_MODEL_VALIDATION_H

#include "ModelInfo.h"

// Script and MODELINDICES pickup IDs can be -1 when the stock DATA/MAPS IDEs
// are disabled. CModelInfo::GetModelInfo(int) does not check its array bounds.
inline CBaseModelInfo *
GetPickupModelInfo(int32 id)
{
    if(id < 0 || id >= MODELINFOSIZE)
        return nil;
    CBaseModelInfo *modelInfo = CModelInfo::GetModelInfo(id);
    // Only simple, time and weapon models can back pickup objects.
    return modelInfo != nil && modelInfo->IsSimple() ? modelInfo : nil;
}

#endif // REVC_PICKUP_MODEL_VALIDATION_H
