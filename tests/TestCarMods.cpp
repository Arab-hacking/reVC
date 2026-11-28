#include "gtest/gtest.h"
#include "core/br/CarModsParse.h"
#include <string>

using namespace brcarmods;

TEST(CarModsParse, ParseLinkSection) {
    const char* data = "link\nbntl_b_ov, bntr_b_ov\nbntl_b_sq, bntr_b_sq\nend\n";
    std::string err;
    CarModsData result = CarModsParser::Parse(data, strlen(data), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.links.size(), 2);
    EXPECT_EQ(result.links[0].left, "bntl_b_ov");
    EXPECT_EQ(result.links[0].right, "bntr_b_ov");
}

TEST(CarModsParse, ParseModsSection) {
    const char* data = "mods\nbravura, nto_b_l, 0, 1000\nbravura, nto_b_s, 1, 1001\nend\n";
    std::string err;
    CarModsData result = CarModsParser::Parse(data, strlen(data), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.mods.size(), 2);
    EXPECT_EQ(result.mods[0].modelName, "bravura");
    EXPECT_EQ(result.mods[0].modName, "nto_b_l");
    EXPECT_EQ(result.mods[0].modType, MOD_BUMPER_F);
    EXPECT_EQ(result.mods[0].modelId, 1000);
}

TEST(CarModsParse, ParseFullFile) {
    const char* data = "link\nbntl_b_ov, bntr_b_ov\nend\n\nmods\nbravura, nto_b_l, 0, 1000\nbravura, nto_b_s, 1, 1001\nbuffalo, nto_b_l, 0, 1002\nend\n";
    std::string err;
    CarModsData result = CarModsParser::Parse(data, strlen(data), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(result.links.size(), 1);
    EXPECT_EQ(result.mods.size(), 3);
    auto* bravuraMods = CarModsParser::GetModsForModel(result, "bravura");
    ASSERT_NE(bravuraMods, nullptr);
    EXPECT_EQ(bravuraMods->size(), 2);
    auto* buffaloMods = CarModsParser::GetModsForModel(result, "buffalo");
    ASSERT_NE(buffaloMods, nullptr);
    EXPECT_EQ(buffaloMods->size(), 1);
}

TEST(CarModsParse, ModTypeToUpgradeSlot) {
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_BUMPER_F), 0);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_BUMPER_R), 2);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_BONNET), 4);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_BOOT), 6);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_EXHAUST), 9);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_GRILLE), 14);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_ROOF), 15);
    EXPECT_EQ(CarModsParser::ModTypeToUpgradeSlot(MOD_UNKNOWN), -1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}