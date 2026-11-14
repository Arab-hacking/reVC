#ifndef __GTA_CARMODMENU_H__
#define __GTA_CARMODMENU_H__

#include "common.h"
#include "General.h"

#include <string>
#include <vector>

class CVehicle;
class CCarModComponent;

// Экран тюнинга. В клиенте такого экрана нет: там детали ставятся скриптом
// через CCarModComponentPool, а интерфейса игрок не видит. Здесь — отладочный
// экран, чтобы подсистему можно было проверить в игре, пока данных и модельера
// нет.
//
// Экран не выдумывает данные: он показывает то, что реально лежит в пуле
// (DATA/CARMODS.DAT + DATA/CARMODS-LEGACY.DAT) для текущей машины, и ставит
// деталь той же последовательностью, что CVehicleModelInfo::CreateInstance
// использует для компонентов машины: клонировать атомик модели, создать кадр,
// повесить кадр на кадр клампа.
class CCarModMenu
{
public:
	enum {
		MAX_VISIBLE = 10,
	};

	static void Initialise(void);
	static void Shutdown(void);

	// Открыть/закрыть экран. Открытие сбрасывает выбор на первую деталь.
	static void Open(void);
	static void Close(void);
	static bool IsOpen(void);

	// Вызывается каждый кадр, пока экран открыт.
	static void Process(void);

	// Рисуется поверх игры (вызывается из отрисовки, как CSceneEdit::Draw).
	static void Draw(void);

private:
	static bool m_bOpen;
	static int32 m_nSelected;
	static int32 m_nFirstVisible;
	static CVehicle *m_pVehicle;

	static std::vector<const CCarModComponent*> m_aCandidates;
	static std::vector<const CCarModComponent*> m_aInstalled;

	static void BuildList(CVehicle *vehicle);
	static void MoveSelection(int32 delta);
	static void ToggleSelected(void);
	static void Install(const CCarModComponent &mod);
	static void Remove(const CCarModComponent &mod);
};

#endif // __GTA_CARMODMENU_H__
