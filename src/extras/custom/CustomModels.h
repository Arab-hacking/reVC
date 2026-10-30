#ifndef __GTA_CUSTOMMODELS_H__
#define __GTA_CUSTOMMODELS_H__

// Runtime loader for explicitly placed custom resources:
//   custom/anim/*.ifp                  animation banks
//   custom/img/*.{img,dir}             VC IMG v1 with DFF/TXD/COL/CLS entries
//   custom/data/maps/*.{ide,ipl}       loose world definitions/placements
//   custom/common.zip                  map IDE/IPL, config.dat, timecyc.json only
// Other ZIPs, game-root img/ and game-root common/ are not custom sources.
// The native VC game archives remain available to the normal streamer.

#include <vector>
#include <stddef.h>

class CCustomModels
{
public:
	// scans the custom folder (also done automatically on the first request)
	static void Initialise(void);
	static void Shutdown(void);
	// Mount every VC IMG v1 (.img + sibling .dir) under custom/img/
	// before Streaming::Init indexes filenames. DFF/TXD use the native
	// streamer; COL/CLS are validated and converted to VC COLL separately.
	// IFP entries inside IMG are ignored.
	static void ResetImageMounts(void);
	static void MountImages(void);
	static bool IsAutoMountedImage(int index);
	// Return the winning IMG index for model.txd when model.dff and its
	// same-named TXD both come from auto-mounted IMGs; they may be in different
	// archives. -1 means use the ordinary IDE TXD. Metadata only, no decoding.
	static int PairedImageTxd(const char *modelName);
	// Script special models may request DFFs without an IDE, or DFFs already
	// bound to another IDE slot. Locate the highest-priority mounted IMG record
	// by name without reading any model bytes; result is sector position/size.
	static bool FindModelDffInImage(const char *modelName, uint32 &position, uint32 &sectors);
	// Transfer a streamed custom TXD into a bounded GPU-resident LRU on
	// eviction. Returns false when Streaming must destroy the TXD normally.
	static bool RetainTxdOnUnload(int32 slot);
	// Called when a TXD is explicitly destroyed or its slot is recycled.
	static void ForgetTxdSlot(int32 slot);
	// Flush detached GPU objects BEFORE the RenderWare device shuts down.
	static void ClearGraphicsCache(void);
	// Temporarily force a streamed model to come from the game's own IMG. Used
	// to keep a native VC ped skeleton available when a custom SA ped replaces
	// every initially loaded reference model.
	static void SetNativeModelOverride(int32 modelId, bool enabled);
	// true when at least one of the explicit custom sources is active
	static bool IsActive(void);

	// Is this stream id served by the custom folder? The game asks this before
	// every read, so it has to stay cheap. Stream ids are models, texture
	// dictionaries and collision slots.
	static bool CanServe(int32 streamId);
	// Retained bounded worker for legacy converters. Runtime IMG contents
	// are served by the native streamer; these entry points never read ZIP.
	static void PrefetchStream(int32 streamId, bool priority = false);
	static bool IsStreamPrepared(int32 streamId, bool wait);
	static void CancelStreamPrefetch(int32 streamId);
	static void PumpBackgroundResults(void);
	// Reads, converts and installs the file of this stream id. Only call it
	// when CanServe said yes for the same id.
	static bool Load(int32 streamId);

	// Per-model collision block from user IMG COL/CLS (false when absent).
	static bool GetCollisionBlock(const char *modelname, std::vector<uint8> &out);

	// Serve an .ifp from custom/anim/; VC banks still use native assets.
	static bool LoadAnimFileFromCustom(const char *filename, std::vector<uint8> &out);

	// World files are only scanned under custom/data/maps/. The optional
	// custom/data/maps/active_ipl.txt selects IPLs; IDEs are always loaded.
	typedef void (*WorldFileCallback)(const char *path, const unsigned char *data,
	                                  size_t size, void *context);
	static bool HasWorldFiles(void);
	static bool HasModelFile(const char *name);
	static void ForEachWorldFile(bool ide, WorldFileCallback callback, void *context);

	// Per-model CED2 collision blocks from .cls entries in user IMGs.
	typedef void (*CollisionCallback)(const char *model, const unsigned char *block,
	                                  size_t size, void *context);
	static void ForEachBrCollision(CollisionCallback callback, void *context);
	// Per-model blocks from user IMG .col and legacy .cls entries.
	// Callback bytes are complete, validated VC COLL blocks.
	typedef bool (*CollisionFilter)(const char *model, void *context);
	static void ForEachGameCollision(CollisionCallback callback, void *context,
	                                 CollisionFilter filter = nullptr);

	// Optional custom/data/timecyc.json, loose first then custom/common.zip.
	// fromCommon means it came from that ZIP, never the game-root common/ tree.
	static bool ReadDataFile(const char *relpath, std::vector<uint8> &out,
	                         bool *fromCommon = nullptr);

	// Legacy frontend entry points; no custom skin images are indexed.
	static bool HasSkin(const char *skinname);
	// Legacy entry point (no .btx sources are indexed in this layout).
	static RwTexture *SkinTextureFromBtx(const char *skinname);
	static bool GetSkinImage(const char *skinname, std::vector<uint8> &out, char *ext, int extCap);
	static int GetNumSkins(void);
	// "name.bmp" style - the frontend skin list strips the extension itself
	static const char *GetSkinFileName(int i);

	// diagnostics (written to the always-on debug.log, see DebugLog.h)
	static void PrintStats(void);
};

#endif // __GTA_CUSTOMMODELS_H__
