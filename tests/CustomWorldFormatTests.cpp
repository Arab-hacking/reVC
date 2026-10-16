#include "../src/extras/custom/CustomWorldFormat.h"
#include "../src/extras/custom/BrCollisionFormat.h"
#include "../src/extras/custom/CustomZip.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void W16(std::vector<uint8_t> &b, size_t off, uint16_t n) {
    b[off] = (uint8_t)n; b[off+1] = (uint8_t)(n>>8);
}
static void W32(std::vector<uint8_t> &b, size_t off, uint32_t n) {
    for(int k = 0; k < 4; k++) b[off+k] = (uint8_t)(n>>(k*8));
}
static void WF(std::vector<uint8_t> &b, size_t off, float x) {
    uint32_t n; std::memcpy(&n, &x, 4); W32(b, off, n);
}
static void A16(std::vector<uint8_t> &b, uint16_t n) {
    b.push_back((uint8_t)n); b.push_back((uint8_t)(n>>8));
}
static void A32(std::vector<uint8_t> &b, uint32_t n) {
    A16(b, (uint16_t)n); A16(b, (uint16_t)(n>>16));
}

static std::vector<uint8_t> MakeBlock(const char *tag = "CLST", bool groups = true)
{
    std::vector<uint8_t> b(120, 0);
    std::memcpy(b.data(), tag, 4);
    std::memcpy(b.data()+8, "XmasBox1", 8);
    std::memcpy(b.data()+28, "CED2", 4);
    WF(b, 32, 1.0f); WF(b, 36, 2.0f); WF(b, 40, 3.0f); WF(b, 44, 10.0f);
    WF(b, 48, -2.0f); WF(b, 52, -3.0f); WF(b, 56, -4.0f);
    WF(b, 60, 4.0f); WF(b, 64, 5.0f); WF(b, 68, 6.0f);
    W16(b, 72, 1); W16(b, 74, 1); W16(b, 76, 1);
    b[80] = groups ? 0x0a : 0x02;
    W32(b, 84, 116); W32(b, 88, 136); W32(b, 96, 164);
    b.resize(140, 0); // sphere A at 120 (SA: cx,cy,cz,radius,surface,piece)
    WF(b, 120, 3.0f); WF(b, 124, 2.0f); WF(b, 128, 1.0f); WF(b, 132, 0.5f);
    b[136] = 255; b[137] = 5;
    b.resize(168, 0); // box B at 140
    WF(b, 140, -1); WF(b, 144, -1); WF(b, 148, -1);
    WF(b, 152, 1); WF(b, 156, 1); WF(b, 160, 1);
    b[164] = 254;
    b.resize(188, 0); // three vertices (0,0,0), (1,0,0), (0,2,0); 2B padding
    W16(b, 174, 128); W16(b, 182, 256);
    if(groups) {
        b.resize(216, 0); // group bounds + u16 firstFace,lastFace
        W16(b, 212, 0); W16(b, 214, 0);
        b.resize(220, 0); W32(b, 216, 1); // group count immediately before faces
    }
    size_t face = b.size(); b.resize(face+8, 0);
    W16(b, face, 0); W16(b, face+2, 1); W16(b, face+4, 2);
    b[face+6] = 255; b[face+7] = 0xff;
    W32(b, 100, (uint32_t)face-4); // offsets are relative to block+4
    W32(b, 4, (uint32_t)b.size()-8);
    return b;
}

static void TestWorldText()
{
    const std::string ide =
        "# IDE file\r\nobjs\r\n"
        "42927, L1_r3_bld_tesltower, r3, 70, 0\r\n"
        "42928, Bad, r3, nan, 0\r\n"
        "42929, flag008, r3, 80, 008\n"
        "tobj\n35707, r3_BarrackWoodNW, r3, 299, 0, 21, 6\n"
        "end\nanim\n10873, Rclub_Spot_osn, AnimTest, Rclub_SpotAnim, 300, 8\n"
        "end\npath\n20 30 40\n";
    std::vector<customworld::Model> models;
    customworld::Stats s = customworld::ParseIde((const unsigned char *)ide.data(), ide.size(),
        [&](const customworld::Model &m){ models.push_back(m); });
    assert(s.records == 4 && s.rejected == 1 && s.overlong == 0);
    assert(models[0].id == 42927 && models[0].name == "L1_r3_bld_tesltower" && models[0].distance == 70);
    assert(models[1].id == 42929 && models[1].flags == 8); // not octal
    assert(models[2].kind == customworld::Model::TIME_OBJECT && models[2].timeOn == 21 && models[2].timeOff == 6);
    assert(models[3].kind == customworld::Model::ANIM_OBJECT && models[3].txd == "Rclub_SpotAnim");

    const std::string ipl =
        "inst\n"
        "35732, 1_PJ_HangarInterA, 1, -189.23204, 416.26126, 1504.19678, 0, 0, 0, 1, -1\n"
        "16480 BlockCRBroads1 0 6.103752 272.181366 334.464844 0 0 0.707279 -0.706934 -1\n"
        "42927 L1_r3_bld_tesltower 0 1 2 3 0 0 0 1\n" // no LOD index
        "42928 invalid 0 1 2 3 0 0 0 0 -1\n" // zero quaternion
        "end\noccl\n808 2 3\n";
    std::vector<customworld::Instance> places;
    s = customworld::ParseIpl((const unsigned char *)ipl.data(), ipl.size(),
        [&](const customworld::Instance &i){ places.push_back(i); });
    assert(s.records == 3 && s.rejected == 1);
    assert(places[0].interior == 1 && places[0].lod == -1);
    assert(places[1].id == 16480 && places[1].name == "BlockCRBroads1");
    assert(places[2].lod == -1);
    float axis[3], w;
    customworld::ViceAxis(places[1], axis, w);
    assert(std::fabs(axis[2]+1.0f) < 0.001f && w < -0.7f);
    std::string longLine(512, 'A');
    longLine += "\ninst\n1 tree 0 0 0 0 0 0 0 1 -1\n";
    s = customworld::ParseIpl((const unsigned char *)longLine.data(), longLine.size(),
        [&](const customworld::Instance &){});
    assert(s.overlong == 1 && s.records == 1);
    assert(!customworld::Name("this_filename_is_far_too_long_to_fit", 20));
}

static void TestCollision()
{
    std::vector<uint8_t> one = MakeBlock();
    std::vector<uint8_t> two = MakeBlock("CLSF", false);
    std::memcpy(two.data()+8, "OtherBox", 8);
    std::vector<uint8_t> file = one;
    file.insert(file.end(), two.begin(), two.end());
    std::vector<brcollision::Block> refs;
    assert(brcollision::Index(file.data(), file.size(), refs) == 0);
    assert(refs.size() == 2 && refs[0].name == "XmasBox1" && refs[1].name == "OtherBox");
    assert(refs[1].offset == one.size() && refs[1].size == two.size());
    std::vector<uint8_t> game;
    assert(brcollision::Convert(one.data(), one.size(), game));
    assert(std::memcmp(game.data(), "COLL", 4) == 0 &&
           brcollision::U32(game.data()+4) == game.size()-8);
    assert(std::strcmp((const char *)game.data()+8, "XmasBox1") == 0);
    assert(std::fabs(brcollision::F32(game.data()+32) - 10.0f) < 1e-5f);
    assert(std::fabs(brcollision::F32(game.data()+36) - 1.0f) < 1e-5f);
    assert(brcollision::U32(game.data()+72) == 1); // VC sphere count
    assert(std::fabs(brcollision::F32(game.data()+76) - 0.5f) < 1e-5f);
    assert(game[92] == 0 && game[93] == 5); // invalid SA material clamped
    assert(brcollision::U32(game.data()+100) == 1); // VC box count
    assert(game[129] == 0); // invalid SA box material clamped
    assert(brcollision::U32(game.data()+132) == 3); // vertices
    assert(std::fabs(brcollision::F32(game.data()+148) - 1.0f) < 1e-5f);
    assert(brcollision::U32(game.data()+172) == 1); // face count
    assert(brcollision::U32(game.data()+180) == 1);
    assert(game[188] == 0); // invalid SA triangle material clamped
    assert(brcollision::Convert(two.data(), two.size(), game)); // CLSF also CED2

    std::vector<uint8_t> damaged = one;
    W32(damaged, 100, 0xFFFFFFFF); // outside file
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    W16(damaged, damaged.size()-6, 50); // face wants non-existent vertex
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    W16(damaged, 212, 1); // group does not cover first face
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    WF(damaged, 132, -0.5f); // invalid sphere radius
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    WF(damaged, 152, -2.0f); // box max.x < min.x
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    W32(damaged, 88, 116); // box overlaps sphere A
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    damaged = one;
    std::memcpy(damaged.data()+28, "XXXX", 4);
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game));
    assert(!brcollision::Convert(one.data(), one.size()-1, game));
    damaged = file;
    damaged.resize(one.size()+7);
    refs.clear();
    assert(brcollision::Index(damaged.data(), damaged.size(), refs) == 1 && refs.size() == 1);
}

// A minimal stored ZIP with nested common-style paths. CustomZip does the
// same central-directory lookup and Extract that CCustomModels uses at boot.
static std::vector<uint8_t> Zip(const std::vector<std::string> &names,
                                const std::vector<std::string> &contents)
{
    std::vector<uint8_t> z;
    std::vector<uint32_t> offsets;
    for(size_t k = 0; k < names.size(); k++) {
        offsets.push_back((uint32_t)z.size());
        A32(z, 0x04034b50); A16(z, 20); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, 0); A32(z, (uint32_t)contents[k].size());
        A32(z, (uint32_t)contents[k].size()); A16(z, (uint16_t)names[k].size()); A16(z, 0);
        z.insert(z.end(), names[k].begin(), names[k].end());
        z.insert(z.end(), contents[k].begin(), contents[k].end());
    }
    uint32_t cd = (uint32_t)z.size();
    for(size_t k = 0; k < names.size(); k++) {
        A32(z, 0x02014b50); A16(z, 20); A16(z, 20); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, 0);
        A32(z, (uint32_t)contents[k].size()); A32(z, (uint32_t)contents[k].size());
        A16(z, (uint16_t)names[k].size()); A16(z, 0); A16(z, 0); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, offsets[k]);
        z.insert(z.end(), names[k].begin(), names[k].end());
    }
    uint32_t cdSize = (uint32_t)z.size()-cd;
    A32(z, 0x06054b50); A16(z, 0); A16(z, 0);
    A16(z, (uint16_t)names.size()); A16(z, (uint16_t)names.size());
    A32(z, cdSize); A32(z, cd); A16(z, 0);
    return z;
}

static void TestZip()
{
    const std::string deep = "data/maps/" + std::string(100, 'x') + "/subdir/sample.ide";
    assert(deep.size() > 127 && deep.size() < CUSTOMZIP_MAX_NAME);
    std::vector<std::string> names = { deep, "data/maps/region/Level_Events/sample.ipl",
                                       "mesh/BRWinter.cls" };
    std::vector<std::string> contents = { "objs\n42927, xmastree, r3, 70, 0\nend\n",
                                          "inst\n42927 xmastree 0 1 2 3 0 0 0 1 -1\nend\n", "x" };
    std::vector<uint8_t> bytes = Zip(names, contents);
    const char *filename = "custom_world_format_test.zip";
    FILE *f = std::fopen(filename, "wb");
    assert(f && std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size());
    std::fclose(f);
    {
        CustomZip zip;
        assert(zip.Open(filename) && zip.GetNumEntries() == 3);
        for(int k = 0; k < 3; k++) {
            assert(zip.GetEntry(k)->name == names[k]); // full nested path, not stem
            std::vector<uint8_t> out(contents[k].size());
            assert(zip.Extract(k, out.data(), (uint32)out.size()));
            assert(std::string((const char *)out.data(), out.size()) == contents[k]);
        }
    }
    std::remove(filename);
}

int main()
{
    TestWorldText();
    TestCollision();
    TestZip();
    std::puts("BR world parser, CED2 collision and nested ZIP tests passed");
}
