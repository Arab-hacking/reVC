#ifndef REVC_MODEL_NAME_INDEX_H
#define REVC_MODEL_NAME_INDEX_H

// A short-lived lookup for IMG .dir indexing. At this point all IDE model
// names are registered; the original per-DFF CModelInfo name scan is quadratic
// with a large common/ map. Keep the lowest model ID on duplicate names, just
// as the original ascending scan did. No DFF/TXD bytes or decoded assets are
// cached here. Rebuild for each directory so later script models remain valid.
#include <cstddef>
#include <string>
#include <unordered_map>

namespace customimg {
class ModelNameIndex {
    std::unordered_map<std::string, int> names;
    size_t modelNameCapacity;

    static bool Key(const char *name, size_t capacity, std::string &out) {
        out.clear();
        if(!name) return false;
        for(size_t i = 0; i < capacity; i++) {
            unsigned char c = (unsigned char)name[i];
            if(!c) return !out.empty();
            // A rare non-ASCII stock name continues through the old lookup;
            // our case fold must not change how the game compares it.
            if(c >= 128) return false;
            out += c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : (char)c;
        }
        // A name without a terminator in its allocated model-info buffer
        // cannot be safely indexed (native loading also expects a C string).
        return false;
    }
public:
    explicit ModelNameIndex(size_t capacity) : modelNameCapacity(capacity) {}
    void Reserve(size_t count) { names.reserve(count); }
    void Add(const char *name, int id) {
        std::string key;
        if(id >= 0 && Key(name, modelNameCapacity, key)) names.emplace(key, id);
    }
    // -1 means either absent or not safely representable in the index.
    int Find(const char *name) const {
        std::string key;
        if(!Key(name, 24, key)) return -1; // IMG DIR names occupy 24 bytes
        std::unordered_map<std::string, int>::const_iterator it = names.find(key);
        return it == names.end() ? -1 : it->second;
    }
    static bool IsASCII(const char *name) {
        std::string unused;
        return Key(name, 24, unused);
    }
};
} // namespace customimg
#endif
