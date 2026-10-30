#ifndef REVC_COMMON_CATALOG_H
#define REVC_COMMON_CATALOG_H

// The unpacked common/data/config.dat is a manifest, not a reVC level DAT.
// In particular its CDIMAGE records point at .bpc containers which are NOT
// mounted or decoded by this reader. Keeping this parser independent of the
// game lets us reject unsafe paths and test active/commented IPL selection.
#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace commoncatalog {

inline std::string Path(const std::string &raw)
{
    if(raw.empty() || raw[0] == '/' || raw[0] == '\\') return std::string();
    std::string out, part;
    for(size_t i = 0; i <= raw.size(); i++) {
        unsigned char c = i == raw.size() ? '/' : (unsigned char)raw[i];
        if(c == '/' || c == '\\') {
            if(part == ".." || part == ".") return std::string();
            if(!part.empty()) {
                if(!out.empty()) out += '/';
                out += part;
                part.clear();
            }
        } else if(c < 33 || c > 126 || c == ':') {
            return std::string();
        } else {
            part += (char)std::tolower(c);
        }
    }
    return out;
}

struct Catalog {
    std::vector<std::string> ide, ipl;
    std::set<std::string> idePaths, iplPaths;
    size_t invalid;
    Catalog() : invalid(0) {}
};

inline Catalog Parse(const unsigned char *bytes, size_t size)
{
    Catalog result;
    if(!bytes || size > 1024 * 1024) { result.invalid++; return result; }
    size_t start = 0;
    while(start < size) {
        size_t end = start;
        while(end < size && bytes[end] != '\n') end++;
        if(end - start > 512) { result.invalid++; start = end + (end < size); continue; }
        std::string line((const char *)bytes + start, end - start);
        start = end + (end < size);
        // Some exports prepend more than one UTF-8 BOM.
        while(line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        size_t comment = line.find('#');
        if(comment != std::string::npos) line.resize(comment);
        size_t first = line.find_first_not_of(" \t\r");
        if(first == std::string::npos) continue;
        line.erase(0, first);
        size_t space = line.find_first_of(" \t\r");
        if(space == std::string::npos) continue;
        std::string directive = line.substr(0, space);
        for(size_t i = 0; i < directive.size(); i++)
            directive[i] = (char)std::toupper((unsigned char)directive[i]);
        if(directive != "IDE" && directive != "IPL") continue;
        size_t p = line.find_first_not_of(" \t\r", space);
        if(p == std::string::npos) { result.invalid++; continue; }
        size_t last = line.find_first_of(" \t\r", p);
        std::string path = Path(line.substr(p, last - p));
        const char *suffix = directive == "IDE" ? ".ide" : ".ipl";
        if(path.compare(0, 5, "data/") != 0 ||
           path.size() <= 5 || path.compare(path.size() - 4, 4, suffix) != 0 ||
           (last != std::string::npos && line.find_first_not_of(" \t\r", last) != std::string::npos)) {
            result.invalid++;
            continue;
        }
        std::set<std::string> &seen = directive == "IDE" ? result.idePaths : result.iplPaths;
        std::vector<std::string> &order = directive == "IDE" ? result.ide : result.ipl;
        if(seen.size() >= 1024) { result.invalid++; continue; }
        if(seen.insert(path).second) order.push_back(path);
    }
    return result;
}

// Do not accidentally enable audio, features, handling.cfg or physics.json
// just because they are present in a user-supplied common/ directory.
inline bool CanReadData(const std::string &path)
{
    return path == "data/config.dat" || path == "data/timecyc.json" ||
           path == "data/carcols.dat";
}

} // namespace commoncatalog
#endif
