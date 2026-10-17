#ifndef __GTA_SA_STATIC_MODEL_H__
#define __GTA_SA_STATIC_MODEL_H__

#ifdef LIBRW
namespace rw { struct Atomic; struct Clump; }

namespace SAStaticModel {
// A VC simple/time model owns ONE atomic. Take the visible geometry of an SA
// static clump and make that atomic independent of the clump's frame tree.
// Caller destroys the clump in either case, and owns the returned atomic and
// its new frame on success. The TXD must be current while reading the DFF;
// RenderWare resolves material texture names at stream-read time.
rw::Atomic *TakeAtomic(rw::Clump *clump, const char **error);
}
#endif

#endif
