#ifndef __GTA_CUSTOMMODELS_H__
#define __GTA_CUSTOMMODELS_H__

// Runtime loader for custom resources:
// - PC (#ifndef ANDROID):
//   custom/anim/*.ifp                  animation banks
//   custom/img/*.{img,dir}             VC IMG v1 with DFF/TXD/COL/CLS entries
//   custom/data/maps/*.{ide,ipl}       loose world definitions/placements
//   custom/common.zip                  map IDE/IPL; data/peds.ide,
//                                      data/vehicles.ide, data/weapons.ide;
//                                      config.dat, timecyc.json, water.dat,
//                                      carcols.dat, lods.info (all under data/)
// - Android and desktop CUSTOM_BPC (GL3 uses the shared TexDB path):
//   custom/zip/common.bpc              20-byte XOR stream archive with config.dat,
//                                      texdb.dat, lods.info, IDE/IPL, .ani
//   custom/zip/mesh/*.bpc              mounted strictly from CDIMAGE in config.dat
//                                      (.mod head-TEA models, .cls, .ani)
//   custom/zip/textures/*.astc.bpc     mounted strictly from TEXDB in texdb.dat
//                                      (+ .tmb companions); .btx KTX1/ASTC uploaded
//                                      directly to GPU without .txd conversion.

#include <vector>
#include <stddef.h>
#include "BRVehicleResources.h"

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
	static bool HasImageTxd(const char *txdName);
	static void EnsureVehicleTxdLoaded(void);
	// Independent GPU resources: caller owns one ref, release with RwTextureDestroy.
	// Palette is an explicit row-major 16x16 RGBA table, not a guessed four-colour layout.
	static RwTexture *CreateVehiclePaletteTexture(const RwRGBA *entries, size_t count);
	static RwTexture *CreateVehicleTonerTexture(const RwRGBA &colour);
	// BR carmods.dat metadata; call after IDE registration. No eager loading of all mods.
	static bool LoadVehicleUpgrades(void);
	static void GetVehicleUpgrades(int32 vehicle, std::vector<brvehicle::Upgrade> &out);
	static bool RequestVehicleUpgrade(int32 vehicle, int32 uniqueId);
	// Nonblocking. Returns a cloned RW object and pins its model until Release.
	// Caller handles attachment to the named dummy; no physics/collision changes.
	static RwObject *CreateVehicleUpgradeInstance(int32 vehicle, int32 uniqueId);
	static void ReleaseVehicleUpgradeInstance(RwObject *object);
	// Before destroying a parent clump, release its instances; global cleanup
	// runs before world/model shutdown, not after their frames have been freed.
	static void ClearVehicleUpgrades(void);
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
#ifdef CUSTOM_BPC_TEXDB
	struct GpuTerrainSplatInfo {
		RwTexture *layers[4];
		float scale[4];
	};
	static bool FindGpuTerrainSplat(const RwTexture *maskTex, GpuTerrainSplatInfo &out);
	static void BeginBlockingLoad(void);
	static void EndBlockingLoad(void);
	struct ScopedBlockingLoad {
		ScopedBlockingLoad(void) { BeginBlockingLoad(); }
		~ScopedBlockingLoad(void) { EndBlockingLoad(); }
	};
	static void BeginImmediateTextures(void);
	static void EndImmediateTextures(void);
	static void UpgradeModelTexturesImmediate(int32 modelId);
	struct ScopedImmediateTextures {
		ScopedImmediateTextures(void) { BeginImmediateTextures(); }
		~ScopedImmediateTextures(void) { EndImmediateTextures(); }
	};
#endif
	// Reads, converts and installs the file of this stream id. Only call it
	// when CanServe said yes for the same id.
	static bool Load(int32 streamId);

	// Per-model collision block from user IMG COL/CLS (false when absent).
	static bool GetCollisionBlock(const char *modelname, std::vector<uint8> &out);

	// Serve an .ifp from custom/anim/; VC banks still use native assets.
	static bool LoadAnimFileFromCustom(const char *filename, std::vector<uint8> &out);

	// Every animation bank the custom archives provide, in index order. An SA
	// installation keeps its whole movement set in custom/zip/mesh/*.bpc, so
	// the dictionary has to be built from what is actually there rather than
	// from a fixed list of file names.
	typedef void (*AnimBankCallback)(const char *bankName, void *context);
	static void ForEachAnimBank(AnimBankCallback callback, void *context);

	// Loose world files are only scanned under custom/data/maps/. The one
	// permitted ZIP also supplies data/peds.ide, data/vehicles.ide and
	// data/weapons.ide. The optional active_ipl.txt selects IPLs; IDEs are
	// always loaded.
	typedef void (*WorldFileCallback)(const char *path, const unsigned char *data,
	                                  size_t size, void *context);
	static bool HasWorldFiles(void);
	static bool ReplacesNativeWorldMap(void);
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

	// Named tables including carmods.dat, timecyc.json, water.dat and carcols.dat can be
	// served here. Timecyc uses loose custom/data/timecyc.json if the ZIP
	// entry is absent/unreadable; water and colours are ZIP-only and fall
	// back to the game's native data when absent/unusable.
	// fromCommon means it came from that ZIP, never game-root common/.
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
