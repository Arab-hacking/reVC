#include <cassert>
#include "../src/core/PlayerModelCycle.h"

int main()
{
    bool isPed[12] = {};
    isPed[0] = isPed[3] = isPed[9] = true;
    auto eligible = [&isPed](int id) { return isPed[id]; };
    assert(FindNextPlayerModel(0, 1, 12, eligible) == 3);
    assert(FindNextPlayerModel(9, 1, 12, eligible) == 0);
    assert(FindNextPlayerModel(0, -1, 12, eligible) == 9);
    assert(FindNextPlayerModel(3, -1, 12, eligible) == 0);
    isPed[3] = isPed[9] = false;
    assert(FindNextPlayerModel(0, 1, 12, eligible) == -1);
    assert(FindNextPlayerModel(-1, 1, 12, eligible) == -1);
    assert(FindNextPlayerModel(0, 0, 12, eligible) == -1);
}
