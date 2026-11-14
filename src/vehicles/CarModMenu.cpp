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
#include "Pad.h"
#include "Font.h"
#include "Text.h"
#include "SavehDiag.h"
#include "RwHelper.h"

#include <algorithm>

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
void
CCarModMenu::Install(const CCarModComponent &mod)
{
	if(m_pVehicle == nil)
		return;

	for(size_t i = 0; i < m_aInstalled.size(); i++)
		if(m_aInstalled[i] == &mod)
			return; // уже стоит

	CCarModComponentPool::AddToVehicle(mod, *m_pVehicle, false);
	m_aInstalled.push_back(&mod);

	// Визуальная часть: нужен загруженный кламп модели детали.
	if(!CStreaming::HasModelLoaded(mod.m_modelId)){
		CStreaming::RequestModel(mod.m_modelId, 0);
		CUSTOM_LOG("carmodmenu: model %d not loaded yet, requested\n", mod.m_modelId);
		return;
	}

	if(m_pVehicle->m_rwObject == nil)
		return;

	CClumpModelInfo *partInfo = (CClumpModelInfo*)CModelInfo::GetModelInfo(mod.m_modelId);
	if(partInfo == nil || partInfo->m_clump == nil)
		return;

	RpAtomic *srcAtomic = GetFirstAtomic(partInfo->m_clump);
	if(srcAtomic == nil)
		return;

	RpClump *vehicleClump = (RpClump*)m_pVehicle->m_rwObject;
	if(RwObjectGetType((RwObject*)vehicleClump) != rpCLUMP)
		return;

	RpAtomic *atomic = RpAtomicClone(srcAtomic);
	RwFrame *frame = RwFrameCreate();
	RwFrameTransform(frame, RwFrameGetMatrix(RpAtomicGetFrame(srcAtomic)), rwCOMBINEREPLACE);
	RpAtomicSetFrame(atomic, frame);
	RpClumpAddAtomic(vehicleClump, atomic);
	RwFrameAddChild(RpClumpGetFrame(vehicleClump), frame);

	CUSTOM_LOG("carmodmenu: installed '%s' (uniqueId %d, slot %d, model %d)\n",
	           mod.m_name.c_str(), mod.m_uniqueId, mod.m_slot, mod.m_modelId);
}

// Контекст поиска детали среди атомиков клампа машины.
struct SRemoveCtx {
	RwFrame *root;
	RpAtomic *found;
};

static RpAtomic *
RemoveFindCB(RpAtomic *atomic, void *data)
{
	SRemoveCtx *ctx = (SRemoveCtx*)data;
	if(ctx->found)
		return atomic;
	RwFrame *frame = RpAtomicGetFrame(atomic);
	if(frame && RwFrameGetParent(frame) == ctx->root)
		ctx->found = atomic;
	return atomic;
}

// Снимает деталь: убирает атомик из клампа и кадр, который мы под него создали.
void
CCarModMenu::Remove(const CCarModComponent &mod)
{
	if(m_pVehicle == nil || m_pVehicle->m_rwObject == nil)
		return;

	RpClump *vehicleClump = (RpClump*)m_pVehicle->m_rwObject;
	if(RwObjectGetType((RwObject*)vehicleClump) != rpCLUMP)
		return;

	// Ищем атомик, чей кадр — прямой потомок кадра клампа: у самой машины
	// атомики сидят на своих именованных кадрах глубже, а деталь мы вешаем
	// прямо на корневой кадр (см. Install).
	SRemoveCtx ctx;
	ctx.root = RpClumpGetFrame(vehicleClump);
	ctx.found = nil;
	RpClumpForAllAtomics(vehicleClump, RemoveFindCB, &ctx);

	RpAtomic *found = ctx.found;
	if(found){
		RpClumpRemoveAtomic(vehicleClump, found);
		RwFrame *f = RpAtomicGetFrame(found);
		if(f)
			RwFrameDestroy(f);
	}

	m_aInstalled.erase(std::remove(m_aInstalled.begin(), m_aInstalled.end(), &mod),
	                   m_aInstalled.end());
	CUSTOM_LOG("carmodmenu: removed '%s'\n", mod.m_name.c_str());
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

void
CCarModMenu::Process(void)
{
	if(!m_bOpen)
		return;

	// Машина могла исчезнуть (игрок вышел, машина удалена пулом).
	if(m_pVehicle == nil || FindPlayerVehicle() != m_pVehicle){
		Close();
		return;
	}

	CPad *pad = CPad::GetPad(0);
	if(pad->GetDPadUpJustDown())   MoveSelection(-1);
	if(pad->GetDPadDownJustDown()) MoveSelection(1);
	if(pad->GetCrossJustDown())    ToggleSelected();
	if(pad->GetSquareJustDown())   Close(); // выход
}

void
CCarModMenu::Draw(void)
{
	if(!m_bOpen)
		return;

	wchar wbuf[160];
	char buf[160];

	CFont::SetPropOn();
	CFont::SetBackgroundOff();
	CFont::SetCentreOff();
	CFont::SetColor(CRGBA(255, 220, 120, 255));

	float y = 60.0f;

	snprintf(buf, sizeof(buf), "CAR MODS  model %d  candidates %d  installed %d",
	         m_pVehicle ? (int)m_pVehicle->GetModelIndex() : -1,
	         (int)m_aCandidates.size(), (int)m_aInstalled.size());
	AsciiToUnicode(buf, wbuf);
	CFont::PrintString(40.0f, y, wbuf);
	y += 24.0f;

	if(m_aCandidates.empty()){
		AsciiToUnicode("no carmods data (DATA/CARMODS.DAT)", wbuf);
		CFont::PrintString(40.0f, y, wbuf);
		return;
	}

	int32 last = m_nFirstVisible + MAX_VISIBLE;
	if(last > (int32)m_aCandidates.size())
		last = (int32)m_aCandidates.size();

	for(int32 i = m_nFirstVisible; i < last; i++){
		const CCarModComponent *mod = m_aCandidates[i];

		bool installed = false;
		for(size_t k = 0; k < m_aInstalled.size(); k++)
			if(m_aInstalled[k] == mod)
				installed = true;

		snprintf(buf, sizeof(buf), "%c %-20.20s slot %2d id %4d model %4d",
		         installed ? '*' : ' ',
		         mod->m_name.c_str(), mod->m_slot, mod->m_uniqueId, mod->m_modelId);

		if(i == m_nSelected)
			CFont::SetColor(CRGBA(255, 255, 255, 255));
		else
			CFont::SetColor(CRGBA(170, 170, 170, 255));

		AsciiToUnicode(buf, wbuf);
		CFont::PrintString(40.0f, y, wbuf);
		y += 20.0f;
	}

	CFont::SetColor(CRGBA(120, 220, 120, 255));
	AsciiToUnicode("UP/DOWN select    CROSS install/remove    SQUARE exit", wbuf);
	CFont::PrintString(40.0f, y + 16.0f, wbuf);
}
