#ifndef REVC_SAVED_POOL_VALIDATION_H
#define REVC_SAVED_POOL_VALIDATION_H

#include <cstddef>
#include <cstdint>
#include <cstring>

// The on-disk VC pool format stores model IDs, not model names. A save made
// with different IDEs can therefore refer to a model that is not registered
// in the current world. Check every record before constructing ANY entity in
// that pool; skipping a record would invalidate script/pickup pool handles.
enum class SavedPoolError {
    None,
    InvalidLayout,
    InvalidVehicleType,
    UnavailableModel
};

struct SavedPoolCheck {
    SavedPoolError error;
    std::size_t entry;
    int16_t model;
    uint32_t type;

    SavedPoolCheck(SavedPoolError e = SavedPoolError::None, std::size_t i = 0,
                   int16_t m = -1, uint32_t t = 0)
        : error(e), entry(i), model(m), type(t) {}
    bool ok() const { return error == SavedPoolError::None; }
};

// Every vehicle pool starts with three signed counts (car, boat, bike). Each
// entry is a packed uint32 type, int16 model, int32 slot, followed by exactly
// its corresponding Save() payload (no per-entry alignment).
template<typename ValidModel>
inline SavedPoolCheck
ValidateSavedVehiclePool(const uint8_t *data, std::size_t length,
                         std::size_t carBytes, std::size_t boatBytes, std::size_t bikeBytes,
                         uint32_t carType, uint32_t boatType, uint32_t bikeType,
                         ValidModel validModel)
{
    const std::size_t countsBytes = 3 * sizeof(int32_t);
    const std::size_t entryHeader = sizeof(uint32_t) + sizeof(int16_t) + sizeof(int32_t);
    if (data == nullptr || length < countsBytes)
        return SavedPoolCheck(SavedPoolError::InvalidLayout);

    int32_t counts[3];
    std::memcpy(counts, data, countsBytes);
    if (counts[0] < 0 || counts[1] < 0 || counts[2] < 0)
        return SavedPoolCheck(SavedPoolError::InvalidLayout);
    const uint64_t total = uint64_t(counts[0]) + counts[1] + counts[2];
    if (total > (length - countsBytes) / entryHeader)
        return SavedPoolCheck(SavedPoolError::InvalidLayout);

    std::size_t offset = countsBytes;
    uint32_t observed[3] = {};
    for (uint64_t i = 0; i < total; i++) {
        uint32_t type;
        int16_t model;
        std::memcpy(&type, data + offset, sizeof(type));
        std::memcpy(&model, data + offset + sizeof(type), sizeof(model));
        offset += entryHeader;

        std::size_t bodyBytes;
        if (type == carType) { bodyBytes = carBytes; observed[0]++; }
        else if (type == boatType) { bodyBytes = boatBytes; observed[1]++; }
        else if (type == bikeType) { bodyBytes = bikeBytes; observed[2]++; }
        else return SavedPoolCheck(SavedPoolError::InvalidVehicleType, i, model, type);
        if (bodyBytes > length - offset)
            return SavedPoolCheck(SavedPoolError::InvalidLayout, i, model, type);
        if (!validModel(model, type))
            return SavedPoolCheck(SavedPoolError::UnavailableModel, i, model, type);
        offset += bodyBytes;
    }
    if (offset != length || observed[0] != uint32_t(counts[0]) ||
        observed[1] != uint32_t(counts[1]) || observed[2] != uint32_t(counts[2]))
        return SavedPoolCheck(SavedPoolError::InvalidLayout, total);
    return SavedPoolCheck();
}

// The mission-object pool stores an int32 count and fixed-size records whose
// first field is an int16 model. The caller supplies the exact record width
// used by CPools::SaveObjectPool for this build.
template<typename ValidModel>
inline SavedPoolCheck
ValidateSavedObjectPool(const uint8_t *data, std::size_t length,
                        std::size_t recordBytes, ValidModel validModel)
{
    if (data == nullptr || length < sizeof(int32_t) || recordBytes < sizeof(int16_t))
        return SavedPoolCheck(SavedPoolError::InvalidLayout);
    int32_t count;
    std::memcpy(&count, data, sizeof(count));
    if (count < 0 || std::size_t(count) > (length - sizeof(count)) / recordBytes ||
        std::size_t(count) * recordBytes != length - sizeof(count))
        return SavedPoolCheck(SavedPoolError::InvalidLayout);
    for (int32_t i = 0; i < count; i++) {
        int16_t model;
        std::memcpy(&model, data + sizeof(count) + std::size_t(i) * recordBytes, sizeof(model));
        if (!validModel(model))
            return SavedPoolCheck(SavedPoolError::UnavailableModel, i, model);
    }
    return SavedPoolCheck();
}

#endif // REVC_SAVED_POOL_VALIDATION_H
