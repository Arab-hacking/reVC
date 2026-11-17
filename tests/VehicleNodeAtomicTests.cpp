// Exercise the exact optional-node operation used by creation/save restoration.
#include "common.h"
#include "VehicleNodeAtomic.h"
#include <cassert>
#include <cstdio>
static RwObject *first;
static int queries, writes;
RwObject *GetFirstObject(RwFrame *frame) { assert(frame); ++queries; return first; }
RwUInt8 RwObjectGetType(const RwObject *object) { assert(object); return object->type; }
RpAtomic *RpAtomicSetFlags(RpAtomic *atomic, RwUInt32 flags)
{
    assert(atomic && flags == 0);
    ++writes;
    atomic->object.object.flags = flags;
    return atomic;
}
int main()
{
    RwFrame frame = {};
    RpAtomic atomic = {};
    RwObject nonAtomic = {};
    assert(!HideOptionalVehicleNodeAtomic(nullptr) && queries == 0 && writes == 0);
    first = nullptr;
    assert(!HideOptionalVehicleNodeAtomic(&frame) && writes == 0);
    nonAtomic.type = 3; // light/non-atomic object
    first = &nonAtomic;
    assert(!HideOptionalVehicleNodeAtomic(&frame) && writes == 0);
    atomic.object.object.type = rpATOMIC;
    atomic.object.object.flags = rpATOMICRENDER;
    first = reinterpret_cast<RwObject*>(&atomic);
    assert(HideOptionalVehicleNodeAtomic(&frame) && writes == 1);
    assert(atomic.object.object.flags == 0);
    // A second model/instance may have the same dummy but no geometry.
    first = nullptr;
    assert(!HideOptionalVehicleNodeAtomic(&frame) && writes == 1);
    std::puts("VehicleNodeAtomicTests: ALL PASSED");
}
