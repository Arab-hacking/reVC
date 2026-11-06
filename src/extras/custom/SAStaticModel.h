#ifndef __GTA_SA_STATIC_MODEL_H__
#define __GTA_SA_STATIC_MODEL_H__

#ifdef LIBRW
#include <vector>

namespace rw { struct Atomic; struct Clump; }

namespace SAStaticModel {

struct LightEffect {
    float pos[3];
    unsigned char r, g, b, a;
    float coronaDist;
    float pointlightRange;
    float coronaSize;
    float shadowSize;
    unsigned char flashType;
    unsigned char reflection;
    unsigned char flareType;
    unsigned char shadowIntensity;
    unsigned char flags1;
    unsigned char flags2;
    char coronaName[24];
    char shadowName[24];
};

// Register the RenderWare RpGeometry 2dfx stream plugin (0x0253F2F8) before RwEngineOpen.
void RegisterPlugins(void);

// Extract all valid 2dfx EFFECT_LIGHT entries across a clump's atomics, transformed by each atomic's frame LTM.
void ExtractClumpLights(rw::Clump *clump, std::vector<LightEffect> &out);

// Heuristic fallback for custom outdoor streetlamp poles exported without a 2dfx chunk.
bool IsOutdoorStreetLampName(const char *modelName);
void EstimateStreetLampLights(rw::Atomic *bakedAtomic, std::vector<LightEffect> &out);

// A VC simple/time model owns ONE atomic. Take the visible geometry of an SA
// static clump and make that atomic independent of the clump's frame tree.
// Caller destroys the clump in either case, and owns the returned atomic and
// its new frame on success. The TXD must be current while reading the DFF;
// RenderWare resolves material texture names at stream-read time.
rw::Atomic *TakeAtomic(rw::Clump *clump, const char **error);
}
#endif

#endif
