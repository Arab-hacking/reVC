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
#include "brreserved.h"

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
	char loose[256];		// archive < 0: a plain file in the custom folder
	char fname[128];		// the name the file has (with the extension)
};

static std::vector<CustomZip*> customArchives;
static std::map<std::string, CustomSource> customMods;
static std::map<std::string, CustomSource> customBtx;
static std::map<std::string, CustomSource> customCls;
static std::map<std::string, CustomSource> customAnims;
// standard-format custom files: the community packs keep ordinary .dff/.txd/
// .col files (and plain images for the player skin) in the same archives
static std::map<std::string, CustomSource> customDff;
static std::map<std::string, CustomSource> customTxdFiles;
static std::map<std::string, CustomSource> customColFiles;	// whole .col containers
static std::map<std::string, CustomSource> customImages;	// .bmp/.png/.jpg

// one collision model inside a .col container
struct ColBlockRef
{
	int archive;
	int entry;
	uint32 offset;			// of the name (behind the fourcc + size)
	uint32 size;			// of the name + body
	bool vcBody;			// 'COLL' float body the game reads as it is
	uint64 mtime;
};
static std::map<std::string, ColBlockRef> customColBlocks;
static std::vector<std::string> customSkinNames;	// "name.bmp" style, for the frontend

static std::string customFolderPath;	// remembered for data files and loose skins
static bool customInitialised = false;
static bool customActive = false;

// .col fourccs, in byte order as they sit in the file
#define COL_FCC_COLL	('C' | ('O'<<8) | ('L'<<16) | ((uint32)'L'<<24))
#define COL_FCC_COL2	('C' | ('O'<<8) | ('L'<<16) | ((uint32)'2'<<24))
#define COL_FCC_COL3	('C' | ('O'<<8) | ('L'<<16) | ((uint32)'3'<<24))

static bool
IsColFourCC(uint32 f)
{
	return f == COL_FCC_COLL || f == COL_FCC_COL2 || f == COL_FCC_COL3;
}

static bool ReadSource(const CustomSource &src, std::vector<uint8> &out);

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

static CustomSource
MakeSource(int archive, int entry, uint64 mtime, const char *path, const char *fname)
{
	CustomSource src;
	src.archive = archive;
	src.entry = entry;
	src.mtime = mtime;
	src.loose[0] = '\0';
	src.fname[0] = '\0';
	if(path){
		strncpy(src.loose, path, sizeof(src.loose)-1);
		src.loose[sizeof(src.loose)-1] = '\0';
	}
	if(fname){
		strncpy(src.fname, fname, sizeof(src.fname)-1);
		src.fname[sizeof(src.fname)-1] = '\0';
	}
	return src;
}

static void
AddIndex(std::map<std::string, CustomSource> &index, const std::string &key,
         const CustomSource &src)
{
	std::map<std::string, CustomSource>::iterator it = index.find(key);
	// the same name in several places: the newer file wins, exactly like the
	// offline converter decides it
	if(it == index.end() || it->second.mtime < src.mtime)
		index[key] = src;
}

// The names of the collision models inside one .col container are indexed at
// scan time, so a model can find its collision whatever file it sits in. The
// blocks keep their [fourcc][size][name][body] layout; whether the body is
// the Vice City layout the game reads natively ('COLL', floats) or a San
// Andreas one ('COL2'/'COL3', fixed point) is remembered for the read.
static void
IndexColContainer(const std::string &key, const std::vector<uint8> &file,
                  const CustomSource &src, int &nBlocks)
{
	uint32 pos = 0;
	while(pos + 8 <= file.size()){
		const uint8 *p = file.data() + pos;
		uint32 fourcc = (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
		uint32 bsize = (uint32)p[4] | ((uint32)p[5] << 8) | ((uint32)p[6] << 16) | ((uint32)p[7] << 24);
		if(!IsColFourCC(fourcc) || bsize > file.size() - pos - 8){
			pos++;		// gap: skip a byte and look again
			continue;
		}
		char name[25];
		memcpy(name, p + 8, 24);
		name[24] = '\0';
		std::string bkey = ToLower(Stem(name));
		if(!bkey.empty() && bkey.size() <= 63){
			std::map<std::string, ColBlockRef>::iterator it = customColBlocks.find(bkey);
			if(it == customColBlocks.end() || it->second.mtime < src.mtime){
				ColBlockRef ref;
				ref.archive = src.archive;
				ref.entry = src.entry;
				ref.offset = pos + 8;
				ref.size = bsize;
				ref.vcBody = fourcc == COL_FCC_COLL;
				ref.mtime = src.mtime;
				customColBlocks[bkey] = ref;
				nBlocks++;
			}
		}
		pos += 8 + bsize;
	}
}

struct ScanStats
{
	int zip, mod, dff, btx, txd, cls, colfile, colblock, anim, img;
};

static void
IndexArchive(const std::string &path, uint64 mtime, ScanStats &st)
{
	CustomZip *zip = new CustomZip;
	if(!zip->Open(path.c_str())){
		delete zip;
		CUSTOM_LOG("archive %s: not a readable zip, skipped\n", path.c_str());
		return;
	}
	int archive = (int)customArchives.size();
	customArchives.push_back(zip);
	int mods = 0, dffs = 0, btxs = 0, txds = 0, clss = 0, colfiles = 0, anims = 0, imgs = 0;
	for(int i = 0; i < zip->GetNumEntries(); i++){
		const CustomZipEntry *ent = zip->GetEntry(i);
		std::string name = ent->name;
		std::string ext = Extension(name);
		// everything else in an archive is not of interest here
		if(ext != "mod" && ext != "dff" && ext != "btx" && ext != "cls" && ext != "txd" &&
		   ext != "col" && ext != "ifp" && ext != "ani" &&
		   ext != "bmp" && ext != "png" && ext != "jpg" && ext != "jpeg")
			continue;
		std::string key = ToLower(Stem(name));
		if(key.empty() || key.size() > 63)
			continue;
		CustomSource src = MakeSource(archive, i, mtime, nil, name.c_str());
		if(ext == "mod"){
			// the same guard the offline converter has: player.mod is an 8 KB
			// stub that crashes the game the moment it is read
			if(brres::isReserved(key)){
				CUSTOM_LOG("  %s: reserved name, skipped\n", name.c_str());
				continue;
			}
			AddIndex(customMods, key, src);
			mods++;
		}else if(ext == "dff"){
			if(brres::isReserved(key)){
				CUSTOM_LOG("  %s: reserved name, skipped\n", name.c_str());
				continue;
			}
			AddIndex(customDff, key, src);
			dffs++;
		}else if(ext == "btx"){
			// the model can only reference 31 characters of a texture name, so
			// a longer file name has to be found under the short name as well
			AddIndex(customBtx, key, src);
			if(key.size() > 31)
				AddIndex(customBtx, key.substr(0, 31), src);
			btxs++;
			// a BR skins pack (its name says "skin") keeps the player skins
			// as .btx textures - offer them in the skin list as well
			if(ToLower(path).find("skin") != std::string::npos){
				AddIndex(customImages, key, src);
				imgs++;
			}
		}else if(ext == "txd"){
			AddIndex(customTxdFiles, key, src);
			txds++;
		}else if(ext == "cls"){
			AddIndex(customCls, key, src);
			clss++;
		}else if(ext == "col"){
			std::vector<uint8> file;
			if(ent->uncompressedSize > 0 && ent->uncompressedSize <= CUSTOM_MAX_ENTRY){
				file.resize(ent->uncompressedSize);
				if(zip->Extract(i, file.data(), (uint32)file.size())){
					IndexColContainer(key, file, src, st.colblock);
					AddIndex(customColFiles, key, src);
					colfiles++;
					continue;
				}
			}
			CUSTOM_LOG("  %s: the .col container could not be read\n", name.c_str());
		}else if(ext == "ifp" || ext == "ani"){
			// .ifp dictionaries and BR single-animation .ani files
			AddIndex(customAnims, key, src);
			anims++;
		}else{
			// plain images: skins for the player (a bmp at any path, a png or
			// jpg when it sits in a skin folder)
			if(ext == "bmp" || ToLower(name).find("skin") != std::string::npos){
				AddIndex(customImages, key, src);
				imgs++;
			}
		}
	}
	st.zip++;
	st.mod += mods; st.dff += dffs; st.btx += btxs; st.txd += txds;
	st.cls += clss; st.colfile += colfiles; st.anim += anims; st.img += imgs;
	CUSTOM_LOG("archive %s: %d entries, %d .mod, %d .dff, %d .btx, %d .txd, %d .cls, %d .col, %d .ifp/.ani, %d image(s)\n",
		path.c_str(), zip->GetNumEntries(), mods, dffs, btxs, txds, clss, colfiles, anims, imgs);
}

static void
ScanFolder(const std::string &folder, int depth, ScanStats &st)
{
	std::vector<std::string> files, dirs;
	if(!ListDirectory(folder.c_str(), files, dirs))
		return;

	std::sort(files.begin(), files.end());
	for(size_t i = 0; i < files.size(); i++){
		const std::string &path = files[i];
		std::string ext = Extension(path);
		uint64 mtime = FileTime(path.c_str());
		if(ext == "zip"){
			IndexArchive(path, mtime, st);
			continue;
		}
		// loose files work as well: the same names the archives hold
		if(ext != "mod" && ext != "dff" && ext != "btx" && ext != "cls" && ext != "txd" &&
		   ext != "col" && ext != "ifp" && ext != "ani" &&
		   ext != "bmp" && ext != "png" && ext != "jpg" && ext != "jpeg")
			continue;
		std::string key = ToLower(Stem(path));
		if(key.empty() || key.size() > 63)
			continue;
		CustomSource src = MakeSource(-1, -1, mtime, path.c_str(), path.c_str());
		if(ext == "mod"){
			if(brres::isReserved(key))
				continue;
			AddIndex(customMods, key, src);
			st.mod++;
		}else if(ext == "dff"){
			if(brres::isReserved(key))
				continue;
			AddIndex(customDff, key, src);
			st.dff++;
		}else if(ext == "btx"){
			AddIndex(customBtx, key, src);
			if(key.size() > 31)
				AddIndex(customBtx, key.substr(0, 31), src);
			st.btx++;
			if(ToLower(path).find("skin") != std::string::npos){
				AddIndex(customImages, key, src);
				st.img++;
			}
		}else if(ext == "txd"){
			AddIndex(customTxdFiles, key, src);
			st.txd++;
		}else if(ext == "cls"){
			AddIndex(customCls, key, src);
			st.cls++;
		}else if(ext == "col"){
			std::vector<uint8> file;
			if(ReadSource(src, file)){
				IndexColContainer(key, file, src, st.colblock);
				AddIndex(customColFiles, key, src);
				st.colfile++;
			}
		}else if(ext == "ifp" || ext == "ani"){
			AddIndex(customAnims, key, src);
			st.anim++;
		}else{
			// loose images only count as skins when they sit in a skins folder
			if(ToLower(path).find("skin") != std::string::npos){
				AddIndex(customImages, key, src);
				st.img++;
			}
		}
	}
	if(depth < CUSTOM_MAX_DEPTH)
		for(size_t i = 0; i < dirs.size(); i++)
			ScanFolder(dirs[i], depth+1, st);
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
	customFolderPath = folder;

	ScanStats st;
	memset(&st, 0, sizeof(st));
	if(FolderExists(customFolderPath.c_str()))
		ScanFolder(customFolderPath, 0, st);

	customActive = !customMods.empty() || !customBtx.empty() || !customCls.empty() ||
	               !customAnims.empty() || !customDff.empty() || !customTxdFiles.empty() ||
	               !customColFiles.empty() || !customColBlocks.empty() || !customImages.empty();

	CUSTOM_LOG("==== custom folder: %s\n", customFolderPath.c_str());
	CUSTOM_LOG("scan: %d archive(s), %d .mod, %d .dff, %d .btx (%d names), %d .txd, %d .cls, %d .col (%d model(s) inside), %d .ifp/.ani (%d names), %d image(s)\n",
		st.zip, st.mod, st.dff, st.btx, (int)customBtx.size(), st.txd, st.cls,
		st.colfile, st.colblock, st.anim, (int)customAnims.size(), st.img);
	if(!customActive)
		CUSTOM_LOG("scan: nothing usable found - the game runs on its own files\n");

	// the skins the frontend can offer: one entry per image name
	std::map<std::string, CustomSource>::iterator im;
	for(im = customImages.begin(); im != customImages.end(); ++im){
		std::string fn = Stem(im->second.fname);
		std::string full = fn + "." + Extension(im->second.fname);
		size_t k;
		for(k = 0; k < customSkinNames.size(); k++)
			if(Stem(customSkinNames[k]) == fn)
				break;
		if(k == customSkinNames.size())
			customSkinNames.push_back(full);
	}
	std::sort(customSkinNames.begin(), customSkinNames.end());
	CUSTOM_LOG("scan: %d player skin(s) available from the custom folder\n", (int)customSkinNames.size());
}

// ---------------------------------------------------------------------------
// reading entries
// ---------------------------------------------------------------------------

// Reads one source: an archive entry or a loose file.
static bool
ReadSource(const CustomSource &src, std::vector<uint8> &out)
{
	if(src.archive < 0 || src.archive >= (int)customArchives.size()){
		if(src.loose[0] == '\0')
			return false;
		FILE *f = fopen(src.loose, "rb");
		if(f == nil)
			return false;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if(size <= 0 || (uint64)size > CUSTOM_MAX_ENTRY){
			fclose(f);
			return false;
		}
		out.resize(size);
		bool ok = fread(out.data(), 1, size, f) == (size_t)size;
		fclose(f);
		if(!ok)
			out.clear();
		return ok;
	}
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

static const CustomSource *
FindSource(const std::map<std::string, CustomSource> &index, const std::string &key)
{
	std::map<std::string, CustomSource>::const_iterator it = index.find(key);
	return it == index.end() ? nil : &it->second;
}

static bool
ReadEntry(const std::map<std::string, CustomSource> &index, const std::string &key, std::vector<uint8> &out)
{
	const CustomSource *src = FindSource(index, key);
	if(src == nil)
		return false;
	return ReadSource(*src, out);
}

// a .col container from the archives or the folder: Vice City blocks ('COLL'
// floats) are what the game reads and go through as they are, San Andreas
// blocks ('COL2'/'COL3', fixed point) are rewritten by the converter
static std::vector<uint8>
ColContainerToGame(const std::vector<uint8> &file, const char *what, const std::string &key)
{
	bool allVc = true;
	uint32 pos = 0;
	while(pos + 8 <= file.size()){
		const uint8 *p = file.data() + pos;
		uint32 fourcc = (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
		uint32 bsize = (uint32)p[4] | ((uint32)p[5] << 8) | ((uint32)p[6] << 16) | ((uint32)p[7] << 24);
		if(!IsColFourCC(fourcc)){
			pos++;
			continue;
		}
		if(fourcc != COL_FCC_COLL)
			allVc = false;
		pos += 8 + bsize;
	}
	if(allVc)
		return file;
	customcol::Stats cstats;
	std::vector<uint8> game = customcol::ToGameFormat(file, cstats);
	if(game.empty())
		CUSTOM_LOG("%s %s: nothing usable after the collision format conversion (%d bad)\n",
			what, key.c_str(), cstats.bad);
	return game;
}

// ---------------------------------------------------------------------------
// model: .mod -> a RenderWare stream the engine can read
// ---------------------------------------------------------------------------

// The file itself is the stream. The archive entry is read once and then
// understood in place - decryption and the BR->RW structure repair happen
// inside that very buffer, no .dff is produced anywhere: what comes out of
// here is handed to the RenderWare loaders as is.
static bool
UnderstandMod(std::vector<uint8> &file, br::RwFixStats &stats, std::string &warn)
{
	std::string err;
	if(!br::understandModInplace(file, stats, err)){
		CUSTOM_LOG("  read failed: %s\n", err.c_str());
		return false;
	}
	if(!err.empty()){
		warn = err;
		CUSTOM_LOG("  read note: %s\n", err.c_str());
	}
	return true;
}

// names of the textures the model references, in the order they appear, plus
// the terrain recipes (models whose texture has to be baked from layers)
// reads the model with this name - out of a BR .mod (understood in place
// here) or as a plain .dff - and says which one it was
static bool
ReadModelEntry(const std::string &key, std::vector<uint8> &file, bool &isMod,
               br::RwFixStats &stats, std::string &warn)
{
	const CustomSource *src = FindSource(customMods, key);
	isMod = src != nil;
	if(src == nil)
		src = FindSource(customDff, key);
	if(src == nil)
		return false;
	if(!ReadSource(*src, file))
		return false;
	if(isMod && !UnderstandMod(file, stats, warn))
		return false;
	return true;
}

static bool
ModelTextureNames(const std::string &key, std::vector<std::string> &names, std::vector<br::TerrainRecipe> &recipes)
{
	std::vector<uint8> file;
	bool isMod;
	br::RwFixStats stats;
	std::string warn;
	if(!ReadModelEntry(key, file, isMod, stats, warn))
		return false;
	names.clear();
	brtex::collectTextureNames(file.data(), file.size(), names);
	recipes = stats.recipes;
	return true;
}

// A .mod/.dff can hold anything that was a RenderWare file: the game only
// takes clumps here (an "atomic" object is a clump with a single atomic as
// well), so the payload is checked once and the answer is remembered.
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

	std::vector<uint8> file;
	bool isMod;
	br::RwFixStats stats;
	std::string warn;
	plan.usable = ReadModelEntry(key, file, isMod, stats, warn) && IsClumpStream(file);
	if(plan.usable){
		std::string usableWhy;
		if(!br::validateClumpForGame(file.data(), (uint32)file.size(), usableWhy)){
			plan.usable = false;
			warn = usableWhy;
		}
	}
	if(!plan.usable)
		CUSTOM_LOG("model %s: not usable as a game model%s, the game's own file is used\n", key.c_str(),
			warn.empty() ? "" : (" (" + warn + ")").c_str());
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
	bool haveTxdFile;
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
	for(it = customDff.begin(); it != customDff.end(); ++it){
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
	plan.haveModel = (customMods.find(key) != customMods.end() ||
	                  customDff.find(key) != customDff.end()) && CanUseModel(key);
	if(!plan.haveModel && FindModelForTxd(key, plan.modelKey))
		plan.haveModel = true;
	plan.haveBtx = customBtx.find(key) != customBtx.end();
	plan.haveTxdFile = customTxdFiles.find(key) != customTxdFiles.end();
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
	// a whole .txd in the archives serves the dictionary as well, as the
	// fallback for the textures that are not single .btx files
	if(!plan.serve && plan.haveTxdFile)
		plan.serve = true;
	return plan;
}

// the game loaders run under a guard on Windows: a model that is broken in a
// way we did not anticipate must not take the game down - the exception is
// logged and the game's own file is used instead
struct GuardedLoadCtx { CBaseModelInfo *mi; RwStream *stream; int32 modelId; bool ok; };
static bool
RunModelLoaders(void *p)
{
	GuardedLoadCtx *c = (GuardedLoadCtx*)p;
	if(c->mi->IsSimple()){
		c->ok = CFileLoader::LoadAtomicFile(c->stream, c->modelId);
	}else if(c->mi->GetModelType() == MITYPE_VEHICLE){
		// vehicles are read in two parts everywhere in the game; both parts are
		// available here, so both run right away
		c->mi->AddRef();
		c->ok = CFileLoader::StartLoadClumpFile(c->stream, c->modelId) &&
		        CFileLoader::FinishLoadClumpFile(c->stream, c->modelId);
	}else{
		c->ok = CFileLoader::LoadClumpFile(c->stream, c->modelId);
	}
	return c->ok;
}

#if defined(_WIN32) && defined(_MSC_VER)	// SEH is MSVC syntax; MinGW builds run unguarded
static bool
CallGuarded(bool (*fn)(void*), void *arg, DWORD *code)
{
	__try{
		return fn(arg);
	}__except(*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER){
		return false;
	}
}
#define CUSTOM_SEH_GUARD 1
#endif

static bool
LoadModelIntoGame(const std::string &key, int32 modelId)
{
	std::vector<uint8> file;
	bool isMod;
	br::RwFixStats stats;
	std::string warn;
	if(!ReadModelEntry(key, file, isMod, stats, warn)){
		CUSTOM_LOG("model %s: cannot read the archive entry\n", key.c_str());
		return false;
	}
	unsigned rawSize = (unsigned)file.size();

	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelId);
	if(mi == nil) return false;

	// an empty client stub or a truncated model is rejected before the game
	// parses it - the game's own file is used instead
	std::string usableWhy;
	if(!br::validateClumpForGame(file.data(), (uint32)file.size(), usableWhy)){
		CUSTOM_LOG("model %s: not usable as a game model (%s), the game's own file is used\n",
			key.c_str(), usableWhy.c_str());
		return false;
	}

	RwMemory mem;
	mem.start = file.data();
	mem.length = (uint32)file.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream == nil){
		CUSTOM_LOG("model %s: cannot open a memory stream\n", key.c_str());
		return false;
	}

	// the same three loaders the game uses, chosen the same way - under a
	// guard on Windows, so a broken model cannot take the game down
	bool ok;
#ifdef CUSTOM_SEH_GUARD
	{
		GuardedLoadCtx ctx; ctx.mi = mi; ctx.stream = stream; ctx.modelId = modelId; ctx.ok = false;
		DWORD code = 0;
		if(!CallGuarded(RunModelLoaders, &ctx, &code)){
			RwStreamClose(stream, &mem);
			CUSTOM_LOG("model %s: the load crashed (exception 0x%08lX), the game's own file is used\n",
				key.c_str(), (unsigned long)code);
			return false;
		}
		ok = ctx.ok;
	}
#else
	{
		GuardedLoadCtx ctx; ctx.mi = mi; ctx.stream = stream; ctx.modelId = modelId; ctx.ok = false;
		ok = RunModelLoaders(&ctx);
	}
#endif
	RwStreamClose(stream, &mem);

	CUSTOM_LOG("model %s (id %d): %s, %u bytes of .%s -> %u bytes read in place%s%s\n", key.c_str(), modelId,
		ok ? "loaded" : "FAILED", rawSize, isMod ? "mod" : "dff", (unsigned)file.size(),
		stats.versionsChanged ? ", chunk versions fixed" : "",
		warn.empty() ? "" : (", " + warn).c_str());
	return ok;
}

// ---------------------------------------------------------------------------
// textures: .btx and plain .txd -> RwTexture
// ---------------------------------------------------------------------------

// adds the textures of a read dictionary, keeping whatever is already there
static RwTexture *
AddIfNewTextureCB(RwTexture *texture, void *pData)
{
	RwTexDictionary *dict = (RwTexDictionary*)pData;
	if(RwTexDictionaryFindNamedTexture(dict, texture->name) == nil)
		RwTexDictionaryAddTexture(dict, texture);
	return texture;
}

// merges a whole .txd out of the archives into an existing dictionary
static bool
MergeTxdFile(const std::string &key, RwTexDictionary *dict)
{
	std::vector<uint8> file;
	if(!ReadEntry(customTxdFiles, key, file)){
		CUSTOM_LOG("textures %s: the .txd entry could not be read\n", key.c_str());
		return false;
	}
	RwMemory mem;
	mem.start = file.data();
	mem.length = (uint32)file.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream == nil)
		return false;
	bool ok = false;
	if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nil, nil)){
		RwTexDictionary *read = RwTexDictionaryGtaStreamRead(stream);
		if(read){
			RwTexDictionaryForAllTextures(read, AddIfNewTextureCB, dict);
			RwTexDictionaryDestroy(read);
			ok = true;
		}
	}
	RwStreamClose(stream, &mem);
	return ok;
}

static bool
LoadTxdFileIntoSlot(int32 slot, const std::string &key)
{
	RwTexDictionary *dict = CTxdStore::GetSlot(slot)->texDict;
	if(dict == nil){
		CTxdStore::Create(slot);
		dict = CTxdStore::GetSlot(slot)->texDict;
	}
	if(dict == nil)
		return false;
	bool ok = MergeTxdFile(key, dict);
	CUSTOM_LOG("textures %s (slot %d): whole .txd from the archives, %s\n", key.c_str(), slot, ok ? "merged" : "FAILED");
	return ok;
}



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

// building a texture runs under the same guard as the models: a .btx with
// data we misread must not take the game down
struct GuardedTexCtx { brtex::Texture *tex; const char *name; RwTexture *out; };
static bool
RunBuildTexture(void *p)
{
	GuardedTexCtx *c = (GuardedTexCtx*)p;
	c->out = BuildTexture(*c->tex, c->name);
	return c->out != nil;
}

static RwTexture *
BuildTextureGuarded(brtex::Texture &tex, const char *name)
{
#ifdef CUSTOM_SEH_GUARD
	GuardedTexCtx c; c.tex = &tex; c.name = name; c.out = nil;
	DWORD code = 0;
	if(!CallGuarded(RunBuildTexture, &c, &code)){
		CUSTOM_LOG("  texture %s: the build crashed (exception 0x%08lX), skipped\n", name, (unsigned long)code);
		return nil;
	}
	return c.out;
#else
	GuardedTexCtx c; c.tex = &tex; c.name = name; c.out = nil;
	RunBuildTexture(&c);
	return c.out;
#endif
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

			RwTexture *texture = BuildTextureGuarded(tex, name.c_str());
			if(texture == nil){ missing++; continue; }
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}
	}else{
		brtex::Texture tex;
		RwTexture *texture = LoadBtxTexture(key, tex) ? BuildTextureGuarded(tex, key.c_str()) : nil;
		if(texture){
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}else missing++;
	}
	// textures the single .btx files do not have can sit in a whole .txd in
	// the archives (the community packs keep them that way)
	if(missing > 0 && customTxdFiles.find(key) != customTxdFiles.end()){
		int before = made;
		if(MergeTxdFile(key, dict))
			made = ((rw::TexDictionary*)dict)->count();
		CUSTOM_LOG("textures %s (slot %d): filled from the whole .txd, %d -> %d textures\n",
			key.c_str(), slot, before, made);
		missing = 0;
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
	bool haveCls = customCls.find(key) != customCls.end();
	bool haveFile = customColFiles.find(key) != customColFiles.end();
	if(!haveCls && !haveFile)
		return false;

	std::vector<uint8> game;
	if(haveCls){
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
		game = customcol::ToGameFormat(col, cstats);
		if(game.empty()){
			CUSTOM_LOG("collision %s: nothing left after the format conversion (%d bad block(s))\n",
				key.c_str(), cstats.bad);
			return false;
		}
	}else{
		std::vector<uint8> file;
		if(!ReadEntry(customColFiles, key, file))
			return false;
		game = ColContainerToGame(file, "collision file", key);
		if(game.empty())
			return false;
	}

	bool ok = CColStore::LoadCol(slot, game.data(), (int32)game.size());
	CUSTOM_LOG("collision %s (slot %d): %s, taken from the %s\n",
		key.c_str(), slot, ok ? "loaded" : "FAILED", haveCls ? ".cls" : ".col file");
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
	customAnims.clear();
	customDff.clear();
	customTxdFiles.clear();
	customColFiles.clear();
	customColBlocks.clear();
	customImages.clear();
	customSkinNames.clear();
	customFolderPath.clear();
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
		if(customMods.find(key) == customMods.end() && customDff.find(key) == customDff.end())
			return false;
		return CanUseModel(key);
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
		std::string key = ToLower(name);
		return customCls.find(key) != customCls.end() ||
		       customColFiles.find(key) != customColFiles.end();
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
			RwTexture *texture = BuildTextureGuarded(tex, name.c_str());
			if(texture == nil){ missing++; continue; }
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}
	}
	if(customTxdFiles.find(key) != customTxdFiles.end() && MergeTxdFile(key, dict))
		made = ((rw::TexDictionary*)dict)->count();
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
	if(customMods.find(key) == customMods.end() && customDff.find(key) == customDff.end())
		return false;

	std::vector<uint8> file;
	bool isMod;
	br::RwFixStats stats;
	std::string warn;
	if(!ReadModelEntry(key, file, isMod, stats, warn))
		return false;
	unsigned rawSize = (unsigned)file.size();

	// the same structural check: stubs and truncated models are not served
	std::string usableWhy;
	if(!br::validateClumpForGame(file.data(), (uint32)file.size(), usableWhy)){
		CUSTOM_LOG("model file %s: not usable as a game model (%s), the game's own file is used\n",
			filename, usableWhy.c_str());
		return false;
	}

	// A hierarchical model file is matched to its model by the name of the
	// clump's frame; the file name is the name of the model here, because the
	// archives are indexed by it.
	int32 id = -1;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(key.c_str(), &id);
	if(mi == nil || id < 0 || !mi->IsClump())
		return false;

	RwMemory mem;
	mem.start = file.data();
	mem.length = (uint32)file.size();
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

	bool ok;
#ifdef CUSTOM_SEH_GUARD
	{
		GuardedLoadCtx ctx; ctx.mi = mi; ctx.stream = stream; ctx.modelId = id; ctx.ok = false;
		DWORD code = 0;
		if(!CallGuarded(RunModelLoaders, &ctx, &code)){
			if(setTxd)
				CTxdStore::PopCurrentTxd();
			RwStreamClose(stream, &mem);
			CUSTOM_LOG("model file %s: the load crashed (exception 0x%08lX), the game's own file is used\n",
				filename, (unsigned long)code);
			return false;
		}
		ok = ctx.ok;
	}
#else
	{
		GuardedLoadCtx ctx; ctx.mi = mi; ctx.stream = stream; ctx.modelId = id; ctx.ok = false;
		ok = RunModelLoaders(&ctx);
	}
#endif

	if(setTxd)
		CTxdStore::PopCurrentTxd();
	RwStreamClose(stream, &mem);

	CUSTOM_LOG("model file %s (id %d): %s, %u bytes of .%s -> %u bytes read in place%s\n",
		filename, id, ok ? "loaded" : "FAILED", rawSize, isMod ? "mod" : "dff", (unsigned)file.size(),
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
	customcol::Stats cstats;
	memset(&cstats, 0, sizeof(cstats));

	if(customCls.find(key) != customCls.end()){
		std::vector<uint8> file;
		if(!ReadEntry(customCls, key, file))
			return false;
		br::ClsStats stats;
		std::vector<uint8> col = br::convertClsToCol(file, stats);
		if(col.size() <= 32)
			return false;
		std::vector<uint8> game = customcol::ToGameFormat(col, cstats);
		if(game.size() <= 8)
			return false;
		// what the game's own reader of such a file expects for one model is
		// the block without its header: the name and everything behind it
		out.assign(game.begin() + 8, game.end());
		CUSTOM_LOG("collision %s: block taken from the .cls, %u bytes, %d sphere(s), %d box(es), %d face(s)\n",
			modelname, (unsigned)out.size(), cstats.spheres, cstats.boxes, cstats.faces);
		return true;
	}

	// or it sits as one model inside a .col container in the archives
	std::map<std::string, ColBlockRef>::const_iterator cb = customColBlocks.find(key);
	if(cb == customColBlocks.end())
		return false;

	// find the container the block lives in
	const CustomSource *src = nil;
	std::map<std::string, CustomSource>::const_iterator cf;
	for(cf = customColFiles.begin(); cf != customColFiles.end(); ++cf){
		if(cf->second.archive == cb->second.archive && cf->second.entry == cb->second.entry){
			src = &cf->second;
			break;
		}
	}
	if(src == nil)
		return false;

	std::vector<uint8> file;
	if(!ReadSource(*src, file))
		return false;
	if(cb->second.offset + cb->second.size > file.size())
		return false;
	const uint8 *p = file.data() + cb->second.offset - 8;	// fourcc + size
	if(cb->second.vcBody){
		// the body is already in the layout the game reads
		out.assign(file.begin() + cb->second.offset, file.begin() + cb->second.offset + cb->second.size);
	}else{
		// San Andreas body: rewrite this one block
		std::vector<uint8> block(p, p + 8 + cb->second.size);
		std::vector<uint8> game = customcol::ToGameFormat(block, cstats);
		if(game.size() <= 8){
			CUSTOM_LOG("collision %s: the block could not be converted\n", modelname);
			return false;
		}
		out.assign(game.begin() + 8, game.end());
	}
	CUSTOM_LOG("collision %s: block taken from a .col container, %u bytes\n", modelname, (unsigned)out.size());
	return true;
}

// Animation dictionaries are requested by path ("ANIM\\PED.IFP"); the archives
// hold them as <name>.ifp (SA/BR dictionaries, ANPK or ANP3) or as BR's
// single-animation <name>.ani files. A .ani keeps BR's own header order -
// brformats puts the fields back where SA (and this game) expects them - and
// then flows through the same reader as any SA dictionary.
bool
CCustomModels::LoadAnimFileFromCustom(const char *filename, std::vector<uint8> &out)
{
	if(filename == nil)
		return false;
	EnsureInitialised();
	if(!customActive)
		return false;

	std::string key = ToLower(Stem(filename));
	if(key.empty() || customAnims.find(key) == customAnims.end())
		return false;

	std::string ext = Extension(filename);
	if(!ReadEntry(customAnims, key, out))
		return false;
	if(ext == "ani" && br::isBrAni(out.data(), out.size()) && !br::convertAniToIfp(out)){
		CUSTOM_LOG("animation %s: the .ani header could not be reordered\n", filename);
		out.clear();
		return false;
	}
	if(out.size() < 12){
		out.clear();
		return false;
	}
	CUSTOM_LOG("animation %s: served from the custom folder, %u bytes\n",
		filename, (unsigned)out.size());
	return true;
}

void
CCustomModels::PrintStats(void)
{
	EnsureInitialised();
	CUSTOM_LOG("--- stats ---\n");
	CUSTOM_LOG("%d archive(s) open, %d model name(s) (+%d plain .dff), %d texture name(s) (+%d whole .txd), %d collision name(s) (+%d .col file(s), %d model(s) inside), %d animation name(s), %d image(s)\n",
		(int)customArchives.size(), (int)customMods.size(), (int)customDff.size(),
		(int)customBtx.size(), (int)customTxdFiles.size(),
		(int)customCls.size(), (int)customColFiles.size(), (int)customColBlocks.size(),
		(int)customAnims.size(), (int)customImages.size());
}

// ---------------------------------------------------------------------------
// data files (the "data/timecyc.json" of a BR common archive) and skins
// ---------------------------------------------------------------------------

bool
CCustomModels::ReadDataFile(const char *relpath, std::vector<uint8> &out)
{
	if(relpath == nil || relpath[0] == '\0')
		return false;
	EnsureInitialised();

	std::string want = ToLower(relpath);
	for(size_t i = 0; i < want.size(); i++)
		if(want[i] == '\\')
			want[i] = '/';

	std::map<std::string, CustomSource>::iterator dummy = customMods.begin();
	(void)dummy;
	for(size_t a = 0; a < customArchives.size(); a++){
		CustomZip *zip = customArchives[a];
		for(int i = 0; i < zip->GetNumEntries(); i++){
			const CustomZipEntry *ent = zip->GetEntry(i);
			std::string name = ToLower(ent->name);
			for(size_t k = 0; k < name.size(); k++)
				if(name[k] == '\\')
					name[k] = '/';
			if(name != want && name.size() > want.size() &&
			   name.compare(name.size() - want.size(), want.size(), want) != 0)
				continue;
			if(name != want)
				continue;
			if(ent->uncompressedSize == 0 || ent->uncompressedSize > CUSTOM_MAX_ENTRY)
				return false;
			out.resize(ent->uncompressedSize);
			if(!zip->Extract(i, out.data(), (uint32)out.size())){
				out.clear();
				return false;
			}
			CUSTOM_LOG("data file %s: read from the archives (%u bytes)\n", relpath, (unsigned)out.size());
			return true;
		}
	}

	// or as a loose file in the custom folder
	if(!customFolderPath.empty()){
		CustomSource src = MakeSource(-1, -1, 0, (customFolderPath + "/" + relpath).c_str(), relpath);
		if(ReadSource(src, out)){
			CUSTOM_LOG("data file %s: read from the custom folder (%u bytes)\n", relpath, (unsigned)out.size());
			return true;
		}
	}
	return false;
}

bool
CCustomModels::HasSkin(const char *skinname)
{
	EnsureInitialised();
	if(!customActive || skinname == nil || skinname[0] == '\0')
		return false;
	return customImages.find(ToLower(Stem(skinname))) != customImages.end();
}

// the bytes of one player skin image (".bmp"/".png"/".jpg" named after the
// skin), from the archives or the skins folder of the custom folder
bool
CCustomModels::GetSkinImage(const char *skinname, std::vector<uint8> &out, char *ext, int extCap)
{
	if(skinname == nil || skinname[0] == '\0')
		return false;
	EnsureInitialised();

	std::string key = ToLower(Stem(skinname));
	const CustomSource *src = FindSource(customImages, key);
	if(src == nil)
		return false;
	if(!ReadSource(*src, out))
		return false;
	if(ext != nil && extCap > 0){
		std::string e = Extension(src->fname);
		strncpy(ext, e.c_str(), extCap-1);
		ext[extCap-1] = '\0';
	}
	CUSTOM_LOG("skin %s: %u bytes taken from %s\n", skinname, (unsigned)out.size(), src->fname);
	return true;
}

RwTexture *
CCustomModels::SkinTextureFromBtx(const char *skinname)
{
	if(skinname == nil || skinname[0] == '\0')
		return nil;
	EnsureInitialised();

	std::string key = ToLower(Stem(skinname));
	const CustomSource *src = FindSource(customImages, key);
	if(src == nil)
		return nil;
	std::string ext = Extension(src->fname);
	if(ext != "btx")
		return nil;			// plain images go through GetSkinImage

	std::vector<uint8> file;
	if(!ReadSource(*src, file))
		return nil;
	brtex::Texture tex;
	tex.name = key;
	std::string err;
	if(!brtex::parseBtx(file.data(), file.size(), tex, &err)){
		CUSTOM_LOG("skin %s: the .btx could not be read (%s)\n", skinname, err.c_str());
		return nil;
	}
	RwTexture *texture = BuildTextureGuarded(tex, key.c_str());
	if(texture)
		CUSTOM_LOG("skin %s: built from the .btx in the custom folder\n", skinname);
	return texture;
}

int
CCustomModels::GetNumSkins(void)
{
	EnsureInitialised();
	return (int)customSkinNames.size();
}

// "name.bmp" style - the frontend list strips the extension itself
const char *
CCustomModels::GetSkinFileName(int i)
{
	EnsureInitialised();
	if(i < 0 || i >= (int)customSkinNames.size())
		return nil;
	return customSkinNames[i].c_str();
}

#endif // CUSTOM_MODELS
