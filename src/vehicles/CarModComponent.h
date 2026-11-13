#ifndef __GTA_CARMODCOMPONENT_H__
#define __GTA_CARMODCOMPONENT_H__

#include "common.h"
#include "General.h"

#include <string>
#include <vector>

class CVehicle;

// Тюнинг машин в клиенте устроен не по-SA. В SA деталь — это просто int в
// m_anUpgrades[15] внутри CVehicle, а вся таблица (позиция, поворот, родитель)
// лежит в ms_vehicleDescs. У клиента наоборот: каждая деталь — самостоятельный
// объект CCarModComponent, а все они лежат в общем пуле на 3500 записей
// (глава 04, §4.7.5 и §4.8).
//
// Раскладка снята с конструктора 0x17f63b8 (158 инструкций):
//
//   +0x00  int32  uniqueId         (%d #1 строки carmods.dat)
//   +0x04  int32  m_type           (%d #2; назначение в клиенте не снято)
//   +0x08  int32  m_slot           (%d #3; обязано быть < MAX_CAR_MOD_SLOTS)
//   +0x0c  int32  m_modelId        (%d #5; проверяется через GetModelInfo)
//   +0x10  ...    m_name           (std::string, %s)
//   +0x28  ...    m_extraTokens    (std::vector<std::string>)
//   +0x40  bool   m_bHiddenFromWheel
//   +0x48  bool   m_bInitialised   (ставит загрузчик, не конструктор)
//
// Шаг пула в клиенте — 0x50 (80 байт): элемент занимает 0x49 и округляется.

enum {
	MAX_CAR_MOD_SLOTS = 45,
	CAR_MOD_POOL_SIZE = 3500,
};

class CCarModComponent
{
public:
	int32 m_uniqueId;
	int32 m_type;
	int32 m_slot;
	int32 m_modelId;
	std::string m_name;
	std::vector<std::string> m_extraTokens;
	bool m_bHiddenFromWheel;
	bool m_bInitialised;

	CCarModComponent(void);
	CCarModComponent(int32 uniqueId, int32 type, int32 slot, int32 modelId,
	                 const char *name, std::vector<std::string> &&extraTokens,
	                 bool bHiddenFromWheel);

	void Init(int32 uniqueId, int32 type, int32 slot, int32 modelId,
	          const char *name, std::vector<std::string> &&extraTokens,
	          bool bHiddenFromWheel);

	// Ассерты конструктора, строки 113 и 114: модель зарегистрирована и
	// слот меньше 45. Здесь они не валят игру, а отбраковывают запись.
	bool IsValid(void) const;

	bool IsForSlot(int32 slot) const { return m_slot == slot; }
};

class CCarModComponentPool
{
public:
	static CCarModComponent *GetPool(void);
	static CCarModComponent *Get(int32 uniqueId);

	// Вход 0x17fa3f4: обнулить 3500 флагов, затем загрузить файлы.
	static void Load(void);
	static bool LoadCarModsDat(void);
	static bool LoadLegacyVehicleUpgrades(void);

	// Крепление детали к машине, 0x17fa790.
	static void AddToVehicle(const CCarModComponent &mod, CVehicle &vehicle, bool b);

	// Детали, привязанные к конкретной машине. В клиенте они лежат в самой
	// машине (vehicle+0x5a0, шаг 0x78, поле +0x70 — слот). В порт это поле
	// переносить нельзя: CPool::New() отдаёт сырую память и не зовёт
	// конструктор, поэтому любое нетривиальное поле в CVehicle (std::vector)
	// осталось бы неинициализированным — в иерархии сущностей таких полей
	// сейчас нет вовсе. Поэтому список деталей конкретной машины ведёт пул.
	static const std::vector<const CCarModComponent*> *FindOnVehicle(const CVehicle *vehicle);

	// Глобальный вектор клиента 0x3ae9f38 (аксессор 0x17cc648): детали,
	// чей слот не попал в 45 слотов машины, живут здесь.
	static std::vector<CCarModComponent*> &GetGlobalVector(void);

private:
	static void AddUnique(int32 uniqueId, int32 type, int32 slot, int32 modelId,
	                      const char *name, std::vector<std::string> &&extra);
	static void AddWheelUpgradeLine(const char *tokens);
	static void AddWheelUpgrade(int32 uniqueId);
};

#endif // __GTA_CARMODCOMPONENT_H__
