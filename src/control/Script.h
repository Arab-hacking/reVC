#ifndef __GTA_SCRIPT_H__
#define __GTA_SCRIPT_H__
#include "Font.h"
#include "UpsideDownCarCheck.h"
#include "StuckCarCheck.h"
#include "Ped.h"
#include "PedType.h"
#include "Text.h"
#include "Sprite2d.h"

class CEntity;
class CBuilding;
class CPhysical;
class CVehicle;
class CPed;
class CObject;
class CPlayerInfo;

class CRunningScript;

extern int32 ScriptParams[32];

// script_assert used to flush the opcode trace log before aborting. The trace
// is gone with the interpreter, so there is nothing to flush; it stays inline
// because MSVC otherwise emits a reference to a symbol that no longer exists.
inline void FlushLog() {}
#define script_assert(_Expression) FlushLog(); assert(_Expression);

#define PICKUP_PLACEMENT_OFFSET (0.5f)
#define PED_FIND_Z_OFFSET (5.0f)
#define COP_PED_FIND_Z_OFFSET (10.0f)

// The upside-down thresholds belong to CUpsideDownCarCheck itself now; SA tests
// the contact-wheel count, not a move/turn speed tolerance.

#define SPHERE_MARKER_R (252)
#define SPHERE_MARKER_G (138)
#define SPHERE_MARKER_B (242)
#define SPHERE_MARKER_A (228)
#define SPHERE_MARKER_PULSE_PERIOD 2048
#define SPHERE_MARKER_PULSE_FRACTION 0.1f

#ifdef USE_PRECISE_MEASUREMENT_CONVERTION
#define MILES_IN_METER (0.000621371192f)
#define METERS_IN_FOOT (0.3048f)
#define FEET_IN_METER (3.28084f)
#else
#define MILES_IN_METER (1 / 1670.f)
#define METERS_IN_FOOT (0.3f)
#define FEET_IN_METER (3.33f)
#endif

#define KEY_LENGTH_IN_SCRIPT (8)

//#define GTA_SCRIPT_COLLECTIVE

struct intro_script_rectangle 
{
	bool m_bIsUsed;
	bool m_bBeforeFade;
	int16 m_nTextureId;
	CRect m_sRect;
	CRGBA m_sColor;
	
	intro_script_rectangle() { }
	~intro_script_rectangle() { }
};

VALIDATE_SIZE(intro_script_rectangle, 0x18);

enum {
	SCRIPT_TEXT_MAX_LENGTH = 100
};

struct intro_text_line 
{
	float m_fScaleX;
	float m_fScaleY;
	CRGBA m_sColor;
	bool m_bJustify;
	bool m_bCentered;
	bool m_bBackground;
	bool m_bBackgroundOnly;
	float m_fWrapX;
	float m_fCenterSize;
	CRGBA m_sBackgroundColor;
	bool m_bTextProportional;
	bool m_bTextBeforeFade;
	bool m_bRightJustify;
	int32 m_nFont;
	float m_fAtX;
	float m_fAtY;
	wchar m_Text[SCRIPT_TEXT_MAX_LENGTH];

	intro_text_line() { }
	~intro_text_line() { }
	
	void Reset()
	{
		m_fScaleX = 0.48f;
		m_fScaleY = 1.12f;
		m_sColor = CRGBA(225, 225, 225, 255);
		m_bJustify = false;
		m_bRightJustify = false;
		m_bCentered = false;
		m_bBackground = false;
		m_bBackgroundOnly = false;
		m_fWrapX = 182.0f;
		m_fCenterSize = DEFAULT_SCREEN_WIDTH;
		m_sBackgroundColor = CRGBA(128, 128, 128, 128);
		m_bTextProportional = true;
		m_bTextBeforeFade = false;
		m_nFont = FONT_STANDARD;
		m_fAtX = 0.0f;
		m_fAtY = 0.0f;
		memset(&m_Text, 0, sizeof(m_Text));
	}
};

VALIDATE_SIZE(intro_text_line, 0x414);

struct script_sphere_struct
{
	bool m_bInUse;
	uint16 m_Index;
	uint32 m_Id;
	CVector m_vecCenter;
	float m_fRadius;
	
	script_sphere_struct() { }
};

struct CStoredLine
{
	CVector vecInf;
	CVector vecSup;
	uint32 color1;
	uint32 color2;
};

enum {
	CLEANUP_UNUSED = 0,
	CLEANUP_CAR,
	CLEANUP_CHAR,
	CLEANUP_OBJECT
};

struct cleanup_entity_struct
{
	uint8 type;
	int32 id;
};

enum {
	MAX_CLEANUP = 50
};

class CMissionCleanup
{
public:
	cleanup_entity_struct m_sEntities[MAX_CLEANUP];
	uint8 m_nCount;

	CMissionCleanup();

	void Init();
	cleanup_entity_struct* FindFree();
	void AddEntityToList(int32, uint8);
	void RemoveEntityFromList(int32, uint8);
	void Process();
	void CheckIfCollisionHasLoadedForMissionObjects();
};

// CUpsideDownCarCheck and CStuckCarCheck used to live here as Vice City
// structures; they are San Andreas classes of their own now, in
// vehicles/UpsideDownCarCheck.h and vehicles/StuckCarCheck.h.

enum {
	ARGUMENT_END = 0,
	ARGUMENT_INT32,
	ARGUMENT_GLOBALVAR,
	ARGUMENT_LOCALVAR,
	ARGUMENT_INT8,
	ARGUMENT_INT16,
	ARGUMENT_FLOAT
};

struct tCollectiveData
{
	int32 colIndex;
	int32 pedIndex;
};

enum {
	USED_OBJECT_NAME_LENGTH = 24
};

struct tUsedObject
{
	char name[USED_OBJECT_NAME_LENGTH];
	int32 index;
};

struct tBuildingSwap
{
	CBuilding* m_pBuilding;
	int32 m_nNewModel;
	int32 m_nOldModel;
};


enum {
	MAX_STACK_DEPTH = 6,
	NUM_LOCAL_VARS = 16,
	NUM_TIMERS = 2
};

class CRunningScript
{
	enum {
		ANDOR_NONE = 0,
		ANDS_1 = 1,
		ANDS_2,
		ANDS_3,
		ANDS_4,
		ANDS_5,
		ANDS_6,
		ANDS_7,
		ANDS_8,
		ORS_1 = 21,
		ORS_2,
		ORS_3,
		ORS_4,
		ORS_5,
		ORS_6,
		ORS_7,
		ORS_8
	};

public:
	CRunningScript* next;
	CRunningScript* prev;
	char m_abScriptName[8];
	uint32 m_nIp;
	uint32 m_anStack[MAX_STACK_DEPTH];
	uint16 m_nStackPointer;
	int32 m_anLocalVariables[NUM_LOCAL_VARS + NUM_TIMERS];
	bool m_bIsActive;
	bool m_bCondResult;
	bool m_bIsMissionScript;
	bool m_bSkipWakeTime;
	uint32 m_nWakeTime;
	uint16 m_nAndOrState;
	bool m_bNotFlag;
	bool m_bDeatharrestEnabled;
	bool m_bDeatharrestExecuted;
	bool m_bMissionFlag;

public:
	void SetIP(uint32 ip) { m_nIp = ip; }
	CRunningScript* GetNext() const { return next; }


	void UpdateTimers(float timeStep) {
		m_anLocalVariables[NUM_LOCAL_VARS] += timeStep;
		m_anLocalVariables[NUM_LOCAL_VARS + 1] += timeStep;
	}

	void Init();
	void Process();

	void RemoveScriptFromList(CRunningScript**);
	void AddScriptToList(CRunningScript**);


	void CollectParameters(uint32*, int16);
	int32 CollectNextParameterWithoutIncreasingPC(uint32);
	int32* GetPointerToScriptVariable(uint32*, int16);
	void StoreParameters(uint32*, int16);

	int8 ProcessOneCommand();
	int8 ProcessWeatherCommands(int32 command);
	void UpdateCompareFlag(bool);



	float LimitAngleOnCircle(float angle) { return angle < 0.0f ? angle + 360.0f : angle; }


	
};


enum {
	VAR_LOCAL = 1,
	VAR_GLOBAL = 2,
};

enum {
#ifdef PS2
	SIZE_MAIN_SCRIPT = 205512,
#else
	SIZE_MAIN_SCRIPT = 225512,
#endif
	SIZE_MISSION_SCRIPT = 35000,
	SIZE_SCRIPT_SPACE = SIZE_MAIN_SCRIPT + SIZE_MISSION_SCRIPT
};

enum {
	MAX_NUM_SCRIPTS = 128,
	MAX_NUM_INTRO_TEXT_LINES = 48,
	MAX_NUM_INTRO_RECTANGLES = 16,
	MAX_NUM_SCRIPT_SRPITES = 16,
	MAX_NUM_SCRIPT_SPHERES = 16,
	MAX_NUM_USED_OBJECTS = 220,
	MAX_NUM_MISSION_SCRIPTS = 120,
	MAX_NUM_BUILDING_SWAPS = 25,
	MAX_NUM_INVISIBILITY_SETTINGS = 20,
	MAX_NUM_STORED_LINES = 1024
};

class CTheScripts
{
public:
	static uint8 ScriptSpace[SIZE_SCRIPT_SPACE];
	static CRunningScript ScriptsArray[MAX_NUM_SCRIPTS];
	static intro_text_line IntroTextLines[MAX_NUM_INTRO_TEXT_LINES];
	static intro_script_rectangle IntroRectangles[MAX_NUM_INTRO_RECTANGLES];
	static CSprite2d ScriptSprites[MAX_NUM_SCRIPT_SRPITES];
	static script_sphere_struct ScriptSphereArray[MAX_NUM_SCRIPT_SPHERES];
	static tUsedObject UsedObjectArray[MAX_NUM_USED_OBJECTS];
	static CStoredLine aStoredLines[MAX_NUM_STORED_LINES];
	static bool DbgFlag;
	static uint32 OnAMissionFlag;
	static CMissionCleanup MissionCleanUp;
	static CStuckCarCheck StuckCars;
	static CUpsideDownCarCheck UpsideDownCars;
	static CRunningScript *pIdleScripts;
	static CRunningScript *pActiveScripts;
	static bool bAlreadyRunningAMissionScript;
	static bool bUsingAMultiScriptFile;
	static uint16 NumberOfIntroRectanglesThisFrame;
	static uint16 NumberOfIntroTextLinesThisFrame;
	static uint32 LastMissionPassedTime;
	static uint16 NumScriptDebugLines;
	static bool bPlayerIsInTheStatium;
	static uint8 RiotIntensity;
	static bool bPlayerHasMetDebbieHarry;

	static void Init();
	static void Process();
	static void SpawnInitialPlayer();

	static bool IsPlayerOnAMission();
	static void ClearSpaceForMissionEntity(const CVector&, CEntity*);


	// Not script debugging any more: the radar draws its markers through this
	// small line buffer, so it stays.
	static void ScriptDebugLine3D(float x1, float y1, float z1, float x2, float y2, float z2, uint32 col, uint32 col2);
	static void RenderTheScriptDebugLines();


	static bool IsDebugOn() { return DbgFlag; };
	static void InvertDebugFlag() { DbgFlag = !DbgFlag; }

	static int32* GetPointerToScriptVariable(int32 offset) { assert(offset >= 8 && offset < CTheScripts::GetSizeOfVariableSpace()); return (int32*)&ScriptSpace[offset]; }

	static int32 Read4BytesFromScript(uint32* pIp) {
		int32 retval = ScriptSpace[*pIp + 3] << 24 | ScriptSpace[*pIp + 2] << 16 | ScriptSpace[*pIp + 1] << 8 | ScriptSpace[*pIp];
		*pIp += 4;
		return retval;
	}
	static int16 Read2BytesFromScript(uint32* pIp) {
		int16 retval = ScriptSpace[*pIp + 1] << 8 | ScriptSpace[*pIp];
		*pIp += 2;
		return retval;
	}
	static int8 Read1ByteFromScript(uint32* pIp) {
		int8 retval = ScriptSpace[*pIp];
		*pIp += 1;
		return retval;
	}
	static float ReadFloatFromScript(uint32* pIp) {
		return Read2BytesFromScript(pIp) / 16.0f;
	}
	static void ReadTextLabelFromScript(uint32* pIp, char* buf) {
		strncpy(buf, (const char*)&CTheScripts::ScriptSpace[*pIp], KEY_LENGTH_IN_SCRIPT);
	}
	static wchar* GetTextByKeyFromScript(uint32* pIp) {
		wchar* text = TheText.Get((const char*)&CTheScripts::ScriptSpace[*pIp]);
		*pIp += KEY_LENGTH_IN_SCRIPT;
		return text;
	}
	static int32 GetSizeOfVariableSpace()
	{
		uint32 tmp = 3;
		return Read4BytesFromScript(&tmp);
	}

	static CRunningScript* StartNewScript(uint32);





	static void RemoveScriptTextureDictionary();
public:
	static void RemoveThisPed(CPed* pPed);
	static void CleanUpThisObject(CObject* pObject);

	static uint32& GetLastMissionPassedTime() { return LastMissionPassedTime; }


#ifdef USE_DEBUG_SCRIPT_LOADER
	static int ScriptToLoad;
	static int OpenScript();
#endif

};


#endif // __GTA_SCRIPT_H__
