#pragma once
// Archive fixtures never create upgrade instances. Fail loudly if their empty
// shutdown path unexpectedly attempts to destroy a real RenderWare object.
#include <cstdlib>
RwUInt8 RwObjectGetType(const RwObject *) { std::abort(); }
RwFrame *RpClumpGetFrame(const RpClump *) { std::abort(); }
RwFrame *RwFrameGetParent(const RwFrame *) { std::abort(); }
RwFrame *RwFrameRemoveChild(RwFrame *) { std::abort(); }
RwBool RpClumpDestroy(RpClump *) { std::abort(); }
RpClump *RpAtomicGetClump(const RpAtomic *) { std::abort(); }
RpClump *RpClumpRemoveAtomic(RpClump *, RpAtomic *) { std::abort(); }
RwFrame *RpAtomicGetFrame(const RpAtomic *) { std::abort(); }
RpAtomic *RpAtomicSetFrame(RpAtomic *, RwFrame *) { std::abort(); }
RwBool RpAtomicDestroy(RpAtomic *) { std::abort(); }
RwBool RwFrameDestroy(RwFrame *) { std::abort(); }
void CBaseModelInfo::RemoveRef(void) { std::abort(); }
