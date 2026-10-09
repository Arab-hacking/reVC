// brformats.h — конвертация форматов Black Russia -> GTA SA (общий код для brmod.asi и brmod_convert.exe)
//   .mod -> .dff  (TEA-дешифровка + нормализация RenderWare-потока в 3.6.0.3 / SA)
//   .cls -> .col  (CLST -> COL3)
//   .ani -> .ifp  (перестановка заголовка ANP3)
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <functional>

namespace br {

typedef std::function<void(const char*)> LogFn;

// ---------------------------------------------------------------------------
// .mod : TEA-подобный шифр (транскрипция libblackrussia-client.so @ 0xC34DD8)
// ---------------------------------------------------------------------------
static const uint32_t MOD_MAGIC   = 0xAB921033;
static const uint32_t MOD_KEY_XOR = 0x12913AFB;
static const uint32_t MOD_KEY_RAW[4] = { 0x6ED9EE7A, 0x930C666B, 0x930E166B, 0x4709EE79 };
static const uint32_t MOD_HDR   = 0x1C;
static const uint32_t MOD_BLOCK = 0x800;

static inline uint32_t ror32(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
static inline uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline void     wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

enum {
    rwSTRUCT = 0x01, rwSTRING = 0x02, rwEXTENSION = 0x03, rwCAMERA = 0x05, rwTEXTURE = 0x06,
    rwMATERIAL = 0x07, rwMATLIST = 0x08, rwFRAMELIST = 0x0E, rwGEOMETRY = 0x0F, rwCLUMP = 0x10,
    rwLIGHT = 0x12, rwATOMIC = 0x14, rwGEOMLIST = 0x1A, rwUVANIMDICT = 0x2B,
};
static const uint32_t SA_LIBID = 0x1803FFFF;   // 3.6.0.3 build 0xFFFF

static inline uint32_t libidToVersion(uint32_t libid) {
    if (libid & 0xFFFF0000) return (((libid >> 14) & 0x3FF00) + 0x30000) | ((libid >> 16) & 0x3F);
    return libid << 8;
}

static inline bool isMod(const uint8_t* buf, size_t size) { return size >= MOD_HDR && rd32(buf) == MOD_MAGIC; }

static inline void tea_decrypt(uint8_t* data, uint32_t len, const uint32_t k[4], uint32_t rounds = 8) {
    const uint32_t delta = 0x9E3779B9;
    for (uint32_t off = 0; off + 8 <= len; off += 8) {
        uint32_t v0 = rd32(data + off), v1 = rd32(data + off + 4);
        uint32_t sum = delta * rounds;
        for (uint32_t r = 0; r < rounds; r++) {
            v1 -= ((k[2] + (v0 << 4)) ^ (k[3] + (v0 >> 5))) ^ (v0 + sum);
            uint32_t s2 = sum + v1;
            sum -= delta;
            v0 -= ((k[0] + (v1 << 4)) ^ s2) ^ (k[1] + (v1 >> 5));
        }
        wr32(data + off, v0); wr32(data + off + 4, v1);
    }
}

// Варианты вывода ключа/раундов. 0 — канонический (libblackrussia-client.so). Остальные пробуются
// автоматически, если после расшифровки первые байты не похожи на RW CLUMP (на случай смены ключа в новом клиенте).
static const int MOD_KEY_VARIANTS = 15;
static inline void modKey(int variant, uint32_t key[4], uint32_t& rounds) {
    int kv = variant % 5, rv = variant / 5;
    rounds = rv == 0 ? 8 : (rv == 1 ? 16 : 32);
    for (int i = 0; i < 4; i++) {
        uint32_t t = MOD_KEY_RAW[i];
        switch (kv) {
        default: key[i] = ror32(t ^ MOD_KEY_XOR, 19); break;
        case 1:  key[i] = t; break;
        case 2:  key[i] = t ^ MOD_KEY_XOR; break;
        case 3:  key[i] = ror32(t, 19); break;
        case 4:  key[i] = ror32(t ^ MOD_KEY_XOR, 13); break;   // = rol 19
        }
    }
}

// Похоже ли начало буфера на RW-поток (CLUMP или UV-anim dict с разумным размером и libid)
static inline bool looksLikeRwStream(const uint8_t* p, uint32_t maxSize) {
    uint32_t id = rd32(p), sz = rd32(p + 4), lib = rd32(p + 8);
    if (id != rwCLUMP && id != rwUVANIMDICT) return false;
    (void)maxSize;                                   // размер корня у части BR-файлов завышен — не критерий
    if (sz < 12 || sz > 512u * 1024 * 1024) return false;
    uint32_t v = libidToVersion(lib);
    return v >= 0x30000 && v <= 0x3FFFF;
}

// Подбор варианта ключа по первым 16 байтам зашифрованного payload (TEA без сцепления — блоки независимы).
// Возвращает номер варианта или -1. -2 = payload вообще не зашифрован.
// maxPayload — верхняя граница размера RW-потока (0 = bufSize); нужно, когда прочитан только заголовок.
static inline int detectModKeyVariant(const uint8_t* buf, uint32_t bufSize, uint32_t maxPayload = 0) {
    if (bufSize < MOD_HDR + 16) return -1;
    if (!maxPayload) maxPayload = bufSize;
    if (looksLikeRwStream(buf + MOD_HDR, maxPayload)) return -2;
    for (int v = 0; v < MOD_KEY_VARIANTS; v++) {
        uint8_t t[16]; memcpy(t, buf + MOD_HDR, 16);
        uint32_t key[4], rounds; modKey(v, key, rounds);
        tea_decrypt(t, 16, key, rounds);
        if (looksLikeRwStream(t, maxPayload)) return v;
    }
    return -1;
}

// Расшифровка на месте. Возвращает длину полезных данных (0 = не .mod / ошибка).
// После вызова buf[0..len) — сырой RW-поток (ещё не нормализованный).
static inline uint32_t decryptModInplace(uint8_t* buf, uint32_t bufSize, std::string* warn = nullptr, int variant = 0) {
    if (!isMod(buf, bufSize)) return 0;
    uint32_t length = rd32(buf + 4), nBlocks = rd32(buf + 8);
    uint32_t enc = nBlocks * MOD_BLOCK;
    if (MOD_HDR + enc > bufSize) {
        if (warn) { char t[160]; snprintf(t, sizeof t, "header says %u blocks (%u bytes) but buffer is %u bytes - truncating", nBlocks, enc, bufSize); *warn = t; }
        enc = bufSize - MOD_HDR;
    }
    if (variant >= 0) {
        uint32_t key[4], rounds; modKey(variant, key, rounds);
        for (uint32_t off = 0; off + MOD_BLOCK <= enc; off += MOD_BLOCK) tea_decrypt(buf + MOD_HDR + off, MOD_BLOCK, key, rounds);
    }
    if (length > bufSize - MOD_HDR) length = bufSize - MOD_HDR;
    memmove(buf, buf + MOD_HDR, length);
    return length;
}

// Диагностическая строка по .mod, который не удалось расшифровать
static inline std::string modDiag(const uint8_t* buf, uint32_t bufSize) {
    char t[512]; std::string s;
    snprintf(t, sizeof t, "file=%u hdr: length=%u blocks=%u (=%u bytes) reserved=", bufSize, rd32(buf + 4), rd32(buf + 8), rd32(buf + 8) * MOD_BLOCK); s = t;
    for (int i = 0; i < 16; i++) { snprintf(t, sizeof t, "%02X", buf[12 + i]); s += t; }
    s += " enc[0..16)=";
    for (int i = 0; i < 16 && MOD_HDR + i < bufSize; i++) { snprintf(t, sizeof t, "%02X", buf[MOD_HDR + i]); s += t; }
    uint8_t d[16]; memcpy(d, buf + MOD_HDR, 16);
    uint32_t key[4], rounds; modKey(0, key, rounds); tea_decrypt(d, 16, key, rounds);
    s += " dec0[0..16)=";
    for (int i = 0; i < 16; i++) { snprintf(t, sizeof t, "%02X", d[i]); s += t; }
    return s;
}

// ---------------------------------------------------------------------------
// RenderWare stream: нормализация в формат SA (3.6.0.3, libid 0x1803FFFF)
// ---------------------------------------------------------------------------
struct RwNode {
    uint32_t id = 0, libid = 0;
    std::vector<uint8_t> data;       // если leaf
    std::vector<RwNode> kids;        // если контейнер
    bool container = false;
    size_t serializedSize() const {
        if (!container) return 12 + data.size();
        size_t s = 12; for (auto& k : kids) s += k.serializedSize(); return s;
    }
};

// BR pbrTerrain (matfx тип 10 без diffuse): маска (RGBA-веса, имя = имя модели, lnd_mask/) + 4 тайловых слоя landscapeMapR/G/B/A.
// В DFF наружу выносится текстура с именем маски; сборщик TXD (brtex::bakeTerrain) вместо маски кладёт запечённый композит.
struct TerrainRecipe { std::string tex; std::string layers[4]; float scale[4] = { 1, 1, 1, 1 }; float extent = 0; };   // extent: размер чанка по XY (м) — шейдер BR тайлит слои по мировым XY/5
struct RwFixStats { int chunks = 0, versionsChanged = 0, geomFixed = 0, geomWithFloats = 0, geomUnknown = 0, texNamesFixed = 0, rootSizeFixed = 0, lenientFixes = 0, matfxFixed = 0, longNames = 0, matfxTexFixed = 0, matfxTexExtracted = 0, fx2dDropped = 0, schemaFixes = 0, binmeshDropped = 0, terrain = 0, planarUv = 0; uint32_t srcVersion = 0; std::vector<TerrainRecipe> recipes; };
// Имя текстуры в нормальной форме (как в TXD/при поиске .btx): нижний регистр, без расширения, не длиннее 31 символа
static inline std::string normTexName(std::string l) {
    for (auto& c : l) c = (char)tolower((unsigned char)c);
    size_t dot = l.find_last_of('.');
    if (dot != std::string::npos && dot > 0) { std::string e = l.substr(dot + 1); if (e == "png" || e == "dds" || e == "tga" || e == "bmp" || e == "jpg" || e == "jpeg" || e == "btx") l.resize(dot); }
    if (l.size() > 31) l.resize(31);
    return l;
}
static const uint32_t rw2DFX = 0x253F2F9;        // 2d Effect (geometry extension)
static const uint32_t rwMATFX = 0x120;          // RpMatFX (material/atomic extension)

static inline bool isContainerId(uint32_t id) {
    switch (id) {
    case rwEXTENSION: case rwCAMERA: case rwTEXTURE: case rwMATERIAL: case rwMATLIST: case rwFRAMELIST:
    case rwGEOMETRY: case rwCLUMP: case rwLIGHT: case rwATOMIC: case rwGEOMLIST: case rwUVANIMDICT:
        return true;
    default: return false;
    }
}

static inline bool parseChildren(const uint8_t* p, size_t n, std::vector<RwNode>& out, int depth);
static thread_local bool g_rwLenient = false;     // терпимый разбор: чанк, вылезающий за родителя, обрезается; хвост < 12 байт игнорируется
static thread_local int  g_rwLenientFixes = 0;

static inline bool parseNode(const uint8_t* p, size_t n, RwNode& node, size_t& consumed, int depth) {
    if (n < 12) return false;
    node.id = rd32(p); uint32_t size = rd32(p + 4); node.libid = rd32(p + 8);
    if (size > n - 12) { if (!g_rwLenient) return false; size = (uint32_t)(n - 12); g_rwLenientFixes++; }
    consumed = 12 + size;
    node.container = false;
    if (depth < 32 && isContainerId(node.id) && size >= 12) {
        std::vector<RwNode> kids;
        if (parseChildren(p + 12, size, kids, depth + 1)) { node.container = true; node.kids.swap(kids); return true; }
    }
    node.data.assign(p + 12, p + 12 + size);
    return true;
}

static inline bool parseChildren(const uint8_t* p, size_t n, std::vector<RwNode>& out, int depth) {
    size_t off = 0;
    while (off < n) {
        if (n - off < 12) { if (g_rwLenient) { g_rwLenientFixes++; return true; } return false; }
        RwNode k; size_t c = 0;
        if (!parseNode(p + off, n - off, k, c, depth)) return false;
        out.push_back(std::move(k)); off += c;
    }
    return off == n;
}

// Диагностика: первый чанк, размер которого не помещается в родителя (для журнала)
static inline std::string rwTreeDiag(const uint8_t* p, size_t n, size_t base = 0, int depth = 0) {
    size_t off = 0; char t[160];
    while (off + 12 <= n) {
        uint32_t id = rd32(p + off), sz = rd32(p + off + 4);
        if (sz > n - off - 12) { snprintf(t, sizeof t, "chunk 0x%X at 0x%zX size %u exceeds parent (%zu left)", id, base + off, sz, n - off - 12); return t; }
        if (depth < 32 && isContainerId(id) && sz >= 12) { std::string d = rwTreeDiag(p + off + 12, sz, base + off + 12, depth + 1); if (!d.empty()) return d; }
        off += 12 + sz;
    }
    if (off != n) { snprintf(t, sizeof t, "%zu trailing bytes at 0x%zX", n - off, base + off); return t; }
    return "";
}

static inline void writeNode(const RwNode& node, std::vector<uint8_t>& out) {
    size_t hdr = out.size(); out.resize(hdr + 12);
    wr32(&out[hdr], node.id); wr32(&out[hdr + 8], node.libid);
    if (node.container) for (auto& k : node.kids) writeNode(k, out);
    else out.insert(out.end(), node.data.begin(), node.data.end());
    wr32(&out[hdr + 4], (uint32_t)(out.size() - hdr - 12));
}

// Размер тела geometry-struct для заданной раскладки. withFloats — есть ли 12 байт surface props (RW < 3.4)
static inline uint64_t geometryStructSize(const uint8_t* d, size_t n, bool withFloats) {
    if (n < 16) return 0;
    uint32_t flags = rd32(d), numTris = rd32(d + 4), numVerts = rd32(d + 8), numMorph = rd32(d + 12);
    uint64_t sz = 16 + (withFloats ? 12 : 0);
    if (!(flags & 0x01000000)) {                            // !rpGEOMETRYNATIVE
        uint32_t numTex = (flags >> 16) & 0xFF;
        if (numTex == 0) numTex = (flags & 0x04 ? 1 : 0) + (flags & 0x80 ? 2 : 0);
        if (flags & 0x08) sz += (uint64_t)numVerts * 4;    // prelit
        sz += (uint64_t)numTex * numVerts * 8;
        sz += (uint64_t)numTris * 8;
    }
    size_t off = (size_t)sz;
    for (uint32_t m = 0; m < numMorph; m++) {
        if (off + 24 > n) return 0;
        uint32_t hasV = rd32(d + off + 16), hasN = rd32(d + off + 20);
        sz += 24; off += 24;
        if (hasV) { sz += (uint64_t)numVerts * 12; off += numVerts * 12; }
        if (hasN) { sz += (uint64_t)numVerts * 12; off += numVerts * 12; }
        if (off > n) return 0;
    }
    return sz;
}

// ---------------------------------------------------------------------------
// Проверка файла как игровой модели: корень — clump, есть хотя бы одна
// непустая geometry, у geometry есть материалы, и объявленные данные
// помещаются в файл. Расклады: новые (geometry внутри atomic) и старые BR
// (geometries в clump-уровневом GEOMLIST, атомики ссылаются по индексу —
// атомики без своей geometry там легитимны). Пустые заглушки клиента (clump
// в ~200-1200 байт вообще без geometry — так клиент хранит неиспользуемые
// оружие/транспорт, напр. rocketla/seaspar) и обрезанные модели отсеиваются
// ДО передачи игре: иначе игра падает позже — уже в рендере/стриминге, без
// внятного следа в журнале. Политика — только положительные находки: если
// дерево не разобралось, модель НЕ бракуем (её читает игра, как раньше).
// ---------------------------------------------------------------------------

// один чанк: заголовок 12 байт — ровно тот формат, в который наш нормализатор
// приводит файлы и который читает игра (проверено на сотнях моделей клиента)
struct BrChunk { uint32_t id; const uint8_t* p; uint32_t len; };

static inline bool brChunkAt(const uint8_t* p, const uint8_t* end, BrChunk& c, const uint8_t*& next) {
    if (end - p < 12) return false;
    c.id = rd32(p);
    uint32_t size = rd32(p + 4);
    if (size > (uint32_t)(end - p - 12)) return false;
    c.p = p + 12; c.len = size;
    next = p + 12 + size;
    return true;
}

// ищет в детях чанка rwSTRUCT и возвращает его данные
static inline bool brFindStruct(const uint8_t* p, const uint8_t* end, const uint8_t*& sd, uint32_t& slen) {
    const uint8_t* q = p;
    while (q < end) {
        BrChunk k; const uint8_t* after;
        if (!brChunkAt(q, end, k, after)) return false;
        q = after;
        if (k.id == rwSTRUCT) { sd = k.p; slen = k.len; return true; }
    }
    return false;
}

// проверка одной geometry (чанк rwGEOMETRY): непустая, с материалами, данные внутри файла
static inline bool brCheckGeometry(const BrChunk& geom, std::string& why) {
    char t[160];
    const uint8_t* g = geom.p; const uint8_t* gend = geom.p + geom.len;
    const uint8_t* sd = nil; uint32_t slen = 0, numMats = 1;
    while (g < gend) {
        BrChunk gk; const uint8_t* gkafter;
        if (!brChunkAt(g, gend, gk, gkafter)) break;
        g = gkafter;
        if (gk.id == rwMATLIST) {
            const uint8_t* msd; uint32_t mlen;
            if (brFindStruct(gk.p, gk.p + gk.len, msd, mlen) && mlen >= 4)
                numMats = rd32(msd);
        }
    }
    if (!brFindStruct(geom.p, gend, sd, slen)) return true;    // без struct не судим
    if (slen < 16) {
        snprintf(t, sizeof t, "a geometry struct is too small (%u bytes)", slen);
        why = t;
        return false;
    }
    uint32_t flags = rd32(sd), numTris = rd32(sd + 4), numVerts = rd32(sd + 8), numMorph = rd32(sd + 12);
    bool native = (flags & 0x01000000) != 0;
    if (numVerts == 0 || numTris == 0) {
        snprintf(t, sizeof t, "a geometry is empty (%u verts, %u tris) - an empty model stub", numVerts, numTris);
        why = t;
        return false;
    }
    if (numVerts > 0x10000) {
        snprintf(t, sizeof t, "a geometry declares too many vertices (%u)", numVerts);
        why = t;
        return false;
    }
    if (numMorph > 16) {
        snprintf(t, sizeof t, "a geometry declares too many morph targets (%u)", numMorph);
        why = t;
        return false;
    }
    if (!native) {
        uint64_t e0 = geometryStructSize(sd, slen, false);
        uint64_t e1 = e0 ? e0 : geometryStructSize(sd, slen, true);
        bool fits = (e0 > 0 && e0 <= slen) || (e1 > 0 && e1 <= slen);
        if (!fits) {
            snprintf(t, sizeof t, "a geometry declares more data (%u verts, %u tris) than the file holds - the model is truncated", numVerts, numTris);
            why = t;
            return false;
        }
    }
    if (numMats == 0) {
        snprintf(t, sizeof t, "a geometry has no materials - an empty model stub");
        why = t;
        return false;
    }
    return true;
}

// ищет в детях чанка все geometry и проверяет каждую; возвращает число проверенных
static inline int brCheckGeometriesIn(const uint8_t* p, const uint8_t* end, std::string& why) {
    int n = 0;
    const uint8_t* q = p;
    while (q < end) {
        BrChunk k; const uint8_t* after;
        if (!brChunkAt(q, end, k, after)) break;
        q = after;
        if (k.id != rwGEOMETRY) continue;
        n++;
        if (!brCheckGeometry(k, why)) return -n;   // отрицательное — уже с ошибкой
    }
    return n;
}

static inline bool validateClumpForGame(const uint8_t* p, size_t n, std::string& why) {
    why.clear();
    char t[160];
    if (n < 12) { why = "the file is too small to be a model"; return false; }
    const uint8_t* end = p + n;
    BrChunk root; const uint8_t* after;
    if (!brChunkAt(p, end, root, after)) return true;      // не разобрались — не бракуем
    if (root.id != rwCLUMP) return true;                   // не clump — пусть решает игра

    int atomics = 0, atomicsWithGeom = 0, geomsChecked = 0;
    const uint8_t* q = root.p; const uint8_t* qend = root.p + root.len;
    while (q < qend) {
        BrChunk k; const uint8_t* kafter;
        if (!brChunkAt(q, qend, k, kafter)) return true;
        q = kafter;
        if (k.id == rwATOMIC) {
            atomics++;
            // geometry либо внутри атомика, либо (старый BR-расклад) в GEOMLIST уровня clump
            int own = brCheckGeometriesIn(k.p, k.p + k.len, why);
            if (own < 0) return false;                     // проверка geometry уже написала why
            if (own > 0) { atomicsWithGeom += own; geomsChecked += own; }
        } else if (k.id == rwGEOMLIST) {
            int gn = brCheckGeometriesIn(k.p, k.p + k.len, why);
            if (gn < 0) return false;
            geomsChecked += gn;
        }
    }
    if (atomics == 0) {
        snprintf(t, sizeof t, "the clump holds no atomics - an empty model stub");
        why = t;
        return false;
    }
    if (geomsChecked == 0) {
        snprintf(t, sizeof t, "the model holds no geometry - an empty model stub");
        why = t;
        return false;
    }
    // атомики без своей geometry законны только при общем GEOMLIST
    if (atomicsWithGeom == 0 && geomsChecked == 0) {
        snprintf(t, sizeof t, "the atomics hold no geometry - an empty model stub");
        why = t;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RpMatFX (0x120) в material extension. Внутри блоба лежат ВЛОЖЕННЫЕ RW texture-чанки:
//   стандартные типы 1..6 (bump/env/dual) — по раскладке RW;
//   BR-тип 10 (PBR): u32 10, u32 ?, u32 nTex, nTex x { u32 keyLen, key, texture-chunk }, затем float/color/string/int списки.
// Возвращает список (offset, длина с заголовком) вложенных texture-чанков; для типа 10 ещё и ключи (albedoTex/diffuseTex/...).
struct MatFxTex { size_t off, len; std::string key; };
static inline uint32_t matfxTextures(const uint8_t* d, size_t n, std::vector<MatFxTex>& out) {
    out.clear(); if (n < 4) return 0;
    uint32_t type = rd32(d);
    auto texAt = [&](size_t off, size_t& len) -> bool {
        if (off + 12 > n || rd32(d + off) != rwTEXTURE) return false;
        uint32_t sz = rd32(d + off + 4); if ((uint64_t)off + 12 + sz > n) return false; len = 12 + sz; return true;
    };
    if (type == 9) {
        // BR terrain v1: type(9), sub(9), затем 5 x { float scale, rwTEXTURE }: 4 тайловых слоя (R,G,B,A маски) и сама маска, хвост u32 0.
        static const char* keys9[5] = { "landscapeMapR", "landscapeMapG", "landscapeMapB", "landscapeMapA", "landscapeMask" };
        size_t off = 8;
        for (int i = 0; i < 5 && off + 4 <= n; i++) {
            off += 4; size_t len; if (!texAt(off, len)) break;
            out.push_back({ off, len, keys9[i] }); off += len;
        }
        return type;
    }
    if (type == 10) {
        if (n < 12) return type;
        uint32_t nTex = rd32(d + 8); size_t off = 12;
        for (uint32_t i = 0; i < nTex && off + 4 <= n; i++) {
            uint32_t kl = rd32(d + off); off += 4; if (kl > 64 || off + kl > n) break;
            std::string key((const char*)d + off, kl); off += kl;
            size_t len; if (!texAt(off, len)) break;
            out.push_back({ off, len, key }); off += len;
        }
        return type;
    }
    if (type == 0 || type > 6) return type;
    size_t off = 4;
    for (int pass = 0; pass < 2 && off + 4 <= n; pass++) {
        uint32_t t = rd32(d + off); off += 4; size_t len;
        switch (t) {
        case 1: /* bump: coef, hasTex, [tex], hasBumpedTex, [tex] */
            if (off + 8 > n) return type;
            { uint32_t has = rd32(d + off + 4); off += 8; if (has) { if (!texAt(off, len)) return type; out.push_back({ off, len, "bump" }); off += len; } }
            if (off + 4 > n) return type;
            { uint32_t has = rd32(d + off); off += 4; if (has) { if (!texAt(off, len)) return type; out.push_back({ off, len, "bumped" }); off += len; } }
            break;
        case 2: /* env: coef, useFBalpha, hasTex, [tex] */
        case 4: /* dual: srcBlend, dstBlend, hasTex, [tex] */
            if (off + 12 > n) return type;
            { uint32_t has = rd32(d + off + 8); off += 12; if (has) { if (!texAt(off, len)) return type; out.push_back({ off, len, t == 2 ? "env" : "dual" }); off += len; } }
            break;
        case 0: case 5: break;
        default: return type;
        }
    }
    return type;
}

static inline void fixTree(RwNode& node, uint32_t parentId, RwFixStats& st);
// Проверка 2dfx-чанка по правилам ридера SA: count, затем записи {pos[12], type u32, size u32, data[size]}.
static inline bool valid2dfx(const std::vector<uint8_t>& d) {
    if (d.size() < 4) return false;
    uint32_t cnt; memcpy(&cnt, d.data(), 4);
    if (cnt == 0) return true;
    if (cnt > 256) return false;
    size_t pos = 4;
    for (uint32_t i = 0; i < cnt; i++) {
        if (pos + 20 > d.size()) return false;
        uint32_t type, sz; memcpy(&type, d.data() + pos + 12, 4); memcpy(&sz, d.data() + pos + 16, 4);
        if (type > 20 || sz > 4096 || pos + 20 + sz > d.size()) return false;
        pos += 20 + sz;
    }
    return pos == d.size();
}

// Имя (первая rwSTRING) вложенного texture-чанка внутри блоба matfx
static inline std::string matfxTexName(const uint8_t* blob, const MatFxTex& t) {
    std::vector<RwNode> nodes;
    if (!parseChildren(blob + t.off, t.len, nodes, 8) || nodes.size() != 1 || !nodes[0].container) return "";
    for (auto& k : nodes[0].kids) if (k.id == rwSTRING && !k.container) return std::string((const char*)k.data.data(), strnlen((const char*)k.data.data(), k.data.size()));
    return "";
}
// vec4-параметр из хвоста блоба типа 10 (после текстур: u32 0, float-список, vec3-список, vec4-список — {u32 len, key, значения})
static inline bool matfx10Vec4(const uint8_t* d, size_t n, const char* key, float out[4]) {
    std::vector<MatFxTex> texs; if (matfxTextures(d, n, texs) != 10) return false;
    size_t off = texs.empty() ? 12 : texs.back().off + texs.back().len;
    if (off + 4 > n) return false; off += 4;                                   // u32 0 (неизвестно)
    const size_t valSz[3] = { 4, 12, 16 };
    for (int sec = 0; sec < 3; sec++) {
        if (off + 4 > n) return false; uint32_t cnt = rd32(d + off); off += 4; if (cnt > 256) return false;
        for (uint32_t i = 0; i < cnt; i++) {
            if (off + 4 > n) return false; uint32_t kl = rd32(d + off); off += 4; if (kl > 64 || off + kl + valSz[sec] > n) return false;
            if (sec == 2 && kl == strlen(key) && memcmp(d + off, key, kl) == 0) { memcpy(out, d + off + kl, 16); return true; }
            off += kl + valSz[sec];
        }
    }
    return false;
}

// Масштабы слоёв террейна типа 9 (float перед каждым из 4 слоёв)
static inline bool matfx9Scales(const uint8_t* d, size_t n, float out[4]) {
    std::vector<MatFxTex> texs; if (matfxTextures(d, n, texs) != 9) return false;
    for (auto& t : texs) { int i = t.key == "landscapeMapR" ? 0 : t.key == "landscapeMapG" ? 1 : t.key == "landscapeMapB" ? 2 : t.key == "landscapeMapA" ? 3 : -1;
        if (i >= 0 && t.off >= 4) { float f; memcpy(&f, d + t.off - 4, 4); if (f > 0.01f && f < 1000.f) out[i] = f; } }
    return true;
}

// Нормализация блоба matfx: вложенные текстуры прогоняем через fixTree (libid, имена) и пересобираем блоб.
static inline void fixMatFxBlob(std::vector<uint8_t>& data, RwFixStats& st) {
    std::vector<MatFxTex> texs; uint32_t type = matfxTextures(data.data(), data.size(), texs);
    if (type == 0 || type > 6 || texs.empty()) return;
    std::vector<uint8_t> out; size_t pos = 0; bool changed = false;
    for (auto& t : texs) {
        out.insert(out.end(), data.begin() + pos, data.begin() + t.off);
        std::vector<RwNode> nodes;
        if (parseChildren(data.data() + t.off, t.len, nodes, 8) && nodes.size() == 1 && nodes[0].container) {
            int v0 = st.versionsChanged, n0 = st.texNamesFixed + st.longNames;
            fixTree(nodes[0], rwMATERIAL, st); writeNode(nodes[0], out);
            if (st.versionsChanged != v0 || st.texNamesFixed + st.longNames != n0) changed = true;
        } else out.insert(out.end(), data.begin() + t.off, data.begin() + t.off + t.len);
        pos = t.off + t.len;
    }
    out.insert(out.end(), data.begin() + pos, data.end());
    data.swap(out);
    if (changed) st.matfxTexFixed++;
}

static inline void fixTree(RwNode& node, uint32_t parentId, RwFixStats& st) {
    st.chunks++;
    uint32_t ver = libidToVersion(node.libid);
    if (!st.srcVersion) st.srcVersion = ver;

    if (node.id == rwSTRUCT && parentId == rwGEOMETRY && !node.container) {
        size_t n = node.data.size();
        uint64_t withF = geometryStructSize(node.data.data(), n, true);
        uint64_t noF   = geometryStructSize(node.data.data(), n, false);
        if (withF == n) {
            // RW < 3.4 (III/VC): после 16-байтного заголовка идут ambient/specular/diffuse — в SA их нет
            node.data.erase(node.data.begin() + 16, node.data.begin() + 28);
            st.geomFixed++; st.geomWithFloats++;
        } else if (noF == n) {
            if (ver < 0x34000) st.geomFixed++;   // помечен старой версией, но раскладка уже новая
        } else {
            st.geomUnknown++;                    // не смогли распознать раскладку — не трогаем данные
        }
    }
    if (node.id == rwTEXTURE && node.container) {
        // BR ссылается на текстуры как "name.png" — в SA/TXD имена без расширения
        for (auto& k : node.kids) if (k.id == rwSTRING && !k.container) {
            std::string nm((const char*)k.data.data(), strnlen((const char*)k.data.data(), k.data.size()));
            bool changed = false;
            size_t dot = nm.find_last_of('.');
            if (dot != std::string::npos && dot > 0) {
                std::string ext = nm.substr(dot + 1); for (auto& c : ext) c = (char)tolower((unsigned char)c);
                if (ext == "png" || ext == "dds" || ext == "tga" || ext == "bmp" || ext == "jpg" || ext == "jpeg" || ext == "btx") { nm.resize(dot); changed = true; st.texNamesFixed++; }
            }
            // RW SA хранит имя в char[32]: длинные имена BR (до 40+ символов) обрезаем так же, как это делает наш TXD
            if (nm.size() > 31) { nm.resize(31); changed = true; st.longNames++; }
            if (changed) {
                size_t len = (nm.size() + 1 + 3) & ~(size_t)3; if (len < 4) len = 4;
                k.data.assign(len, 0); memcpy(k.data.data(), nm.data(), nm.size());
            }
            break;   // только первая строка (имя); вторая — маска
        }
    }
    if (node.id == rwMATERIAL && node.container) {
        // matfx в material extension:
        //  - типы 1..6 (env/bump/dual): внутри лежат вложенные rwTEXTURE со СТАРЫМ libid (у VC-моделей BR) —
        //    RwTextureStreamRead в SA принимает только версии 0x34000..0x36003, иначе чтение материала падает
        //    и вся модель отвергается (это причина с sunrise). Прогоняем вложенные текстуры через fixTree.
        //  - BR-тип 10 (PBR): RW SA его не знает -> модель отвергается (flowera). У таких материалов обычно НЕТ
        //    обычного rwTEXTURE — диффуз лежит только внутри блоба (diffuseTex/albedoTex): достаём его наружу,
        //    а сам эффект обнуляем.
        size_t extIdx = SIZE_MAX, structIdx = SIZE_MAX; bool hasTex = false;
        for (size_t i = 0; i < node.kids.size(); i++) { auto& k = node.kids[i]; if (k.id == rwEXTENSION && k.container) extIdx = i; else if (k.id == rwSTRUCT && !k.container) structIdx = i; else if (k.id == rwTEXTURE) hasTex = true; }
        if (extIdx != SIZE_MAX) {
            size_t fxIdx = SIZE_MAX;
            for (size_t j = 0; j < node.kids[extIdx].kids.size(); j++) { auto& f = node.kids[extIdx].kids[j]; if (f.id == rwMATFX && !f.container && f.data.size() >= 4) { fxIdx = j; break; } }
            if (fxIdx != SIZE_MAX) {
                std::vector<MatFxTex> texs; uint32_t type = matfxTextures(node.kids[extIdx].kids[fxIdx].data.data(), node.kids[extIdx].kids[fxIdx].data.size(), texs);
                if (type <= 6) fixMatFxBlob(node.kids[extIdx].kids[fxIdx].data, st);
                else {
                    RwNode extracted; bool got = false;
                    if (!hasTex) {
                        const MatFxTex* pick = nullptr;
                        for (auto& t : texs) { if (t.key == "diffuseTex") pick = &t; }
                        if (!pick) { for (auto& t : texs) { if (t.key == "albedoTex") pick = &t; } }
                        if (!pick) { for (auto& t : texs) { if (t.key == "baseColorTex" || t.key == "colorTex") pick = &t; } }
                        const MatFxTex* mask = nullptr; const MatFxTex* lay[4] = { nullptr, nullptr, nullptr, nullptr };
                        if (!pick) {
                            // pbrTerrain: диффуза нет вовсе; наружу выносим маску (имя = имя модели), а рецепт запекания отдаём сборщику TXD
                            for (auto& t : texs) { if (t.key == "landscapeMask") mask = &t; else if (t.key == "landscapeMapR") lay[0] = &t; else if (t.key == "landscapeMapG") lay[1] = &t; else if (t.key == "landscapeMapB") lay[2] = &t; else if (t.key == "landscapeMapA") lay[3] = &t; }
                            for (int i = 0; i < 4; i++) if (lay[i] && matfxTexName(node.kids[extIdx].kids[fxIdx].data.data(), *lay[i]).empty()) lay[i] = nullptr;   // пустой слот слоя (тип 9)
                            if (mask) pick = mask; else for (auto& t : texs) { if (t.key.rfind("landscapeMap", 0) == 0) { pick = &t; break; } }
                        }
                        if (pick) {
                            std::vector<RwNode> nodes; const auto& blob = node.kids[extIdx].kids[fxIdx].data;
                            if (parseChildren(blob.data() + pick->off, pick->len, nodes, 8) && nodes.size() == 1 && nodes[0].container) { extracted = std::move(nodes[0]); got = true; }
                            if (got && pick == mask) {
                                TerrainRecipe r; r.tex = normTexName(matfxTexName(blob.data(), *mask));
                                for (int i = 0; i < 4; i++) if (lay[i]) r.layers[i] = normTexName(matfxTexName(blob.data(), *lay[i]));
                                if (type == 9) matfx9Scales(blob.data(), blob.size(), r.scale); else matfx10Vec4(blob.data(), blob.size(), "landscapeScale", r.scale);
                                if (!r.tex.empty()) { st.recipes.push_back(r); st.terrain++; }
                            }
                        }
                    }
                    // v6.4.3: чанк matfx удаляем целиком. Раньше писали 4 байта «тип 0», но ридер RW (rpmatfx) после типа
                    // ВСЕГДА читает ещё 2 x u32 подэффектов -> перечитывал 8 байт из следующего чанка (Refl), поток
                    // сбивался, RpClumpStreamRead возвращал NULL и модель вечно перезапрашивалась (lf_kor_*_LOD2 и др.).
                    node.kids[extIdx].kids.erase(node.kids[extIdx].kids.begin() + fxIdx); st.matfxFixed++;
                    if (got) {
                        if (structIdx != SIZE_MAX && node.kids[structIdx].data.size() >= 16) wr32(&node.kids[structIdx].data[12], 1);   // isTextured
                        // у PBR/terrain-материалов BR цвет 0x00000000 и ambient/specular/diffuse = 0 (шейдер BR их не использует);
                        // в SA такой материал рисуется чёрным -> ставим белый и стандартные surface props
                        if (structIdx != SIZE_MAX && node.kids[structIdx].data.size() >= 28) {
                            auto& sd = node.kids[structIdx].data;
                            if (rd32(&sd[4]) == 0 || sd[7] == 0) wr32(&sd[4], 0xFFFFFFFF);
                            float amb, spec, dif; memcpy(&amb, &sd[16], 4); memcpy(&spec, &sd[20], 4); memcpy(&dif, &sd[24], 4);
                            if (!(amb > 0.f)) { amb = 1.f; memcpy(&sd[16], &amb, 4); } if (!(spec >= 0.f) || spec > 1.f) { spec = 0.f; memcpy(&sd[20], &spec, 4); } if (!(dif > 0.f)) { dif = 1.f; memcpy(&sd[24], &dif, 4); }
                        }
                        size_t at = structIdx == SIZE_MAX ? 0 : structIdx + 1;
                        node.kids.insert(node.kids.begin() + at, std::move(extracted)); st.matfxTexExtracted++;
                    }
                }
            }
        }
    }
    if (node.container && node.id == rwGEOMETRY) {
        // v6.7: BinMesh, не согласованный с геометрией (число сплитов/индексов не сходится с размером чанка, материал сплита >=
        // numMaterials, индекс >= numVerts) — удаляем: без плагина RpGeometryStreamRead строит меши заново из треугольников.
        uint32_t numVerts = 0, numMat = 0;
        for (auto& g : node.kids) if (g.id == rwSTRUCT && !g.container && g.data.size() >= 16) { numVerts = rd32(g.data.data() + 8); break; }
        for (auto& g : node.kids) if (g.id == rwMATLIST && g.container && !g.kids.empty() && g.kids[0].id == rwSTRUCT && g.kids[0].data.size() >= 4) { numMat = rd32(g.kids[0].data.data()); break; }
        for (auto& g : node.kids) if (g.id == rwEXTENSION && g.container) for (size_t i = 0; i < g.kids.size();) {
            auto& e = g.kids[i]; bool bad = false;
            if (e.id == 0x50E && !e.container) {
                const uint8_t* b = e.data.data(); size_t bn = e.data.size();
                if (bn < 12) bad = true;
                else {
                    uint32_t nSplit = rd32(b + 4), total = rd32(b + 8); size_t o = 12; uint32_t sum = 0;
                    for (uint32_t sI = 0; sI < nSplit && !bad; sI++) {
                        if (o + 8 > bn) { bad = true; break; }
                        uint32_t cnt = rd32(b + o), mi = rd32(b + o + 4); o += 8;
                        if ((numMat && mi >= numMat) || o + (size_t)cnt * 4 > bn) { bad = true; break; }
                        if (numVerts) for (uint32_t k = 0; k < cnt; k++) if (rd32(b + o + (size_t)k * 4) >= numVerts) { bad = true; break; }
                        o += (size_t)cnt * 4; sum += cnt;
                    }
                    if (!bad && (o != bn || sum != total)) bad = true;
                }
            }
            if (bad) { g.kids.erase(g.kids.begin() + i); st.binmeshDropped++; } else i++;
        }
    }
    if (node.container && node.id == rwEXTENSION) {
        // BR-модели содержат 2dfx с count=0xFFFFFFFF и мусором: ридер SA (Rwt2dEffectPluginDataChunkReadCallBack)
        // верит count -> Malloc/чтение мусора -> RpClumpStreamRead падает -> модель бесконечно перезапрашивается.
        for (size_t i = 0; i < node.kids.size();) {
            if (!node.kids[i].container && node.kids[i].id == rw2DFX && !valid2dfx(node.kids[i].data)) { node.kids.erase(node.kids.begin() + i); st.fx2dDropped++; }
            else i++;
        }
    }
    if (node.libid != SA_LIBID) { node.libid = SA_LIBID; st.versionsChanged++; }
    int terrainBefore = st.terrain; size_t recipesBefore = st.recipes.size();
    if (node.container) for (auto& k : node.kids) fixTree(k, node.id, st);
    // v6.9: у части ландшафтных чанков BR (LND_uznii_a17 и др.) геометрия БЕЗ UV: шейдер BR берёт координаты слоёв из
    // мировых XY (Vertex.xy/5). В SA такой меш рисуется одним пикселем текстуры. Даём планарную UV по bbox XY -> запечённая
    // терраин-текстура ложится на весь чанк.
    if (node.container && node.id == rwGEOMETRY && st.terrain > terrainBefore) {
        for (auto& g : node.kids) if (g.id == rwSTRUCT && !g.container && g.data.size() >= 16) {
            auto& d = g.data; size_t n = d.size();
            uint32_t flags = rd32(d.data()), numTris = rd32(d.data() + 4), numVerts = rd32(d.data() + 8), numMorph = rd32(d.data() + 12);
            uint32_t numTex = (flags >> 16) & 0xFF; if (numTex == 0) numTex = (flags & 0x04 ? 1 : 0) + (flags & 0x80 ? 2 : 0);
            if ((flags & 0x01000000) || !numVerts || !numMorph || geometryStructSize(d.data(), n, false) != n) break;
            size_t off = 16; if (flags & 0x08) off += (size_t)numVerts * 4;
            size_t triOff = off + (size_t)numTex * numVerts * 8, vOff = triOff + (size_t)numTris * 8 + 24;
            if (numTex) {   // UV уже есть: только размер чанка для авто-тайлинга
                if (rd32(d.data() + vOff - 8) == 0 || vOff + (size_t)numVerts * 12 > n) break;
                float mn[2] = { 1e30f, 1e30f }, mx[2] = { -1e30f, -1e30f };
                for (uint32_t i = 0; i < numVerts; i++) { float v[3]; memcpy(v, d.data() + vOff + (size_t)i * 12, 12); for (int k = 0; k < 2; k++) { mn[k] = std::min(mn[k], v[k]); mx[k] = std::max(mx[k], v[k]); } }
                for (size_t ri = recipesBefore; ri < st.recipes.size(); ri++) st.recipes[ri].extent = std::max(mx[0] - mn[0], mx[1] - mn[1]);
                break;
            }
            if (rd32(d.data() + vOff - 8) == 0 || vOff + (size_t)numVerts * 12 > n) break;   // нет вершин в morph target 0
            float mn[2] = { 1e30f, 1e30f }, mx[2] = { -1e30f, -1e30f };
            for (uint32_t i = 0; i < numVerts; i++) { float v[3]; memcpy(v, d.data() + vOff + (size_t)i * 12, 12); for (int k = 0; k < 2; k++) { mn[k] = std::min(mn[k], v[k]); mx[k] = std::max(mx[k], v[k]); } }
            float ex = std::max(mx[0] - mn[0], 1e-3f), ey = std::max(mx[1] - mn[1], 1e-3f);
            for (size_t ri = recipesBefore; ri < st.recipes.size(); ri++) st.recipes[ri].extent = std::max(ex, ey);
            std::vector<uint8_t> uv((size_t)numVerts * 8);
            for (uint32_t i = 0; i < numVerts; i++) { float v[3]; memcpy(v, d.data() + vOff + (size_t)i * 12, 12); float u = (v[0] - mn[0]) / ex, t = 1.f - (v[1] - mn[1]) / ey; memcpy(&uv[(size_t)i * 8], &u, 4); memcpy(&uv[(size_t)i * 8 + 4], &t, 4); }
            d.insert(d.begin() + triOff, uv.begin(), uv.end());
            flags = (flags & ~0x00FF0000u) | 0x00010000u | 0x04u; wr32(&d[0], flags);
            st.planarUv++; break;
        }
    }
}


// ---------------------------------------------------------------------------
// v6.7: восстановление дерева по СХЕМЕ RpClump, когда размеры чанков врут.
//   В BR встречаются модели, где у одного Material size=52 вместо 140, у MaterialList size=10000 (константа),
//   у Extension геометрии — размер с потолка. Обычный разбор «по размерам» тогда рассыпается (2-я текстура
//   вылезает за материал, потом 2dfx/тела читаются как чанки id=0x5D000004 и т.п.). Но структура известна:
//   Clump{Struct, FrameList{Struct, Ext*N}, GeomList{Struct, Geometry{Struct, MatList{Struct, Material{Struct, [Texture{Struct,String,String,Ext}], Ext}*M}, Ext}*G}, Atomic{Struct, Ext}*A, Ext}
//   Идём по этой схеме, размеры листьев берём из их заголовков (они верны), а размеры контейнеров пересчитываем.
//   Extension читается как список leaf-плагинов до первого «структурного» id / чужого libid / выхода за лимит.
// ---------------------------------------------------------------------------
struct SchemaCtx { const uint8_t* d; size_t n; int fixes; bool fail; };
static inline bool schHdr(SchemaCtx& c, size_t o, uint32_t& id, uint32_t& sz, uint32_t& lib) {
    if (o + 12 > c.n) return false; id = rd32(c.d + o); sz = rd32(c.d + o + 4); lib = rd32(c.d + o + 8); return true;
}
static inline bool schLeaf(SchemaCtx& c, size_t& o, uint32_t expect, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != expect) return false;
    // BR (детали тюнинга): в размере Struct старший байт занят мусором/флагом (0x0100000C вместо 12)
    if (sz > c.n - o - 12 && (sz & 0xFF000000) && (sz & 0x00FFFFFF) <= c.n - o - 12) { sz &= 0x00FFFFFF; c.fixes++; }
    if (sz > c.n - o - 12) return false;
    out.id = id; out.libid = lib; out.container = false; out.data.assign(c.d + o + 12, c.d + o + 12 + sz); o += 12 + sz; return true;
}
static inline bool schIsStructural(uint32_t id) {
    switch (id) { case rwSTRUCT: case rwSTRING: case rwEXTENSION: case rwTEXTURE: case rwMATERIAL: case rwMATLIST: case rwFRAMELIST:
                  case rwGEOMETRY: case rwCLUMP: case rwATOMIC: case rwGEOMLIST: case rwLIGHT: case rwCAMERA: case rwUVANIMDICT: return true; default: return false; }
}
static inline bool schExtension(SchemaCtx& c, size_t& o, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != rwEXTENSION) return false;
    out.id = rwEXTENSION; out.libid = lib; out.container = true; out.kids.clear();
    size_t p = o + 12, declared = o + 12 + (size_t)sz;
    auto plausibleNext = [&](size_t q) { if (q == c.n) return true; uint32_t ni, ns, nl; if (!schHdr(c, q, ni, ns, nl)) return false; return nl == lib && ni != 0 && ns <= c.n - q - 12; };
    for (;;) {
        uint32_t ci, cs, cl; if (!schHdr(c, p, ci, cs, cl)) break;
        if (schIsStructural(ci) || cl != lib || (ci == 0 && cs == 0) || cs > c.n - p - 12) break;
        if (ci == 0x50E && cs >= 12 && !plausibleNext(p + 12 + cs)) {
            // BinMesh с неверным размером: длину берём по содержимому (сплиты {count, material, idx[count]})
            size_t b = p + 12, q = 12, lim = c.n - b; uint32_t got = 0;
            while (q + 8 <= lim) { uint32_t n = rd32(c.d + b + q), m = rd32(c.d + b + q + 4); if (m > 65535 || n > 16777216 || q + 8 + (size_t)n * 4 > lim) break; q += 8 + (size_t)n * 4; got++; if (plausibleNext(b + q)) break; }
            if (got && plausibleNext(b + q)) { cs = (uint32_t)q; c.fixes++; }
        }
        RwNode k; k.id = ci; k.libid = cl; k.container = false; k.data.assign(c.d + p + 12, c.d + p + 12 + cs); out.kids.push_back(std::move(k)); p += 12 + cs;
    }
    if (p != declared) c.fixes++;
    o = p; return true;
}
static inline bool schTexture(SchemaCtx& c, size_t& o, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != rwTEXTURE) return false;
    out.id = rwTEXTURE; out.libid = lib; out.container = true; out.kids.resize(4);
    size_t p = o + 12;
    if (!schLeaf(c, p, rwSTRUCT, out.kids[0]) || !schLeaf(c, p, rwSTRING, out.kids[1]) || !schLeaf(c, p, rwSTRING, out.kids[2]) || !schExtension(c, p, out.kids[3])) return false;
    if (p != o + 12 + (size_t)sz) c.fixes++;
    o = p; return true;
}
static inline bool schMaterial(SchemaCtx& c, size_t& o, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != rwMATERIAL) return false;
    out.id = rwMATERIAL; out.libid = lib; out.container = true; out.kids.clear();
    size_t p = o + 12; RwNode k;
    if (!schLeaf(c, p, rwSTRUCT, k) || k.data.size() < 12) return false;
    bool textured = rd32(k.data.data() + 12) != 0; out.kids.push_back(std::move(k));   // RpMaterial struct: flags, color, unused, isTextured, ...
    if (textured) { RwNode t; if (!schTexture(c, p, t)) return false; out.kids.push_back(std::move(t)); }
    RwNode e; if (!schExtension(c, p, e)) return false; out.kids.push_back(std::move(e));
    if (p != o + 12 + (size_t)sz) c.fixes++;
    o = p; return true;
}
static inline bool schGeometry(SchemaCtx& c, size_t& o, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != rwGEOMETRY) return false;
    out.id = rwGEOMETRY; out.libid = lib; out.container = true; out.kids.clear();
    size_t p = o + 12; RwNode k;
    if (!schLeaf(c, p, rwSTRUCT, k)) return false; out.kids.push_back(std::move(k));
    // MaterialList
    uint32_t mid, msz, mlib; if (!schHdr(c, p, mid, msz, mlib) || mid != rwMATLIST) return false;
    RwNode ml; ml.id = rwMATLIST; ml.libid = mlib; ml.container = true; size_t q = p + 12; RwNode ms;
    if (!schLeaf(c, q, rwSTRUCT, ms) || ms.data.size() < 4) return false;
    uint32_t nm = rd32(ms.data.data()); if (nm > 4096) return false; ml.kids.push_back(std::move(ms));
    for (uint32_t i = 0; i < nm; i++) { RwNode m; if (!schMaterial(c, q, m)) return false; ml.kids.push_back(std::move(m)); }
    if (q != p + 12 + (size_t)msz) c.fixes++;
    p = q; out.kids.push_back(std::move(ml));
    RwNode e; if (!schExtension(c, p, e)) return false; out.kids.push_back(std::move(e));
    if (p != o + 12 + (size_t)sz) c.fixes++;
    o = p; return true;
}
static inline bool schClump(SchemaCtx& c, size_t& o, RwNode& out) {
    uint32_t id, sz, lib; if (!schHdr(c, o, id, sz, lib) || id != rwCLUMP) return false;
    out.id = rwCLUMP; out.libid = lib; out.container = true; out.kids.clear();
    size_t p = o + 12; RwNode k;
    if (!schLeaf(c, p, rwSTRUCT, k) || k.data.size() < 4) return false;
    uint32_t nAtomics = rd32(k.data.data()); if (nAtomics > 4096) return false; out.kids.push_back(std::move(k));
    // FrameList
    { uint32_t fid, fsz, flib; if (!schHdr(c, p, fid, fsz, flib) || fid != rwFRAMELIST) return false;
      RwNode fl; fl.id = rwFRAMELIST; fl.libid = flib; fl.container = true; size_t q = p + 12; RwNode fs;
      if (!schLeaf(c, q, rwSTRUCT, fs) || fs.data.size() < 4) return false;
      uint32_t nf = rd32(fs.data.data()); if (nf > 4096) return false; fl.kids.push_back(std::move(fs));
      for (uint32_t i = 0; i < nf; i++) { RwNode e; if (!schExtension(c, q, e)) return false; fl.kids.push_back(std::move(e)); }
      if (q != p + 12 + (size_t)fsz) c.fixes++;
      p = q; out.kids.push_back(std::move(fl)); }
    // GeometryList
    { uint32_t gid, gsz, glib; if (!schHdr(c, p, gid, gsz, glib) || gid != rwGEOMLIST) return false;
      RwNode gl; gl.id = rwGEOMLIST; gl.libid = glib; gl.container = true; size_t q = p + 12; RwNode gs;
      if (!schLeaf(c, q, rwSTRUCT, gs) || gs.data.size() < 4) return false;
      uint32_t ng = rd32(gs.data.data()); if (ng > 4096) return false; gl.kids.push_back(std::move(gs));
      for (uint32_t i = 0; i < ng; i++) { RwNode g; if (!schGeometry(c, q, g)) return false; gl.kids.push_back(std::move(g)); }
      if (q != p + 12 + (size_t)gsz) c.fixes++;
      p = q; out.kids.push_back(std::move(gl)); }
    for (uint32_t i = 0; i < nAtomics; i++) {
        uint32_t aid, asz, alib; if (!schHdr(c, p, aid, asz, alib) || aid != rwATOMIC) return false;
        RwNode a; a.id = rwATOMIC; a.libid = alib; a.container = true; size_t q = p + 12; RwNode as, ae;
        if (!schLeaf(c, q, rwSTRUCT, as) || !schExtension(c, q, ae)) return false;
        a.kids.push_back(std::move(as)); a.kids.push_back(std::move(ae));
        if (q != p + 12 + (size_t)asz) c.fixes++;
        p = q; out.kids.push_back(std::move(a));
    }
    // lights / cameras (редко) — как есть, если размер сходится
    for (;;) { uint32_t xid, xsz, xlib; if (!schHdr(c, p, xid, xsz, xlib) || (xid != rwLIGHT && xid != rwCAMERA) || xsz > c.n - p - 12) break;
               RwNode x; x.id = xid; x.libid = xlib; x.container = false; x.data.assign(c.d + p + 12, c.d + p + 12 + xsz); out.kids.push_back(std::move(x)); p += 12 + xsz; }
    { RwNode e; if (!schExtension(c, p, e)) return false; out.kids.push_back(std::move(e)); }
    if (p != o + 12 + (size_t)sz) c.fixes++;
    o = p; return true;
}
// Есть ли в дереве структурный контейнер, который не удалось разобрать как контейнер (признак вранья в размерах)?
static inline bool rwTreeDamaged(const std::vector<RwNode>& nodes) {
    for (auto& n : nodes) {
        if (!n.container) { switch (n.id) { case rwCLUMP: case rwFRAMELIST: case rwGEOMLIST: case rwGEOMETRY: case rwMATLIST: case rwMATERIAL: case rwATOMIC: case rwTEXTURE: if (n.data.size() >= 12) return true; default: break; } }
        else if (rwTreeDamaged(n.kids)) return true;
    }
    return false;
}
// Пересборка потока по схеме. true — получилось (buf заменён на корректный поток), fixes — сколько размеров исправлено.
static inline bool rebuildClumpBySchema(std::vector<uint8_t>& buf, int& fixes) {
    if (buf.size() < 12) return false;
    SchemaCtx c{ buf.data(), buf.size(), 0, false }; size_t o = 0; std::vector<RwNode> roots;
    // необязательный UVAnimDict впереди — только если его размер сходится
    if (rd32(buf.data()) == rwUVANIMDICT) { uint32_t sz = rd32(buf.data() + 4); if ((uint64_t)sz + 12 > buf.size()) return false;
        RwNode u; if (!parseChildren(buf.data(), sz + 12, roots, 0) || roots.size() != 1) return false; o = sz + 12; }
    RwNode cl; if (!schClump(c, o, cl)) return false;
    roots.push_back(std::move(cl));
    std::vector<uint8_t> out; out.reserve(buf.size()); for (auto& r : roots) writeNode(r, out);
    fixes = c.fixes; buf.swap(out); return true;
}

// Нормализует DFF-поток в формат SA. Возвращает false, если это не валидный RW-поток (буфер не тронут).
static inline bool normalizeDffToSA(std::vector<uint8_t>& buf, RwFixStats& st) {
    if (buf.size() < 12) return false;
    uint32_t first = rd32(&buf[0]);
    if (first != rwCLUMP && first != rwUVANIMDICT) return false;
    std::vector<RwNode> roots;
    auto parseAll = [&](std::vector<RwNode>& r) -> bool {
        if (parseChildren(buf.data(), buf.size(), r, 0) && !(r.size() == 1 && !r[0].container)) return true;
        r.clear(); return false;
    };
    bool ok = parseAll(roots);
    // v6.7: не разобралось, или разобралось, но контейнер (Geometry/MaterialList/...) остался «листом» — размеры чанков врут;
    // пересобираем дерево по схеме RpClump (см. rebuildClumpBySchema). Если схема не подошла — идём старым путём.
    if (!ok || rwTreeDamaged(roots)) {
        std::vector<uint8_t> copy = buf; int fx = 0;
        if (rebuildClumpBySchema(copy, fx)) {
            std::vector<RwNode> r2;
            if (parseChildren(copy.data(), copy.size(), r2, 0) && !r2.empty() && !(r2.size() == 1 && !r2[0].container) && !rwTreeDamaged(r2)) { buf.swap(copy); roots.swap(r2); ok = true; st.schemaFixes = fx; }
        }
    }
    if (!ok) {
        // терпимый режим
        g_rwLenient = true; g_rwLenientFixes = 0;
        ok = parseAll(roots);
        g_rwLenient = false;
        if (ok) st.lenientFixes = g_rwLenientFixes;
    }
    if (!ok) {
        roots.clear();
        uint32_t sz = rd32(&buf[4]);
        if ((uint64_t)sz + 12 <= buf.size()) {
            // после clump лежит мусор/паддинг — ровно один корневой чанк
            if (!parseChildren(buf.data(), sz + 12, roots, 0)) roots.clear();
        }
        if (roots.empty()) {
            // размер корневого чанка неверен (встречается в BR) — разбираем детей по фактическому размеру
            RwNode root; root.id = first; root.libid = rd32(&buf[8]); root.container = true;
            bool k = parseChildren(buf.data() + 12, buf.size() - 12, root.kids, 1);
            if (!k) { root.kids.clear(); g_rwLenient = true; g_rwLenientFixes = 0; k = parseChildren(buf.data() + 12, buf.size() - 12, root.kids, 1); g_rwLenient = false; if (k) st.lenientFixes = g_rwLenientFixes; }
            if (!k || root.kids.empty()) return false;
            st.rootSizeFixed = 1;
            roots.push_back(std::move(root));
        }
    }
    // корень разобран как лист (дети не парсятся) — считаем неудачей, чтобы вызывающий увидел проблему
    if (roots.size() == 1 && !roots[0].container) return false;
    for (auto& r : roots) fixTree(r, 0, st);
    std::vector<uint8_t> out; out.reserve(buf.size());
    for (auto& r : roots) writeNode(r, out);
    buf.swap(out);
    return true;
}

// Нормализация прямо в игровом буфере (результат всегда <= исходного размера).
static inline bool normalizeDffInplace(uint8_t* buf, uint32_t& len, RwFixStats& st) {
    std::vector<uint8_t> v(buf, buf + len);
    if (!normalizeDffToSA(v, st)) return false;
    if (v.size() > len) return false;   // не должно случиться
    memcpy(buf, v.data(), v.size());
    if (v.size() < len) memset(buf + v.size(), 0, len - v.size());
    len = (uint32_t)v.size();
    return true;
}

// Нативное чтение .mod: расшифровка и починка структуры прямо в буфере файла.
// Отдельного .dff не создаётся: буфер после вызова и есть поток, который отдаётся
// движку (RW-чанк за чанком, в RAM, без файлов на диске). false = файл не понятен.
static inline bool understandModInplace(std::vector<uint8_t>& mod, RwFixStats& st, std::string& err, bool saFormat = true) {
    err.clear();
    if (!isMod(mod.data(), mod.size())) { err = "not a .mod file (bad magic)"; return false; }
    int variant = detectModKeyVariant(mod.data(), (uint32_t)mod.size());
    if (variant == -1) { err = "decrypted data is not a RW stream with any known key (" + modDiag(mod.data(), (uint32_t)mod.size()) + ")"; return false; }
    std::string warn;
    uint32_t len = decryptModInplace(mod.data(), (uint32_t)mod.size(), &warn, variant);
    if (!len) { err = "not a .mod file (bad magic)"; return false; }
    if (variant == -2) warn += (warn.empty() ? "" : "; ") + std::string("payload is not encrypted");
    else if (variant != 0) { char t[64]; snprintf(t, sizeof t, "%skey variant %d", warn.empty() ? "" : "; ", variant); warn += t; }
    if (!warn.empty()) err = "warning: " + warn;
    mod.resize(len);
    if (mod.size() < 12 || (rd32(&mod[0]) != rwCLUMP && rd32(&mod[0]) != rwUVANIMDICT)) { err = "decrypted data is not a CLUMP"; return false; }
    if (saFormat) {
        if (!normalizeDffToSA(mod, st)) {
            // не смогли разобрать дерево — оставляем как есть, но чиним размер корневого чанка
            uint32_t sz = (uint32_t)mod.size() - 12; wr32(&mod[4], sz);
            std::string d = rwTreeDiag(mod.data(), mod.size());
            err = (err.empty() ? "" : err + "; ") + "warning: RW tree parse failed (" + (d.empty() ? "unknown" : d) + "), written as-is";
        } else {
            if (st.rootSizeFixed) err = (err.empty() ? "" : err + "; ") + "root chunk size fixed";
            if (st.lenientFixes) { char t[64]; snprintf(t, sizeof t, "%d chunk size(s) clamped", st.lenientFixes); err = (err.empty() ? "" : err + "; ") + t; }
            if (st.binmeshDropped) { char t[96]; snprintf(t, sizeof t, "%d inconsistent BinMesh chunk(s) dropped (RW rebuilds meshes)", st.binmeshDropped); err = (err.empty() ? "" : err + "; ") + t; }
            if (st.schemaFixes) { char t[96]; snprintf(t, sizeof t, "tree rebuilt by schema: %d chunk size(s) corrected", st.schemaFixes); err = (err.empty() ? "" : err + "; ") + t; }
        }
    } else {
        uint32_t sz = (uint32_t)mod.size() - 12; wr32(&mod[4], sz);
    }
    return true;
}

// Совместимая обёртка для brconv/стенда: та же нативная читка, но на копии.
static inline std::vector<uint8_t> convertModToDff(const std::vector<uint8_t>& mod, RwFixStats& st, std::string& err, bool saFormat = true) {
    std::vector<uint8_t> buf = mod;
    if (!understandModInplace(buf, st, err, saFormat)) return {};
    return buf;
}

// ---------------------------------------------------------------------------
// .cls -> .col   (коллизии Black Russia)
//   Файл .cls = последовательность блоков  fourcc | u32 size | name[22] | u16 modelId | body
//   CLST: тело как COL3 SA, НО bounds записаны в порядке sphere(center xyz, radius), box(min, max);
//         в COL3 SA порядок box(min,max), sphere(center,radius) -> надо переставить, иначе SA
//         получает bbox с max<min (объекты не рисуются / коллизия не работает).
//   CLSF: bounds уже в порядке COL3, тело в формате COL1 (float-вершины, u32-индексы граней):
//         u32 nSpheres, {TSphere V1}..., u32 nLines, ..., u32 nBoxes, {TBox}..., u32 nVerts, {xyz f32}...,
//         u32 nFaces, {u32 a,b,c, surface}...  -> переупаковываем в COL3 (compressed verts, u16 faces).
//   COL3/COLL/COL2 блоки (если встретятся) копируются как есть.
// ---------------------------------------------------------------------------
static inline uint32_t clsFourcc(const uint8_t* p) { return rd32(p); }
static const uint32_t FCC_CLST = 0x54534C43, FCC_CLSF = 0x46534C43, FCC_COL3 = 0x334C4F43, FCC_COL2 = 0x324C4F43, FCC_COLL = 0x4C4C4F43;
static inline bool isClsBlockFourcc(uint32_t f) { return f == FCC_CLST || f == FCC_CLSF || f == FCC_COL3 || f == FCC_COL2 || f == FCC_COLL; }
static inline bool isClsFile(const uint8_t* p, size_t n) { return n >= 8 && (clsFourcc(p) == FCC_CLST || clsFourcc(p) == FCC_CLSF); }

struct ClsStats { int blocks = 0, clst = 0, clsf = 0, passthru = 0, bad = 0, matFixed = 0; };

// bounds: CLST -> COL3 (40 байт по смещению 32 от начала блока). Возвращает false, если после перестановки bbox всё равно невалиден.
static inline void clsSwapBounds(uint8_t* b) {   // b -> 40 байт bounds
    uint8_t sph[16], box[24];
    memcpy(sph, b, 16); memcpy(box, b + 16, 24);
    memcpy(b, box, 24); memcpy(b + 24, sph, 16);
}
static inline void clsPad4(std::vector<uint8_t>& b) { while (b.size() % 4) b.push_back(0); wr32(&b[4], (uint32_t)(b.size() - 8)); }

// CLSF (COL1-тело) -> COL3 блок
static inline bool clsfToCol3(const uint8_t* blk, size_t n, std::vector<uint8_t>& out) {
    if (n < 72) return false;
    const uint8_t* body = blk + 72; size_t bn = n - 72, off = 0;
    auto need = [&](size_t k) { return off + k <= bn; };
    if (!need(4)) return false; uint32_t nS = rd32(body + off); off += 4;
    const uint8_t* sph = body + off; if (!need((size_t)nS * 20)) return false; off += (size_t)nS * 20;
    if (!need(4)) return false; uint32_t nL = rd32(body + off); off += 4; if (!need((size_t)nL * 24)) return false; off += (size_t)nL * 24;
    if (!need(4)) return false; uint32_t nB = rd32(body + off); off += 4;
    const uint8_t* box = body + off; if (!need((size_t)nB * 28)) return false; off += (size_t)nB * 28;
    if (!need(4)) return false; uint32_t nV = rd32(body + off); off += 4;
    const uint8_t* vtx = body + off; if (!need((size_t)nV * 12)) return false; off += (size_t)nV * 12;
    if (!need(4)) return false; uint32_t nF = rd32(body + off); off += 4;
    const uint8_t* fac = body + off; if (!need((size_t)nF * 16)) return false;
    if (nV > 65535 || nF > 65535 || nS > 65535 || nB > 65535) return false;

    out.assign(blk, blk + 72);                         // fourcc, size, name, id, bounds (уже в порядке COL3)
    memcpy(&out[0], "COL3", 4);
    // V3 header: u16 nS, u16 nB, u16 nF, u8 nL, u8 pad, u32 flags, u32 offS, offB, offL, offV, offF, offPlanes, u32 nShdwF, offShdwV, offShdwF
    size_t h = out.size(); out.resize(h + 48, 0);   // V3 header без bounds = 48 байт -> данные с +120 (offset 116 + 4)
    uint32_t flags = (nS || nB || nF) ? 2 : 0;
    uint32_t cur = 116;                                // смещения в COL2/3 = позиция в блоке - 4 (от конца fourcc)
    uint32_t offS = 0, offB = 0, offV = 0, offF = 0;
    if (nS) { offS = cur; cur += nS * 20; }
    if (nB) { offB = cur; cur += nB * 28; }
    if (nV) { offV = cur; cur += nV * 6; }
    if (nF) { offF = cur; cur += nF * 8; }
    uint8_t* H = &out[h];
    H[0] = nS & 255; H[1] = nS >> 8; H[2] = nB & 255; H[3] = nB >> 8; H[4] = nF & 255; H[5] = nF >> 8; H[6] = 0; H[7] = 0;
    wr32(H + 8, flags); wr32(H + 12, offS); wr32(H + 16, offB); wr32(H + 20, 0); wr32(H + 24, offV); wr32(H + 28, offF); wr32(H + 32, 0);
    // spheres: V1 = radius, center, surface -> V2 = center, radius, surface
    for (uint32_t i = 0; i < nS; i++) { const uint8_t* s = sph + i * 20; out.insert(out.end(), s + 4, s + 16); out.insert(out.end(), s, s + 4); out.insert(out.end(), s + 16, s + 20); }
    for (uint32_t i = 0; i < nB; i++) { const uint8_t* s = box + i * 28; out.insert(out.end(), s, s + 28); }
    for (uint32_t i = 0; i < nV; i++) {
        for (int k = 0; k < 3; k++) { float f; memcpy(&f, vtx + i * 12 + k * 4, 4); float q = f * 128.f; if (q > 32767.f) q = 32767.f; if (q < -32768.f) q = -32768.f; int16_t v = (int16_t)(q < 0 ? q - 0.5f : q + 0.5f); out.push_back((uint8_t)(v & 255)); out.push_back((uint8_t)((uint16_t)v >> 8)); }
    }
    if (nV & 1) { out.push_back(0); out.push_back(0); }   // как в SA: вершины выровнены на 4
    if (nF && (nV & 1)) { wr32(out.data() + h + 28, offF + 2); }   // out мог переехать после insert/push_back — адрес берём заново
    for (uint32_t i = 0; i < nF; i++) {
        const uint8_t* f = fac + i * 16; uint32_t a = rd32(f), b = rd32(f + 4), c = rd32(f + 8);
        if (a >= nV || b >= nV || c >= nV) return false;
        uint16_t t[3] = { (uint16_t)a, (uint16_t)b, (uint16_t)c };
        out.insert(out.end(), (uint8_t*)t, (uint8_t*)t + 6); out.push_back(f[12]); out.push_back(f[15]);   // material, light
    }
    clsPad4(out);
    return true;
}

// Пост-обработка COL3-блока (после clsBlockToCol): материалы поверхностей > 178 (у BR массово 255) в SA читают мусор
// за таблицей g_surfaceInfos -> странная физика/«дырки». Заменяем на 0 (default). Возвращает число исправленных граней.
static inline int fixColBlockMaterials(uint8_t* blk, size_t n) {
    if (n < 120 || rd32(blk) != FCC_COL3) return 0;
    uint32_t nF = blk[76] | (blk[77] << 8), nS = blk[72] | (blk[73] << 8), nB = blk[74] | (blk[75] << 8);
    uint32_t offF = rd32(blk + 100), offS = rd32(blk + 92), offB = rd32(blk + 96);
    int fixed = 0;
    if (nF && offF && (size_t)offF + 4 + (size_t)nF * 8 <= n) for (uint32_t i = 0; i < nF; i++) { uint8_t* m = blk + 4 + offF + i * 8 + 6; if (*m > 178) { *m = 0; fixed++; } }
    if (nS && offS && (size_t)offS + 4 + (size_t)nS * 20 <= n) for (uint32_t i = 0; i < nS; i++) { uint8_t* m = blk + 4 + offS + i * 20 + 16; if (*m > 178) { *m = 0; fixed++; } }
    if (nB && offB && (size_t)offB + 4 + (size_t)nB * 28 <= n) for (uint32_t i = 0; i < nB; i++) { uint8_t* m = blk + 4 + offB + i * 28 + 24; if (*m > 178) { *m = 0; fixed++; } }
    return fixed;
}
// Есть ли в блоке (CLST/COL3) хоть какие-то объёмы (сферы/боксы/грани)? Пустой блок = только bbox.
static inline bool colBlockHasGeometry(const uint8_t* blk, size_t n) {
    if (n < 120) return false;
    uint32_t nS = blk[72] | (blk[73] << 8), nB = blk[74] | (blk[75] << 8), nF = blk[76] | (blk[77] << 8);
    return (nS | nB | nF) != 0;
}

// Один блок .cls -> COL3 блок. Пустой вектор при ошибке.
static inline bool clsBlockToCol(const uint8_t* blk, size_t n, std::vector<uint8_t>& out, ClsStats& st) {
    uint32_t f = clsFourcc(blk);
    if (f == FCC_CLST) {
        if (n < 72) { st.bad++; return false; }
        out.assign(blk, blk + n); memcpy(&out[0], "COL3", 4); clsSwapBounds(&out[32]); clsPad4(out); st.matFixed += fixColBlockMaterials(out.data(), out.size()); st.clst++; return true;
    }
    if (f == FCC_CLSF) { if (!clsfToCol3(blk, n, out)) { st.bad++; return false; } st.clsf++; return true; }
    out.assign(blk, blk + n); clsPad4(out); st.matFixed += fixColBlockMaterials(out.data(), out.size()); st.passthru++; return true;
}

// Перебор блоков (терпимо к мусору между блоками и к обрезанному хвосту)
template <class Fn> static inline void clsForEachBlock(const uint8_t* d, size_t n, Fn fn) {
    size_t idx = 0;
    while (idx + 8 <= n) {
        size_t pos = std::string::npos;
        for (size_t i = idx; i + 8 <= n; i++) if (isClsBlockFourcc(rd32(d + i))) { pos = i; break; }
        if (pos == std::string::npos) break;
        uint32_t len = rd32(d + pos + 4); size_t end = pos + 8 + (size_t)len; if (end > n || len < 24) end = n;
        fn(d + pos, end - pos);
        idx = end;
    }
}

static inline std::vector<uint8_t> convertClsToCol(const std::vector<uint8_t>& cls, ClsStats& st) {
    std::vector<uint8_t> out, blk;
    clsForEachBlock(cls.data(), cls.size(), [&](const uint8_t* b, size_t n) { st.blocks++; if (clsBlockToCol(b, n, blk, st)) out.insert(out.end(), blk.begin(), blk.end()); });
    return out;
}
static inline std::vector<uint8_t> convertClsToCol(const std::vector<uint8_t>& cls, int& blocks) { ClsStats st; auto r = convertClsToCol(cls, st); blocks = st.blocks; return r; }

// Имена моделей в .cls (для индекса VFS)
static inline void clsModelNames(const uint8_t* d, size_t n, std::vector<std::string>& names) {
    clsForEachBlock(d, n, [&](const uint8_t* b, size_t bn) { if (bn >= 32) names.emplace_back((const char*)b + 8, strnlen((const char*)b + 8, 22)); });
}

// In-place вариант для IMG-хука (размер блока не меняется: CLST -> COL3 + перестановка bounds; CLSF пропускаем)
static inline int fixClsInplace(uint8_t* buf, uint32_t n) {
    uint32_t pos = 0; int fixed = 0;
    while (pos + 8 <= n) {
        uint32_t fourcc = rd32(buf + pos);
        if (fourcc == FCC_CLST) { memcpy(buf + pos, "COL3", 4); if (pos + 72 <= n) clsSwapBounds(buf + pos + 32); fixed++; }
        if (rd32(buf + pos) == FCC_COL3) { uint32_t l = rd32(buf + pos + 4); if (l && pos + 8 + (size_t)l <= n) fixColBlockMaterials(buf + pos, 8 + l); }
        else if (fourcc != FCC_COL3 && fourcc != FCC_COLL && fourcc != FCC_COL2 && fourcc != FCC_CLSF) break;
        uint32_t len = rd32(buf + pos + 4); if (!len) break;
        pos += 8 + len;
    }
    return fixed;
}

// ---------------------------------------------------------------------------
// Габариты модели из RW-геометрии (для коллизии-заглушки: bbox без объёмов)
//   Обходим CLUMP -> GEOMLIST -> GEOMETRY -> STRUCT; берём вершины первого morph target'а
//   (или bounding sphere, если вершин нет). Трансформации фреймов не учитываем (у карт они единичные).
// ---------------------------------------------------------------------------
struct GeomBounds { float mn[3] = { 1e9f, 1e9f, 1e9f }, mx[3] = { -1e9f, -1e9f, -1e9f }; int geoms = 0; bool valid() const { return geoms > 0 && mn[0] <= mx[0]; } };
static inline void geomBoundsAddPoint(GeomBounds& gb, const float* v) { for (int k = 0; k < 3; k++) { if (v[k] < gb.mn[k]) gb.mn[k] = v[k]; if (v[k] > gb.mx[k]) gb.mx[k] = v[k]; } }
static inline void geomBoundsFromStruct(const uint8_t* d, size_t n, GeomBounds& gb) {
    if (n < 16) return;
    uint32_t flags = rd32(d), numTris = rd32(d + 4), numVerts = rd32(d + 8), numMorph = rd32(d + 12);
    bool withF = geometryStructSize(d, n, true) == n; bool noF = geometryStructSize(d, n, false) == n;
    if (!withF && !noF) return;
    size_t off = 16 + (withF ? 12 : 0);
    if (!(flags & 0x01000000)) {
        uint32_t numTex = (flags >> 16) & 0xFF; if (numTex == 0) numTex = (flags & 0x04 ? 1 : 0) + (flags & 0x80 ? 2 : 0);
        if (flags & 0x08) off += (size_t)numVerts * 4;
        off += (size_t)numTex * numVerts * 8 + (size_t)numTris * 8;
    }
    if (!numMorph || off + 24 > n) return;
    float sph[4]; memcpy(sph, d + off, 16); uint32_t hasV = rd32(d + off + 16); off += 24;
    bool any = false;
    if (hasV && !(flags & 0x01000000) && off + (size_t)numVerts * 12 <= n) {
        for (uint32_t i = 0; i < numVerts; i++) { float v[3]; memcpy(v, d + off + i * 12, 12); if (v[0] == v[0] && v[1] == v[1] && v[2] == v[2] && fabsf(v[0]) < 1e5f && fabsf(v[1]) < 1e5f && fabsf(v[2]) < 1e5f) { geomBoundsAddPoint(gb, v); any = true; } }
    }
    if (!any && sph[3] > 0 && sph[3] < 1e5f) { float a[3] = { sph[0] - sph[3], sph[1] - sph[3], sph[2] - sph[3] }, b[3] = { sph[0] + sph[3], sph[1] + sph[3], sph[2] + sph[3] }; geomBoundsAddPoint(gb, a); geomBoundsAddPoint(gb, b); any = true; }
    if (any) gb.geoms++;
}
static inline void collectGeometryBounds(const uint8_t* p, size_t n, GeomBounds& gb, int depth = 0) {
    size_t off = 0;
    while (off + 12 <= n) {
        uint32_t id = rd32(p + off), sz = rd32(p + off + 4);
        if (sz > n - off - 12) sz = (uint32_t)(n - off - 12);
        const uint8_t* body = p + off + 12;
        if (id == rwGEOMETRY && sz >= 12 + 16 && rd32(body) == rwSTRUCT) { uint32_t ssz = rd32(body + 4); if (ssz > sz - 12) ssz = sz - 12; geomBoundsFromStruct(body + 12, ssz, gb); }
        else if (depth < 16 && (id == rwCLUMP || id == rwGEOMLIST)) collectGeometryBounds(body, sz, gb, depth + 1);
        off += 12 + sz;
    }
}

// ---------------------------------------------------------------------------
// Строгая проверка готового (нормализованного) DFF: сможет ли его прочитать RpClumpStreamRead SA.
// Возвращает пустую строку, если модель в норме, иначе — причину. Такие .mod плагин не публикует вовсе:
// игра бы приняла файл, RW отверг бы его при чтении, и модель осталась бы без RwClump (у педа/игрока это
// падение 0x749B7B, у объекта карты — невидимый объект и мусор в стриминге).
// ---------------------------------------------------------------------------
static const uint32_t rwSKIN_PLG = 0x116, rwHANIM_PLG = 0x11E;
static const uint32_t rwBINMESH_PLG = 0x50E, rwATOMIC_ID = 0x14;
struct DffCheck { int atomics = 0, geoms = 0, frames = 0, badGeom = 0, skin = 0, emptyGeom = 0, hugeGeom = 0; int badMat = 0, badBin = 0, badSkin = 0, badAtomic = 0; char why[160] = {0}; };
// v6.6.1: глубокая проверка геометрии: индексы материалов треугольников < numMaterials, индексы BinMesh < numVerts и
// materialIndex сплитов < numMaterials, у скина numBones <= 64 и все индексы костей < numBones, атомик ссылается на
// существующие frame/geometry. Такие ошибки RW читает без жалоб, а D3D9-пайплайн SA (0x754AE0..0x7580B3, сборка индексного
// буфера / vertex declaration) падает на них: EIP 0x754B1C, ESI = адрес за концом буфера индексов.
static inline bool geometryDeepCheck(const RwNode& geo, DffCheck& c) {
    const uint8_t* d = nullptr; size_t n = 0;
    for (auto& g : geo.kids) if (g.id == rwSTRUCT && !g.container) { d = g.data.data(); n = g.data.size(); break; }
    if (!d || n < 16) return true;
    uint32_t flags = rd32(d), numTris = rd32(d + 4), numVerts = rd32(d + 8);
    uint32_t numMat = 0;
    for (auto& g : geo.kids) if (g.id == rwMATLIST && g.container && !g.kids.empty() && g.kids[0].id == rwSTRUCT && g.kids[0].data.size() >= 4) numMat = rd32(g.kids[0].data.data());
    if (!(flags & 0x01000000) && numMat) {
        size_t off = 16; uint32_t numTex = (flags >> 16) & 0xFF; if (numTex == 0) numTex = (flags & 0x04 ? 1 : 0) + (flags & 0x80 ? 2 : 0);
        if (flags & 0x08) off += (size_t)numVerts * 4; off += (size_t)numTex * numVerts * 8;
        if (off + (size_t)numTris * 8 <= n) for (uint32_t t = 0; t < numTris; t++) {
            const uint8_t* tr = d + off + (size_t)t * 8; uint16_t v2 = rd16(tr), v1 = rd16(tr + 2), mat = rd16(tr + 4), v3 = rd16(tr + 6);
            if (mat >= numMat || v1 >= numVerts || v2 >= numVerts || v3 >= numVerts) { c.badMat++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "triangle %u: material %u/%u verts %u,%u,%u/%u", t, mat, numMat, v1, v2, v3, numVerts); return false; }
        }
    }
    for (auto& g : geo.kids) if (g.id == rwEXTENSION && g.container) for (auto& e : g.kids) {
        if (e.id == rwBINMESH_PLG && !e.container && e.data.size() >= 12) {
            const uint8_t* b = e.data.data(); size_t bn = e.data.size(); uint32_t nSplit = rd32(b + 4), total = rd32(b + 8); size_t o = 12; uint32_t sum = 0;
            for (uint32_t sI = 0; sI < nSplit; sI++) {
                if (o + 8 > bn) { c.badBin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "binmesh truncated at split %u", sI); return false; }
                uint32_t cnt = rd32(b + o), mi = rd32(b + o + 4); o += 8;
                if (numMat && mi >= numMat) { c.badBin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "binmesh split %u: material %u/%u", sI, mi, numMat); return false; }
                if (o + (size_t)cnt * 4 > bn) { c.badBin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "binmesh split %u: %u indices exceed chunk", sI, cnt); return false; }
                for (uint32_t k = 0; k < cnt; k++) { uint32_t ix = rd32(b + o + (size_t)k * 4); if (ix >= numVerts) { c.badBin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "binmesh split %u index %u/%u", sI, ix, numVerts); return false; } }
                o += (size_t)cnt * 4; sum += cnt;
            }
            (void)total;
        } else if (e.id == rwSKIN_PLG && !e.container && e.data.size() >= 4) {
            const uint8_t* sk = e.data.data(); size_t sn = e.data.size(); uint32_t numBones = sk[0], numUsed = sk[1];
            if (numBones == 0 || numBones > 64 || numUsed > numBones) { c.badSkin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "skin: %u bones, %u used", numBones, numUsed); return false; }
            size_t o = 4 + numUsed; if (o + (size_t)numVerts * 20 > sn) { c.badSkin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "skin data truncated"); return false; }
            for (uint32_t v = 0; v < numVerts; v++) for (int k = 0; k < 4; k++) if (sk[o + (size_t)v * 4 + k] >= numBones) { c.badSkin++; if (!c.why[0]) snprintf(c.why, sizeof c.why, "skin: vertex %u bone %u/%u", v, sk[o + (size_t)v * 4 + k], numBones); return false; }
        }
    }
    return true;
}
static inline void dffCheckWalk(const std::vector<RwNode>& kids, DffCheck& c, int depth) {
    for (auto& k : kids) {
        if (k.id == rwATOMIC && k.container) c.atomics++;
        if (k.id == rwFRAMELIST && k.container && !k.kids.empty() && k.kids[0].id == rwSTRUCT && k.kids[0].data.size() >= 4) c.frames += (int)rd32(k.kids[0].data.data());
        if (k.id == rwGEOMETRY && k.container) {
            c.geoms++;
            bool ok = false;
            for (auto& g : k.kids) {
                if (g.id == rwSTRUCT && !g.container) {
                    const uint8_t* d = g.data.data(); size_t n = g.data.size();
                    if (n >= 16) {
                        uint32_t numTris = rd32(d + 4), numVerts = rd32(d + 8), numMorph = rd32(d + 12);
                        if (numVerts == 0 || numTris == 0) c.emptyGeom++;          // пустая геометрия RW читает нормально — не ошибка
                        if (numVerts > 200000 || numTris > 400000 || numMorph == 0 || numMorph > 8) c.hugeGeom++;
                        else ok = geometryStructSize(d, n, false) == n || geometryStructSize(d, n, true) == n;
                    }
                    break;
                }
            }
            for (auto& g : k.kids) if (g.id == rwEXTENSION && g.container) for (auto& e : g.kids) if (e.id == rwSKIN_PLG) { c.skin++; break; }
            if (ok) ok = geometryDeepCheck(k, c);
            if (!ok) c.badGeom++;
            continue;
        }
        if (k.container && depth < 32) dffCheckWalk(k.kids, c, depth + 1);
    }
}
static inline std::string validateDff(const std::vector<uint8_t>& dff, bool allowSkin = true) {
    if (dff.size() < 12 + 12) return "too small";
    if (rd32(&dff[0]) != rwCLUMP) return rd32(&dff[0]) == rwUVANIMDICT ? "" : "root is not a CLUMP";
    uint32_t sz = rd32(&dff[4]); if ((uint64_t)sz + 12 > dff.size()) return "root chunk size exceeds file";
    std::vector<RwNode> roots;
    if (!parseChildren(dff.data(), dff.size(), roots, 0) || roots.empty() || !roots[0].container) return "RW tree does not parse";
    DffCheck c; dffCheckWalk(roots[0].kids, c, 0);
    char t[160];
    if (c.atomics == 0) return "no atomics (nothing to render)";
    if (c.geoms == 0) return "no geometry";
    if (c.frames == 0) return "no frames";
    if (c.badGeom) { if (c.badMat || c.badBin || c.badSkin) { snprintf(t, sizeof t, "geometry indices out of range (%s) - would crash the D3D9 pipeline (EIP 0x754B1C)", c.why); return t; }
                     snprintf(t, sizeof t, "%d of %d geometry struct(s) malformed (%d empty, %d oversized)", c.badGeom, c.geoms, c.emptyGeom, c.hugeGeom); return t; }
    if (c.skin && !allowSkin) return "skinned model (ped skin) - not a map object";
    return "";
}

// Блок COL3 "только bbox" (120 байт): имя модели, границы, без объёмов (flags=0 -> SA выключает коллизию у объекта, но габариты для рендера/секторов есть)
static inline void appendBoundsColBlock(std::vector<uint8_t>& out, const std::string& name, const float mn[3], const float mx[3]) {
    size_t h = out.size(); out.resize(h + 120, 0); uint8_t* b = &out[h];
    memcpy(b, "COL3", 4); wr32(b + 4, 112);
    memcpy(b + 8, name.c_str(), name.size() < 22 ? name.size() : 22);
    float c[3], r = 0; for (int k = 0; k < 3; k++) c[k] = (mn[k] + mx[k]) * 0.5f;
    for (int k = 0; k < 3; k++) r += (mx[k] - c[k]) * (mx[k] - c[k]); r = sqrtf(r);
    memcpy(b + 32, mn, 12); memcpy(b + 44, mx, 12); memcpy(b + 56, c, 12); memcpy(b + 68, &r, 4);
}

// ---------------------------------------------------------------------------
// .ani -> .ifp
//   SA IFP : "ANP3" | size(4) | name[0x18] | body...
//   BR .ani: "ANP3" | name[0x1C] @0x04 | size(4) @0x20 | 4 байта мусора @0x24 | body...
//   Преобразование трогает только первые 0x24 байт; хвост (с 0x28) остаётся на месте.
// ---------------------------------------------------------------------------
static inline bool isBrAni(const uint8_t* d, size_t n) {
    // SA IFP : @0x04 лежит бинарный размер (< 16 МБ => d[7]==0, d[6] мал)
    // BR .ani: @0x04 лежит ASCII-имя анимации. 4 печатных символа подряд в позиции размера
    //          означали бы размер >= 0x20202020 (538 МБ) — невозможно. Это и есть признак.
    if (n < 0x28 || (memcmp(d, "ANP3", 4) && memcmp(d, "ANPK", 4))) return false;
    for (int i = 4; i < 8; i++) if (d[i] < 0x20 || d[i] >= 0x7F) return false;
    return rd32(d + 0x20) < 64u * 1024 * 1024;          // size на своём (BR) месте
}

static inline bool fixAniInplace(uint8_t* buf, uint32_t n) {
    if (!isBrAni(buf, n)) return false;
    uint8_t sz[4]; memcpy(sz, buf + 0x20, 4);
    memmove(buf + 8, buf + 4, 0x1C);   // имя: 0x04..0x20 -> 0x08..0x24 (затирает мусор @0x24? нет: 0x08+0x1C = 0x24, ровно до мусора)
    memcpy(buf + 4, sz, 4);            // размер на своё место
    return true;
}

static inline bool convertAniToIfp(std::vector<uint8_t>& d) {
    if (!isBrAni(d.data(), d.size())) return false;
    return fixAniInplace(d.data(), (uint32_t)d.size());
}

// Верхняя граница размера данных в буфере стриминга, вычисленная из самого содержимого
//   (не зависит от CStreaming::ms_aInfoForModel — таблица переезжает при fastman92 Limit Adjuster)
static inline uint32_t guessBufferSize(const uint8_t* p) {
    if (rd32(p) == MOD_MAGIC) return MOD_HDR + rd32(p + 8) * MOD_BLOCK;
    return 0;
}


// v6.4.1: минимальный валидный DFF (RW 3.6.0.3): 1 фрейм, 1 геометрия (1 вырожденный треугольник в нуле), 1 материал без
// текстуры, 1 атомик. Подставляется вместо модели, которую RenderWare SA раз за разом отвергает (стриминг иначе зацикливается).
inline std::vector<uint8_t> buildDummyDff() {
    struct W {
        std::vector<uint8_t> b; std::vector<size_t> st;
        void u8(uint8_t v) { b.push_back(v); }
        void u16(uint16_t v) { for (int i = 0; i < 2; i++) b.push_back((uint8_t)(v >> (8 * i))); }
        void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i))); }
        void f32(float v) { uint32_t u; memcpy(&u, &v, 4); u32(u); }
        void open(uint32_t type) { u32(type); u32(0); u32(0x1803FFFF); st.push_back(b.size()); }
        void close() { size_t s = st.back(); st.pop_back(); uint32_t len = (uint32_t)(b.size() - s); memcpy(&b[s - 8], &len, 4); }
    } w;
    w.open(0x10);                                   // Clump
      w.open(0x01); w.u32(1); w.u32(0); w.u32(0); w.close();
      w.open(0x0E);                                 // Frame List
        w.open(0x01); w.u32(1);
          w.f32(1); w.f32(0); w.f32(0);  w.f32(0); w.f32(1); w.f32(0);  w.f32(0); w.f32(0); w.f32(1);
          w.f32(0); w.f32(0); w.f32(0);  w.u32(0xFFFFFFFF); w.u32(0);
        w.close();
        w.open(0x03); w.close();                    // frame extension (пустое)
      w.close();
      w.open(0x1A);                                 // Geometry List
        w.open(0x01); w.u32(1); w.close();
        w.open(0x0F);                               // Geometry
          w.open(0x01);
            w.u32(0x00000002);                      // flags: rpGEOMETRYPOSITIONS, 0 UV, native 0
            w.u32(1); w.u32(3); w.u32(1);           // triangles, vertices, morph targets
            w.u16(1); w.u16(0); w.u16(0); w.u16(2); // triangle: v2 v1 matId v3
            w.f32(0); w.f32(0); w.f32(0); w.f32(0.05f); w.u32(1); w.u32(0);   // sphere, hasVertices, hasNormals
            for (int i = 0; i < 3; i++) { w.f32(0); w.f32(0); w.f32(0); }
          w.close();
          w.open(0x08);                             // Material List
            w.open(0x01); w.u32(1); w.u32(0xFFFFFFFF); w.close();
            w.open(0x07);                           // Material
              w.open(0x01); w.u32(0); w.u8(255); w.u8(255); w.u8(255); w.u8(255); w.u32(0); w.u32(0); w.f32(1); w.f32(1); w.f32(1); w.close();
              w.open(0x03); w.close();
            w.close();
          w.close();
          w.open(0x03); w.close();
        w.close();
      w.close();
      w.open(0x14);                                 // Atomic
        w.open(0x01); w.u32(0); w.u32(0); w.u32(5); w.u32(0); w.close();
        w.open(0x03); w.close();
      w.close();
      w.open(0x03); w.close();
    w.close();
    return w.b;
}
// ---------------------------------------------------------------------------
// ANP2/ANP3 animation packages (San Andreas / Black Russia)
//
// The PC packages start [magic][size][blockName 24][numAnims]...; the SA
// mobile ones (what the Black Russia common.zip carries) have no root size
// and one extra framesAllocSize per block: [magic][blockName 24][numAnims]
// [framesAllocSize].... This walks either variant and collects the animation
// names, so the game can tell whether a custom package really carries the
// animation set it needs - without building any game objects first.
// ---------------------------------------------------------------------------

inline uint32 SniffU32(const uint8 *p) { return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24); }

// outNames receives up to maxNames animation names. Returns false when the
// buffer is not a well-formed ANP2/ANP3 package (ANPK and anything else
// returns false as well - the caller decides what that means for it).
inline bool
SniffAnimNames(const uint8 *data, size_t size, char (*outNames)[24], int maxNames, int &numNames)
{
    numNames = 0;
    if(data == nil || size < 40)
        return false;
    bool isANP3 = memcmp(data, "ANP3", 4) == 0;
    if(!isANP3 && memcmp(data, "ANP2", 4) != 0)
        return false;

    // PC layout starts [magic][size]; the size-less (SA mobile) layout fails
    // that sanity check because its "size" field is really the first four
    // bytes of the block name
    bool mobile = SniffU32(data + 4) > size;
    uint32 pos = mobile ? 4 : 8;

    size_t n = size;
    while(pos + 32 <= n){
        // block: name[24], numAnims, (mobile: framesAllocSize)
        uint32 numAnims = SniffU32(data + pos + 24);
        pos += 28;
        if(mobile)
            pos += 4;
        if(numAnims > 10000)
            return false;
        for(uint32 a = 0; a < numAnims; a++){
            if(pos + 32 > n)
                return false;
            uint32 numSeq = SniffU32(data + pos + 24);
            uint32 animNameOff = pos;
            pos += 28;
            if(isANP3)
                pos += 8;       // framesAllocSize + flags
            if(numSeq == 0 || numSeq > 4096)
                return false;
            if(numNames < maxNames){
                memcpy(outNames[numNames], data + animNameOff, 23);
                outNames[numNames][23] = '\0';
                numNames++;
            }
            for(uint32 sq = 0; sq < numSeq; sq++){
                if(pos + 36 > n)
                    return false;
                uint32 frameType = SniffU32(data + pos + 24);
                uint32 numFrames = SniffU32(data + pos + 28);
                pos += 36;
                uint32 fsz;
                switch(frameType){
                case 1: fsz = 0x14; break;
                case 2: fsz = 0x20; break;
                case 3: fsz = 0x0A; break;
                case 4: fsz = 0x10; break;
                default: return false;
                }
                if(fsz * (uint64)numFrames > n - pos)
                    return false;
                pos += fsz * numFrames;
            }
        }
    }
    return true;
}

} // namespace br
