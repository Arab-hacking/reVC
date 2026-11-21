#ifndef __GTA_CUSTOMCARPLATEMGR_H__
#define __GTA_CUSTOMCARPLATEMGR_H__

#include "common.h"

// SA CCustomCarPlateMgr (0x6FD500..0x6FE0F0), ported from gta-reversed
// (source/game_sa/CustomCarPlateMgr.{h,cpp}).
//
// A number plate in SA is not geometry: every vehicle model carries a material
// named "carplate", and the plate texture is generated at runtime from two
// sources - a charset bitmap (A-Z, 0-9 laid out four characters to a row) and
// one of three "plateback" background textures chosen by which city the player
// is in. The manager owns those textures and copies characters out of the
// charset into a freshly created raster whenever a vehicle needs its plate.
//
// The three backgrounds are not cosmetic: Los Santos, San Fierro and Las
// Venturas have visibly different plates in SA, and a car spawned in one city
// keeps that city's plate until it is generated again.

enum eCarPlateType : uint8
{
	CARPLATE_DEFAULT = 255,	// decide from the map region at generation time
	CARPLATE_SF = 0,
	CARPLATE_LV = 1,
	CARPLATE_LA = 2,
	MAX_CARPLATE = 3
};

class CCustomCarPlateMgr
{
public:
	static const uint32 MAX_TEXT_LENGTH = 8;

private:
	// All in pixels. The charset is a four-by-four grid of character cells,
	// each cell eight by sixteen; the row index counts down the grid.
	static const uint32 CHARSET_CHAR_WIDTH = 8;
	static const uint32 CHARSET_CHAR_HEIGHT = 16;
	static const uint32 CHARSET_COL_WIDTH = 4 * CHARSET_CHAR_WIDTH;	// 4 chars / row
	static const uint32 CHARSET_ROW_HEIGHT = 16;
	// Charset textures are 32-bit; one texel is four bytes.
	static const uint32 CHARSET_TEXEL_SIZE = 4;

	static RwTexture *pCharsetTex;
	static RwTexture *pPlatebackTexTab[MAX_CARPLATE];
	static RwUInt8 *pCharsetLockedData;
	static RpMaterial *CurrentLicensePlateMaterial;
	static uint8 CurrentLicensePlateType;

public:
	// Load the charset and the three backgrounds out of the vehicle TXD and
	// lock the charset raster so plate generation can read it directly.
	static bool Initialise(void);

	// Unlock the charset, destroy every texture this manager created, and drop
	// the vehicle TXD slot.
	static void Shutdown(void);

	// Fill `out` with a plate text: two letters then two digits, and for the
	// eight-character case a space and three more characters, which is the
	// "SPB 123" layout.
	static bool GeneratePlateText(char *out, uint32 length);

	// The plate background for the current map region, as an index into
	// pPlatebackTexTab.
	static int8 GetMapRegionPlateDesign(void);

	// Read the plate charset directly from a file. Present in SA, unused there
	// and unused here; kept so the port is complete.
	static int8 LoadPlatecharsetDat(const char *filename, uint8 *data);

	// Point a material at the plate background matching the plate type.
	static void SetupMaterialPlatebackTexture(RpMaterial *material, uint8 plateType);

	// Build the plate texture for `text` out of the charset, one 64x16 raster.
	static RwTexture *CreatePlateTexture(const char *text, uint8 plateType);

	// Same as SetupClump, for the case where the vehicle was resprayed or had an
	// upgrade fitted and its plate material already exists: replace the old
	// plate material with the new one and re-apply the background.
	static int8 SetupClumpAfterVehicleUpgrade(RpClump *clump, RpMaterial *plateMaterial, uint8 plateType);

	// Give `material` a freshly generated plate texture.
	static void SetupMaterialPlateTexture(RpMaterial *material, const char *plateText, uint8 plateType);

	// Walk the clump's materials, and for the plate one generate `plateText`
	// into it. Returns the plate material, or nil when the clump has none.
	static RpMaterial *SetupClump(RpClump *clump, const char *plateText, uint8 plateType);

private:
	// The actual blit: copy each character of `text` out of the charset raster
	// into the plate raster.
	static bool RenderLicenseplateTextToRaster(const char *text, RwRaster *charsetRaster, RwRaster *plateRaster);

	// Material walkers. RpClumpForAllAtomics needs a plain function pointer, so
	// the per-call state (which plate type, which text) goes through these
	// statics, exactly as SA does with its two file-scope pointers.
	static RpMaterial *MaterialUpgradeSetCarplateTextureCB(RpMaterial *material, void *data);
	static RpAtomic *AtomicUpgradeSetCarplateTextureCB(RpAtomic *atomic, void *data);
	static RpMaterial *MaterialSetCarplateTextureCB(RpMaterial *material, void *plateText);
	static RpAtomic *AtomicSetCarplateTextureCB(RpAtomic *atomic, void *plateText);

	// Column and row of a character inside the charset bitmap. A character the
	// bitmap has no glyph for falls back to the same position SA uses for an
	// unknown character.
	static void GetCharacterPositionInCharSet(char c, uint32 *col, uint32 *row);

	// Turn CARPLATE_DEFAULT into a concrete type.
	static uint8 ResolvePlateType(uint8 plateType);
};

#endif // __GTA_CUSTOMCARPLATEMGR_H__
