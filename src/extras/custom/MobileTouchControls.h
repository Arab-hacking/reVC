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

	static void SetDeviceDisplayMetrics(float density, float xdpi, float ydpi,
	                                    int32 safeLeft, int32 safeTop, int32 safeRight, int32 safeBottom);
	static void SetFallbackDpi(float dpi);

	static float Dp(float dp);
	static float ScaleHudPx(float ps2Units);
	static float GetSafeLeftInsetPx(void);
	static float GetSafeRightInsetPx(void);
	static float GetSafeTopInsetPx(void);
	static float GetSafeBottomInsetPx(void);
	static void GetMobileRadarRect(float *outLeft, float *outTop, float *outWidth, float *outHeight);

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

	// Native smartphone menu touch, swipe, slider scrub, stats scroll, and map pan/pinch
	static void ProcessMenuTouch(bool &goDown, bool &goUp, bool &optionSelected, bool &goBack, int8 &changeAmount);
	static float ConsumeStatsScrollDelta(void);
	static void DrawMenuOverlay(void);
};
