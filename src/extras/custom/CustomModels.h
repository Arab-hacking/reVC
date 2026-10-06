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

class CCustomModels
{
public:
	// scans the custom folder (also done automatically on the first request)
	static void Initialise(void);
	static void Shutdown(void);
	// true when the folder exists and there is at least one usable archive
	static bool IsActive(void);

	// Is this stream id served by the custom folder? The game asks this before
	// every read, so it has to stay cheap. Stream ids are models, texture
	// dictionaries and collision slots.
	static bool CanServe(int32 streamId);
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

	// diagnostics (written to custom_models.log when REVC_CUSTOM_LOG is set)
	static void PrintStats(void);
};

#endif // __GTA_CUSTOMMODELS_H__
