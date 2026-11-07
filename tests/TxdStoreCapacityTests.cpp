// Exercise the real TXD name/slot index at the scale shown in debug.log.
// RenderWare dictionaries are not created: these are metadata slots, not a
// test of resident texture memory or the renderer.
#include "common.h"
#include "TxdStore.h"
#include "CustomModels.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
RwBool RwTexDictionaryDestroy(RwTexDictionary *) { std::abort(); return false; }
void CCustomModels::ForgetTxdSlot(int32) {}

int main() {
    static_assert(TXDSTORESIZE <= 32768, "model-info TXD slot is a signed int16");
    CTxdStore::Initialise();
    const int native = CTxdStore::AddTxdSlot("NATIVE");
    const int duplicate = CTxdStore::AddTxdSlot("native");
    assert(CTxdStore::FindTxdSlot("NaTiVe") == native);
    CTxdStore::RemoveTxdSlot(native);
    assert(CTxdStore::FindTxdSlot("NATIVE") == duplicate);
    CTxdStore::RemoveTxdSlot(duplicate);
    assert(CTxdStore::FindTxdSlot("native") == -1);

    std::vector<int> slots;
    // Reserve stock names first, then simulate 26k IMG TXD names. None of
    // these slots contains a RW dictionary or counts against a GPU/CPU LRU.
    slots.push_back(CTxdStore::AddTxdSlot("gta3"));
    for(int i = 0; i < 28000; i++) {
        std::string name = "brtex_" + std::to_string(i);
        slots.push_back(CTxdStore::AddTxdSlot(name.c_str()));
        assert(CTxdStore::FindTxdSlot(name.c_str()) == slots.back());
    }
    assert(CTxdStore::FindTxdSlot("GTA3") == slots.front());
    assert(CTxdStore::HasFreeCustomSlot());
    for(int i = 28000; CTxdStore::HasFreeCustomSlot(); i++) {
        std::string name = "brtex_" + std::to_string(i);
        slots.push_back(CTxdStore::AddTxdSlot(name.c_str()));
    }
    assert(!CTxdStore::HasFreeCustomSlot() && CTxdStore::HasFreeSlot());
    for(int i = 0; CTxdStore::HasFreeSlot(); i++) {
        std::string name = "stock_" + std::to_string(i);
        slots.push_back(CTxdStore::AddTxdSlot(name.c_str()));
    }
    assert(slots.size() == TXDSTORESIZE && !CTxdStore::HasFreeSlot());
    assert(CTxdStore::FindTxdSlot("stock_31") >= 0);

    for(size_t i = 0; i < slots.size(); i++)
        if(i % 2) CTxdStore::RemoveTxdSlot(slots[i]);
    assert(CTxdStore::HasFreeCustomSlot());
    assert(CTxdStore::FindTxdSlot("brtex_0") == -1);
    assert(CTxdStore::FindTxdSlot("brtex_1") >= 0);
    assert(CTxdStore::FindTxdSlot("gta3") == slots.front());
    CTxdStore::GameShutdown(); // releases unreferenced metadata and resets the index
    assert(CTxdStore::FindTxdSlot("gta3") == -1);
    assert(CTxdStore::HasFreeCustomSlot());
    CTxdStore::Shutdown();
    CTxdStore::Initialise();
    assert(CTxdStore::FindTxdSlot("stock_0") == -1);
    assert(CTxdStore::AddTxdSlot("fresh") >= 0);
    CTxdStore::Shutdown();
    std::puts("TXD pool: 28k+ names, native headroom, case-insensitive lookup and restart passed");
}
