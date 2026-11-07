#pragma once

#include "common.h"

class CPad;
class CSprite2d;

class CMobileTouchControls
{
public:
	static void Initialise(void);
	static void ReloadTXD(void);
	static void Shutdown(void);

	static void UpgradeHudSprites(CSprite2d *hudSprites, int32 numSprites);
	static void UpgradeRadarSprites(void);
	static RwTexture *GetWeaponIcon(int32 weaponType, const char *modelName);
	static bool HasCustomHudTxd(void);

	static void OnFingerDown(int64 fingerId, float normX, float normY);
	static void OnFingerMotion(int64 fingerId, float normX, float normY, float normDX, float normDY);
	static void OnFingerUp(int64 fingerId, float normX, float normY);
	static void ResetTouches(void);

	static void ConsumeLookDelta(int *dx, int *dy);
	static void ApplyKeyState(void);
	static void UpdatePad(CPad *pad);
	static void Draw(void);
};
