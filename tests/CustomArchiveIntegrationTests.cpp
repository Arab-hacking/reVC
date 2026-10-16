// Exercise the REAL custom archive scanner, IMG mounting and per-model
// collision enumeration with small synthetic ZIP/IMG files. Standalone test:
// unused game/RenderWare sections are garbage-collected at link time.
#include "common.h"
#include "CustomModels.h"
#include "CustomColFormat.h"
#include "CustomImgFormat.h"
#include "CdStream.h"
#include "TxdStore.h"

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
    char temp[] = "revc-custom-layout-XXXXXX";
    char *dir = mkdtemp(temp);
    assert(dir && chdir(dir) == 0);
    assert(mkdir("img", 0700) == 0 && mkdir("custom", 0700) == 0);
    std::vector<uint8> img(4*2048, 0), directory(3*32, 0);
    W32(directory, 0, 1); W32(directory, 4, 2);
    std::memcpy(directory.data()+8, "house1.dff", 11);
    W32(directory, 32, 3); W32(directory, 36, 1);
    std::memcpy(directory.data()+40, "R3.txd", 7);
    W32(directory, 64, 99); W32(directory, 68, 9000);
    std::memcpy(directory.data()+72, "world.col", 10); // must be ignored
    Write("img/map.img", img); Write("img/map.dir", directory);
    assert(mkdir("img/area", 0700) == 0);
    Write("img/area/extra.img", img); Write("img/area/extra.dir", directory);
    Write("img/no_dir.img", img);
    img.assign(4*2048, 0); std::memcpy(img.data(), "VER2", 4);
    Write("img/v2.img", img); Write("img/v2.dir", directory);
    assert(mkdir("models", 0700) == 0);
    std::vector<uint8> nativeDir(2*32, 0);
    std::memcpy(nativeDir.data()+8, "R3.TXD", 7); // shared with auto IMG
    std::memcpy(nativeDir.data()+40, "baseonly.TXD", 13);
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
    CCustomModels::MountImages(); assert(images.size() == 3); // idempotent

    Zip("custom/common.zip", {
        { "data/maps/IDE/test.ide", Bytes("objs\n1 house1 r3 100 0\nend\n") },
        { "data/maps/Level_Events/test.ipl", Bytes("inst\n1 house1 0 0 0 0 0 0 0 1 -1\nend\n") }
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
    CCustomModels::ForEachWorldFile(true, WorldFile, &ide);
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    CCustomModels::ForEachBrCollision(Ced2, &cls);
    CCustomModels::ForEachGameCollision(Collision, &col);
    assert(ide.size() == 1 && ide.count("data/maps/ide/test.ide"));
    assert(ipl.size() == 1 && ipl.count("data/maps/level_events/test.ipl"));
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
    CCustomModels::Shutdown();
    CTxdStore::Shutdown();
    assert(chdir("..") == 0);
    std::string root(dir);
    for(const char *file : { "models/gta3.DIR", "img/map.img", "img/map.dir", "img/area/extra.img",
                             "img/area/extra.dir", "img/no_dir.img",
                             "img/v2.img", "img/v2.dir", "custom/common.zip",
                             "custom/coll_a.zip", "custom/coll_b.zip" })
        assert(std::remove((root+"/"+file).c_str()) == 0);
    assert(rmdir((root+"/img/area").c_str()) == 0);
    assert(rmdir((root+"/img").c_str()) == 0);
    assert(rmdir((root+"/models").c_str()) == 0);
    assert(rmdir((root+"/custom").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("real custom IMG/ZIP scanner: nested IDE/IPL and colliding CLS/COL names passed");
}
