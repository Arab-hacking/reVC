#ifndef REVC_CUSTOM_TESTGRAPHICS_H
#define REVC_CUSTOM_TESTGRAPHICS_H

// "TEST GRAPHICS" — the graphics keys of the Black Russia client, wired to the
// levers this port actually has.
//
// Source: PUNCT ANALYSE v43, graphics chapters 17 / 18 / 19.
//
// The client exposes twelve keys and chapter 19 traces every one of them to the
// instruction that changes the render. Seven of them cannot be honest menu
// items here, so they are deliberately absent instead of being faked:
//
//   res     the client scales an offscreen target by `res / 100.0` (0xe186f4).
//           RW3 has no doRenderBufWidth/Height, so there is no such target to
//           shrink — only the window, which is a different thing.
//   water   0x247958c only picks texture flags (0x4200 + mip 0x8000, 0x18c8c58).
//           It is a two-line change inside one loader and moves nothing this
//           port renders with.
//   fps     not a limiter: the value lands in the timing object 0x3c130d0 and
//           is clamped to 1.0..6.0. A cap added here would be a new behaviour,
//           not the client's.
//   snowIntensity, winterSlip
//           a byte-width winter key and a tyre-friction key. Neither is
//           graphics, and this port has no winter pass to feed.
//   vegetation
//           `cset ne`, binary, and it only toggles VC's *grass and low-LOD
//           foliage*. Vice City has no such layer — trees are ordinary map
//           objects — so there is nothing for the switch to remove.
//   ssaa    `fmov s0, #1.25` at 0xe18b38: the scene is rendered into an
//           offscreen buffer 1.25x the screen and resolved down. librw GL3
//           renders straight into the cap's own raster, and the only copy path
//           (RwRasterRenderFast -> gl3 rasterRenderFast) is 1:1 and refuses a
//           source larger than its destination. A render scale here would need
//           an FBO resolve that does not exist, so the row would be a dead
//           switch. Explained in GRAPHICS.md instead of drawn.
//
// The remaining five have a real lever, and this module owns them:
//
//   shad   shadow pass: 0 = not rendered at all, 1..3 = reach and strength
//          step (client table 0x3d0a70 is 128/256/512 texels; librw's depth
//          map is a fixed SHADOW_MAP_SIZE, so the level scales the sampled
//          volume — fewer sectors walked — instead of the texture)
//   refl   car reflections: 0 = the paraboloid capture is never rendered,
//          1..3 = 128/256/512 render target (the RT really is this size)
//   dd     draw distance: client table 0x3d34e0 = 0.5 / 0.7 / 0.9 / 1.1
//   effect 0 removes the whole post-processing block (`cbnz` at 0xe787d4)
//   maxFps ceiling; the client keeps it in the settings map only
//
// Deliberate difference from the client: there, `shad` and `refl` both write
// the global 0x3648a18, so raising one raises the other (chapter 19, §15.5).
// That is an artefact of two features sharing one integer, not of the level
// table. This port has two independent resources, so the two rows are
// independent too — the coupling is documented in GRAPHICS.md instead of being
// reproduced.
//
// `dd = 4+` indexes past the four-entry table; the client falls back to the
// last valid entry rather than reading past it, and so does this module.
//
// This header is pure logic on purpose: no renderer, no menu, no state that
// needs a device, so the numbers stay testable without a GPU.

#include <stdint.h>

namespace revctestgfx {

// --- level tables (client 0x3d0a70 / 0x3d34e0) ----------------------------
enum { GFX_LEVELS = 4 };

// Render target size for `refl = level`; 0 means "no capture at all", which is
// what the client's getter 0x1894a08 returns below level 1.
inline int EnvMapSize(int level)
{
	if(level <= 0)
		return 0;
	if(level >= GFX_LEVELS - 1)
		return 512;
	return level == 1 ? 128 : 256;
}

// Share of the renderer's normal shadow volume that `shad = level` covers.
// Level 3 is the volume the renderer was tuned with.
inline float ShadowExtentScale(int level)
{
	if(level <= 0)
		return 0.0f;
	if(level == 1)
		return 0.5f;
	if(level == 2)
		return 0.75f;
	return 1.0f;
}

// Shadow strength for `shad = level`. The client's small maps are not "dimmer",
// but a pass that covers a quarter of the volume at full strength reads as a
// hard cut, so the strength steps with the reach.
inline float ShadowStrengthScale(int level)
{
	if(level <= 0)
		return 0.0f;
	if(level == 1)
		return 0.45f;
	if(level == 2)
		return 0.75f;
	return 1.0f;
}

// Depth bias scale: a level that covers less ground per texel needs a wider
// bias, otherwise casters shadow themselves with visible stripes.
inline float ShadowBiasScale(int level)
{
	if(level <= 0)
		return 1.0f;
	if(level == 1)
		return 2.2f;
	if(level == 2)
		return 1.4f;
	return 1.0f;
}

// --- draw distance (client 0x3d34e0) --------------------------------------
inline float DrawDistanceMultiplier(int index)
{
	static const float table[GFX_LEVELS] = { 0.5f, 0.7f, 0.9f, 1.1f };
	if(index < 0)
		index = 0;
	if(index >= GFX_LEVELS)
		index = GFX_LEVELS - 1;	// client falls back to the last valid entry
	return table[index];
}

// The client multiplies the object/LOD distance by this value and caps the
// product at 2.2 (`fcsel` at 0xde3964).
inline float ClampDistanceMultiplier(float multiplier)
{
	if(!(multiplier > 0.0f))
		return 0.0f;
	return multiplier > 2.2f ? 2.2f : multiplier;
}

// --- binary switch --------------------------------------------------------
inline bool EffectsEnabled(int value)
{
	return value != 0;	// client: cbnz over the whole post block
}

// --- the settings block ---------------------------------------------------
// The four select rows are one byte wide because that is what the menu writes
// through (CCFOSelect::value is an `int8*`) and what the .ini round-trip reads
// back. Spelled int8_t rather than the game's `int8` so this header stays a
// plain logic header that a host test can include on its own.
struct Settings
{
	int8_t shadows;		// shad       0..3
	int8_t reflections;	// refl       0..3
	int8_t drawDistance;	// dd         0..3
	int8_t effects;		// effect     0..3 (0 = block skipped)
	// One byte as well: the .ini round-trip for a select/dynamic row reads and
	// writes a single int8 through the row's value pointer, so a wider field
	// would only ever get its low byte updated. 120 fits.
	int8_t maxFps;		// maxFps     ceiling only, cycled by hand

	// Client defaults (chapter 18.5, SettingsHelper.getSerializedSettings):
	// shadow_quality_new 0, reflection_on_car_new 0, distance_new 1,
	// effect_quality_new 1, maxFps 60. ssaa_new is 0 as well, and 0 is the only
	// value this port can honour — see the note at the top.
	static Settings Defaults(void)
	{
		Settings s;
		s.shadows = 0;
		s.reflections = 0;
		s.drawDistance = 1;
		s.effects = 1;
		s.maxFps = 60;
		return s;
	}
};

// Everything a caller needs to touch the renderer, derived in one place so the
// numeric behaviour stays testable without a GPU.
struct Applied
{
	int shadowLevel;		// 0 disables the pass
	float shadowStrength;		// multiplier for the renderer's own strength
	float shadowExtent;		// multiplier for the sampled volume
	float shadowBias;		// multiplier for the depth bias
	int envMapSize;			// 0 disables the paraboloid capture
	float distanceMultiplier;	// already clamped to 2.2
	bool effectsEnabled;
	int maxFps;
};

inline int ClampLevel(int value)
{
	if(value < 0)
		return 0;
	if(value >= GFX_LEVELS)
		return GFX_LEVELS - 1;
	return value;
}

inline Applied Apply(const Settings &s)
{
	Applied a;
	a.shadowLevel = ClampLevel(s.shadows);
	a.shadowStrength = ShadowStrengthScale(a.shadowLevel);
	a.shadowExtent = ShadowExtentScale(a.shadowLevel);
	a.shadowBias = ShadowBiasScale(a.shadowLevel);
	a.envMapSize = EnvMapSize(s.reflections);
	a.distanceMultiplier = ClampDistanceMultiplier(DrawDistanceMultiplier(s.drawDistance));
	a.effectsEnabled = EffectsEnabled(s.effects);
	a.maxFps = s.maxFps < 0 ? 0 : (int)s.maxFps;
	return a;
}

// maxFps is not a select row: it is a small list of ceilings. 0 means "no
// ceiling", offered on purpose so a real cap can be told apart from vsync.
inline int CycleMaxFps(int current)
{
	static const int values[] = { 0, 30, 60, 90, 120 };
	const int n = (int)(sizeof(values) / sizeof(values[0]));
	for(int i = 0; i < n; i++)
		if(values[i] == current)
			return values[(i + 1) % n];
	return values[0];
}

inline const char *MaxFpsText(int value)
{
	static const char *const names[] = { "FEM_OFF", "FEM_30", "FEM_60", "FEM_90", "FEM_120" };
	switch(value) {
	case 30: return names[1];
	case 60: return names[2];
	case 90: return names[3];
	case 120: return names[4];
	default: return names[0];
	}
}

} // namespace revctestgfx

// --- runtime, implemented in TestGraphics.cpp -----------------------------
//
// These are plain functions rather than an exported struct: the renderer
// queries them before the menu has ever been built, and has to get the client
// defaults instead of zeroed memory.
int   TestGraphicsShadowLevel(void);
float TestGraphicsShadowStrengthScale(void);
float TestGraphicsShadowExtentScale(void);
float TestGraphicsShadowBiasScale(void);
float TestGraphicsDrawDistanceMultiplier(void);
bool  TestGraphicsReflectionEnabled(void);
bool  TestGraphicsEffectsEnabled(void);
int   TestGraphicsMaxFps(void);

// Settings block the menu edits, plus the apply/startup pair.
revctestgfx::Settings &TestGraphicsSettings(void);
void TestGraphicsStartup(void);
void TestGraphicsApply(void);

#endif // REVC_CUSTOM_TESTGRAPHICS_H
