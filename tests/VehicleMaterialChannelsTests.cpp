#include "VehicleMaterialClassify.h"
#include <cassert>
#include <cstdio>
int main()
{
    const unsigned char markers[][3] = {{60,255,0},{255,0,175},{0,255,255},{255,0,255}};
    for(int i = 0; i < 4; ++i){
        int slot = 0;
        assert(customveh::IsEditableCarColorMarker(markers[i][0], markers[i][1], markers[i][2], slot));
        assert(slot == i + 1);
    }
    int slot = 7;
    assert(!customveh::IsEditableCarColorMarker(255,255,255,slot) && slot == 0);
    assert(customveh::IsBRBodyRemap("remapvehicles"));
    assert(customveh::IsBRBodyRemap("remapvehiclessnow"));
    const char *nonPaint[] = {nullptr, "", "rem", "remaptoner", "remaptonersnow", "plate_remap", "platem_remap", "custom_toner", "vehiclelights128"};
    for(const char *name : nonPaint){
        assert(!customveh::IsBRBodyRemap(name));
        assert(!customveh::IsLikelyBodyShellPaint("chassis",name,255));
    }
    assert(customveh::IsLikelyBodyShellPaint("chassis", "remapvehicles",255));
    std::puts("VehicleMaterialChannelsTests: ALL PASSED");
}
