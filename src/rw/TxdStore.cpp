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
	}
}

void
CTxdStore::Shutdown(void)
{
	delete ms_pTxdPool;
	ms_pTxdPool = nil;
	txdSlotsByName.clear();
	txdSlotsUsed = 0;
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
	strncpy(def->name, name, sizeof(def->name)-1);
	def->name[sizeof(def->name)-1] = '\0';
	int slot = ms_pTxdPool->GetJustIndex(def);
	txdSlotsByName[TxdKey(def->name)].insert(slot);
	txdSlotsUsed++;
	return slot;
}

void
CTxdStore::RemoveTxdSlot(int slot)
{
#ifdef CUSTOM_MODELS
	CCustomModels::ForgetTxdSlot(slot);
#endif
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
CTxdStore::PushCurrentTxd(void)
{
	ms_pStoredTxd = RwTexDictionaryGetCurrent();
}

void
CTxdStore::PopCurrentTxd(void)
{
	RwTexDictionarySetCurrent(ms_pStoredTxd);
	ms_pStoredTxd = nil;
}

void
CTxdStore::SetCurrentTxd(int slot)
{
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
	GetSlot(slot)->refCount++;
}

void
CTxdStore::RemoveRef(int slot)
{
	if(--GetSlot(slot)->refCount <= 0)
		CStreaming::RemoveTxd(slot);
}

void
CTxdStore::RemoveRefWithoutDelete(int slot)
{
	GetSlot(slot)->refCount--;
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
