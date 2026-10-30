// Exercise the REAL custom folder scanner, IMG mounting and loose collision
// enumeration. Valid ZIPs and legacy paths are NEGATIVE fixtures. Standalone test:
// unused game/RenderWare sections are garbage-collected at link time.
#include "common.h"
#include "CustomModels.h"
#include "CustomColFormat.h"
#include "CustomImgFormat.h"
#include "ModelLoadDispatch.h"
#include "CdStream.h"
#include "ColStore.h"
#include "Streaming.h"
#include "TxdStore.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static std::vector<std::string> images;
extern "C" bool CdStreamAddImage(const char *path) { images.push_back(path); return true; }
extern "C" int32 CdStreamGetNumImages(void) { return (int32)images.size(); }
extern "C" char *CdStreamGetImageName(int cd) { return &images.at(cd)[0]; }
char *CColStore::GetColName(int32 slot) {
    static char world[] = "world";
    return slot == 0 ? world : nullptr;
}
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

static void MkDir(const char *path) { assert(mkdir(path, 0700) == 0); }

// Only delete files beneath the private mkdtemp root after chdir back out.
static void RemoveTree(const std::string &path) {
    struct stat st;
    assert(lstat(path.c_str(), &st) == 0);
    if(!S_ISDIR(st.st_mode)) { assert(std::remove(path.c_str()) == 0); return; }
    DIR *dir = opendir(path.c_str()); assert(dir);
    struct dirent *e;
    while((e = readdir(dir)) != nullptr)
        if(std::strcmp(e->d_name, ".") && std::strcmp(e->d_name, ".."))
            RemoveTree(path + "/" + e->d_name);
    assert(closedir(dir) == 0);
    assert(rmdir(path.c_str()) == 0);
}

int main() {
    // Selecting an IMG DFF must still dispatch by the IDE model-info type.
    assert(GetModelLoadKind(MITYPE_SIMPLE) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_TIME) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_WEAPON) == MODELLOAD_ATOMIC);
    assert(GetModelLoadKind(MITYPE_CLUMP) == MODELLOAD_CLUMP);
    assert(GetModelLoadKind(MITYPE_VEHICLE) == MODELLOAD_VEHICLE);
    assert(GetModelLoadKind(MITYPE_PED) == MODELLOAD_PED);
    assert(GetModelLoadKind(MITYPE_MLO) == MODELLOAD_UNSUPPORTED);
    assert(GetModelLoadKind(MITYPE_XTRACOMPS) == MODELLOAD_UNSUPPORTED);
    assert(!IsModelClumpCompatible(MODELLOAD_PED, true, true, false, false));
    assert(IsModelClumpCompatible(MODELLOAD_PED, true, true, true, true));
    assert(!IsModelClumpCompatible(MODELLOAD_PED, true, true, true, false));
    assert(IsModelClumpCompatible(MODELLOAD_VEHICLE, true, true, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_VEHICLE, false, true, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_CLUMP, true, false, false, false));
    assert(!IsModelClumpCompatible(MODELLOAD_ATOMIC, true, true, false, false));

    assert(unsetenv("REVC_CUSTOM_DIR") == 0);
    char temp[] = "revc-custom-layout-XXXXXX";
    char *dir = mkdtemp(temp);
    assert(dir && chdir(dir) == 0);
    const std::string root(dir);
    MkDir("custom"); MkDir("custom/maps"); MkDir("custom/img"); MkDir("custom/img/area");
    MkDir("custom/anim"); MkDir("custom/collision");
    MkDir("custom/collision/nested"); MkDir("custom/data");
    MkDir("custom/data/maps"); MkDir("custom/data/maps/ide");
    MkDir("custom/data/maps/level_events");
    MkDir("models"); MkDir("img"); MkDir("common"); MkDir("common/data");
    MkDir("common/data/maps"); MkDir("zip");

    std::vector<uint8> img(4*2048, 0), dirBytes(3*32, 0);
    W32(dirBytes, 0, 1); W32(dirBytes, 4, 2);
    std::memcpy(dirBytes.data()+8, "house1.dff", 11);
    W32(dirBytes, 32, 3); W32(dirBytes, 36, 1);
    std::memcpy(dirBytes.data()+40, "R3.txd", 7); // named IDE TXD
    W32(dirBytes, 64, 99); W32(dirBytes, 68, 9000);
    std::memcpy(dirBytes.data()+72, "world.col", 10); // never index collision inside IMG
    Write("custom/img/map.img", img); Write("custom/img/map.dir", dirBytes);
    std::vector<uint8> extraDir(32, 0);
    W32(extraDir, 0, 1); W32(extraDir, 4, 1);
    std::memcpy(extraDir.data()+8, "HOUSE1.TXD", 11); // paired across two custom IMGs
    Write("custom/img/area/extra.img", img); Write("custom/img/area/extra.dir", extraDir);
    Write("custom/img/no_dir.img", img);
    img[0] = 'V'; img[1] = 'E'; img[2] = 'R'; img[3] = '2';
    Write("custom/img/v2.img", img); Write("custom/img/v2.dir", dirBytes);
    std::vector<uint8> legacyImg(3*2048, 0), legacyDir(2*32, 0);
    W32(legacyDir, 0, 1); W32(legacyDir, 4, 1);
    std::memcpy(legacyDir.data()+8, "rootonly.dff", 13);
    W32(legacyDir, 32, 2); W32(legacyDir, 36, 1);
    std::memcpy(legacyDir.data()+40, "rootonly.txd", 13);
    Write("img/obsolete.img", legacyImg); Write("img/obsolete.dir", legacyDir);

    std::vector<uint8> nativeDir(4*32, 0);
    W32(nativeDir, 0, 1); W32(nativeDir, 4, 1);
    std::memcpy(nativeDir.data()+8, "R3.TXD", 7);
    W32(nativeDir, 32, 2); W32(nativeDir, 36, 1);
    std::memcpy(nativeDir.data()+40, "baseonly.TXD", 13);
    W32(nativeDir, 64, 3); W32(nativeDir, 68, 1);
    std::memcpy(nativeDir.data()+72, "baseonly.DFF", 13);
    W32(nativeDir, 96, 4); W32(nativeDir, 100, 1);
    std::memcpy(nativeDir.data()+104, "house1.TXD", 11);
    Write("models/gta3.DIR", nativeDir); // stock IMG remains available
    images.push_back("models/gta3.img");

    Write("custom/data/maps/ide/test.ide", Bytes("objs\n1 house1 r3 100 0\nend\n"));
    Write("custom/data/maps/level_events/test.ipl", Bytes("inst\n1 house1 0 0 0 0 0 0 0 1 -1\nend\n"));
    Write("custom/data/maps/level_events/alternate.ipl", Bytes("inst\n2 house1 0 2 0 0 0 0 0 1 -1\nend\n"));
    Write("custom/data/timecyc.json", Bytes("{\"ambient\":1}"));
    std::vector<uint8> ifp = Bytes("ANPK12345678"); // only the indexing/reader is under test
    Write("custom/anim/userbank.ifp", ifp);
    Write("custom/anim/bad.ifp", Bytes("short"));
    Write("custom/anim/old.ani", ifp); // wrong extension in the right directory

    std::vector<uint8> twoBlocks = Col("house3");
    std::vector<uint8> override = Col("house1");
    WF(override, 32, 3); // .col wins over the same model's .cls
    twoBlocks.insert(twoBlocks.end(), override.begin(), override.end());
    Write("custom/collision/part.cls", Cls("house1"));
    Write("custom/collision/nested/part.cls", Cls("house2"));
    Write("custom/collision/world.col", twoBlocks);
    Write("custom/collision/nested/world.col", Col("house4"));
    Write("custom/collision/legacy.cls", LegacyCls("house6"));
    Write("custom/collision/nested/sa.col", Col3("house7"));
    std::vector<uint8> badOffset = Col3("badOffset");
    W32(badOffset, 84, 0xfffffff0);
    Write("custom/collision/nested/bad.col", badOffset);

    // All of these legacy or unrelated paths must be inert, even if the ZIPs
    // are valid and contain filenames that used to be indexed as custom data.
    Zip("custom/old.zip", {
        { "anim/zipbank.ifp", ifp },
        { "collision/zip.col", Col("zipcollision") },
        { "data/maps/ide/ziponly.ide", Bytes("objs\n1 ziphouse bad 100 0\nend\n") },
        { "data/maps/level_events/ziponly.ipl", Bytes("inst\n1 ziphouse 0 0 0 0 0 0 0 1 -1\nend\n") },
        { "models/SPECIAL1.dff", Bytes("ZIP model") },
        { "textures/ziptexture.btx", RawBtx(32, 32) },
        { "data/timecyc.json", Bytes("ZIP lighting") }
    });
    Zip("zip/visuals.ZIP", {
        { "vehicles/CarReflectionMask.btx", RawBtx(32, 32) },
        { "lightmaps/window_LM.btx", RawBtx(32, 16) },
        { "models/forbidden.dff", Bytes("ZIP model") }
    });
    Zip("custom/img/unused.zip", { { "models/zipimg.dff", Bytes("ZIP model") } });
    Write("custom/old.ifp", ifp);
    Write("custom/old.col", Col("oldcollision"));
    Write("custom/maps/old.ide", Bytes("objs\n1 oldhouse bad 100 0\nend\n"));
    Write("custom/data/old.ipl", Bytes("inst\n1 oldhouse 0 0 0 0 0 0 0 1 -1\nend\n"));
    Write("common/data/config.dat", Bytes("IDE data/maps/test.ide\nIPL data/maps/test.ipl\n"));
    Write("common/data/maps/test.ide", Bytes("objs\n1 commonhouse bad 100 0\nend\n"));
    Write("common/data/maps/test.ipl", Bytes("inst\n1 commonhouse 0 0 0 0 0 0 0 1 -1\nend\n"));
    Write("common/data/timecyc.json", Bytes("{\"ambient\":999}"));

    CTxdStore::Initialise();
    CCustomModels::ResetImageMounts(); CCustomModels::MountImages();
    assert(images.size() == 3); // native + two good custom/img pairs, NOT root img/
    assert(images[1].find("custom/img/area/extra.img") != std::string::npos);
    assert(images[2].find("custom/img/map.img") != std::string::npos);
    assert(CCustomModels::IsActive());
    assert(CCustomModels::IsAutoMountedImage(1) && CCustomModels::IsAutoMountedImage(2));
    assert(!CCustomModels::IsAutoMountedImage(0));
    assert(CTxdStore::FindTxdSlot("r3") >= 0);
    assert(CTxdStore::FindTxdSlot("BASEONLY") >= 0);
    assert(CCustomModels::PairedImageTxd("HoUsE1") == 1); // DFF 2 + TXD 1
    assert(CCustomModels::PairedImageTxd("baseonly") == -1); // stock pair is not custom
    assert(CCustomModels::PairedImageTxd("rootonly") == -1);
    uint32 specialPos = 0, specialSize = 0;
    assert(CCustomModels::FindModelDffInImage("HoUsE1", specialPos, specialSize));
    assert(specialPos == (2u<<24 | 1u) && specialSize == 2);
    assert(CCustomModels::FindModelDffInImage("baseonly", specialPos, specialSize));
    assert(specialPos == 3 && specialSize == 1); // stock VC is still allowed
    assert(!CCustomModels::FindModelDffInImage("rootonly", specialPos, specialSize));
    assert(!CCustomModels::FindModelDffInImage("SPECIAL1", specialPos, specialSize));
    CCustomModels::MountImages(); assert(images.size() == 3); // idempotent
    assert(CCustomModels::HasModelFile("house1"));
    assert(CCustomModels::HasModelFile("baseonly"));
    assert(!CCustomModels::CanServe(0)); // DFF/TXD use the native IMG streamer
    assert(!CCustomModels::CanServe(STREAM_OFFSET_TXD));
    assert(CCustomModels::CanServe(STREAM_OFFSET_COL)); // loose world.col still serves COLFILE
    assert(!CCustomModels::CanServe(STREAM_OFFSET_COL + 1));
    assert(!CCustomModels::HasModelFile("rootonly"));
    assert(!CCustomModels::HasModelFile("SPECIAL1"));
    assert(!CCustomModels::HasModelFile("forbidden"));
    assert(!CCustomModels::HasModelFile("zipimg"));

    std::map<std::string, std::string> ide, ipl, cls, col;
    assert(CCustomModels::HasWorldFiles());
    CCustomModels::ForEachWorldFile(true, WorldFile, &ide);
    CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    CCustomModels::ForEachBrCollision(Ced2, &cls);
    CCustomModels::ForEachGameCollision(Collision, &col);
    assert(ide.size() == 1 && ide.count("data/maps/ide/test.ide"));
    assert(ide.begin()->second.find("house1 r3 100") != std::string::npos);
    assert(ipl.size() == 2 && ipl.count("data/maps/level_events/test.ipl") &&
           ipl.count("data/maps/level_events/alternate.ipl"));
    assert(cls.size() == 2 && cls["house1"] == "house1" && cls["house2"] == "house2");
    assert(col.size() == 5 && col["house1"] == "house1" && col["house3"] == "house3" &&
           col["house4"] == "house4" && col["house6"] == "house6" && col["house7"] == "house7");
    std::vector<uint8> selected;
    assert(CCustomModels::GetCollisionBlock("house1", selected));
    float radius = 0; std::memcpy(&radius, selected.data()+24, 4);
    assert(radius == 3);
    assert(CCustomModels::GetCollisionBlock("house2", selected));
    std::memcpy(&radius, selected.data()+24, 4);
    assert(radius == 2);
    assert(CCustomModels::GetCollisionBlock("house7", selected));
    assert(selected.size() == 84 && std::string((const char *)selected.data()) == "house7");
    assert(!CCustomModels::GetCollisionBlock("zipcollision", selected));
    assert(!CCustomModels::GetCollisionBlock("oldcollision", selected));
    assert(!CCustomModels::GetCollisionBlock("badOffset", selected));
    assert(CCustomModels::LoadAnimFileFromCustom("USERBANK.IFP", selected) && selected == ifp);
    assert(!CCustomModels::LoadAnimFileFromCustom("zipbank.ifp", selected));
    assert(!CCustomModels::LoadAnimFileFromCustom("old.ani", selected));
    assert(!CCustomModels::LoadAnimFileFromCustom("old.ifp", selected));
    assert(!CCustomModels::LoadAnimFileFromCustom("bad.ifp", selected));
    bool fromCommon = true;
    assert(CCustomModels::ReadDataFile("data/timecyc.json", selected, &fromCommon));
    assert(!fromCommon && std::string(selected.begin(), selected.end()) == "{\"ambient\":1}");
    assert(!CCustomModels::ReadDataFile("data/carcols.dat", selected));
    assert(!CCustomModels::ReadDataFile("data/features/ignored.ide", selected));
    assert(!CCustomModels::ReadDataFile("audio/ignored.ipl", selected));
    assert(!CCustomModels::ReadDataFile("../common/data/timecyc.json", selected));
    assert(std::remove("custom/data/timecyc.json") == 0);
    assert(!CCustomModels::ReadDataFile("data/timecyc.json", selected)); // no common/ or ZIP fallback

    Write("custom/active_ipl.txt", Bytes("data/maps/level_events/unknown.ipl\n"));
    ipl.clear(); CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 2); // old manifest path is ignored
    Write("custom/data/maps/active_ipl.txt", Bytes(
        "# select one real custom IPL (zip-only and old paths cannot activate)\n"
        " DATA\\Maps\\Level_Events\\test.IPL // selected\n"
        "data/maps/level_events/ziponly.ipl\n"));
    ipl.clear(); CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.size() == 1 && ipl.count("data/maps/level_events/test.ipl"));
    ide.clear(); CCustomModels::ForEachWorldFile(true, WorldFile, &ide);
    assert(ide.size() == 1); // manifest does not filter IDE
    Write("custom/data/maps/active_ipl.txt", Bytes("# intentionally empty\n"));
    ipl.clear(); CCustomModels::ForEachWorldFile(false, WorldFile, &ipl);
    assert(ipl.empty());
    assert(std::remove("custom/data/maps/active_ipl.txt") == 0);

    CCustomModels::Shutdown(); CTxdStore::Shutdown();
    assert(CCustomModels::PairedImageTxd("house1") == -1);
    assert(!CCustomModels::FindModelDffInImage("house1", specialPos, specialSize));
    // ZIP-only directory: no world, animation, collision, graphics, models,
    // or native-IMG replacements are ever selected from those ZIP entries.
    assert(std::rename("custom", "custom_off") == 0);
    MkDir("custom");
    Zip("custom/only.zip", {
        { "anim/zipbank.ifp", ifp },
        { "collision/zip.col", Col("zipcollision") },
        { "data/maps/zip.ide", Bytes("objs\n1 ziphouse bad 100 0\nend\n") },
        { "data/timecyc.json", Bytes("ZIP lighting") }
    });
    images.clear(); images.push_back("models/gta3.img");
    CTxdStore::Initialise(); CCustomModels::MountImages();
    assert(images.size() == 1 && !CCustomModels::IsActive());
    assert(!CCustomModels::HasWorldFiles());
    assert(!CCustomModels::LoadAnimFileFromCustom("zipbank.ifp", selected));
    assert(!CCustomModels::GetCollisionBlock("zipcollision", selected));
    assert(!CCustomModels::ReadDataFile("data/timecyc.json", selected));
    assert(!CCustomModels::HasModelFile("rootonly"));
    assert(CCustomModels::HasModelFile("baseonly")); // stock VC unaffected
    CCustomModels::Shutdown(); CTxdStore::Shutdown();

    // IMG-only directory: no IDE/IFP/COL needed for valid user models.
    MkDir("custom/img");
    std::vector<uint8> onlyDir(2*32, 0);
    W32(onlyDir, 0, 1); W32(onlyDir, 4, 1);
    std::memcpy(onlyDir.data()+8, "imgonly.dff", 12);
    W32(onlyDir, 32, 2); W32(onlyDir, 36, 1);
    std::memcpy(onlyDir.data()+40, "imgonly.txd", 12);
    Write("custom/img/only.img", legacyImg); Write("custom/img/only.dir", onlyDir);
    images.clear(); images.push_back("models/gta3.img");
    CTxdStore::Initialise(); CCustomModels::MountImages();
    assert(images.size() == 2 && CCustomModels::IsActive());
    assert(CCustomModels::HasModelFile("imgonly") && CCustomModels::PairedImageTxd("imgonly") == 1);
    assert(!CCustomModels::HasWorldFiles() && !CCustomModels::HasModelFile("rootonly"));
    CCustomModels::Shutdown(); CTxdStore::Shutdown();

    assert(chdir("..") == 0);
    RemoveTree(root);
    std::puts("custom layout: only custom/img, anim, collision and data/maps served; all ZIP, root img and common paths rejected");
}
