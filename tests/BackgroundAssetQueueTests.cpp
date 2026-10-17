#include "../tools/custom-folder-verification/shim/common.h"
#include "BackgroundAssetQueue.h"
#include "CustomZip.h"
#include "brtex.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>
#include <thread>

static void Check(bool yes, const char *why)
{
	if(!yes){ fprintf(stderr, "background asset queue: %s\n", why); abort(); }
}

static void U32(std::vector<uint8_t> &b, size_t offset, uint32_t v)
{
	memcpy(&b[offset], &v, sizeof(v));
}

// An actual unencrypted .mod container and a tiny RGBA .btx. The worker
// exercises the same format conversions as the game, but never calls RW.
static std::vector<uint8_t> SampleMod(void)
{
	std::vector<uint8_t> bytes(br::MOD_HDR + 24, 0);
	U32(bytes, 0, br::MOD_MAGIC);
	U32(bytes, 4, 24);
	U32(bytes, br::MOD_HDR, br::rwCLUMP);
	U32(bytes, br::MOD_HDR + 4, 12);
	U32(bytes, br::MOD_HDR + 8, br::SA_LIBID);
	U32(bytes, br::MOD_HDR + 12, br::rwSTRUCT);
	U32(bytes, br::MOD_HDR + 16, 0);
	U32(bytes, br::MOD_HDR + 20, br::SA_LIBID);
	return bytes;
}

static std::vector<uint8_t> SampleBtx(void)
{
	std::vector<uint8_t> bytes(4 + 64 + 4 + 16, 0);
	static const uint8_t magic[12] = {0xAB,'K','T','X',' ','1','1',0xBB,'\r','\n',0x1A,'\n'};
	memcpy(bytes.data() + 4, magic, sizeof(magic));
	U32(bytes, 4 + 12, 0x04030201); // little endian
	U32(bytes, 4 + 16, 0x1401); // unsigned byte RGBA8
	U32(bytes, 4 + 24, 0x1908);
	U32(bytes, 4 + 28, 0x8058);
	U32(bytes, 4 + 32, 0x1908);
	U32(bytes, 4 + 36, 2); U32(bytes, 4 + 40, 2);
	U32(bytes, 4 + 52, 1); U32(bytes, 4 + 56, 1);
	U32(bytes, 4 + 64, 16);
	for(int i = 0; i < 16; i++) bytes[4 + 68 + i] = (uint8_t)(i * 11);
	return bytes;
}

// Two stored entries in a minimal ZIP. This exercises CustomZip::ReadAt from
// the worker and the main thread using the same immutable archive index.
static void Append16(std::vector<uint8_t> &b, uint16_t value)
{
	b.push_back((uint8_t)value); b.push_back((uint8_t)(value >> 8));
}
static void Append32(std::vector<uint8_t> &b, uint32_t value)
{
	Append16(b, (uint16_t)value); Append16(b, (uint16_t)(value >> 16));
}
static std::vector<uint8_t> StoredZip(const std::vector<uint8_t> &mod, const std::vector<uint8_t> &btx)
{
	const char *names[2] = { "small.mod", "small.btx" };
	const std::vector<uint8_t> *data[2] = { &mod, &btx };
	uint32_t offsets[2];
	std::vector<uint8_t> bytes;
	for(int i = 0; i < 2; i++){
		offsets[i] = (uint32_t)bytes.size();
		Append32(bytes, 0x04034b50); // local header
		Append16(bytes, 20); Append16(bytes, 0); Append16(bytes, 0); // version, flags, stored
		Append32(bytes, 0); Append32(bytes, 0); // time/date, CRC (reader doesn't use it)
		Append32(bytes, (uint32_t)data[i]->size()); Append32(bytes, (uint32_t)data[i]->size());
		Append16(bytes, (uint16_t)strlen(names[i])); Append16(bytes, 0);
		bytes.insert(bytes.end(), names[i], names[i] + strlen(names[i]));
		bytes.insert(bytes.end(), data[i]->begin(), data[i]->end());
	}
	uint32_t cdOffset = (uint32_t)bytes.size();
	for(int i = 0; i < 2; i++){
		Append32(bytes, 0x02014b50); // central-directory header
		Append16(bytes, 20); Append16(bytes, 20); Append16(bytes, 0); Append16(bytes, 0);
		Append32(bytes, 0); Append32(bytes, 0);
		Append32(bytes, (uint32_t)data[i]->size()); Append32(bytes, (uint32_t)data[i]->size());
		Append16(bytes, (uint16_t)strlen(names[i]));
		Append16(bytes, 0); Append16(bytes, 0); Append16(bytes, 0); Append16(bytes, 0);
		Append32(bytes, 0); Append32(bytes, offsets[i]);
		bytes.insert(bytes.end(), names[i], names[i] + strlen(names[i]));
	}
	uint32_t cdSize = (uint32_t)bytes.size() - cdOffset;
	Append32(bytes, 0x06054b50); // EOCD
	Append16(bytes, 0); Append16(bytes, 0);
	Append16(bytes, 2); Append16(bytes, 2);
	Append32(bytes, cdSize); Append32(bytes, cdOffset); Append16(bytes, 0);
	return bytes;
}

struct PreparedAsset {
	std::vector<uint8_t> bytes;
	brtex::Texture texture;
};

int main(void)
{
	customcache::BackgroundAssetQueue<int> queue(3, 1);
	const std::thread::id mainThread = std::this_thread::get_id();
	std::promise<void> started, release;
	std::shared_future<void> gate = release.get_future().share();
	std::atomic<int> ran(0);
	Check(queue.Schedule("slow", [&]{
		Check(std::this_thread::get_id() != mainThread, "preparation is off the game thread");
		started.set_value(); gate.wait(); ++ran;
		return std::make_shared<int>(42);
	}), "queue first task");
	started.get_future().wait();
	Check(queue.Schedule("slow", [&]{ ++ran; return std::make_shared<int>(99); }),
	      "duplicate prefetch is coalesced");
	Check(queue.Schedule("discard", [&]{ ++ran; return std::make_shared<int>(1); }), "queue cancellable task");
	queue.Cancel("discard");
	Check(!queue.IsPending("discard"), "cancellation removes queued task");
	Check(queue.Schedule("priority", [&]{ ++ran; return std::make_shared<int>(7); }, true),
	      "priority task can use freed capacity");
	Check(queue.Schedule("normal", [&]{ ++ran; return std::make_shared<int>(8); }),
	      "queue fills its last slot");
	Check(!queue.Schedule("overflow", [&]{ ++ran; return std::make_shared<int>(9); }),
	      "bounded queue refuses extra prefetch without allocating it");
	Check(queue.Schedule("normal", [&]{ ++ran; return std::make_shared<int>(99); }, true),
	      "duplicate request promotes an existing job without another decode");
	release.set_value();

	customcache::BackgroundAssetQueue<int>::Completed result;
	queue.WaitForResult();
	Check(queue.Pop(result) && result.key == "slow" && result.value && *result.value == 42,
	      "read prepared bytes on the game thread");
	queue.WaitForResult();
	Check(queue.Pop(result) && result.key == "priority" && result.value && *result.value == 7,
	      "first high-priority request finishes before the later promotion");
	queue.WaitForResult();
	Check(queue.Pop(result) && result.key == "normal" && result.value && *result.value == 8,
	      "promoted request finishes without running its duplicate task");
	Check(ran == 3 && queue.PendingCount() == 0, "canceled/duplicate tasks were never run");

	std::promise<void> restarted, release2;
	std::shared_future<void> gate2 = release2.get_future().share();
	Check(queue.Schedule("old", [&]{ restarted.set_value(); gate2.wait(); return std::make_shared<int>(1); }),
	      "start a task that will be canceled in flight");
	restarted.get_future().wait();
	queue.Cancel("old");
	Check(queue.Schedule("old", []{ return std::make_shared<int>(2); }), "new request uses a new ticket");
	release2.set_value();
	queue.WaitForResult();
	Check(queue.Pop(result) && result.key == "old" && result.value && *result.value == 2,
	      "stale result does not replace a newly requested resource");
	queue.Stop();
	Check(queue.PendingCount() == 0, "shutdown joins worker and drops pending assets");
	Check(queue.Schedule("after-restart", []{ return std::make_shared<int>(3); }), "worker can restart after game reload");
	queue.WaitForResult();
	Check(queue.Pop(result) && result.value && *result.value == 3, "restart returned correct bytes");
	queue.Stop();

	const std::vector<uint8_t> mod = SampleMod(), btx = SampleBtx();
	const char *filename = "background_asset_queue_fixture.zip";
	std::vector<uint8_t> zipBytes = StoredZip(mod, btx);
	FILE *zipFile = fopen(filename, "wb");
	Check(zipFile != nullptr, "create synthetic ZIP archive");
	Check(fwrite(zipBytes.data(), 1, zipBytes.size(), zipFile) == zipBytes.size(), "write archive");
	fclose(zipFile);
	CustomZip archive;
	Check(archive.Open(filename) && archive.GetNumEntries() == 2, "index one ZIP on game thread");

	std::promise<void> readingStarted, allowWorker;
	std::shared_future<void> workGate = allowWorker.get_future().share();
	customcache::BackgroundAssetQueue<PreparedAsset> pipeline(2, 1);
	Check(pipeline.Schedule("mod+btx", [&archive, &readingStarted, workGate, mainThread]{
		Check(std::this_thread::get_id() != mainThread, "real format decoding runs on worker");
		readingStarted.set_value(); workGate.wait();
		std::shared_ptr<PreparedAsset> asset = std::make_shared<PreparedAsset>();
		std::vector<uint8_t> compressedTexture(archive.GetEntry(1)->uncompressedSize);
		for(int i = 0; i < 100; i++){
			asset->bytes.resize(archive.GetEntry(0)->uncompressedSize);
			Check(archive.Extract(0, asset->bytes.data(), (uint32)asset->bytes.size()), "worker ZIP extraction");
			Check(archive.Extract(1, compressedTexture.data(), (uint32)compressedTexture.size()),
			      "worker extracts texture from same archive");
		}
		br::RwFixStats stats;
		std::string error;
		Check(br::understandModInplace(asset->bytes, stats, error), "worker understood .mod in place");
		Check(brtex::parseBtx(compressedTexture.data(), compressedTexture.size(), asset->texture, &error),
		      "worker decoded .btx");
		return asset;
	}), "queue actual .mod and .btx CPU work");
	readingStarted.get_future().wait();
	allowWorker.set_value();
	std::vector<uint8_t> mainRead(btx.size());
	for(int i = 0; i < 100; i++)
		Check(archive.Extract(1, mainRead.data(), (uint32)mainRead.size()) && mainRead == btx,
		      "main-thread ZIP extraction does not race the worker");
	pipeline.WaitForResult();
	customcache::BackgroundAssetQueue<PreparedAsset>::Completed prepared;
	Check(pipeline.Pop(prepared) && prepared.value && prepared.value->bytes.size() == 24 &&
	      br::rd32(prepared.value->bytes.data()) == br::rwCLUMP &&
	      prepared.value->texture.mips.size() == 1 &&
	      prepared.value->texture.mips[0].rgba.size() == 16,
	      "game thread receives repaired model bytes and decoded pixels");
	pipeline.Stop(); // join before freeing the archive index
	archive.Close();
	Check(remove(filename) == 0, "clean up test archive");
	puts("Background custom asset queue, real format decoding and cancellation OK");
}
