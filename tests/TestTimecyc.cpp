#include "gtest/gtest.h"
#include "core/br/TimecycParse.h"
#include <string>

using namespace brtimecyc;

TEST(TimecycParse, ParseTimecycJson) {
    const char* json = R"([
        {
            "AmbientPhysicalRGB": [220, 212, 130],
            "AmbientRGB": [22, 22, 22],
            "CloudRGB": [30, 20, 0],
            "DirectionalRGB": [255, 255, 255],
            "FarClip": 400.0,
            "FluffyBottomRGB": [3, 3, 3],
            "FogStart": 100.0,
            "LightGnd": 1.0,
            "LightShad": 100,
            "PoleShad": 0,
            "PostFX1ARGB": [255, 87, 87, 87],
            "PostFX2ARGB": [255, 60, 121, 122],
            "Shad": 200,
            "SkyBottomRGB": [0, 31, 32],
            "SkyTopRGB": [0, 23, 24],
            "SpriteBrght": 0.3,
            "SpriteSize": 1.0,
            "SunCoreRGB": [255, 128, 0],
            "SunCoronaRGB": [5, 0, 0]
        }
    ])";

    std::string err;
    TimecycData result = TimecycParser::ParseTimecyc(json, strlen(json), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.presets.size(), 1);
    EXPECT_FLOAT_EQ(result.presets[0].FarClip, 400.0f);
    EXPECT_FLOAT_EQ(result.presets[0].AmbientRGB[0], 22.0f);
}

TEST(TimecycParse, ParseWeatherJson) {
    const char* json = R"({
        "CLEAR": {
            "probability": 0.5,
            "durationMin": 600,
            "durationMax": 1800,
            "transitions": ["CLOUDY", "RAIN"]
        },
        "RAIN": {
            "probability": 0.2,
            "durationMin": 300,
            "durationMax": 900,
            "transitions": ["CLEAR"]
        }
    })";

    std::string err;
    TimecycData result = TimecycParser::ParseWeather(json, strlen(json), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.weatherCycles.size(), 2);
    EXPECT_EQ(result.weatherCycles[0].name, "CLEAR");
    EXPECT_FLOAT_EQ(result.weatherCycles[0].probability, 0.5f);
}

TEST(TimecycParse, ParseBoth) {
    const char* tc = R"([{"FarClip": 400.0}])";
    const char* w = R"({"CLEAR": {"probability": 1.0}})";
    std::string err;
    TimecycData result = TimecycParser::ParseBoth(tc, strlen(tc), w, strlen(w), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.presets.size(), 1);
    EXPECT_EQ(result.weatherCycles.size(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}