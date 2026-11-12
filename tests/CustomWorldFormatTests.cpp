#include "../src/extras/custom/CustomWorldFormat.h"
#include "../src/extras/custom/CommonCatalog.h"
#include "../src/extras/custom/CommonCarcols.h"
#include "../src/extras/custom/BrCollisionFormat.h"
#include "../src/extras/custom/CustomZip.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>

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

// A .cls record in the layout PUNCT ANALYSE v45 section 10.3 documents:
// tag, size, name[24] at +8, the structure at +0x18, then the arrays, with each
// pointer field holding the array's payload offset plus four.
//
//   CLST: spheres x,y,z,r (20 B), boxes 28 B, vertices s16x3/128 (6 B),
//         triangles HHHBB (8 B) ending the record.
//   CLSF: no pointers. 40-byte header, numBoxes at +0x30 and boxes from +0x34,
//         or numBoxes = 0 and numTriangles at +0x34 with float3 vertices and
//         16-byte triangles ending the record.
//
// Bounds are built from the geometry so the record is self-consistent (centre
// at the middle of the box, radius reaching a corner), which is what all 17 594
// CLST records in the corpus do.
static std::vector<uint8_t> MakeBlock(const char *tag = "CLST", bool triangles = true)
{
    const size_t payload = 8, stOff = 0x18;
    const bool clsf = std::strcmp(tag, "CLSF") == 0;

    if(clsf){
        if(!triangles){
            // Box variant: one 1x1x1 box at the origin, 28 bytes from +0x34.
            std::vector<uint8_t> b(payload + stOff + 0x34 + 28, 0);
            std::memcpy(&b[0], "CLSF", 4);
            std::memcpy(&b[payload], "XmasBox1", 8);
            size_t h = payload + stOff;
            WF(b, h + 0x00, -0.5f); WF(b, h + 0x04, -0.5f); WF(b, h + 0x08, -0.5f);
            WF(b, h + 0x0c,  0.5f); WF(b, h + 0x10,  0.5f); WF(b, h + 0x14,  0.5f);
            WF(b, h + 0x18,  0.0f); WF(b, h + 0x1c,  0.0f); WF(b, h + 0x20,  0.0f);
            WF(b, h + 0x24,  0.8660254f);
            W32(b, h + 0x30, 1);                 // numBoxes; +0x34 is the box itself
            WF(b, h + 0x34 +  0, -0.5f); WF(b, h + 0x34 +  4, -0.5f); WF(b, h + 0x34 +  8, -0.5f);
            WF(b, h + 0x34 + 12,  0.5f); WF(b, h + 0x34 + 16,  0.5f); WF(b, h + 0x34 + 20,  0.5f);
            b[h + 0x34 + 24] = 80;               // SA grass -> VC grass
            W32(b, 4, (uint32_t)(b.size() - 8));
            return b;
        }
        // Triangle variant: three float3 vertices at +0x38, then the 16-byte
        // triangles at the end of the record.
        const uint32_t nt = 1;
        size_t vertOff = payload + stOff + 0x38;
        size_t triOff = vertOff + 3 * 12;
        std::vector<uint8_t> b(triOff + nt * 16, 0);
        std::memcpy(&b[0], "CLSF", 4);
        std::memcpy(&b[payload], "XmasBox1", 8);
        size_t h = payload + stOff;
        WF(b, h + 0x00, 0.0f); WF(b, h + 0x04, 0.0f); WF(b, h + 0x08, 0.0f);
        WF(b, h + 0x0c, 2.0f); WF(b, h + 0x10, 2.0f); WF(b, h + 0x14, 0.0f);
        WF(b, h + 0x18, 1.0f); WF(b, h + 0x1c, 1.0f); WF(b, h + 0x20, 0.0f);
        WF(b, h + 0x24, 1.4142135f);
        W32(b, h + 0x30, 0);                     // numBoxes = 0 selects the other body
        W32(b, h + 0x34, nt);                    // numTriangles
        WF(b, vertOff +  0, 0.0f); WF(b, vertOff +  4, 0.0f); WF(b, vertOff +  8, 0.0f);
        WF(b, vertOff + 12, 2.0f); WF(b, vertOff + 16, 0.0f); WF(b, vertOff + 20, 0.0f);
        WF(b, vertOff + 24, 0.0f); WF(b, vertOff + 28, 2.0f); WF(b, vertOff + 32, 0.0f);
        W32(b, triOff + 0, 0); W32(b, triOff + 4, 1); W32(b, triOff + 8, 2);
        W32(b, triOff + 12, 80u << 24);          // surface in the top byte
        W32(b, 4, (uint32_t)(b.size() - 8));
        return b;
    }

    // CLST: one sphere, one box and (optionally) one triangle, so every array
    // the converter walks is exercised. Offsets below are payload-relative, the
    // frame the analysis' field list is written in; a pointer field holds the
    // payload offset plus four, which is what the reader's `st + (field-0x1c)`
    // resolves to the array.
    const size_t nS = 1, nB = 1, nV = triangles ? 3 : 0, nT = triangles ? 1 : 0;
    size_t rel = stOff + 0x48;
    size_t sphRel = rel; rel += nS * 20;
    size_t boxRel = rel; rel += nB * 28;
    size_t vertRel = rel; rel += nV * 6;
    size_t triRel = rel; rel += nT * 8;
    std::vector<uint8_t> b(payload + rel, 0);
    std::memcpy(&b[0], "CLST", 4);
    std::memcpy(&b[payload], "XmasBox1", 8);

    // Geometry: sphere at (3,2,1) r=0.5, box (-1,-1,-1)..(1,1,1), triangle
    // (0,0,0) (1,0,0) (0,2,0) in 1/128 units. Bounds enclose all of it.
    float bmin[3] = { -1.0f, -1.0f, -1.0f };
    float bmax[3] = { 3.5f, 2.5f, 1.5f };
    float ctr[3] = { (bmin[0]+bmax[0])*0.5f, (bmin[1]+bmax[1])*0.5f, (bmin[2]+bmax[2])*0.5f };
    float hx = (bmax[0]-bmin[0])*0.5f, hy = (bmax[1]-bmin[1])*0.5f, hz = (bmax[2]-bmin[2])*0.5f;
    float radius = std::sqrt(hx*hx + hy*hy + hz*hz);

    size_t s = payload + stOff;
    WF(b, s + 0x00, ctr[0]); WF(b, s + 0x04, ctr[1]); WF(b, s + 0x08, ctr[2]);
    WF(b, s + 0x0c, radius);
    WF(b, s + 0x10, bmin[0]); WF(b, s + 0x14, bmin[1]); WF(b, s + 0x18, bmin[2]);
    WF(b, s + 0x1c, bmax[0]); WF(b, s + 0x20, bmax[1]); WF(b, s + 0x24, bmax[2]);
    W16(b, s + 0x28, (uint16_t)nS);
    W16(b, s + 0x2a, (uint16_t)nB);
    W16(b, s + 0x2c, (uint16_t)nT);
    b[s + 0x30] = 0x02;
    auto ptr = [](size_t payloadOff) { return (uint32_t)(payloadOff + 4); };
    W32(b, s + 0x34, ptr(sphRel));
    W32(b, s + 0x38, ptr(boxRel));
    W32(b, s + 0x40, nV ? ptr(vertRel) : 0u);
    W32(b, s + 0x44, nT ? ptr(triRel) : 0u);

    size_t sphAt = payload + sphRel, boxAt = payload + boxRel;
    size_t vertAt = payload + vertRel, triAt = payload + triRel;
    WF(b, sphAt + 0, 3.0f); WF(b, sphAt + 4, 2.0f);
    WF(b, sphAt + 8, 1.0f); WF(b, sphAt + 12, 0.5f);
    b[sphAt + 16] = 255; b[sphAt + 17] = 5;
    WF(b, boxAt +  0, -1.0f); WF(b, boxAt +  4, -1.0f); WF(b, boxAt +  8, -1.0f);
    WF(b, boxAt + 12,  1.0f); WF(b, boxAt + 16,  1.0f); WF(b, boxAt + 20,  1.0f);
    b[boxAt + 24] = 254;
    if(triangles){
        W16(b, vertAt +  0, 0);   W16(b, vertAt +  2, 0);   W16(b, vertAt +  4, 0);
        W16(b, vertAt +  6, 128); W16(b, vertAt +  8, 0);   W16(b, vertAt + 10, 0);
        W16(b, vertAt + 12, 0);   W16(b, vertAt + 14, 256); W16(b, vertAt + 16, 0);
        W16(b, triAt + 0, 0); W16(b, triAt + 2, 1); W16(b, triAt + 4, 2);
        b[triAt + 6] = 255; b[triAt + 7] = 0xff;
    }
    W32(b, 4, (uint32_t)(b.size() - 8));
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
    assert(models[3].kind == customworld::Model::ANIM_OBJECT);
    assert(models[3].txd == "AnimTest" && models[3].anim == "Rclub_SpotAnim");
    const std::string badTxd =
        "anim\n5, testing_model, AnArbitraryTxdLongerThanItsSlot, anim_block, 70, 0\nend\n";
    s = customworld::ParseIde((const unsigned char *)badTxd.data(), badTxd.size(),
        [](const customworld::Model &) { assert(false); });
    assert(s.records == 0 && s.rejected == 1);

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
    assert(std::fabs(axis[2]-1.0f) < 0.001f && w < -0.7f);
    std::string longLine(512, 'A');
    longLine += "\ninst\n1 tree 0 0 0 0 0 0 0 1 -1\n";
    s = customworld::ParseIpl((const unsigned char *)longLine.data(), longLine.size(),
        [&](const customworld::Instance &){});
    assert(s.overlong == 1 && s.records == 1);
    assert(!customworld::Name("this_filename_is_far_too_long_to_fit", 20));
}

static void TestSubmittedWorldLines()
{
    // Actual shapes and representative lines from the supplied 71 IDE / 82 IPL
    // archive. In particular, ID 700 in another IDE must not erase this model.
    const std::string ide =
        "objs\n"
        "700, bar_gatebar01, docentrance, 1, 84, 0\n"
        "716, bar_barrier12, fences, 2, 73, 39, 4\n"
        "817, lamppost2, dynsigns, 3, 78, 200, 60, 128\n"
        "1362, CJ_FIREBIN_(L0), CJ_BINS, 70, 128\n"
        "42495, L1_r3_rdBridgC, r3, 299, 0\n"
        "17239, SM_nn_bt_bld_с, nn_bt, 299, 4194304\n"
        "end\ntobj\n"
        "985, htl_fan_rotate_nt, fanani, 1, 30, 32, 20, 6\n"
        "3115, svalka_beam03, atom889, 250, 68\n"
        "end\nanim\n"
        "10873, Rclub_Spot_osn, AnimTest, Rclub_SpotAnim, 300, 8\n"
        "100165, SM_acc_PlayerBP, SM_acc_PlayerBPSM_acc_PlayerBP, acc_anim, 70, 0\n"
        "end\n";
    customworld::ModelIndex index;
    customworld::Stats s = customworld::ParseIde((const unsigned char *)ide.data(), ide.size(),
        [&index](const customworld::Model &m) { index.Add(m); });
    assert(s.records == 10 && s.rejected == 0);
    assert(index.definitions.at("rclub_spot_osn").txd == "AnimTest");
    assert(index.definitions.at("rclub_spot_osn").anim == "Rclub_SpotAnim");
    assert(index.definitions.at("sm_acc_playerbp").txd == "SM_acc_PlayerBP");
    assert(index.definitions.at("sm_acc_playerbp").anim == "acc_anim");
    assert(index.definitions.at("cj_firebin_(l0)").distance == 70.0f);
    assert(index.definitions.at("sm_nn_bt_bld_c").name == "SM_nn_bt_bld_c");
    assert(index.definitions.at("bar_gatebar01").distance == 84.0f);
    assert(index.definitions.at("bar_gatebar01").flags == 0);
    assert(index.definitions.at("bar_barrier12").distance == 73.0f);
    assert(index.definitions.at("bar_barrier12").flags == 4);
    assert(index.definitions.at("lamppost2").distance == 200.0f);
    assert(index.definitions.at("lamppost2").flags == 128);
    assert(index.definitions.at("htl_fan_rotate_nt").kind == customworld::Model::TIME_OBJECT);
    assert(index.definitions.at("htl_fan_rotate_nt").distance == 30.0f);
    assert(index.definitions.at("htl_fan_rotate_nt").flags == 32);
    assert(index.definitions.at("htl_fan_rotate_nt").timeOn == 20);
    assert(index.definitions.at("svalka_beam03").kind == customworld::Model::OBJECT);
    assert(index.definitions.at("svalka_beam03").distance == 250.0f);
    customworld::Model reused = index.definitions.at("bar_gatebar01");
    reused.name = "OTHER_GATE";
    index.Add(reused);
    assert(index.conflicts == 1 && index.definitions.count("bar_gatebar01") == 1);
    assert(index.definitions.count("other_gate") == 1);
    assert(index.lastNameForId.at(700) == "other_gate");
    reused.id = 701; reused.name = "bar_gatebar01";
    index.Add(reused);
    assert(index.sharedNames == 1 && index.lastNameForId.at(701) == "bar_gatebar01");
    assert(index.definitions.at("bar_gatebar01").id == 701);

    const std::string ipl =
        "inst\n"
        // Marketplace line with a non-ASCII trailing // comment.
        "3131, SM_mrktplace_ext_a, 0, 2566.25854, -2517.95679, 23.04858, 0.0, 0.0, 1.50996e-07, 1.0, -1 // Южный - 1\n"
        // BRmap_Interiors: malformed decimal comma in the Z field.
        "17620, K7, 1, 2503.62245, 2443.6332, 2499,8149, 0.0, 0.0, 1.0, 1.62921e-007, -1\n"
        "100255  1_ql2_arrow  0  311.70410  221.73036  11.34064  0.00000  0.00000  -0.81620  0.57777  200.0\n"
        "100301  1_ql2_startline  0  2441.72938  -2695.14984  24.32877  0.00000  0.00000  -0.70560  0.70861  299.0\n"
        "42495, L1_r3_rdBridgС, 0, -2164.13525, 173.86201, 21.60893, 0.0, 0.0, -0.220659, 0.975351, -1\n"
        // The truncated winter quaternion is NOT repairable from the text.
        "13592, ny_snegk8, 0, 556.78, -1250.0, 39.3712, -0.0422235, 0.00 54, 0.104189, 0.99366, -1\n"
        "end\n";
    std::vector<customworld::Instance> places;
    s = customworld::ParseIpl((const unsigned char *)ipl.data(), ipl.size(),
        [&places](const customworld::Instance &inst) { places.push_back(inst); });
    assert(s.records == 5 && s.rejected == 1 && s.overlong == 0);
    assert(places[4].name == "L1_r3_rdBridgC");
    assert(index.definitions.count(customworld::Lower(places[4].name)) == 1);
    assert(places[0].name == "SM_mrktplace_ext_a" && places[0].lod == -1);
    assert(places[1].name == "K7" && std::fabs(places[1].pos[2] - 2499.8149f) < 0.001f);
    assert(places[2].lod == -1 && places[3].lod == -1);

    std::unordered_set<customworld::PlacementKey, customworld::PlacementHash> seen;
    assert(seen.insert(customworld::PlacementKey(50, places[0])).second);
    customworld::Instance same = places[0];
    same.id += 1000; // another IDE reuses its numeric ID
    for(int k = 0; k < 4; k++) same.quat[k] = -same.quat[k];
    assert(!seen.insert(customworld::PlacementKey(50, same)).second); // q == -q
    same.pos[0] += 0.01f;
    assert(seen.insert(customworld::PlacementKey(50, same)).second);
    assert(seen.insert(customworld::PlacementKey(51, places[0])).second);
    same = places[0]; same.interior = 1;
    assert(seen.insert(customworld::PlacementKey(50, same)).second);
}

static void TestCommonCatalogAndTypes()
{
    const std::string manifest =
        "\xef\xbb\xbfIDE DATA\\MAPS\\IDE\\Main.IDE\n"
        "# IPL DATA\\MAPS\\EVENT\\winter.ipl\n"
        "CDIMAGE mesh\\br_map_00.bpc\n"
        "IDE data/maps/ide/main.ide\n" // duplicate, not loaded twice
        "IPL DATA\\MAPS\\Main.IPL\n"
        "IPL data/maps/main.ipl\n"
        "IDE data/../audio/audo.ide\n"
        "IPL /data/maps/absolute.ipl\n"
        "IDE data/features/trick.bpc\n"
        "HANDLING data/handling.cfg\n";
    commoncatalog::Catalog cfg = commoncatalog::Parse(
        (const unsigned char*)manifest.data(), manifest.size());
    assert(cfg.ide.size() == 1 && cfg.ide[0] == "data/maps/ide/main.ide");
    assert(cfg.ipl.size() == 1 && cfg.ipl[0] == "data/maps/main.ipl");
    assert(cfg.invalid == 3);
    assert(commoncatalog::Path("data\\maps\\Main.IPL") == "data/maps/main.ipl");
    assert(commoncatalog::Path("../audio/audo.ide").empty());
    assert(!commoncatalog::CanReadData("audio/audo.dat"));
    assert(!commoncatalog::CanReadData("data/features/reflections.json"));
    assert(!commoncatalog::CanReadData("data/handling.cfg"));
    assert(commoncatalog::CanReadData("data/carcols.dat"));
    assert(commoncatalog::CanReadData("data/water.dat"));
    assert(!commoncatalog::CanReadData("data/weapons.ide"));

    const std::string ide =
        "peds\n"
        "3, ANDRE, ANDRE, CIVMALE, STAT_SENSIBLE_GUY, man, 0, 0, man, 1,4, PED_TYPE_GEN, VOICE_GEN, VOICE_GEN\n"
        "4, badped, badped, CIVMALE, STAT_STD_MISSION, man, 0, 0, man, 9999,4\n" // malformed field
        "end\ncars\n"
        "400, landstal, landstal, car, LANDSTAL, LANDSTK, null, normal, 7, 0, 0, -1, 0.75, 0.75, 0\n"
        "435, artict1, artict1, trailer, ARTICT1, ARTICT1, null, big, 5, 1, 0, -1, 1.0, 1.0, -1\n"
        "end\nweap\n"
        "321, gun_dildo1, 50\n"
        "322, invalid_weapon_name_is_too_long, 50\n"
        "end\n";
    std::vector<customworld::Model> entries;
    customworld::Stats s = customworld::ParseIde((const unsigned char *)ide.data(), ide.size(),
        [&](const customworld::Model &m) { entries.push_back(m); });
    assert(s.records == 4 && s.rejected == 2 && s.overlong == 0);
    assert(entries[0].kind == customworld::Model::PED && entries[0].name == "ANDRE");
    assert(entries[0].pedStats == "STAT_SENSIBLE_GUY" && entries[0].animGroup == "man");
    assert(entries[0].radio1 == 1 && entries[0].radio2 == 4);
    // The supplied peds.ide also uses radio IDs 10/11. We do not feed
    // these SA audio IDs into VC, but they must not discard the ped skin.
    const std::string morePeds =
        "peds\n24, WBDYG1, WBDYG1, CIVMALE, STAT_TOUGH_GUY, man, 170F, 1, man, 10, 11, PED_TYPE_GEN\n"
        "11950, MK_wfesteraddams_man, MK_wfesteraddams_man, CRIMINAL, STAT_CRIMINAL, man, 110F, 0, man, 4, 4\nend\n";
    std::vector<customworld::Model> extendedPeds;
    customworld::Stats pedStats = customworld::ParseIde(
        (const uint8_t*)morePeds.data(), morePeds.size(),
        [&](const customworld::Model &m) { extendedPeds.push_back(m); });
    assert(pedStats.records == 1 && pedStats.rejected == 1);
    assert(extendedPeds[0].radio1 == 10 && extendedPeds[0].radio2 == 11);
    assert(entries[1].kind == customworld::Model::VEHICLE && entries[1].subtype == "car");
    assert(entries[1].handling == "LANDSTAL" && entries[1].gameName == "LANDSTK");
    assert(entries[2].kind == customworld::Model::VEHICLE && entries[2].subtype == "trailer");
    assert(entries[3].kind == customworld::Model::WEAPON && entries[3].name == "gun_dildo1");
    assert(entries[3].distance == 50.0f);

    const std::string colors = "col\n0,0,0 # black\n 245, 245, 245\nend\ncar\nvehicle 1 2\nend\n";
    std::vector<commoncarcols::RGB> palette;
    assert(commoncarcols::Parse((const uint8_t*)colors.data(), colors.size(), palette));
    assert(palette.size() == 2 && palette[0].r == 0 && palette[1].b == 245);
    const std::string badColors = "col\n255,0,0\n256,0,0\nend\n";
    assert(!commoncarcols::Parse((const uint8_t*)badColors.data(), badColors.size(), palette));
    const std::string noColors = "car\nvehicle, 1, 2\nend\n";
    assert(!commoncarcols::Parse((const uint8_t*)noColors.data(), noColors.size(), palette));
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

    // The converted block is a COLL the game's own reader accepts: tag, size,
    // name[24], then radius/centre/bbox at +32 and the five sections at +72.
    std::vector<uint8_t> game;
    const char *why = nullptr;
    assert(brcollision::Convert(one.data(), one.size(), game, &why));
    assert(std::memcmp(game.data(), "COLL", 4) == 0 &&
           brcollision::U32(game.data()+4) == game.size()-8);
    assert(std::strcmp((const char *)game.data()+8, "XmasBox1") == 0);
    // Centre of the box that encloses the sphere, the box and the triangle.
    // sqrt(2.25^2 + 1.75^2 + 1.25^2): the corner of the box the geometry fills.
    assert(std::fabs(brcollision::F32(game.data()+32) - 3.11247f) < 1e-3f);
    assert(std::fabs(brcollision::F32(game.data()+36) - 1.25f) < 1e-4f);
    assert(std::fabs(brcollision::F32(game.data()+40) - 0.75f) < 1e-4f);
    assert(std::fabs(brcollision::F32(game.data()+44) - 0.25f) < 1e-4f);

    assert(brcollision::U32(game.data()+72) == 1);          // spheres
    // CLST stores x,y,z,r and COLL stores r,x,y,z.
    assert(std::fabs(brcollision::F32(game.data()+76) - 0.5f) < 1e-5f);
    assert(std::fabs(brcollision::F32(game.data()+80) - 3.0f) < 1e-5f);
    assert(game[92] == 0 && game[93] == 5);                 // SA surface 255 clamped
    assert(brcollision::U32(game.data()+96) == 0);          // lines
    assert(brcollision::U32(game.data()+100) == 1);         // boxes
    assert(std::fabs(brcollision::F32(game.data()+104) - (-1.0f)) < 1e-5f);
    assert(game[128] == 0);                                 // SA surface 254 clamped
    assert(brcollision::U32(game.data()+132) == 3);         // vertices: maxIndex+1
    assert(std::fabs(brcollision::F32(game.data()+148) - 1.0f) < 1e-5f); // 128/128
    assert(brcollision::U32(game.data()+172) == 1);         // triangles
    assert(brcollision::U32(game.data()+176) == 0);
    assert(brcollision::U32(game.data()+180) == 1);         // face indices
    assert(brcollision::U32(game.data()+184) == 2);
    assert(game.size() == 188 + 4);
    assert(game.size() == 72 + 4 + 20 + 4 + 4 + 28 + 4 + 36 + 4 + 16);

    // The CLSF box variant converts as well, and its single box survives.
    assert(brcollision::Convert(two.data(), two.size(), game, &why));
    assert(std::memcmp(game.data(), "COLL", 4) == 0);
    assert(brcollision::U32(game.data()+72) == 0);          // spheres
    assert(brcollision::U32(game.data()+76) == 0);          // lines
    assert(brcollision::U32(game.data()+80) == 1);          // boxes
    assert(brcollision::U32(game.data()+112) == 0);         // vertices
    assert(brcollision::U32(game.data()+116) == 0);         // triangles

    // A CLSF whose vertices are float3 keeps them as float metres - there is no
    // 1/128 quantisation in that body, and no +/-256 m clamp either. The counts
    // sit behind an empty sphere and line section, so: 0 spheres, 0 lines,
    // 0 boxes, 3 vertices, 1 triangle.
    std::vector<uint8_t> clsfTris = MakeBlock("CLSF", true);
    assert(brcollision::Convert(clsfTris.data(), clsfTris.size(), game, &why));
    assert(brcollision::U32(game.data()+72) == 0);          // spheres
    assert(brcollision::U32(game.data()+96) == 0);          // boxes
    assert(brcollision::U32(game.data()+84) == 3);          // vertices
    // Vertex 0 is (0,0,0) and vertex 1 is (2,0,0), so x of the second float is 2.
    assert(std::fabs(brcollision::F32(game.data()+100) - 2.0f) < 1e-5f);
    assert(brcollision::U32(game.data()+124) == 1);         // triangles
    assert(brcollision::U32(game.data()+128) == 0);
    assert(brcollision::U32(game.data()+132) == 1);
    assert(brcollision::U32(game.data()+136) == 2);

    // Damage that has to be refused rather than half-read.
    std::vector<uint8_t> damaged = one;
    W32(damaged, 4, 0x7FFFFFF0u);                            // size outside the file
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    damaged = one;
    W16(damaged, damaged.size()-6, 50);                      // vertex that is not there
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    damaged = one;
    WF(damaged, 8 + 0x18 + 0x0c, -1.0f);                     // negative bounding radius
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    damaged = one;
    // A box that is inside out. The box array is found the way the converter
    // finds it: the pointer field holds the payload offset plus four, so the
    // file offset is the field value plus four.
    {
        size_t boxAt = (size_t)brcollision::U32(damaged.data() + 8 + 0x18 + 0x38) + 4;
        WF(damaged, boxAt, 5.0f);                            // min.x past max.x
    }
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    damaged = one;
    W32(damaged, 8 + 0x18 + 0x34, 0);                        // sphere pointer dropped
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    damaged = one;
    W32(damaged, 8 + 0x18 + 0x44, 4);                        // pointer field below 0x1c
    assert(!brcollision::Convert(damaged.data(), damaged.size(), game, &why));
    assert(!brcollision::Convert(one.data(), one.size()-1, game, &why));
    // Truncating the container stops the walk at the record that does not fit.
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

static void TestLodIdeAndLodsInfo()
{
    const char *lodIde =
        "#BlackRussia\n"
        "objs\n"
        "18850, LODbui_bat_98, bat_lod1, 1500, 0\n"
        "35622, 2_r3_bsv_Rtnd, r3_bsv_build, 80, 0\n"
        "35863, lf_kor_h1_bld_LOD2, r3, 600, 0\n"
        "43326, L2_E_CityShop_10, BR_E_CityShop_2, 1500, 0\n"
        "end\n";
    customworld::ModelIndex idx;
    customworld::Stats st = customworld::ParseIde(
        (const unsigned char *)lodIde, std::strlen(lodIde),
        [&idx](const customworld::Model &m) { idx.Add(m); });
    assert(st.records == 4 && st.rejected == 0);
    assert(idx.definitions.count("l2_e_cityshop_10") == 1);
    assert(idx.definitions["l2_e_cityshop_10"].distance == 1500.0f);
    assert(idx.definitions["2_r3_bsv_rtnd"].distance == 80.0f);

    std::vector<std::string> cands;
    customworld::CollectLodBaseCandidates("lodbui_bat_98", cands);
    assert(!cands.empty() && cands[0] == "bui_bat_98");

    customworld::CollectLodBaseCandidates("l2_e_cityshop_10", cands);
    assert(cands.size() >= 2 && cands[0] == "e_cityshop_10" && cands[1] == "l1_e_cityshop_10");

    customworld::CollectLodBaseCandidates("2_r3_bsv_rtnd", cands);
    assert(cands.size() >= 2 && cands[0] == "1_r3_bsv_rtnd" && cands[1] == "r3_bsv_rtnd");

    customworld::CollectLodBaseCandidates("lod_ars_bush2", cands);
    assert(cands.size() >= 2 && cands[0] == "ars_bush2" && cands[1] == "sm_ars_bush2");

    customworld::CollectLodBaseCandidates("lf_kor_h1_bld_lod2", cands);
    assert(!cands.empty() && cands[0] == "lf_kor_h1_bld");
}

int main()
{
    TestWorldText();
    TestSubmittedWorldLines();
    TestCommonCatalogAndTypes();
    TestCollision();
    TestZip();
    TestLodIdeAndLodsInfo();
    std::puts("BR world parser, CED2 collision and nested ZIP tests passed");
}
