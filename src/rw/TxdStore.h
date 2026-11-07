#ifndef __GTA_TXDSTORE_H__
#define __GTA_TXDSTORE_H__

#include "templates.h"

struct TxdDef {
	RwTexDictionary *texDict;
	int refCount;
	int16 parentIndex;
	char name[32];
};

class CTxdStore
{
	static CPool<TxdDef,TxdDef> *ms_pTxdPool;
	static RwTexDictionary *ms_pStoredTxd;
	static int ms_currentTxdSlot;
	static int ms_storedTxdSlot;
	static int ms_vehicleTxdSlot;
public:
	static void Initialise(void);
	static void Shutdown(void);
	static void GameShutdown(void);
	static int AddTxdSlot(const char *name);
	static bool HasFreeSlot(void);
	static bool HasFreeCustomSlot(void); // leave headroom for runtime/UI TXDs
	static void RemoveTxdSlot(int slot);
	static int FindTxdSlot(const char *name);
	static char *GetTxdName(int slot);
	static void SetTxdParent(int slot, int parentSlot);
	static int GetParentTxdSlot(int slot);
	static int GetCurrentTxdSlot(void) { return ms_currentTxdSlot; }
	static void SetVehicleTxdSlot(int slot) { ms_vehicleTxdSlot = slot; }
	static int GetVehicleTxdSlot(void) { return ms_vehicleTxdSlot; }
	static RwTexture *FindTextureInHierarchy(int slot, const char *name);
	static void PushCurrentTxd(void);
	static void PopCurrentTxd(void);
	static void SetCurrentTxd(int slot);
	static void Create(int slot);
	static int GetNumRefs(int slot);
	static void AddRef(int slot);
	static void RemoveRef(int slot);
	static void RemoveRefWithoutDelete(int slot);
	static bool LoadTxd(int slot, RwStream *stream);
	static bool LoadTxd(int slot, const char *filename);
	static bool StartLoadTxd(int slot, RwStream *stream);
	static bool FinishLoadTxd(int slot, RwStream *stream);
	static void RemoveTxd(int slot);

	static TxdDef *GetSlot(int slot) {
		assert(slot >= 0);
		assert(ms_pTxdPool);
		assert(slot < ms_pTxdPool->GetSize());
		return ms_pTxdPool->GetSlot(slot);
	}
	static bool isTxdLoaded(int slot);
};

#endif // __GTA_TXDSTORE_H__
