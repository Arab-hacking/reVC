#ifndef REVC_CUSTOM_OPTIMIZATIONPROFILE_H
#define REVC_CUSTOM_OPTIMIZATIONPROFILE_H

// Memory-pressure driven optimization, mirroring the mobile client's model.
//
// The client has no "graphics preset": it reacts to a memory budget with a
// single structure of 48 parameters in five json sections, and three loops run
// from it:
//
//   /proc/meminfo RAM  ->  budget (memoryBudgetScaling table)
//   used/budget        ->  pressure = (used - base) / (limit - base - reserve)
//   free headroom      ->  mode machine (mitigation / critical, with hysteresis)
//   mode               ->  distance scale, smoothed towards the target
//   pressure           ->  per-frame request quota (normal / constrained)
//
// This header holds that logic with no engine dependencies so it can be tested
// on the host. LoadConfig() accepts any subset of the sections: a partial file
// keeps the defaults below, exactly like the client's parser.

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "MiniJson.h"

namespace optprofile {

// Section 8 of the client analysis: every default value, in structure order.
struct Params
{
	// memoryPressure (+0x20..+0x30)
	float mitigationEnterHeadroomMb = 32.0f;
	float mitigationLeaveHeadroomMb = 48.0f;
	float criticalEnterHeadroomMb = 8.0f;
	float criticalLeaveHeadroomMb = 24.0f;
	float projectedPressureThrottle = 0.75f;

	// worldDistance (+0x34..+0x48)
	float mitigationDistanceScale = 0.75f;
	float criticalDistanceScale = 0.60f;
	float distanceScaleChangePerSecond = 0.25f;
	float mitigationDelayMs = 1000.0f;
	float recoveryDelayMs = 3000.0f;
	float farPlaneMarginScale = 1.05f;

	// networkVehicles (+0x4c..+0x7c)
	float vehicleUnloadDistance = 35.0f;
	float vehicleReloadDistance = 25.0f;
	float aggressiveVehicleUnloadDistance = 20.0f;
	float aggressiveVehicleReloadDistance = 15.0f;
	float aggressiveVehicleProtectedDistance = 20.0f;
	float aggressiveVehicleUpdateIntervalMs = 250.0f;
	int vehicleBatchMin = 2;
	int vehicleBatchMax = 8;
	float vehicleBatchDensityDivisor = 16.0f;
	int vehicleRequestsPerFrame = 3;
	int constrainedVehicleRequestsPerFrame = 1;
	float vehicleModelTimeoutMs = 3000.0f;
	int vehicleModelMaxRequests = 50;

	// remotePlayerAppearance (+0x80..+0xbc)
	float accessoryUnloadDistance = 20.0f;
	float accessoryReloadDistance = 15.0f;
	float skinUnloadDistance = 30.0f;
	float skinReloadDistance = 25.0f;
	float appearancePreserveDistance = 30.0f;
	float accessoryProtectedDistance = 12.0f;
	float skinProtectedDistance = 20.0f;
	float fallbackSkinProjectedPressure = 0.98f;
	float severeProjectedPressure = 1.0f;
	float aggressivePlayerUpdateIntervalMs = 250.0f;
	int playerBatchMin = 2;
	int playerBatchMax = 8;
	float playerBatchDensityDivisor = 12.0f;
	int minimumPreservedPlayers = 8;
	int preservedPlayersDivisor = 3;
	int severePreservedPlayersDivisor = 4;

	// asyncModelRequests (+0xc0..+0xdc)
	int asyncRequestsPerFrame = 5;
	int constrainedAsyncRequestsPerFrame = 2;
	float asyncModelTimeoutMs = 10000.0f;
	int asyncModelMaxRequests = 150;
	float retryBaseDelayMs = 100.0f;
	float retryMaxDelayMs = 2000.0f;
	int retryMaxExponent = 5;
	int cleanupChecksPerFrame = 100;
};

// Reads one section of the config. Missing keys leave the default in place;
// non-numeric or non-finite values are ignored, so a corrupt file cannot
// install NaN into the frame-time maths.
class ConfigReader
{
public:
	explicit ConfigReader(Params &out) : p(out), applied(0) {}

	int Applied(void) const { return applied; }

	// Returns true when the section existed (even if every key was invalid).
	bool Section(const minijson::Value *root, const char *name)
	{
		if(root == nil)
			return false;
		const minijson::Value *section = root->Find(name);
		if(section == nil || !section->IsObject())
			return false;
		return true;
	}

	void Float(const minijson::Value *root, const char *section, const char *key, float &field)
	{
		const minijson::Value *v = Lookup(root, section, key);
		if(v == nil || !v->IsNumber() || !IsFinite((float)v->num))
			return;
		field = (float)v->num;
		applied++;
	}

	void Int(const minijson::Value *root, const char *section, const char *key, int &field)
	{
		const minijson::Value *v = Lookup(root, section, key);
		if(v == nil || !v->IsNumber() || !IsFinite((float)v->num))
			return;
		field = (int)lround(v->num);
		applied++;
	}

	static bool IsFinite(float f)
	{
		// NaN fails both comparisons; tiny magnitudes below FLT_MIN are still
		// meaningful here, so this check is deliberately comparison based.
		return f == f && f <= 3.4e38f && f >= -3.4e38f;
	}

private:
	const minijson::Value *Lookup(const minijson::Value *root, const char *section, const char *key) const
	{
		if(root == nil || !root->IsObject())
			return nil;
		const minijson::Value *s = root->Find(section);
		if(s == nil || !s->IsObject())
			return nil;
		return s->Find(key);
	}

	Params &p;
	int applied;
};

inline float
ClampF(float v, float lo, float hi)
{
	if(v < lo)
		return lo;
	if(v > hi)
		return hi;
	return v;
}

// Clamps a config into the ranges the client itself enforces (chapter 8: the
// validator clamps distances and scales, and rejects a zero-length distance
// scale that would stop the world from converging).
inline void
Sanitize(Params &p)
{
	p.mitigationEnterHeadroomMb = ClampF(p.mitigationEnterHeadroomMb, 0.0f, 4096.0f);
	p.mitigationLeaveHeadroomMb = ClampF(p.mitigationLeaveHeadroomMb, 0.0f, 4096.0f);
	p.criticalEnterHeadroomMb = ClampF(p.criticalEnterHeadroomMb, 0.0f, 4096.0f);
	p.criticalLeaveHeadroomMb = ClampF(p.criticalLeaveHeadroomMb, 0.0f, 4096.0f);
	p.projectedPressureThrottle = ClampF(p.projectedPressureThrottle, 0.0f, 2.0f);
	// The leave thresholds must be above the enter thresholds or the machine
	// would flip every frame at one threshold (that is the whole point of the
	// hysteresis pairs in the client).
	if(p.mitigationLeaveHeadroomMb <= p.mitigationEnterHeadroomMb)
		p.mitigationLeaveHeadroomMb = p.mitigationEnterHeadroomMb + 16.0f;
	if(p.criticalLeaveHeadroomMb <= p.criticalEnterHeadroomMb)
		p.criticalLeaveHeadroomMb = p.criticalEnterHeadroomMb + 16.0f;
	if(p.criticalLeaveHeadroomMb > p.mitigationLeaveHeadroomMb)
		p.criticalLeaveHeadroomMb = p.mitigationLeaveHeadroomMb;

	p.mitigationDistanceScale = ClampF(p.mitigationDistanceScale, 0.10f, 1.0f);
	p.criticalDistanceScale = ClampF(p.criticalDistanceScale, 0.10f, 1.0f);
	if(p.criticalDistanceScale > p.mitigationDistanceScale)
		p.criticalDistanceScale = p.mitigationDistanceScale;
	p.distanceScaleChangePerSecond = ClampF(p.distanceScaleChangePerSecond, 0.01f, 8.0f);
	p.mitigationDelayMs = ClampF(p.mitigationDelayMs, 0.0f, 60000.0f);
	p.recoveryDelayMs = ClampF(p.recoveryDelayMs, 0.0f, 60000.0f);
	// Not clamped up to 1.0: the client only checks that the margin is a
	// positive scale, and a margin below 1 is exactly the GPU-saving knob
	// chapter 3.2 tells mods to turn. Clamping it to 1.0 silently discarded
	// every such setting.
	p.farPlaneMarginScale = ClampF(p.farPlaneMarginScale, 0.10f, 4.0f);

	p.vehicleUnloadDistance = ClampF(p.vehicleUnloadDistance, 5.0f, 500.0f);
	p.vehicleReloadDistance = ClampF(p.vehicleReloadDistance, 1.0f, p.vehicleUnloadDistance);
	p.aggressiveVehicleUnloadDistance = ClampF(p.aggressiveVehicleUnloadDistance, 5.0f, 500.0f);
	p.aggressiveVehicleReloadDistance = ClampF(p.aggressiveVehicleReloadDistance, 1.0f,
	                                           p.aggressiveVehicleUnloadDistance);
	p.aggressiveVehicleProtectedDistance = ClampF(p.aggressiveVehicleProtectedDistance, 1.0f, 500.0f);
	p.aggressiveVehicleUpdateIntervalMs = ClampF(p.aggressiveVehicleUpdateIntervalMs, 16.0f, 5000.0f);
	p.vehicleBatchMin = (int)ClampF((float)p.vehicleBatchMin, 1.0f, 64.0f);
	p.vehicleBatchMax = (int)ClampF((float)p.vehicleBatchMax, (float)p.vehicleBatchMin, 256.0f);
	p.vehicleBatchDensityDivisor = ClampF(p.vehicleBatchDensityDivisor, 1.0f, 1024.0f);
	p.vehicleRequestsPerFrame = (int)ClampF((float)p.vehicleRequestsPerFrame, 1.0f, 64.0f);
	p.constrainedVehicleRequestsPerFrame =
	    (int)ClampF((float)p.constrainedVehicleRequestsPerFrame, 1.0f,
	                (float)p.vehicleRequestsPerFrame);
	p.vehicleModelTimeoutMs = ClampF(p.vehicleModelTimeoutMs, 100.0f, 120000.0f);
	p.vehicleModelMaxRequests = (int)ClampF((float)p.vehicleModelMaxRequests, 1.0f, 4096.0f);

	p.accessoryUnloadDistance = ClampF(p.accessoryUnloadDistance, 1.0f, 500.0f);
	p.accessoryReloadDistance = ClampF(p.accessoryReloadDistance, 0.0f, p.accessoryUnloadDistance);
	p.skinUnloadDistance = ClampF(p.skinUnloadDistance, 1.0f, 500.0f);
	p.skinReloadDistance = ClampF(p.skinReloadDistance, 0.0f, p.skinUnloadDistance);
	p.appearancePreserveDistance = ClampF(p.appearancePreserveDistance, 0.0f, 500.0f);
	p.accessoryProtectedDistance = ClampF(p.accessoryProtectedDistance, 0.0f, 500.0f);
	p.skinProtectedDistance = ClampF(p.skinProtectedDistance, 0.0f, 500.0f);
	p.fallbackSkinProjectedPressure = ClampF(p.fallbackSkinProjectedPressure, 0.0f, 2.0f);
	p.severeProjectedPressure = ClampF(p.severeProjectedPressure, 0.0f, 2.0f);
	p.aggressivePlayerUpdateIntervalMs = ClampF(p.aggressivePlayerUpdateIntervalMs, 16.0f, 5000.0f);
	p.playerBatchMin = (int)ClampF((float)p.playerBatchMin, 1.0f, 64.0f);
	p.playerBatchMax = (int)ClampF((float)p.playerBatchMax, (float)p.playerBatchMin, 256.0f);
	p.playerBatchDensityDivisor = ClampF(p.playerBatchDensityDivisor, 1.0f, 1024.0f);
	p.minimumPreservedPlayers = (int)ClampF((float)p.minimumPreservedPlayers, 0.0f, 512.0f);
	p.preservedPlayersDivisor = (int)ClampF((float)p.preservedPlayersDivisor, 1.0f, 64.0f);
	p.severePreservedPlayersDivisor = (int)ClampF((float)p.severePreservedPlayersDivisor, 1.0f, 64.0f);

	p.asyncRequestsPerFrame = (int)ClampF((float)p.asyncRequestsPerFrame, 1.0f, 128.0f);
	p.constrainedAsyncRequestsPerFrame = (int)ClampF((float)p.constrainedAsyncRequestsPerFrame,
	                                                 1.0f, (float)p.asyncRequestsPerFrame);
	p.asyncModelTimeoutMs = ClampF(p.asyncModelTimeoutMs, 100.0f, 120000.0f);
	p.asyncModelMaxRequests = (int)ClampF((float)p.asyncModelMaxRequests, 1.0f, 8192.0f);
	p.retryBaseDelayMs = ClampF(p.retryBaseDelayMs, 1.0f, 60000.0f);
	p.retryMaxDelayMs = ClampF(p.retryMaxDelayMs, p.retryBaseDelayMs, 60000.0f);
	p.retryMaxExponent = (int)ClampF((float)p.retryMaxExponent, 0.0f, 16.0f);
	p.cleanupChecksPerFrame = (int)ClampF((float)p.cleanupChecksPerFrame, 1.0f, 4096.0f);
}

// Parses the five sections. Returns the number of applied keys, or -1 when the
// document is not valid json at all.
inline int
LoadConfig(const char *data, size_t size, Params &out, std::string &error)
{
	minijson::Value root;
	if(!minijson::Parse(data, size, root, error))
		return -1;
	if(!root.IsObject()) {
		error = "the top level is not an object";
		return -1;
	}
	ConfigReader reader(out);	// writes straight into `out`, defaults survive

	// memoryPressure
	reader.Float(&root, "memoryPressure", "mitigationEnterHeadroomMb", out.mitigationEnterHeadroomMb);
	reader.Float(&root, "memoryPressure", "mitigationLeaveHeadroomMb", out.mitigationLeaveHeadroomMb);
	reader.Float(&root, "memoryPressure", "criticalEnterHeadroomMb", out.criticalEnterHeadroomMb);
	reader.Float(&root, "memoryPressure", "criticalLeaveHeadroomMb", out.criticalLeaveHeadroomMb);
	reader.Float(&root, "memoryPressure", "projectedPressureThrottle", out.projectedPressureThrottle);

	// worldDistance
	reader.Float(&root, "worldDistance", "mitigationDistanceScale", out.mitigationDistanceScale);
	reader.Float(&root, "worldDistance", "criticalDistanceScale", out.criticalDistanceScale);
	reader.Float(&root, "worldDistance", "distanceScaleChangePerSecond", out.distanceScaleChangePerSecond);
	reader.Float(&root, "worldDistance", "mitigationDelayMs", out.mitigationDelayMs);
	reader.Float(&root, "worldDistance", "recoveryDelayMs", out.recoveryDelayMs);
	reader.Float(&root, "worldDistance", "farPlaneMarginScale", out.farPlaneMarginScale);

	// networkVehicles
	reader.Float(&root, "networkVehicles", "vehicleUnloadDistance", out.vehicleUnloadDistance);
	reader.Float(&root, "networkVehicles", "vehicleReloadDistance", out.vehicleReloadDistance);
	reader.Float(&root, "networkVehicles", "aggressiveVehicleUnloadDistance", out.aggressiveVehicleUnloadDistance);
	reader.Float(&root, "networkVehicles", "aggressiveVehicleReloadDistance", out.aggressiveVehicleReloadDistance);
	reader.Float(&root, "networkVehicles", "aggressiveVehicleProtectedDistance", out.aggressiveVehicleProtectedDistance);
	reader.Float(&root, "networkVehicles", "aggressiveVehicleUpdateIntervalMs", out.aggressiveVehicleUpdateIntervalMs);
	reader.Int(&root, "networkVehicles", "vehicleBatchMin", out.vehicleBatchMin);
	reader.Int(&root, "networkVehicles", "vehicleBatchMax", out.vehicleBatchMax);
	reader.Float(&root, "networkVehicles", "vehicleBatchDensityDivisor", out.vehicleBatchDensityDivisor);
	reader.Int(&root, "networkVehicles", "vehicleRequestsPerFrame", out.vehicleRequestsPerFrame);
	reader.Int(&root, "networkVehicles", "constrainedVehicleRequestsPerFrame", out.constrainedVehicleRequestsPerFrame);
	reader.Float(&root, "networkVehicles", "vehicleModelTimeoutMs", out.vehicleModelTimeoutMs);
	reader.Int(&root, "networkVehicles", "vehicleModelMaxRequests", out.vehicleModelMaxRequests);

	// remotePlayerAppearance
	reader.Float(&root, "remotePlayerAppearance", "accessoryUnloadDistance", out.accessoryUnloadDistance);
	reader.Float(&root, "remotePlayerAppearance", "accessoryReloadDistance", out.accessoryReloadDistance);
	reader.Float(&root, "remotePlayerAppearance", "skinUnloadDistance", out.skinUnloadDistance);
	reader.Float(&root, "remotePlayerAppearance", "skinReloadDistance", out.skinReloadDistance);
	reader.Float(&root, "remotePlayerAppearance", "appearancePreserveDistance", out.appearancePreserveDistance);
	reader.Float(&root, "remotePlayerAppearance", "accessoryProtectedDistance", out.accessoryProtectedDistance);
	reader.Float(&root, "remotePlayerAppearance", "skinProtectedDistance", out.skinProtectedDistance);
	reader.Float(&root, "remotePlayerAppearance", "fallbackSkinProjectedPressure", out.fallbackSkinProjectedPressure);
	reader.Float(&root, "remotePlayerAppearance", "severeProjectedPressure", out.severeProjectedPressure);
	reader.Float(&root, "remotePlayerAppearance", "aggressivePlayerUpdateIntervalMs", out.aggressivePlayerUpdateIntervalMs);
	reader.Int(&root, "remotePlayerAppearance", "playerBatchMin", out.playerBatchMin);
	reader.Int(&root, "remotePlayerAppearance", "playerBatchMax", out.playerBatchMax);
	reader.Float(&root, "remotePlayerAppearance", "playerBatchDensityDivisor", out.playerBatchDensityDivisor);
	reader.Int(&root, "remotePlayerAppearance", "minimumPreservedPlayers", out.minimumPreservedPlayers);
	reader.Int(&root, "remotePlayerAppearance", "preservedPlayersDivisor", out.preservedPlayersDivisor);
	reader.Int(&root, "remotePlayerAppearance", "severePreservedPlayersDivisor", out.severePreservedPlayersDivisor);

	// asyncModelRequests
	reader.Int(&root, "asyncModelRequests", "asyncRequestsPerFrame", out.asyncRequestsPerFrame);
	reader.Int(&root, "asyncModelRequests", "constrainedAsyncRequestsPerFrame", out.constrainedAsyncRequestsPerFrame);
	reader.Float(&root, "asyncModelRequests", "asyncModelTimeoutMs", out.asyncModelTimeoutMs);
	reader.Int(&root, "asyncModelRequests", "asyncModelMaxRequests", out.asyncModelMaxRequests);
	reader.Float(&root, "asyncModelRequests", "retryBaseDelayMs", out.retryBaseDelayMs);
	reader.Float(&root, "asyncModelRequests", "retryMaxDelayMs", out.retryMaxDelayMs);
	reader.Int(&root, "asyncModelRequests", "retryMaxExponent", out.retryMaxExponent);
	reader.Int(&root, "asyncModelRequests", "cleanupChecksPerFrame", out.cleanupChecksPerFrame);

	Sanitize(out);
	error.clear();
	return reader.Applied();
}

// ---------------------------------------------------------------------------
// memoryBudgetScaling: the client picks the budget row by installed RAM
// (§8.5: <6 GB -> 180 MB, 6 -> 256, 8 -> 320, >=12 -> 384) and the validator
// keeps every value inside 64..4096 MB.
// ---------------------------------------------------------------------------

struct BudgetRow
{
	uint32_t minimumRamMb;
	uint32_t budgetMb;
};

inline uint32_t
BudgetForRamMb(uint32_t ramMb, const BudgetRow *rows, size_t count)
{
	uint32_t budget = 0;
	for(size_t i = 0; i < count; i++) {
		if(ramMb >= rows[i].minimumRamMb)
			budget = rows[i].budgetMb;
		else
			break;
	}
	if(budget == 0 && count > 0)
		budget = rows[0].budgetMb;
	if(budget < 64)
		budget = 64;
	if(budget > 4096)
		budget = 4096;
	return budget;
}

inline const BudgetRow *DefaultBudgetTable(size_t &count)
{
	static const BudgetRow rows[] = {
		{ 0, 180 },      // < 6 GB
		{ 6 * 1024, 256 },
		{ 8 * 1024, 320 },
		{ 12 * 1024, 384 },
	};
	count = sizeof(rows) / sizeof(rows[0]);
	return rows;
}

// Parses one "Name:      1234 kB" line out of /proc/meminfo.
inline bool
MemInfoValueKb(const char *text, const char *name, size_t &outKb)
{
	if(text == nil || name == nil)
		return false;
	size_t nameLen = strlen(name);
	const char *p = text;
	while(*p) {
		while(*p == '\n' || *p == ' ' || *p == '\t')
			p++;
		if(strncmp(p, name, nameLen) == 0) {
			const char *q = p + nameLen;
			while(*q == ' ' || *q == '\t' || *q == ':')
				q++;
			unsigned long long v = 0;
			bool any = false;
			while(*q >= '0' && *q <= '9') {
				v = v * 10 + (unsigned)(*q - '0');
				q++;
				any = true;
			}
			if(any) {
				outKb = (size_t)v;
				return true;
			}
			return false;
		}
		while(*p && *p != '\n')
			p++;
	}
	return false;
}

// ---------------------------------------------------------------------------
// the mode machine (§9.1 - §9.3)
// ---------------------------------------------------------------------------

class Controller
{
public:
	Controller() { Reset(); }

	void SetParams(const Params &p) { params = p; }

	// Budget in bytes; the client derives it once at streaming init from RAM.
	void SetBudgetBytes(size_t bytes)
	{
		// 64..4096 MB, like the client's validator.
		size_t lo = (size_t)64 << 20;
		size_t hi = (size_t)4096 << 20;
		if(bytes < lo)
			bytes = lo;
		if(bytes > hi)
			bytes = hi;
		budgetBytes = bytes;
	}

	// `usedBytes` must be the pool usage measured the way the client measures
	// it: relative to the streaming cap, i.e. the allocator base plus what is
	// actually streamed. `baseBytes` is that reservation (chapter 9 §9.1). The
	// bare streamed size is not a valid input - this build has no custom
	// allocator, so it stays far below the RAM-derived budget and pressure
	// never crosses the thresholds.
	void Update(uint32_t nowMs, size_t usedBytes, size_t baseBytes, float dtSeconds)
	{
		if(dtSeconds <= 0.0f)
			dtSeconds = 0.0f;
		if(dtSeconds > 0.25f)
			dtSeconds = 0.25f;

		size_t limit = budgetBytes;
		size_t base = baseBytes < limit ? baseBytes : 0;
		size_t span = limit - base;
		size_t used = usedBytes > base ? usedBytes - base : 0;

		// saturation (used - base) / (limit - base)
		pressure = span ? (float)used / (float)span : 1.0f;
		if(pressure > 1.0f)
			pressure = 1.0f;

		// free headroom in bytes: the client compares it against the Mb
		// thresholds converted with a 20-bit fixed point shift.
		freeBytes = usedBytes < limit ? limit - usedBytes : 0;
		freeHeadroomMb = (float)freeBytes / (float)((size_t)1 << 20);

		UpdateModes();
		UpdateTargetScale(nowMs);
		UpdateSmoothedScale(dtSeconds);
	}

	void Reset(void)
	{
		params = Params();
		budgetBytes = (size_t)256 << 20;
		pressure = 0.0f;
		freeBytes = budgetBytes;
		freeHeadroomMb = 256.0f;
		mitigation = false;
		critical = false;
		mitigationTimerStart = 0;
		mitigationTimerRunning = false;
		recoveryTimerStart = 0;
		recoveryTimerRunning = false;
		targetScale = 1.0f;
		smoothedScale = 1.0f;
		lastLogPressure = -1.0f;
		lastLogMode = -1;
	}

	// --- outputs ---
	const Params &GetParams(void) const { return params; }
	float Pressure(void) const { return pressure; }
	float FreeHeadroomMb(void) const { return freeHeadroomMb; }
	bool MitigationActive(void) const { return mitigation; }
	bool CriticalActive(void) const { return critical; }
	// The scale the machine is heading for (1.0 / mitigation / critical).
	float TargetDistanceScale(void) const { return targetScale; }
	// The smoothed scale the renderer should use this frame.
	float DistanceScale(void) const { return smoothedScale; }
	size_t BudgetBytes(void) const { return budgetBytes; }

	// §9.4: the same branch the client uses to pick the per-frame quota.
	int VehicleRequestsPerFrame(void) const
	{
		if(params.mitigationEnterHeadroomMb <= 0.0f)
			return params.constrainedVehicleRequestsPerFrame;
		if(pressure >= params.projectedPressureThrottle)
			return params.constrainedVehicleRequestsPerFrame;
		return params.vehicleRequestsPerFrame;
	}
	int AsyncRequestsPerFrame(void) const
	{
		if(params.mitigationEnterHeadroomMb <= 0.0f)
			return params.constrainedAsyncRequestsPerFrame;
		if(pressure >= params.projectedPressureThrottle)
			return params.constrainedAsyncRequestsPerFrame;
		return params.asyncRequestsPerFrame;
	}
	// How many models the vehicle ring buffer keeps.
	//
	// This is a reVC-side policy, not a ported number: the client's
	// preservedPlayers* fields size a *multiplayer* remote-player set, and the
	// single-player ring buffer has no equivalent (its divisor would cut 25
	// loaded cars to 8 in soft mode, which shows up as traffic popping in).
	// Keep a mild, monotonic reduction instead: 80 % under mitigation, 60 %
	// under critical, never below eight cars.
	//
	// The clamp is a floor, not a target: with the normal target at 25 slots
	// the three modes give 25 / 20 / 15, so the reduction is real. It only
	// collapses to a single value for a caller that already asks for eight.
	int VehicleRingTarget(int normalSlots) const
	{
		if(normalSlots <= 0)
			return 0;
		int target = normalSlots;
		if(critical)
			target = normalSlots * 3 / 5;
		else if(mitigation)
			target = normalSlots * 4 / 5;
		if(target < 8)
			target = 8;
		if(target > normalSlots)
			target = normalSlots;
		return target;
	}
	// True when the appearance budget should fall back to a cheap skin.
	bool SkinFallbackWanted(void) const
	{
		return pressure >= params.fallbackSkinProjectedPressure || critical;
	}

	// Bumped whenever the mode or the quota changed, so the caller can log or
	// re-evaluate cached decisions without comparing floats every frame.
	uint32_t ModeRevision(void) const { return modeRevision; }
	uint32_t QuotaRevision(void) const { return quotaRevision; }

private:
	Params params;
	size_t budgetBytes;
	float pressure;
	size_t freeBytes;
	float freeHeadroomMb;
	bool mitigation;
	bool critical;
	uint32_t mitigationTimerStart;
	bool mitigationTimerRunning;
	uint32_t recoveryTimerStart;
	bool recoveryTimerRunning;
	float targetScale;
	float smoothedScale;
	float lastLogPressure;
	int lastLogMode;
	uint32_t modeRevision = 0;
	uint32_t quotaRevision = 0;

	void SetMitigation(bool on)
	{
		if(mitigation == on)
			return;
		mitigation = on;
		modeRevision++;
	}
	void SetCritical(bool on)
	{
		if(critical == on)
			return;
		critical = on;
		modeRevision++;
	}

	void UpdateModes(void)
	{
		const float MB20 = (float)((size_t)1 << 20);

		// --- mitigation with hysteresis (enter 32 / leave 48 by default) ---
		if(params.mitigationEnterHeadroomMb <= 0.0f) {
			// disabled: the client forces full distance
			SetMitigation(false);
			SetCritical(false);
			mitigationTimerRunning = false;
			recoveryTimerRunning = false;
			return;
		}
		float enterBytes = params.mitigationEnterHeadroomMb * MB20;
		float leaveBytes = params.mitigationLeaveHeadroomMb * MB20;
		float freeB = (float)((uint64_t)freeBytes);
		if(freeB <= enterBytes) {
			SetMitigation(true);
		} else if(freeB >= leaveBytes) {
			SetMitigation(false);
		}	// between the thresholds the previous state survives: that is the point

		// --- critical with hysteresis (enter 8 / leave 24 by default) ---
		float critEnter = params.criticalEnterHeadroomMb * MB20;
		float critLeave = params.criticalLeaveHeadroomMb * MB20;
		if(freeB <= critEnter)
			SetCritical(true);
		else if(freeB >= critLeave)
			SetCritical(false);
	}

	void UpdateTargetScale(uint32_t nowMs)
	{
		int previousQuota = VehicleRequestsPerFrame();
		if(critical) {
			// critical applies at once, no delay (the client does the same)
			targetScale = params.criticalDistanceScale;
			mitigationTimerRunning = false;
			recoveryTimerRunning = false;
		} else if(mitigation) {
			recoveryTimerRunning = false;
			if(!mitigationTimerRunning) {
				mitigationTimerRunning = true;
				mitigationTimerStart = nowMs;
			}
			if((float)(uint32_t)(nowMs - mitigationTimerStart) >= params.mitigationDelayMs)
				targetScale = params.mitigationDistanceScale;
		} else {
			mitigationTimerRunning = false;
			if(!recoveryTimerRunning) {
				recoveryTimerRunning = true;
				recoveryTimerStart = nowMs;
			}
			if((float)(uint32_t)(nowMs - recoveryTimerStart) >= params.recoveryDelayMs)
				targetScale = 1.0f;
		}
		if(previousQuota != VehicleRequestsPerFrame())
			quotaRevision++;
	}

	void UpdateSmoothedScale(float dtSeconds)
	{
		float maxStep = params.distanceScaleChangePerSecond * dtSeconds;
		if(smoothedScale < targetScale) {
			smoothedScale += maxStep;
			if(smoothedScale > targetScale)
				smoothedScale = targetScale;
		} else if(smoothedScale > targetScale) {
			smoothedScale -= maxStep;
			if(smoothedScale < targetScale)
				smoothedScale = targetScale;
		}
	}
};

} // namespace optprofile

#endif // REVC_CUSTOM_OPTIMIZATIONPROFILE_H
