#include "gtest/gtest.h"
#include "core/br/HandlingParse.h"
#include <string>

using namespace brhandling;

TEST(HandlingParse, ParseVcLine) {
    const char* line = "LANDSTAL\t1800.0\t5000.0\t0.5\t0.0\t0.0\t-0.3\t70.0\t0.65\t0.85\t0.49\t5\t290\t15.9\t55\t4\tP\t12\t0.6\t1\t33\t0.9\t0.2\t0\t0.3\t-0.22\t0.5\t0.3\t0.2\t0.3\t50000\t2000\t300002\t1\t1\t0";
    HandlingEntry entry;
    EXPECT_TRUE(HandlingParser::ParseLine(line, entry));
    EXPECT_STREQ(entry.data.name, "LANDSTAL");
    EXPECT_FLOAT_EQ(entry.data.mass, 1800.0f);
    EXPECT_FALSE(entry.isExtended);
}

TEST(HandlingParse, ParseSaExtendedLine) {
    const char* line = "BRAVURA\t955\t2190\t1.6\t0\t0\t0\t75\t0.65\t0.95\t0.55\t5\t145\t12\t148\tR\tP\t5\t0.5\t0\t30\t1\t0.1\t0\t0.35\t-0.17\t0.5\t0.5\t0.23\t0.45\t20000\t0\t1800000\t1\t1\t0\t0.0\t0.0\t0.0";
    HandlingEntry entry;
    EXPECT_TRUE(HandlingParser::ParseLine(line, entry));
    EXPECT_STREQ(entry.data.name, "BRAVURA");
    EXPECT_TRUE(entry.isExtended);
    EXPECT_FLOAT_EQ(entry.data.suspensionBiasFront, 0.0f);
    EXPECT_FLOAT_EQ(entry.data.suspensionBiasRear, 0.0f);
    EXPECT_FLOAT_EQ(entry.data.suspensionAntiDive, 0.0f);
}

TEST(HandlingParse, ParseFullFile) {
    const char* data = "LANDSTAL\t1800.0\t5000.0\t0.5\t0.0\t0.0\t-0.3\t70.0\t0.65\t0.85\t0.49\t5\t290\t15.9\t55\t4\tP\t12\t0.6\t1\t33\t0.9\t0.2\t0\t0.3\t-0.22\t0.5\t0.3\t0.2\t0.3\t50000\t2000\t300002\t1\t1\t0\n"
                       "BRAVURA\t955\t2190\t1.6\t0\t0\t0\t75\t0.65\t0.95\t0.55\t5\t145\t12\t148\tR\tP\t5\t0.5\t0\t30\t1\t0.1\t0\t0.35\t-0.17\t0.5\t0.5\t0.23\t0.45\t20000\t0\t1800000\t1\t1\t0\t0.0\t0.0\t0.0\n";
    std::string err;
    auto entries = HandlingParser::Parse(data, strlen(data), &err);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(entries.size(), 2);
    EXPECT_STREQ(entries[0].data.name, "LANDSTAL");
    EXPECT_STREQ(entries[1].data.name, "BRAVURA");
    EXPECT_FALSE(entries[0].isExtended);
    EXPECT_TRUE(entries[1].isExtended);
}

TEST(HandlingParse, PhysicsJsonIntegration) {
    HandlingEntry entry;
    const char* line = "LANDSTAL\t1800.0\t5000.0\t0.5\t0.0\t0.0\t-0.3\t70.0\t0.65\t0.85\t0.49\t5\t290\t15.9\t55\t4\tP\t12\t0.6\t1\t33\t0.9\t0.2\t0\t0.3\t-0.22\t0.5\t0.3\t0.2\t0.3\t50000\t2000\t300002\t1\t1\t0";
    HandlingParser::ParseLine(line, entry);

    const char* json = R"({
        "vehicles": {
            "angularCollisionResponseXY": [1.0, 0.4],
            "angularCollisionResponseZ": [1.0, 0.2],
            "collisionTorqueRateZ": [1.0, 0.4],
            "linearCollisionResponseXY": [1.0, 0.4],
            "linearCollisionResponseZ": [1.0, 0.2],
            "speedRange": [30.0, 60.0]
        }
    })";

    EXPECT_TRUE(HandlingParser::ApplyPhysicsJson(json, strlen(json), entry));
    EXPECT_FLOAT_EQ(entry.data.angularCollisionResponseXY[0], 1.0f);
    EXPECT_FLOAT_EQ(entry.data.angularCollisionResponseXY[1], 0.4f);
    EXPECT_FLOAT_EQ(entry.data.speedRange[0], 30.0f);
    EXPECT_FLOAT_EQ(entry.data.speedRange[1], 60.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}