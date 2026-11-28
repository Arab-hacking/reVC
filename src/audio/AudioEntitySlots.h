#ifndef REVC_AUDIO_ENTITY_SLOTS_H
#define REVC_AUDIO_ENTITY_SLOTS_H

#include <cstddef>
#include <new>
#include <vector>

// Grow the entity registry and its index-ordered active list as a pair.
// Existing audio handles are integer indices, never pointers into a vector.
template<typename Entity, typename Index>
inline bool GrowAudioEntitySlots(std::vector<Entity> &entities,
                                std::vector<Index> &order,
                                std::size_t increment, std::size_t maxEntries)
{
    const std::size_t oldSize = entities.size();
    if(oldSize != order.size() || !increment || increment > maxEntries ||
       oldSize > maxEntries - increment ||
       oldSize + increment > entities.max_size() ||
       oldSize + increment > order.max_size())
        return false;
    try {
        entities.resize(oldSize + increment);
        order.resize(oldSize + increment, static_cast<Index>(oldSize + increment));
    } catch(const std::bad_alloc &) {
        entities.resize(oldSize); // shrinking cannot allocate
        return false;
    }
    return true;
}

#endif
