#ifndef REVC_SAVE_BLOCK_BOUNDS_H
#define REVC_SAVE_BLOCK_BOUNDS_H

#include <cstdint>

// totalPayload counts aligned block bodies (including any nested size words),
// not their outer 4-byte length words. The legacy padding loop and verifier
// use exactly that accounting. Reserve a checksum and at least five bytes for
// the last padding block (SIZE_OF_ONE_GAME_IN_BYTES is 1 modulo 4).
inline bool
FitsLegacySave(uint32_t totalPayload, uint32_t nextBody, uint32_t saveLimit)
{
    uint64_t aligned = (uint64_t(nextBody) + 3u) & ~uint64_t(3u);
    return uint64_t(totalPayload) + aligned +
           sizeof(uint32_t) /* checksum */ + 5u /* padding */ <= saveLimit;
}

#endif
