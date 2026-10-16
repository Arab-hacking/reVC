#include "../src/extras/custom/CustomImgFormat.h"
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
    Entry(dir, 2, "collision.COL", 4, 65000); // ignored even when not in IMG
    customimg::Stats st;
    std::string why;
    assert(customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    assert(st.dff == 1 && st.txd == 1 && st.ignored == 1 && st.largestSectors == 2);
    assert(!customimg::Validate(dir.data(), dir.size()-1, 4*2048, st, why));
    W32(dir, 0, 0x1000000); // IMG index byte must not leak into sector offset
    assert(!customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    W32(dir, 0, 1); W32(dir, 4, 32769); // no unbounded streaming buffer
    assert(!customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    W32(dir, 4, 2); W32(dir, 0, 3); // exceeds IMG length
    assert(!customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    W32(dir, 0, 1);
    assert(customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    assert(((uint32_t)255 << 24 | customimg::U32(dir.data())) >> 24 == 255);
    std::memset(dir.data()+8, 0, 24);
    std::memcpy(dir.data()+8, "01234567890123456789.DFF", 24); // no space for NUL
    assert(customimg::Validate(dir.data(), dir.size(), 4*2048, st, why));
    assert(st.dff == 0 && st.txd == 1 && st.ignored == 2);

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
    // Two ZIP entries with the SAME filename may contain distinct models.
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
    TestCol();
    std::puts("IMG v1 directory and ZIP collision format tests passed");
}
