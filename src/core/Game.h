#ifndef __GTA_GAME_H__
#define __GTA_GAME_H__

enum eLevelName {
	LEVEL_IGNORE = -1, // beware, this is only used in CPhysical's m_nZoneLevel
	LEVEL_GENERIC = 0,
	LEVEL_BEACH,
	LEVEL_MAINLAND,

	NUM_LEVELS
};

enum eAreaName {
	AREA_MAIN_MAP,
	AREA_HOTEL,
	AREA_MANSION,
	AREA_BANK,
	AREA_MALL,
	AREA_STRIP_CLUB,
	AREA_LAWYERS,
	AREA_COFFEE_SHOP,
	AREA_CONCERT_HALL,
	AREA_STUDIO,
	AREA_RIFLE_RANGE,
	AREA_BIKER_BAR,
	AREA_POLICE_STATION,
	AREA_EVERYWHERE,
	AREA_DIRT,
	AREA_BLOOD,
	AREA_OVALRING,
	AREA_MALIBU_CLUB,
	AREA_PRINT_WORKS
};

class CVehicle;

class CGame
{
public:
	static eLevelName currLevel;
	static int32 currArea;
	static bool bDemoMode;
	static bool nastyGame;
	static bool frenchGame;
	static bool germanGame;
#ifdef MORE_LANGUAGES
	static bool russianGame;
	static bool japaneseGame;
#endif
	static bool playingIntro;

#ifndef MASTER
	static CVector PlayerCoords;
	static bool8 VarUpdatePlayerCoords;
#endif

	static bool InitialiseOnceBeforeRW(void);
	static bool InitialiseRenderWare(void);
	static void ShutdownRenderWare(void);
	static bool InitialiseOnceAfterRW(void);
	static void FinalShutdown(void);
	static bool Initialise(void);
	static bool ShutDown(void);
	static void ReInitGameObjectVariables(void);
	static void ReloadIPLs(void);
	static void ShutDownForRestart(void);
	static void InitialiseWhenRestarting(void);
	static void CreatePlayer(void);
	// The one place a vehicle is created: builds the class the model's type word
	// names, puts it down unlocked and abandoned, and adds it to the world. The
	// caller has already loaded the model and chosen the spot. nil when the type
	// word names no class.
	static CVehicle *SpawnVehicle(uint32 model, CVector pos, float heading);
	static void GetSpawnPosition(CVector &pos, float &heading);
	static void Process(void);

	static void InitAfterFocusLoss(void);

	static bool IsInInterior(void) { return currArea != AREA_MAIN_MAP; }
	static bool CanSeeWaterFromCurrArea(void);
	static bool CanSeeOutSideFromCurrArea(void);
	
	// NB: these do something on PS2
	static void TidyUpMemory(bool, bool);
	static void DrasticTidyUpMemory(bool);
	static void ProcessTidyUpMemory(void);
};

inline bool IsAreaVisible(int area) { return area == CGame::currArea || area == AREA_EVERYWHERE; }

#endif // __GTA_GAME_H__
