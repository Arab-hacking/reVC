#include "common.h"
#include "main.h"

#include "General.h"
#include "CarModMenu.h"
#include "CarModComponent.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "ModelInfo.h"
#include "ClumpModelInfo.h"
#include "VehicleModelInfo.h"
#include "Streaming.h"
#include "PlayerInfo.h"
#include "SavehDiag.h"
#include "RwHelper.h"
#include "NodeName.h"

#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool CCarModMenu::m_bOpen;
int32 CCarModMenu::m_nSelected;
int32 CCarModMenu::m_nFirstVisible;
CVehicle *CCarModMenu::m_pVehicle;
std::vector<const CCarModComponent*> CCarModMenu::m_aCandidates;
std::vector<const CCarModComponent*> CCarModMenu::m_aInstalled;

void
CCarModMenu::Initialise(void)
{
	m_bOpen = false;
	m_nSelected = 0;
	m_nFirstVisible = 0;
	m_pVehicle = nil;
	m_aCandidates.clear();
	m_aInstalled.clear();
}

void
CCarModMenu::Shutdown(void)
{
	m_aCandidates.clear();
	m_aInstalled.clear();
	m_bOpen = false;
	m_pVehicle = nil;
}

bool
CCarModMenu::IsOpen(void)
{
	return m_bOpen;
}

// Список деталей для этой машины: всё из пула, у чего m_slot попадает в слоты
// этой модели. В клиенте такую выборку делает 0x180d6e4, собирая слоты машины
// и прогоняя их через AddToVehicle (0x17fa790).
void
CCarModMenu::BuildList(CVehicle *vehicle)
{
	m_aCandidates.clear();
	if(vehicle == nil)
		return;

	CVehicleModelInfo *mi = vehicle->GetModelInfo();
	if(mi == nil)
		return;

	CCarModComponent *pool = CCarModComponentPool::GetPool();
	for(int i = 0; i < CAR_MOD_POOL_SIZE; i++){
		CCarModComponent &mod = pool[i];
		// В пуле лежат только те записи, что загрузились из файлов.
		if(!mod.m_bInitialised)
			continue;
		// Слот должен быть слотом этой модели. Слоты 20..26 — это misc_a..misc_c
		// и aspl_a..aspl_c (см. veh_component_tables.md), они есть не у каждой
		// модели, поэтому проверяем кадр по имени.
		if(mod.m_slot < 0 || mod.m_slot >= MAX_CAR_MOD_SLOTS)
			continue;
		if(CModelInfo::GetModelInfo(mod.m_modelId) == nil)
			continue;
		m_aCandidates.push_back(&mod);
	}

	// Уже стоящие на машине — из учёта пула.
	const std::vector<const CCarModComponent*> *installed =
		CCarModComponentPool::FindOnVehicle(vehicle);
	m_aInstalled.clear();
	if(installed)
		m_aInstalled = *installed;

	if(m_nSelected >= (int32)m_aCandidates.size())
		m_nSelected = 0;
	m_nFirstVisible = 0;
}

void
CCarModMenu::Open(void)
{
	CVehicle *vehicle = FindPlayerVehicle();
	if(vehicle == nil){
		CUSTOM_LOG("carmodmenu: no vehicle\n");
		return;
	}

	m_pVehicle = vehicle;
	m_bOpen = true;
	m_nSelected = 0;
	m_nFirstVisible = 0;
	BuildList(vehicle);
	CUSTOM_LOG("carmodmenu: %d candidate(s) for model %d\n",
	           (int)m_aCandidates.size(), vehicle->GetModelIndex());
}

void
CCarModMenu::Close(void)
{
	m_bOpen = false;
	m_pVehicle = nil;
	m_aCandidates.clear();
	m_aInstalled.clear();
}

void
CCarModMenu::MoveSelection(int32 delta)
{
	if(m_aCandidates.empty())
		return;

	int32 count = (int32)m_aCandidates.size();
	m_nSelected += delta;
	if(m_nSelected < 0)
		m_nSelected = count - 1;
	if(m_nSelected >= count)
		m_nSelected = 0;

	// Прокрутка окна.
	if(m_nSelected < m_nFirstVisible)
		m_nFirstVisible = m_nSelected;
	if(m_nSelected >= m_nFirstVisible + MAX_VISIBLE)
		m_nFirstVisible = m_nSelected - MAX_VISIBLE + 1;
}

// Ставит деталь на машину. Последовательность взята из
// CVehicleModelInfo::CreateInstance (src/modelinfo/VehicleModelInfo.cpp), где
// клиент так же вешает компоненты машины: клонировать атомик модели, создать
// кадр, перенести в него матрицу атомика, повесить кадр на кадр клампа.
//
// ВАЖНО: у клиента для деталей тюнинга это делает не CreateInstance, а код,
// который ещё не снят (см. tools/client-analysis/carmod_attach.md, §5).
// Поэтому здесь тот же приём, но без позиции/поворота детали — её задаёт
// модель детали собственным кадром.
// Поиск кадра (dummy) по имени в клампе. Клиент делает то же самое:
// 0x16b4650(clump, component->m_name) — деталь садится на dummy машины,
// имя которого записано в carmods.dat (см. carmod_attach.md).
static RwFrame *
FindFrameByName(RwFrame *frame, const char *name)
{
	if(frame == nil || name == nil || *name == '\0')
		return nil;

	const char *frameName = GetFrameNodeName(frame);
	if(frameName && strcmp(frameName, name) == 0)
		return frame;

	for(RwFrame *child = frame->child; child; child = child->next){
		RwFrame *found = FindFrameByName(child, name);
		if(found)
			return found;
	}
	return nil;
}

// Контекст снятия: атомики деталей помечены именем слота, чтобы снимать
// именно тот, который поставил Install, а не любой чужой.
struct SRemoveCtx {
	RwFrame *root;
	int32 slot;
	RpAtomic *found;
};

static RpAtomic *
RemoveFindCB(RpAtomic *atomic, void *data)
{
	SRemoveCtx *ctx = (SRemoveCtx*)data;
	if(ctx->found)
		return atomic;
	RwFrame *frame = RpAtomicGetFrame(atomic);
	if(frame == nil || RwFrameGetParent(frame) != ctx->root)
		return atomic;
	const char *name = GetFrameNodeName(frame);
	if(name && atoi(name) == ctx->slot)
		ctx->found = atomic;
	return atomic;
}

// Крепление детали, как в клиенте: снять то, что стоит в этом слоте, найти
// dummy по имени из carmods.dat, склонировать атомик и посадить его на этот
// кадр — тогда деталь встаёт в свою точку крепления, а не в центр машины.
// Кадру даётся имя слота: по нему Remove находит свою деталь.
bool
CCarModMenu::Install(const CCarModComponent &mod)
{
	if(m_pVehicle == nil)
		return false;

	// Старую деталь в том же слоте клиент снимает перед установкой.
	for(size_t i = 0; i < m_aInstalled.size(); i++){
		if(m_aInstalled[i] != nil && m_aInstalled[i]->m_slot == mod.m_slot){
			Remove(*m_aInstalled[i]);
			break;
		}
	}

	if(!CCarModComponentPool::AddToVehicle(mod, *m_pVehicle, true))
		return false;

	if(!m_pVehicle->m_rwObject)
		return false;

	CClumpModelInfo *partInfo = (CClumpModelInfo*)CModelInfo::GetModelInfo(mod.m_modelId);
	if(partInfo == nil || partInfo->m_clump == nil)
		return false;

	RpAtomic *srcAtomic = GetFirstAtomic(partInfo->m_clump);
	if(srcAtomic == nil)
		return false;

	RpClump *vehicleClump = (RpClump*)m_pVehicle->m_rwObject;
	if(RwObjectGetType((RwObject*)vehicleClump) != rpCLUMP)
		return false;

	// Имя dummy берём из carmods.dat — это ровно то, что делает клиент.
	RwFrame *dummy = FindFrameByName(RpClumpGetFrame(vehicleClump), mod.m_name.c_str());
	if(dummy == nil){
		CUSTOM_LOG("carmodmenu: no dummy '%s' on model %d, slot %d\n",
		           mod.m_name.c_str(), m_pVehicle->GetModelIndex(), mod.m_slot);
		return false;
	}

	RpAtomic *atomic = RpAtomicClone(srcAtomic);
	RwFrame *frame = RwFrameCreate();
	// Матрица детали: у клиента она собирается из dummy (0x16a208c в
	// 0x17f9174). Здесь берём матрицу самого dummy — этого достаточно,
	// чтобы деталь встала в точку крепления.
	RwFrameTransform(frame, RwFrameGetMatrix(dummy), rwCOMBINEREPLACE);

	// Метка слота на кадре: по ней Remove находит именно свою деталь.
	char slotName[16];
	snprintf(slotName, sizeof(slotName), "%d", mod.m_slot);
	SetFrameNodeName(frame, slotName);

	RpAtomicSetFrame(atomic, frame);
	RwFrameAddChild(dummy, frame);
	RpClumpAddAtomic(vehicleClump, atomic);

	m_aInstalled.push_back(&mod);
	CUSTOM_LOG("carmodmenu: installed '%s' (uniqueId %d, slot %d, model %d) on '%s'\n",
	           mod.m_name.c_str(), mod.m_uniqueId, mod.m_slot, mod.m_modelId, mod.m_name.c_str());
	return true;
}

// Снятие: убирает атомик, который Install повесил на dummy, и освобождает кадр.
// Учёт в пуле чистится по слоту.
void
CCarModMenu::Remove(const CCarModComponent &mod)
{
	if(m_pVehicle == nil || m_pVehicle->m_rwObject == nil)
		return;

	RpClump *vehicleClump = (RpClump*)m_pVehicle->m_rwObject;
	if(RwObjectGetType((RwObject*)vehicleClump) != rpCLUMP)
		return;

	SRemoveCtx ctx;
	ctx.root = RpClumpGetFrame(vehicleClump);
	ctx.slot = mod.m_slot;
	ctx.found = nil;
	RpClumpForAllAtomics(vehicleClump, RemoveFindCB, &ctx);

	RpAtomic *found = ctx.found;
	if(found){
		RpClumpRemoveAtomic(vehicleClump, found);
		RwFrame *f = RpAtomicGetFrame(found);
		if(f)
			RwFrameDestroy(f);
		RpAtomicDestroy(found);
	}

	CCarModComponentPool::RemoveFromVehicle(*m_pVehicle, mod.m_slot);

	m_aInstalled.erase(std::remove(m_aInstalled.begin(), m_aInstalled.end(), &mod),
	                   m_aInstalled.end());
	CUSTOM_LOG("carmodmenu: removed '%s' (slot %d)\n", mod.m_name.c_str(), mod.m_slot);
}

void
CCarModMenu::ToggleSelected(void)
{
	if(m_nSelected < 0 || m_nSelected >= (int32)m_aCandidates.size())
		return;

	const CCarModComponent *mod = m_aCandidates[m_nSelected];
	bool installed = false;
	for(size_t i = 0; i < m_aInstalled.size(); i++)
		if(m_aInstalled[i] == mod)
			installed = true;

	if(installed)
		Remove(*mod);
	else
		Install(*mod);
}

int32
CCarModMenu::GetCount(void)
{
	return (int32)m_aCandidates.size();
}

int32
CCarModMenu::GetSelected(void)
{
	return m_nSelected;
}

int32
CCarModMenu::GetFirstVisible(void)
{
	return m_nFirstVisible;
}

void
CCarModMenu::StepSelection(int32 delta)
{
	MoveSelection(delta);
}

void
CCarModMenu::ActivateSelected(void)
{
	ToggleSelected();
}

// Прокрутка списка стрелками панели (по 6 строк, как у спавнера машин).
void
CCarModMenu::ToggleFirstVisible(int32 delta)
{
	int32 count = (int32)m_aCandidates.size();
	if(count <= MAX_VISIBLE)
		return;
	m_nFirstVisible += delta * MAX_VISIBLE;
	if(m_nFirstVisible < 0)
		m_nFirstVisible = 0;
	if(m_nFirstVisible > count - MAX_VISIBLE)
		m_nFirstVisible = count - MAX_VISIBLE;
	if(m_nSelected < m_nFirstVisible)
		m_nSelected = m_nFirstVisible;
	if(m_nSelected >= m_nFirstVisible + MAX_VISIBLE)
		m_nSelected = m_nFirstVisible + MAX_VISIBLE - 1;
}

void
CCarModMenu::ClosePanel(void)
{
	Close();
}

// Описание строки: имя детали, слот и признак «стоит».
void
CCarModMenu::GetRowText(int32 index, char *out, int32 outSize)
{
	if(index < 0 || index >= (int32)m_aCandidates.size()){
		if(outSize > 0) out[0] = '\0';
		return;
	}

	const CCarModComponent *mod = m_aCandidates[index];
	bool installed = false;
	for(size_t i = 0; i < m_aInstalled.size(); i++)
		if(m_aInstalled[i] == mod)
			installed = true;

	snprintf(out, outSize, "%s%s  slot %d", installed ? "* " : "",
	         mod->m_name.c_str(), mod->m_slot);
}

// Панель закрывается сама, если машина под игроком сменилась или пропала:
// деталь ставится в конкретную машину, и держать список от другой нет смысла.
void
CCarModMenu::Process(void)
{
	if(!m_bOpen)
		return;

	if(m_pVehicle == nil || FindPlayerVehicle() != m_pVehicle){
		Close();
		return;
	}
}
