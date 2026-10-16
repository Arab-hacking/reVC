#ifndef REVC_CUSTOM_WORLD_FORMAT_H
#define REVC_CUSTOM_WORLD_FORMAT_H

// BR's IDE and IPL records look like SA records, not VC's IDE/axis-angle IPL.
// Keep the text parser independent of RenderWare so malformed archive entries
// can be exercised without loading a game (and never reach VC's sscanf code).
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cctype>
#include <string>
#include <vector>

namespace customworld {

struct Model {
    enum Kind { OBJECT, TIME_OBJECT, ANIM_OBJECT } kind;
    int id;
    std::string name, txd, anim;
    float distance;
    unsigned flags;
    int timeOn, timeOff;
};

struct Instance {
    int id, interior, lod;
    std::string name;
    float pos[3], quat[4];  // x, y, z, w
};

struct Stats {
    size_t records, rejected, overlong;
    Stats() : records(0), rejected(0), overlong(0) {}
};

inline std::string Lower(const std::string &s)
{
    std::string out = s;
    for(size_t i = 0; i < out.size(); i++)
        out[i] = (char)std::tolower((unsigned char)out[i]);
    return out;
}

inline bool Name(const std::string &s, size_t maxLen)
{
    if(s.empty() || s.size() > maxLen) return false;
    for(size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if(!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

inline bool Int(const std::string &s, int &out)
{
    if(s.empty()) return false;
    char *end;
    errno = 0;
    long n = std::strtol(s.c_str(), &end, 10);
    if(errno || *end || n < INT_MIN || n > INT_MAX) return false;
    out = (int)n;
    return true;
}

inline bool UInt(const std::string &s, unsigned &out)
{
    if(s.empty() || s[0] == '-') return false;
    char *end;
    errno = 0;
    unsigned long n = std::strtoul(s.c_str(), &end,
        s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') ? 16 : 10);
    if(errno || *end || n > UINT_MAX) return false;
    out = (unsigned)n;
    return true;
}

inline bool Float(const std::string &s, float &out)
{
    if(s.empty()) return false;
    char *end;
    errno = 0;
    float n = std::strtof(s.c_str(), &end);
    if(errno || *end || !std::isfinite(n)) return false;
    out = n;
    return true;
}

inline std::vector<std::string> Tokens(const std::string &line)
{
    std::vector<std::string> out;
    size_t start = 0;
    for(size_t i = 0; i <= line.size(); i++) {
        bool sep = i == line.size() || line[i] == ',' ||
                   std::isspace((unsigned char)line[i]);
        if(!sep) continue;
        if(i > start) out.push_back(line.substr(start, i - start));
        start = i + 1;
        if(out.size() > 32) break;
    }
    return out;
}

inline bool ParseModel(const std::vector<std::string> &t, Model &m)
{
    if(t.size() < 5 || t.size() > 32 || !Int(t[0], m.id) || m.id < 0 ||
       !Name(t[1], 20)) return false; // CBaseModelInfo::m_name[21]
    m.name = t[1];
    m.anim.clear();
    m.timeOn = m.timeOff = -1;
    if(m.kind == Model::ANIM_OBJECT) {
        // BR: id, model, animBlock, txd, drawdist, flags
        if(t.size() < 6 || !Name(t[2], 19) || !Name(t[3], 19)) return false;
        m.anim = t[2]; m.txd = t[3];
        if(!Float(t[4], m.distance) || !UInt(t[5], m.flags)) return false;
    } else {
        if(!Name(t[2], 19)) return false; // CTxdStore::TxdDef::name[20]
        m.txd = t[2];
        if(!Float(t[3], m.distance) || !UInt(t[4], m.flags)) return false;
        if(m.kind == Model::TIME_OBJECT) {
            if(t.size() < 7 || !Int(t[t.size()-2], m.timeOn) ||
               !Int(t[t.size()-1], m.timeOff) ||
               m.timeOn < 0 || m.timeOn > 24 ||
               m.timeOff < 0 || m.timeOff > 24) return false;
        }
    }
    return m.distance > 0.0f && m.distance < 100000.0f;
}

inline bool ParseInstance(const std::vector<std::string> &t, Instance &i)
{
    if((t.size() != 10 && t.size() != 11) ||
       !Int(t[0], i.id) || i.id < 0 || !Name(t[1], 20) ||
       !Int(t[2], i.interior) || i.interior < 0 || i.interior > 127)
        return false;
    i.name = t[1];
    for(int k = 0; k < 3; k++)
        if(!Float(t[3+k], i.pos[k]) || std::fabs(i.pos[k]) > 100000.0f) return false;
    double norm = 0;
    for(int k = 0; k < 4; k++) {
        if(!Float(t[6+k], i.quat[k])) return false;
        norm += (double)i.quat[k] * i.quat[k];
    }
    if(norm < 0.01 || norm > 100.0) return false;
    float scale = 1.0f / std::sqrt((float)norm);
    for(int k = 0; k < 4; k++) i.quat[k] *= scale;
    i.lod = -1;
    return t.size() == 10 || Int(t[10], i.lod);
}

// CFileLoader::LoadObjectInstance rotates by -2*acos(w) around its VC axis.
// A unit BR quaternion uses +2*acos(w) around xyz, so reverse the axis.
inline void ViceAxis(const Instance &i, float axis[3], float &w)
{
    w = i.quat[3];
    float sinHalf = std::sqrt(std::fmax(0.0f, 1.0f - w*w));
    if(sinHalf < 1.0e-6f) {
        axis[0] = axis[1] = 0.0f; axis[2] = 1.0f;
    } else {
        for(int k = 0; k < 3; k++) axis[k] = -i.quat[k] / sinHalf;
    }
}

// Parse a complete file without relying on a trailing NUL, commas, or native
// line endings. A too-long line is rejected as a whole (not parsed in pieces).
template<class ModelFn, class InstanceFn>
Stats Parse(const unsigned char *data, size_t size, bool ide,
            ModelFn onModel, InstanceFn onInstance)
{
    Stats stats;
    if(!data || size > 64u*1024u*1024u) { stats.rejected++; return stats; }
    std::string section;
    for(size_t pos = 0; pos < size;) {
        size_t end = pos;
        while(end < size && data[end] != '\n') end++;
        size_t n = end - pos;
        if(n > 511 || std::memchr(data + pos, 0, n) != NULL) {
            stats.overlong++; pos = end + (end < size); continue;
        }
        std::string line((const char *)data + pos, n);
        pos = end + (end < size);
        size_t comment = line.find('#');
        if(comment != std::string::npos) line.resize(comment);
        std::vector<std::string> t = Tokens(line);
        if(t.empty()) continue;
        if(t.size() == 1) {
            std::string keyword = Lower(t[0]);
            if(keyword == "end") section.clear();
            else section = keyword; // BR files do not all use 'end' between sections
            continue;
        }
        if(ide && (section == "objs" || section == "tobj" || section == "anim")) {
            Model m;
            m.kind = section == "tobj" ? Model::TIME_OBJECT :
                     section == "anim" ? Model::ANIM_OBJECT : Model::OBJECT;
            if(ParseModel(t, m)) { onModel(m); stats.records++; }
            else stats.rejected++;
        } else if(!ide && section == "inst") {
            Instance i;
            if(ParseInstance(t, i)) { onInstance(i); stats.records++; }
            else stats.rejected++;
        }
        // Other sections (e.g. BR occl and 2dfx) do not have VC's layout.
    }
    return stats;
}

template<class Fn>
Stats ParseIde(const unsigned char *data, size_t size, Fn fn) {
    return Parse(data, size, true, fn, [](const Instance &) {});
}
template<class Fn>
Stats ParseIpl(const unsigned char *data, size_t size, Fn fn) {
    return Parse(data, size, false, [](const Model &) {}, fn);
}

} // namespace customworld
#endif
