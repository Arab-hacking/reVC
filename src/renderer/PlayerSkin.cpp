#include "common.h"

#include "main.h"
#include "PlayerSkin.h"
#include "TxdStore.h"
#include "rtbmp.h"
#include "ClumpModelInfo.h"
#include "VisibilityPlugins.h"
#include "World.h"
#include "PlayerInfo.h"
#include "CdStream.h"
#include "FileMgr.h"
#include "Directory.h"
#include "RwHelper.h"
#include "Timer.h"
#include "Lights.h"
#include "MemoryMgr.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "DebugLog.h"
#include "SavehDiag.h"
#endif

RpClump *gpPlayerClump;
float gOldFov;

int CPlayerSkin::m_txdSlot;

void
FindPlayerDff(uint32 &offset, uint32 &size)
{
	int file;
	CDirectory::DirectoryInfo info;

	file = CFileMgr::OpenFile("models\\gta3.dir", "rb");

	do {
		if (!CFileMgr::Read(file, (char*)&info, sizeof(CDirectory::DirectoryInfo)))
			return;
	} while (strcasecmp("player.dff", info.name) != 0);

	offset = info.offset;
	size = info.size;
}

void
LoadPlayerDff(void)
{
	RwStream *stream;
	RwMemory mem;
	uint32 offset, size;
	uint8 *buffer;
	bool streamWasAdded = false;

	if (CdStreamGetNumImages() == 0) {
		CdStreamAddImage("models\\gta3.img");
		streamWasAdded = true;
	}

	FindPlayerDff(offset, size);
	buffer = (uint8*)RwMallocAlign(size << 11, 2048);
	CdStreamRead(0, buffer, offset, size);
	CdStreamSync(0);

	mem.start = buffer;
	mem.length = size << 11;
	stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);

	if (RwStreamFindChunk(stream, rwID_CLUMP, nil, nil))
		gpPlayerClump = RpClumpStreamRead(stream);

	RwStreamClose(stream, &mem);
	RwFreeAlign(buffer);

	if (streamWasAdded)
		CdStreamRemoveImages();
}

void
CPlayerSkin::Initialise(void)
{
	// empty on PS2
	m_txdSlot = CTxdStore::AddTxdSlot("skin");
	CTxdStore::Create(m_txdSlot);
	CTxdStore::AddRef(m_txdSlot);
}

void
CPlayerSkin::Shutdown(void)
{
	// empty on PS2
	CTxdStore::RemoveTxdSlot(m_txdSlot);
}

#ifdef CUSTOM_MODELS
// One uncompressed 24/32 bpp BMP out of memory (the player skins the custom
// folder serves). Returns a texture added to the skin dictionary, or nil.
static RwTexture *
MakeSkinTextureFromBmp(const char *texName, const std::vector<uint8> &bmp)
{
	if (bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M') {
		CUSTOM_LOG("skin %s: not a BMP the game can use (%u bytes)\n", texName, (unsigned)bmp.size());
		return nil;
	}
	uint32 dataOff = bmp[10] | (bmp[11] << 8) | (bmp[12] << 16) | ((uint32)bmp[13] << 24);
	uint32 headerSize = bmp[14] | (bmp[15] << 8) | (bmp[16] << 16) | ((uint32)bmp[17] << 24);
	int32 w = bmp[18] | (bmp[19] << 8) | (bmp[20] << 16) | ((int32)bmp[21] << 24);
	int32 h = bmp[22] | (bmp[23] << 8) | (bmp[24] << 16) | ((int32)bmp[25] << 24);
	uint16 bpp = bmp[28] | (bmp[29] << 8);
	uint16 compression = bmp[30] | (bmp[31] << 8);
	bool topDown = h < 0;
	if (topDown) h = -h;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (bpp != 24 && bpp != 32) || compression != 0) {
		CUSTOM_LOG("skin %s: unsupported BMP (%dx%d, %d bpp, compression %d) - use an uncompressed 24/32 bpp BMP\n",
			texName, w, h, bpp, compression);
		return nil;
	}
	uint32 srcStride = ((w * bpp / 8) + 3) & ~3u;

	RwRaster *raster = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
	if (raster == nil) {
		CUSTOM_LOG("skin %s: the raster could not be created\n", texName);
		return nil;
	}
	bool swapRB = rw::platform == rw::PLATFORM_D3D9 || rw::platform == rw::PLATFORM_D3D8 ||
	              rw::platform == rw::PLATFORM_XBOX;
	for (int32 y = 0; y < h; y++) {
		uint8 *dst = RwRasterLock(raster, (uint8)y, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
		if (dst == nil) {
			RwRasterDestroy(raster);
			return nil;
		}
		uint32 sy = topDown ? y : (h - 1 - y);
		const uint8 *src = bmp.data() + dataOff + (size_t)sy * srcStride;
		for (int32 x = 0; x < w; x++) {
			uint8 b = src[x*4 + 0], g = src[x*4 + 1], r = src[x*4 + 2], a;
			if (bpp == 24) {
				b = src[x*3 + 0], g = src[x*3 + 1], r = src[x*3 + 2];
				a = 255;
			} else
				a = src[x*4 + 3];
			uint8 *d = dst + (size_t)x * 4;
			if (swapRB)
				d[0] = b, d[1] = g, d[2] = r, d[3] = a;
			else
				d[0] = r, d[1] = g, d[2] = b, d[3] = a;
		}
		raster->unlock(y);
	}

	RwTexture *tex = RwTextureCreate(raster);
	if (tex == nil) {
		RwRasterDestroy(raster);
		return nil;
	}
	strncpy(tex->name, texName, 31);
	tex->name[31] = '\0';
	tex->mask[0] = '\0';
	RwTextureSetFilterMode(tex, rwFILTERLINEAR);
	return tex;
}
#endif

RwTexture *
CPlayerSkin::GetSkinTexture(const char *texName)
{
	RwTexture *tex;
	RwRaster *raster;
	int32 width, height, depth, format;

	CTxdStore::PushCurrentTxd();
	CTxdStore::SetCurrentTxd(m_txdSlot);
	tex = RwTextureRead(texName, NULL);
	CTxdStore::PopCurrentTxd();
	if (tex != nil) {
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("skin %s: served from the skin dictionary\n", texName);
#endif
		return tex;
	}

	if (strcmp(DEFAULT_SKIN_NAME, texName) == 0 || texName[0] == '\0')
		sprintf(gString, "models\\generic\\player.bmp");
	else
		sprintf(gString, "skins\\%s.bmp", texName);

	if (RwImage *image = RtBMPImageRead(gString)) {
		RwImageFindRasterFormat(image, rwRASTERTYPETEXTURE, &width, &height, &depth, &format);
		raster = RwRasterCreate(width, height, depth, format);
		RwRasterSetFromImage(raster, image);

		tex = RwTextureCreate(raster);
		RwTextureSetName(tex, texName);
		RwTextureSetFilterMode(tex, rwFILTERLINEAR);
		RwTexDictionaryAddTexture(CTxdStore::GetSlot(m_txdSlot)->texDict, tex);

		RwImageDestroy(image);
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("skin %s: read from %s\n", texName, gString);
#endif
		return tex;
	}

#ifdef CUSTOM_MODELS
	// the custom folder: the skin can sit in one of the archives (or in the
	// skins folder of it) as a plain image
	std::vector<uint8> img;
	char ext[8] = "";
	if (CCustomModels::GetSkinImage(texName, img, ext, sizeof(ext))) {
		if (strcmp(ext, "bmp") == 0)
			tex = MakeSkinTextureFromBmp(texName, img);
		else {
			CUSTOM_LOG("skin %s: a .%s skin is not supported by the game - repack it as an uncompressed .bmp\n",
				texName, ext);
			tex = nil;
		}
		if (tex) {
			RwTexDictionaryAddTexture(CTxdStore::GetSlot(m_txdSlot)->texDict, tex);
			CUSTOM_LOG("skin %s: applied from the custom folder (%u bytes)\n", texName, (unsigned)img.size());
			return tex;
		}
	}
	CUSTOM_LOG("skin %s: not found - not in the skin dictionary, not in skins\\%s.bmp, not in the custom folder\n",
		texName, texName);
	return nil;
#else
	return nil;
#endif
}

void
CPlayerSkin::BeginFrontendSkinEdit(void)
{
	LoadPlayerDff();
	RpClumpForAllAtomics(gpPlayerClump, CClumpModelInfo::SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderPlayerCB);
	CWorld::Players[0].LoadPlayerSkin();
	gOldFov = CDraw::GetFOV();
	CDraw::SetFOV(30.0f);
}

void
CPlayerSkin::EndFrontendSkinEdit(void)
{
	RpClumpDestroy(gpPlayerClump);
	gpPlayerClump = NULL;
	CDraw::SetFOV(gOldFov);
}

void
CPlayerSkin::RenderFrontendSkinEdit(void)
{
	static float rotation = 0.0f;
	RwRGBAReal AmbientColor = { 0.65f, 0.65f, 0.65f, 1.0f };
	const RwV3d pos = { 1.35f, 0.35f, 7.725f };
	const RwV3d axis = { 0.0f, 1.0f, 0.0f };
	static uint32 LastFlash = 0;

	RwFrame *frame = RpClumpGetFrame(gpPlayerClump);

	if (CTimer::GetTimeInMillisecondsPauseMode() - LastFlash > 7) {
		rotation += 2.0f;
		if (rotation > 360.0f)
			rotation -= 360.0f;
		LastFlash = CTimer::GetTimeInMillisecondsPauseMode();
	}
	RwFrameTransform(frame, RwFrameGetMatrix(RwCameraGetFrame(Scene.camera)), rwCOMBINEREPLACE);
	RwFrameTranslate(frame, &pos, rwCOMBINEPRECONCAT);
	RwFrameRotate(frame, &axis, rotation, rwCOMBINEPRECONCAT);
	RwFrameUpdateObjects(frame);
	SetAmbientColours(&AmbientColor);
	RpClumpRender(gpPlayerClump);
}
