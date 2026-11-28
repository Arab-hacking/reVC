#ifndef __GTA_CUSTOMZONES_H__
#define __GTA_CUSTOMZONES_H__

// data/mapzones.json from the common archive (with the game's own
// data/map.zon layout as fallback) defines world boxes that pin the
// game clock, force a weather and scale scene brightness while the
// player is inside. data/weather.json carries the client's snow
// configuration (texture, tint, amount, tiling); it is parsed and
// exposed here for the material pass. Without either file the weather
// and clock behave exactly as stock.
class CCustomZones
{
public:
	// Lazy-loads the definitions on first call; call once per frame.
	static void Update(void);
	// Brightness of the zone the player is currently in, blended over
	// the zone's transition; 1.0 outside any zone. Consumed by the
	// colour grade evaluation.
	static float CurrentBrightness(void);
	static bool SnowEnabled(void);
	static const char *SnowTexture(void);
	static void SnowColour(uint8 &r, uint8 &g, uint8 &b, uint8 &a);
	static float SnowAmount(void);
	static float SnowTile(void);
};

#endif // __GTA_CUSTOMZONES_H__
