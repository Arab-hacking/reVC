// SA CCustomCarPlateMgr (0x6FD500..0x6FE0F0), ported from gta-reversed
// (source/game_sa/CustomCarPlateMgr.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// The plate pipeline is: a material named "carplate" exists in every vehicle
// clump; this manager writes a generated texture into it. The texture is a
// copy of the characters out of a charset bitmap, so no per-plate art is
// needed and any text can be shown.
#include "common.h"

#include "CustomCarPlateMgr.h"

#include "Clock.h"
#include "FileMgr.h"
#include "General.h"
#include "TxdStore.h"
#include "Weather.h"
#include "Zones.h"
#include "Camera.h"

RwTexture *CCustomCarPlateMgr::pCharsetTex;
RwTexture *CCustomCarPlateMgr::pPlatebackTexTab[MAX_CARPLATE];
RwUInt8 *CCustomCarPlateMgr::pCharsetLockedData;
RpMaterial *CCustomCarPlateMgr::CurrentLicensePlateMaterial;
uint8 CCustomCarPlateMgr::CurrentLicensePlateType;

// Not a SA function: case-insensitive compare, so a model whose material is
// spelled "CarPlate" is still found. SA calls _stricmp, which is the same test
// and not available on every platform this fork builds for.
static bool
NameMatches(const char *a, const char *b)
{
	if(a == nil || b == nil)
		return false;
	while(*a && *b){
		char ca = *a, cb = *b;
		if(ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
		if(cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
		if(ca != cb)
			return false;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

// SA CCustomCarPlateMgr::Initialise (0x6FD500).
//
// The plate textures live in the shared "vehicle" TXD, so the dictionary is
// pushed before reading them and popped afterwards - RwTextureRead looks in the
// current dictionary only.
bool
CCustomCarPlateMgr::Initialise(void)
{
	{
		const int32 txd = CTxdStore::FindTxdSlot("vehicle");
		CTxdStore::PushCurrentTxd();
		CTxdStore::SetCurrentTxd(txd);
	}

	// SA calls these with a filter mode where an addressing mode belongs - the
	// reference transcribes it as written. What the code is reaching for is
	// clamp addressing (a plate never tiles, so a glyph near the edge must not
	// wrap round) plus nearest filtering for the charset, whose glyphs are
	// copied pixel for pixel and would blur under linear filtering.
	pCharsetTex = RwTextureRead("platecharset", nil);
	if(pCharsetTex){
		RwTextureSetAddressingU(pCharsetTex, rwTEXTUREADDRESSCLAMP);
		RwTextureSetAddressingV(pCharsetTex, rwTEXTUREADDRESSCLAMP);
		RwTextureSetFilterMode(pCharsetTex, rwFILTERNEAREST);
	}

	// The backgrounds are filtered linearly: they are smooth gradients with a
	// border, and nearest sampling on them makes the border crawl as the
	// plate's on-screen size changes.
	static const char *backNames[MAX_CARPLATE] = { "plateback1", "plateback2", "plateback3" };
	for(uint32 i = 0; i < MAX_CARPLATE; i++){
		pPlatebackTexTab[i] = RwTextureRead(backNames[i], nil);
		if(pPlatebackTexTab[i]){
			RwTextureSetAddressingU(pPlatebackTexTab[i], rwTEXTUREADDRESSCLAMP);
			RwTextureSetAddressingV(pPlatebackTexTab[i], rwTEXTUREADDRESSCLAMP);
			RwTextureSetFilterMode(pPlatebackTexTab[i], rwFILTERLINEAR);
		}
	}

	CTxdStore::PopCurrentTxd();

	// The charset is read straight out of the locked raster for every plate,
	// so it stays locked for the whole run of the game.
	if(pCharsetTex)
		pCharsetLockedData = RwRasterLock(RwTextureGetRaster(pCharsetTex), 0, rwRASTERLOCKREAD);
	else
		pCharsetLockedData = nil;

	return pCharsetLockedData != nil;
}

// SA CCustomCarPlateMgr::Shutdown (0x6FD720).
void
CCustomCarPlateMgr::Shutdown(void)
{
	if(pCharsetTex){
		RwRasterUnlock(RwTextureGetRaster(pCharsetTex));
		pCharsetLockedData = nil;

		RwTextureDestroy(pCharsetTex);
		pCharsetTex = nil;
	}

	for(uint32 i = 0; i < MAX_CARPLATE; i++){
		if(pPlatebackTexTab[i]){
			RwTextureDestroy(pPlatebackTexTab[i]);
			pPlatebackTexTab[i] = nil;
		}
	}

	const int32 slot = CTxdStore::FindTxdSlot("vehicle");
	if(slot != -1)
		CTxdStore::RemoveTxdSlot(slot);
}

// SA CCustomCarPlateMgr::GetMapRegionPlateDesign (0x6FD7A0).
//
// SA asks the weather code which region the player is in rather than the zone
// code, because the region is what the streaming weather model is keyed on. In
// this fork the region is the current zone level, so the mapping is the same
// three-way split expressed over eLevelName.
int8
CCustomCarPlateMgr::GetMapRegionPlateDesign(void)
{
	switch(CTheZones::m_CurrLevel){
	case LEVEL_GENERIC:	return CARPLATE_SF;	// San Fierro and the countryside
	case LEVEL_BEACH:	return CARPLATE_LA;	// Los Santos
	case LEVEL_MAINLAND:	return CARPLATE_LV;	// Las Venturas and the desert
	default:
		// A vehicle generated before the level is known still needs a plate;
		// the San Fierro background is the one the game starts on.
		return CARPLATE_SF;
	}
}

uint8
CCustomCarPlateMgr::ResolvePlateType(uint8 plateType)
{
	if(plateType == CARPLATE_DEFAULT)
		return (uint8)GetMapRegionPlateDesign();
	if(plateType >= MAX_CARPLATE)
		return CARPLATE_SF;
	return plateType;
}

// SA CCustomCarPlateMgr::LoadPlatecharsetDat (0x6FDC00).
//
// Unused in SA - the charset always comes out of the TXD - and unused here. It
// is kept so the port matches the reference, and it reads the file through the
// game's own file manager so a modded charset in the game directory is found.
int8
CCustomCarPlateMgr::LoadPlatecharsetDat(const char *filename, uint8 *data)
{
	if(filename == nil || data == nil)
		return -1;

	const int32 fd = CFileMgr::OpenFile(filename);
	if(fd < 0)
		return -1;

	// The charset file is a raw blob: 32 characters of 8x16 texels, four to a
	// row. It has no header, so the size is what the caller allocated for.
	const uint32 size = CHARSET_COL_WIDTH * CHARSET_ROW_HEIGHT * 9 * CHARSET_TEXEL_SIZE;
	const size_t read = CFileMgr::Read(fd, (char*)data, size);
	CFileMgr::CloseFile(fd);

	return read == size ? 0 : -1;
}

// SA CCustomCarPlateMgr::SetupMaterialPlatebackTexture (0x6FDE50).
void
CCustomCarPlateMgr::SetupMaterialPlatebackTexture(RpMaterial *material, uint8 plateType)
{
	if(material == nil)
		return;

	RwTexture *tex = pPlatebackTexTab[ResolvePlateType(plateType)];
	if(tex)
		RpMaterialSetTexture(material, tex);
}

// SA CCustomCarPlateMgr::GetCharacterPositionInCharSet (0x6FD7C0).
//
// The charset is a grid of four columns by nine rows. The letters run from the
// top left in alphabet order, the digits are in the last two rows, and
// everything else maps to the blank cell in the bottom right corner.
void
CCustomCarPlateMgr::GetCharacterPositionInCharSet(char c, uint32 *col, uint32 *row)
{
	// Letters: A is (0,0), and the grid is four wide.
	if(c >= 'A' && c <= 'Z'){
		const uint32 idx = c - 'A';
		*col = idx % 4;
		*row = idx / 4;
		return;
	}
	// The generator writes upper case, but a script-supplied plate can carry
	// lower case; the charset has no separate glyphs for it.
	if(c >= 'a' && c <= 'z'){
		const uint32 idx = c - 'a';
		*col = idx % 4;
		*row = idx / 4;
		return;
	}
	if(c >= '0' && c <= '9'){
		// '0' starts the seventh row: the letters take rows 0..5 whole (26
		// characters over four columns is six full rows plus two).
		const uint32 idx = c - '0';
		*col = (idx + 2) % 4;
		*row = 6 + (idx + 2) / 4;
		return;
	}

	// Anything else: the blank cell.
	*col = 0;
	*row = 9;
}

// SA CCustomCarPlateMgr::RenderLicenseplateTextToRaster (0x6FDD70).
//
// Copies eight characters, each eight by sixteen texels, from the charset
// raster to the plate raster. The charset is 32-bit, so a texel is four bytes
// and a row copy is a memcpy of eight texels.
bool
CCustomCarPlateMgr::RenderLicenseplateTextToRaster(const char *text, RwRaster *charsetRaster, RwRaster *plateRaster)
{
	(void)charsetRaster;	// the charset is read through pCharsetLockedData

	if(text == nil || plateRaster == nil)
		return false;
	if(pCharsetLockedData == nil)
		return false;

	RwUInt8 *plateLocked = RwRasterLock(plateRaster, 0, rwRASTERLOCKNOFETCH | rwRASTERLOCKWRITE);
	if(plateLocked == nil)
		return false;

	const RwInt32 plateStride = RwRasterGetStride(plateRaster);
	if(plateStride == 0){
		RwRasterUnlock(plateRaster);
		return false;
	}

	const RwInt32 charsetStride = RwRasterGetStride(RwTextureGetRaster(pCharsetTex));
	if(charsetStride == 0){
		RwRasterUnlock(plateRaster);
		return false;
	}

	// One character occupies CHARSET_COL_WIDTH * CHARSET_CHAR_WIDTH texels in
	// the charset; the plate is exactly eight of them wide.
	RwUInt8 *plateChar = plateLocked;
	for(uint32 letter = 0; letter < MAX_TEXT_LENGTH; letter++){
		uint32 charCol, charRow;
		GetCharacterPositionInCharSet(text[letter], &charCol, &charRow);

		// Top left texel of the glyph inside the charset.
		RwUInt8 *charsetChar = pCharsetLockedData +
			(CHARSET_COL_WIDTH * CHARSET_ROW_HEIGHT * charRow + CHARSET_CHAR_WIDTH * charCol) * CHARSET_TEXEL_SIZE;
		RwUInt8 *plateRow = plateChar;

		for(uint32 r = 0; r < CHARSET_CHAR_HEIGHT; r++){
			memcpy(plateRow, charsetChar, CHARSET_CHAR_WIDTH * CHARSET_TEXEL_SIZE);
			plateRow += plateStride;
			charsetChar += charsetStride;
		}

		plateChar += CHARSET_CHAR_WIDTH * CHARSET_TEXEL_SIZE;
	}

	RwRasterUnlock(plateRaster);
	return true;
}

// SA CCustomCarPlateMgr::CreatePlateTexture (0x6FDEA0).
RwTexture *
CCustomCarPlateMgr::CreatePlateTexture(const char *text, uint8 plateType)
{
	if(text == nil || pCharsetTex == nil)
		return nil;

	RwRaster *plateRaster = RwRasterCreate(64, 16, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT888 | rwRASTERLOCKWRITE);
	if(plateRaster == nil)
		return nil;

	if(!RenderLicenseplateTextToRaster(text, RwTextureGetRaster(pCharsetTex), plateRaster)){
		RwRasterDestroy(plateRaster);
		return nil;
	}

	RwTexture *plateTex = RwTextureCreate(plateRaster);
	if(plateTex == nil)
		return nil;

	// The texture is named after its own text, which is also what makes the
	// per-city plate type selectable later: the name is stable per plate.
	RwTextureSetName(plateTex, text);
	RwTextureSetFilterMode(plateTex, rwFILTERNEAREST);
	(void)plateType;	// the background is applied to the other material
	return plateTex;
}

// SA CCustomCarPlateMgr::MaterialUpgradeSetCarplateTextureCB (0x6FDF50).
//
// After an upgrade the geometry has two plate materials and the old one has to
// be swapped for the one the new texture was generated for.
RpMaterial *
CCustomCarPlateMgr::MaterialUpgradeSetCarplateTextureCB(RpMaterial *material, void *data)
{
	RpGeometry *geometry = (RpGeometry*)data;
	if(material == nil || geometry == nil)
		return material;

	RwTexture *tex = RpMaterialGetTexture(material);
	if(tex == nil)
		return material;

	const char *name = RwTextureGetName(tex);
	if(name == nil)
		return material;

	if(NameMatches(name, "careplate")){
		if(CurrentLicensePlateMaterial)
			RpMaterialSetTexture(material, RpMaterialGetTexture(CurrentLicensePlateMaterial));
	}else if(NameMatches(name, "carpback")){
		SetupMaterialPlatebackTexture(material, CurrentLicensePlateType);
	}

	return material;
}

// SA CCustomCarPlateMgr::AtomicUpgradeSetCarplateTextureCB (0x6FDFC0).
RpAtomic *
CCustomCarPlateMgr::AtomicUpgradeSetCarplateTextureCB(RpAtomic *atomic, void *data)
{
	(void)data;
	if(atomic == nil)
		return atomic;

	RpGeometry *geometry = RpAtomicGetGeometry(atomic);
	RpGeometryForAllMaterials(geometry, MaterialUpgradeSetCarplateTextureCB, geometry);
	return atomic;
}

// SA CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade (0x6FDFE0).
int8
CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade(RpClump *clump, RpMaterial *plateMaterial, uint8 plateType)
{
	if(clump == nil || plateMaterial == nil)
		return false;

	CurrentLicensePlateType = plateType;
	CurrentLicensePlateMaterial = plateMaterial;
	RpClumpForAllAtomics(clump, AtomicUpgradeSetCarplateTextureCB, nil);
	return true;
}

// SA CCustomCarPlateMgr::SetupMaterialPlateTexture (0x6FE020).
void
CCustomCarPlateMgr::SetupMaterialPlateTexture(RpMaterial *material, const char *plateText, uint8 plateType)
{
	if(material == nil || plateText == nil)
		return;

	RwTexture *plateTex = CreatePlateTexture(plateText, plateType);
	if(plateTex == nil)
		return;

	// The material keeps its own reference, so the one CreatePlateTexture made
	// has to be dropped - otherwise every respray leaks a texture.
	RpMaterialSetTexture(material, plateTex);
	RwTextureDestroy(plateTex);
}

// SA CCustomCarPlateMgr::MaterialSetCarplateTextureCB (0x6FE060).
RpMaterial *
CCustomCarPlateMgr::MaterialSetCarplateTextureCB(RpMaterial *material, void *plateText)
{
	if(material == nil || plateText == nil)
		return material;

	RwTexture *tex = RpMaterialGetTexture(material);
	if(tex == nil)
		return material;

	const char *name = RwTextureGetName(tex);
	if(name == nil)
		return material;

	if(NameMatches(name, "carplate")){
		CurrentLicensePlateMaterial = material;
		SetupMaterialPlateTexture(material, (const char*)plateText, CurrentLicensePlateType);
	}else if(NameMatches(name, "carpback")){
		SetupMaterialPlatebackTexture(material, CurrentLicensePlateType);
	}

	return material;
}

// SA CCustomCarPlateMgr::AtomicSetCarplateTextureCB (0x6FE0D0).
RpAtomic *
CCustomCarPlateMgr::AtomicSetCarplateTextureCB(RpAtomic *atomic, void *plateText)
{
	if(atomic == nil)
		return atomic;

	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), MaterialSetCarplateTextureCB, plateText);
	return atomic;
}

// SA CCustomCarPlateMgr::SetupClump (0x6FE0F0).
RpMaterial *
CCustomCarPlateMgr::SetupClump(RpClump *clump, const char *plateText, uint8 plateType)
{
	if(clump == nil)
		return nil;

	CurrentLicensePlateType = plateType;
	CurrentLicensePlateMaterial = nil;
	RpClumpForAllAtomics(clump, AtomicSetCarplateTextureCB, (void*)plateText);
	return CurrentLicensePlateMaterial;
}

// SA CCustomCarPlateMgr::GeneratePlateText (0x6FD5B0).
//
// Four characters are the minimum; the eight-character case that SA writes is
// "AB 123" style - two letters, two digits, a space and three more alphabetic
// characters, which is what its own generator produces for the longer plates.
//
// SA's generator has a bug: its letter range stops at 'X' and its digit range
// stops at '9' exclusive, so 'Y', 'Z' and '9' never appear. That is reproduced
// here, because a plate is a visible artefact of the original game and cars
// with a 'Z' in the plate would look wrong.
bool
CCustomCarPlateMgr::GeneratePlateText(char *out, uint32 length)
{
	if(out == nil || length < 4)
		return false;

	uint32 i = 0;
	const uint32 limit = Min(length, MAX_TEXT_LENGTH);

	out[i++] = (char)CGeneral::GetRandomNumberInRange('A', 'X' + 1);
	out[i++] = (char)CGeneral::GetRandomNumberInRange('A', 'X' + 1);
	out[i++] = (char)CGeneral::GetRandomNumberInRange('0', '9');
	out[i++] = (char)CGeneral::GetRandomNumberInRange('0', '9');

	// Everything past the first four is written in blocks of four: a space and
	// three letters, which fills eight characters exactly.
	while(i + 3 < limit){
		out[i++] = ' ';
		out[i++] = (char)CGeneral::GetRandomNumberInRange('A', 'X' + 1);
		out[i++] = (char)CGeneral::GetRandomNumberInRange('A', 'X' + 1);
		out[i++] = (char)CGeneral::GetRandomNumberInRange('A', 'X' + 1);
	}

	out[i] = '\0';
	return true;
}
