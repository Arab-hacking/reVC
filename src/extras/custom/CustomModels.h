#ifndef __GTA_CUSTOMMODELS_H__
#define __GTA_CUSTOMMODELS_H__

// Runtime loader for the community's custom model containers.
//
// The containers (what the offline brconv tool and the brmod plugin convert)
// are:
//     X.mod   a RenderWare model (the same layout as a .dff) behind a TEA
//             style cypher; the first bytes are a header, the payload has to be
//             decrypted before RenderWare can read it,
//     X.btx   one texture in a KTX11 container (ASTC compressed or plain
//             RGBA/RGB/4444/565/5551, occasionally a PNG or a whole TXD),
//     X.cls   collision volumes (CLST/CLSF/COL3 blocks, one per model).
//
// brconv builds .dff/.txd/.col files out of those offline. This module does
// exactly the same work inside the game and hands the result to reVC's own
// loaders (CFileLoader, CTxdStore, CColStore), so nothing is converted on disk
// and no temporary files are written: the archive entry is decompressed into
// memory, converted in place and given to the engine.
//
// The archives are zip files in the game's custom folder (CUSTOM_MODELS_FOLDER
// in config.h); every archive is indexed by name the first time the game asks
// for a file, and the folder wins over the game's own IMG archives. A custom
// file replaces a file the game already knows - the name of the file has to be
// the name the game asks for (the model/txd/collision name from the IDE), which
// is why the lookup is done by name and not by file path.

#include <vector>
#include <stddef.h>

class CCustomModels
{
public:
	// scans the custom folder (also done automatically on the first request)
	static void Initialise(void);
	static void Shutdown(void);
	// Mount every VC IMG v1 (.img + sibling .dir) under the game's img/
	// directory before Streaming::Init indexes filenames. Models and TXDs in
	// these images use the native streamer; .col/.ifp entries are ignored.
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
	// A loose/ZIP model can be streamed through the synthetic CD position when
	// no IMG record exists (the ordinary loader still validates the bytes).
	static bool HasArchiveModelFile(const char *modelName);
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
	// true when the folder exists and there is at least one usable archive
	static bool IsActive(void);

	// Is this stream id served by the custom folder? The game asks this before
	// every read, so it has to stay cheap. Stream ids are models, texture
	// dictionaries and collision slots.
	static bool CanServe(int32 streamId);
	// CPU-only archive reads and .mod/.btx decoding run ahead of ordinary
	// streaming on one bounded worker. These are game-thread entry points;
	// explicit blocking loads may wait, normal frames must never wait.
	static void PrefetchStream(int32 streamId, bool priority = false);
	static bool IsStreamPrepared(int32 streamId, bool wait);
	static void CancelStreamPrefetch(int32 streamId);
	static void PumpBackgroundResults(void);
	// Reads, converts and installs the file of this stream id. Only call it
	// when CanServe said yes for the same id.
	static bool Load(int32 streamId);

	// Files that the game reads from disk by name (the .dff/.txd/.col of its
	// data files) are looked up in the custom folder first as well.
	static bool LoadClumpFileFromCustom(const char *filename);
	static RwTexDictionary *LoadTexDictionaryFromCustom(const char *filename);
	// the collision of one model, in the .col file block layout, taken from the
	// model's .cls (false when the custom folder has no collision for it)
	static bool GetCollisionBlock(const char *modelname, std::vector<uint8> &out);

	// serves an animation dictionary (.ifp, or BR .ani) from the archives;
	// true when the custom folder has the file the game asked for
	static bool LoadAnimFileFromCustom(const char *filename, std::vector<uint8> &out);

	// IDE/IPL from custom/common.zip and loose custom/, plus the unpacked
	// game-root common/ tree. common/data/config.dat chooses active paths and
	// its same-named files take priority; custom/active_ipl.txt can override
	// the IPL selection. Bytes live only for the callback.
	static bool HasCommonWorld(void);
	typedef void (*WorldFileCallback)(const char *path, const unsigned char *data,
	                                  size_t size, void *context);
	static bool HasWorldFiles(void);
	// zip/*.zip is an isolated graphics-only index, not a generic source for
	// streaming TXDs or other game files. These are metadata queries (no decode).
	static bool HasGraphicsReflections(void);
	static bool HasGraphicsLightmaps(void);
	// Returns an owned GPU texture, or nil when zip/ has no full-sized car mask.
	// The caller must destroy the returned reference before RW shuts down.
	static RwTexture *CarReflectionMaskFromZip(void);
	// Override only explicitly named DUAL lightmaps on UV2 world atomics;
	// attach the selected texture to the model's TXD for normal eviction.
	static void ApplyGraphicsLightmaps(RpAtomic *atomic, int32 txdSlot);
	static bool HasModelFile(const char *name);
	static void ForEachWorldFile(bool ide, WorldFileCallback callback, void *context);

	// Per-model CED2 collision blocks inside the .cls containers of
	// br_collision.zip. No decompressed archive is retained between callbacks.
	typedef void (*CollisionCallback)(const char *model, const unsigned char *block,
	                                  size_t size, void *context);
	static void ForEachBrCollision(CollisionCallback callback, void *context);
	// Per-model blocks from every .col in custom/*.zip, plus legacy single
	// .cls files. Callback bytes are complete, validated VC COLL blocks;
	// no decompressed container is retained after the callback returns.
	typedef bool (*CollisionFilter)(const char *model, void *context);
	static void ForEachGameCollision(CollisionCallback callback, void *context,
	                                 CollisionFilter filter = nullptr);

	// Only allowlisted data files are served from unpacked common/ first;
	// legacy archive/custom sources keep their original fallback behavior.
	// fromCommon (optional) reports whether the winning source is common/.
	static bool ReadDataFile(const char *relpath, std::vector<uint8> &out,
	                         bool *fromCommon = nullptr);

	// player skins out of the custom folder: plain images (.bmp, or .png/.jpg
	// in a skins folder) named after the skin, in the archives or loose
	static bool HasSkin(const char *skinname);
	// builds the player skin texture straight out of a .btx in a BR skins
	// pack (nil when there is no .btx under this name)
	static RwTexture *SkinTextureFromBtx(const char *skinname);
	static bool GetSkinImage(const char *skinname, std::vector<uint8> &out, char *ext, int extCap);
	static int GetNumSkins(void);
	// "name.bmp" style - the frontend skin list strips the extension itself
	static const char *GetSkinFileName(int i);

	// diagnostics (written to the always-on debug.log, see DebugLog.h)
	static void PrintStats(void);
};

#endif // __GTA_CUSTOMMODELS_H__
