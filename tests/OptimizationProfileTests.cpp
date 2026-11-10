#include "OptimizationProfile.h"
#include "Optimization.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace optprofile;

static void
TestDefaults(void)
{
	Params p;
	// §8.3: the whole table, checked value by value.
	assert(p.mitigationEnterHeadroomMb == 32.0f);
	assert(p.mitigationLeaveHeadroomMb == 48.0f);
	assert(p.criticalEnterHeadroomMb == 8.0f);
	assert(p.criticalLeaveHeadroomMb == 24.0f);
	assert(p.projectedPressureThrottle == 0.75f);
	assert(p.mitigationDistanceScale == 0.75f);
	assert(p.criticalDistanceScale == 0.60f);
	assert(p.distanceScaleChangePerSecond == 0.25f);
	assert(p.mitigationDelayMs == 1000.0f);
	assert(p.recoveryDelayMs == 3000.0f);
	assert(p.farPlaneMarginScale == 1.05f);
	assert(p.vehicleUnloadDistance == 35.0f);
	assert(p.vehicleReloadDistance == 25.0f);
	assert(p.aggressiveVehicleUnloadDistance == 20.0f);
	assert(p.aggressiveVehicleReloadDistance == 15.0f);
	assert(p.aggressiveVehicleProtectedDistance == 20.0f);
	assert(p.aggressiveVehicleUpdateIntervalMs == 250.0f);
	assert(p.vehicleBatchMin == 2 && p.vehicleBatchMax == 8);
	assert(p.vehicleBatchDensityDivisor == 16.0f);
	assert(p.vehicleRequestsPerFrame == 3);
	assert(p.constrainedVehicleRequestsPerFrame == 1);
	assert(p.vehicleModelTimeoutMs == 3000.0f);
	assert(p.vehicleModelMaxRequests == 50);
	assert(p.accessoryUnloadDistance == 20.0f && p.accessoryReloadDistance == 15.0f);
	assert(p.skinUnloadDistance == 30.0f && p.skinReloadDistance == 25.0f);
	assert(p.appearancePreserveDistance == 30.0f);
	assert(p.accessoryProtectedDistance == 12.0f && p.skinProtectedDistance == 20.0f);
	assert(p.fallbackSkinProjectedPressure == 0.98f);
	assert(p.severeProjectedPressure == 1.0f);
	assert(p.aggressivePlayerUpdateIntervalMs == 250.0f);
	assert(p.playerBatchMin == 2 && p.playerBatchMax == 8);
	assert(p.playerBatchDensityDivisor == 12.0f);
	assert(p.minimumPreservedPlayers == 8);
	assert(p.preservedPlayersDivisor == 3 && p.severePreservedPlayersDivisor == 4);
	assert(p.asyncRequestsPerFrame == 5 && p.constrainedAsyncRequestsPerFrame == 2);
	assert(p.asyncModelTimeoutMs == 10000.0f);
	assert(p.asyncModelMaxRequests == 150);
	assert(p.retryBaseDelayMs == 100.0f && p.retryMaxDelayMs == 2000.0f);
	assert(p.retryMaxExponent == 5);
	assert(p.cleanupChecksPerFrame == 100);

	// Sanitize must not disturb a default table.
	Params q;
	Sanitize(q);
	assert(q.mitigationDistanceScale == 0.75f);
	assert(q.criticalLeaveHeadroomMb == 24.0f);
	printf("defaults ok\n");
}

static void
TestPartialConfig(void)
{
	// Only mitigationEnterHeadroomMb is present: everything else keeps its
	// default, exactly like the client's optional sections (§2.3).
	const char *json = "{ \"memoryPressure\": { \"mitigationEnterHeadroomMb\": 64 } }";
	Params p;
	std::string err;
	int applied = LoadConfig(json, strlen(json), p, err);
	assert(applied == 1);
	assert(p.mitigationEnterHeadroomMb == 64.0f);
	assert(p.mitigationLeaveHeadroomMb == 64.0f + 16.0f); // pushed out of the enter band
	assert(p.criticalEnterHeadroomMb == 8.0f);
	assert(p.criticalLeaveHeadroomMb == 24.0f);
	assert(p.mitigationDistanceScale == 0.75f);

	// Full five sections.
	const char *full =
	    "{\n"
	    "  // comments are tolerated\n"
	    "  \"memoryPressure\": { \"mitigationEnterHeadroomMb\": 40, \"mitigationLeaveHeadroomMb\": 60,\n"
	    "                       \"criticalEnterHeadroomMb\": 10, \"criticalLeaveHeadroomMb\": 30,\n"
	    "                       \"projectedPressureThrottle\": 0.8 },\n"
	    "  \"worldDistance\": { \"mitigationDistanceScale\": 0.7, \"criticalDistanceScale\": 0.5,\n"
	    "                      \"distanceScaleChangePerSecond\": 0.5, \"mitigationDelayMs\": 500,\n"
	    "                      \"recoveryDelayMs\": 1500, \"farPlaneMarginScale\": 1.1 },\n"
	    "  \"networkVehicles\": { \"vehicleUnloadDistance\": 50, \"vehicleRequestsPerFrame\": 4,\n"
	    "                        \"constrainedVehicleRequestsPerFrame\": 2, \"vehicleBatchMax\": 12 },\n"
	    "  \"remotePlayerAppearance\": { \"minimumPreservedPlayers\": 4, \"skinProtectedDistance\": 25 },\n"
	    "  \"asyncModelRequests\": { \"asyncRequestsPerFrame\": 6, \"cleanupChecksPerFrame\": 50 }\n"
	    "}\n";
	Params f;
	applied = LoadConfig(full, strlen(full), f, err);
	assert(applied == 5 + 6 + 4 + 2 + 2);
	assert(f.mitigationEnterHeadroomMb == 40.0f && f.mitigationLeaveHeadroomMb == 60.0f);
	assert(f.criticalEnterHeadroomMb == 10.0f && f.criticalLeaveHeadroomMb == 30.0f);
	assert(f.projectedPressureThrottle == 0.8f);
	assert(f.mitigationDistanceScale == 0.7f && f.criticalDistanceScale == 0.5f);
	assert(f.distanceScaleChangePerSecond == 0.5f);
	assert(f.mitigationDelayMs == 500.0f && f.recoveryDelayMs == 1500.0f);
	assert(f.farPlaneMarginScale == 1.1f);
	assert(f.vehicleUnloadDistance == 50.0f && f.vehicleReloadDistance == 25.0f); // unset keeps default
	assert(f.vehicleRequestsPerFrame == 4 && f.constrainedVehicleRequestsPerFrame == 2);
	assert(f.vehicleBatchMax == 12 && f.vehicleBatchMin == 2);
	assert(f.minimumPreservedPlayers == 4 && f.skinProtectedDistance == 25.0f);
	assert(f.asyncRequestsPerFrame == 6 && f.cleanupChecksPerFrame == 50);
	printf("partial config ok (%d keys)\n", applied);
}

static void
TestBadConfig(void)
{
	Params p;
	std::string err;
	// Not json at all.
	assert(LoadConfig("not json", 8, p, err) == -1);
	assert(!err.empty());
	// A truncated document must fail rather than half-apply.
	const char *trunc = "{ \"worldDistance\": { \"mitigationDistanceScale\": 0.5";
	assert(LoadConfig(trunc, strlen(trunc), p, err) == -1);

	// A top-level array is not a config.
	assert(LoadConfig("[1,2,3]", 7, p, err) == -1);

	// Non-finite and non-numeric values are ignored, not installed.
	const char *poison =
	    "{ \"worldDistance\": { \"mitigationDistanceScale\": \"half\","
	    "                      \"criticalDistanceScale\": 1e40 } }";
	Params q;
	assert(LoadConfig(poison, strlen(poison), q, err) >= 0);
	assert(q.mitigationDistanceScale == 0.75f);   // defaults survive
	assert(q.criticalDistanceScale == 0.60f);

	// Out-of-range values are clamped by Sanitize: a zero distance scale would
	// freeze the world at zero draw distance.
	const char *wild =
	    "{ \"worldDistance\": { \"mitigationDistanceScale\": 0, \"criticalDistanceScale\": 5,"
	    "                      \"farPlaneMarginScale\": 0.1, \"distanceScaleChangePerSecond\": 0 },"
	    "  \"networkVehicles\": { \"vehicleUnloadDistance\": 10, \"vehicleReloadDistance\": 100 } }";
	Params w;
	assert(LoadConfig(wild, strlen(wild), w, err) > 0);
	assert(w.mitigationDistanceScale >= 0.10f);
	assert(w.criticalDistanceScale <= w.mitigationDistanceScale);
	// 0.1 is a legitimate sub-unit margin (§3.2 turns it for GPU savings), so
	// it must survive; only a non-positive value is rejected.
	assert(w.farPlaneMarginScale > 0.099f && w.farPlaneMarginScale < 0.101f);
	assert(w.distanceScaleChangePerSecond > 0.0f);
	assert(w.vehicleReloadDistance <= w.vehicleUnloadDistance);

	// Deeply nested garbage must be rejected, not overflow the stack.
	std::string deep;
	for(int i = 0; i < 400; i++)
		deep += "[";
	Params d;
	assert(LoadConfig(deep.data(), deep.size(), d, err) == -1);
	printf("bad config ok\n");
}

static void
TestBudget(void)
{
	size_t n;
	const BudgetRow *rows = DefaultBudgetTable(n);
	assert(n == 4);
	assert(BudgetForRamMb(3 * 1024, rows, n) == 180);
	assert(BudgetForRamMb(6 * 1024, rows, n) == 256);
	assert(BudgetForRamMb(7 * 1024, rows, n) == 256);
	assert(BudgetForRamMb(8 * 1024, rows, n) == 320);
	assert(BudgetForRamMb(12 * 1024, rows, n) == 384);
	assert(BudgetForRamMb(64 * 1024, rows, n) == 384);

	// The validator corridor is 64..4096 MB.
	BudgetRow small[] = { { 0, 8 } };
	BudgetRow huge[] = { { 0, 999999 } };
	assert(BudgetForRamMb(1024, small, 1) == 64);
	assert(BudgetForRamMb(1024, huge, 1) == 4096);

	// /proc/meminfo parsing.
	const char *mi =
	    "MemTotal:        7800000 kB\n"
	    "MemFree:          400000 kB\n"
	    "MemAvailable:    2345678 kB\n";
	size_t kb = 0;
	assert(MemInfoValueKb(mi, "MemTotal:", kb) && kb == 7800000);
	assert(MemInfoValueKb(mi, "MemAvailable:", kb) && kb == 2345678);
	assert(!MemInfoValueKb(mi, "SwapTotal:", kb));
	assert(!MemInfoValueKb(nil, "MemTotal:", kb));
	printf("budget ok\n");
}

static void
TestPressure(void)
{
	Controller c;
	c.SetBudgetBytes((size_t)256 << 20);
	// used == base (no reserve) at half the budget -> pressure 0.5
	c.Update(0, (size_t)128 << 20, 0, 0.0f);
	assert(c.Pressure() > 0.49f && c.Pressure() < 0.51f);
	assert(c.FreeHeadroomMb() > 127.0f && c.FreeHeadroomMb() < 129.0f);

	// saturation: over the budget is 1.0, not above
	c.Update(0, (size_t)512 << 20, 0, 0.0f);
	assert(c.Pressure() == 1.0f);
	assert(c.FreeHeadroomMb() == 0.0f);

	// a reserve shrinks the usable span, pushing pressure up
	Controller r;
	r.SetBudgetBytes((size_t)256 << 20);
	r.Update(0, (size_t)128 << 20, (size_t)64 << 20, 0.0f);
	// (128-64)/(256-64) = 1/3
	assert(r.Pressure() > 0.32f && r.Pressure() < 0.35f);

	// below the reserve clamps to zero used
	r.Update(0, (size_t)32 << 20, (size_t)64 << 20, 0.0f);
	assert(r.Pressure() == 0.0f);
	printf("pressure ok\n");
}

static void
TestModeMachine(void)
{
	Controller c;
	c.SetBudgetBytes((size_t)256 << 20);
	const size_t MB = (size_t)1 << 20;

	// Plenty of headroom: full distance, normal quota.
	c.Update(0, 40 * MB, 0, 0.0f);   // 216 MB free
	assert(!c.MitigationActive() && !c.CriticalActive());
	assert(c.TargetDistanceScale() == 1.0f);
	assert(c.DistanceScale() == 1.0f);
	assert(c.VehicleRequestsPerFrame() == 3);
	assert(c.VehicleRingTarget(25) == 25);

	// 200 MB used -> 56 MB free: below the 48 MB leave threshold but above the
	// 32 MB enter threshold, so the previous (off) state must survive.
	c.Update(1000, 200 * MB, 0, 0.0f);
	assert(!c.MitigationActive());

	// 230 MB used -> 26 MB free: crosses the enter threshold.
	c.Update(2000, 230 * MB, 0, 0.0f);
	assert(c.MitigationActive());
	assert(!c.CriticalActive());
	// quota flips as soon as pressure passes projectedPressureThrottle
	assert(c.VehicleRequestsPerFrame() == 1);
	assert(c.AsyncRequestsPerFrame() == 2);
	assert(c.VehicleRingTarget(25) == 20);    // 80 % under mitigation

	// The delay: 1000 ms must pass before the target scale drops.
	assert(c.TargetDistanceScale() == 1.0f);
	c.Update(2000 + 999, 230 * MB, 0, 0.0f);
	assert(c.TargetDistanceScale() == 1.0f);
	c.Update(2000 + 1000, 230 * MB, 0, 0.0f);
	assert(c.TargetDistanceScale() == 0.75f);

	// It moves at distanceScaleChangePerSecond, not instantly: one 0.25 s step
	// covers 0.0625, so the first step only reaches 0.9375 - not 0.75.
	c.Update(3100, 230 * MB, 0, 0.25f);
	assert(c.DistanceScale() > 0.937f && c.DistanceScale() < 0.938f);
	// keep stepping until it settles
	for(int i = 0; i < 100; i++)
		c.Update(3100 + i * 250, 230 * MB, 0, 0.25f);
	assert(c.DistanceScale() > 0.749f && c.DistanceScale() < 0.751f);

	// Critical: 250 MB used -> 6 MB free.
	c.Update(5000, 250 * MB, 0, 0.0f);
	assert(c.CriticalActive());
	assert(c.TargetDistanceScale() == 0.60f);
	for(int i = 0; i < 100; i++)
		c.Update(5000 + i * 250, 250 * MB, 0, 0.25f);
	assert(c.DistanceScale() > 0.599f && c.DistanceScale() < 0.601f);
	// The vehicle ring buffer keeps 60 % of its normal target under critical,
	// not a quarter: a fourth of 25 cars pops traffic in and out.
	assert(c.VehicleRingTarget(25) == 15);
	assert(c.VehicleRingTarget(35) == 21);
	assert(c.VehicleRingTarget(6) == 6);      // floor never exceeds the target

	// Recovery has its own, longer delay: leave at 24 MB, come back to 60 MB free.
	c.Update(9000, 196 * MB, 0, 0.0f);  // 60 MB free
	assert(!c.CriticalActive());
	assert(!c.MitigationActive());
	// recoveryDelayMs = 3000
	c.Update(9000 + 2999, 196 * MB, 0, 0.0f);
	assert(c.TargetDistanceScale() == 0.60f); // still holding critical target
	c.Update(9000 + 3000, 196 * MB, 0, 0.0f);
	assert(c.TargetDistanceScale() == 1.0f);

	// Disabled mitigation (enter threshold <= 0) forces full distance and the
	// constrained quota, which is what the client does when the section is off.
	Controller off;
	Params p;
	p.mitigationEnterHeadroomMb = 0.0f;
	off.SetParams(p);
	off.SetBudgetBytes(256 * MB);
	off.Update(0, 250 * MB, 0, 0.0f);
	assert(!off.MitigationActive() && !off.CriticalActive());
	assert(off.TargetDistanceScale() == 1.0f);
	assert(off.VehicleRequestsPerFrame() == 1);
	printf("mode machine ok\n");
}

static void
TestHysteresisNeverOscillates(void)
{
	// Sitting exactly between the enter and leave thresholds must not toggle.
	Controller c;
	c.SetBudgetBytes((size_t)256 << 20);
	const size_t MB = (size_t)1 << 20;
	c.Update(0, 230 * MB, 0, 0.0f);   // 26 MB free -> mitigation on
	assert(c.MitigationActive());
	int flips = 0;
	bool prev = c.MitigationActive();
	for(int i = 0; i < 500; i++) {
		// 40 MB free: above enter (32), below leave (48)
		c.Update(1000 + i * 16, (256 - 40) * MB, 0, 0.016f);
		if(c.MitigationActive() != prev) {
			flips++;
			prev = c.MitigationActive();
		}
	}
	assert(flips == 0);
	assert(c.MitigationActive());

	// And the same holds coming from the other side.
	Controller d;
	d.SetBudgetBytes((size_t)256 << 20);
	d.Update(0, 40 * MB, 0, 0.0f);
	assert(!d.MitigationActive());
	for(int i = 0; i < 500; i++) {
		d.Update(1000 + i * 16, (256 - 40) * MB, 0, 0.016f);
		assert(!d.MitigationActive());
	}
	printf("hysteresis ok\n");
}

static void
TestBudgetIsNotDeviceRam(void)
{
	// The client reacts to its own streaming budget, not to how much RAM the
	// phone happens to have free. Two devices with the same budget but very
	// different free RAM must land in the same mode when the free RAM is far
	// above the budget (the old heuristic compared 280/450 MB and would have
	// called both "plenty").
	const size_t MB = (size_t)1 << 20;
	Controller a, b;
	a.SetBudgetBytes(320 * MB);
	b.SetBudgetBytes(320 * MB);
	a.Update(0, 300 * MB, 0, 0.0f);        // 20 MB free of budget
	b.Update(0, 300 * MB, 0, 0.0f);
	assert(a.MitigationActive() == b.MitigationActive());
	assert(a.MitigationActive());

	// The machine only ever sees "used of the budget", so a phone with 4 GB
	// free behaves exactly like one with 400 MB free: the same used figure
	// gives the same mode.
	Controller c;
	c.SetBudgetBytes(180 * MB);
	c.Update(0, 0, 0, 0.0f);              // nothing used: full headroom
	assert(c.Pressure() == 0.0f);
	assert(!c.MitigationActive());
	c.Update(16, 150 * MB, 0, 0.0f);      // 30 MB of a 180 MB budget left: under 32
	assert(c.MitigationActive());
	assert(!c.CriticalActive());           // but above the 8 MB critical line
	printf("budget model ok\n");
}

static void
TestRefreshRateScaling(void)
{
	// The smoothing is per second, so 60 and 30 fps reach the same value over
	// the same wall time (the old code multiplied by a frame count instead).
	const size_t MB = (size_t)1 << 20;
	Controller c60, c30;
	c60.SetBudgetBytes(256 * MB);
	c30.SetBudgetBytes(256 * MB);
	Params p;
	p.mitigationDelayMs = 0.0f;
	c60.SetParams(p);
	c30.SetParams(p);
	// prime both into mitigation with zero-length delay
	for(int i = 0; i < 200; i++) {
		c60.Update(0, 230 * MB, 0, 1.0f / 60.0f);
		c30.Update(0, 230 * MB, 0, 1.0f / 30.0f);
	}
	assert(c60.DistanceScale() > 0.749f && c60.DistanceScale() < 0.751f);
	assert(c30.DistanceScale() > 0.749f && c30.DistanceScale() < 0.751f);
	printf("refresh-rate scaling ok\n");
}


static void
TestPoolOccupancyIsThePressureInput(void)
{
	// §9.2: the machine divides (used - base) by (limit - base), where "used"
	// is the pool occupancy relative to the streaming cap, not the raw byte
	// count of streamed data and certainly not the device's free RAM.
	//
	// This is the regression that made the whole feature a no-op in game: the
	// caller fed ms_memoryUsed alone, which for a several-hundred-MB budget
	// stays far below 32 MB of headroom pressure, so neither mode ever fired.
	const size_t MB = (size_t)1 << 20;
	Params p;
	p.mitigationDelayMs = 0.0f;
	p.recoveryDelayMs = 0.0f;

	// Streamed-only figure (the old, broken input): no mode, full distance.
	Controller raw;
	raw.SetBudgetBytes(320 * MB);
	raw.SetParams(p);
	for(int i = 0; i < 300; i++)
		raw.Update((uint32_t)(i * 17), 60 * MB, 0, 1.0f / 60.0f);
	assert(!raw.MitigationActive());
	assert(raw.DistanceScale() > 0.999f);

	// Same streamed data plus the allocation base: the budget really is 90 %
	// consumed, so the soft mode must engage and pull the scale to 0.75.
	Controller occ;
	occ.SetBudgetBytes(320 * MB);
	occ.SetParams(p);
	for(int i = 0; i < 300; i++)
		occ.Update((uint32_t)(i * 17), (240 + 60) * MB, 0, 1.0f / 60.0f);
	assert(occ.MitigationActive());
	assert(!occ.CriticalActive());
	assert(occ.DistanceScale() > 0.749f && occ.DistanceScale() < 0.751f);
	printf("pool occupancy is the pressure input ok\n");
}

static void
TestFarPlaneMarginAcceptsSubUnitValues(void)
{
	// §3.2 calls farPlaneMarginScale the knob to turn for GPU savings. The
	// validator used to clamp it up to 1.0, which silently discarded any
	// value below one; only a non-positive margin is invalid.
	Params p;
	p.farPlaneMarginScale = 0.8f;
	Sanitize(p);
	assert(p.farPlaneMarginScale > 0.799f && p.farPlaneMarginScale < 0.801f);

	p.farPlaneMarginScale = 0.0f;
	Sanitize(p);
	assert(p.farPlaneMarginScale > 0.0f);

	p.farPlaneMarginScale = 1.05f;
	Sanitize(p);
	assert(p.farPlaneMarginScale > 1.049f && p.farPlaneMarginScale < 1.051f);
	printf("far plane margin accepts sub-unit values ok\n");
}

static void
TestPressureDrivesAllThreeOutputs(void)
{
	// One pressure value must move the distance scale, the ring target and the
	// request quota together; a stale branch in any of them is invisible until
	// the device is under load.
	const size_t MB = (size_t)1 << 20;
	Params p;
	p.mitigationDelayMs = 0.0f;
	p.recoveryDelayMs = 0.0f;

	Controller idle;
	idle.SetBudgetBytes(320 * MB);
	idle.SetParams(p);
	for(int i = 0; i < 300; i++)
		idle.Update((uint32_t)(i * 17), 80 * MB, 0, 1.0f / 60.0f);
	assert(!idle.MitigationActive());
	assert(idle.VehicleRequestsPerFrame() == 3);
	assert(idle.AsyncRequestsPerFrame() == 5);
	assert(idle.VehicleRingTarget(25) == 25);

	Controller loaded;
	loaded.SetBudgetBytes(320 * MB);
	loaded.SetParams(p);
	for(int i = 0; i < 900; i++)
		loaded.Update((uint32_t)(i * 17), 319 * MB, 0, 1.0f / 60.0f);
	assert(loaded.CriticalActive());
	assert(loaded.DistanceScale() > 0.599f && loaded.DistanceScale() < 0.601f);
	assert(loaded.VehicleRequestsPerFrame() == 1);
	assert(loaded.AsyncRequestsPerFrame() == 2);
	assert(loaded.VehicleRingTarget(25) == 15);
	assert(loaded.VehicleRingTarget(35) == 21);
	printf("pressure drives all three outputs ok\n");
}

// ---------------------------------------------------------------------------
// Chapter 9 §9.1 - pressure from the exact 0xe582b4 disassembly.
//   denominator = limit - (base + extraReserved)
//   numerator   = max(used - base, 0) + reserveBytes
// The reserve lives in the numerator, not the denominator.
// ---------------------------------------------------------------------------
static bool
Near(float a, float b, float eps) { return a > b - eps && a < b + eps; }

static void
TestPressureUsesBaseAndReserveExactly(void)
{
	const size_t MB = (size_t)1 << 20;
	Controller c;
	c.SetBudgetBytes(320 * MB);

	// No base, no reserve: plain used/limit.
	c.Update(0, 160 * MB, 0, 1.0f/60.0f);
	assert(Near(c.Pressure(), 0.5f, 0.001f));

	// Reserve adds to the numerator, so pressure grows without the limit moving.
	c.Reset();
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 160 * MB, 0, 1.0f/60.0f, 32 * MB, 0);
	assert(Near(c.Pressure(), 192.0f/320.0f, 0.001f));

	// extraReservedBytes shrinks the denominator instead.
	c.Reset();
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 160 * MB, 0, 1.0f/60.0f, 0, 64 * MB);
	assert(Near(c.Pressure(), 160.0f/256.0f, 0.001f));

	// used below base saturates the numerator at zero (no negative pressure).
	c.Reset();
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 32 * MB, 96 * MB, 1.0f/60.0f);
	assert(Near(c.Pressure(), 0.0f, 0.001f));

	// base is subtracted from usage, not added to it.
	c.Reset();
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 192 * MB, 32 * MB, 1.0f/60.0f);
	assert(Near(c.Pressure(), 160.0f/288.0f, 0.001f));
	printf("pressure uses base and reserve exactly ok\n");
}

// freeHeadroomMb follows the same accounting: a full pool reports none, so the
// mode machine can still reach critical.
static void
TestFreeHeadroomFollowsTheSameAccounting(void)
{
	const size_t MB = (size_t)1 << 20;
	Controller c;
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 192 * MB, 0, 1.0f/60.0f);
	assert(Near(c.FreeHeadroomMb(), 128.0f, 0.05f));

	c.Reset();
	c.SetBudgetBytes(320 * MB);
	c.Update(0, 320 * MB, 0, 1.0f/60.0f);
	assert(c.FreeHeadroomMb() < 0.05f);
	printf("free headroom follows the same accounting ok\n");
}

// §9.4 constants: one world walk is capped at 2000 entities (0x7d0) and the
// start ring at 512 slots (0x200). Both are client constants, not config.
static void
TestWorldWalkLimits(void)
{
	assert(revcopt::WORLD_WALK_MAX_ENTITIES == 2000);
	assert(revcopt::WORLD_WALK_RING_SLOTS == 512);

	assert(revcopt::WorldWalkHasBudget(0, 5));
	assert(revcopt::WorldWalkHasBudget(4, 5));
	assert(!revcopt::WorldWalkHasBudget(5, 5));
	assert(!revcopt::WorldWalkHasBudget(2000, 4000));

	assert(revcopt::WorldWalkRingCapacity(0) == 0);
	assert(revcopt::WorldWalkRingCapacity(64) == 64);
	assert(revcopt::WorldWalkRingCapacity(4000) == 512);
	printf("world walk limits ok\n");
}

int
main(void)
{
	TestDefaults();
	TestPartialConfig();
	TestBadConfig();
	TestBudget();
	TestPressure();
	TestModeMachine();
	TestHysteresisNeverOscillates();
	TestBudgetIsNotDeviceRam();
	TestRefreshRateScaling();
	TestPoolOccupancyIsThePressureInput();
	TestFarPlaneMarginAcceptsSubUnitValues();
	TestPressureDrivesAllThreeOutputs();
	TestPressureUsesBaseAndReserveExactly();
	TestFreeHeadroomFollowsTheSameAccounting();
	TestWorldWalkLimits();
	printf("optimization profile tests passed\n");
	return 0;
}
