#include <cassert>
#include <climits>
#include <cstdint>
#include <vector>

typedef int32_t int32;
#include "../src/renderer/FrameRenderList.h"

int main()
{
    int entities[3000];
    std::vector<int *> visible;
    int32 count = 0;
    for(int i = 0; i < 3000; ++i){
        entities[i] = i;
        AppendFrameRenderItem(visible, count, &entities[i]);
    }
    assert(count == 3000 && visible.size() == 3000);
    assert(*visible[0] == 0 && *visible[2999] == 2999);
    ClearFrameRenderItems(visible, count);
    assert(count == 0 && visible.empty() && visible.capacity() >= 3000);
    AppendFrameRenderItem(visible, count, &entities[42]);
    assert(count == 1 && visible[0] == &entities[42]);
    count = INT_MAX;
    AppendFrameRenderItem(visible, count, &entities[0]);
    assert(count == INT_MAX && visible.size() == 1);
}
