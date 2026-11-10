#include "common.h"

#include "Optimization.h"
#include "OptimizationProfile.h"
#include "CustomModels.h"
#include "DebugLog.h"
#include "SavehDiag.h"

#include "Streaming.h"
#include "Renderer.h"
#include "Frontend.h"
#include "Timer.h"

#ifdef CUSTOM_MODELS

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace {

using optprofile::Controller;
using optprofile::Params;

Controller s_controller;
OptimizationSnapshot s_snapshot;
bool s_started = false;
bool s_configLoaded = false;
const char *s_configSource = "built-in defaults";

// The RAM figures only change on plug/unplug, so sampling /proc every frame is
// pure overhead. 250 ms is well below the 1 s mitigation delay, which is the
// fastest reaction the client ever takes.
const uint32 kSampleIntervalMs = 250;

unsigned long long s_lastSampleMs = 0;
bool s_haveSample = false;
size_t s_totalRamMb = 0;
size_t s_availableMb = 0;

// Resolved distance scale; re-applied when the smoothed value moves enough to
// matter so we never write ms_lodDistScale every frame for a 0.0001 step.
float s_appliedScale = -1.0f;

void
ReadMemInfo(void)
{
	s_totalRamMb = 0;
	s_availableMb = 0;
	FILE *fp = fopen("/proc/meminfo", "rb");
	if(fp == nil)
		return;
	// MemTotal is near the start, MemAvailable a few lines down; 2 KiB covers
	// the head of the file on every Android kernel we have seen.
	char buf[2048];
	size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
	fclose(fp);
	buf[n] = '\0';

	size_t kb = 0;
	if(optprofile::MemInfoValueKb(buf, "MemTotal:", kb))
		s_totalRamMb = kb / 1024;
	if(optprofile::MemInfoValueKb(buf, "MemAvailable:", kb))
		s_availableMb = kb / 1024;
	// Older kernels only export MemFree; use it rather than reporting zero.
	if(s_availableMb == 0 && optprofile::MemInfoValueKb(buf, "MemFree:", kb))
		s_availableMb = kb / 1024;
}

size_t
BudgetBytesFromRam(void)
{
	size_t rows = 0;
	const optprofile::BudgetRow *table = optprofile::DefaultBudgetTable(rows);
	size_t ramMb = s_totalRamMb;
	if(ramMb == 0) {
		// Unknown device: the middle row, which is what a mid-range phone gets.
		ramMb = 8 * 1024;
	}
	return (size_t)optprofile::BudgetForRamMb((uint32_t)ramMb, table, rows) << 20;
}

void
Publish(const Controller &c)
{
	s_snapshot.distanceScale = c.DistanceScale();
	s_snapshot.pressure = c.Pressure();
	s_snapshot.freeHeadroomMb = c.FreeHeadroomMb();
	s_snapshot.mitigation = c.MitigationActive();
	s_snapshot.critical = c.CriticalActive();
	s_snapshot.vehicleRequestsPerFrame = c.VehicleRequestsPerFrame();
	s_snapshot.asyncRequestsPerFrame = c.AsyncRequestsPerFrame();
	s_snapshot.skinFallbackWanted = c.SkinFallbackWanted();
}

void
LoadConfigFile(void)
{
	// data/optimization.json (or the ZIP entry of the same name). Everything is
	// optional: without the file the built-in defaults from the analysis apply.
	std::vector<uint8> data;
	if(!CCustomModels::ReadDataFile("data/optimization.json", data, nil) || data.empty()) {
		CUSTOM_LOG("optimization: no data/optimization.json; built-in defaults "
		           "(32/48 MB, 0.75 / 0.6, 0.25 per second)\n");
		return;
	}

	Params params = s_controller.GetParams();
	std::string err;
	int applied = optprofile::LoadConfig((const char *)data.data(), data.size(), params, err);
	if(applied < 0) {
		CUSTOM_LOG("optimization: data/optimization.json is invalid (%s); using defaults\n",
		           err.c_str());
		return;
	}
	s_controller.SetParams(params);
	s_configLoaded = true;
	s_configSource = "data/optimization.json";
	CUSTOM_LOG("optimization: %d parameter(s) from data/optimization.json "
	           "(mitigation %g/%g MB scale %g, critical %g/%g MB scale %g)\n",
	           applied,
	           params.mitigationEnterHeadroomMb, params.mitigationLeaveHeadroomMb,
	           params.mitigationDistanceScale,
	           params.criticalEnterHeadroomMb, params.criticalLeaveHeadroomMb,
	           params.criticalDistanceScale);
}

} // namespace

namespace revcopt {

bool
Startup(void)
{
	if(s_started)
		return s_configLoaded;
	s_started = true;

	s_controller.Reset();
	ReadMemInfo();
	s_controller.SetBudgetBytes(BudgetBytesFromRam());
	LoadConfigFile();

	// Start from whatever the menu asked for, not from 1.0: the streaming scale
	// is refreshed as soon as the first Tick runs, and a stale 1.0 in between
	// would widen the draw distance for a frame.
	s_appliedScale = -1.0f;
	Publish(s_controller);

	CUSTOM_LOG("optimization: RAM %u MB, streaming budget %u MB, %d vehicle / %d async "
	           "model request(s) per frame when unconstrained\n",
	           (unsigned)s_totalRamMb, (unsigned)(s_controller.BudgetBytes() >> 20),
	           s_snapshot.vehicleRequestsPerFrame, s_snapshot.asyncRequestsPerFrame);
	return s_configLoaded;
}

bool
ConfigLoaded(void)
{
	return s_configLoaded;
}

const char *
ConfigSource(void)
{
	return s_configSource;
}

OptimizationSnapshot
Tick(unsigned nowMs, float dtSeconds, unsigned long long usedBytes,
     unsigned long long availableBytes)
{
	if(!s_started)
		Startup();

	if(!s_haveSample || (uint32)(nowMs - s_lastSampleMs) >= kSampleIntervalMs) {
		s_haveSample = true;
		s_lastSampleMs = nowMs;
		ReadMemInfo();
	}

	// The budget is re-derived when the device reports a different RAM figure
	// (rare, but a hotplug does change it on some devices).
	if(s_totalRamMb != 0) {
		size_t budget = BudgetBytesFromRam();
		if(budget != s_controller.BudgetBytes())
			s_controller.SetBudgetBytes(budget);
	}

	// "used" is pool occupancy, exactly as the client's machine computes it
	// (chapter 9 §9.2: limit - base = denominator, used - base = numerator).
	// Deriving it from the device's free RAM instead was wrong in both
	// directions: it is not comparable with the RAM-derived budget, and on
	// Android the limit for a 64-bit app is ~3-4 GB while the budget is a few
	// hundred MB, so pressure stayed pinned at 1.0 and the distance scale sat
	// at the critical value for the whole session.
	size_t budget = s_controller.BudgetBytes();
	size_t used;
	if(usedBytes != 0) {
		used = (size_t)usedBytes;
	} else if(s_availableMb != 0) {
		// No pool figure yet: fall back to the old free-RAM comparison so a
		// caller that cannot measure usage still gets a sane signal.
		size_t avail = s_availableMb << 20;
		used = avail < budget ? budget - avail : budget;
	} else {
		used = 0;	// nothing known: report no pressure rather than full
	}

	uint32 before = s_controller.ModeRevision();
	s_controller.Update(nowMs, used, 0, dtSeconds);
	Publish(s_controller);

	if(s_controller.ModeRevision() != before) {
		CUSTOM_LOG("optimization: %s (pressure %.2f, headroom %.0f MB) -> distance scale %g\n",
		           s_snapshot.critical ? "critical" : (s_snapshot.mitigation ? "mitigation" : "normal"),
		           s_snapshot.pressure, s_snapshot.freeHeadroomMb,
		           s_snapshot.critical ? s_controller.GetParams().criticalDistanceScale
		                               : (s_snapshot.mitigation ? s_controller.GetParams().mitigationDistanceScale
		                                                        : 1.0f));
	}

	// Fold the multiplier into the menu's draw-distance preference exactly where
	// the old RAM heuristic did, so the slider keeps its meaning.
	float menuScale = FrontEndMenuManager.m_PrefsLOD;
	float wanted = Clamp(menuScale * s_snapshot.distanceScale, DRAWDIST_MIN, DRAWDIST_MAX);
	if(s_appliedScale < 0.0f || fabsf(wanted - s_appliedScale) > 0.002f) {
		s_appliedScale = wanted;
		CRenderer::ms_lodDistScale = wanted;
	}
	return s_snapshot;
}

const OptimizationSnapshot &
Current(void)
{
	if(!s_started) {
		s_snapshot.distanceScale = 1.0f;
		s_snapshot.pressure = 0.0f;
		s_snapshot.freeHeadroomMb = 0.0f;
		s_snapshot.mitigation = false;
		s_snapshot.critical = false;
		s_snapshot.vehicleRequestsPerFrame = 3;
		s_snapshot.asyncRequestsPerFrame = 5;
		s_snapshot.skinFallbackWanted = false;
	}
	return s_snapshot;
}

size_t
BudgetBytes(void)
{
	if(!s_started)
		return (size_t)256 << 20;
	return s_controller.BudgetBytes();
}

int
VehicleRingTarget(int normalSlots)
{
	if(!s_started)
		return normalSlots;
	return s_controller.VehicleRingTarget(normalSlots);
}

float
FarPlaneMargin(void)
{
	if(!s_started)
		return 1.0f;
	return s_controller.GetParams().farPlaneMarginScale;
}

} // namespace revcopt

#else // CUSTOM_MODELS

namespace revcopt {

bool Startup(void) { return false; }
bool ConfigLoaded(void) { return false; }
const char *ConfigSource(void) { return "disabled"; }

OptimizationSnapshot
Tick(unsigned, float, unsigned long long)
{
	OptimizationSnapshot s;
	memset(&s, 0, sizeof(s));
	s.distanceScale = 1.0f;
	s.vehicleRequestsPerFrame = 3;
	s.asyncRequestsPerFrame = 5;
	return s;
}

const OptimizationSnapshot &
Current(void)
{
	static OptimizationSnapshot s;
	return s;
}

size_t BudgetBytes(void) { return (size_t)0; }
int VehicleRingTarget(int normalSlots) { return normalSlots; }

float FarPlaneMargin(void) { return 1.0f; }

} // namespace revcopt

#endif // CUSTOM_MODELS
