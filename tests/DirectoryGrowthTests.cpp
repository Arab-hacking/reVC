// Exercise the actual IMG extra-objects directory with more native DFFs
// than its original 256-slot reserve. No game assets or RenderWare runtime.
#include "common.h"
#include "Directory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void re3_assert(const char *expr, const char *, unsigned int, const char *)
{
    std::fprintf(stderr, "Directory test assertion failed: %s\n", expr);
    std::abort();
}
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}

int main()
{
    CDirectory extra(EXTRADIRSIZE);
    // With [World] LoadViceCityMap=0 the native IMG contains many DFFs
    // without a registered IDE, but scripts can still request them by name.
    for(int i = 0; i < 1400; i++){
        CDirectory::DirectoryInfo entry = {};
        std::snprintf(entry.name, sizeof(entry.name), "native%04d", i);
        entry.offset = (uint32)(0x1000 + i);
        entry.size = (uint32)(i + 1);
        extra.AddItem(entry, 0);
    }
    assert(extra.numEntries == 1400 && extra.maxEntries >= extra.numEntries);
    for(int i = 0; i < 1400; i++){
        char name[24];
        std::snprintf(name, sizeof(name), "NATIVE%04d", i);
        uint32 offset = 0, size = 0;
        assert(extra.FindItem(name, offset, size));
        assert(offset == (uint32)(0x1000 + i) && size == (uint32)(i + 1));
    }

    CDirectory::DirectoryInfo duplicate = {};
    std::strcpy(duplicate.name, "NaTiVe0000");
    duplicate.offset = 123;
    duplicate.size = 456;
    extra.AddItem(duplicate, 7);
    uint32 offset = 0, size = 0;
    assert(extra.numEntries == 1400 && extra.FindItem("native0000", offset, size));
    assert(offset == 0x1000 && size == 1); // first IMG takes precedence

    CDirectory::DirectoryInfo another = {};
    std::strcpy(another.name, "special");
    another.offset = 0x4321;
    another.size = 42;
    extra.AddItem(another, 7);
    assert(extra.numEntries == 1401 && extra.FindItem("SPECIAL", offset, size));
    assert(offset == ((uint32)7 << 24 | 0x4321) && size == 42);

    CDirectory initiallyEmpty(0);
    initiallyEmpty.AddItem(another);
    assert(initiallyEmpty.numEntries == 1 && initiallyEmpty.FindItem("special", offset, size));
    assert(offset == 0x4321);
    std::puts("CDirectory: native IMG overflow, duplicate priority and special IMG offsets OK");
    return 0;
}
