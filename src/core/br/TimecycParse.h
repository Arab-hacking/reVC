#pragma once

// Black Russia timecyc.json / weather.json parser

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <nlohmann/json.hpp>

namespace brtimecyc {

// One timecycle preset (22 in BR)
struct TimecyclePreset {
    std::array<float, 3> AmbientPhysicalRGB;
    std::array<float, 3> AmbientRGB;
    std::array<float, 3> CloudRGB;
    std::array<float, 3> DirectionalRGB;
    float FarClip;
    std::array<float, 3> FluffyBottomRGB;
    float FogStart;
    float LightGnd;
    float LightShad;
    float PoleShad;
    std::array<float, 4> PostFX1ARGB;
    std::array<float, 4> PostFX2ARGB;
    float Shad;
    std::array<float, 3> SkyBottomRGB;
    std::array<float, 3> SkyTopRGB;
    float SpriteBrght;
    float SpriteSize;
    std::array<float, 3> SunCoreRGB;
    std::array<float, 3> SunCoronaRGB;
    // ... potentially more fields
};

struct WeatherCycle {
    std::string name;
    float probability;
    float durationMin, durationMax;
    std::vector<std::string> transitions;  // next weather names
};

struct TimecycData {
    std::vector<TimecyclePreset> presets;  // 22 presets for 0-23h (some hours shared)
    std::vector<WeatherCycle> weatherCycles;
};

class TimecycParser {
public:
    static TimecycData ParseTimecyc(const char* jsonData, size_t size, std::string* err = nullptr);
    static TimecycData ParseWeather(const char* jsonData, size_t size, std::string* err = nullptr);
    static TimecycData ParseBoth(const char* timecycJson, size_t tcSize, const char* weatherJson, size_t wSize, std::string* err = nullptr);

    // Convert to SA CTimecycle format (for compatibility)
    static void ToSAFormat(const TimecyclePreset& preset, int hour, void* saTimecycleSlot);
};

} // namespace brtimecyc