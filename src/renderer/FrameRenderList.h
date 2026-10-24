#ifndef REVC_FRAME_RENDER_LIST_H
#define REVC_FRAME_RENDER_LIST_H

#include <climits>
#include <vector>

// Lists of entities to draw are transient, not streaming/model caches. Only
// the active count is used by the legacy renderer; vector capacity is reused
// between frames so dense modded scenes cannot overrun a fixed pointer array.
template<typename T>
inline void AppendFrameRenderItem(std::vector<T *> &items, int32 &count, T *item)
{
    if(count == INT_MAX) return; // the legacy renderer counts with int32
    items.push_back(item);
    ++count;
}

template<typename T>
inline void ClearFrameRenderItems(std::vector<T *> &items, int32 &count)
{
    items.clear();
    count = 0;
}

#endif
