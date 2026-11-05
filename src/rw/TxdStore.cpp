#include "common.h"

#include "templates.h"
#include "General.h"
#include "Streaming.h"
#include "RwHelper.h"
#include "TxdStore.h"
#include <string>
#include <unordered_map>
#include <set>
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif

CPool<TxdDef,TxdDef> *CTxdStore::ms_pTxdPool;
RwTexDictionary *CTxdStore::ms_pStoredTxd;
int CTxdStore::ms_currentTxdSlot = -1;
int CTxdStore::ms_storedTxdSlot = -1;
int CTxdStore::ms_vehicleTxdSlot = -1;
static_assert(TXDSTORESIZE <= 32768, "CBaseModelInfo stores TXD slot in signed int16");

namespace {
// The pool may hold tens of thousands of BR TXDs. A linear name lookup or
// GetNoOfUsedSpaces scan per IDE/DIR record makes startup quadratic. Track
// actual slots (not their RW dictionaries); duplicate names still resolve to
// the lowest pool index, as the original FindTxdSlot loop did.
std::unordered_map<std::string, std::set<int> > txdSlotsByName;
int txdSlotsUsed = 0;

std::string TxdKey(const char *name)
{
	std::string key(name);
	for(size_t i = 0; i < key.size(); i++)
		if(key[i] >= 'A' && key[i] <= 'Z') key[i] += 'a' - 'A';
	return key;
}
}

void
CTxdStore::Initialise(void)
{
	if(ms_pTxdPool == nil){
		ms_pTxdPool = new CPool<TxdDef,TxdDef>(TXDSTORESIZE, "TexDictionary");
		txdSlotsByName.clear();
		txdSlotsUsed = 0;
		ms_currentTxdSlot = -1;
		ms_storedTxdSlot = -1;
		ms_vehicleTxdSlot = -1;
	}
}

void
CTxdStore::Shutdown(void)
{
	delete ms_pTxdPool;
	ms_pTxdPool = nil;
	txdSlotsByName.clear();
	txdSlotsUsed = 0;
	ms_currentTxdSlot = -1;
	ms_storedTxdSlot = -1;
	ms_vehicleTxdSlot = -1;
}

void
CTxdStore::GameShutdown(void)
{
	int i;

	for(i = 0; i < TXDSTORESIZE; i++){
		TxdDef *def = GetSlot(i);
		if(def && GetNumRefs(i) == 0)
			RemoveTxdSlot(i);
	}
}

bool
CTxdStore::HasFreeSlot(void)
{
	return ms_pTxdPool && txdSlotsUsed < TXDSTORESIZE;
}

bool
CTxdStore::HasFreeCustomSlot(void)
{
	// Frontend/script dictionaries may still be created after IMG indexing.
	return ms_pTxdPool && txdSlotsUsed < TXDSTORESIZE - 32;
}

int
CTxdStore::AddTxdSlot(const char *name)
{
	TxdDef *def = ms_pTxdPool->New();
	assert(def);
	def->texDict = nil;
	def->refCount = 0;
	def->parentIndex = -1;
	strncpy(def->name, name, sizeof(def->name)-1);
	def->name[sizeof(def->name)-1] = '\0';
	int slot = ms_pTxdPool->GetJustIndex(def);
	std::string key = TxdKey(def->name);
	txdSlotsByName[key].insert(slot);
	if(ms_vehicleTxdSlot < 0 && key == "vehicle")
		ms_vehicleTxdSlot = slot;
	txdSlotsUsed++;
	return slot;
}

void
CTxdStore::RemoveTxdSlot(int slot)
{
#ifdef CUSTOM_MODELS
	CCustomModels::ForgetTxdSlot(slot);
#endif
	if(ms_currentTxdSlot == slot)
		ms_currentTxdSlot = -1;
	if(ms_storedTxdSlot == slot)
		ms_storedTxdSlot = -1;
	if(ms_vehicleTxdSlot == slot)
		ms_vehicleTxdSlot = -1;
	// Detach any children referencing this slot so no dangling parentIndex remains.
	for(int i = 0; i < TXDSTORESIZE; i++){
		TxdDef *child = GetSlot(i);
		if(child && child->parentIndex == slot)
			child->parentIndex = -1;
	}
	TxdDef *def = GetSlot(slot);
	std::unordered_map<std::string, std::set<int> >::iterator it =
		txdSlotsByName.find(TxdKey(def->name));
	assert(it != txdSlotsByName.end() && it->second.count(slot));
	if(it != txdSlotsByName.end()) {
		it->second.erase(slot);
		if(it->second.empty()) txdSlotsByName.erase(it);
	}
	if(def->texDict)
		RwTexDictionaryDestroy(def->texDict);
	ms_pTxdPool->Delete(def);
	txdSlotsUsed--;
}

int
CTxdStore::FindTxdSlot(const char *name)
{
	std::unordered_map<std::string, std::set<int> >::const_iterator it =
		txdSlotsByName.find(TxdKey(name));
	return it == txdSlotsByName.end() ? -1 : *it->second.begin();
}

char*
CTxdStore::GetTxdName(int slot)
{
	return GetSlot(slot)->name;
}

void
CTxdStore::SetTxdParent(int slot, int parentSlot)
{
	if(slot < 0 || slot >= TXDSTORESIZE)
		return;
	TxdDef *def = GetSlot(slot);
	if(def == nil || def->refCount > 0)
		return;
	if(parentSlot < 0 || parentSlot >= TXDSTORESIZE || parentSlot == slot || GetSlot(parentSlot) == nil){
		def->parentIndex = -1;
		return;
	}
	// Reject cycles (e.g. A -> B -> A) in malformed txdp tables.
	for(int cur = parentSlot, depth = 0; cur >= 0 && depth < 16; depth++){
		if(cur == slot)
			return;
		TxdDef *p = GetSlot(cur);
		cur = p ? p->parentIndex : -1;
	}
	def->parentIndex = (int16)parentSlot;
}

int
CTxdStore::GetParentTxdSlot(int slot)
{
	if(slot < 0 || slot >= TXDSTORESIZE || ms_pTxdPool == nil)
		return -1;
	TxdDef *def = GetSlot(slot);
	return def ? (int)def->parentIndex : -1;
}

RwTexture*
CTxdStore::FindTextureInHierarchy(int slot, const char *name)
{
	if(name == nil || name[0] == '\0' || ms_pTxdPool == nil)
		return nil;
	for(int cur = slot, depth = 0; cur >= 0 && cur < TXDSTORESIZE && depth < 8; depth++){
		TxdDef *def = GetSlot(cur);
		if(def == nil)
			break;
		if(def->texDict){
			RwTexture *tex = RwTexDictionaryFindNamedTexture(def->texDict, name);
			if(tex)
				return tex;
		}
		cur = def->parentIndex;
	}
	if(ms_vehicleTxdSlot >= 0 && ms_vehicleTxdSlot < TXDSTORESIZE){
		TxdDef *vehDef = GetSlot(ms_vehicleTxdSlot);
		if(vehDef && vehDef->texDict){
			RwTexture *tex = RwTexDictionaryFindNamedTexture(vehDef->texDict, name);
			if(tex)
				return tex;
		}
	}
	return nil;
}

void
CTxdStore::PushCurrentTxd(void)
{
	ms_pStoredTxd = RwTexDictionaryGetCurrent();
	ms_storedTxdSlot = ms_currentTxdSlot;
}

void
CTxdStore::PopCurrentTxd(void)
{
	RwTexDictionarySetCurrent(ms_pStoredTxd);
	ms_currentTxdSlot = ms_storedTxdSlot;
	ms_pStoredTxd = nil;
	ms_storedTxdSlot = -1;
}

void
CTxdStore::SetCurrentTxd(int slot)
{
	ms_currentTxdSlot = slot;
	RwTexDictionarySetCurrent(GetSlot(slot)->texDict);
}

void
CTxdStore::Create(int slot)
{
	GetSlot(slot)->texDict = RwTexDictionaryCreate();
}

int
CTxdStore::GetNumRefs(int slot)
{
	return GetSlot(slot)->refCount;
}

void
CTxdStore::AddRef(int slot)
{
	for(int cur = slot, depth = 0; cur >= 0 && cur < TXDSTORESIZE && depth < 8; depth++){
		TxdDef *def = GetSlot(cur);
		if(def == nil)
			break;
		def->refCount++;
		cur = def->parentIndex;
	}
}

void
CTxdStore::RemoveRef(int slot)
{
	for(int cur = slot, depth = 0; cur >= 0 && cur < TXDSTORESIZE && depth < 8; depth++){
		TxdDef *def = GetSlot(cur);
		if(def == nil)
			break;
		int next = def->parentIndex;
		if(--def->refCount <= 0){
			def->refCount = 0;
			if(def->texDict != nil || CStreaming::HasTxdLoaded(cur))
				CStreaming::RemoveTxd(cur);
		}
		cur = next;
	}
}

void
CTxdStore::RemoveRefWithoutDelete(int slot)
{
	for(int cur = slot, depth = 0; cur >= 0 && cur < TXDSTORESIZE && depth < 8; depth++){
		TxdDef *def = GetSlot(cur);
		if(def == nil)
			break;
		if(--def->refCount < 0)
			def->refCount = 0;
		cur = def->parentIndex;
	}
}

bool
CTxdStore::LoadTxd(int slot, RwStream *stream)
{
	TxdDef *def = GetSlot(slot);

	if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nil, nil)){
		def->texDict = RwTexDictionaryGtaStreamRead(stream);
		return def->texDict != nil;
	}
	printf("Failed to load TXD\n");
	return false;
}

bool
CTxdStore::LoadTxd(int slot, const char *filename)
{
	RwStream *stream;
	bool ret;

	ret = false;
#ifdef GTA_PC
	_rwD3D8TexDictionaryEnableRasterFormatConversion(true);
#endif
	do
		stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	while(stream == nil);
	ret = LoadTxd(slot, stream);
	RwStreamClose(stream, nil);
	return ret;
}

bool
CTxdStore::StartLoadTxd(int slot, RwStream *stream)
{
	TxdDef *def = GetSlot(slot);
	if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nil, nil)){
		def->texDict = RwTexDictionaryGtaStreamRead1(stream);
		return def->texDict != nil;
	}else{
		printf("Failed to load TXD\n");
		return false;
	}
}

bool
CTxdStore::FinishLoadTxd(int slot, RwStream *stream)
{
	TxdDef *def = GetSlot(slot);
	def->texDict = RwTexDictionaryGtaStreamRead2(stream, def->texDict);
	return def->texDict != nil;
}

void
CTxdStore::RemoveTxd(int slot)
{
#ifdef CUSTOM_MODELS
	CCustomModels::ForgetTxdSlot(slot);
#endif
	TxdDef *def = GetSlot(slot);
	if(def->texDict)
		RwTexDictionaryDestroy(def->texDict);
	def->texDict = nil;
}
