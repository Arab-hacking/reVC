#pragma once

#include "RwHelper.h"

// Vehicle dummies are attachment points, not a guarantee of an atomic object.
// In particular helicopters and custom cars may omit wheel geometry entirely.
inline bool HideOptionalVehicleNodeAtomic(RwFrame *frame)
{
    if(!frame) return false;
    RwObject *object = GetFirstObject(frame);
    if(!object || RwObjectGetType(object) != rpATOMIC) return false;
    RpAtomicSetFlags(reinterpret_cast<RpAtomic*>(object), 0);
    return true;
}
