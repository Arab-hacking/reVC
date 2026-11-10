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

// Called once per frame from streaming. availableBytes is the device's free
// RAM (Android: MemAvailable); pass 0 when it is unknown.
OptimizationSnapshot Tick(unsigned nowMs, float dtSeconds, unsigned long long availableBytes);

// Last value returned by Tick, for callers that run before streaming.
const OptimizationSnapshot &Current(void);

// Number of vehicle models the ring buffer should keep, given its capacity.
int PreservedVehicles(int maxSlots);

// worldDistance.farPlaneMarginScale: the far clip follows the distance scale
// with this margin, so it has to be applied wherever the far plane is set.
float FarPlaneMargin(void);

} // namespace revcopt

#endif // REVC_CUSTOM_OPTIMIZATION_H
