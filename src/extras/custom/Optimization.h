#ifndef REVC_CUSTOM_OPTIMIZATION_H
#define REVC_CUSTOM_OPTIMIZATION_H

// Runtime side of the memory-pressure optimization (see OptimizationProfile.h
// for the model itself and OPTIMIZATION.md for the port notes).
//
// The client picks a memory budget from the installed RAM once at streaming
// init, then every frame compares the free headroom against the thresholds in
// the config, moves the distance scale smoothly and switches the per-frame
// request quota. This module is the only place that touches /proc and the
// config file; the decision logic stays pure and host-testable.

struct OptimizationSnapshot
{
	float distanceScale;       // smoothed multiplier for CRenderer::ms_lodDistScale
	float pressure;            // 0..1 saturation of the budget
	float freeHeadroomMb;      // what is left of the budget right now
	bool mitigation;           // soft mode active
	bool critical;             // hard mode active
	int vehicleRequestsPerFrame;
	int asyncRequestsPerFrame;
	bool skinFallbackWanted;
};

namespace revcopt {

// Reads the RAM budget and the optional optimization json once. Safe to call
// more than once: only the first call does work. Returns true when a config
// file was found and applied.
bool Startup(void);
bool ConfigLoaded(void);
const char *ConfigSource(void);

// Called once per frame from streaming.
//
// usedBytes      - what streaming currently holds (CStreaming::ms_memoryUsed),
//                  the "занято" term of the client's pressure (§9.1).
// availableBytes - the device's free RAM (Android: MemAvailable); 0 if unknown.
//                  Only used as a fallback when usedBytes is not known.
OptimizationSnapshot Tick(unsigned nowMs, float dtSeconds,
                          unsigned long long usedBytes,
                          unsigned long long availableBytes);

// The streaming memory limit the client derives from installed RAM through the
// memoryBudgetScaling table (180 / 256 / 320 / 384 MB, clamped 64..4096).
// In the client this value *is* the allocator limit given to streaming, so the
// pool is sized from it and the headroom thresholds are expressed against it.
size_t BudgetBytes(void);

// Last value returned by Tick, for callers that run before streaming.
const OptimizationSnapshot &Current(void);

// Number of vehicle models the ring buffer should keep, given its normal
// target (80 % under mitigation, 60 % under critical, floor of eight).
int VehicleRingTarget(int normalSlots);

// worldDistance.farPlaneMarginScale: the far clip follows the distance scale
// with this margin, so it has to be applied wherever the far plane is set.
float FarPlaneMargin(void);

} // namespace revcopt

#endif // REVC_CUSTOM_OPTIMIZATION_H
