#pragma once

// Black Russia handling.cfg parser — supports both VC (36 fields) and SA/BR (39 fields)

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace brhandling {

// BR/SA extended handling (39 fields vs VC 36)
struct HandlingData {
    char name[24];           // 0: model name
    float mass;              // 1
    float turnMass;          // 2
    float dragMult;          // 3
    float centerOfMass[3];   // 4-6 (x,y,z)
    float percentSubmerged;  // 7
    float tractionMultiplier; // 8
    float tractionLoss;      // 9
    float tractionBias;      // 10
    int transmissionType;    // 11 (F/R/4)
    int engineType;          // 12 (P/D/E)
    int driveType;           // 13 (F/R/4)
    int nGears;              // 14
    float maxVelocity;       // 15
    float engineAcceleration; // 16
    float engineInertia;     // 17
    float brakeDeceleration; // 18
    float brakeBias;         // 19
    int ABS;                 // 20
    float steeringLock;      // 21
    float suspensionForce;   // 22
    float suspensionDamping; // 23
    float suspensionHighSpeedDamping; // 24
    float suspensionUpperLimit; // 25
    float suspensionLowerLimit; // 26
    float suspensionBias;    // 27
    float antiDiveMultiplier; // 28
    int seatOffset;          // 29
    float collisionDamageMultiplier; // 30
    int monetaryValue;       // 31
    int modelFlags;          // 32
    int handlingFlags;       // 33
    int frontLights;         // 34
    int rearLights;          // 35
    // BR/SA extensions (36-38)
    float suspensionBiasFront;    // 36
    float suspensionBiasRear;     // 37
    float suspensionAntiDive;     // 38
    // Physics.json extras (not in handling.cfg)
    float angularCollisionResponseXY[2]; // from physics.json
    float angularCollisionResponseZ[2];
    float collisionTorqueRateZ[2];
    float linearCollisionResponseXY[2];
    float linearCollisionResponseZ[2];
    float speedRange[2];
};

struct HandlingEntry {
    HandlingData data;
    bool isExtended;  // true if 39 fields parsed
};

class HandlingParser {
public:
    static std::vector<HandlingEntry> Parse(const char* data, size_t size, std::string* err = nullptr);
    static bool ParseLine(const char* line, HandlingEntry& out);

    // Map BR handling names to SA handling IDs
    static int FindHandlingId(const char* name);

    // Physics.json integration
    static bool ApplyPhysicsJson(const char* jsonData, size_t size, HandlingEntry& entry);
};

} // namespace brhandling