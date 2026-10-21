// Exercise the REAL custom archive scanner, IMG mounting and per-model
// collision enumeration with small synthetic ZIP/IMG files. Standalone test:
// unused game/RenderWare sections are garbage-collected at link time.
#include "common.h"
#include "CustomModels.h"
#include "CustomColFormat.h"
#include "CustomImgFormat.h"
#include "ModelLoadDispatch.h"
#include "CdStream.h"
#include "TxdStore.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

static std::vector<std::string> images;
extern "C" bool CdStreamAddImage(const char *path) { images.push_back(path); return true; }
extern "C" int32 CdStreamGetNumImages(void) { return (int32)images.size(); }
extern "C" char *CdStreamGetImageName(int cd) { return &images.at(cd)[0]; }
void DebugLogPrintf(const char *, ...) {}
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
FILE *_fcaseopen(const char *name, const char *mode) { return std::fopen(name, mode); }
RwBool RwTexDictionaryDestroy(RwTexDictionary *) { return true; }

static void W16(std::vector<uint8> &b, size_t at, uint16 n) {
    b[at] = (uint8)n; b[at+1] = (uint8)(n>>8);
}
static void W32(std::vector<uint8> &b, size_t at, uint32 n) {
    W16(b, at, n); W16(b, at+2, n>>16);
}
static void WF(std::vector<uint8> &b, size_t at, float x) {
    uint32 n; std::memcpy(&n, &x, 4); W32(b, at, n);
}
static void A16(std::vector<uint8> &b, uint16 n) {
    b.push_back((uint8)n); b.push_back((uint8)(n>>8));
}
static void A32(std::vector<uint8> &b, uint32 n) {
    A16(b, n); A16(b, n>>16);
}
static std::vector<uint8> Bytes(const std::string &s) {
    return std::vector<uint8>(s.begin(), s.end());
}
static std::vector<uint8> RawBtx(uint32 w, uint32 h) {
    std::vector<uint8> file(4 + 64 + 4 + (size_t)w*h*4, 0);
    std::fill(file.begin() + 4 + 68, file.end(), 255);
    const uint8 magic[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };
    std::memcpy(file.data() + 4, magic, 12);
    W32(file, 4+12, 0x04030201); W32(file, 4+16, 0x1401);
    W32(file, 4+20, 1); W32(file, 4+24, 0x1908);
    W32(file, 4+28, 0x8058); W32(file, 4+32, 0x1908);
    W32(file, 4+36, w); W32(file, 4+40, h);
    W32(file, 4+52, 1); W32(file, 4+56, 1);
    W32(file, 4+64, w*h*4);
    return file;
}
static void Write(const char *path, const std::vector<uint8> &bytes) {
    FILE *f = std::fopen(path, "wb");
    assert(f && std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size());
    std::fclose(f);
}

// 92-byte VC COLL bounding box with no geometry. Its name is INTERNAL to a
// world.col file, not the filename of the archive entry.
static std::vector<uint8> Col(const char *name) {
    std::vector<uint8> b(92, 0);
    std::memcpy(b.data(), "COLL", 4); W32(b, 4, 84);
    std::memcpy(b.data()+8, name, std::strlen(name));
    WF(b, 32, 2);
    WF(b, 48, -1); WF(b, 52, -1); WF(b, 56, -1);
    WF(b, 60, 1); WF(b, 64, 1); WF(b, 68, 1);
    assert(customcolformat::ValidVC(b.data(), b.size()));
    return b;
}
static std::vector<uint8> Col3(const char *name) {
    std::vector<uint8> b(120, 0);
    std::memcpy(b.data(), "COL3", 4); W32(b, 4, 112);
    std::memcpy(b.data()+8, name, std::strlen(name));
    WF(b, 32, -1); WF(b, 36, -1); WF(b, 40, -1);
    WF(b, 44, 1); WF(b, 48, 1); WF(b, 52, 1);
    WF(b, 56, 2); // bounds in SA COL3 box-then-sphere order
    return b;
}
static std::vector<uint8> Cls(const char *name) {
    std::vector<uint8> b(120, 0);
    std::memcpy(b.data(), "CLST", 4); W32(b, 4, 112);
    std::memcpy(b.data()+8, name, std::strlen(name));
    std::memcpy(b.data()+28, "CED2", 4);
    WF(b, 44, 2);
    WF(b, 48, -1); WF(b, 52, -1); WF(b, 56, -1);
    WF(b, 60, 1); WF(b, 64, 1); WF(b, 68, 1);
    return b;
}
static std::vector<uint8> LegacyCls(const char *name) {
    std::vector<uint8> b = Cls(name);
    std::memcpy(b.data()+28, "OLD0", 4); // not a CED2 block
    return b;
}

struct File { std::string path; std::vector<uint8> contents; };
static void Zip(const char *path, const std::vector<File> &files) {
    std::vector<uint8> z;
    std::vector<uint32> offsets;
    for(size_t i = 0; i < files.size(); i++) {
        offsets.push_back((uint32)z.size());
        const File &f = files[i];
        A32(z, 0x04034b50); A16(z, 20); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, 0); A32(z, (uint32)f.contents.size());
        A32(z, (uint32)f.contents.size()); A16(z, (uint16)f.path.size()); A16(z, 0);
        z.insert(z.end(), f.path.begin(), f.path.end());
        z.insert(z.end(), f.contents.begin(), f.contents.end());
    }
    uint32 cd = (uint32)z.size();
    for(size_t i = 0; i < files.size(); i++) {
        const File &f = files[i];
        A32(z, 0x02014b50); A16(z, 20); A16(z, 20); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, 0); A32(z, (uint32)f.contents.size());
        A32(z, (uint32)f.contents.size()); A16(z, (uint16)f.path.size());
        A16(z, 0); A16(z, 0); A16(z, 0); A16(z, 0);
        A32(z, 0); A32(z, offsets[i]);
        z.insert(z.end(), f.path.begin(), f.path.end());
    }
    uint32 len = (uint32)z.size()-cd;
    A32(z, 0x06054b50); A16(z, 0); A16(z, 0);
    A16(z, (uint16)files.size()); A16(z, (uint16)files.size());
    A32(z, len); A32(z, cd); A16(z, 0);
    Write(path, z);
}

static void WorldFile(const char *name, const unsigned char *data, size_t size, void *user) {
    std::map<std::string, std::string> &out = *(std::map<std::string, std::string> *)user;
    out[name] = std::string((const char *)data, size);
}
static void Collision(const char *name, const unsigned char *data, size_t size, void *user) {
    std::map<std::string, std::string> &out = *(std::map<std::string, std::string> *)user;
    assert(customcolformat::ValidVC(data, size));
    out[name] = std::string((const char *)data+8); // internal model name in VC block
}
static void Ced2(const char *name, const unsigned char *data, size_t size, void *user) {
    std::map<std::string, std::string> &out = *(std::map<std::string, std::string> *)user;
    assert(size == 120 && !std::memcmp(data, "CLST", 4));
    out[name] = std::string((const char *)data+8);
}

int main() {
    // A DFF's filename does not determine which model-specific loader runs.
    assert(GetModelLoadKind(MITYPE_SIMPLE) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_TIME) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_WEAPON) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_CLUMP) == MODELLOAD_CLUMP);
    assert(GetModelLoadKind(MITYPE_VEHICLE) == MODELLOAD_VEHICLE);
    assert(GetModelLoadKind(MITYPE_PED) == MODELLOAD_PED);
    assert(GetModelLoadKind(MITYPE_MLO) == MODELLOAD_UNSUPPORTED);
    assert(GetModelLoadKind(MITYPE_XTRACOMPS) == MODELLOAD_UNSUPPORTED);
    assert(!IsModelClumpCompatible(MODELLOAD_PED, true, true, false, false)); // no skin: hit/animation code unsafe
    assert(IsModelClumpCompatible(MODELLOAD_PED, true, true, true, true)); // VC or SA skin
    assert(!IsModelClumpCompatible(MODELLOAD_PED, true, true, true, false));
    assert(IsModelClumpCompatible(MODELLOAD_VEHICLE, true, true, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_VEHICLE, false, true, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_CLUMP, true, false, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_ATOMIC, true, true, false, false));
    char temp[] = "revc-custom-layout-XXXXXX";
    char *dir = mkdtemp(temp);
    assert(dir && chdir(dir) == 0);
    assert(mkdir("img", 0700) == 0 && mkdir("custom", 0700) == 0);
    assert(mkdir("zip", 0700) == 0);
    Zip("zip/visuals.ZIP", {
        { "vehicles/CarReflectionMask.BTX", RawBtx(32, 32) },
        { "lightmaps/window_LM.btx", RawBtx(32, 16) },
        { "lightmaps/preview_lightmap.btx", RawBtx(8, 8) },
        { "scenes/unrelated.btx", RawBtx(32, 32) }, // metadata only, NEVER a base TXD
        { "models/forbidden.dff", Bytes("do not load") },
        { "maps/forbidden.ide", Bytes("objs\n1 forbidden bad 100 0\nend\n") },
        { "audio/forbidden.audo", Bytes("do not load") }
    });
    std::vector<uint8> img(4*2048, 0), directory(3*32, 0);
    W32(directory, 0, 1); W32(directory, 4, 2);
    std::memcpy(directory.data()+8, "house1.dff", 11);
    W32(directory, 32, 3); W32(directory, 36, 1);
    std::memcpy(directory.data()+40, "R3.txd", 7); // IDE points here, not to house1.txd
    W32(directory, 64, 99); W32(directory, 68, 9000);
    std::memcpy(directory.data()+72, "world.col", 10); // must be ignored
    Write("img/map.img", img); Write("img/map.dir", directory);
    assert(mkdir("img/area", 0700) == 0);
    std::vector<uint8> extraDir(32, 0);
    W32(extraDir, 0, 1); W32(extraDir, 4, 1);
    std::memcpy(extraDir.data()+8, "HOUSE1.TXD", 11); // separate IMG from house1.dff
    Write("img/area/extra.img", img); Write("img/area/extra.dir", extraDir);
    Write("img/no_dir.img", img);
    img.assign(4*2048, 0); std::memcpy(img.data(), "VER2", 4);
    Write("img/v2.img", img); Write("img/v2.dir", directory);
    assert(mkdir("models", 0700) == 0);
    std::vector<uint8> nativeDir(4*32, 0);
    W32(nativeDir, 0, 1); W32(nativeDir, 4, 1);
    std::memcpy(nativeDir.data()+8, "R3.TXD", 7); // shared with auto IMG
    W32(nativeDir, 32, 2); W32(nativeDir, 36, 1);
    std::memcpy(nativeDir.data()+40, "baseonly.TXD", 13);
    W32(nativeDir, 64, 3); W32(nativeDir, 68, 1);
    std::memcpy(nativeDir.data()+72, "baseonly.DFF", 13);
    W32(nativeDir, 96, 4); W32(nativeDir, 100, 1);
    std::memcpy(nativeDir.data()+104, "house1.TXD", 11); // auto IMG wins on duplicate
    Write("models/gta3.DIR", nativeDir);
    images.push_back("models/gta3.img");
    CTxdStore::Initialise();
    CCustomModels::ResetImageMounts(); CCustomModels::MountImages();
    assert(CTxdStore::FindTxdSlot("r3") >= 0);
    assert(CTxdStore::FindTxdSlot("BASEONLY") >= 0); // native slots reserved first
    assert(images.size() == 3 && CCustomModels::IsAutoMountedImage(1) &&
           CCustomModels::IsAutoMountedImage(2));
    assert(images[1].find("area/extra.img") != std::string::npos); // sorted
    assert(!CCustomModels::IsAutoMountedImage(0));
    assert(CCustomModels::PairedImageTxd("HoUsE1") == 1); // DFF in IMG 2, TXD in IMG 1
    assert(CCustomModels::PairedImageTxd("R3") == -1); // no matching DFF
    assert(CCustomModels::PairedImageTxd("baseonly") == -1); // paired, but stock IMG
    uint32 specialPos = 0, specialSize = 0;
    assert(CCustomModels::FindModelDffInImage("HoUsE1", specialPos, specialSize));
    assert(specialPos == (2u<<24 | 1u) && specialSize == 2); // no IDE required to find a script DFF
    assert(CCustomModels::FindModelDffInImage("baseonly", specialPos, specialSize));
    assert(specialPos == 3 && specialSize == 1); // stock IMG also searchable when bound to an IDE
    assert(!CCustomModels::FindModelDffInImage("missing", specialPos, specialSize));
    CCustomModels::MountImages(); assert(images.size() == 3); // idempotent

    Zip("custom/common.zip", {
        { "data/maps/IDE/test.ide", Bytes("objs\n1 house1 r3 100 0\nend\n") },
        { "maps/IDE/test.ide", Bytes("objs\n1 house1 r3 100 0\nend\n") },
        { "data/maps/Level_Events/test.ipl", Bytes("inst\n1 house1 0 0 0 0 0 0 0 1 -1\nend\n") },
        { "maps/Level_Events/test.ipl", Bytes("inst\n1 house1 0 0 0 0 0 0 0 1 -1\nend\n") },
        { "data/maps/Level_Events/alternate.ipl", Bytes("inst\n2 house1 0 1 0 0 0 0 0 1 -1\nend\n") },
        // A different version under the alias must remain selectable/visible.
        { "maps/Level_Events/alternate.ipl", Bytes("inst\n2 house1 0 2 0 0 0 0 0 1 -1\nend\n") },
        { "models/SPECIAL1.dff", Bytes("not a valid DFF - only metadata is indexed") }
    });
    // Identical .cls and .col basenames in different ZIPs; each carries a
    // different internal model name. Both must reach the world loader.
    std::vector<uint8> twoBlocks = Col("house3");
    std::vector<uint8> override = Col("house1");
    WF(override, 32, 3); // .col wins over the house1 .cls (radius 2)
    twoBlocks.insert(twoBlocks.end(), override.begin(), override.end());
    std::vector<uint8> badOffset = Col3("badOffset");
    W32(badOffset, 84, 0xfffffff0); // offS without spheres must still be in bounds
    Zip("custom/coll_a.zip", {
        { "mesh/shared.cls", Cls("house1") }, { "mesh/shared.col", twoBlocks },
        { "mesh/legacy.cls", LegacyCls("house5") }
    });
    Zip("custom/coll_b.zip", {
        { "mesh/shared.cls", Cls("house2") }, { "mesh/shared.col", Col("house4") },
        { "mesh/legacy.cls", LegacyCls("house6") }, { "mesh/sa.col", Col3("house7") },
        { "mesh/bad.col", badOffset }
    });
    std::map<std::string, std::string> ide, ipl, cls, col;
    assert(CCustomModels::HasGraphicsReflections());
    assert(CCustomModels::HasGraphicsLightmaps());
    assert(!CCustomModels::HasModelFile("forbidden"));
    assert(!CCustomModels::HasArchiveModelFile("forbidden"));
    assert(CCustomModels::HasModelFile("house1")); // mounted DFF before Streaming::Init
    assert(CCustomModels::HasModelFile("baseonly")); // usable stock DFF even without its IDE
    assert(CCustomModels::HasArchiveModelFile("SPECIAL1")); // archive-only special, no IMG DIR
    assert(!CCustomModels::HasArchiveModelFile("baseonly"));
    CCustomModels::ForEachWorldFile(true, WorldFile, &ide);
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    CCustomModels::ForEachBrCollision(Ced2, &cls);
    CCustomModels::ForEachGameCollision(Collision, &col);
    assert(ide.size() == 1 && ide.count("data/maps/ide/test.ide"));
    assert(ide["data/maps/ide/test.ide"].find("house1 r3") != std::string::npos);
    assert(CCustomModels::PairedImageTxd("house1") == 1); // prefer IMG 1 TXD, not IDE r3
    assert(ipl.size() == 3 && ipl.count("data/maps/level_events/test.ipl") &&
           !ipl.count("maps/level_events/test.ipl") && // exact alias skipped
           ipl.count("data/maps/level_events/alternate.ipl") &&
           ipl.count("maps/level_events/alternate.ipl")); // distinct variant kept
    assert(ipl["maps/level_events/alternate.ipl"].find("house1 0 2 0 0") != std::string::npos);
    Write("custom/active_ipl.txt", Bytes("# Select one variant, using the full path inside common.zip\n"
                                         " DATA\\Maps\\Level_Events\\test.IPL // selected\n"
                                         "data/maps/level_events/unknown.ipl\n"));
    ipl.clear();
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 1 && ipl.count("data/maps/level_events/test.ipl"));
    ide.clear();
    CCustomModels::ForEachWorldFile(true, WorldFile, &ide); // IDEs are not filtered
    assert(ide.size() == 1);
    Write("custom/active_ipl.txt", Bytes("maps/Level_Events/test.ipl\n"));
    ipl.clear();
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 1 && ipl.count("maps/level_events/test.ipl"));
    Write("custom/active_ipl.txt", Bytes("# empty selection intentionally disables BR IPLs\n"));
    ipl.clear();
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.empty());
    assert(std::remove("custom/active_ipl.txt") == 0);
    assert(cls.size() == 2 && cls["house1"] == "house1" && cls["house2"] == "house2");
    assert(col.size() == 6 && col["house1"] == "house1" &&
           col["house3"] == "house3" && col["house4"] == "house4" &&
           col["house5"] == "house5" && col["house6"] == "house6" &&
           col["house7"] == "house7");
    std::vector<uint8> selected;
    assert(CCustomModels::GetCollisionBlock("house1", selected));
    float radius = 0; std::memcpy(&radius, selected.data()+24, 4);
    assert(radius == 3); // .col, NOT the other ZIP's same-named container or .cls
    assert(CCustomModels::GetCollisionBlock("house2", selected));
    std::memcpy(&radius, selected.data()+24, 4);
    assert(radius == 2); // .cls from coll_b.zip
    assert(CCustomModels::GetCollisionBlock("house6", selected));
    assert(std::string((const char *)selected.data()) == "house6"); // legacy .cls
    assert(CCustomModels::GetCollisionBlock("house7", selected));
    assert(selected.size() == 84 && std::string((const char *)selected.data()) == "house7");
    std::memcpy(&radius, selected.data()+24, 4);
    assert(radius == 2); // SA COL3 became a VC COLL payload
    assert(!CCustomModels::GetCollisionBlock("badOffset", selected));

    // Re-scan after installing a synthetic ALREADY unpacked common/ tree.
    // Keep its event IPL present but commented out in config.dat: selecting
    // every .ipl on disk would enable mutually exclusive scenes.
    CCustomModels::Shutdown();
    assert(mkdir("common", 0700) == 0);
    assert(mkdir("common/data", 0700) == 0);
    assert(mkdir("common/data/maps", 0700) == 0);
    assert(mkdir("common/data/maps/ide", 0700) == 0);
    assert(mkdir("common/data/maps/level_events", 0700) == 0);
    assert(mkdir("common/data/features", 0700) == 0);
    assert(mkdir("common/audio", 0700) == 0);
    Write("common/data/config.dat", Bytes(
        "CDIMAGE mesh\\\\br_map_00.bpc\n"
        "IDE data\\\\maps\\\\ide\\\\test.ide\n"
        "IDE DATA\\\\VEHICLES.IDE\n"
        "IPL DATA\\\\MAPS\\\\Level_Events\\\\test.IPL\n"
        "IPL data/maps/level_events/new.ipl\n"
        "# IPL data/maps/level_events/event.ipl\n"));
    Write("common/data/maps/ide/test.ide", Bytes("objs\n1 house1 r3 199 0\nend\n"));
    Write("common/data/vehicles.ide", Bytes("cars\n400 landstal landstal car LANDSTAL LANDSTK null normal 7 0 0 -1 0.75 0.75 0\nend\n"));
    Write("common/data/maps/level_events/test.ipl", Bytes("inst\n1 house1 0 9 0 0 0 0 0 1 -1\nend\n"));
    Write("common/data/maps/level_events/new.ipl", Bytes("inst\n1 house1 0 3 0 0 0 0 0 1 -1\nend\n"));
    Write("common/data/maps/level_events/event.ipl", Bytes("inst\n1 house1 0 8 0 0 0 0 0 1 -1\nend\n"));
    Write("common/data/timecyc.json", Bytes("{\"ambient\":1}"));
    Write("common/data/carcols.dat", Bytes("col\n1,2,3\nend\n"));
    Write("common/data/features/ignored.ide", Bytes("objs\n2 evil evil 100 0\nend\n"));
    Write("common/audio/ignored.ipl", Bytes("inst\n2 evil 0 0 0 0 0 0 0 1 -1\nend\n"));
    ide.clear(); ipl.clear();
    assert(CCustomModels::HasCommonWorld());
    CCustomModels::ForEachWorldFile(true, WorldFile, &ide);
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ide.size() == 2 && ide.count("data/maps/ide/test.ide") && ide.count("data/vehicles.ide"));
    assert(ide["data/maps/ide/test.ide"].find("199") != std::string::npos); // common wins path
    assert(!ide.count("maps/ide/test.ide") && !ide.count("data/features/ignored.ide"));
    assert(ipl.size() == 2 && ipl.count("data/maps/level_events/test.ipl") &&
           ipl.count("data/maps/level_events/new.ipl") &&
           !ipl.count("data/maps/level_events/event.ipl") &&
           !ipl.count("maps/level_events/alternate.ipl")); // obsolete common.zip map hidden
    assert(ipl["data/maps/level_events/test.ipl"].find(" 9 0 0") != std::string::npos);
    bool fromCommon = false;
    assert(CCustomModels::ReadDataFile("data/timecyc.json", selected, &fromCommon) && fromCommon);
    assert(std::string(selected.begin(), selected.end()) == "{\"ambient\":1}");
    assert(CCustomModels::ReadDataFile("data/carcols.dat", selected, &fromCommon) && fromCommon);
    assert(!CCustomModels::ReadDataFile("data/features/ignored.ide", selected));
    assert(!CCustomModels::ReadDataFile("audio/ignored.ipl", selected));
    Write("custom/active_ipl.txt", Bytes("data/maps/level_events/event.ipl\n"));
    ipl.clear();
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 1 && ipl.count("data/maps/level_events/event.ipl"));
    Write("custom/active_ipl.txt", Bytes("maps/level_events/alternate.ipl\n"));
    ipl.clear();
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 1 && ipl.count("maps/level_events/alternate.ipl"));
    assert(std::remove("custom/active_ipl.txt") == 0);
    CCustomModels::Shutdown();
    assert(CCustomModels::PairedImageTxd("house1") == -1); // no stale IMG map after restart
    assert(!CCustomModels::FindModelDffInImage("house1", specialPos, specialSize));
    CTxdStore::Shutdown();
    // A common IDE may use a native DFF even with the VC map disabled and
    // no user IMG installed. Indexing native DIR must not depend on img/.
    assert(std::rename("img", "img_off") == 0);
    images.clear(); images.push_back("models/gta3.img");
    CTxdStore::Initialise();
    CCustomModels::MountImages();
    assert(images.size() == 1);
    assert(CCustomModels::HasModelFile("baseonly"));
    assert(!CCustomModels::HasModelFile("house1"));
    CCustomModels::Shutdown(); CTxdStore::Shutdown();
    assert(std::rename("img_off", "img") == 0);
    assert(chdir("..") == 0);
    std::string root(dir);
    for(const char *file : { "models/gta3.DIR", "img/map.img", "img/map.dir", "img/area/extra.img",
                             "img/area/extra.dir", "img/no_dir.img",
                             "img/v2.img", "img/v2.dir", "custom/common.zip",
                             "custom/coll_a.zip", "custom/coll_b.zip", "zip/visuals.ZIP",
                             "common/data/config.dat", "common/data/vehicles.ide",
                             "common/data/maps/ide/test.ide",
                             "common/data/maps/level_events/test.ipl",
                             "common/data/maps/level_events/new.ipl",
                             "common/data/maps/level_events/event.ipl",
                             "common/data/timecyc.json", "common/data/carcols.dat",
                             "common/data/features/ignored.ide", "common/audio/ignored.ipl" })
        assert(std::remove((root+"/"+file).c_str()) == 0);
    assert(rmdir((root+"/common/data/maps/ide").c_str()) == 0);
    assert(rmdir((root+"/common/data/maps/level_events").c_str()) == 0);
    assert(rmdir((root+"/common/data/maps").c_str()) == 0);
    assert(rmdir((root+"/common/data/features").c_str()) == 0);
    assert(rmdir((root+"/common/data").c_str()) == 0);
    assert(rmdir((root+"/common/audio").c_str()) == 0);
    assert(rmdir((root+"/common").c_str()) == 0);
    assert(rmdir((root+"/img/area").c_str()) == 0);
    assert(rmdir((root+"/img").c_str()) == 0);
    assert(rmdir((root+"/models").c_str()) == 0);
    assert(rmdir((root+"/custom").c_str()) == 0);
    assert(rmdir((root+"/zip").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("real custom IMG/ZIP scanner: paired DFF/TXD, nested IDE/IPL and CLS/COL passed");
}
