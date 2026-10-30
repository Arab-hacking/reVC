#include "../src/extras/custom/CustomImgFormat.h"
#include "../src/extras/custom/ModelNameIndex.h"
#include "../src/extras/custom/CustomColFormat.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static void W32(std::vector<uint8_t> &b, size_t pos, uint32_t n) {
    for(int k = 0; k < 4; k++) b[pos+k] = (uint8_t)(n >> (8*k));
}
static void WF(std::vector<uint8_t> &b, size_t pos, float x) {
    uint32_t n; std::memcpy(&n, &x, 4); W32(b, pos, n);
}
static void Entry(std::vector<uint8_t> &dir, size_t n, const char *name,
                  uint32_t sector, uint32_t sectors) {
    assert(std::strlen(name) <= 23);
    W32(dir, n*32, sector); W32(dir, n*32+4, sectors);
    std::memcpy(dir.data()+n*32+8, name, std::strlen(name));
}
static void TestImgDir() {
    std::vector<uint8_t> dir(3*32, 0);
    Entry(dir, 0, "XmasBox1.DFF", 1, 2);
    Entry(dir, 1, "R3.TXD", 3, 1);
    Entry(dir, 2, "collision.COL", 4, 1); // checked and handled outside native streaming
    customimg::Stats st;
    std::string why;
    assert(customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    assert(st.dff == 1 && st.txd == 1 && st.col == 1 && st.cls == 0 &&
           st.ignored == 0 && st.largestSectors == 2);
    std::vector<std::string> colEntries;
    customimg::ForEachCollisionEntry(dir.data(), dir.size(), 5*2048,
        [&colEntries](const std::string &name, bool cls, uint32_t sector, uint32_t sectors) {
            assert(!cls && sector == 4 && sectors == 1);
            colEntries.push_back(name);
        });
    assert(colEntries.size() == 1 && colEntries[0] == "collision");
    W32(dir, 2*32, 0x1000000); // broken COL cannot block good DFF/TXD
    assert(customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    assert(st.col == 0 && st.badCollision == 1);
    colEntries.clear();
    customimg::ForEachCollisionEntry(dir.data(), dir.size(), 5*2048,
        [&colEntries](const std::string &name, bool, uint32_t, uint32_t) {
            colEntries.push_back(name);
        });
    assert(colEntries.empty());
    W32(dir, 2*32, 4);
    assert(!customimg::Validate(dir.data(), dir.size()-1, 5*2048, st, why));
    W32(dir, 0, 0x1000000); // IMG index byte must not leak into sector offset
    assert(!customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    W32(dir, 0, 1); W32(dir, 4, 32769); // no unbounded streaming buffer
    assert(!customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    W32(dir, 4, 2); W32(dir, 0, 4); // exceeds IMG length
    assert(!customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    W32(dir, 0, 1);
    assert(customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    assert(((uint32_t)255 << 24 | customimg::U32(dir.data())) >> 24 == 255);
    std::memset(dir.data()+8, 0, 24);
    std::memcpy(dir.data()+8, "01234567890123456789.DFF", 24); // no space for NUL
    assert(customimg::Validate(dir.data(), dir.size(), 5*2048, st, why));
    assert(st.dff == 0 && st.txd == 1 && st.col == 1 && st.ignored == 1);

    // An IMG containing ONLY collision files must mount, including CLS.
    std::vector<uint8_t> onlyCol(2*32, 0);
    Entry(onlyCol, 0, "bridge.COL", 1, 1);
    Entry(onlyCol, 1, "world.CLS", 2, 1);
    assert(customimg::Validate(onlyCol.data(), onlyCol.size(), 3*2048, st, why));
    assert(st.dff == 0 && st.txd == 0 && st.col == 1 && st.cls == 1);
    int visited = 0;
    customimg::ForEachCollisionEntry(onlyCol.data(), onlyCol.size(), 3*2048,
        [&visited](const std::string &name, bool cls, uint32_t, uint32_t) {
            assert((name == "bridge" && !cls) || (name == "world" && cls));
            visited++;
        });
    assert(visited == 2);
    W32(onlyCol, 0, 0x1000000); // one invalid COL does not hide a valid CLS
    assert(customimg::Validate(onlyCol.data(), onlyCol.size(), 3*2048, st, why));
    assert(st.col == 0 && st.cls == 1 && st.badCollision == 1);
    W32(onlyCol, 0, 1); W32(onlyCol, 4, 0);
    assert(customimg::Validate(onlyCol.data(), onlyCol.size(), 3*2048, st, why));
    assert(st.badCollision == 1);
    W32(onlyCol, 4, 32769);
    assert(customimg::Validate(onlyCol.data(), onlyCol.size(), 3*2048, st, why));
    W32(onlyCol, 4, 1);
    W32(onlyCol, 1*32, 0x1000000); // neither COL nor CLS remains usable
    W32(onlyCol, 0, 0x1000000);
    assert(!customimg::Validate(onlyCol.data(), onlyCol.size(), 3*2048, st, why));
    W32(onlyCol, 0, 1); W32(onlyCol, 1*32, 2);
    // Validate sector arithmetic beyond 2 GiB without allocating an IMG.
    W32(onlyCol, 0, 0x110000);
    uint64_t bigImg = (uint64_t)(0x110000+1)*2048;
    assert(customimg::Validate(onlyCol.data(), onlyCol.size(), bigImg, st, why));
    visited = 0;
    customimg::ForEachCollisionEntry(onlyCol.data(), onlyCol.size(), bigImg,
        [&visited](const std::string &, bool, uint32_t sector, uint32_t) {
            if(sector == 0x110000) visited++;
        });
    assert(visited == 1);

    std::vector<uint8_t> native(5*32+13, 0);
    Entry(native, 0, "R3.TXD", 1, 1);
    Entry(native, 1, "test.COL", 2, 1);
    Entry(native, 2, "0123456789012345678.TXD", 3, 1); // 19-char TXD stem
    std::memcpy(native.data()+3*32+8, "01234567890123456789.TXD", 24); // 20: no slot space
    std::memcpy(native.data()+4*32+8, "a\0b.TXD", 7); // native strchr stops at NUL
    std::vector<std::string> names;
    customimg::ForEachNativeTxd(native.data(), native.size(),
        [&names](const std::string &name) { names.push_back(name); });
    assert(names.size() == 2 && names[0] == "R3" &&
           names[1] == "0123456789012345678");
}

static void TestPairedImageTextures() {
    customimg::PairIndex pairs;
    std::vector<uint8_t> first(5*32, 0);
    Entry(first, 0, "Tower.DFF", 1, 1);
    Entry(first, 1, "TOWER.txd", 2, 1);
    Entry(first, 2, "split.txd", 3, 1);
    Entry(first, 3, "other.dff", 4, 1);
    Entry(first, 4, "empty.dff", 5, 0); // not a streamable model
    pairs.AddDirectory(1, first.data(), first.size());
    int dffImage = -1;
    uint32_t position = 0, sectors = 0;
    assert(pairs.FindDffRecord("ToWeR", position, sectors) &&
           position == (1u<<24 | 1u) && sectors == 1);
    assert(!pairs.FindDffRecord("empty", position, sectors));
    assert(pairs.FindTxdImage("tower", &dffImage) == 1 && dffImage == 1);
    assert(pairs.FindTxdImage("TOWER") == 1); // case-insensitive names
    assert(pairs.FindTxdImage("empty") == -1 && pairs.FindTxdImage("split") == -1);

    std::vector<uint8_t> second(3*32, 0);
    Entry(second, 0, "split.dff", 1, 1);
    Entry(second, 1, "other.txd", 2, 1);
    Entry(second, 2, "tower.DFF", 3, 1);
    pairs.AddDirectory(2, second.data(), second.size());
    assert(pairs.FindDffRecord("tower", position, sectors) &&
           position == (2u<<24 | 3u) && sectors == 1);
    assert(pairs.FindTxdImage("tower", &dffImage) == 1 && dffImage == 2);
    assert(pairs.FindTxdImage("split", &dffImage) == 1 && dffImage == 2);
    assert(pairs.FindTxdImage("other", &dffImage) == 2 && dffImage == 1);
    pairs.AddDirectory(0, first.data(), first.size()); // lower-priority duplicate
    assert(pairs.FindTxdImage("tower", &dffImage) == 1 && dffImage == 2);
    std::vector<uint8_t> third(2*32, 0);
    Entry(third, 0, "tower.dff", 1, 1);
    Entry(third, 1, "TOWER.TXD", 2, 1);
    third.resize(3*32, 0);
    Entry(third, 2, "ToWeR.DFF", 10, 1); // same IMG: first record wins
    pairs.AddDirectory(3, third.data(), third.size());
    assert(pairs.FindDffRecord("tower", position, sectors) &&
           position == (3u<<24 | 1u) && sectors == 1);
    assert(pairs.FindTxdImage("tower", &dffImage) == 3 && dffImage == 3);
    std::vector<uint8_t> fourth(32, 0);
    Entry(fourth, 0, "tower.txd", 1, 1);
    pairs.AddDirectory(4, fourth.data(), fourth.size());
    assert(pairs.FindTxdImage("tower", &dffImage) == 4 && dffImage == 3);
    pairs.Clear();
    assert(!pairs.FindDffRecord("tower", position, sectors));
    assert(pairs.FindTxdImage("tower") == -1);
}

static void TestModelNameIndex() {
    customimg::ModelNameIndex names(21); // CBaseModelInfo::MAX_MODEL_NAME
    names.Reserve(32768);
    names.Add("Landstal", 130);
    names.Add("LANDSTAL", 15000); // native/lowest ID wins as in GetModelInfo
    names.Add("", 9);
    names.Add(nullptr, 10);
    names.Add("invalid", -3);
    assert(names.Find("landSTAL") == 130);
    assert(names.Find("unused") == -1 && names.Find("") == -1);
    assert(names.Find("invalid") == -1);
    char unterminated[21];
    std::memset(unterminated, 'X', sizeof(unterminated));
    names.Add(unterminated, 42); // do not read beyond CBaseModelInfo::m_name
    assert(names.Find("xxxxxxxxxxxxxxxxxxxxx") == -1);
    assert(!customimg::ModelNameIndex::IsASCII("b\xD0\xA1r"));
    // Exercise the approximate cardinality of the selected real common/ IDE.
    char model[24];
    for(int i = 0; i < 27760; i++) {
        std::snprintf(model, sizeof(model), "common_%05d", i);
        names.Add(model, 4900+i);
    }
    assert(names.Find("COMMON_00000") == 4900);
    assert(names.Find("common_27759") == 32659);
}

// A minimal but valid VC COLL with one triangle; the native reader accesses
// these counts, vertices, face indices and bounds without a length argument.
static std::vector<uint8_t> Triangle(const char *name) {
    std::vector<uint8_t> b(32+40+4+4+4+4+3*12+4+16, 0);
    std::memcpy(b.data(), "COLL", 4);
    W32(b, 4, (uint32_t)b.size()-8);
    std::memcpy(b.data()+8, name, std::strlen(name));
    WF(b, 32, 3); // sphere radius
    WF(b, 48, -1); WF(b, 52, -1); WF(b, 56, -1);
    WF(b, 60, 1); WF(b, 64, 1); WF(b, 68, 1);
    W32(b, 84, 3); // vertex count (after spheres, lines, boxes)
    WF(b, 88+12, 1); WF(b, 88+24+4, 1);
    W32(b, 124, 1); // face count
    W32(b, 128, 0); W32(b, 132, 1); W32(b, 136, 2);
    return b;
}
static void TestCol() {
    std::vector<uint8_t> a = Triangle("house1");
    assert(customcolformat::ValidVC(a.data(), a.size()));
    std::vector<uint8_t> b = Triangle("house2");
    std::vector<uint8_t> file = a;
    file.insert(file.end(), b.begin(), b.end());
    std::vector<customcolformat::Block> blocks;
    assert(customcolformat::Index(file.data(), file.size(), blocks) == 0);
    assert(blocks.size() == 2 && blocks[0].name == "house1" && blocks[1].name == "house2");
    assert(customcolformat::ValidVC(file.data()+blocks[1].offset, blocks[1].size));
    // Two IMG entries with the SAME filename may contain distinct models.
    std::vector<customcolformat::Block> others;
    assert(customcolformat::Index(b.data(), b.size(), others) == 0);
    std::map<std::string, std::pair<int, size_t> > index;
    index[blocks[0].name] = std::make_pair(0, blocks[0].offset);
    index[others[0].name] = std::make_pair(1, others[0].offset);
    assert(index.size() == 2 && index["house2"].first == 1);

    W32(a, 132, 99); // face index outside the vertex array
    assert(!customcolformat::ValidVC(a.data(), a.size()));
    a = Triangle("house1"); W32(a, 84, 32000); // truncated vertex array
    assert(!customcolformat::ValidVC(a.data(), a.size()));
    a = Triangle("house1"); a[140] = 250; // surface outside VC's table
    assert(!customcolformat::ValidVC(a.data(), a.size()));
    a = Triangle("house1"); a[0] = '2';
    blocks.clear();
    assert(customcolformat::Index(a.data(), a.size(), blocks) == 1 && blocks.empty());
    a = Triangle("house1"); W32(a, 4, 0xffffffffu);
    blocks.clear();
    assert(customcolformat::Index(a.data(), a.size(), blocks) == 1 && blocks.empty());
    file = Triangle("house1");
    file.insert(file.end(), 2048-file.size(), 0);
    blocks.clear();
    assert(customcolformat::Index(file.data(), file.size(), blocks) == 0 && blocks.size() == 1);
}

int main() {
    TestImgDir();
    TestPairedImageTextures();
    TestModelNameIndex();
    TestCol();
    std::puts("IMG v1 directory and collision format tests passed");
}
