#include <cassert>
#include "../src/core/NativeMapOptions.h"

int main()
{
    assert(IsNativeVCMapFile("DATA\\MAPS\\DOWNTOWN\\DOWNTOWN.IPL "));
    assert(IsNativeVCMapFile("data/maps/generic.ide"));
    assert(IsNativeVCMapFile(".\\DaTa\\MaPs\\nested\\foo.Ide"));
    assert(IsNativeVCMapFile("/DATA/MAPS/PATHS.IPL"));
    assert(!IsNativeVCMapFile("DATA\\DEFAULT.DAT"));
    assert(!IsNativeVCMapFile("DATA\\OCCLU.IPL"));
    assert(!IsNativeVCMapFile("DATA\\MAP.ZON"));
    assert(!IsNativeVCMapFile("CUSTOM\\DATA\\MAPS\\myworld.IPL"));
    assert(!IsNativeVCMapFile("DATA\\MAPS\\myworld.ide.bak"));
    assert(!IsNativeVCMapFile("DATA\\MAPS\\myworld.i"));
    assert(!IsNativeVCMapFile("DATA\\MAPS\\"));
    assert(!IsNativeVCMapFile(nullptr));
}
