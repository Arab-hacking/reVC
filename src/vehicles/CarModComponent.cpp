#include "common.h"
#include "main.h"

#include "General.h"
#include "CarModComponent.h"
#include "Vehicle.h"
#include "ModelInfo.h"
#include "FileMgr.h"
#include "Streaming.h"
#include "SavehDiag.h"
#include "CustomModels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>

// Пул компонентов тюнинга. В клиенте это синглтон (аксессор 0x17f61f4,
// объект 0x3aea328), который лениво наполняет 0x17fa3f4: обнуляет 3500
// флагов и зовёт загрузчик carmods.dat.
static CCarModComponent g_carModPool[CAR_MOD_POOL_SIZE];
static std::vector<CCarModComponent*> g_carModGlobalVector;
static std::map<const CVehicle*, std::vector<const CCarModComponent*>> g_vehicleMods;
static bool g_bCarModsLoaded;

CCarModComponent::CCarModComponent(void)
{
	m_uniqueId = 0;
	m_type = 0;
	m_slot = 0;
	m_modelId = 0;
	m_bHiddenFromWheel = false;
	m_bInitialised = false;
}

CCarModComponent::CCarModComponent(int32 uniqueId, int32 type, int32 slot,
                                   int32 modelId, const char *name,
                                   std::vector<std::string> &&extraTokens,
                                   bool bHiddenFromWheel)
{
	Init(uniqueId, type, slot, modelId, name, std::move(extraTokens), bHiddenFromWheel);
}

// Порядок записи повторяет 0x17f63b8:
//   stp w1,w2,[x19]    -> +0x00 uniqueId, +0x04 type
//   stp w3,w4,[x19,#8] -> +0x08 slot,     +0x0c modelId
// дальше строка имени (+0x10), вектор токенов (+0x28) и bool (+0x40).
void
CCarModComponent::Init(int32 uniqueId, int32 type, int32 slot, int32 modelId,
                       const char *name, std::vector<std::string> &&extraTokens,
                       bool bHiddenFromWheel)
{
	m_uniqueId = uniqueId;
	m_type = type;
	m_slot = slot;
	m_modelId = modelId;
	m_name = name ? name : "";
	m_extraTokens = std::move(extraTokens);
	m_bHiddenFromWheel = bHiddenFromWheel;
	m_bInitialised = true;
}

// Обе проверки конструктора:
//   0x17f64d4  bl 0xa79144 (GetModelInfo); cbz -> assert 113
//   0x17f64e0  ldr w8,[x19,#8]; cmp w8,#0x2d; b.hs -> assert 114
bool
CCarModComponent::IsValid(void) const
{
	if(CModelInfo::GetModelInfo(m_modelId) == nil)
		return false;
	if(m_slot < 0 || m_slot >= MAX_CAR_MOD_SLOTS)
		return false;
	return true;
}

CCarModComponent *
CCarModComponentPool::GetPool(void)
{
	return g_carModPool;
}

// Индексация загрузчика (§4.7.5):
//   smaddl x25, w1, w9, x8   ; elem = base + uniqueId * 0x50
// Своей проверки границы в клиенте нет: её даёт ассерт
// 'carmods: max carmods uniqueId %d' (уровень 4) в 0x17fc074.
CCarModComponent *
CCarModComponentPool::Get(int32 uniqueId)
{
	if(uniqueId < 0 || uniqueId >= CAR_MOD_POOL_SIZE)
		return nil;
	return &g_carModPool[uniqueId];
}

std::vector<CCarModComponent*> &
CCarModComponentPool::GetGlobalVector(void)
{
	return g_carModGlobalVector;
}

// 0x17fa3f4: обнуление флагов и загрузка, ровно в этом порядке

// Клиентские данные лежат в custom/: пробуем custom/data/<name> (и common.zip),
// и только потом старый путь DATA/<NAME>. Раньше загрузчик смотрел только в
// DATA/, поэтому при установленном клиентском пакете carmods.dat "не находился".
static bool
ReadCarModsFromCustom(const char *relPath, std::string &out)
{
	std::vector<uint8> data;
	if(!CCustomModels::ReadDataFile(relPath, data, nil))
		return false;
	if(data.empty())
		return false;
	out.assign((const char*)data.data(), data.size());
	return true;
}

// Построчный ридер по уже прочитанному тексту. fmemopen требует _GNU_SOURCE,
// поэтому обходимся без него: загрузчик принимает любую из двух реализаций.
struct CarModsTextReader
{
	const std::string *text;
	size_t pos;
	bool Read(char *line, int maxLen);
};

bool
CarModsTextReader::Read(char *line, int maxLen)
{
	if(text == nil || pos >= text->size() || maxLen <= 1)
		return false;
	const size_t begin = pos;
	size_t end = text->find('\n', begin);
	if(end == std::string::npos)
		end = text->size();
	size_t len = end - begin;
	if(len > (size_t)(maxLen - 1))
		len = (size_t)(maxLen - 1);
	memcpy(line, text->data() + begin, len);
	line[len] = '\0';
	pos = (end < text->size()) ? end + 1 : text->size();
	return true;
}

// (0x17fa41c -> bl 0x17fbbb0).
void
CCarModComponentPool::Load(void)
{
	if(g_bCarModsLoaded)
		return;

	for(int i = 0; i < CAR_MOD_POOL_SIZE; i++){
		g_carModPool[i].m_uniqueId = i;
		g_carModPool[i].m_bInitialised = false;
	}

	LoadCarModsDat();
	g_bCarModsLoaded = true;
}

// 0x17fbbb0: первым делом legacy-файл, затем основной.
//   0x17fbbe8  bl LoadLegacyVehicleUpgrades
//   0x17fbbec  tbz w0,#0 -> другая ветка
bool
CCarModComponentPool::LoadCarModsDat(void)
{
	LoadLegacyVehicleUpgrades();

	const int MAX_LINE = 256; // клиент: getline(поток, 1, 0x100)
	int fd;
	char line[MAX_LINE];
	uint32 lineNum = 0;

	// Клиентский пакет кладёт файл в custom/data/ (иногда внутрь common.zip),
	// поэтому сначала смотрим туда и только потом в старый DATA/, прописными.
	std::string customText;
	CarModsTextReader customReader = { nil, 0 };
	const bool fromCustom = ReadCarModsFromCustom("data/carmods.dat", customText);
	if(fromCustom){
		customReader.text = &customText;
		CUSTOM_LOG("carmods.dat: read from custom/data/carmods.dat (%u bytes)\n",
		           (unsigned)customText.size());
	}else{
		CFileMgr::ChangeDir("\\DATA\\");
		fd = CFileMgr::OpenFile("CARMODS.DAT", "r");
		CFileMgr::ChangeDir("\\");

		// Нет файла — не ошибка: данных может не быть вовсе, порт работает молча.
		if(fd == 0)
			return false;
	}

	while(fromCustom ? customReader.Read(line, MAX_LINE)
	                 : CFileMgr::ReadLine(fd, line, MAX_LINE)){
		const char *tok = line;
		lineNum++;

		// 0x17fbda4  cmp w8,#0x23 ; '#' -> комментарий
		if(tok[0] == '#')
			continue;

		// Формат 0x41d186 = "%d %d %d %s %d %n". Клиент требует БОЛЬШЕ
		// четырёх полей (0x17fbdec cmp w0,#4; b.gt), иначе
		// 'Wrong number of car mod component params in line %u. Skipping'.
		int uniqueId, type, slot, modelId, consumed = 0;
		char name[64];
		name[0] = '\0';
		int n = sscanf(tok, "%d %d %d %63s %d %n",
		               &uniqueId, &type, &slot, name, &modelId, &consumed);
		if(n <= 4){
			CUSTOM_LOG("carmods: wrong number of car mod component params in line %u. Skipping\n", lineNum);
			continue;
		}

		// 0x17fbe14..0x17fbefc: остаток строки добирается тем же форматом,
		// пока sscanf не вернёт меньше одного поля. Клиент складывает эти
		// токены в вектор [sp+0x40] и передаёт конструктору шестым
		// аргументом (§4.7.4).
		std::vector<std::string> extra;
		while(tok[consumed] != '\0'){
			char extraTok[64];
			int consumed2 = 0;
			if(sscanf(tok + consumed, "%63s%n", extraTok, &consumed2) < 1)
				break;
			extra.push_back(extraTok);
			consumed += consumed2;
		}

		AddUnique(uniqueId, type, slot, modelId, name, std::move(extra));
	}

	if(!fromCustom)
		CFileMgr::CloseFile(fd);
	return true;
}

// Вставка записи в пул, 0x17fc178..0x17fc1e8:
//   ldrb w8,[x25,#0x48]  ; запись занята?
//   cbz  w8 -> конструктор
//   иначе освободить её, сбросить флаг и
//          'uniqueid %d is already initialized in carmods.dat or legacy carmods' (3)
void
CCarModComponentPool::AddUnique(int32 uniqueId, int32 type, int32 slot,
                                int32 modelId, const char *name,
                                std::vector<std::string> &&extra)
{
	// 0x17fc074: 'carmods: max carmods uniqueId %d', уровень 4.
	if(uniqueId < 0 || uniqueId >= CAR_MOD_POOL_SIZE){
		CUSTOM_LOG("carmods: max carmods uniqueId %d\n", uniqueId);
		return;
	}

	CCarModComponent *elem = &g_carModPool[uniqueId];

	if(elem->m_bInitialised){
		CUSTOM_LOG("uniqueid %d is already initialized in carmods.dat or legacy carmods\n", uniqueId);
		elem->m_bInitialised = false;
	}

	// 0x17fc164..0x17fc174: GetModelInfo ДО конструктора, чтобы не портить
	// пул; иначе 'Vehicle model "%d" is not registered. Skipping'.
	if(CModelInfo::GetModelInfo(modelId) == nil){
		CUSTOM_LOG("Vehicle model \"%d\" is not registered. Skipping\n", modelId);
		return;
	}

	elem->Init(uniqueId, type, slot, modelId, name, std::move(extra), false);
}

// 0x17fb074 LoadLegacyVehicleUpgrades (480 инстр.), формат §4.9.3:
//   # комментарий
//   link
//   <слово> <слово>        -- %s %s, назначение в клиенте не снято
//   mods
//   ...
//   wheel
//   <токены>
//   end
bool
CCarModComponentPool::LoadLegacyVehicleUpgrades(void)
{
	enum {
		SECTION_NONE  = 0,
		SECTION_LINK  = 1,
		SECTION_MODS  = 2,
		SECTION_WHEEL = 3,
	};

	const int MAX_LINE = 256;
	int fd;
	char line[MAX_LINE];
	int section = SECTION_NONE;

	// Клиентский пакет кладёт файл в custom/data/, поэтому сначала смотрим
	// туда (включая common.zip), и только затем — в старый DATA/.
	std::string customText;
	CarModsTextReader customReader = { nil, 0 };
	const bool fromCustom = ReadCarModsFromCustom("data/carmods-legacy.dat", customText);
	if(fromCustom){
		customReader.text = &customText;
		CUSTOM_LOG("carmods-legacy: read from custom/data/carmods-legacy.dat (%u bytes)\n",
		           (unsigned)customText.size());
	}else{
		CFileMgr::ChangeDir("\\DATA\\");
		fd = CFileMgr::OpenFile("CARMODS-LEGACY.DAT", "r");
		CFileMgr::ChangeDir("\\");
		if(fd == 0)
			return false;
	}

	while(fromCustom ? customReader.Read(line, MAX_LINE)
	                 : CFileMgr::ReadLine(fd, line, MAX_LINE)){
		char *p = line;

		while(*p == ' ' || *p == '\t')
			p++;

		// Первое слово строки сравнивается с именами секций (§4.9.3).
		char word[64];
		int wi = 0;
		while(p[wi] > ' ' && wi < (int)sizeof(word) - 1){
			word[wi] = p[wi];
			wi++;
		}
		word[wi] = '\0';
		p += wi;

		if(word[0] == '#' || word[0] == '\0')
			continue;

		// Таблица секций: link(1), mods(2), wheel(3), end(0). Имена 'wheel'
		// и 'end' в клиенте собраны из иммедиатов, поэтому поиском по
		// строкам их не найти (§4.9.2).
		if(strcmp(word, "link") == 0){ section = SECTION_LINK; continue; }
		if(strcmp(word, "mods") == 0){ section = SECTION_MODS; continue; }
		if(strcmp(word, "wheel") == 0){ section = SECTION_WHEEL; continue; }
		if(strcmp(word, "end") == 0){ section = SECTION_NONE; continue; }

		while(*p == ' ' || *p == '\t')
			p++;

		switch(section){
		case SECTION_LINK:{
			// 0x17fb250: sscanf(p, "%s %s", ...). Куда клиент кладёт
			// результат, в главе 04 не снято (§4.13 п.2), поэтому здесь
			// только проверка, что пара действительно читается.
			char a[64], b[64];
			if(sscanf(p, "%63s %63s", a, b) != 2)
				CUSTOM_LOG("carmods-legacy: invalid link line\n");
			break;
		}
		case SECTION_MODS:
			// Переключатель клиента для 'mods' уводит на следующую строку
			// (0x17fb248 cbnz w28 -> 0x17fb194): отдельной обработки нет.
			break;
		case SECTION_WHEEL:
			// 0x17fb26c: значение разбивается на токены (0xbe7680), каждый
			// токен уходит в AddWheelUpgrade (0x17fb2d4).
			AddWheelUpgradeLine(p);
			break;
		default:
			break;
		}
	}

	if(!fromCustom)
		CFileMgr::CloseFile(fd);
	return true;
}

// 0x17faa00 AddWheelUpgrade (356 инстр.). Аргументы конструктора перед
// вызовом 0x17fac38 (0x17fac1c..0x17fac34):
//   w1 = w19            uniqueId  = число из токена
//   w2 = 0x190 (400)    type
//   w3 = 0x14  (20)     slot
//   w4 = w19            modelId   = uniqueId: имя модели совпадает с числом
//   x5 = строка имени
//   x6 = вектор доп. токенов (пуст)
//   w7 = 1              bool
// Ассерт строки 690 — uniqueId < m_carMods.size().
void
CCarModComponentPool::AddWheelUpgrade(int32 uniqueId)
{
	if(uniqueId < 0 || uniqueId >= CAR_MOD_POOL_SIZE){
		CUSTOM_LOG("carmods-legacy: max carmods uniqueId %d\n", uniqueId);
		return;
	}

	CCarModComponent *elem = &g_carModPool[uniqueId];

	if(elem->m_bInitialised){
		CUSTOM_LOG("uniqueid %d is already initialized in carmods.dat\n", uniqueId);
		elem->m_bInitialised = false;
	}

	char name[32];
	snprintf(name, sizeof(name), "%d", uniqueId);

	std::vector<std::string> empty;
	elem->Init(uniqueId, 400, 20, uniqueId, name, std::move(empty), true);
}

void
CCarModComponentPool::AddWheelUpgradeLine(const char *tokens)
{
	const char *p = tokens;

	while(*p){
		while(*p == ' ' || *p == '\t')
			p++;
		if(*p == '\0')
			break;

		char *end = nil;
		long uniqueId = strtol(p, &end, 10);
		if(end == p)
			break; // не число — разбор строки закончен
		p = end;

		AddWheelUpgrade((int32)uniqueId);
	}
}

// 0x17fa4ec AddToVehicle(const CCarModComponent&, CVehicle&, bool bAttach).
//
// Разобрано по псевдокоду (tools/client-analysis/carmod_attach.md):
//   0x17fa4ec  FUN_018fa790(pool, vehicle, mod->slot)  -- снять деталь в этом слоте
//   0x17fa504  IsModelLoaded(mod->modelId)             -- модель обязана быть в памяти
//   0x17fa590  RequestModel + ожидание до 100 попыток; иначе assert
//              "Cant load model Id %u for car mod component"
//   0x17fa6xx  if (!(mod->m_bHiddenFromWheel))  0x17f9158(mod, vehicle)
//              else                              0x17f8be0(mod, vehicle)
//
// Две ветки крепления: обычная (деталь на видимой машине) и для деталей,
// которые не показываются в колесе выбора (m_bHiddenFromWheel, +0x40).
// Обе заканчиваются записью узла 0x78 в список машины (см. carmod_attach.md).
//
// Порт переносит проверки и учёт; визуальное крепление делает CCarModMenu,
// потому что ему нужен загруженный атомик модели, а не только кламп.
bool
CCarModComponentPool::AddToVehicle(const CCarModComponent &mod, CVehicle &vehicle, bool bAttach)
{
	(void)bAttach;

	// Слот вне диапазона: клиент не привязывает такую деталь к машине.
	if(mod.m_slot < 0 || mod.m_slot >= MAX_CAR_MOD_SLOTS)
		return false;

	// Клиент сначала снимает всё, что стоит в этом слоте, и только потом
	// ставит новое. Иначе в списке машины остался бы старый узел.
	RemoveFromVehicle(vehicle, mod.m_slot);

	if(!mod.IsValid() || mod.m_modelId <= 0)
		return false;

	// Без модели в памяти клиент не крепит деталь, а грузит её и ждёт.
	if(!CStreaming::HasModelLoaded(mod.m_modelId)){
		CStreaming::RequestModel(mod.m_modelId, 0);
		return false;
	}

	g_vehicleMods[&vehicle].push_back(&mod);
	return true;
}

// 0x17f7830 RemoveFromVehicle(CCarModComponent*, CVehicle*): ищет узел по слоту
// (0x180d490 -> узел+0x08) и снимает владение с четырёх объектов (+0x08..+0x20),
// затем стирает узел (0x17f67ec).
void
CCarModComponentPool::RemoveFromVehicle(CVehicle &vehicle, int32 slot)
{
	std::map<const CVehicle*, std::vector<const CCarModComponent*>>::iterator it =
		g_vehicleMods.find(&vehicle);
	if(it == g_vehicleMods.end())
		return;

	std::vector<const CCarModComponent*> &list = it->second;
	for(size_t i = 0; i < list.size(); i++){
		if(list[i] != nil && list[i]->m_slot == slot){
			list.erase(list.begin() + i);
			return;
		}
	}
}

// Список деталей машины. Клиент держит его внутри самой машины, порт — здесь
// (см. комментарий в Vehicle.h: CVehicle не может носить std::vector, потому
// что CPool::New() отдаёт сырую память).
const std::vector<const CCarModComponent*> *
CCarModComponentPool::FindOnVehicle(const CVehicle *vehicle)
{
	std::map<const CVehicle*, std::vector<const CCarModComponent*>>::const_iterator it =
		g_vehicleMods.find(vehicle);
	if(it == g_vehicleMods.end())
		return nil;
	return &it->second;
}
