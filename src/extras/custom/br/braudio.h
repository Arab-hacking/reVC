// braudio.h — аудио-индексы Black Russia (audio/PakFiles.dat, *.bpcmeta, BanksNum.dat)
//
// PakFiles.dat: записи по 52 байта, имя пака с NUL-дополнением (STEPS/GENERIC/VEHICLE).
// <PAK>.bpcmeta: u32 count, далее на каждый семпл: u32 offset, u32 size,
// u8 reserved, u16 nameLen, char name[nameLen] — имя вида "Bank_001/sound_001.mp3",
// offset/size адресуют сырые данные семпла внутри одноимённого .bpc пака.
// BanksNum.dat: массив u32 (служебные счётчики паков, отдаём как есть).
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace braudio {

struct Sample {
    uint32_t offset = 0;
    uint32_t size = 0;
    std::string name; // "Bank_001/sound_001.mp3"
};

struct PakMeta {
    std::vector<Sample> samples;
};

static inline uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

static inline bool parseBpcMeta(const uint8_t* p, size_t n, PakMeta& out) {
    out.samples.clear();
    if (!p || n < 4) return false;
    uint32_t count = rd32(p);
    // Минимум 11 байт заголовка на семпл; отсекаем мусорные count.
    if (count > (n - 4) / 11 + 1) return false;
    size_t off = 4;
    out.samples.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        if (off + 11 > n) return false;
        Sample s;
        s.offset = rd32(p + off);
        s.size = rd32(p + off + 4);
        uint16_t nameLen = rd16(p + off + 9);
        off += 11;
        if (nameLen == 0 || nameLen > 256 || off + nameLen > n) return false;
        s.name.assign((const char*)p + off, nameLen);
        off += nameLen;
        out.samples.push_back(s);
    }
    return true;
}

static inline bool parsePakFiles(const uint8_t* p, size_t n, std::vector<std::string>& names) {
    names.clear();
    if (!p || n == 0 || n % 52 != 0) return false;
    for (size_t off = 0; off + 52 <= n; off += 52) {
        const char* s = (const char*)p + off;
        size_t len = 0;
        while (len < 52 && s[len]) len++;
        if (len == 0 || len >= 52) continue;
        names.push_back(std::string(s, len));
    }
    return !names.empty();
}

static inline bool parseBanksNum(const uint8_t* p, size_t n, std::vector<uint32_t>& values) {
    values.clear();
    if (!p || n == 0 || n % 4 != 0) return false;
    for (size_t off = 0; off + 4 <= n; off += 4)
        values.push_back(rd32(p + off));
    return true;
}

// "Bank_007/sound_012.mp3" -> 7 / 12; -1, если префикс не распознан.
static inline int bankIndex(const std::string& name) {
    if (name.compare(0, 5, "Bank_") != 0) return -1;
    return atoi(name.c_str() + 5);
}

static inline int sampleIndex(const std::string& name) {
    size_t slash = name.find('/');
    const char* s = slash == std::string::npos ? name.c_str() : name.c_str() + slash + 1;
    if (strncmp(s, "sound_", 6) != 0) return -1;
    return atoi(s + 6);
}

} // namespace braudio
