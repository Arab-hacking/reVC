#include "BRVehicleResources.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
static bool Parse(const char *s, std::vector<brvehicle::Upgrade> &out)
{
    return brvehicle::ParseUpgrades(reinterpret_cast<const uint8_t*>(s), std::strlen(s), out);
}
int main(int argc, char **argv)
{
    using namespace brvehicle;
    std::vector<Upgrade> out;
    assert(Parse("\xEF\xBB\xBF" "79, 420, 9, chassis_dummy, 1807, exhaust_stock exhaust\r\n"
                 "90 551 6 boot 2839 null # optional stock\n91 551 14 door_lf_dummy 3501351\n", out));
    assert(out.size() == 3 && out[0].stockParts.size() == 2);
    assert(out[1].stockParts.empty() && out[2].model == 3501351);
    assert(Parse("1 551 0 boot 100 null\n1 551 0 boot 100 null\n", out) && out.size() == 1);
    const char *bad[] = {"", "0 551 43 boot 100 null", "-1 551 0 boot 100 null",
        "0 551 0 ../boot 100 null", "0 551 0 boot 2147483648 null",
        "0 551 0 boot 100 null\n0 551 1 boot 101 null", "0 551 0 boot", "0 551 0 boot 1x null"};
    for(const char *s : bad){ assert(!Parse(s,out)); assert(out.empty()); }
    std::string nul = "0 551 0 boot 100 null"; nul.push_back(0);
    assert(!ParseUpgrades(reinterpret_cast<const uint8_t*>(nul.data()),nul.size(),out));
    assert(!Parse(std::string(1024,'a').c_str(),out));
    assert(Role(nullptr) == Other && Role("rem") == Other);
    assert(Role("remapvehicles") == Palette && Role("remapvehiclessnow") == Palette);
    assert(Role("remaptoner") == Toner && Role("remaptonersnow") == Toner);
    assert(Role("plate_remap") == PlateFront && Role("platem_remap") == PlateBack);
    assert(Role("vehiclelights128") == LightsOff && Role("vehiclelightson128") == LightsOn);
    assert(Role("v_t_left") == IndicatorLeft && Role("v_t_right") == IndicatorRight);
    assert(Role("other_plate_remap") == Other);
    if(argc > 1){
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        assert(ParseUpgrades(bytes.data(), bytes.size(), out));
        std::printf("Real carmods table: %zu definitions\n", out.size());
    }
    std::puts("BRVehicleResourcesTests: ALL PASSED");
}
