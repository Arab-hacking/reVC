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
#include <map>
#include <functional>

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

// Identical instances exported into multiple IPLs need only one building;
// otherwise duplicate geometry z-fights at the same position. Compare the
// actual resolved VC model, area, position and rotation, not the BR numeric ID
// (which is reused) or an imprecisely rounded position. q and -q are the same
// rotation. Keep genuinely distinct placements and alternative scenery.
struct PlacementKey {
    int slot, interior;
    float v[7];
    PlacementKey(int modelSlot, const Instance &i) : slot(modelSlot), interior(i.interior) {
        for(int k = 0; k < 3; k++) v[k] = i.pos[k];
        int sign = 1;
        for(int k = 3; k >= 0; k--)
            if(i.quat[k] != 0.0f) { sign = i.quat[k] < 0.0f ? -1 : 1; break; }
        for(int k = 0; k < 4; k++) v[3+k] = sign * i.quat[k];
    }
    bool operator==(const PlacementKey &other) const {
        if(slot != other.slot || interior != other.interior) return false;
        for(int k = 0; k < 7; k++) if(v[k] != other.v[k]) return false;
        return true;
    }
};
struct PlacementHash {
    size_t operator()(const PlacementKey &key) const {
        size_t h = std::hash<int>()(key.slot);
        h ^= std::hash<int>()(key.interior) + 0x9e3779b9U + (h<<6) + (h>>2);
        for(int k = 0; k < 7; k++)
            h ^= std::hash<float>()(key.v[k]) + 0x9e3779b9U + (h<<6) + (h>>2);
        return h;
    }
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

// The numeric ID is not a unique key across IDE files. Keep all names so an
// IPL with a reused ID can resolve its *actual* model instead of the last IDE's
// unrelated definition. Within one name the last file still wins.
struct ModelIndex {
    std::map<std::string, Model> definitions;
    std::map<int, std::string> lastNameForId;
    size_t conflicts, sharedNames;
    ModelIndex() : conflicts(0), sharedNames(0) {}
    void Add(const Model &m) {
        std::string key = Lower(m.name);
        std::map<int, std::string>::iterator prev = lastNameForId.find(m.id);
        if(prev != lastNameForId.end() && prev->second != key) conflicts++;
        lastNameForId[m.id] = key;
        std::map<std::string, Model>::iterator same = definitions.find(key);
        if(same != definitions.end() && same->second.id != m.id) sharedNames++;
        definitions[key] = m;
    }
};

// Some submitted records visually use Cyrillic С/с in an otherwise Latin
// model name. The matching IDE/DFF (e.g. L1_r3_rdBridgC) uses Latin C. Repair
// only this observed confusable, not arbitrary UTF-8 or a lossy global recode.
inline std::string ModelName(const std::string &source)
{
    std::string out;
    for(size_t i = 0; i < source.size();) {
        if(i+1 < source.size() && (unsigned char)source[i] == 0xD0 &&
           (unsigned char)source[i+1] == 0xA1) {
            out += 'C'; i += 2;
        } else if(i+1 < source.size() && (unsigned char)source[i] == 0xD1 &&
                  (unsigned char)source[i+1] == 0x81) {
            out += 'c'; i += 2;
        } else out += source[i++];
    }
    return out;
}

inline bool Name(const std::string &s, size_t maxLen)
{
    if(s.empty() || s.size() > maxLen) return false;
    for(size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if(c >= 128 || !(std::isalnum(c) || c == '_' || c == '-' || c == '.' ||
                         c == '(' || c == ')')) return false;
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
    if(t.size() < 5 || t.size() > 10 || !Int(t[0], m.id) || m.id < 0) return false;
    m.name = ModelName(t[1]);
    if(!Name(m.name, 20)) return false; // CBaseModelInfo::m_name[21]
    m.anim.clear();
    m.timeOn = m.timeOff = -1;
    if(m.kind == Model::ANIM_OBJECT) {
        // BR: id, model, animBlock, txd, drawdist, flags
        // The anim block is not installed into VC's animation table here;
        // one supplied export even duplicates its name to 30 characters.
        // Only the model and TXD have fixed-size engine fields.
        if(t.size() != 6 || !Name(t[2], 63) || !Name(t[3], 19)) return false;
        m.anim = t[2]; m.txd = t[3];
        if(!Float(t[4], m.distance) || !UInt(t[5], m.flags)) return false;
    } else {
        if(!Name(t[2], 19)) return false; // CTxdStore::TxdDef::name[20]
        m.txd = t[2];
        size_t end = t.size();
        if(m.kind == Model::TIME_OBJECT) {
            if(end == 5) {
                // Several landfill "tobj" records have no time fields at all.
                // They can be placed as ordinary, always-visible objects; a
                // time window cannot be guessed from an incomplete export.
                m.kind = Model::OBJECT;
            } else {
                if(end < 7 || !Int(t[end-2], m.timeOn) ||
                   !Int(t[end-1], m.timeOff) ||
                   m.timeOn < 0 || m.timeOn > 24 ||
                   m.timeOff < 0 || m.timeOff > 24) return false;
                end -= 2;
            }
        }
        // SA IDE: either one draw distance, or a mesh count (1..3) followed
        // by that many draw distances. The LAST field before the time window
        // is always flags. Treating the mesh count as the draw distance made
        // 202 objects disappear at distance 1 and misread their flags.
        if(end < 5 || end > 8) return false;
        size_t count = end == 5 ? 1 : end - 5;
        size_t first = end == 5 ? 3 : 4;
        int declared;
        if(end != 5 && (!Int(t[3], declared) || declared != (int)count)) return false;
        m.distance = 0.0f;
        for(size_t k = 0; k < count; k++) {
            float d;
            if(!Float(t[first+k], d) || d <= 0.0f || d >= 100000.0f) return false;
            if(d > m.distance) m.distance = d; // VC uses one flattened atomic
        }
        if(!UInt(t[end-1], m.flags)) return false;
    }
    return m.distance > 0.0f && m.distance < 100000.0f;
}

inline bool ParseInstance(const std::vector<std::string> &t, Instance &i)
{
    if((t.size() != 10 && t.size() != 11) ||
       !Int(t[0], i.id) || i.id < 0 ||
       !Int(t[2], i.interior) || i.interior < 0 || i.interior > 127)
        return false;
    i.name = ModelName(t[1]);
    if(!Name(i.name, 20)) return false;
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
    if(t.size() == 10 || Int(t[10], i.lod)) return true;
    // Quest/race IPLs export a floating-point draw distance here (200.0 or
    // 299.0), not an index into the inst section. Their IDE already supplies
    // that distance; never turn it into a LOD link to an unrelated object.
    float drawDistance;
    return t[10].find_first_of(".eE") != std::string::npos &&
           Float(t[10], drawDistance) && drawDistance > 0.0f &&
           drawDistance < 100000.0f;
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
        size_t slash = line.find("//");
        if(slash < comment) comment = slash;
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
            // Two BRmap_Interiors records contain "2499,8149" for their Z
            // coordinate. Repair ONLY this unambiguous 12-field shape; a
            // global decimal-comma rule would corrupt ordinary CSV records.
            if(t.size() == 12 && !t[5].empty() && t[6].size() >= 1 &&
               t[6].size() <= 6 &&
               t[5].find_first_not_of("-0123456789") == std::string::npos &&
               t[6].find_first_not_of("0123456789") == std::string::npos) {
                t[5] += "." + t[6];
                t.erase(t.begin()+6);
            }
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
