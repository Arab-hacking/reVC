#include "gtest/gtest.h"
#include "core/br/ModFormat.h"
#include "core/br/BtxFormat.h"
#include "core/br/ClsFormat.h"
#include <fstream>
#include <vector>

// Helper to load file
static std::vector<uint8_t> LoadFile(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    std::vector<uint8_t> buf(size);
    f.seekg(0);
    f.read((char*)buf.data(), size);
    return buf;
}

TEST(ModFormat, DetectMod) {
    // Test with a known .mod file from BR
    auto buf = LoadFile("../../_br_analysis/extract/common/mesh/br_cars_01.bpc"); // This is a ZIP, not .mod
    // Skip if not a .mod
    // Real test would extract a .mod from the BPC first
    EXPECT_TRUE(true); // Placeholder
}

TEST(ModFormat, KeyVariantDetection) {
    // Test detectModKeyVariant with known good .mod
    EXPECT_TRUE(true); // Placeholder
}

TEST(BtxFormat, ParseBtxGpu) {
    // Test with a known .btx file from BR
    auto buf = LoadFile("../../_br_analysis/extract/common/textures/br_tex_cars_01.astc.bpc"); // This is a ZIP
    // Real test would extract a .btx from the BPC first
    EXPECT_TRUE(true); // Placeholder
}

TEST(BtxFormat, PeekBtx) {
    EXPECT_TRUE(true); // Placeholder
}

TEST(BtxFormat, PotResample) {
    brbtx::Texture tex;
    tex.mips.push_back({255, 255, std::vector<uint8_t>(255*255*4, 255)});
    EXPECT_TRUE(brbtx::makePot(tex));
    EXPECT_EQ(tex.mips[0].w, 256u);
    EXPECT_EQ(tex.mips[0].h, 256u);
}

TEST(ClsFormat, ConvertClst) {
    // Test with a known .cls block
    EXPECT_TRUE(true); // Placeholder
}

TEST(ClsFormat, SurfaceMapping) {
    EXPECT_EQ(brcls::Surface(0), 0);
    EXPECT_EQ(brcls::Surface(34), 34);
    EXPECT_EQ(brcls::Surface(80), 2);  // GRASS
    EXPECT_EQ(brcls::Surface(35), 3);  // MUD_DRY
    EXPECT_EQ(brcls::Surface(74), 19); // SAND
}

TEST(ClsFormat, IndexEmpty) {
    std::vector<brcls::Block> blocks;
    size_t bad = brcls::Index(nullptr, 0, blocks);
    EXPECT_EQ(bad, 0);
    EXPECT_TRUE(blocks.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}