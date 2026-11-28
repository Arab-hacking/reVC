#ifndef __OPTIMIZATION_CONFIG_H__
#define __OPTIMIZATION_CONFIG_H__

#include <cstddef>

// PUNCT ANALYSE v38 — optimization config parameters
// All 48 parameters from chapter 8 of the analysis, implemented strictly
// as described in the analysis.

// ---- Section: memoryPressure (+0x20..+0x30) ----
// Hysteresis on headroom (budget - used): enter mitigation when headroom
// drops below mitigationEnterHeadroomMb, leave when it rises above
// mitigationLeaveHeadroomMb. Critical mode is the same with its own pair.
static const float OPT_mitigationEnterHeadroomMb = 32.0f;
static const float OPT_mitigationLeaveHeadroomMb = 48.0f;
static const float OPT_criticalEnterHeadroomMb = 8.0f;
static const float OPT_criticalLeaveHeadroomMb = 24.0f;
static const float OPT_projectedPressureThrottle = 0.75f;

// ---- Section: worldDistance (+0x34..+0x48) ----
// Under memory pressure the client reduces draw distance. The scale
// changes gradually (distanceScaleChangePerSecond) with a delay before
// entering and recovering. farPlaneMarginScale is a multiplier on the
// far clip plane relative to the current scale.
static const float OPT_mitigationDistanceScale = 0.75f;
static const float OPT_criticalDistanceScale = 0.6f;
static const float OPT_distanceScaleChangePerSecond = 0.25f;
static const int   OPT_mitigationDelayMs = 1000;
static const int   OPT_recoveryDelayMs = 3000;
static const float OPT_farPlaneMarginScale = 1.05f;

// ---- Section: networkVehicles (+0x4c..+0x7c) ----
// Vehicle streaming distances and batching. Vehicles beyond
// vehicleUnloadDistance are removed; within vehicleReloadDistance they
// are loaded again. Aggressive mode (high pressure) uses tighter
// distances. vehicleBatchMin/Max control how many vehicles are loaded
// per batch; vehicleBatchDensityDivisor scales the batch size by area.
// vehicleRequestsPerFrame is the max model requests per frame in
// normal mode; constrainedVehicleRequestsPerFrame is the limit under
// pressure. vehicleModelTimeoutMs is how long a pending request waits;
// vehicleModelMaxRequests caps the total in-flight requests.
static const float OPT_vehicleUnloadDistance = 35.0f;
static const float OPT_vehicleReloadDistance = 25.0f;
static const float OPT_aggressiveVehicleUnloadDistance = 20.0f;
static const float OPT_aggressiveVehicleReloadDistance = 15.0f;
static const float OPT_aggressiveVehicleProtectedDistance = 20.0f;
static const int   OPT_aggressiveVehicleUpdateIntervalMs = 250;
static const int   OPT_vehicleBatchMin = 2;
static const int   OPT_vehicleBatchMax = 8;
static const int   OPT_vehicleBatchDensityDivisor = 16;
static const int   OPT_vehicleRequestsPerFrame = 3;
static const int   OPT_constrainedVehicleRequestsPerFrame = 1;
static const int   OPT_vehicleModelTimeoutMs = 3000;
static const int   OPT_vehicleModelMaxRequests = 50;

// ---- Section: remotePlayerAppearance (+0x80..+0xbc) ----
// Remote player (multiplayer) appearance streaming: accessories and
// skins are unloaded/reloaded by distance, with protected zones that
// are never unloaded. preservedPlayersDivisor scales the minimum number
// of players that keep their appearance under pressure.
static const float OPT_accessoryUnloadDistance = 20.0f;
static const float OPT_accessoryReloadDistance = 15.0f;
static const float OPT_skinUnloadDistance = 30.0f;
static const float OPT_skinReloadDistance = 25.0f;
static const float OPT_appearancePreserveDistance = 30.0f;
static const float OPT_accessoryProtectedDistance = 12.0f;
static const float OPT_skinProtectedDistance = 20.0f;
static const float OPT_fallbackSkinProjectedPressure = 0.98f;
static const float OPT_severeProjectedPressure = 1.0f;
static const int   OPT_aggressivePlayerUpdateIntervalMs = 250;
static const int   OPT_playerBatchMin = 2;
static const int   OPT_playerBatchMax = 8;
static const int   OPT_playerBatchDensityDivisor = 12;
static const int   OPT_minimumPreservedPlayers = 8;
static const int   OPT_preservedPlayersDivisor = 3;
static const int   OPT_severePreservedPlayersDivisor = 4;

// ---- Section: asyncModelRequests (+0xc0..+0xdc) ----
// Asynchronous model loading: asyncRequestsPerFrame is the max requests
// per frame; constrainedAsyncRequestsPerFrame under pressure. Failed
// requests retry with exponential backoff from retryBaseDelayMs to
// retryMaxDelayMs capped at retryMaxExponent. cleanupChecksPerFrame is
// how many pending requests are checked for timeout each frame.
static const int   OPT_asyncRequestsPerFrame = 5;
static const int   OPT_constrainedAsyncRequestsPerFrame = 2;
static const int   OPT_asyncModelTimeoutMs = 10000;
static const int   OPT_asyncModelMaxRequests = 150;
static const int   OPT_retryBaseDelayMs = 100;
static const int   OPT_retryMaxDelayMs = 2000;
static const int   OPT_retryMaxExponent = 5;
static const int   OPT_cleanupChecksPerFrame = 100;

// ---- Memory budget scaling table ----
// Memory budget scales with total RAM (analysis ch. 8.5). The client
// picks the row with the nearest minimumRamGb that does not exceed the
// device RAM and uses its budgetMb as the target. The validator
// clamps the budget to 64..4096 MB.
struct MemoryBudgetScaling {
	size_t minimumRamGb;
	size_t budgetMb;
};

static const MemoryBudgetScaling OPT_memoryBudgetScaling[] = {
	{ 0,   180 },  // < 6 GB
	{ 6,   256 },  // 6 GB
	{ 8,   320 },  // 8 GB
	{ 12,  384 },  // >= 12 GB
};
static const int OPT_memoryBudgetScalingCount = 4;

// Validator clamp range for budgetMb (analysis ch. 8.5).
static const int OPT_budgetMinMb = 64;
static const int OPT_budgetMaxMb = 4096;

#endif // __OPTIMIZATION_CONFIG_H__
