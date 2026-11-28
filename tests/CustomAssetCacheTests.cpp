// Regression tests for the two ownership modes of custom asset caching:
// immutable decoded bytes/pixels and detached GPU dictionaries. No RW or game
// resources are needed; actual .btx parsing and the .mod XTEA primitive are
// exercised using tiny synthetic inputs.
#include "../tools/custom-folder-verification/shim/common.h"
#include "DecodedAssetCache.h"
#include "ResidentObjectCache.h"
#include "brtex.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

static void Check(bool ok, const char *why)
{
	if(!ok){ fprintf(stderr, "custom asset cache: %s\n", why); abort(); }
}

static void U32(std::vector<uint8_t> &bytes, size_t pos, uint32_t value)
{
	memcpy(&bytes[pos], &value, 4);
}

static std::vector<uint8_t> RawBtx(void)
{
	std::vector<uint8_t> file(4 + 64 + 4 + 16, 0);
	static const uint8_t magic[12] = {0xAB,'K','T','X',' ','1','1',0xBB,'\r','\n',0x1A,'\n'};
	memcpy(file.data() + 4, magic, sizeof(magic));
	U32(file, 4 + 12, 0x04030201); // KTX little-endian
	U32(file, 4 + 16, 0x1401); // GL_UNSIGNED_BYTE
	U32(file, 4 + 20, 1);      // glTypeSize
	U32(file, 4 + 24, 0x1908); // GL_RGBA
	U32(file, 4 + 28, 0x8058); // RGBA8
	U32(file, 4 + 32, 0x1908);
	U32(file, 4 + 36, 2); U32(file, 4 + 40, 2);
	U32(file, 4 + 52, 1);      // faces
	U32(file, 4 + 56, 1);      // mip levels
	U32(file, 4 + 64, 16);     // image size
	for(int i = 0; i < 16; i++) file[4 + 68 + i] = (uint8_t)(i * 11);
	return file;
}

struct FakeDict { int id; explicit FakeDict(int value) : id(value) {} };
static int destroyed = 0;
static void DestroyDict(FakeDict *dict) { ++destroyed; delete dict; }

int main(void)
{
	customcache::DecodedAssetCache<std::vector<uint8_t> > models(16);
	int decrypts = 0;
	const uint32_t key[4] = {1, 2, 3, 4};
	auto prepare = [&models, &decrypts, &key](const char *name){
		std::shared_ptr<const std::vector<uint8_t> > result = models.Find(name);
		if(result) return result;
		++decrypts;
		std::shared_ptr<std::vector<uint8_t> > data = std::make_shared<std::vector<uint8_t> >(8, 0x72);
		br::tea_decrypt(data->data(), (uint32_t)data->size(), key);
		models.Insert(name, data, data->size());
		return std::shared_ptr<const std::vector<uint8_t> >(data);
	};
	auto a = prepare("glendale.mod");
	Check(models.Contains("glendale.mod") && !models.Contains("absent.mod") &&
	      models.Hits() == 0, "readiness lookup does not perturb cache statistics or LRU");
	Check(prepare("glendale.mod") == a && decrypts == 1, ".mod decoded once on a cache hit");
	std::vector<uint8_t> copy = *a;
	prepare("other.mod");
	prepare("third.mod"); // 8+8+8 exceeds budget: glendale is least recently used
	Check(models.Bytes() == 16 && !models.Find("glendale.mod"), "LRU respects model byte budget");
	Check(*a == copy, "in-flight RenderWare stream stays alive across eviction");
	prepare("glendale.mod");
	Check(decrypts == 4 && models.Evictions() >= 2, "an evicted entry must be prepared again");
	models.Clear();
	Check(models.Bytes() == 0 && models.Count() == 0 && *a == copy,
	      "shutdown clears cache without invalidating a borrowed model");

	customcache::DecodedAssetCache<brtex::Texture> textures(1024);
	std::vector<uint8_t> file = RawBtx();
	int parses = 0;
	auto decode = [&textures, &file, &parses](const char *source, const char *name){
		std::shared_ptr<const brtex::Texture> result = textures.Find(source);
		if(result) return result;
		++parses;
		std::shared_ptr<brtex::Texture> value = std::make_shared<brtex::Texture>();
		value->name = name;
		std::string error;
		Check(brtex::parseBtx(file.data(), file.size(), *value, &error), "synthetic KTX RGBA .btx parses");
		textures.Insert(source, value, sizeof(brtex::Texture) + value->mips[0].rgba.size());
		return std::shared_ptr<const brtex::Texture>(value);
	};
	auto tex = decode("archive:1:7:", "very_long_texture_name");
	Check(decode("archive:1:7:", "very_long_texture") == tex && parses == 1 &&
	      tex->mips[0].rgba.size() == 16 && tex->mips[0].rgba[2] == file[74],
	      ".btx decoded once and reused by its archive entry, including aliases");
	textures.Clear();
	Check(!textures.Find("archive:1:7:"), "decoded pixels released on shutdown");

	customcache::ResidentObjectCache<FakeDict> gpu(24, DestroyDict);
	Check(gpu.Store(10, "cars", new FakeDict(10), 12), "store first GPU dictionary");
	Check(gpu.Store(11, "peds", new FakeDict(11), 12), "store second GPU dictionary");
	Check(gpu.Contains(10, "cars") && !gpu.Contains(10, "peds") && gpu.Hits() == 0,
	      "GPU readiness check does not transfer ownership");
	FakeDict *restored = gpu.Take(10, "cars");
	Check(restored && restored->id == 10 && gpu.Bytes() == 12 && destroyed == 0,
	      "restoring a TXD transfers ownership without destroying GPU resources");
	DestroyDict(restored); // CTxdStore now owns it
	Check(gpu.Store(12, "shrub", new FakeDict(12), 20) && gpu.Bytes() == 20 &&
	      destroyed == 2, "GPU LRU frees old dictionaries when full");
	Check(!gpu.Take(12, "another-slot-name") && gpu.Bytes() == 0 && destroyed == 3,
	      "recycled slot never restores a stale dictionary");
	FakeDict *tooLarge = new FakeDict(13);
	Check(!gpu.Store(13, "oversize", tooLarge, 25) && destroyed == 3,
	      "oversized dictionary stays owned by the caller");
	DestroyDict(tooLarge);
	Check(gpu.Store(14, "small", new FakeDict(14), 10), "store before slot recycling");
	gpu.Erase(14); // CTxdStore::RemoveTxdSlot explicitly invalidates an old slot
	Check(!gpu.Take(14, "small") && destroyed == 5, "slot removal invalidates cached dictionary");
	Check(gpu.Store(14, "replacement", new FakeDict(15), 10), "store before graphics shutdown");
	gpu.Clear();
	Check(gpu.Bytes() == 0 && destroyed == 6, "all detached GPU resources freed at shutdown");
	puts("Custom .mod/.btx decoded cache and GPU TXD ownership OK");
	return 0;
}
