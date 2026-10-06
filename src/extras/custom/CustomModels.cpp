#include "common.h"

#include "CustomModels.h"

#ifdef CUSTOM_MODELS

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <stdio.h>		// snprintf
#include <stdlib.h>		// getenv
#include <ctype.h>		// tolower

#include "CustomZip.h"
#include "CustomCol.h"
#include "SavehDiag.h"

// the container conversion, shared with brconv/brmod, so what the game does
// here is byte for byte what the offline converter would have produced
#define STB_DXT_IMPLEMENTATION	// the encoder needs one translation unit
#include "brformats.h"
#include "brtex.h"

#include "FileLoader.h"
#include "FileMgr.h"
#include "ModelInfo.h"
#include "Streaming.h"
#include "TxdStore.h"
#include "ColStore.h"
#include "RwHelper.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// where the files live
// ---------------------------------------------------------------------------

// zip archives that are a bit silly (a 2 GB archive with one entry) are not
// worth the memory, so entries have to stay under this limit
#define CUSTOM_MAX_ENTRY (64*1024*1024)
#define CUSTOM_MAX_DEPTH 4			// folder nesting that is still scanned

struct CustomSource
{
	int archive;
	int entry;
	uint64 mtime;
};

static std::vector<CustomZip*> customArchives;
static std::map<std::string, CustomSource> customMods;
static std::map<std::string, CustomSource> customBtx;
static std::map<std::string, CustomSource> customCls;
static bool customInitialised = false;
static bool customActive = false;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

static std::string
ToLower(const std::string &s)
{
	std::string r = s;
	for(size_t i = 0; i < r.size(); i++)
		r[i] = (char)tolower((unsigned char)r[i]);
	return r;
}

// name without a directory part and without the extension
static std::string
Stem(const std::string &path)
{
	size_t slash = path.find_last_of("/\\");
	std::string base = slash == std::string::npos ? path : path.substr(slash+1);
	size_t dot = base.find_last_of('.');
	return dot == std::string::npos ? base : base.substr(0, dot);
}

static std::string
Extension(const std::string &path)
{
	size_t slash = path.find_last_of("/\\");
	std::string base = slash == std::string::npos ? path : path.substr(slash+1);
	size_t dot = base.find_last_of('.');
	return dot == std::string::npos ? std::string() : ToLower(base.substr(dot+1));
}

// mtime is only ever compared with another archive's, so any unit works as
// long as one platform stays consistent with itself
static uint64
FileTime(const char *path)
{
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA fa;
	if(!GetFileAttributesExA(path, GetFileExInfoStandard, &fa))
		return 0;
	ULARGE_INTEGER t;
	t.LowPart = fa.ftLastWriteTime.dwLowDateTime;
	t.HighPart = fa.ftLastWriteTime.dwHighDateTime;
	return (uint64)t.QuadPart;
#else
	struct stat st;
	if(stat(path, &st) != 0) return 0;
	return (uint64)st.st_mtime;
#endif
}

// Enumerates one directory: files and subdirectories (full paths).
static bool
ListDirectory(const char *dir, std::vector<std::string> &files, std::vector<std::string> &dirs)
{
#ifdef _WIN32
	char pattern[512];
	WIN32_FIND_DATAA data;
	HANDLE h;

	snprintf(pattern, sizeof(pattern), "%s\\*", dir);
	h = FindFirstFileA(pattern, &data);
	if(h == INVALID_HANDLE_VALUE)
		return false;
	do {
		if(strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
			continue;
		char path[512];
		snprintf(path, sizeof(path), "%s\\%s", dir, data.cFileName);
		if(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			dirs.push_back(path);
		else
			files.push_back(path);
	} while(FindNextFileA(h, &data));
	FindClose(h);
	return true;
#else
	DIR *d = opendir(dir);
	if(d == nil)
		return false;
	struct dirent *e;
	while((e = readdir(d)) != nil){
		if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		char path[512];
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if(e->d_type == DT_DIR)
			dirs.push_back(path);
		else
			files.push_back(path);
	}
	closedir(d);
	return true;
#endif
}

// The game folder is not necessarily spelled the way the config says (and on
// Linux the file system is case sensitive), so try a few spellings.
static bool
FolderExists(const char *folder)
{
	std::vector<std::string> files, dirs;
	if(ListDirectory(folder, files, dirs)) return true;
	std::string lower = ToLower(folder);
	if(lower != folder && ListDirectory(lower.c_str(), files, dirs)) return true;
	std::string upper = folder;
	for(size_t i = 0; i < upper.size(); i++) upper[i] = (char)toupper((unsigned char)upper[i]);
	if(upper != folder && ListDirectory(upper.c_str(), files, dirs)) return true;
	return false;
}

// ---------------------------------------------------------------------------
// indexing the archives
// ---------------------------------------------------------------------------

static void
AddIndex(std::map<std::string, CustomSource> &index, const std::string &key,
         int archive, int entry, uint64 mtime)
{
	std::map<std::string, CustomSource>::iterator it = index.find(key);
	// the same name in several archives: the newer archive wins, exactly like
	// the offline converter decides it
	if(it == index.end() || it->second.mtime < mtime){
		CustomSource src;
		src.archive = archive;
		src.entry = entry;
		src.mtime = mtime;
		index[key] = src;
	}
}

static void
IndexArchive(const std::string &path, uint64 mtime, int &nMod, int &nBtx, int &nCls)
{
	CustomZip *zip = new CustomZip;
	if(!zip->Open(path.c_str())){
		delete zip;
		return;
	}
	int archive = (int)customArchives.size();
	customArchives.push_back(zip);
	for(int i = 0; i < zip->GetNumEntries(); i++){
		const CustomZipEntry *ent = zip->GetEntry(i);
		std::string name = ent->name;
		std::string ext = Extension(name);
		// nothing else in an archive is of interest here
		if(ext != "mod" && ext != "btx" && ext != "cls")
			continue;
		std::string key = ToLower(Stem(name));
		if(key.empty() || key.size() > 63)
			continue;
		if(ext == "mod"){
			AddIndex(customMods, key, archive, i, mtime);
			nMod++;
		}else if(ext == "btx"){
			// the model can only reference 31 characters of a texture name, so
			// a longer file name has to be found under the short name as well
			AddIndex(customBtx, key, archive, i, mtime);
			if(key.size() > 31)
				AddIndex(customBtx, key.substr(0, 31), archive, i, mtime);
			nBtx++;
		}else{
			AddIndex(customCls, key, archive, i, mtime);
			nCls++;
		}
	}
}

static void
ScanFolder(const std::string &folder, int depth, int &nZip, int &nMod, int &nBtx, int &nCls)
{
	std::vector<std::string> files, dirs;
	if(!ListDirectory(folder.c_str(), files, dirs))
		return;

	std::sort(files.begin(), files.end());
	for(size_t i = 0; i < files.size(); i++){
		if(Extension(files[i]) != "zip")
			continue;
		uint64 mtime = FileTime(files[i].c_str());
		IndexArchive(files[i], mtime, nMod, nBtx, nCls);
		nZip++;
	}
	if(depth < CUSTOM_MAX_DEPTH)
		for(size_t i = 0; i < dirs.size(); i++)
			ScanFolder(dirs[i], depth+1, nZip, nMod, nBtx, nCls);
}

static void
EnsureInitialised(void)
{
	if(customInitialised)
		return;
	customInitialised = true;

	const char *folder = CUSTOM_MODELS_FOLDER;
	const char *env = getenv("REVC_CUSTOM_DIR");
	if(env && env[0] != '\0')
		folder = env;

	int nZip = 0, nMod = 0, nBtx = 0, nCls = 0;
	if(FolderExists(folder))
		ScanFolder(folder, 0, nZip, nMod, nBtx, nCls);

	customActive = !customMods.empty() || !customBtx.empty() || !customCls.empty();
	debug("custom: folder %s: %d archive(s), %d .mod (%d names), %d .btx (%d names), %d .cls\n",
		folder, nZip, nMod, (int)customMods.size(), nBtx, (int)customBtx.size(), nCls);
	CUSTOM_LOG("--- scan ---\n");
	CUSTOM_LOG("folder %s: %d archive(s), %d .mod (%d names), %d .btx (%d names), %d .cls (%d names)\n",
		folder, nZip, nMod, (int)customMods.size(), nBtx, (int)customBtx.size(), nCls, (int)customCls.size());
}

// ---------------------------------------------------------------------------
// reading entries
// ---------------------------------------------------------------------------

static bool
ReadEntry(const std::map<std::string, CustomSource> &index, const std::string &key, std::vector<uint8> &out)
{
	std::map<std::string, CustomSource>::const_iterator it = index.find(key);
	if(it == index.end())
		return false;
	const CustomSource &src = it->second;
	if(src.archive < 0 || src.archive >= (int)customArchives.size())
		return false;
	const CustomZipEntry *ent = customArchives[src.archive]->GetEntry(src.entry);
	if(ent == nil || ent->uncompressedSize == 0 || ent->uncompressedSize > CUSTOM_MAX_ENTRY)
		return false;
	out.resize(ent->uncompressedSize);
	if(!customArchives[src.archive]->Extract(src.entry, out.data(), (uint32)out.size())){
		out.clear();
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// model: .mod -> a RenderWare stream the engine can read
// ---------------------------------------------------------------------------

static bool
ConvertMod(const std::vector<uint8> &file, std::vector<uint8> &dff, br::RwFixStats &stats, std::string &warn)
{
	std::string err;
	dff = br::convertModToDff(file, stats, err);
	if(dff.empty()){
		CUSTOM_LOG("  conversion failed: %s\n", err.c_str());
		return false;
	}
	if(!err.empty()){
		warn = err;
		CUSTOM_LOG("  conversion note: %s\n", err.c_str());
	}
	return true;
}

// names of the textures the model references, in the order they appear, plus
// the terrain recipes (models whose texture has to be baked from layers)
static bool
ModelTextureNames(const std::string &key, std::vector<std::string> &names, std::vector<br::TerrainRecipe> &recipes)
{
	std::vector<uint8> file;
	if(!ReadEntry(customMods, key, file))
		return false;
	std::vector<uint8> dff;
	br::RwFixStats stats;
	std::string warn;
	if(!ConvertMod(file, dff, stats, warn))
		return false;
	names.clear();
	brtex::collectTextureNames(dff.data(), dff.size(), names);
	recipes = stats.recipes;
	return true;
}

// A .mod can hold anything that was a RenderWare file: the game only takes
// clumps here (an "atomic" object is a clump with a single atomic as well), so
// the payload is checked once and the answer is remembered.
struct ModPlan
{
	bool computed;
	bool usable;
};

static std::map<std::string, ModPlan> customModPlans;

// root chunk of the converted model has to be a clump
static bool
IsClumpStream(const std::vector<uint8> &dff)
{
	if(dff.size() < 12)
		return false;
	uint32 id = (uint32)dff[0] | ((uint32)dff[1] << 8) | ((uint32)dff[2] << 16) | ((uint32)dff[3] << 24);
	return id == 0x10;	// rwID_CLUMP
}

static bool
CanUseModel(const std::string &key)
{
	ModPlan &plan = customModPlans[key];
	if(plan.computed)
		return plan.usable;
	plan.computed = true;

	std::vector<uint8> file, dff;
	br::RwFixStats stats;
	std::string warn;
	plan.usable = ReadEntry(customMods, key, file) && ConvertMod(file, dff, stats, warn) && IsClumpStream(dff);
	if(!plan.usable)
		CUSTOM_LOG("model %s: not usable as a game model, the game's own file is used\n", key.c_str());
	return plan.usable;
}

// A model references its textures by name; whether the custom folder can serve
// the model's texture dictionary depends on those names, and finding them means
// converting the model. The answer is remembered, so the check that the game
// makes before every single read stays cheap.
struct TxdPlan
{
	bool computed;
	bool haveModel;
	bool haveBtx;
	bool serve;
	std::string modelKey;		// the model the textures come from
	std::vector<std::string> names;
	std::vector<br::TerrainRecipe> recipes;
};

static std::map<std::string, TxdPlan> customTxdPlans;

// The dictionary a model uses does not always carry the model's name (a car is
// called "hotdog" but its textures can live in "hotdog1"), so a name that is
// not in the archives is matched against the dictionaries of the custom models.
static bool
FindModelForTxd(const std::string &txdName, std::string &modelKey)
{
	std::map<std::string, CustomSource>::const_iterator it;
	std::map<std::string, ModPlan>::const_iterator plan;
	for(it = customMods.begin(); it != customMods.end(); ++it){
		// a model that is already known to be unusable is skipped; the others
		// are only looked at, not converted - converting a whole map pack here
		// would cost seconds
		plan = customModPlans.find(it->first);
		if(plan != customModPlans.end() && plan->second.computed && !plan->second.usable)
			continue;
		int32 id = -1;
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(it->first.c_str(), &id);
		if(mi == nil || id < 0)
			continue;
		int32 slot = mi->GetTxdSlot();
		if(slot < 0)
			continue;
		const char *name = CTxdStore::GetTxdName(slot);
		if(name && name[0] && ToLower(name) == txdName){
			modelKey = it->first;
			return true;
		}
	}
	return false;
}

static TxdPlan &
GetTxdPlan(const std::string &key)
{
	TxdPlan &plan = customTxdPlans[key];
	if(plan.computed)
		return plan;
	plan.computed = true;
	plan.modelKey = key;
	plan.haveModel = customMods.find(key) != customMods.end() && CanUseModel(key);
	if(!plan.haveModel && FindModelForTxd(key, plan.modelKey))
		plan.haveModel = true;
	plan.haveBtx = customBtx.find(key) != customBtx.end();
	if(plan.haveModel){
		// the name of a texture is the name of its .btx file; a model texture
		// that is a terrain composite is named after its mask
		ModelTextureNames(plan.modelKey, plan.names, plan.recipes);
		for(size_t i = 0; i < plan.names.size(); i++)
			if(customBtx.find(ToLower(plan.names[i])) != customBtx.end()){
				plan.serve = true;
				break;
			}
	}else if(plan.haveBtx)
		plan.serve = true;
	return plan;
}

static bool
LoadModelIntoGame(const std::string &key, int32 modelId)
{
	std::vector<uint8> file;
	if(!ReadEntry(customMods, key, file)){
		CUSTOM_LOG("model %s: cannot read the archive entry\n", key.c_str());
		return false;
	}
	std::vector<uint8> dff;
	br::RwFixStats stats;
	std::string warn;
	if(!ConvertMod(file, dff, stats, warn))
		return false;

	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelId);
	if(mi == nil) return false;

	RwMemory mem;
	mem.start = dff.data();
	mem.length = (uint32)dff.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream == nil){
		CUSTOM_LOG("model %s: cannot open a memory stream\n", key.c_str());
		return false;
	}

	// the same three loaders the game uses, chosen the same way
	bool ok;
	if(mi->IsSimple()){
		ok = CFileLoader::LoadAtomicFile(stream, modelId);
	}else if(mi->GetModelType() == MITYPE_VEHICLE){
		// vehicles are read in two parts everywhere in the game; both parts are
		// available here, so both run right away
		mi->AddRef();
		ok = CFileLoader::StartLoadClumpFile(stream, modelId) &&
		     CFileLoader::FinishLoadClumpFile(stream, modelId);
	}else{
		ok = CFileLoader::LoadClumpFile(stream, modelId);
	}
	RwStreamClose(stream, &mem);

	CUSTOM_LOG("model %s (id %d): %s, %u bytes -> %u bytes%s%s\n", key.c_str(), modelId,
		ok ? "loaded" : "FAILED", (unsigned)file.size(), (unsigned)dff.size(),
		stats.versionsChanged ? ", chunk versions fixed" : "",
		warn.empty() ? "" : (", " + warn).c_str());
	return ok;
}

// ---------------------------------------------------------------------------
// textures: .btx -> RwTexture
// ---------------------------------------------------------------------------

static bool
LoadBtxTexture(const std::string &name, brtex::Texture &tex)
{
	std::vector<uint8> file;
	if(!ReadEntry(customBtx, name, file))
		return false;
	std::string err;
	tex.name = name;
	if(brtex::parseBtx(file.data(), file.size(), tex, &err))
		return true;
	CUSTOM_LOG("texture %s: %s\n", name.c_str(), err.c_str());
	return false;
}

// Builds a game texture out of the decoded mip chain. The mip levels are
// uploaded one by one (the raster API takes care of the platform specific
// layout), which is why the pixels have to be written in the platform's own
// byte order: Direct3D rasters are BGRA, OpenGL ones are RGBA.
static RwTexture*
BuildTexture(brtex::Texture &tex, const char *name)
{
	// the game and its renderers are happiest with power of two textures, and
	// it is what the offline converter does as well
	if(!tex.mips.empty() && !(brtex::isPot(tex.mips[0].w) && brtex::isPot(tex.mips[0].h)))
		brtex::makePot(tex);
	if(tex.mips.empty())
		return nil;

	uint32 w = tex.mips[0].w, h = tex.mips[0].h;
	if(w == 0 || h == 0)
		return nil;
	int levels = (int)tex.mips.size();
	if(levels > 1 && (tex.mips[0].w < 4 || tex.mips[0].h < 4))
		levels = 1;

	RwRaster *ras = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 |
		(levels > 1 ? rwRASTERFORMATMIPMAP : 0));
	if(ras == nil)
		return nil;

	bool swapRB = rw::platform == rw::PLATFORM_D3D9 || rw::platform == rw::PLATFORM_D3D8 ||
	              rw::platform == rw::PLATFORM_XBOX;
	for(int l = 0; l < levels; l++){
		brtex::Mip &mip = tex.mips[l];
		uint8 *dst = RwRasterLock(ras, (uint8)l, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
		if(dst == nil){
			levels = l;
			break;
		}
		uint32 stride = ras->stride;
		if(stride < mip.w*4) stride = mip.w*4;
		for(uint32 y = 0; y < mip.h; y++){
			uint8 *d = dst + (size_t)y*stride;
			const uint8 *s = mip.rgba.data() + (size_t)y*mip.w*4;
			if(swapRB)
				for(uint32 x = 0; x < mip.w; x++){
					d[x*4+0] = s[x*4+2];
					d[x*4+1] = s[x*4+1];
					d[x*4+2] = s[x*4+0];
					d[x*4+3] = s[x*4+3];
				}
			else
				memcpy(d, s, (size_t)mip.w*4);
		}
		ras->unlock(l);
	}
	if(levels == 0){
		RwRasterDestroy(ras);
		return nil;
	}

	RwTexture *texture = RwTextureCreate(ras);
	if(texture == nil){
		RwRasterDestroy(ras);
		return nil;
	}
	strncpy(texture->name, name, 31);
	texture->name[31] = '\0';
	texture->mask[0] = '\0';
	RwTextureSetFilterMode(texture, levels > 1 ? rwFILTERLINEARMIPLINEAR : rwFILTERLINEAR);
	RwTextureSetAddressing(texture, rwTEXTUREADDRESSWRAP);
	return texture;
}

// builds the dictionary of one texture dictionary slot: every texture the
// model references gets an RwTexture built from its .btx
static bool
LoadTxdSlot(int32 slot, const std::string &key, TxdPlan &plan)
{
	RwTexDictionary *dict = CTxdStore::GetSlot(slot)->texDict;
	if(dict == nil){
		CTxdStore::Create(slot);
		dict = CTxdStore::GetSlot(slot)->texDict;
	}
	if(dict == nil)
		return false;
	if(((rw::TexDictionary*)dict)->count() != 0){
		CUSTOM_LOG("textures %s (slot %d): the dictionary is already filled\n", key.c_str(), slot);
		return true;			// nothing to do
	}
	int made = 0, missing = 0;

	if(plan.haveModel){
		for(size_t i = 0; i < plan.names.size(); i++){
			const std::string &name = plan.names[i];
			// a model texture can be a terrain composite: the mask names the
			// texture the model references, the layers come from their own .btx
			const br::TerrainRecipe *recipe = nil;
			for(size_t r = 0; r < plan.recipes.size(); r++)
				if(plan.recipes[r].tex == name)
					recipe = &plan.recipes[r];

			brtex::Texture tex;
			bool ok;
			if(recipe)
				ok = brtex::bakeTerrain(*recipe, LoadBtxTexture, tex, 512, 0.0f);
			else
				ok = LoadBtxTexture(name, tex);
			if(!ok){ missing++; continue; }

			RwTexture *texture = BuildTexture(tex, name.c_str());
			if(texture == nil){ missing++; continue; }
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}
	}else{
		brtex::Texture tex;
		RwTexture *texture = LoadBtxTexture(key, tex) ? BuildTexture(tex, key.c_str()) : nil;
		if(texture){
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}else missing++;
	}
	CUSTOM_LOG("textures %s (slot %d): %d built, %d not found in the archives\n",
		key.c_str(), slot, made, missing);
	// An empty dictionary is better than a failed load: the file would be
	// requested again and again, and the model would never appear at all.
	return true;
}

// ---------------------------------------------------------------------------
// collision: .cls -> the .col the game reads
// ---------------------------------------------------------------------------

static bool
LoadColSlot(int32 slot)
{
	const char *name = CColStore::GetColName(slot);
	if(name == nil || name[0] == '\0')
		return false;
	std::string key = ToLower(name);
	if(customCls.find(key) == customCls.end())
		return false;

	std::vector<uint8> file;
	if(!ReadEntry(customCls, key, file))
		return false;

	// the containers hold San Andreas collision, the game reads the old
	// version of the format: convert, then hand it to the collision store
	br::ClsStats stats;
	std::vector<uint8> col = br::convertClsToCol(file, stats);
	if(col.empty()){
		CUSTOM_LOG("collision %s: no usable block (%d blocks, %d bad)\n",
			key.c_str(), stats.blocks, stats.bad);
		return false;
	}
	customcol::Stats cstats;
	std::vector<uint8> game = customcol::ToGameFormat(col, cstats);
	if(game.empty()){
		CUSTOM_LOG("collision %s: nothing left after the format conversion (%d bad block(s))\n",
			key.c_str(), cstats.bad);
		return false;
	}

	bool ok = CColStore::LoadCol(slot, game.data(), (int32)game.size());
	CUSTOM_LOG("collision %s (slot %d): %s, %d block(s), %d spheres, %d boxes, %d vertices, %d faces, %d material(s) fixed%s\n",
		key.c_str(), slot, ok ? "loaded" : "FAILED", cstats.blocks, cstats.spheres, cstats.boxes,
		cstats.vertices, cstats.faces, cstats.materials,
		cstats.dropped ? ", some faces dropped" : "");
	return ok;
}

// ---------------------------------------------------------------------------
// public interface
// ---------------------------------------------------------------------------

void
CCustomModels::Initialise(void)
{
	EnsureInitialised();
}

void
CCustomModels::Shutdown(void)
{
	for(size_t i = 0; i < customArchives.size(); i++)
		delete customArchives[i];
	customArchives.clear();
	customMods.clear();
	customBtx.clear();
	customCls.clear();
	customModPlans.clear();
	customTxdPlans.clear();
	customInitialised = false;
	customActive = false;
}

bool
CCustomModels::IsActive(void)
{
	EnsureInitialised();
	return customActive;
}

// Is this stream id (a model, a texture dictionary or a collision slot) one of
// ours? A model is ours when the archives have a .mod with its name, a texture
// dictionary when it belongs to such a model or when there is a .btx with its
// name, a collision slot when there is a .cls with its name.
bool
CCustomModels::CanServe(int32 streamId)
{
	if(!IsActive())
		return false;

	if(streamId < 0 || streamId >= NUMSTREAMINFO)
		return false;

	if(streamId < STREAM_OFFSET_TXD){
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(streamId);
		if(mi == nil)
			return false;
		// only models the game reads from a clump can take one: simple
		// objects, time objects, weapons, clumps, vehicles and peds are all
		// stored as clumps (a simple object's file holds one atomic)
		if(!mi->IsSimple() && mi->GetModelType() != MITYPE_CLUMP &&
		   mi->GetModelType() != MITYPE_VEHICLE && mi->GetModelType() != MITYPE_PED)
			return false;
		std::string key = ToLower(mi->GetModelName());
		return customMods.find(key) != customMods.end() && CanUseModel(key);
	}

	if(streamId >= STREAM_OFFSET_TXD && streamId < STREAM_OFFSET_COL){
		const char *name = CTxdStore::GetTxdName(streamId - STREAM_OFFSET_TXD);
		if(name == nil || name[0] == '\0')
			return false;
		// the dictionary is only ours when at least one of the textures it
		// has to contain is in the archives; otherwise the game's own
		// dictionary is the right one
		return GetTxdPlan(ToLower(name)).serve;
	}

	if(streamId >= STREAM_OFFSET_COL && streamId < STREAM_OFFSET_ANIM){
		const char *name = CColStore::GetColName(streamId - STREAM_OFFSET_COL);
		if(name == nil || name[0] == '\0')
			return false;
		return customCls.find(ToLower(name)) != customCls.end();
	}

	return false;	// animations are never taken from the custom folder
}

bool
CCustomModels::Load(int32 streamId)
{
	if(!CanServe(streamId))
		return false;

	if(streamId < STREAM_OFFSET_TXD){
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(streamId);
		return LoadModelIntoGame(ToLower(mi->GetModelName()), streamId);
	}

	if(streamId >= STREAM_OFFSET_TXD && streamId < STREAM_OFFSET_COL){
		int32 slot = streamId - STREAM_OFFSET_TXD;
		std::string key = ToLower(CTxdStore::GetTxdName(slot));
		return LoadTxdSlot(slot, key, GetTxdPlan(key));
	}

	if(streamId >= STREAM_OFFSET_COL && streamId < STREAM_OFFSET_ANIM)
		return LoadColSlot(streamId - STREAM_OFFSET_COL);

	return false;
}

// ---------------------------------------------------------------------------
// files the game reads from disk by name (the ones its data files list)
// ---------------------------------------------------------------------------

// Builds a texture dictionary holding everything the archives have for this
// name. Used when the game loads a .txd by name instead of streaming it.
static RwTexDictionary *
BuildDictionaryFor(const std::string &key)
{
	TxdPlan &plan = GetTxdPlan(key);
	RwTexDictionary *dict = RwTexDictionaryCreate();
	if(dict == nil)
		return nil;
	int made = 0, missing = 0;

	if(plan.haveModel || plan.haveBtx){
		std::vector<std::string> names = plan.names;
		if(!plan.haveModel)
			names.push_back(key);
		for(size_t i = 0; i < names.size(); i++){
			const std::string &name = names[i];
			const br::TerrainRecipe *recipe = nil;
			for(size_t r = 0; r < plan.recipes.size(); r++)
				if(plan.recipes[r].tex == name)
					recipe = &plan.recipes[r];
			brtex::Texture tex;
			bool ok = recipe ? brtex::bakeTerrain(*recipe, LoadBtxTexture, tex, 512, 0.0f)
			                 : LoadBtxTexture(name, tex);
			if(!ok){ missing++; continue; }
			RwTexture *texture = BuildTexture(tex, name.c_str());
			if(texture == nil){ missing++; continue; }
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}
	}
	CUSTOM_LOG("textures %s: dictionary built from the custom folder, %d textures, %d missing\n",
		key.c_str(), made, missing);
	(void)missing;
	return dict;
}

bool
CCustomModels::LoadClumpFileFromCustom(const char *filename)
{
	EnsureInitialised();
	if(!customActive)
		return false;

	std::string key = ToLower(Stem(filename));
	if(customMods.find(key) == customMods.end())
		return false;

	std::vector<uint8> file, dff;
	br::RwFixStats stats;
	std::string warn;
	if(!ReadEntry(customMods, key, file) || !ConvertMod(file, dff, stats, warn))
		return false;

	// A hierarchical model file is matched to its model by the name of the
	// clump's frame; the file name is the name of the model here, because the
	// archives are indexed by it.
	int32 id = -1;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(key.c_str(), &id);
	if(mi == nil || id < 0 || !mi->IsClump())
		return false;

	RwMemory mem;
	mem.start = dff.data();
	mem.length = (uint32)dff.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream == nil)
		return false;

	// the model's own dictionary is the current one while it is read, so its
	// textures are found by name
	bool setTxd = false;
	int32 txdSlot = mi->GetTxdSlot();
	if(txdSlot >= 0 && GetTxdPlan(key).serve){
		CTxdStore::PushCurrentTxd();
		if(CTxdStore::GetSlot(txdSlot)->texDict == nil)
			LoadTxdSlot(txdSlot, key, GetTxdPlan(key));
		CTxdStore::SetCurrentTxd(txdSlot);
		setTxd = true;
	}

	bool ok = CFileLoader::LoadClumpFile(stream, id);

	if(setTxd)
		CTxdStore::PopCurrentTxd();
	RwStreamClose(stream, &mem);

	CUSTOM_LOG("model file %s (id %d): %s, %u bytes -> %u bytes%s\n",
		filename, id, ok ? "loaded" : "FAILED", (unsigned)file.size(), (unsigned)dff.size(),
		warn.empty() ? "" : (", " + warn).c_str());
	return ok;
}

RwTexDictionary *
CCustomModels::LoadTexDictionaryFromCustom(const char *filename)
{
	EnsureInitialised();
	if(!customActive)
		return nil;
	std::string key = ToLower(Stem(filename));
	if(!GetTxdPlan(key).serve)
		return nil;
	return BuildDictionaryFor(key);
}

// The collision of one model, in the layout a .col file has: the header the
// game expects, the model name and the body. The body of the .cls is a COL3
// model, which is what the game's collision reader wants.
bool
CCustomModels::GetCollisionBlock(const char *modelname, std::vector<uint8> &out)
{
	EnsureInitialised();
	if(!customActive || modelname == nil)
		return false;

	std::string key = ToLower(modelname);
	if(customCls.find(key) == customCls.end())
		return false;

	std::vector<uint8> file;
	if(!ReadEntry(customCls, key, file))
		return false;
	br::ClsStats stats;
	std::vector<uint8> col = br::convertClsToCol(file, stats);
	if(col.size() <= 32)
		return false;
	customcol::Stats cstats;
	std::vector<uint8> game = customcol::ToGameFormat(col, cstats);
	if(game.size() <= 8)
		return false;

	// what the game's own reader of such a file expects for one model is the
	// block without its header: the name and everything behind it
	out.assign(game.begin() + 8, game.end());
	CUSTOM_LOG("collision %s: block taken from the custom folder, %u bytes, %d sphere(s), %d box(es), %d face(s)\n",
		modelname, (unsigned)out.size(), cstats.spheres, cstats.boxes, cstats.faces);
	return true;
}

void
CCustomModels::PrintStats(void)
{
	EnsureInitialised();
	CUSTOM_LOG("--- stats ---\n");
	CUSTOM_LOG("%d archive(s) open, %d model name(s), %d texture name(s), %d collision name(s)\n",
		(int)customArchives.size(), (int)customMods.size(), (int)customBtx.size(), (int)customCls.size());
}

#endif // CUSTOM_MODELS
