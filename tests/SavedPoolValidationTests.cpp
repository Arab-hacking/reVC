#include "../src/save/SavedPoolValidation.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

template<typename T>
static void Append(std::vector<uint8_t> &bytes, T value)
{
    const uint8_t *p = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(value));
}
static void AddVehicle(std::vector<uint8_t> &bytes, uint32_t type, int16_t model,
                       int32_t slot, std::size_t body)
{
    Append(bytes, type);
    Append(bytes, model);
    Append(bytes, slot);
    bytes.resize(bytes.size() + body, 0);
}
static bool VehicleModel(int16_t id, uint32_t type)
{
    return (id == 104 && type == 0) || (id == 105 && type == 1) ||
           (id == 106 && type == 5);
}
static SavedPoolCheck Vehicles(const std::vector<uint8_t> &b)
{
    return ValidateSavedVehiclePool(b.data(), b.size(), 1500, 1216, 1260,
                                     0, 1, 5, VehicleModel);
}
static bool ObjectModel(int16_t id) { return id == 1234; }
static SavedPoolCheck Objects(const std::vector<uint8_t> &b)
{
    return ValidateSavedObjectPool(b.data(), b.size(), 88, ObjectModel);
}

int main()
{
    // The real compatible-save vehicle records have 10-byte headers and
    // bodies of 1500/1216/1260 bytes. Their order is arbitrary in the pool.
    std::vector<uint8_t> vehicles;
    Append<int32_t>(vehicles, 1);
    Append<int32_t>(vehicles, 1);
    Append<int32_t>(vehicles, 1);
    AddVehicle(vehicles, 5, 106, 0x100, 1260);
    AddVehicle(vehicles, 0, 104, 0x200, 1500);
    AddVehicle(vehicles, 1, 105, 0x300, 1216);
    assert(Vehicles(vehicles).ok());

    std::vector<uint8_t> broken = vehicles;
    const int16_t missing = 2500;
    std::memcpy(&broken[12 + sizeof(uint32_t)], &missing, sizeof(missing));
    SavedPoolCheck result = Vehicles(broken);
    assert(result.error == SavedPoolError::UnavailableModel);
    assert(result.entry == 0 && result.model == missing && result.type == 5);

    broken = vehicles;
    const int16_t noModel = -1;
    std::memcpy(&broken[12 + sizeof(uint32_t)], &noModel, sizeof(noModel));
    assert(Vehicles(broken).error == SavedPoolError::UnavailableModel);
    broken = vehicles;
    broken[12] = 9; // not a car, boat or bike
    assert(Vehicles(broken).error == SavedPoolError::InvalidVehicleType);
    broken = vehicles;
    broken.pop_back(); // truncated boat
    assert(Vehicles(broken).error == SavedPoolError::InvalidLayout);
    broken = vehicles;
    broken.push_back(0); // unexpected trailing bytes inside the block
    assert(Vehicles(broken).error == SavedPoolError::InvalidLayout);
    broken = vehicles;
    const int32_t wrongCount = -1;
    std::memcpy(broken.data(), &wrongCount, sizeof(wrongCount));
    assert(Vehicles(broken).error == SavedPoolError::InvalidLayout);
    broken = vehicles;
    const int32_t tooMany = 100000;
    std::memcpy(broken.data(), &tooMany, sizeof(tooMany));
    assert(Vehicles(broken).error == SavedPoolError::InvalidLayout);
    broken = vehicles;
    const int32_t mismatched = 0;
    std::memcpy(broken.data(), &mismatched, sizeof(mismatched));
    assert(Vehicles(broken).error == SavedPoolError::InvalidLayout);

    std::vector<uint8_t> objects;
    Append<int32_t>(objects, 2);
    Append<int16_t>(objects, 1234);
    objects.resize(4 + 88, 0);
    Append<int16_t>(objects, 1234);
    objects.resize(4 + 2 * 88, 0);
    assert(Objects(objects).ok());
    broken = objects;
    std::memcpy(&broken[4 + 88], &noModel, sizeof(noModel));
    result = Objects(broken);
    assert(result.error == SavedPoolError::UnavailableModel && result.entry == 1 && result.model == -1);
    broken = objects;
    broken.resize(broken.size() - 1);
    assert(Objects(broken).error == SavedPoolError::InvalidLayout);
    broken = objects;
    std::memcpy(broken.data(), &wrongCount, sizeof(wrongCount));
    assert(Objects(broken).error == SavedPoolError::InvalidLayout);

    std::vector<uint8_t> empty;
    Append<int32_t>(empty, 0);
    assert(Objects(empty).ok());
    empty.clear();
    Append<int32_t>(empty, 0);
    Append<int32_t>(empty, 0);
    Append<int32_t>(empty, 0);
    assert(Vehicles(empty).ok());

    std::puts("Saved vehicle/object pool preflight: valid, missing models, truncated and malformed passed");
}
