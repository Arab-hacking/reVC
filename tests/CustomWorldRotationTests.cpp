// Exercise the actual librw axis/angle matrix used by VC's IPL loader, with
// IDE/IPL lines from the supplied BR map. SA's loader conjugates IPL quats;
// VC already negates the angle, so the adapter must NOT also negate the axis.
#include "../src/extras/custom/CustomWorldFormat.h"
#include <rw.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static bool Near(float a, float b, float tolerance = 0.0002f)
{
    return std::fabs(a - b) < tolerance;
}

static rw::Matrix VicePlacement(const customworld::Instance &inst)
{
    float axis[3], w;
    customworld::ViceAxis(inst, axis, w);
    rw::V3d vector = rw::makeV3d(axis[0], axis[1], axis[2]);
    rw::Matrix matrix;
    // Same angle and librw operation as CFileLoader::LoadObjectInstance.
    matrix.rotate(&vector, -2.0f * std::acos(w) * (180.0f / 3.14159265358979323846f),
                  rw::COMBINEREPLACE);
    return matrix;
}

static void CheckSaRotation(const customworld::Instance &inst)
{
    const rw::Matrix placed = VicePlacement(inst);
    // GTA SA's CFileLoader conjugates the quaternion before SetRotate for
    // tilted buildings; the resulting matrix has these same RW basis columns.
    const rw::Quat conjugated = rw::makeQuat(inst.quat[3], -inst.quat[0],
                                              -inst.quat[1], -inst.quat[2]);
    rw::Matrix expected;
    expected.rotate(conjugated, rw::COMBINEREPLACE);
    assert(Near(placed.right.x, expected.right.x));
    assert(Near(placed.right.y, expected.right.y));
    assert(Near(placed.right.z, expected.right.z));
    assert(Near(placed.up.x, expected.up.x));
    assert(Near(placed.up.y, expected.up.y));
    assert(Near(placed.up.z, expected.up.z));
    assert(Near(placed.at.x, expected.at.x));
    assert(Near(placed.at.y, expected.at.y));
    assert(Near(placed.at.z, expected.at.z));
}

int main()
{
    // Definitions from BRmap_1.3.ide, PortJob.ide, BRmap_Halloween.ide;
    // placements from the corresponding Core and obstacle-course IPLs.
    const std::string ide =
        "objs\n"
        "16480, BlockCRBroads1, rip, 299, 0\n"
        "35775, 1_PJ_HangarExterA, pj, 299, 0\n"
        "100048, SM_H25_pr_column, tex, 100, 0 # Стела\n"
        "end\n";
    customworld::ModelIndex models;
    customworld::Stats stats = customworld::ParseIde(
        (const unsigned char *)ide.data(), ide.size(),
        [&models](const customworld::Model &m) { models.Add(m); });
    assert(stats.records == 3 && stats.rejected == 0);

    const std::string ipl =
        "inst\n"
        "16480 BlockCRBroads1 1 6.103752 272.181366 334.464844 0.000000 0.000000 0.707279 -0.706934 -1\n"
        "35775, 1_PJ_HangarExterA, 0, -1812.64490, 2215.11597, 12.84428, 0.00147014, -0.000727735, 0.690259, 0.723561, -1\n"
        "100048 SM_H25_pr_column 1 1.215883 57.610687 254.685196 0.502064 -0.497927 0.502064 -0.497927 -1\n"
        "end\n";
    std::vector<customworld::Instance> instances;
    stats = customworld::ParseIpl((const unsigned char *)ipl.data(), ipl.size(),
        [&instances](const customworld::Instance &inst) { instances.push_back(inst); });
    assert(stats.records == 3 && stats.rejected == 0);
    for(size_t n = 0; n < instances.size(); n++) {
        const customworld::Instance &i = instances[n];
        assert(models.definitions.at(customworld::Lower(i.name)).id == i.id);
        CheckSaRotation(i);
    }
    assert(instances[0].interior == 1 && instances[1].interior == 0);
    assert(Near(instances[1].pos[0], -1812.6449f, 0.001f));
    // With z>0 but w<0, SA's upright heading is about +90 degrees.
    rw::Matrix negativeW = VicePlacement(instances[0]);
    assert(Near(negativeW.right.x, 0.0f, 0.001f));
    assert(Near(negativeW.right.y, 1.0f, 0.001f));
    assert(Near(negativeW.up.x, -1.0f, 0.001f));
    // The event object really is tilted, not just a 2-D heading.
    rw::Matrix tilted = VicePlacement(instances[2]);
    assert(std::fabs(tilted.right.z) > 0.1f || std::fabs(tilted.up.z) > 0.1f);

    // A canonical positive-w quarter turn must follow SA's -90-degree
    // heading, and q and -q must make the same placement.
    const std::string turn =
        "inst\n7 turn 0 10 20 30 0 0 0.70710678 0.70710678 -1\nend\n";
    std::vector<customworld::Instance> canonical;
    stats = customworld::ParseIpl((const unsigned char *)turn.data(), turn.size(),
        [&canonical](const customworld::Instance &i) { canonical.push_back(i); });
    assert(stats.records == 1);
    const rw::Matrix rightTurn = VicePlacement(canonical[0]);
    assert(Near(rightTurn.right.x, 0.0f) && Near(rightTurn.right.y, -1.0f));
    assert(Near(rightTurn.up.x, 1.0f) && Near(rightTurn.up.y, 0.0f));
    customworld::Instance opposite = canonical[0];
    for(int k = 0; k < 4; k++) opposite.quat[k] = -opposite.quat[k];
    CheckSaRotation(opposite);
    const rw::Matrix sameTurn = VicePlacement(opposite);
    assert(Near(rightTurn.right.x, sameTurn.right.x));
    assert(Near(rightTurn.right.y, sameTurn.right.y));
    assert(Near(rightTurn.up.x, sameTurn.up.x));
    assert(Near(rightTurn.up.y, sameTurn.up.y));

    std::puts("BR IDE/IPL rotations match SA's conjugated quaternion in librw");
}
