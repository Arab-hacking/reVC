#include "HandlingParse.h"
#include <cctype>
#include <sstream>

namespace brhandling {

static inline void trim(std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) start++;
    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end-1])) end--;
    s = s.substr(start, end - start);
}

static inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::vector<HandlingEntry> HandlingParser::Parse(const char* data, size_t size, std::string* err) {
    std::vector<HandlingEntry> entries;
    if (!data || size == 0) return entries;

    std::string_view view(data, size);
    size_t pos = 0;
    while (pos < view.size()) {
        size_t nl = view.find('\n', pos);
        if (nl == std::string_view::npos) nl = view.size();
        std::string line(view.substr(pos, nl - pos));
        pos = nl + 1;

        trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        HandlingEntry entry;
        if (ParseLine(line.c_str(), entry)) {
            entries.push_back(std::move(entry));
        } else if (err) {
            *err = "Failed to parse line: " + line;
        }
    }
    return entries;
}

bool HandlingParser::ParseLine(const char* line, HandlingEntry& out) {
    if (!line) return false;
    std::string s(line);
    trim(s);
    if (s.empty() || s[0] == ';' || s[0] == '#') return false;

    auto parts = split(s, '\t');
    if (parts.empty()) parts = split(s, ' ');  // fallback

    // Expect at least 36 fields (VC) or 39 (SA/BR)
    if (parts.size() < 36) return false;

    HandlingData& d = out.data;
    std::memset(&d, 0, sizeof(d));
    out.isExtended = (parts.size() >= 39);

    // Name
    std::strncpy(d.name, parts[0].c_str(), 23);
    d.name[23] = '\0';

    // Float fields
    auto getFloat = [&](size_t idx, float def = 0.0f) -> float {
        if (idx < parts.size()) {
            try { return std::stof(parts[idx]); } catch (...) { return def; }
        }
        return def;
    };
    auto getInt = [&](size_t idx, int def = 0) -> int {
        if (idx < parts.size()) {
            try { return std::stoi(parts[idx]); } catch (...) { return def; }
        }
        return def;
    };

    d.mass = getFloat(1);
    d.turnMass = getFloat(2);
    d.dragMult = getFloat(3);
    d.centerOfMass[0] = getFloat(4);
    d.centerOfMass[1] = getFloat(5);
    d.centerOfMass[2] = getFloat(6);
    d.percentSubmerged = getFloat(7);
    d.tractionMultiplier = getFloat(8);
    d.tractionLoss = getFloat(9);
    d.tractionBias = getFloat(10);
    d.transmissionType = getInt(11);
    d.engineType = getInt(12);
    d.driveType = getInt(13);
    d.nGears = getInt(14);
    d.maxVelocity = getFloat(15);
    d.engineAcceleration = getFloat(16);
    d.engineInertia = getFloat(17);
    d.brakeDeceleration = getFloat(18);
    d.brakeBias = getFloat(19);
    d.ABS = getInt(20);
    d.steeringLock = getFloat(21);
    d.suspensionForce = getFloat(22);
    d.suspensionDamping = getFloat(23);
    d.suspensionHighSpeedDamping = getFloat(24);
    d.suspensionUpperLimit = getFloat(25);
    d.suspensionLowerLimit = getFloat(26);
    d.suspensionBias = getFloat(27);
    d.antiDiveMultiplier = getFloat(28);
    d.seatOffset = getInt(29);
    d.collisionDamageMultiplier = getFloat(30);
    d.monetaryValue = getInt(31);
    d.modelFlags = getInt(32);
    d.handlingFlags = getInt(33);
    d.frontLights = getInt(34);
    d.rearLights = getInt(35);

    // Extended fields (39)
    if (out.isExtended) {
        d.suspensionBiasFront = getFloat(36);
        d.suspensionBiasRear = getFloat(37);
        d.suspensionAntiDive = getFloat(38);
    }

    return true;
}

int HandlingParser::FindHandlingId(const char* name) {
    // Map handling names to indices (SA style)
    static const std::unordered_map<std::string, int> nameToId = {
        {"LANDSTAL", 0}, {"BRAVURA", 1}, {"BUFFALO", 2}, {"LINERUN", 3},
        {"PEREN", 4}, {"SENTINEL", 5}, {"DUMPER", 6}, {"FIRETRUK", 7},
        // ... would be populated from vehicles.ide
    };
    auto it = nameToId.find(name);
    return it != nameToId.end() ? it->second : -1;
}

bool HandlingParser::ApplyPhysicsJson(const char* jsonData, size_t size, HandlingEntry& entry) {
    try {
        auto j = nlohmann::json::parse(jsonData, jsonData + size);
        if (j.contains("vehicles")) {
            auto& v = j["vehicles"];
            if (v.contains("angularCollisionResponseXY")) {
                auto& arr = v["angularCollisionResponseXY"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.angularCollisionResponseXY[0] = arr[0].get<float>();
                    entry.data.angularCollisionResponseXY[1] = arr[1].get<float>();
                }
            }
            if (v.contains("angularCollisionResponseZ")) {
                auto& arr = v["angularCollisionResponseZ"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.angularCollisionResponseZ[0] = arr[0].get<float>();
                    entry.data.angularCollisionResponseZ[1] = arr[1].get<float>();
                }
            }
            if (v.contains("collisionTorqueRateZ")) {
                auto& arr = v["collisionTorqueRateZ"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.collisionTorqueRateZ[0] = arr[0].get<float>();
                    entry.data.collisionTorqueRateZ[1] = arr[1].get<float>();
                }
            }
            if (v.contains("linearCollisionResponseXY")) {
                auto& arr = v["linearCollisionResponseXY"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.linearCollisionResponseXY[0] = arr[0].get<float>();
                    entry.data.linearCollisionResponseXY[1] = arr[1].get<float>();
                }
            }
            if (v.contains("linearCollisionResponseZ")) {
                auto& arr = v["linearCollisionResponseZ"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.linearCollisionResponseZ[0] = arr[0].get<float>();
                    entry.data.linearCollisionResponseZ[1] = arr[1].get<float>();
                }
            }
            if (v.contains("speedRange")) {
                auto& arr = v["speedRange"];
                if (arr.is_array() && arr.size() >= 2) {
                    entry.data.speedRange[0] = arr[0].get<float>();
                    entry.data.speedRange[1] = arr[1].get<float>();
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace brhandling