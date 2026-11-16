// Share the synthetic BPC/ASTC fixtures and headless RenderWare stubs, but
// compile the real CustomModels.cpp WITHOUT ANDROID (including its CPU path).
#define REVC_DESKTOP_BPC_TEST
#include "AndroidBpcArchiveIntegrationTests.cpp"

static std::vector<uint8> Chunk(uint32 type, const std::vector<uint8> &payload)
{
	std::vector<uint8> out;
	PutU32(out, type); PutU32(out, (uint32)payload.size()); PutU32(out, 0x1803FFFFu);
	out.insert(out.end(), payload.begin(), payload.end());
	return out;
}
static void Append(std::vector<uint8> &out, const std::vector<uint8> &part)
{
	out.insert(out.end(), part.begin(), part.end());
}
static std::vector<uint8> TexturedTree(const uint8 *data, size_t size, const char *name)
{
	std::vector<uint8> out;
	for(size_t off = 0; off + 12 <= size; ){
		uint32 type = br::rd32(data + off), len = br::rd32(data + off + 4);
		Check(len <= size - off - 12, "valid fixture chunk");
		const uint8 *p = data + off + 12;
		std::vector<uint8> payload(p, p + len);
		if(type == 0x10 || type == 0x1A || type == 0x0F || type == 0x08 || type == 0x0E || type == 0x14)
			payload = TexturedTree(p, len, name);
		if(type == 0x07){
			Check(payload.size() >= 40, "material struct");
			W32At(payload, 12 + 12, 1); // material.textured
			std::vector<uint8> tex, flags;
			PutU32(flags, 0x1102);
			Append(tex, Chunk(1, flags));
			std::vector<uint8> text(name, name + std::strlen(name) + 1);
			while(text.size() % 4) text.push_back(0);
			Append(tex, Chunk(2, text));
			Append(tex, Chunk(2, std::vector<uint8>(4, 0)));
			Append(tex, Chunk(3, {}));
			// Material extension is empty; insert the texture before it.
			std::vector<uint8> texture = Chunk(6, tex);
			payload.insert(payload.begin() + 40, texture.begin(), texture.end());
		}
		Append(out, Chunk(type, payload));
		off += 12 + len;
	}
	return out;
}
static std::vector<uint8> TexturedMod(const char *name)
{
	std::vector<uint8> dff = br::buildDummyDff();
	return EncryptDffToMod(TexturedTree(dff.data(), dff.size(), name));
}
int main()
{
	char root[] = "/tmp/revc_desktop_tex_XXXXXX";
	Check(mkdtemp(root) != nullptr, "temporary directory");
	char old[4096]; Check(getcwd(old, sizeof(old)) && chdir(root) == 0, "cwd");
	mkdir("custom", 0755); mkdir("custom/zip", 0755);
	mkdir("custom/zip/mesh", 0755); mkdir("custom/zip/textures", 0755);
	WriteFileBytes("custom/zip/common.bpc", MakeZip({
		{ "data/config.dat", Bytes("CDIMAGE mesh/world.bpc\n") },
		{ "data/texdb.dat", Bytes("TEXDB textures/roads\n") }
	}, true));
	WriteFileBytes("custom/zip/mesh/world.bpc", MakeZip({
		{ "road_br_01.mod", TexturedMod("road_asphalt_br") },
		{ "road_br_02.mod", TexturedMod("other_btx") }
	}, false));
	WriteFileBytes("custom/zip/textures/roads.astc.bpc", MakeZip({
		{ "road_asphalt_br.btx", MakeAstcBtx() }, { "other_btx.btx", MakeAstcBtx() }
	}, false));
	CTxdStore::Initialise();
	int slot = CTxdStore::AddTxdSlot("shared_native");
	CTxdStore::Create(slot);
	for(int id = 100; id <= 101; ++id){
		CBaseModelInfo *mi = CModelInfo::AddSimpleModel(id);
		mi->SetModelName(id == 100 ? "road_br_01" : "road_br_02");
		mi->SetTexDictionary("shared_native");
	}
	CCustomModels::Initialise();
	// Native TXD is not replaced, but each custom model must contribute its own
	// textures even when that shared dictionary is already nonempty.
	CStreaming::ms_aInfoForModel[STREAM_OFFSET_TXD + slot].m_size = 1;
	Check(!CCustomModels::CanServe(STREAM_OFFSET_TXD + slot), "preserve native TXD streamer");
	CTxdStore::SetCurrentTxd(slot);
	for(int id = 100; id <= 101; ++id){
		Check(CCustomModels::Load(id), "per-model ASTC texture resolved before reading materials");
		Check(g_lastResolvedTex && g_lastResolvedTex->raster, "real CPU decoded raster");
	}
	Check(g_liveRwTextures == 2, "both models' textures in the same TXD");
	Check(CCustomModels::Load(101) && g_liveRwTextures == 2, "no duplicate GPU uploads on reload");
	int absent = CTxdStore::AddTxdSlot("no_custom_model");
	for(int i = 0; i < 8; ++i)
		Check(!CCustomModels::CanServe(STREAM_OFFSET_TXD + absent), "native/missing TXD fallback");
	Check(g_linearGetModelInfoByNameCalls == 0, "no nested linear model-name lookup on TXD misses");
	CTxdStore::RemoveTxd(slot);
	CCustomModels::Shutdown(); CModelInfo::ShutDown(); CTxdStore::Shutdown();
	Check(g_liveRwTextures == 0, "no texture ownership leaks");
	Check(chdir(old) == 0, "restore cwd");
	std::string cmd = std::string("rm -rf ") + root;
	std::system(cmd.c_str());
	std::puts("DesktopBpcTexturesTests: ALL PASSED");
}
