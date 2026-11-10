#ifndef REVC_CUSTOM_WORLD_H
#define REVC_CUSTOM_WORLD_H

class CVector;

// Installs BR IDE definitions, IPL instances and .cls collision in the VC
// world. Only the text/binary parsers are format-specific; streaming of the
// actual .mod/.dff/.btx remains the ordinary CCustomModels path.
class CCustomWorld
{
public:
    static void LoadIde(void);         // after all VC IDEs, before Streaming::Init
    static void LoadCollisions(void);  // after native collision has been loaded
    static void LoadIpl(void);         // after native IPLs
    static bool OwnsModelSlot(int slot); // for normal RW eviction of BR LODs
    static bool ResolveSpawnPosition(CVector &pos, float *outHeading = 0);
    static bool ResolveRestartPosition(const CVector &refPos, CVector &inOutRestartPos, float *inOutHeading = 0);
    static void Shutdown(void);
};
#endif
