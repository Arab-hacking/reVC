#include "common.h"

#include "TestGraphics.h"

#ifdef CUSTOM_FRONTEND_OPTIONS

#include "Frontend.h"
#include "custompipes.h"
#include "SavehDiag.h"

// Runtime end of "TEST GRAPHICS" (see TestGraphics.h for what each key means,
// which client chapter it comes from, and why six of the twelve keys are not
// offered here at all).
//
// The module owns exactly three things:
//
//   1. the settings the menu edits and the apply pass that clamps them;
//   2. the environment map render target, because the paraboloid capture
//      camera in custompipes.cpp is created from one `EnvMapSize` global;
//   3. the published `Applied` block the renderer reads.
//
// Everything else stays where it already lives: CSunShadowMap keeps its own
// volume and strength and only receives multipliers, postfx.cpp keeps its own
// pipeline and only asks whether effects are on, and the draw-distance
// multiplier is folded in where the optimisation stage already writes
// CRenderer::ms_lodDistScale.

namespace {

revctestgfx::Settings s_settings;
revctestgfx::Applied s_applied;

// The client defaults are also what a caller sees if it asks before
// TestGraphicsStartup has run (the renderer starts before the menu exists).
bool s_started = false;
bool s_envMapDirty = false;

// TestGraphicsApply is also called from LoadINISettings, and that runs inside
// RsInitialize - before RwEngineInit. Reaching a raster from there goes through
// Engine::memfuncs.rwmalloc while it is still nil, i.e. an indirect call to
// address 0, which is exactly the pc=0 SIGSEGV seen on device once reVC.ini
// carried a non-zero `refl`. The reflection map is therefore only rebuilt after
// CustomPipeInit has created the pipelines (TestGraphicsEngineReady); until then
// the requested size just stays pending.
bool s_pipesReady = false;

bool
CanTouchRenderWare(void)
{
	return s_pipesReady;
}

// Rebuild the capture camera when the requested RT size changed. While the
// camera is gone EnvMapRender returns immediately, so there is no window in
// which the capture pass could run against a half-destroyed raster.
void
ApplyEnvMapSize(void)
{
	if(!s_envMapDirty)
		return;
	// Not yet applicable: keep the flag so the call from Game.cpp after
	// CustomPipeInit does the work.
	if(!CanTouchRenderWare())
		return;
	s_envMapDirty = false;

	CustomPipes::EnvMapReinit(s_applied.envMapSize);
}

// Called once CustomPipeInit has built the reflection map. Until this runs the
// menu can record a requested size but must not touch a raster.
void
TestGraphicsEngineReady(void)
{
	s_pipesReady = true;
	// EnvMapInit built the map at its default size, so a pending request for a
	// different one still has to run; a matching one is already satisfied.
	if(s_envMapDirty && s_applied.envMapSize == CustomPipes::EnvMapSize)
		s_envMapDirty = false;
	ApplyEnvMapSize();
}

} // namespace

void
TestGraphicsStartup(void)
{
	s_settings = revctestgfx::Settings::Defaults();
	s_applied = revctestgfx::Apply(s_settings);
	s_started = true;
	s_envMapDirty = false;
	CUSTOM_LOG("test graphics: shadows %d, reflections %d, draw distance %d, effects %d, maxFps %d\n",
	           s_settings.shadows, s_settings.reflections, s_settings.drawDistance,
	           s_settings.effects, s_settings.maxFps);
}

// Called whenever a menu row changes, and once after the ini has been read.
void
TestGraphicsApply(void)
{
	s_settings.shadows = revctestgfx::ClampLevel(s_settings.shadows);
	s_settings.reflections = revctestgfx::ClampLevel(s_settings.reflections);
	s_settings.drawDistance = revctestgfx::ClampLevel(s_settings.drawDistance);
	s_settings.effects = revctestgfx::ClampLevel(s_settings.effects);
	if(s_settings.maxFps != 0 && s_settings.maxFps != 30 && s_settings.maxFps != 60 &&
	   s_settings.maxFps != 90 && s_settings.maxFps != 120)
		s_settings.maxFps = 60;

	if(!s_started) {
		s_started = true;
		s_envMapDirty = false;
	}

	const int prevEnv = s_applied.envMapSize;
	s_applied = revctestgfx::Apply(s_settings);
	if(s_applied.envMapSize != prevEnv)
		s_envMapDirty = true;

	ApplyEnvMapSize();
}

revctestgfx::Settings &
TestGraphicsSettings(void)
{
	return s_settings;
}

int
TestGraphicsShadowLevel(void)
{
	return s_started ? s_applied.shadowLevel : 0;
}

float
TestGraphicsShadowStrengthScale(void)
{
	return s_started ? s_applied.shadowStrength : 1.0f;
}

float
TestGraphicsShadowExtentScale(void)
{
	return s_started ? s_applied.shadowExtent : 1.0f;
}

float
TestGraphicsShadowBiasScale(void)
{
	return s_started ? s_applied.shadowBias : 1.0f;
}

float
TestGraphicsDrawDistanceMultiplier(void)
{
	return s_started ? s_applied.distanceMultiplier : 1.0f;
}

bool
TestGraphicsReflectionEnabled(void)
{
	// Before the module starts the capture runs, which is the current
	// behaviour of the port; the menu turns it off explicitly.
	return s_started ? s_applied.envMapSize > 0 : true;
}

bool
TestGraphicsEffectsEnabled(void)
{
	return s_started ? s_applied.effectsEnabled : true;
}

// The ceiling is enforced where the frame is presented, not by dropping the
// work: holding the frame back keeps the simulation at one step per presented
// frame, which is what the client's maxFps does too.
int
TestGraphicsFrameHoldMs(float frameMs)
{
	return (int)revctestgfx::FrameHoldMs(TestGraphicsMaxFps(), frameMs);
}

int
TestGraphicsMaxFps(void)
{
	return s_started ? s_applied.maxFps : 0;
}

#endif // CUSTOM_FRONTEND_OPTIONS
