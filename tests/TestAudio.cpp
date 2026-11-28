#include "gtest/gtest.h"
#include "core/br/AudioConfigParse.h"
#include <string>

using namespace braudio;

TEST(AudioConfigParse, ParseVehicleAudioSettings) {
    const char* data = "landstal\t0\t71\t72\t0\t0.77999997138977\t1.0\t15\t1.00000000000000\t2\t8\t0\t60\t0.0\n"
                       "bravura\t0\t41\t42\t0\t0.69999998807907\t1.0\t15\t1.18920707702640\t2\t5\t0\t60\t0.0\n";
    std::string err;
    AudioConfigData result = AudioConfigParser::ParseVehicleAudio(data, strlen(data), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.vehicleSettings.size(), 2);
    EXPECT_EQ(result.byModel.size(), 2);

    const VehicleAudioSettings* landstal = AudioConfigParser::GetVehicleAudio(result, "landstal");
    ASSERT_NE(landstal, nullptr);
    EXPECT_EQ(landstal->engineSample, 71);
    EXPECT_EQ(landstal->hornSample, 72);
    EXPECT_FLOAT_EQ(landstal->pitch, 0.78f);
}

TEST(AudioConfigParse, ParseBpcMeta) {
    // Create a simple bpcmeta-like buffer
    // Format: "Bank_001/sound_001.mp3\0" + offset (4 bytes) + size (4 bytes)
    std::vector<uint8_t> buf;
    std::string name = "Bank_001/sound_001.mp3";
    buf.insert(buf.end(), name.begin(), name.end());
    buf.push_back(0); // null terminator
    // offset = 1000, size = 5000
    buf.push_back(0xE8); buf.push_back(0x03); buf.push_back(0x00); buf.push_back(0x00);
    buf.push_back(0x88); buf.push_back(0x13); buf.push_back(0x00); buf.push_back(0x00);

    std::string err;
    AudioConfigData result = AudioConfigParser::ParseBpcMeta(buf.data(), buf.size(), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.bankIndex.size(), 1);

    const auto* entries = AudioConfigParser::GetBankEntries(result, "Bank_001");
    ASSERT_NE(entries, nullptr);
    EXPECT_EQ(entries->size(), 1);
    EXPECT_EQ((*entries)[0].soundName, "Bank_001/sound_001.mp3");
    EXPECT_EQ((*entries)[0].offset, 1000u);
    EXPECT_EQ((*entries)[0].size, 5000u);
}

TEST(AudioConfigParse, GetVehicleAudioNotFound) {
    AudioConfigData data;
    const VehicleAudioSettings* vas = AudioConfigParser::GetVehicleAudio(data, "nonexistent");
    EXPECT_EQ(vas, nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}