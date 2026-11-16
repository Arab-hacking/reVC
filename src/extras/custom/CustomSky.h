#ifndef __GTA_CUSTOMSKY_H__
#define __GTA_CUSTOMSKY_H__

class CCustomSky
{
public:
	enum Mode {
		MODE_AUTO = 0,
		MODE_SKYBOX = 1,
		MODE_SKYDOME = 2,
		MODE_OFF = 3
	};

	// Auto-registers missing IDE slots for BR_Sky_* (rip) and BR_SkyDome (SkyDome)
	// before CStreaming::Init() indexes custom/img/ archives.
	static void RegisterModels(void);
	static void InitAfterStreaming(void);
	static void Shutdown(void);

	// Renders active 3D SkyBox or SkyDome centered on camPos.
	// Returns true when 3D sky was drawn (caller skips 2D horizon/cloud bands).
	static bool Render(const CVector &camPos, bool forEnvMap = false);
	static bool IsActive(void);

	// Shift + Num* cycles available modes (Auto/SkyBox/SkyDome).
	static Mode CycleMode(void);
	static Mode GetMode(void);
	static const char *GetModeName(Mode mode);
};

#endif
