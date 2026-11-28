#include "TimecycParse.h"

namespace brtimecyc {

TimecycData TimecycParser::ParseTimecyc(const char* jsonData, size_t size, std::string* err) {
    TimecycData result;
    try {
        auto j = nlohmann::json::parse(jsonData, jsonData + size);
        if (!j.is_array()) { if (err) *err = "timecyc.json root is not an array"; return result; }

        for (const auto& preset : j) {
            TimecyclePreset p;
            auto getArr3 = [&](const char* key, std::array<float,3>& out, std::array<float,3> def = {0,0,0}) {
                if (preset.contains(key) && preset[key].is_array() && preset[key].size() >= 3) {
                    out[0] = preset[key][0].get<float>();
                    out[1] = preset[key][1].get<float>();
                    out[2] = preset[key][2].get<float>();
                } else out = def;
            };
            auto getArr4 = [&](const char* key, std::array<float,4>& out, std::array<float,4> def = {0,0,0,0}) {
                if (preset.contains(key) && preset[key].is_array() && preset[key].size() >= 4) {
                    out[0] = preset[key][0].get<float>();
                    out[1] = preset[key][1].get<float>();
                    out[2] = preset[key][2].get<float>();
                    out[3] = preset[key][3].get<float>();
                } else out = def;
            };

            getArr3("AmbientPhysicalRGB", p.AmbientPhysicalRGB);
            getArr3("AmbientRGB", p.AmbientRGB);
            getArr3("CloudRGB", p.CloudRGB);
            getArr3("DirectionalRGB", p.DirectionalRGB);
            p.FarClip = preset.value("FarClip", 400.0f);
            getArr3("FluffyBottomRGB", p.FluffyBottomRGB);
            p.FogStart = preset.value("FogStart", 100.0f);
            p.LightGnd = preset.value("LightGnd", 1.0f);
            p.LightShad = preset.value("LightShad", 100.0f);
            p.PoleShad = preset.value("PoleShad", 0.0f);
            getArr4("PostFX1ARGB", p.PostFX1ARGB);
            getArr4("PostFX2ARGB", p.PostFX2ARGB);
            p.Shad = preset.value("Shad", 200.0f);
            getArr3("SkyBottomRGB", p.SkyBottomRGB);
            getArr3("SkyTopRGB", p.SkyTopRGB);
            p.SpriteBrght = preset.value("SpriteBrght", 0.3f);
            p.SpriteSize = preset.value("SpriteSize", 1.0f);
            getArr3("SunCoreRGB", p.SunCoreRGB);
            getArr3("SunCoronaRGB", p.SunCoronaRGB);

            result.presets.push_back(std::move(p));
        }
    } catch (const std::exception& e) {
        if (err) *err = std::string("JSON parse error: ") + e.what();
    }
    return result;
}

TimecycData TimecycParser::ParseWeather(const char* jsonData, size_t size, std::string* err) {
    TimecycData result;
    try {
        auto j = nlohmann::json::parse(jsonData, jsonData + size);
        if (!j.is_object()) { if (err) *err = "weather.json root is not an object"; return result; }

        for (auto it = j.begin(); it != j.end(); ++it) {
            WeatherCycle wc;
            wc.name = it.key();
            auto& v = it.value();
            wc.probability = v.value("probability", 1.0f);
            wc.durationMin = v.value("durationMin", 600.0f);
            wc.durationMax = v.value("durationMax", 1800.0f);
            if (v.contains("transitions") && v["transitions"].is_array()) {
                for (const auto& t : v["transitions"]) wc.transitions.push_back(t.get<std::string>());
            }
            result.weatherCycles.push_back(std::move(wc));
        }
    } catch (const std::exception& e) {
        if (err) *err = std::string("JSON parse error: ") + e.what();
    }
    return result;
}

TimecycData TimecycParser::ParseBoth(const char* timecycJson, size_t tcSize, const char* weatherJson, size_t wSize, std::string* err) {
    TimecycData result = ParseTimecyc(timecycJson, tcSize, err);
    if (err && !err->empty()) return result;
    auto weather = ParseWeather(weatherJson, wSize, err);
    result.weatherCycles = std::move(weather.weatherCycles);
    return result;
}

void TimecycParser::ToSAFormat(const TimecyclePreset& preset, int hour, void* saTimecycleSlot) {
    // Convert BR preset to SA timecycle structure
    // This would map to the game's internal CTimecycle slot
    // Implementation depends on SA timecycle memory layout
    (void)preset; (void)hour; (void)saTimecycleSlot;
}

} // namespace brtimecyc