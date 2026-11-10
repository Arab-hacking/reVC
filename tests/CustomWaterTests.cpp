#include "../src/extras/custom/CommonWater.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static std::string Vertex(float x, float y, float z, float flow = 0.0f) {
    char line[128];
    std::snprintf(line, sizeof(line), "%g %g %g %g 0 0 0", x, y, z, flow);
    return line;
}
static std::string Quad(float left, float bottom, float right, float top,
                        float sw, float nw, float se, float ne, int flag = 1,
                        float flow = 0.0f) {
    return Vertex(right, bottom, se, flow) + "  " + Vertex(right, top, ne) + "  " +
           Vertex(left, bottom, sw) + "  " + Vertex(left, top, nw) +
           (flag >= 0 ? "  " + std::to_string(flag) : "") + "\n";
}
static bool Read(const std::string &text, std::vector<commonwater::Quad> &quads,
                 commonwater::Stats &stats) {
    return commonwater::Parse((const uint8_t*)text.data(), text.size(), quads, stats);
}
static bool Equal(float a, float b) { return std::fabs(a-b) < 0.001f; }

int main() {
    std::vector<commonwater::Quad> quads;
    commonwater::Stats stats;
    // Two high river sections beyond Vice City's [-2048,2048] raster and
    // one later pond that overrides only the rectangle it intersects.
    const std::string water = "processed\r\n" +
        Quad(-3000, 2800, -2800, 3008, 0, 0, 0, 0) +
        Quad(-3000, 2800, -2900, 2900, 10, 12, 20, 22) +
        Quad(-2960, 2830, -2940, 2860, 44, 44, 44, 44);
    assert(Read(water, quads, stats));
    assert(stats.accepted == 3 && stats.invalid == 0 && stats.unsupported == 0);
    float z = -123, dx = 0, dy = 0;
    assert(commonwater::Sample(quads, -2850, 2950, z) && Equal(z, 0));
    assert(commonwater::Sample(quads, -2925, 2875, z, &dx, &dy));
    assert(Equal(z, 19.0f) && Equal(dx, 0.1f) && Equal(dy, 0.02f));
    assert(commonwater::Sample(quads, -2950, 2840, z) && Equal(z, 44));
    assert(!commonwater::Sample(quads, -2801, 2700, z));
    assert(Equal(commonwater::DistanceToWater(quads, -3100, 2850), 77));
    assert(Equal(commonwater::DistanceToWater(quads, -2950, 2840), 0));

    // Unsupported flow, flags and non-coplanar water are skipped; a corrupt
    // row is rejected without using uninitialised coordinates or heights.
    std::string mixed = "processed\n" + Quad(0, 0, 32, 64, 1, 1, 1, 1) +
        Quad(40, 0, 72, 64, 1, 1, 1, 1, 2) +
        Quad(80, 0, 112, 64, 1, 1, 1, 1, 1, 0.5f) +
        Quad(120, 0, 152, 64, 1, 1, 1, 2) +
        "nan 0 0 0 0 0 0  2 2 2 0 0 0 0  1 1 1 0 0 0 0  2 1 1 0 0 0 0 1\n";
    assert(Read(mixed, quads, stats));
    assert(stats.accepted == 1 && stats.unsupported == 3 && stats.invalid == 1);
    assert(!commonwater::Sample(quads, 45, 5, z));
    assert(Read("processed\n" + Quad(0, 0, 32, 64, 3, 3, 3, 3, -1), quads, stats));
    assert(stats.accepted == 1 && commonwater::Sample(quads, 16, 32, z) && Equal(z, 3));
    assert(!Read("processed\n" + Quad(0, 0, 32, 64, 0, 0, 0, 0, 2), quads, stats));
    assert(quads.empty());
    assert(!Read("0 0 0 10 10\n", quads, stats)); // VC format must not be misread
    assert(quads.empty());
    assert(!Read("processed\n" + std::string(512, 'x') + "\n", quads, stats));
    assert(quads.empty() && stats.invalid == 1);
    assert(!commonwater::Parse(nullptr, 1, quads, stats));
    std::puts("SA water.dat quads: bounded parse, sloped heights, distant coords and fallback validated");
}
