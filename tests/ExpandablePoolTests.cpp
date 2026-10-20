// Exercise the same CPool implementation used by buildings and sector nodes
// without starting RenderWare or reserving hundreds of thousands of slots.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef int32_t int32;
typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uintptr_t uintptr;
#define nil nullptr
#define FIX_BUGS
#define debug(...) ((void)0)
#include "../src/core/templates.h"

struct Node { int value; Node *next; };
struct BasePed { int state; };
struct PlayerPed : BasePed { int extras[8]; };

int main()
{
    // Derived slot storage must remain stable as the ped pool grows, and a
    // validity check must reject stale/foreign pointers without dereferencing.
    CPool<BasePed, PlayerPed> peds(2, "Peds", 2);
    BasePed *ped = peds.New();
    BasePed outsider = {0};
    assert(peds.IsAllocated(ped));
    assert(!peds.IsAllocated(&outsider) && !peds.IsAllocated(nullptr));
    assert(!peds.IsAllocated((BasePed*)((char*)ped + 1)));
    peds.Delete(ped);
    assert(!peds.IsAllocated(ped) && peds.GetSlot(0) == nullptr);
    BasePed *savedPed = peds.New((5 << 8) | 13);
    assert(savedPed && peds.GetSize() == 6 && peds.GetAt((5 << 8) | 13) == savedPed);
    assert(peds.GetAt((5 << 8) | 12) == nullptr);
    assert(peds.GetNoOfUsedSpaces() == 1);
    BasePed *nextPed = peds.New();
    assert(nextPed && peds.IsAllocated(nextPed));
    assert(peds.IsAllocated(savedPed));

    CPool<Node> fixed(1, "ordinary pool");
    assert(!fixed.EnsureFree(2));
    Node *ordinary = fixed.New();
    assert(ordinary && fixed.New() == nullptr);
    assert(fixed.IsAllocated(ordinary));
    fixed.Delete(ordinary);
    assert(!fixed.IsAllocated(ordinary));
    assert(fixed.GetNoOfFreeSpaces() == 1);
    fixed.SetIsFree(0, false);
    assert(fixed.GetNoOfUsedSpaces() == 1);
    fixed.SetIsFree(0, true);
    assert(fixed.GetNoOfFreeSpaces() == 1);

    CPool<Node> pool(3, "sector nodes", 2);
    assert(pool.GetSize() == 3 && pool.GetNoOfFreeSpaces() == 3);
    Node *first = pool.New(), *second = pool.New(), *third = pool.New();
    first->value = 11;
    second->value = 22;
    third->value = 33;
    assert(pool.GetNoOfFreeSpaces() == 0);
    assert(pool.EnsureFree(2) && pool.GetSize() == 5);
    Node *fourth = pool.New(), *fifth = pool.New();
    fourth->value = 44;
    fifth->value = 55;
    assert(first->value == 11 && second->value == 22 && third->value == 33);
    assert(pool.GetJustIndex(first) == 0 && pool.GetJustIndex(fourth) == 3);
    assert(pool.GetSlot(4) == fifth && pool.GetAt(pool.GetIndex(fourth)) == fourth);
    Node unrelated = {0, nullptr};
    assert(pool.GetJustIndex_NoFreeAssert(&unrelated) == -1);
    assert(pool.GetSlot(-1) == nullptr && pool.GetSlot(5) == nullptr);
    assert(pool.GetIsFree(-1) && pool.GetIsFree(5));
    pool.Delete(second);
    Node *reused = pool.New();
    assert(reused == second && pool.GetNoOfFreeSpaces() == 0);
    pool.Delete(fifth);
    assert(pool.GetSlot(4) == nullptr);
    assert(pool.GetNoOfUsedSpaces() == 4);

    // Replay stores whole pool contents, including pointers to earlier blocks.
    uint8 *flags = nullptr;
    Node *entries = nullptr;
    pool.Store(flags, entries);
    int32 handle = pool.GetIndex(fourth);
    fourth->value = 100;
    pool.Delete(fourth);
    assert(pool.EnsureFree(5) && pool.GetSize() >= 9); // grow after Store
    Node *extra = pool.New();
    extra->value = 101;
    pool.CopyBack(flags, entries);
    assert(flags == nullptr && entries == nullptr);
    assert(pool.GetNoOfUsedSpaces() == 4);
    assert(pool.GetAt(handle) == fourth && fourth->value == 44);
    assert(pool.GetSlot(5) == nullptr); // new blocks must not survive replay
    assert(first->value == 11 && third->value == 33);

    // More than the 129,918 accepted instances in the supplied map archive:
    // no 50k-building/16-bit-slot ceiling, and the first pointer stays valid.
    CPool<Node> large(1, "buildings", 32768);
    assert(large.EnsureFree(130000));
    Node *last = nullptr, *firstLarge = nullptr;
    for(int i = 0; i < 130000; i++) {
        last = large.New();
        if(i == 0) firstLarge = last;
    }
    assert(firstLarge && last && large.GetJustIndex(firstLarge) == 0);
    assert(large.GetJustIndex(last) == 129999);
    assert(large.GetAt(large.GetIndex(last)) == last);

    // Model IDs refer to stable infos, not positions in a vector that may
    // relocate on reallocation. Test both sides of the original store edge.
    CStore<Node, 2, 3, 9> infos;
    Node *model0 = infos.Alloc();
    model0->value = 123;
    Node *model1 = infos.Alloc();
    Node *model2 = infos.Alloc();
    assert(infos.GetItem(0) == model0 && infos.GetItem(1) == model1);
    assert(infos.GetIndex(model2) == 2 && infos.GetItem(2) == model2);
    for(int i = 3; i < 9; i++)
        assert(infos.GetIndex(infos.Alloc()) == i);
    assert(!infos.CanAlloc());
    assert(model0->value == 123 && infos.GetItem(2) == model2);
    infos.Clear();
    assert(infos.CanAlloc() && infos.Alloc() == model0);
    std::puts("stable expandable pool and model-store tests passed");
}
