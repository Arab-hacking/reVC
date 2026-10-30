#include "common.h"

#include "CustomModels.h"

#ifdef CUSTOM_MODELS

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <memory>
#include <chrono>
#include <stdio.h>		// snprintf
#include <stdlib.h>		// getenv
#include <ctype.h>		// tolower

#include "CustomCol.h"
#include "BrCollisionFormat.h"
#include "CustomColFormat.h"
#include "CustomImgFormat.h"
#include "CdStream.h"
#include "DecodedAssetCache.h"
#include "ResidentObjectCache.h"
#include "BackgroundAssetQueue.h"
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
#include "ModelLoadDispatch.h"
#include "Streaming.h"
#include "TxdStore.h"
#include "ColStore.h"
#include "RwHelper.h"
#include "crossplatform.h" // fcaseopen: native DIR spelling can differ on case-sensitive hosts

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// where the files live
// ---------------------------------------------------------------------------

// Bound a single loose collision/animation/map input before reading it.
#define CUSTOM_MAX_ENTRY (64*1024*1024)
#define CUSTOM_MAX_DEPTH 8			// subdirectories under each asset folder

struct CustomSource
{
	uint64 mtime;
	char loose[256];
	char fname[256];
};

// No ZIP archives are mounted: models and TXDs come only from custom/img/.
// The legacy converters below have no external .mod/.btx/.dff/.txd inputs.
static std::map<std::string, CustomSource> customMods;
static std::map<std::string, CustomSource> customBtx;
static std::map<std::string, CustomSource> customCls;
static std::map<std::string, CustomSource> customAnims;
// Legacy model/texture maps have no entries in the IMG-only layout.
static std::map<std::string, CustomSource> customDff;
static std::map<std::string, CustomSource> customTxdFiles;
static std::map<std::string, CustomSource> customColFiles;	// whole .col containers
static std::map<std::string, CustomSource> customImages;	// .bmp/.png/.jpg
static std::map<std::string, CustomSource> customIdeFiles;
static std::map<std::string, CustomSource> customIplFiles;
// Keep a source for each loose collision container, even with repeated filenames.
static std::vector<CustomSource> customClsSources, customColSources;
static std::vector<bool> customClsHasBrBlocks;
struct BrBlockRef {
	size_t source, offset, size;
	uint64 mtime;
};
static std::map<std::string, BrBlockRef> customBrColBlocks;
static std::map<std::string, BrBlockRef> customLegacyClsBlocks; // after CLS->VC conversion

struct ColBlockRef {
	size_t source, offset, size; // includes [fourcc, size, name, body]
	bool vcBody;
	uint64 mtime;
};
static std::map<std::string, ColBlockRef> customColBlocks;
static std::vector<std::string> customSkinNames;	// "name.bmp" style, for the frontend

static std::string customFolderPath;	// custom root (or REVC_CUSTOM_DIR)
static bool customInitialised = false;
static bool customActive = false;
static int32 nativeModelOverride = -1;
// Guard native streams against repeatedly rejected legacy custom overrides.
static std::set<std::string> rejectedArchiveModels;
static bool customImagesScanned = false;
static std::set<int> customImageSlots; // IMG indices (the stream position's high byte)
static customimg::PairIndex imageModelTextures; // directory names only; no decoded resources

static bool ReadSource(const CustomSource &src, std::vector<uint8> &out);
static void IndexBrCollisionBlocks(void);

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

// File timestamps only compare loose files within this installation.
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

// ---------------------------------------------------------------------------
// indexing explicit custom folders
// ---------------------------------------------------------------------------

static CustomSource
MakeSource(uint64 mtime, const char *path, const char *fname)
{
	CustomSource src;
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
	if(it == index.end() || it->second.mtime < src.mtime)
		index[key] = src;
}

static std::string
WorldPath(const std::string &name)
{
	std::string path = ToLower(name);
	for(size_t k = 0; k < path.size(); k++)
		if(path[k] == '\\') path[k] = '/';
	while(!path.empty() && path[0] == '/') path.erase(0, 1);
	return path;
}

static void
AddWorldFile(std::map<std::string, CustomSource> &index, const std::string &path,
             const CustomSource &src)
{
	std::string key = WorldPath(path);
	if(key.empty() || key.find("../") != std::string::npos || key.size() > 255)
		return;
	std::map<std::string, CustomSource>::iterator it = index.find(key);
	if(it == index.end() || src.mtime > it->second.mtime)
		index[key] = src;
}

// The names of the collision models inside one .col container are indexed at
// scan time, so a model can find its collision whatever file it sits in. The
// blocks keep their [fourcc][size][name][body] layout; whether the body is
// the Vice City layout the game reads natively ('COLL', floats) or a San
// Andreas one ('COL2'/'COL3', fixed point) is remembered for the read.
static void
IndexColContainer(const std::vector<uint8> &file, const CustomSource &src, int &nBlocks)
{
	std::vector<customcolformat::Block> blocks;
	size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
	if(bad) CUSTOM_LOG("collision container %s: %u malformed chunk(s)\n",
		src.fname, (unsigned)bad);
	if(blocks.empty()) return;
	size_t source = customColSources.size();
	customColSources.push_back(src);
	for(size_t i = 0; i < blocks.size(); i++) {
		std::string key = ToLower(blocks[i].name);
		std::map<std::string, ColBlockRef>::iterator it = customColBlocks.find(key);
		if(it != customColBlocks.end() && it->second.mtime >= src.mtime) continue;
		ColBlockRef ref = { source, blocks[i].offset, blocks[i].size, blocks[i].vc, src.mtime };
		customColBlocks[key] = ref;
		nBlocks++;
	}
}

struct ScanStats
{
	int cls, colfile, colblock, anim, ide, ipl;
};

static std::string
Subfolder(const std::string &root, const char *name)
{
	std::vector<std::string> files, dirs;
	if(!ListDirectory(root.c_str(), files, dirs)) return std::string();
	std::sort(dirs.begin(), dirs.end());
	for(size_t i = 0; i < dirs.size(); i++)
		if(ToLower(Stem(dirs[i])) == name) return dirs[i];
	return std::string();
}

// Only visit the four explicit asset folders. Archives and loose legacy files
// in custom/, the game-root img/, common/ and zip/ are never inspected here.
static void
ScanAssetFolder(const std::string &folder, const char *kind, int depth, ScanStats &st)
{
	std::vector<std::string> files, dirs;
	if(!ListDirectory(folder.c_str(), files, dirs)) return;
	std::sort(files.begin(), files.end());
	for(size_t i = 0; i < files.size(); i++) {
		const std::string &path = files[i];
		if(path.size() >= sizeof(CustomSource::loose)) continue;
		const std::string ext = Extension(path);
		const std::string key = ToLower(Stem(path));
		if(key.empty() || key.size() > 63) continue;
		std::string relative = path.substr(customFolderPath.size());
		if(!relative.empty() && (relative[0] == '/' || relative[0] == '\\')) relative.erase(0, 1);
		CustomSource src = MakeSource(FileTime(path.c_str()), path.c_str(), relative.c_str());
		if(strcmp(kind, "anim") == 0) {
			if(ext == "ifp") { AddIndex(customAnims, key, src); st.anim++; }
		} else if(strcmp(kind, "data/maps") == 0) {
			if(ext == "ide" || ext == "ipl") {
				AddWorldFile(ext == "ide" ? customIdeFiles : customIplFiles, relative, src);
				if(ext == "ide") st.ide++; else st.ipl++;
			}
		} else if(strcmp(kind, "collision") == 0) {
			if(ext == "cls") {
				AddIndex(customCls, key, src);
				customClsSources.push_back(src);
				customClsHasBrBlocks.push_back(false);
				st.cls++;
			} else if(ext == "col") {
				std::vector<uint8> bytes;
				if(ReadSource(src, bytes)) {
					IndexColContainer(bytes, src, st.colblock);
					AddIndex(customColFiles, key, src);
					st.colfile++;
				} else CUSTOM_LOG("collision %s: cannot read .col\n", path.c_str());
			}
		}
	}
	if(depth >= CUSTOM_MAX_DEPTH) return;
	std::sort(dirs.begin(), dirs.end());
	for(size_t i = 0; i < dirs.size(); i++)
		ScanAssetFolder(dirs[i], kind, depth+1, st);
}

static void
EnsureInitialised(void)
{
	if(customInitialised) return;
	customInitialised = true;
	const char *folder = CUSTOM_MODELS_FOLDER;
	const char *env = getenv("REVC_CUSTOM_DIR");
	if(env && env[0]) folder = env;
	customFolderPath = folder;
	ScanStats st = {};
	const std::string anim = Subfolder(customFolderPath, "anim");
	const std::string collision = Subfolder(customFolderPath, "collision");
	const std::string data = Subfolder(customFolderPath, "data");
	const std::string maps = data.empty() ? std::string() : Subfolder(data, "maps");
	if(!anim.empty()) ScanAssetFolder(anim, "anim", 0, st);
	if(!collision.empty()) ScanAssetFolder(collision, "collision", 0, st);
	if(!maps.empty()) ScanAssetFolder(maps, "data/maps", 0, st);
	IndexBrCollisionBlocks();
	customActive = !customAnims.empty() || !customCls.empty() ||
	               !customColFiles.empty() || !customIdeFiles.empty() || !customIplFiles.empty();
	CUSTOM_LOG("==== custom folder: %s (loose folders only; ZIP, game-root img/ and common/ disabled)\n", customFolderPath.c_str());
	CUSTOM_LOG("scan: %d .ifp (%u names), %d .cls (%u CED2 models), %d .col (%d models), %d IDE, %d IPL\n",
		st.anim, (unsigned)customAnims.size(), st.cls, (unsigned)customBrColBlocks.size(),
		st.colfile, st.colblock, st.ide, st.ipl);
	if(!customActive)
		CUSTOM_LOG("scan: no loose animation/map/collision files; checking custom/img/ for models\n");
}

// ---------------------------------------------------------------------------
// reading entries
// ---------------------------------------------------------------------------

// Read only an explicitly indexed loose file. No ZIP reader is called at runtime.
static bool
ReadSource(const CustomSource &src, std::vector<uint8> &out)
{
	out.clear();
	if(src.loose[0] == '\0') return false;
	FILE *f = fcaseopen(src.loose, "rb");
	if(f == nil) return false;
	if(fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
	long size = ftell(f);
	if(size <= 0 || (uint64)size > CUSTOM_MAX_ENTRY || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}
	out.resize((size_t)size);
	bool ok = fread(out.data(), 1, (size_t)size, f) == (size_t)size;
	fclose(f);
	if(!ok) out.clear();
	return ok;
}

// The IMG streamer uses a one-byte image index and expects an external VC
// .dir. Discover pairs before Streaming::Init, after DAT CDIMAGE directives;
// this also lets names from the custom IDEs be registered before indexing.
struct ImagePair { std::string img, dir; };

static void
ScanImagePairs(const std::string &folder, int depth, std::vector<ImagePair> &pairs)
{
	std::vector<std::string> files, dirs;
	if(!ListDirectory(folder.c_str(), files, dirs)) return;
	std::map<std::string, std::string> byName;
	for(size_t i = 0; i < files.size(); i++)
		byName[ToLower(files[i])] = files[i];
	for(size_t i = 0; i < files.size(); i++) {
		if(Extension(files[i]) != "img") continue;
		std::string sibling = ToLower(files[i].substr(0, files[i].find_last_of('.')) + ".dir");
		std::map<std::string, std::string>::const_iterator found = byName.find(sibling);
		ImagePair pair = { files[i], found == byName.end() ? std::string() : found->second };
		pairs.push_back(pair);
	}
	if(depth >= CUSTOM_MAX_DEPTH) return;
	std::sort(dirs.begin(), dirs.end());
	for(size_t i = 0; i < dirs.size(); i++)
		ScanImagePairs(dirs[i], depth+1, pairs);
}

static uint64
ImageBytes(const char *path)
{
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA fa;
	if(!GetFileAttributesExA(path, GetFileExInfoStandard, &fa) ||
	   (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
	return ((uint64)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
#else
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode) ? (uint64)st.st_size : 0;
#endif
}

static std::string
ImagePathKey(std::string path)
{
	path = ToLower(path);
	for(size_t i = 0; i < path.size(); i++)
		if(path[i] == '\\') path[i] = '/';
	while(path.compare(0, 2, "./") == 0) path.erase(0, 2);
	return path;
}

// Native IDEs have been read, but their DIRs are indexed AFTER custom/img/.
// Reserve native TXD names; the streamer's reverse IMG order prefers custom/img/.
static void
IndexNativeImageDirectories(void)
{
	// Stock DFF metadata remains available to scripts when the VC map IDE is
	// disabled. Reserve stock TXD names
	// only when additional IMGs actually need that headroom.
	bool reserveTxds = !customImageSlots.empty();
	unsigned reserved = 0, full = 0;
	for(int n = CdStreamGetNumImages()-1; n >= 0; n--) {
		if(customImageSlots.count(n)) continue;
		const char *image = CdStreamGetImageName(n);
		if(!image) continue;
		std::string dirName(image);
		size_t dot = dirName.find_last_of('.');
		if(dot == std::string::npos || dot+4 > dirName.size()) continue;
		// Match LoadCdDirectory's extension replacement, including any
		// whitespace suffix in a stock CDIMAGE line.
		dirName.replace(dot+1, 3, "DIR");
		CustomSource dirFile = MakeSource(0, dirName.c_str(), dirName.c_str());
		std::vector<uint8> directory;
		if(!ReadSource(dirFile, directory)) continue;
		imageModelTextures.AddDirectory(n, directory.data(), directory.size());
		if(reserveTxds)
			customimg::ForEachNativeTxd(directory.data(), directory.size(),
				[&](const std::string &name) {
					if(CTxdStore::FindTxdSlot(name.c_str()) != -1) return;
					if(!CTxdStore::HasFreeSlot()) { full++; return; }
					CTxdStore::AddTxdSlot(name.c_str());
					reserved++;
				});
	}
	if(reserveTxds)
		CUSTOM_LOG("custom/img: reserved %u native TXD slots before custom images (%u without space)\n",
			reserved, full);
}

void
CCustomModels::ResetImageMounts(void)
{
	customImagesScanned = false;
	customImageSlots.clear();
	imageModelTextures.Clear();
}

bool
CCustomModels::IsAutoMountedImage(int index)
{
	return customImageSlots.find(index) != customImageSlots.end();
}

int
CCustomModels::PairedImageTxd(const char *modelName)
{
	int dffImage = -1;
	int txdImage = imageModelTextures.FindTxdImage(modelName, &dffImage);
	return IsAutoMountedImage(dffImage) && IsAutoMountedImage(txdImage) ? txdImage : -1;
}

bool
CCustomModels::FindModelDffInImage(const char *modelName, uint32 &position, uint32 &sectors)
{
	return imageModelTextures.FindDffRecord(modelName, position, sectors);
}

void
CCustomModels::MountImages(void)
{
	if(customImagesScanned) return;
	customImagesScanned = true;
	EnsureInitialised();
	std::vector<ImagePair> pairs;
	const std::string images = Subfolder(customFolderPath, "img");
	if(!images.empty()) ScanImagePairs(images, 0, pairs);
	std::sort(pairs.begin(), pairs.end(), [](const ImagePair &a, const ImagePair &b) {
		return ImagePathKey(a.img) < ImagePathKey(b.img);
	});
	unsigned mounted = 0, rejected = 0;
	for(size_t i = 0; i < pairs.size(); i++) {
		const ImagePair &pair = pairs[i];
		if(pair.dir.empty() || pair.img.size() >= 64) {
			CUSTOM_LOG("custom/img %s: missing sibling .dir or path exceeds 63 characters\n", pair.img.c_str());
			rejected++; continue;
		}
		uint64 imageBytes = ImageBytes(pair.img.c_str());
		std::vector<uint8> directory;
		CustomSource dirFile = MakeSource(0, pair.dir.c_str(), pair.dir.c_str());
		customimg::Stats stats;
		std::string why;
		FILE *img = fopen(pair.img.c_str(), "rb");
		char magic[4] = {};
		bool v2 = img && fread(magic, 1, 4, img) == 4 && !memcmp(magic, "VER2", 4);
		if(img) fclose(img);
		if(v2 || !ReadSource(dirFile, directory) ||
		   !customimg::Validate(directory.empty() ? nil : directory.data(),
		                       directory.size(), imageBytes, stats, why)) {
			CUSTOM_LOG("custom/img %s: skipped (%s)\n", pair.img.c_str(),
				v2 ? "SA IMG v2; VC IMG v1 + external .dir required" :
				why.empty() ? "cannot read .dir" : why.c_str());
			rejected++; continue;
		}
		int slot = -1;
		for(int n = 0; n < CdStreamGetNumImages(); n++)
			if(ImagePathKey(CdStreamGetImageName(n)) == ImagePathKey(pair.img)) {
				slot = n; break; // already mounted through CDIMAGE
			}
		if(slot < 0) {
			if(CdStreamGetNumImages() >= MAX_CDIMAGES || !CdStreamAddImage(pair.img.c_str())) {
				CUSTOM_LOG("custom/img %s: could not mount (IMG slot limit or I/O error)\n", pair.img.c_str());
				rejected++; continue;
			}
			slot = CdStreamGetNumImages()-1;
		}
		customImageSlots.insert(slot);
		imageModelTextures.AddDirectory(slot, directory.data(), directory.size());
		mounted++;
		customActive = true;
		CUSTOM_LOG("custom/img %s: IMG %d, %u DFF, %u TXD, %u other DIR entries ignored\n",
			pair.img.c_str(), slot, (unsigned)stats.dff, (unsigned)stats.txd, (unsigned)stats.ignored);
	}
	CUSTOM_LOG("custom/img/: %u IMG(s) found, %u mounted, %u skipped\n",
		(unsigned)pairs.size(), mounted, rejected);
	IndexNativeImageDirectories(); // before CCustomWorld::LoadIde and CStreaming::Init
}

static std::vector<uint8>
LegacyClsToGame(const std::vector<uint8> &data)
{
	br::ClsStats stats;
	std::vector<uint8> col = br::convertClsToCol(data, stats);
	customcol::Stats cstats;
	return customcol::ToGameFormat(col, cstats);
}

static void
IndexBrCollisionBlocks(void)
{
	for(size_t source = 0; source < customClsSources.size(); source++){
		const CustomSource &src = customClsSources[source];
		std::vector<uint8> data;
		if(!ReadSource(src, data)){
			CUSTOM_LOG("collision container %s: cannot read .cls\n", src.fname);
			continue;
		}
		std::vector<brcollision::Block> blocks;
		size_t bad = brcollision::Index(data.data(), data.size(), blocks);
		customClsHasBrBlocks[source] = !blocks.empty();
		for(size_t b = 0; b < blocks.size(); b++){
			std::string key = ToLower(blocks[b].name);
			std::map<std::string, BrBlockRef>::iterator found = customBrColBlocks.find(key);
			if(found != customBrColBlocks.end() && found->second.mtime >= src.mtime)
				continue;
			BrBlockRef ref = { source, blocks[b].offset, blocks[b].size, src.mtime };
			customBrColBlocks[key] = ref;
		}
		if(blocks.empty()) {
			// Non-CED2 .cls containers may share a filename across loose folders.
			// Index the INTERNAL names of their converted blocks.
			std::vector<uint8> game = LegacyClsToGame(data);
			std::vector<customcolformat::Block> legacy;
			customcolformat::Index(game.data(), game.size(), legacy);
			for(size_t b = 0; b < legacy.size(); b++) {
				const customcolformat::Block &block = legacy[b];
				if(!customcolformat::ValidVC(game.data()+block.offset, block.size)) continue;
				std::string key = ToLower(block.name);
				std::map<std::string, BrBlockRef>::const_iterator found = customLegacyClsBlocks.find(key);
				if(found != customLegacyClsBlocks.end() && found->second.mtime >= src.mtime) continue;
				BrBlockRef ref = { source, block.offset, block.size, src.mtime };
				customLegacyClsBlocks[key] = ref;
			}
			if(!legacy.empty()) CUSTOM_LOG("collision container %s: %u legacy .cls block(s)\n",
				src.fname, (unsigned)legacy.size());
		}
		if(!blocks.empty() || bad)
			CUSTOM_LOG("collision container %s: %u CED2 block(s), %u invalid chunk(s)\n",
				src.fname, (unsigned)blocks.size(), (unsigned)bad);
	}
}

static bool
HasBrBlocks(const CustomSource &src)
{
	for(size_t i = 0; i < customClsSources.size(); i++){
		const CustomSource &other = customClsSources[i];
		bool same = strcmp(src.loose, other.loose) == 0;
		if(same) return customClsHasBrBlocks[i];
	}
	return false;
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

// A loose .col container: Vice City blocks ('COLL'
// floats) are what the game reads and go through as they are, San Andreas
// blocks ('COL2'/'COL3', fixed point) are rewritten by the converter
static std::vector<uint8>
ColContainerToGame(const std::vector<uint8> &file, const char *what, const std::string &key)
{
	std::vector<customcolformat::Block> blocks;
	size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
	std::vector<uint8> result;
	for(size_t i = 0; i < blocks.size(); i++) {
		const customcolformat::Block &b = blocks[i];
		const uint8 *p = file.data()+b.offset;
		if(b.vc) {
			if(customcolformat::ValidVC(p, b.size))
				result.insert(result.end(), p, p+b.size);
			else bad++;
		} else {
			std::vector<uint8> raw(p, p+b.size);
			customcol::Stats cstats;
			std::vector<uint8> game = customcol::ToGameFormat(raw, cstats);
			if(customcolformat::ValidVC(game.data(), game.size()))
				result.insert(result.end(), game.begin(), game.end());
			else bad++;
		}
	}
	if(bad || result.empty())
		CUSTOM_LOG("%s %s: %u invalid collision block(s), %u usable bytes\n",
			what, key.c_str(), (unsigned)bad, (unsigned)result.size());
	return result;
}

// ---------------------------------------------------------------------------
// model: .mod -> a RenderWare stream the engine can read
// ---------------------------------------------------------------------------

// Offline converter code is retained, but no .mod/.dff source is indexed here.

static bool ValidateModelForGame(const std::string &key, bool isMod,
                                 const std::vector<uint8> &file, std::string &why);

// Prepared model bytes are immutable once repaired/decrypted. CanServe's
// validation, TXD texture discovery and the actual RenderWare load all share
// them, including across streaming unload/reload. RenderWare still owns and
// evicts its *object*; only the prepared input is cached here.
struct PreparedModel
{
	std::vector<uint8> bytes;
	br::RwFixStats stats;
	std::string warn;
	std::string rejectReason;
	bool isMod;
	bool usable;
	bool readOk;
	unsigned rawSize;
	std::vector<std::string> textureNames; // scanned off the game thread too
	std::vector<std::string> uv2DualNames; // explicit lightmaps only, not diffuse
};

// Only pure CPU work is allowed here. In particular, no CModelInfo, TXD,
// RenderWare or GPU calls: the same function can run on the streaming worker.
static std::shared_ptr<PreparedModel>
PrepareModelBytes(const std::string &key, const CustomSource &src, bool isMod)
{
	std::shared_ptr<PreparedModel> model = std::make_shared<PreparedModel>();
	model->isMod = isMod;
	model->usable = false;
	model->readOk = false;
	if(!ReadSource(src, model->bytes)){
		model->rejectReason = "the loose file could not be read";
		return model;
	}
	model->rawSize = (unsigned)model->bytes.size();
	if(isMod && !br::understandModInplace(model->bytes, model->stats, model->warn)){
		model->rejectReason = model->warn;
		return model;
	}
	model->readOk = true;
	model->usable = ValidateModelForGame(key, isMod, model->bytes, model->rejectReason);
	brtex::collectTextureNames(model->bytes.data(), model->bytes.size(), model->textureNames);
	brtex::collectUv2DualTextureNames(model->bytes.data(), model->bytes.size(), model->uv2DualNames);
	return model;
}

// Keep the existing per-cache ceilings. Any larger aggregate cache budget
// should be chosen explicitly, separately from enabling background streaming.
static const size_t CUSTOM_MODEL_CACHE_BYTES = 64u * 1024u * 1024u;
static const size_t CUSTOM_TEXTURE_CACHE_BYTES = 64u * 1024u * 1024u;
static customcache::DecodedAssetCache<PreparedModel> customDecodedModels(CUSTOM_MODEL_CACHE_BYTES);

static std::shared_ptr<const PreparedModel>
ReadModelEntry(const std::string &key)
{
	std::shared_ptr<const PreparedModel> cached = customDecodedModels.Find(key);
	if(cached)
		return cached;
	const CustomSource *src = FindSource(customMods, key);
	bool isMod = src != nil;
	if(src == nil)
		src = FindSource(customDff, key);
	if(src == nil)
		return std::shared_ptr<const PreparedModel>();
	std::shared_ptr<PreparedModel> model = PrepareModelBytes(key, *src, isMod);
	if(!model->readOk){
		CUSTOM_LOG("model %s: cannot prepare .%s (%s)\n", key.c_str(),
			isMod ? "mod" : "dff", model->rejectReason.c_str());
		return std::shared_ptr<const PreparedModel>();
	}
	if(!model->warn.empty())
		CUSTOM_LOG("  read note: %s\n", model->warn.c_str());
	customDecodedModels.Insert(key, model, model->bytes.capacity() + sizeof(PreparedModel));
	return model;
}

// Names and terrain recipes are discovered once per TXD plan from the same
// bytes that will later be given to RenderWare; no second XTEA pass.
static bool
ModelTextureNames(const std::string &key, std::vector<std::string> &names,
                  std::vector<br::TerrainRecipe> &recipes, std::vector<std::string> &uv2DualNames)
{
	std::shared_ptr<const PreparedModel> model = ReadModelEntry(key);
	if(!model)
		return false;
	names = model->textureNames;
	recipes = model->stats.recipes;
	uv2DualNames = model->uv2DualNames;
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

// A ped model is always read through the clump loader. Unlike ordinary model
// indexing, this path must never accept a different RW root (notably the tiny
// player.mod placeholder found in some custom packs). Requiring both a clump
// root and the geometry validator is consistent for archive and loose files.
static bool
ValidateModelForGame(const std::string &key, bool isMod, const std::vector<uint8> &file, std::string &why)
{
	if(key == "player" && isMod){
		why = "player.mod is reserved and unsafe; use a validated player.dff";
		return false;
	}
	if(!IsClumpStream(file)){
		why = "the root RenderWare chunk is not a clump";
		return false;
	}
	return br::validateClumpForGame(file.data(), (uint32)file.size(), why);
}

static bool
CanUseModel(const std::string &key)
{
	ModPlan &plan = customModPlans[key];
	if(plan.computed)
		return plan.usable;
	plan.computed = true;

	std::shared_ptr<const PreparedModel> model = ReadModelEntry(key);
	plan.usable = model && model->usable;
	if(!plan.usable)
		CUSTOM_LOG("model %s: not usable as a game model%s, the game's own file is used\n", key.c_str(),
			!model || model->rejectReason.empty() ? "" : (" (" + model->rejectReason + ")").c_str());
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
	std::vector<std::string> uv2DualNames; // used for texture discovery
	std::vector<br::TerrainRecipe> recipes;
};

static std::map<std::string, TxdPlan> customTxdPlans;
static std::map<std::string, std::string> customTxdCandidates;

// The dictionary a model uses does not always carry the model's name (a car is
// called "hotdog" but its textures can live in "hotdog1"), so a name that is
// not in the loose index was matched against a converted model (now unused).
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

static std::string PotentialModelForTxd(const std::string &key);

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
	if(!plan.haveModel){
		plan.modelKey = PotentialModelForTxd(key);
		plan.haveModel = !plan.modelKey.empty();
	}
	plan.haveBtx = customBtx.find(key) != customBtx.end();
	plan.haveTxdFile = customTxdFiles.find(key) != customTxdFiles.end();
	if(plan.haveModel){
		// the name of a texture is the name of its .btx file; a model texture
		// that is a terrain composite is named after its mask
		ModelTextureNames(plan.modelKey, plan.names, plan.recipes, plan.uv2DualNames);
		for(size_t i = 0; i < plan.names.size(); i++)
			if(customBtx.find(ToLower(plan.names[i])) != customBtx.end()){
				// Never turn a native IMG TXD into an incomplete custom TXD.
				// Native IMG diffuse textures keep their normal streaming path.
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
	switch(GetModelLoadKind(c->mi->GetModelType())){
	case MODELLOAD_ATOMIC:
		c->ok = CFileLoader::LoadAtomicFile(c->stream, c->modelId);
		break;
	case MODELLOAD_VEHICLE:
		// Unlike streaming, both halves are read in this one call; no extra
		// model reference needs to be held across frames.
		c->ok = CFileLoader::StartLoadClumpFile(c->stream, c->modelId) &&
		        CFileLoader::FinishLoadClumpFile(c->stream, c->modelId);
		break;
	case MODELLOAD_PED:
	case MODELLOAD_CLUMP:
		c->ok = CFileLoader::LoadClumpFile(c->stream, c->modelId);
		break;
	default:
		CUSTOM_LOG("model %d: unsupported IDE model info type %d; refusing DFF\n",
			c->modelId, c->mi->GetModelType());
		c->ok = false;
		break;
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
	std::shared_ptr<const PreparedModel> model = ReadModelEntry(key);
	if(!model){
		CUSTOM_LOG("model %s: cannot read the archive entry\n", key.c_str());
		return false;
	}

	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelId);
	if(mi == nil) return false;

	// Validation runs once on the immutable prepared bytes, not again on
	// every reappearance of a model in the streaming pool.
	if(!model->usable){
		CUSTOM_LOG("model %s: not usable as a game model (%s), the game's own file is used\n",
			key.c_str(), model->rejectReason.c_str());
		return false;
	}

	RwMemory mem;
	// rwSTREAMREAD never writes the memory; keep the shared input alive until
	// RwStreamClose even if LRU eviction happens while a different asset loads.
	mem.start = const_cast<uint8*>(model->bytes.data());
	mem.length = (uint32)model->bytes.size();
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

	CUSTOM_LOG("model %s (id %d): %s, %u bytes of .%s -> %u prepared bytes%s%s\n", key.c_str(), modelId,
		ok ? "loaded" : "FAILED", model->rawSize, model->isMod ? "mod" : "dff", (unsigned)model->bytes.size(),
		model->stats.versionsChanged ? ", chunk versions fixed" : "",
		model->warn.empty() ? "" : (", " + model->warn).c_str());
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



static customcache::DecodedAssetCache<brtex::Texture> customDecodedTextures(CUSTOM_TEXTURE_CACHE_BYTES);
static std::set<std::string> customFailedCpuTextures;
// A single texture larger than the LRU budget is still delivered once from
// the worker to the next load. It is not allowed to accumulate indefinitely.
static std::string customTransientTextureKey;
static std::shared_ptr<const brtex::Texture> customTransientTexture;

static bool
HasPreparedTexture(const std::string &key)
{
	return customDecodedTextures.Contains(key) ||
	       (customTransientTexture && customTransientTextureKey == key) ||
	       customFailedCpuTextures.find(key) != customFailedCpuTextures.end();
}

static size_t
DecodedTextureSize(const brtex::Texture &tex)
{
	size_t bytes = sizeof(tex);
	for(size_t i = 0; i < tex.mips.size(); i++)
		bytes += sizeof(brtex::Mip) + tex.mips[i].rgba.capacity();
	return bytes;
}

// A long .btx name may also be indexed by its shortened RW name. Cache by
// source identity, so either spelling (including a skin) shares one decode.
static std::string
BtxCacheKey(const CustomSource &src)
{
	return std::string("loose:") + src.loose;
}

static std::shared_ptr<brtex::Texture>
DecodeBtxBytes(const CustomSource &src, const std::string &name, std::string &err)
{
	std::vector<uint8> file;
	if(!ReadSource(src, file)){
		err = "the loose file could not be read";
		return std::shared_ptr<brtex::Texture>();
	}
	std::shared_ptr<brtex::Texture> tex = std::make_shared<brtex::Texture>();
	tex->name = name;
	if(!brtex::parseBtx(file.data(), file.size(), *tex, &err))
		return std::shared_ptr<brtex::Texture>();
	return tex;
}

static std::shared_ptr<const brtex::Texture>
GetDecodedBtx(const CustomSource &src, const std::string &name)
{
	std::string key = BtxCacheKey(src);
	std::shared_ptr<const brtex::Texture> cached = customDecodedTextures.Find(key);
	if(cached)
		return cached;
	if(customTransientTexture && customTransientTextureKey == key){
		std::shared_ptr<const brtex::Texture> ready = customTransientTexture;
		customTransientTexture.reset(); // caller pins it through the GPU build
		customTransientTextureKey.clear();
		return ready;
	}
	if(customFailedCpuTextures.find(key) != customFailedCpuTextures.end())
		return std::shared_ptr<const brtex::Texture>();
	std::string err;
	std::shared_ptr<brtex::Texture> tex = DecodeBtxBytes(src, name, err);
	if(!tex){
		CUSTOM_LOG("texture %s: %s\n", name.c_str(), err.c_str());
		return std::shared_ptr<const brtex::Texture>();
	}
	customDecodedTextures.Insert(key, tex, DecodedTextureSize(*tex));
	return tex;
}

static std::shared_ptr<const brtex::Texture>
GetDecodedBtx(const std::string &name)
{
	const CustomSource *src = FindSource(customBtx, ToLower(name));
	return src ? GetDecodedBtx(*src, name) : std::shared_ptr<const brtex::Texture>();
}

// Decode and bake terrain without touching game state. The input map is a
// snapshot of source descriptors, not live iterators into the archive index.
static std::shared_ptr<brtex::Texture>
BakeTerrainBytes(const br::TerrainRecipe &recipe,
                 const std::map<std::string, CustomSource> &sources)
{
	std::shared_ptr<brtex::Texture> baked = std::make_shared<brtex::Texture>();
	std::function<bool(const std::string&, brtex::Texture&)> read =
		[&sources](const std::string &name, brtex::Texture &out){
			std::map<std::string, CustomSource>::const_iterator src = sources.find(ToLower(name));
			if(src == sources.end())
				return false;
			std::string error;
			std::shared_ptr<brtex::Texture> layer = DecodeBtxBytes(src->second, name, error);
			if(!layer)
				return false;
			out = *layer;
			return true;
		};
	return brtex::bakeTerrain(recipe, read, *baked, 512, 0.0f) ? baked : std::shared_ptr<brtex::Texture>();
}

// The terrain baker's callback currently takes an output Texture by value.
// Individual layers are decoded once and copied only when a bake is needed.
static bool
LoadBtxTexture(const std::string &name, brtex::Texture &out)
{
	std::shared_ptr<const brtex::Texture> tex = GetDecodedBtx(name);
	if(!tex)
		return false;
	out = *tex;
	return true;
}

static std::shared_ptr<const brtex::Texture>
GetPreparedTexture(const std::string &modelKey, const std::string &name,
                   const br::TerrainRecipe *recipe)
{
	if(!recipe) return GetDecodedBtx(name);
	std::string key = "terrain:" + modelKey + ":" + ToLower(name);
	std::shared_ptr<const brtex::Texture> cached = customDecodedTextures.Find(key);
	if(cached)
		return cached;
	if(customTransientTexture && customTransientTextureKey == key){
		std::shared_ptr<const brtex::Texture> ready = customTransientTexture;
		customTransientTexture.reset();
		customTransientTextureKey.clear();
		return ready;
	}
	if(customFailedCpuTextures.find(key) != customFailedCpuTextures.end())
		return std::shared_ptr<const brtex::Texture>();
	std::shared_ptr<brtex::Texture> tex = std::make_shared<brtex::Texture>();
	if(!brtex::bakeTerrain(*recipe, LoadBtxTexture, *tex, 512, 0.0f))
		return std::shared_ptr<const brtex::Texture>();
	customDecodedTextures.Insert(key, tex, DecodedTextureSize(*tex));
	return tex;
}

// Builds a game texture out of the decoded mip chain. The mip levels are
// uploaded one by one (the raster API takes care of the platform specific
// layout), which is why the pixels have to be written in the platform's own
// byte order: Direct3D rasters are BGRA, OpenGL ones are RGBA.
static RwTexture*
BuildTexture(const brtex::Texture &decoded, const char *name)
{
	// Never mutate a cached decode. Resample NPOT only for this GPU upload;
	// the cache remains usable on either GL or Direct3D.
	brtex::Texture pot;
	const brtex::Texture *tex = &decoded;
	if(!decoded.mips.empty() && !(brtex::isPot(decoded.mips[0].w) && brtex::isPot(decoded.mips[0].h))){
		pot = decoded;
		brtex::makePot(pot);
		tex = &pot;
	}
	if(tex->mips.empty())
		return nil;

	uint32 w = tex->mips[0].w, h = tex->mips[0].h;
	if(w == 0 || h == 0)
		return nil;
	int levels = (int)tex->mips.size();
	if(levels > 1 && (tex->mips[0].w < 4 || tex->mips[0].h < 4))
		levels = 1;

	RwRaster *ras = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 |
		(levels > 1 ? rwRASTERFORMATMIPMAP : 0));
	if(ras == nil)
		return nil;

	bool swapRB = rw::platform == rw::PLATFORM_D3D9 || rw::platform == rw::PLATFORM_D3D8 ||
	              rw::platform == rw::PLATFORM_XBOX;
	for(int l = 0; l < levels; l++){
		const brtex::Mip &mip = tex->mips[l];
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
struct GuardedTexCtx { const brtex::Texture *tex; const char *name; RwTexture *out; };
static bool
RunBuildTexture(void *p)
{
	GuardedTexCtx *c = (GuardedTexCtx*)p;
	c->out = BuildTexture(*c->tex, c->name);
	return c->out != nil;
}

static RwTexture *
BuildTextureGuarded(const brtex::Texture &tex, const char *name)
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

// The streaming system normally destroys a TXD when its last model is
// evicted. Keep a *detached* dictionary (including GPU rasters) for a bounded
// number of recent custom slots; never move one texture between two live TXDs.
// A Take transfers the original dictionary back to CTxdStore, so neither the
// cache nor the streaming system can destroy the same object twice.
static void
DestroyCachedTxd(RwTexDictionary *dict)
{
	RwTexDictionaryDestroy(dict);
}
static const size_t CUSTOM_GPU_TXD_CACHE_BYTES = 64u * 1024u * 1024u;
static customcache::ResidentObjectCache<RwTexDictionary> customResidentTxds(CUSTOM_GPU_TXD_CACHE_BYTES, DestroyCachedTxd);
static std::map<int32, RwTexDictionary*> customLoadedTxds;

// CPU preparation is separate from RenderWare. The only things passed to the
// worker are immutable loose-source descriptors, names and terrain recipes.
// Results enter the existing LRU caches ONLY on the game thread.
struct CpuWorkResult
{
	std::shared_ptr<PreparedModel> model;
	std::shared_ptr<brtex::Texture> texture;
	std::string error;
	unsigned milliseconds;
};
static customcache::BackgroundAssetQueue<CpuWorkResult> customCpuWorker(16, 1);
static std::map<int32, std::set<std::string> > customJobInterests;
// If a prepared asset is evicted before the rest of a TXD fits in the LRU,
// don't keep decoding it forever. Fall back to the ordinary load of that TXD.
static std::map<int32, std::set<std::string> > customCompletedCpuJobs;
static std::map<int32, std::chrono::steady_clock::time_point> customPreparationWaits;
static std::set<int32> customSyncFallbackStreams;
static size_t customWorkerModels = 0, customWorkerTextures = 0, customWorkerFailures = 0;
static size_t customWorkerSyncFallbacks = 0;

static std::string ModelJobKey(const std::string &key) { return "m:" + key; }
static std::string TextureJobKey(const std::string &key) { return "b:" + key; }

static void
TrackCpuJob(int32 streamId, const std::string &job)
{
	if(streamId >= 0)
		customJobInterests[streamId].insert(job);
}

static bool
QueueModelPreparation(const std::string &key, int32 streamId, bool priority)
{
	if(customDecodedModels.Contains(key))
		return true;
	std::map<std::string, ModPlan>::const_iterator plan = customModPlans.find(key);
	if(plan != customModPlans.end() && plan->second.computed && !plan->second.usable)
		return true;
	const CustomSource *src = FindSource(customMods, key);
	bool isMod = src != nil;
	if(!src) src = FindSource(customDff, key);
	if(!src) return true;
	const CustomSource copy = *src;
	std::string jobKey = ModelJobKey(key);
	bool queued = customCpuWorker.Schedule(jobKey, [copy, key, isMod]{
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		std::shared_ptr<CpuWorkResult> result = std::make_shared<CpuWorkResult>();
		result->model = PrepareModelBytes(key, copy, isMod);
		result->milliseconds = (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count();
		return result;
	}, priority);
	if(queued) TrackCpuJob(streamId, jobKey);
	return queued;
}

static bool
QueueTexturePreparation(const std::string &cacheKey, const std::string &name,
                        const CustomSource &source, int32 streamId, bool priority)
{
	if(HasPreparedTexture(cacheKey))
		return true;
	const CustomSource copy = source;
	std::string jobKey = TextureJobKey(cacheKey);
	bool queued = customCpuWorker.Schedule(jobKey, [copy, name]{
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		std::shared_ptr<CpuWorkResult> result = std::make_shared<CpuWorkResult>();
		result->texture = DecodeBtxBytes(copy, name, result->error);
		result->milliseconds = (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count();
		return result;
	}, priority);
	if(queued) TrackCpuJob(streamId, jobKey);
	return queued;
}

static bool
QueueTerrainPreparation(const std::string &cacheKey, const br::TerrainRecipe &recipe,
                        int32 streamId, bool priority)
{
	if(HasPreparedTexture(cacheKey))
		return true;
	std::map<std::string, CustomSource> inputs;
	const std::string names[5] = { recipe.tex, recipe.layers[0], recipe.layers[1], recipe.layers[2], recipe.layers[3] };
	for(int i = 0; i < 5; i++){
		const CustomSource *source = FindSource(customBtx, ToLower(names[i]));
		if(source && !names[i].empty()) inputs[ToLower(names[i])] = *source;
	}
	std::string jobKey = TextureJobKey(cacheKey);
	bool queued = customCpuWorker.Schedule(jobKey, [recipe, inputs]{
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		std::shared_ptr<CpuWorkResult> result = std::make_shared<CpuWorkResult>();
		result->texture = BakeTerrainBytes(recipe, inputs);
		if(!result->texture) result->error = "no usable terrain layers";
		result->milliseconds = (unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count();
		return result;
	}, priority);
	if(queued) TrackCpuJob(streamId, jobKey);
	return queued;
}

void
CCustomModels::PumpBackgroundResults(void)
{
	customcache::BackgroundAssetQueue<CpuWorkResult>::Completed completed;
	while(customCpuWorker.Pop(completed)){
		std::shared_ptr<CpuWorkResult> result = completed.value;
		if(completed.key.compare(0, 2, "m:") == 0){
			std::string key = completed.key.substr(2);
			std::shared_ptr<PreparedModel> model = result ? result->model : std::shared_ptr<PreparedModel>();
			ModPlan &plan = customModPlans[key];
			plan.computed = true;
			plan.usable = model && model->readOk && model->usable;
			if(model && model->readOk){
				customDecodedModels.Insert(key, model, model->bytes.capacity() + sizeof(PreparedModel));
				customWorkerModels++;
			}
			if(!plan.usable){
				customWorkerFailures++;
				CUSTOM_LOG("background model %s: rejected (%s)\n", key.c_str(),
					model ? model->rejectReason.c_str() : "worker error");
			}else if(!model->warn.empty())
				CUSTOM_LOG("background model %s: %s\n", key.c_str(), model->warn.c_str());
		}else if(completed.key.compare(0, 2, "b:") == 0){
			std::string key = completed.key.substr(2);
			if(result && result->texture){
				size_t bytes = DecodedTextureSize(*result->texture);
				if(!customDecodedTextures.Insert(key, result->texture, bytes)){
					customTransientTextureKey = key;
					customTransientTexture = result->texture;
				}
				customWorkerTextures++;
			}else{
				customFailedCpuTextures.insert(key);
				customWorkerFailures++;
				CUSTOM_LOG("background texture %s: %s\n", key.c_str(),
					result && !result->error.empty() ? result->error.c_str() : "worker error");
			}
		}
		if(result && result->milliseconds > 5)
			CUSTOM_LOG("background asset %s: %u ms CPU preparation (not on game thread)\n",
				completed.key.c_str(), result->milliseconds);
		for(std::map<int32, std::set<std::string> >::iterator it = customJobInterests.begin(); it != customJobInterests.end(); ){
			if(it->second.erase(completed.key))
				customCompletedCpuJobs[it->first].insert(completed.key);
			if(it->second.empty()) it = customJobInterests.erase(it);
			else ++it;
		}
	}
}

struct TxdSize { size_t bytes; bool measurable; };
static RwTexture *
CountTxdTexture(RwTexture *texture, void *arg)
{
	TxdSize *size = (TxdSize*)arg;
	RwRaster *ras = RwTextureGetRaster(texture);
	size->bytes += sizeof(RwTexture);
	if(ras == nil)
		return texture;
	int w = RwRasterGetWidth(ras), h = RwRasterGetHeight(ras);
	if(w <= 0 || h <= 0 || w > 8192 || h > 8192){
		size->measurable = false;
		return texture;
	}
	bool mips = (ras->format & rwRASTERFORMATMIPMAP) != 0;
	for(;;){
		size->bytes += (size_t)w * (size_t)h * 4; // conservative RGBA8 estimate
		if(!mips || (w == 1 && h == 1))
			break;
		w = w > 1 ? w / 2 : 1;
		h = h > 1 ? h / 2 : 1;
	}
	return texture;
}

bool
CCustomModels::RetainTxdOnUnload(int32 slot)
{
#ifdef USE_CUSTOM_ALLOCATOR
	// That allocator counts *actual* RW allocations as live streaming memory.
	// Keeping an unloaded TXD there would make MakeSpaceFor free unrelated
	// models without reclaiming the counted TXD. The CPU decode cache still
	// avoids re-parsing .btx in this optional configuration.
	return false;
#else
	std::map<int32, RwTexDictionary*>::iterator it = customLoadedTxds.find(slot);
	TxdDef *def = CTxdStore::GetSlot(slot);
	if(it == customLoadedTxds.end() || def == nil || def->texDict != it->second ||
	   CTxdStore::GetNumRefs(slot) != 0)
		return false; // native, externally replaced or still referenced
	TxdSize estimate = { sizeof(RwTexDictionary), true };
	RwTexDictionaryForAllTextures(def->texDict, CountTxdTexture, &estimate);
	const std::string key = ToLower(def->name);
	if(!estimate.measurable || !customResidentTxds.Store(slot, key, def->texDict, estimate.bytes)){
		customLoadedTxds.erase(slot);
		return false; // caller retains ownership and destroys it normally
	}
	if(RwTexDictionaryGetCurrent() == def->texDict)
		RwTexDictionarySetCurrent(nil);
	def->texDict = nil;
	customLoadedTxds.erase(slot);
	return true;
#endif
}

void
CCustomModels::ForgetTxdSlot(int32 slot)
{
	customLoadedTxds.erase(slot);
	customResidentTxds.Erase(slot);
}

void
CCustomModels::ClearGraphicsCache(void)
{
	customLoadedTxds.clear();
	customResidentTxds.Clear(); // while RenderWare and the GPU context exist
}

static bool
RestoreCachedTxd(int32 slot, const std::string &key)
{
	TxdDef *def = CTxdStore::GetSlot(slot);
	if(def == nil || def->texDict != nil)
		return false;
	RwTexDictionary *dict = customResidentTxds.Take(slot, key);
	if(dict == nil)
		return false;
	def->texDict = dict;
	customLoadedTxds[slot] = dict;
	CUSTOM_LOG("textures %s (slot %d): reused resident GPU dictionary\n", key.c_str(), slot);
	return true;
}

// builds the dictionary of one texture dictionary slot: every texture the
// model references gets an RwTexture built from its .btx
static bool
LoadTxdSlot(int32 slot, const std::string &key, TxdPlan &plan)
{
	if(RestoreCachedTxd(slot, key))
		return true; // no ASTC decode, raster allocation or GPU upload
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

			std::shared_ptr<const brtex::Texture> tex = GetPreparedTexture(plan.modelKey, name, recipe);
			if(!tex){ missing++; continue; }

			RwTexture *texture = BuildTextureGuarded(*tex, name.c_str());
			if(texture == nil){ missing++; continue; }
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}
	}else{
		std::shared_ptr<const brtex::Texture> tex = GetDecodedBtx(key);
		RwTexture *texture = tex ? BuildTextureGuarded(*tex, key.c_str()) : nil;
		if(texture){
			RwTexDictionaryAddTexture(dict, texture);
			made++;
		}else missing++;
	}
	// textures the single .btx files do not have can sit in a whole .txd in
	// an optional whole .txd (not indexed in this layout)
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
	if(made > 0 && key == ToLower(CTxdStore::GetTxdName(slot)))
		customLoadedTxds[slot] = dict;
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
	bool haveCls = customCls.find(key) != customCls.end() &&
	               !HasBrBlocks(customCls.find(key)->second);
	bool haveFile = customColFiles.find(key) != customColFiles.end();
	if(!haveCls && !haveFile)
		return false;

	std::vector<uint8> game;
	bool useCol = false;
	if(haveFile) {
		std::vector<uint8> file;
		if(ReadEntry(customColFiles, key, file))
			game = ColContainerToGame(file, "collision file", key);
		useCol = !game.empty();
	}
	if(!useCol && haveCls) {
		std::vector<uint8> file;
		if(!ReadEntry(customCls, key, file)) return false;
		// Legacy non-CED2 .cls; CED2 blocks are installed directly by name.
		br::ClsStats stats;
		std::vector<uint8> col = br::convertClsToCol(file, stats);
		game = ColContainerToGame(col, "collision .cls", key);
	}
	if(game.empty()) return false;

	bool ok = CColStore::LoadCol(slot, game.data(), (int32)game.size());
	CUSTOM_LOG("collision %s (slot %d): %s, taken from the %s\n",
		key.c_str(), slot, ok ? "loaded" : "FAILED", useCol ? ".col file" : ".cls");
	return ok;
}

// ---------------------------------------------------------------------------
// public interface
// ---------------------------------------------------------------------------

void
CCustomModels::Initialise(void)
{
	EnsureInitialised();
	// CGame calls this after loading the IDE model names. Build the reverse
	// model->TXD index once here, not by scanning every custom model in the
	// hot CanServe/GetNextFileOnCd path. Like FindModelForTxd, prefer .mod
	// over .dff and the first name in lexical order within each kind.
	customTxdCandidates.clear();
	if(customMods.empty() && customDff.empty()) return;
	for(int32 id = 0; id < MODELINFOSIZE; id++){
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
		if(!mi || mi->GetTxdSlot() < 0 || mi->GetTxdSlot() >= TXDSTORESIZE) continue;
		std::string modelKey = ToLower(mi->GetModelName());
		bool isMod = customMods.find(modelKey) != customMods.end();
		if(!isMod && customDff.find(modelKey) == customDff.end()) continue;
		const char *name = CTxdStore::GetTxdName(mi->GetTxdSlot());
		if(!name || !name[0]) continue;
		std::string txdKey = ToLower(name);
		std::map<std::string, std::string>::iterator current = customTxdCandidates.find(txdKey);
		if(current == customTxdCandidates.end() ||
		   (isMod && customMods.find(current->second) == customMods.end()) ||
		   (isMod == (customMods.find(current->second) != customMods.end()) && modelKey < current->second))
			customTxdCandidates[txdKey] = modelKey;
	}
}

void
CCustomModels::SetNativeModelOverride(int32 modelId, bool enabled)
{
	nativeModelOverride = enabled ? modelId : -1;
}

void
CCustomModels::Shutdown(void)
{
	// The worker may still be reading a loose file. Join it BEFORE the
	// source indices and decoded caches are destroyed or a new game is started.
	customCpuWorker.Stop();
	customJobInterests.clear();
	customCompletedCpuJobs.clear();
	customPreparationWaits.clear();
	customSyncFallbackStreams.clear();
	ClearGraphicsCache();
	customDecodedModels.Clear();
	customDecodedTextures.Clear();
	customFailedCpuTextures.clear();
	customTransientTexture.reset();
	customTransientTextureKey.clear();
	customMods.clear();
	customBtx.clear();
	customCls.clear();
	customAnims.clear();
	customDff.clear();
	customTxdFiles.clear();
	customColFiles.clear();
	customColSources.clear();
	customClsSources.clear();
	customClsHasBrBlocks.clear();
	customColBlocks.clear();
	customBrColBlocks.clear();
	customLegacyClsBlocks.clear();
	customIdeFiles.clear();
	customIplFiles.clear();
	customImages.clear();
	customSkinNames.clear();
	customFolderPath.clear();
	customModPlans.clear();
	rejectedArchiveModels.clear();
	customTxdPlans.clear();
	customTxdCandidates.clear();
	customInitialised = false;
	customActive = false;
	ResetImageMounts();
	nativeModelOverride = -1;
}

bool
CCustomModels::IsActive(void)
{
	EnsureInitialised();
	return customActive;
}

bool
CCustomModels::HasWorldFiles(void)
{
	EnsureInitialised();
	return !customIdeFiles.empty() || !customIplFiles.empty();
}

bool
CCustomModels::HasModelFile(const char *name)
{
	EnsureInitialised();
	return name && imageModelTextures.FindDffImage(name) >= 0;
}

// Map packs contain alternative scenes (events, races, old/new vegetation).
// Do not guess which variant the user wants: when present, this optional list
// selects exactly which IPL paths are active. IDEs still all contribute their
// named models so reused source IDs cannot silently replace each other.
static bool
ReadActiveIpls(std::set<std::string> &active)
{
	std::string maps = Subfolder(Subfolder(customFolderPath, "data"), "maps");
	if(maps.empty()) return false;
	std::string filename = maps + "/active_ipl.txt";
	FILE *f = fcaseopen(filename.c_str(), "rb");
	if(f == nil) return false; // no manifest: load all custom/data/maps IPLs
	char line[512];
	unsigned invalid = 0, missing = 0, lineNo = 0;
	while(fgets(line, sizeof(line), f)){
		lineNo++;
		size_t n = strlen(line);
		if(n && line[n-1] != '\n' && !feof(f)){
			int c;
			while((c = fgetc(f)) != '\n' && c != EOF) {}
			invalid++;
			CUSTOM_LOG("active_ipl.txt:%u: line too long\n", lineNo);
			continue;
		}
		std::string text(line, n);
		size_t comment = text.find('#');
		size_t slash = text.find("//");
		if(slash < comment) comment = slash;
		if(comment != std::string::npos) text.resize(comment);
		size_t begin = text.find_first_not_of(" \t\r\n");
		if(begin == std::string::npos) continue;
		text = text.substr(begin, text.find_last_not_of(" \t\r\n")-begin+1);
		std::string key = WorldPath(text);
		if(text[0] == '/' || key.compare(0, 10, "data/maps/") != 0 ||
		   key.size() > 255 || key.find("../") != std::string::npos ||
		   Extension(key) != "ipl"){
			invalid++;
			CUSTOM_LOG("active_ipl.txt:%u: invalid IPL path: %.120s\n", lineNo, text.c_str());
			continue;
		}
		active.insert(key);
		if(customIplFiles.find(key) == customIplFiles.end()) {
			missing++;
			CUSTOM_LOG("active_ipl.txt: IPL path not found: %s\n", key.c_str());
		}
	}
	bool ok = !ferror(f);
	fclose(f);
	if(!ok){
		CUSTOM_LOG("active_ipl.txt: read failed; loading all IPLs instead\n");
		active.clear();
		return false;
	}
	CUSTOM_LOG("custom/data/maps/active_ipl.txt: %u path(s) selected from %u IPLs, %u missing, %u invalid line(s)\n",
		(unsigned)active.size(), (unsigned)customIplFiles.size(), missing, invalid);
	return true;
}

void
CCustomModels::ForEachWorldFile(bool ide, WorldFileCallback callback, void *context)
{
	EnsureInitialised();
	if(callback == nil) return;
	std::set<std::string> active;
	const bool selected = !ide && ReadActiveIpls(active);
	const std::map<std::string, CustomSource> &world = ide ? customIdeFiles : customIplFiles;
	for(std::map<std::string, CustomSource>::const_iterator it = world.begin();
	    it != world.end(); ++it) {
		if(selected && !active.count(it->first)) continue;
		std::vector<uint8> bytes;
		if(!ReadSource(it->second, bytes)) {
			CUSTOM_LOG("world %s: cannot read %s\n", ide ? "IDE" : "IPL", it->first.c_str());
			continue;
		}
		callback(it->first.c_str(), bytes.data(), bytes.size(), context);
	}
}

void
CCustomModels::ForEachBrCollision(CollisionCallback callback, void *context)
{
	EnsureInitialised();
	if(callback == nil) return;
	// Read each loose .cls source once, even if it has multiple model blocks.
	std::map<size_t, std::vector<std::pair<std::string, BrBlockRef> > > groups;
	for(std::map<std::string, BrBlockRef>::const_iterator it = customBrColBlocks.begin();
	    it != customBrColBlocks.end(); ++it)
		groups[it->second.source].push_back(*it);
	for(std::map<size_t, std::vector<std::pair<std::string, BrBlockRef> > >::const_iterator group = groups.begin();
	    group != groups.end(); ++group) {
		std::vector<uint8> bytes;
		if(!ReadSource(customClsSources[group->first], bytes)) continue;
		for(size_t b = 0; b < group->second.size(); b++) {
			const std::pair<std::string, BrBlockRef> &item = group->second[b];
			if(item.second.offset <= bytes.size() &&
			   item.second.size <= bytes.size()-item.second.offset)
				callback(item.first.c_str(), bytes.data()+item.second.offset,
				         item.second.size, context);
		}
	}
}

// A .col may hold hundreds of models; serve each indexed internal name,
// including ones stored in different .col containers with identical names.
// Converted blocks are scoped to one callback, never cached in the LRU.
void
CCustomModels::ForEachGameCollision(CollisionCallback callback, void *context, CollisionFilter filter)
{
	EnsureInitialised();
	if(callback == nil) return;
	// Legacy .cls files are also matched by internal model name. Group
	// selected blocks by loose file and convert each chosen file only once.
	std::map<size_t, std::vector<std::pair<std::string, BrBlockRef> > > clsGroups;
	for(std::map<std::string, BrBlockRef>::const_iterator it = customLegacyClsBlocks.begin();
	    it != customLegacyClsBlocks.end(); ++it)
		if(!filter || filter(it->first.c_str(), context))
			clsGroups[it->second.source].push_back(*it);
	for(std::map<size_t, std::vector<std::pair<std::string, BrBlockRef> > >::const_iterator group = clsGroups.begin();
	    group != clsGroups.end(); ++group) {
		std::vector<uint8> file;
		if(!ReadSource(customClsSources[group->first], file)) continue;
		std::vector<uint8> game = LegacyClsToGame(file);
		for(size_t b = 0; b < group->second.size(); b++) {
			const std::pair<std::string, BrBlockRef> &item = group->second[b];
			const BrBlockRef &ref = item.second;
			if(ref.offset <= game.size() && ref.size <= game.size()-ref.offset &&
			   customcolformat::ValidVC(game.data()+ref.offset, ref.size))
				callback(item.first.c_str(), game.data()+ref.offset, ref.size, context);
		}
	}
	std::map<size_t, std::vector<std::pair<std::string, ColBlockRef> > > groups;
	for(std::map<std::string, ColBlockRef>::const_iterator it = customColBlocks.begin();
	    it != customColBlocks.end(); ++it)
		groups[it->second.source].push_back(*it);
	for(std::map<size_t, std::vector<std::pair<std::string, ColBlockRef> > >::const_iterator group = groups.begin();
	    group != groups.end(); ++group) {
		if(filter) {
			bool wanted = false;
			for(size_t b = 0; b < group->second.size(); b++)
				if(filter(group->second[b].first.c_str(), context)) { wanted = true; break; }
			if(!wanted) continue;
		}
		std::vector<uint8> bytes;
		if(!ReadSource(customColSources[group->first], bytes)) continue;
		for(size_t b = 0; b < group->second.size(); b++) {
			const std::pair<std::string, ColBlockRef> &item = group->second[b];
			if(filter && !filter(item.first.c_str(), context)) continue;
			const ColBlockRef &ref = item.second;
			if(ref.offset > bytes.size() || ref.size > bytes.size()-ref.offset) {
				CUSTOM_LOG("collision %s: block changed/shortened in %s\n",
					item.first.c_str(), customColSources[group->first].fname);
				continue;
			}
			const uint8 *block = bytes.data()+ref.offset;
			if(ref.vcBody) {
				if(customcolformat::ValidVC(block, ref.size))
					callback(item.first.c_str(), block, ref.size, context);
				else CUSTOM_LOG("collision %s: invalid VC COLL block in %s\n",
					item.first.c_str(), customColSources[group->first].fname);
			} else {
				std::vector<uint8> raw(block, block+ref.size);
				customcol::Stats stats;
				std::vector<uint8> game = customcol::ToGameFormat(raw, stats);
				if(customcolformat::ValidVC(game.data(), game.size()))
					callback(item.first.c_str(), game.data(), game.size(), context);
				else CUSTOM_LOG("collision %s: invalid SA COL2/COL3 block in %s (%d rejected)\n",
					item.first.c_str(), customColSources[group->first].fname, stats.bad);
			}
		}
	}
}

static std::string
PotentialModelForTxd(const std::string &key)
{
	if(customMods.find(key) != customMods.end() || customDff.find(key) != customDff.end()){
		std::map<std::string, ModPlan>::const_iterator it = customModPlans.find(key);
		if(it == customModPlans.end() || !it->second.computed || it->second.usable)
			return key;
	}
	std::map<std::string, std::string>::iterator cached = customTxdCandidates.find(key);
	if(cached != customTxdCandidates.end()){
		std::map<std::string, ModPlan>::const_iterator plan = customModPlans.find(cached->second);
		if(cached->second.empty() || plan == customModPlans.end() || !plan->second.computed || plan->second.usable)
			return cached->second;
		customTxdCandidates.erase(cached); // candidate was later rejected
	}
	std::string modelKey;
	FindModelForTxd(key, modelKey);
	customTxdCandidates[key] = modelKey; // including misses: CanServe runs often
	return modelKey;
}

static bool
EvictedBeforeReady(int32 streamId, const std::string &job)
{
	std::map<int32, std::set<std::string> >::const_iterator it = customCompletedCpuJobs.find(streamId);
	if(it == customCompletedCpuJobs.end() || it->second.find(job) == it->second.end())
		return false;
	customSyncFallbackStreams.insert(streamId);
	customWorkerSyncFallbacks++;
	CUSTOM_LOG("background asset %d: %s evicted before its TXD was ready; using synchronous load\n",
		streamId, job.c_str());
	return true;
}

// Called only on the game thread. A TXD's plan needs the model's texture names,
// so first prepare the model, then queue precisely the .btx entries in that
// dictionary. Until this is ready the request stays in the normal streaming
// list, leaving the RW object and the camera-based eviction rules untouched.
static bool
CheckStreamPreparation(int32 streamId, bool priority)
{
	if(streamId == nativeModelOverride || !customActive || streamId < 0 || streamId >= NUMSTREAMINFO)
		return true;
	// Native custom/img/ DFFs and TXDs always go through the stock streamer.
	// The legacy converter cannot override their scheduled IMG reads.
	if(streamId < STREAM_OFFSET_COL && CStreaming::ms_aInfoForModel[streamId].m_size &&
	   CCustomModels::IsAutoMountedImage(_GET_INDEX(CStreaming::ms_aInfoForModel[streamId].m_position)))
		return true;
	if(streamId < STREAM_OFFSET_TXD){
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(streamId);
		if(!mi || (!mi->IsSimple() && mi->GetModelType() != MITYPE_CLUMP &&
		           mi->GetModelType() != MITYPE_VEHICLE && mi->GetModelType() != MITYPE_PED))
			return true;
		std::string key = ToLower(mi->GetModelName());
		if(rejectedArchiveModels.find(key) != rejectedArchiveModels.end() ||
		   (customMods.find(key) == customMods.end() && customDff.find(key) == customDff.end()))
			return true;
		std::map<std::string, ModPlan>::const_iterator plan = customModPlans.find(key);
		if(plan != customModPlans.end() && plan->second.computed && !plan->second.usable)
			return true; // the game's own file is used
		if(customDecodedModels.Contains(key)){
			if(plan == customModPlans.end() || !plan->second.computed)
				CanUseModel(key); // cache hit, no parsing or I/O
			return true;
		}
		if(EvictedBeforeReady(streamId, ModelJobKey(key))) return true;
		QueueModelPreparation(key, streamId, priority);
		return false;
	}
	if(streamId >= STREAM_OFFSET_TXD && streamId < STREAM_OFFSET_COL){
		int32 slot = streamId - STREAM_OFFSET_TXD;
		const char *slotName = CTxdStore::GetTxdName(slot);
		if(!slotName || !slotName[0]) return true;
		TxdDef *active = CTxdStore::GetSlot(slot);
		if(active && active->texDict && ((rw::TexDictionary*)active->texDict)->count() != 0)
			return true; // dictionary already resident, no extra CPU preparation
		std::string key = ToLower(slotName);
		if(customResidentTxds.Contains(slot, key))
			return true; // evicted custom TXD already has its GPU rasters
		std::map<std::string, TxdPlan>::const_iterator known = customTxdPlans.find(key);
		if(known == customTxdPlans.end() || !known->second.computed){
			std::string modelKey = PotentialModelForTxd(key);
			if(modelKey.empty() && customBtx.find(key) == customBtx.end() &&
			   customTxdFiles.find(key) == customTxdFiles.end())
				return true; // game's native TXD, not our path
			if(!modelKey.empty()){
				std::map<std::string, ModPlan>::const_iterator plan = customModPlans.find(modelKey);
				if(plan == customModPlans.end() || !plan->second.computed || plan->second.usable){
					if(!customDecodedModels.Contains(modelKey)){
						if(EvictedBeforeReady(streamId, ModelJobKey(modelKey))) return true;
						QueueModelPreparation(modelKey, streamId, priority);
						return false;
					}
				}
			}
		}
		TxdPlan &plan = GetTxdPlan(key);
		if(!plan.serve)
			return true;
		bool pending = false;
		if(plan.haveModel){
			for(size_t i = 0; i < plan.names.size(); i++){
				const std::string &name = plan.names[i];
				const br::TerrainRecipe *recipe = nil;
				for(size_t r = 0; r < plan.recipes.size(); r++)
					if(plan.recipes[r].tex == name) recipe = &plan.recipes[r];
				if(recipe){
					std::string texKey = "terrain:" + plan.modelKey + ":" + ToLower(name);
					if(!HasPreparedTexture(texKey)){
						if(EvictedBeforeReady(streamId, TextureJobKey(texKey))) return true;
						QueueTerrainPreparation(texKey, *recipe, streamId, priority);
						pending = true;
					}
				}else{
					const CustomSource *src = FindSource(customBtx, ToLower(name));
					if(src && !HasPreparedTexture(BtxCacheKey(*src))){
						std::string texKey = BtxCacheKey(*src);
						if(EvictedBeforeReady(streamId, TextureJobKey(texKey))) return true;
						QueueTexturePreparation(texKey, name, *src, streamId, priority);
						pending = true;
					}
				}
			}
		}else if(plan.haveBtx){
			const CustomSource *src = FindSource(customBtx, key);
			if(src && !HasPreparedTexture(BtxCacheKey(*src))){
				std::string texKey = BtxCacheKey(*src);
				if(EvictedBeforeReady(streamId, TextureJobKey(texKey))) return true;
				QueueTexturePreparation(texKey, key, *src, streamId, priority);
				pending = true;
			}
		}
		return !pending;
	}
	return true;
}

void
CCustomModels::PrefetchStream(int32 streamId, bool priority)
{
#ifdef USE_CUSTOM_ALLOCATOR
	// Its global operator new uses CMemoryHeap, which is neither thread-safe
	// nor sized for concurrent decompression. Preserve the old synchronous
	// loading path with this experimental allocator enabled.
	(void)streamId;
	(void)priority;
#else
	// RequestModel can run before the custom folder is indexed during startup.
	if(!customInitialised || !customActive)
		return;
	PumpBackgroundResults();
	CheckStreamPreparation(streamId, priority);
#endif
}

bool
CCustomModels::IsStreamPrepared(int32 streamId, bool wait)
{
#ifdef USE_CUSTOM_ALLOCATOR
	(void)streamId;
	(void)wait;
	return true; // CMemoryHeap cannot be used safely by a worker thread
#else
	EnsureInitialised();
	if(customSyncFallbackStreams.find(streamId) != customSyncFallbackStreams.end())
		return true;
	for(int attempts = 0; attempts < (wait ? 64 : 1); attempts++){
		PumpBackgroundResults();
		if(CheckStreamPreparation(streamId, wait)){
			customPreparationWaits.erase(streamId);
			customCompletedCpuJobs.erase(streamId);
			return true;
		}
		if(!wait){
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			std::map<int32, std::chrono::steady_clock::time_point>::iterator first = customPreparationWaits.find(streamId);
			if(first == customPreparationWaits.end())
				first = customPreparationWaits.insert(std::make_pair(streamId, now)).first;
			std::chrono::steady_clock::duration elapsed = now - first->second;
			if(elapsed < std::chrono::seconds(3) ||
			   (customCpuWorker.PendingCount() != 0 && elapsed < std::chrono::seconds(30)))
				return false;
			// Never deadlock streaming if the source exceeds the cache budget,
			// is broken, or the worker cannot start. Three seconds is a safety
			// net, not a per-frame timeout: subsequent lookups don't reset it.
			customPreparationWaits.erase(streamId);
			customSyncFallbackStreams.insert(streamId);
			customWorkerSyncFallbacks++;
			CUSTOM_LOG("background asset %d: deferred too long, using synchronous load\n", streamId);
			return true;
		}
		if(customCpuWorker.PendingCount() == 0)
			break; // thread unavailable or queue cannot accept an asset
		customCpuWorker.WaitForResult();
	}
	customSyncFallbackStreams.insert(streamId);
	customWorkerSyncFallbacks++;
	return true; // explicit blocking load: preserve native streaming semantics
#endif
}

void
CCustomModels::CancelStreamPrefetch(int32 streamId)
{
	customPreparationWaits.erase(streamId);
	std::string transientJob = TextureJobKey(customTransientTextureKey);
	std::map<int32, std::set<std::string> >::const_iterator done = customCompletedCpuJobs.find(streamId);
	bool usedTransient = customTransientTexture && done != customCompletedCpuJobs.end() &&
	                     done->second.find(transientJob) != done->second.end();
	customCompletedCpuJobs.erase(streamId);
	if(usedTransient){
		bool stillWanted = false;
		for(std::map<int32, std::set<std::string> >::const_iterator other = customCompletedCpuJobs.begin();
		    other != customCompletedCpuJobs.end(); ++other)
			if(other->second.find(transientJob) != other->second.end()){ stillWanted = true; break; }
		if(!stillWanted){ customTransientTexture.reset(); customTransientTextureKey.clear(); }
	}
	customSyncFallbackStreams.erase(streamId);
	std::map<int32, std::set<std::string> >::iterator it = customJobInterests.find(streamId);
	if(it == customJobInterests.end())
		return;
	std::set<std::string> jobs = it->second;
	customJobInterests.erase(it);
	for(std::set<std::string>::const_iterator job = jobs.begin(); job != jobs.end(); ++job){
		bool shared = false;
		for(std::map<int32, std::set<std::string> >::const_iterator other = customJobInterests.begin(); other != customJobInterests.end(); ++other)
			if(other->second.find(*job) != other->second.end()) { shared = true; break; }
		if(!shared) customCpuWorker.Cancel(*job);
	}
}

// IMG models and TXDs always use the native streamer. Only a loose collision
// container referenced by a COLFILE directive needs a synthetic CD position;
// per-model world collisions are installed separately by CCustomWorld.
bool
CCustomModels::CanServe(int32 streamId)
{
	if(streamId < STREAM_OFFSET_COL || streamId >= STREAM_OFFSET_ANIM || !IsActive())
		return false;
	const char *name = CColStore::GetColName(streamId - STREAM_OFFSET_COL);
	if(name == nil || name[0] == '\0') return false;
	const std::string key = ToLower(name);
	std::map<std::string, CustomSource>::const_iterator cls = customCls.find(key);
	return (cls != customCls.end() && !HasBrBlocks(cls->second)) ||
	       customColFiles.find(key) != customColFiles.end();
}

bool
CCustomModels::Load(int32 streamId)
{
	return CanServe(streamId) && LoadColSlot(streamId - STREAM_OFFSET_COL);
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
			std::shared_ptr<const brtex::Texture> tex = GetPreparedTexture(plan.modelKey, name, recipe);
			if(!tex){ missing++; continue; }
			RwTexture *texture = BuildTextureGuarded(*tex, name.c_str());
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

// The native collision reader takes [name, bounds, geometry] without a size.
// Validate loose blocks first. A .col block wins over a .cls of the same model.
bool
CCustomModels::GetCollisionBlock(const char *modelname, std::vector<uint8> &out)
{
	EnsureInitialised();
	if(!customActive || modelname == nil) return false;
	std::string key = ToLower(modelname);
	customcol::Stats stats;

	std::map<std::string, ColBlockRef>::const_iterator cb = customColBlocks.find(key);
	if(cb != customColBlocks.end()) {
		const ColBlockRef &ref = cb->second;
		std::vector<uint8> file;
		if(ReadSource(customColSources[ref.source], file) && ref.offset <= file.size() &&
		   ref.size <= file.size()-ref.offset) {
			const uint8 *block = file.data()+ref.offset;
			std::vector<uint8> game;
			if(ref.vcBody) game.assign(block, block+ref.size);
			else {
				std::vector<uint8> raw(block, block+ref.size);
				game = customcol::ToGameFormat(raw, stats);
			}
			if(customcolformat::ValidVC(game.data(), game.size())) {
				out.assign(game.begin()+8, game.end());
				CUSTOM_LOG("collision %s: .col from %s (%u bytes)\n",
					modelname, customColSources[ref.source].fname, (unsigned)out.size());
				return true;
			}
		}
		CUSTOM_LOG("collision %s: invalid .col block, trying .cls\n", modelname);
	}

	std::map<std::string, BrBlockRef>::const_iterator brref = customBrColBlocks.find(key);
	if(brref != customBrColBlocks.end()) {
		const BrBlockRef &ref = brref->second;
		std::vector<uint8> bytes, game;
		if(ReadSource(customClsSources[ref.source], bytes) && ref.offset <= bytes.size() &&
		   ref.size <= bytes.size()-ref.offset &&
		   brcollision::Convert(bytes.data()+ref.offset, ref.size, game) &&
		   customcolformat::ValidVC(game.data(), game.size())) {
			out.assign(game.begin()+8, game.end());
			return true;
		}
	}

	std::map<std::string, BrBlockRef>::const_iterator legacy = customLegacyClsBlocks.find(key);
	if(legacy != customLegacyClsBlocks.end()) {
		const BrBlockRef &ref = legacy->second;
		std::vector<uint8> bytes;
		if(ReadSource(customClsSources[ref.source], bytes)) {
			std::vector<uint8> game = LegacyClsToGame(bytes);
			if(ref.offset <= game.size() && ref.size <= game.size()-ref.offset &&
			   customcolformat::ValidVC(game.data()+ref.offset, ref.size)) {
				out.assign(game.begin()+ref.offset+8, game.begin()+ref.offset+ref.size);
				return true;
			}
		}
	}
	return false;
}

// Animation dictionaries are requested by path ("ANIM\\PED.IFP"). Only
// loose <name>.ifp files inside custom/anim/ are indexed; the animation
// manager validates the format and reserves native ANPK for VC skeletons.
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

	const CustomSource *src = FindSource(customAnims, key);
	if(src == nil || !ReadSource(*src, out)){
		CUSTOM_LOG("animation %s: custom file could not be read, using game file\n", filename);
		return false;
	}
	const char *source = src->loose;
	if(out.size() < 12){
		CUSTOM_LOG("animation %s: %s in %s is too small to be an IFP\n",
			filename, src->fname, source);
		out.clear();
		return false;
	}
	CUSTOM_LOG("animation %s: read %s from %s (%u bytes)\n",
		filename, src->fname, source, (unsigned)out.size());
	return true;
}

void
CCustomModels::PrintStats(void)
{
	EnsureInitialised();
	CUSTOM_LOG("--- stats ---\n");
	CUSTOM_LOG("custom/img: %zu mounted; loose resources: %zu .cls (%zu CED2 blocks), %zu .col (%zu model blocks), %zu .ifp, %zu IDE, %zu IPL\n",
		customImageSlots.size(), customClsSources.size(), customBrColBlocks.size(),
		customColSources.size(), customColBlocks.size(), customAnims.size(),
		customIdeFiles.size(), customIplFiles.size());
	CUSTOM_LOG("asset caches: models %zu hit / %zu miss / %zu evicted (%zu KiB retained), decoded btx %zu / %zu / %zu (%zu KiB), resident TXDs %zu / %zu / %zu (%zu KiB)\n",
		customDecodedModels.Hits(), customDecodedModels.Misses(), customDecodedModels.Evictions(), customDecodedModels.Bytes() / 1024,
		customDecodedTextures.Hits(), customDecodedTextures.Misses(), customDecodedTextures.Evictions(), customDecodedTextures.Bytes() / 1024,
		customResidentTxds.Hits(), customResidentTxds.Misses(), customResidentTxds.Evictions(), customResidentTxds.Bytes() / 1024);
	CUSTOM_LOG("background CPU: %zu models, %zu textures, %zu failed, %zu synchronous fallbacks; %zu pending; cache ceilings %zu / %zu / %zu MiB (model / texture / GPU TXD)\n",
		customWorkerModels, customWorkerTextures, customWorkerFailures, customWorkerSyncFallbacks,
		customCpuWorker.PendingCount(), CUSTOM_MODEL_CACHE_BYTES / (1024u*1024u),
		CUSTOM_TEXTURE_CACHE_BYTES / (1024u*1024u), CUSTOM_GPU_TXD_CACHE_BYTES / (1024u*1024u));
}

// ---------------------------------------------------------------------------
// optional loose lighting table and skins
// ---------------------------------------------------------------------------

bool
CCustomModels::ReadDataFile(const char *relpath, std::vector<uint8> &out,
                             bool *fromCommon)
{
	if(fromCommon) *fromCommon = false;
	out.clear();
	if(relpath == nil || strcmp(relpath, "data/timecyc.json") != 0)
		return false; // no arbitrary paths, ZIPs, or game-root common/ data
	EnsureInitialised();
	const std::string data = Subfolder(customFolderPath, "data");
	if(data.empty()) return false;
	const std::string path = data + "/timecyc.json";
	if(path.size() >= sizeof(CustomSource::loose)) return false;
	CustomSource src = MakeSource(FileTime(path.c_str()), path.c_str(), relpath);
	if(!ReadSource(src, out)) return false;
	CUSTOM_LOG("data file %s: read from %s (%u bytes)\n", relpath, path.c_str(), (unsigned)out.size());
	return true;
}

bool
CCustomModels::HasSkin(const char *skinname)
{
	EnsureInitialised();
	if(!customActive || skinname == nil || skinname[0] == '\0')
		return false;
	return customImages.find(ToLower(Stem(skinname))) != customImages.end();
}

// The bytes of one player skin image from the separate custom/skins/ folder.
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

	std::shared_ptr<const brtex::Texture> tex = GetDecodedBtx(*src, key);
	if(!tex)
		return nil;
	RwTexture *texture = BuildTextureGuarded(*tex, key.c_str());
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
