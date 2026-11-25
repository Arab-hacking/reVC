#include "common.h"
#ifdef PS2_MENU
#include "platform.h"
#include "main.h"
#include "Timer.h"
#include "Pad.h"
#include "Sprite2d.h"
#include "Text.h"
#include "Font.h"
#include "Hud.h"
#include "MBlur.h"
#include "DMAudio.h"
#include "Streaming.h"
#include "Camera.h"
#include "Credits.h"
#include "General.h"
#include "TxdStore.h"
#include "FileMgr.h"
#include "Messages.h"
#include "Frontend_PS2.h"
#include "Stats.h"
#include "Game.h"
#include "World.h"
#include "PlayerInfo.h"
#include "FrontEndControls.h"

#define CRect_SZ(x, y, w, h) CRect(x, y, x+w, y+h)

CMenuManager FrontEndMenuManager;

// TEMP: put into header
bool DoRWStuffStartOfFrame_Horizon(int16 TopRed, int16 TopGreen, int16 TopBlue, int16 BottomRed, int16 BottomGreen, int16 BottomBlue, int16 Alpha);
bool DoRWStuffStartOfFrame(int16 TopRed, int16 TopGreen, int16 TopBlue, int16 BottomRed, int16 BottomGreen, int16 BottomBlue, int16 Alpha);
void DoRWStuffEndOfFrame(void);


#define SCRW SCREEN_WIDTH
#define SCRH SCREEN_HEIGHT
//#define X SCREEN_STRETCH_X
//#define Y SCREEN_STRETCH_Y
#define X SCREEN_SCALE_X
#define Y SCREEN_SCALE_Y

#define YF(x) Y(float(x)*(float(DEFAULT_SCREEN_HEIGHT)/float(SCREEN_HEIGHT_PAL)))
//#define X(x) ((x)/640.0f*SCRW)
//#define Y(y) ((y)/448.0f*SCRH)


static float MENU_TEXT_SIZE_X = 0.644f;
static float MENU_TEXT_SIZE_Y = 0.84f;	//0.96f;
float BUTTONTAB_TEXT_SIZE_X = 0.35f;
float BUTTONTAB_TEXT_SIZE_Y = 0.7f; //0.8f;
float PANEL_TEXT_SIZE_X = 0.8f;
float PANEL_TEXT_SIZE_Y = 1.2f;	//0.96f/0.7f;	//??

CRGBA SELECTED_TEXT_COLOR(255, 182, 48, 255);
CRGBA BACKGROUND_SPLASH_COLOR(48, 48, 48, 255);

CVector2D CONTR_DESCR_NEW_TEXTSCALE(0.4564f, 0.63f);	// 0.72
CVector2D CONFIGS_NEW_TEXTSCALE(0.49f, 0.7f);	// 0.8
CVector2D AUDIO_OUTPUT_POS(0.0f, 0.0f);
CVector2D AUDIO_RSTATION_POS(154.0f, 0.0f);
CVector2D DISPLAY_BRIGHTNESS_POS(0.0f, 0.0f);

CRGBA TEXT_COLOR(150, 110, 30, 255);
CRGBA PAD_TEXT_COLOR(200, 200, 200, 255);
CRGBA CRIM_RATING_TEXT_COLOR(255, 182, 48, 255);
CRGBA SCROLL_TEXT_COLOR(150, 110, 30, 255);
CRGBA TITLE_TEXT_COLOR(170, 130, 50, 255);
CRGBA TEXT_SHADOW_COLOR(0, 0, 0, 255);
CVector2D SHADOW_VECTOR(1.0f, 1.0f);
CRGBA SLIDER_RIGHT_COLOR(20, 94, 136, 255);
CRGBA SLIDER_LEFT_COLOR(86, 196, 255, 255);
CRGBA MENU_SELECTED_COLOR(255, 212, 88, 255);
CRGBA rgbaATC(96, 96, 96, 255);	// active text color. not constant

float BUTTONTAB_TEXT_X_SCALES[NUM_PAGES] = { 1.0f };
float PANEL_TEXT_X_SCALES[NUM_PAGES] = { 1.0f };

uint32 TimeToStopPadShaking;
bool bFrontEnd_ReloadObrTxtGxt;

extern CMenuPage MenuPage_NewGame;
CMenuPage *pActiveMenuPage;
CMenuPage *pMenuNewGame = &MenuPage_NewGame;
bool bIgnoreTriangleButton;
bool gErrorSampleTriggered;

bool gMusicPlaying;

CMenuPage MenuPage_Stats;
 CMenuLineLister MenuStats_1;
 CMenuPictureAndText MenuStats_2;	// criminal rating
CMenuPage MenuPage_Briefs;
 CMenuPictureAndText MenuBriefs_1;
 CMenuDummy MenuBriefs_2;
CMenuPage MenuPage_NewGame;
 CMenuMultiChoiceTriggered MenuNewGame_1;	// "New Game"
CMenuPage MenuPage_NewGameQuestion;
 CMenuPictureAndText MenuNewGameQ_1;	// "Start a new game?"
 CMenuMultiChoiceTriggered MenuNewGameQ_2;	// "No", "Yes"
CMenuPage MenuPage_Controls;
 CMenuPictureAndText MenuControls_3; // controller images
 CMenuPictureAndText MenuControls_6;
 CMenuPictureAndText MenuControls_4;
 CMenuPictureAndText MenuControls_7;
 CMenuMultiChoiceTriggeredAlways MenuControls_1; // "Configuration:" "Setup1", "Setup2", "Setup3", "Setup4"
 CMenuMultiChoiceTriggered MenuControls_2; // "Controller Display:" "On Foot", "In Car"
 CMenuOnOffTriggered MenuControls_5; // "Vibration:"
CMenuPageAnyMove MenuPage_Audio;
 CMenuSliderTriggered MenuAudio_1;	// "Music Volume"
 CMenuMultiChoiceTriggered MenuAudio_4;	// "Output:" "Stereo", "Mono"
 CMenuSliderTriggered MenuAudio_2;	// "SFX Volume"
 CMenuMultiChoicePicturedTriggeredAnyMove MenuAudio_3; // "Radio station select:"
CMenuPage MenuPage_Display;
 CMenuSlider MenuDisplay_1;	// "Brightness"
#ifdef GTA_PC
 CMenuOnOffTriggered MenuDisplay_2;	// "Trails:"
#else
 CMenuOnOff MenuDisplay_2;	// "Trails:"
#endif
 CMenuOnOff MenuDisplay_3;	// "Subtitles:"
 CMenuOnOff MenuDisplay_4;	// "Wide Screen:"
CMenuPage MenuPage_Language;
 CMenuMultiChoiceTriggered MenuLanguage_1;	// "English", "French", "German", "Italian", "Spanish"



VALIDATE_SIZE(CPlaceableText, 0x10);
VALIDATE_SIZE(CPlaceableShText, 0x20);
VALIDATE_SIZE(CPlaceableShTextTwoLines, 0x30);
VALIDATE_SIZE(CPlaceableShOption, 0x28);
VALIDATE_SIZE(CPlaceableShOptionTwoLines, 0x38);
VALIDATE_SIZE(CPlaceableSprite, 0x18);
VALIDATE_SIZE(CPlaceableShSprite, 0x34);
VALIDATE_SIZE(CMenuMultiChoice, 0x2CC);
VALIDATE_SIZE(CMenuMultiChoiceTriggered, 0x310);
VALIDATE_SIZE(CMenuMultiChoiceTwoLines, 0x3CC);
VALIDATE_SIZE(CMenuOnOff, 0x90);

#include "FrontendTriggers.h"

static const char* FrontendFilenames[][2] =
{
	{"fe2_mainpanel_ul",  "" },
	{"fe2_mainpanel_ur",  "" },
	{"fe2_mainpanel_dl",  "" },
	{"fe2_mainpanel_dr",  "" },
	{"fe2_mainpanel_dr2", "" },
	{"fe2_tabactive",     "" },
	{"fe_iconbrief",      "" },
	{"fe_iconstats",      "" },
	{"fe_iconcontrols",   "" },
	{"fe_iconsave",       "" },
	{"fe_iconaudio",      "" },
	{"fe_icondisplay",    "" },
	{"fe_iconlanguage",   "" },
	{"fe_controller",     "" },
	{"fe_controllersh",   "" },
	{"fe_arrows1",        "" },
	{"fe_arrows2",        "" },
	{"fe_arrows3",        "" },
	{"fe_arrows4",        "" },
	{"fe_radio1",         "" },
	{"fe_radio2",         "" },
	{"fe_radio3",         "" },
	{"fe_radio4",         "" },
	{"fe_radio5",         "" },
	{"fe_radio6",         "" },
	{"fe_radio7",         "" },
	{"fe_radio8",         "" },
	{"fe_radio9",         "" },
};

#ifdef CUTSCENE_BORDERS_SWITCH
bool CMenuManager::m_PrefsCutsceneBorders = true;
#endif

int32 CMenuManager::m_PrefsSfxVolume = 102;
int32 CMenuManager::m_PrefsMusicVolume = 102;
int32 CMenuManager::m_PrefsBrightness = 256;
bool CMenuManager::m_PrefsShowTrails = true;
bool CMenuManager::m_PrefsShowSubtitles = true;
bool CMenuManager::m_PrefsAllowNastyGame = true;

int32 CMenuManager::m_PrefsRadioStation = 0;
int32 CMenuManager::m_PrefsStereoMono = 0;
int8 CMenuManager::m_PrefsUseWideScreen = 0;
int32 CMenuManager::m_PrefsLanguage = 0;
CMenuManager::CONTRCONFIG CMenuManager::m_PrefsControllerConfig = CONFIG_1;
bool CMenuManager::m_PrefsUseVibration = false;


#ifdef GTA_PC
#include "PlayerSkin.h"
int32 CMenuManager::OS_Language = 0;
int8 CMenuManager::m_PrefsVsync = 1;
int8 CMenuManager::m_PrefsVsyncDisp = 1;
int8 CMenuManager::m_PrefsFrameLimiter = 1;
int8 CMenuManager::m_PrefsSpeakers;
int32 CMenuManager::m_ControlMethod = CONTROL_CLASSIC;
int8 CMenuManager::m_PrefsDMA = 1;
float CMenuManager::m_PrefsLOD = 1.0f;
char CMenuManager::m_PrefsSkinFile[256] = DEFAULT_SKIN_NAME;

#ifndef MASTER
bool CMenuManager::m_PrefsMarketing;
bool CMenuManager::m_PrefsDisableTutorials;
#endif // !MASTER

#ifdef MENU_MAP
bool  CMenuManager::bMenuMapActive;
float CMenuManager::fMapSize;
float CMenuManager::fMapCenterY;
float CMenuManager::fMapCenterX;
#endif

#endif


CMenuManager::CMenuManager(void)
{
	int32 i;

	SetSoundLevelsForMusicMenu();

	m_pageState = PAGESTATE_NORMAL;
	m_currentPage = PAGE_FIRST;
	m_newPage = PAGE_FIRST;
	m_bMenuActive = false;
	m_bRenderGameInMenu = false;
	m_bTexturesLoaded = false;
	m_nPageLeftTimer = 0;
	m_nPageRightTimer = 0;
	m_nChangePageTimer = 0;
	field_18 = 0;
	m_fade = 255;
	m_someAlpha = 255;
	m_position.x = 0.0f;
	m_position.y = 0.0f;
	m_nSlidingDir = SLIDE_TO_BOTTOM;
	m_nStartPauseTimer = 0;
	m_nEndPauseTimer = 0;
	m_bInitialised = false;
	m_bWantToUpdateContent = false;
	field_3C = 0;

	for(i = 0; i < NUM_PAGES; i++){
		BUTTONTAB_TEXT_X_SCALES[i] = 1.0f;
		PANEL_TEXT_X_SCALES[i] = 1.0f;
	}

#ifdef GTA_PC
	TheCamera.m_bUseMouse3rdPerson = m_ControlMethod == CONTROL_STANDARD;
	CMBlur::BlurOn = m_PrefsShowTrails;
#endif
}

void
CMenuManager::LoadAllTextures(void)
{
	int32 i;

	if(m_bTexturesLoaded)
		return;

	DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_STARTING, 0);
	DMAudio.Service();
	DoRWStuffStartOfFrame(0, 0, 0, 0, 0, 0, 255);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	CSprite2d *splash = LoadSplash(nil);
	if(splash)
		splash->Draw(CRect(0.0f, 0.0f, SCRW, SCRH), BACKGROUND_SPLASH_COLOR);
	else // doesn't exist!!
		CHud::Sprites[19].Draw(CRect(0.0f, 0.0f, SCRW, SCRH), BACKGROUND_SPLASH_COLOR);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERMIPNEAREST);
	DoRWStuffEndOfFrame();

	CFileMgr::SetDir("");
	CFileMgr::SetDir("");

	CTimer::Stop();
	CStreaming::MakeSpaceFor(60*1024);
	CStreaming::ImGonnaUseStreamingMemory();
	CGame::TidyUpMemory(false, true);
	int32 slot = CTxdStore::FindTxdSlot("frontend");
	if(slot == -1)
		slot = CTxdStore::AddTxdSlot("frontend");
	printf("LOAD frontend\n");
	CTxdStore::LoadTxd(slot, "MODELS/FRONTEND.TXD");
	CTxdStore::SetCurrentTxd(slot);
	CStreaming::IHaveUsedStreamingMemory();
	CTimer::Update();

	for(i = 0; i < NUM_SPRIRES; i++)
	{
		m_sprites[i].SetTexture(FrontendFilenames[i][0], FrontendFilenames[i][1]);
		m_sprites[i].SetAddressing(rwTEXTUREADDRESSBORDER);
	}

	m_bTexturesLoaded = true;
}

void
CMenuManager::UnloadTextures(void)
{
	int32 slot;
	int32 i;

	if ( !m_bTexturesLoaded )
		return;

	slot = CTxdStore::FindTxdSlot("frontend");
#ifdef FIX_BUGS
	for(i = 0; i < NUM_SPRIRES; i++)
		m_sprites[i].Delete();
#endif

	printf("REMOVE frontend\n");
	CTxdStore::RemoveTxd(slot);
	m_bTexturesLoaded = false;
}

void
CMenuManager::InitialiseMenusOnce(void)
{
	if(m_bInitialised)
		return;
	m_bInitialised = true;

	InitialiseChangedLanguageSettings();

	// Normal menu
	MenuPage_Stats.Initialise();
	MenuPage_Briefs.Initialise();
	MenuPage_NewGame.Initialise();
	MenuPage_NewGameQuestion.Initialise();
	MenuPage_Controls.Initialise();
	MenuPage_Audio.Initialise();
	MenuPage_Display.Initialise();
	MenuPage_Language.Initialise();


	/* Stats */

	MenuStats_1.ResetNumberOfTextLines();
	MenuStats_1.SetPosition(X(75.0f), Y(70.0f));
	MenuStats_1.m_width = X(480.0f);
	MenuStats_1.m_height = Y(274.0f);
	MenuStats_1.field_10E8 = 0;	// unknown
	MenuStats_1.m_lineSpacing = Y(20.0f);
	MenuStats_1.m_scrollSpeed = 1.0f;
	MenuStats_1.SetLinesColor(SCROLL_TEXT_COLOR);
	MenuStats_1.ResetNumberOfTextLines();
	MenuPage_Stats.AddMenu(&MenuStats_1);
	MenuStats_2.SetPosition(X(75.0f), Y(50.0f));
	MenuStats_2.SetTextsColor(CRIM_RATING_TEXT_COLOR);
	MenuPage_Stats.AddMenu(&MenuStats_2);
	MenuPage_Stats.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Stats.ActivatePage();



	/* New Game */

	MenuNewGame_1.m_numOptions = 0;
	MenuNewGame_1.SetPosition(X(220.0f), Y(110.0f));
	MenuNewGame_1.AddOption(TheText.Get("FES_NGA"), 0.0f, Y(20.0f), TriggerNewGame_Confirm, false, true);
	MenuNewGame_1.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, TEXT_COLOR);
	MenuPage_NewGame.AddMenu(&MenuNewGame_1);
	MenuPage_NewGame.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_NewGame.ActivatePage();

	/* New Game confirmation (texts and options are set by TriggerNewGame_Confirm) */

	MenuNewGameQ_1.m_numTexts = 0;
	MenuNewGameQ_1.SetPosition(X(170.0f), Y(130.0f));
	MenuNewGameQ_1.SetTextsColor(TEXT_COLOR);
	MenuNewGameQ_1.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x-20.0f), X(580.0f));
	MenuPage_NewGameQuestion.AddMenu(&MenuNewGameQ_1);
	MenuNewGameQ_2.m_numOptions = 0;
	MenuNewGameQ_2.SetPosition(X(170.0f), Y(180.0f));
	MenuNewGameQ_2.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, TEXT_COLOR);
	MenuPage_NewGameQuestion.AddMenu(&MenuNewGameQ_2);
	MenuPage_NewGameQuestion.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_NewGameQuestion.ActivatePage();

	CVector2D briefsTextScale(X(0.525f), Y(0.7f));
	CVector2D defaultTextScale1(X(MENU_TEXT_SIZE_X), Y(MENU_TEXT_SIZE_Y));

	/* Briefs */

	MenuBriefs_1.m_numTexts = 0;
	MenuBriefs_1.SetPosition(X(60.0f), Y(60.0f));
	MenuBriefs_1.SetTextsColor(TEXT_COLOR);
	MenuBriefs_1.SetNewOldTextScale(true, briefsTextScale, defaultTextScale1);
	MenuBriefs_1.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x), X(600.0f));
	MenuPage_Briefs.AddMenu(&MenuBriefs_1);
	MenuPage_Briefs.AddMenu(&MenuBriefs_2);
	MenuPage_Briefs.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Briefs.ActivatePage();


	CVector2D defaultTextScale2(X(MENU_TEXT_SIZE_X), Y(MENU_TEXT_SIZE_Y));
	CVector2D defaultTextScale3(X(MENU_TEXT_SIZE_X), Y(MENU_TEXT_SIZE_Y));
	CVector2D CONTR_DESCR_NEW_TEXTSCALE_scaled(X(CONTR_DESCR_NEW_TEXTSCALE.x), Y(CONTR_DESCR_NEW_TEXTSCALE.y));
	CVector2D CONFIGS_NEW_TEXTSCALE_scaled(X(CONFIGS_NEW_TEXTSCALE.x), Y(CONFIGS_NEW_TEXTSCALE.y));

	/* Controls */

	MenuControls_3.m_numTexts = 0;
	MenuControls_3.m_numSprites = 0;
	MenuControls_3.SetPosition(X(170.0f), Y(88.0f));
	MenuControls_3.AddPicture(&m_sprites[FE_CONTROLLER],
		&m_sprites[FE_CONTROLLERSH],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_3.AddPicture(&m_sprites[FE_ARROWS1],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_3.SetNewOldTextScale(true, CONTR_DESCR_NEW_TEXTSCALE_scaled, defaultTextScale2);
	InitialiseTextsInMenuControllerOnFoot(&MenuControls_3, CMenuManager::m_PrefsControllerConfig);
	MenuControls_3.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuControls_3.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x), X(600.0f));
	MenuPage_Controls.AddMenu(&MenuControls_3);

	MenuControls_6.m_numTexts = 0;
	MenuControls_6.m_numSprites = 0;
	MenuControls_6.SetPosition(X(170.0f), Y(88.0f));
	MenuControls_6.AddPicture(&m_sprites[FE_CONTROLLER],
		&m_sprites[FE_CONTROLLERSH],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_6.AddPicture(&m_sprites[FE_ARROWS3],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_6.SetNewOldTextScale(true, CONTR_DESCR_NEW_TEXTSCALE_scaled, defaultTextScale2);
	InitialiseTextsInMenuControllerOnFoot(&MenuControls_6, CMenuManager::CONFIG_2);
	MenuControls_6.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuControls_6.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x), X(600.0f));

	MenuControls_4.m_numTexts = 0;
	MenuControls_4.m_numSprites = 0;
	MenuControls_4.SetPosition(X(170.0f), Y(88.0f));
	MenuControls_4.AddPicture(&m_sprites[FE_CONTROLLER],
		&m_sprites[FE_CONTROLLERSH],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_4.AddPicture(&m_sprites[FE_ARROWS2],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_4.SetNewOldTextScale(true, CONTR_DESCR_NEW_TEXTSCALE_scaled, defaultTextScale2);
	InitialiseTextsInMenuControllerInCar(&MenuControls_4, CMenuManager::m_PrefsControllerConfig);
	MenuControls_4.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuControls_4.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x), X(600.0f));

	MenuControls_7.m_numTexts = 0;
	MenuControls_7.m_numSprites = 0;
	MenuControls_7.SetPosition(X(170.0f), Y(88.0f));
	MenuControls_7.AddPicture(&m_sprites[FE_CONTROLLER],
		&m_sprites[FE_CONTROLLERSH],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_7.AddPicture(&m_sprites[FE_ARROWS4],
		0.0f, 0.0f, X(235.2f), Y(175.2), CRGBA(255, 255, 255, 255));
	MenuControls_7.SetNewOldTextScale(true, CONTR_DESCR_NEW_TEXTSCALE_scaled, defaultTextScale2);
	InitialiseTextsInMenuControllerInCar(&MenuControls_7, CMenuManager::CONFIG_2);
	MenuControls_7.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuControls_7.SetNewOldShadowWrapX(true, X(600.0f+SHADOW_VECTOR.x), X(600.0f));

	MenuControls_1.m_numOptions = 0;
	MenuControls_1.SetPosition(X(284.0f), Y(290.0f));
	MenuControls_1.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, SELECTED_TEXT_COLOR);
	MenuControls_1.SetNewOldTextScale(true, CONFIGS_NEW_TEXTSCALE_scaled, defaultTextScale3, false);
	MenuControls_1.AddTitle(TheText.Get("FEC_CCF"), 0.0f, 0.0f, true);
	MenuControls_1.AddOption(TheText.Get("FEC_CF1"), X(15.0f), Y(2.0f), TriggerControls_ContrConfig, false, false);
	MenuControls_1.AddOption(TheText.Get("FEC_CF2"), X(85.0f), Y(2.0f), TriggerControls_ContrConfig, false, false);
	MenuControls_1.AddOption(TheText.Get("FEC_CF3"), X(155.0f), Y(2.0f), TriggerControls_ContrConfig, false, false);
	MenuControls_1.AddOption(TheText.Get("FEC_CF4"), X(225.0f), Y(2.0f), TriggerControls_ContrConfig, false, false);
	MenuPage_Controls.AddMenu(&MenuControls_1);
	MenuControls_1.m_alwaysTrigger = (CMenuMultiChoiceTriggered::Trigger)TriggerControls_DrawContrConfig;
	MenuControls_1.m_alwaysHighlightTrigger = (CMenuMultiChoiceTriggered::Trigger)TriggerControls_DrawHNContrConfig;
	MenuControls_1.m_alwaysNormalTrigger = (CMenuMultiChoiceTriggered::Trigger)TriggerControls_DrawHNContrConfig;

	MenuControls_2.m_numOptions = 0;
	MenuControls_2.SetPosition(X(284.0f), Y(310.0f));
	MenuControls_2.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, SELECTED_TEXT_COLOR);
	MenuControls_2.SetNewOldTextScale(true, CONFIGS_NEW_TEXTSCALE_scaled, defaultTextScale3, false);
	MenuControls_2.AddTitle(TheText.Get("FEC_CDP"), 0.0f, 0.0f, true);
	MenuControls_2.AddOption(TheText.Get("FEC_ONF"), X(15.0f), Y(2.0f), (CMenuMultiChoiceTriggered::Trigger)TriggerControls_ContrDisplay, false, false);
	MenuControls_2.AddOption(TheText.Get("FEC_INC"), X(105.0f), Y(2.0f), (CMenuMultiChoiceTriggered::Trigger)TriggerControls_ContrDisplay, false, false);
	MenuPage_Controls.AddMenu(&MenuControls_2);
	MenuControls_2.m_bTwoState = true;
	MenuControls_2.SetMenuSelection(0);

	MenuControls_5.SetPosition(X(284.0f), Y(330.0f));
	MenuControls_5.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR);
	MenuControls_5.SetNewOldTextScale(true, CONFIGS_NEW_TEXTSCALE_scaled, defaultTextScale3, false);
	MenuControls_5.AddTitle(TheText.Get("FEC_VIB"), false, 0.0f, 0.0f, true);
	MenuControls_5.SetOptionPosition(X(15.0f), Y(2.0f), TriggerControls_Vibrations, false);
	MenuPage_Controls.AddMenu(&MenuControls_5);
	MenuPage_Controls.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Controls.ActivatePage();


	/* Audio */

	CVector2D audioOutputScale(X(0.49f), Y(0.63f));
	CVector2D defaultTextScale4(X(MENU_TEXT_SIZE_X), Y(MENU_TEXT_SIZE_Y));

	FEC_MOVETAB movetab;
	MenuAudio_1.SetPosition(X(70.0f), Y(80.0f));
	MenuAudio_1.SetColors(TEXT_COLOR, TEXT_COLOR, SLIDER_LEFT_COLOR, SLIDER_RIGHT_COLOR);
	MenuAudio_1.AddTitle(TheText.Get("FEA_MUS"), 0.0f, 0.0f);
	MenuAudio_1.AddTickBox(X(15.0f), Y(20.0f), X(150.0f), Y(5.0f), Y(45.0f), TriggerAudio_MusicVolume, TriggerAudio_MusicVolumeAlways);
	movetab.right = 1;
	movetab.left = 2;
	movetab.down = 3;
	movetab.up = 3;
	MenuPage_Audio.AddMenu(&MenuAudio_1, &movetab);

	MenuAudio_4.m_numOptions = 0;
	MenuAudio_4.SetPosition(X(280.0f), Y(80.0f));
	MenuAudio_4.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, SELECTED_TEXT_COLOR);
	MenuAudio_4.SetNewOldTextScale(true, audioOutputScale, defaultTextScale4, false);
	MenuAudio_4.AddTitle(TheText.Get("FEA_OUT"), X(AUDIO_OUTPUT_POS.x), Y(AUDIO_OUTPUT_POS.y), false);
	MenuAudio_4.AddOption(TheText.Get("FEA_ST"), X(-15.0f), Y(30.0f), TriggerAudio_StereoMono, false, false);
	MenuAudio_4.AddOption(TheText.Get("FEA_MNO"), X(55.0f), Y(30.0f), TriggerAudio_StereoMono, false, false);
	movetab.right = 2;
	movetab.left = 0;
	movetab.down = 3;
	movetab.up = 3;
	MenuPage_Audio.AddMenu(&MenuAudio_4, &movetab);
	MenuAudio_4.m_bTwoState = true;

	MenuAudio_2.SetPosition(X(410.0f), Y(80.0f));
	MenuAudio_2.SetColors(TEXT_COLOR, TEXT_COLOR, SLIDER_LEFT_COLOR, SLIDER_RIGHT_COLOR);
	MenuAudio_2.AddTitle(TheText.Get("FEA_SFX"), 0.0f, 0.0f);
	MenuAudio_2.AddTickBox(X(5.0f), Y(20.0f), X(150.0f), Y(5.0f), Y(45.0f), TriggerAudio_SfxVolume, TriggerAudio_SfxVolumeAlways);
	movetab.right = 0;
	movetab.left = 1;
	movetab.down = 3;
	movetab.up = 3;
	MenuPage_Audio.AddMenu(&MenuAudio_2, &movetab);

	MenuAudio_3.m_numOptions = 0;
	MenuAudio_3.SetPosition(X(50.0f), Y(170.0f));
	MenuAudio_3.SetColors(TITLE_TEXT_COLOR, CRGBA(64, 64, 64, 255), CRGBA(250, 250, 250, 255));
	MenuAudio_3.AddTitle(TheText.Get("FEA_RSS"), X(AUDIO_RSTATION_POS.x), Y(AUDIO_RSTATION_POS.y), false);
	// first row
	movetab.right = 1;
	movetab.left = 4;
	movetab.down = 5;
	movetab.up = 5;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO1], &movetab, 0.0f, Y(18.0f),
		CVector2D(X(96.0f), YF(72.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 2;
	movetab.left = 0;
	movetab.down = 6;
	movetab.up = 6;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO2], &movetab, X(106.0f), Y(20.0f),
		CVector2D(X(79.2f), YF(81.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 3;
	movetab.left = 1;
	movetab.down = 7;
	movetab.up = 7;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO5], &movetab, X(210.0f), Y(20.0f),
		CVector2D(X(86.4f), YF(72.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 4;
	movetab.left = 2;
	movetab.down = 8;
	movetab.up = 8;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO7], &movetab, X(324.0f), Y(5.0f),
		CVector2D(X(115.2f), YF(102.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 0;
	movetab.left = 3;
	movetab.down = 8;
	movetab.up = 8;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO8], &movetab, X(446.0f), Y(5.0f),
		CVector2D(X(102.96f), YF(101.4f)), TriggerAudio_RadioStation, false);
	// second row
	movetab.right = 6;
	movetab.left = 8;
	movetab.down = 0;
	movetab.up = 0;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO3], &movetab, X(60.0f), Y(96.0f),
		CVector2D(X(87.36f), YF(85.8f)), TriggerAudio_RadioStation, false);
	movetab.right = 7;
	movetab.left = 5;
	movetab.down = 1;
	movetab.up = 1;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO4], &movetab, X(130.0f), Y(72.0f),
		CVector2D(X(129.6f), YF(129.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 8;
	movetab.left = 6;
	movetab.down = 2;
	movetab.up = 2;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO6], &movetab, X(284.0f), Y(108.0f),
		CVector2D(X(60.0f), YF(60.0f)), TriggerAudio_RadioStation, false);
	movetab.right = 5;
	movetab.left = 7;
	movetab.down = 3;
	movetab.up = 3;
	MenuAudio_3.AddOption(&m_sprites[FE_RADIO9], &movetab, X(404.0f), Y(85.0f),
		CVector2D(X(81.12f), YF(101.4f)), TriggerAudio_RadioStation, false);
	movetab.right = 2;
	movetab.left = 0;
	movetab.down = 1;
	movetab.up = 1;
	MenuPage_Audio.AddMenu(&MenuAudio_3, &movetab);
	MenuPage_Audio.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Audio.ActivatePage();


	/* Display */

	MenuDisplay_1.SetPosition(X(240.0f), Y(140.0f));
	MenuDisplay_1.SetColors(TEXT_COLOR, TEXT_COLOR, SLIDER_LEFT_COLOR, SLIDER_RIGHT_COLOR);
	MenuDisplay_1.m_style = 0;	// ticks
	MenuDisplay_1.AddTitle(TheText.Get("FED_BRI"), X(DISPLAY_BRIGHTNESS_POS.x), Y(DISPLAY_BRIGHTNESS_POS.y));
	MenuDisplay_1.AddTickBox(X(-30.0f), Y(20.0f), X(200.0f), Y(40.0f), Y(40.0f));
	MenuPage_Display.AddMenu(&MenuDisplay_1);
	MenuDisplay_2.SetPosition(X(290.0f), Y(240.0f));
	MenuDisplay_2.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR);
	MenuDisplay_2.AddTitle(TheText.Get("FED_TRA"), false, 0.0f, 0.0f, true);
#ifdef GTA_PC
	MenuDisplay_2.SetOptionPosition(X(40.0f), 0.0f, TriggerDisplay_Trails, false);
#else
	MenuDisplay_2.SetOptionPosition(X(40.0f), 0.0f, false);
#endif
	MenuDisplay_2.m_bTwoState = true;
	MenuPage_Display.AddMenu(&MenuDisplay_2);
	MenuDisplay_3.SetPosition(X(290.0f), Y(260.0f));
	MenuDisplay_3.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR);
	MenuDisplay_3.AddTitle(TheText.Get("FED_SUB"), false, 0.0f, 0.0f, true);
	MenuDisplay_3.SetOptionPosition(X(40.0f), 0.0f, false);
	MenuDisplay_3.m_bTwoState = true;
	MenuPage_Display.AddMenu(&MenuDisplay_3);
	MenuDisplay_4.SetPosition(X(290.0f), Y(280.0f));
	MenuDisplay_4.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR);
	MenuDisplay_4.AddTitle(TheText.Get("FED_WIS"), false, 0.0f, 0.0f, true);
	MenuDisplay_4.SetOptionPosition(X(40.0f), 0.0f, false);
	MenuDisplay_4.m_bTwoState = true;
	MenuPage_Display.AddMenu(&MenuDisplay_4);
	MenuPage_Display.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Display.ActivatePage();


	/* Language */
	MenuLanguage_1.m_numOptions = 0;
	MenuLanguage_1.SetPosition(X(288.0f), Y(160.0f));
	MenuLanguage_1.SetColors(TITLE_TEXT_COLOR, TEXT_COLOR, SELECTED_TEXT_COLOR);
	MenuLanguage_1.AddOption(TheText.Get("FEL_ENG"), 0.0f, 0.0f, TriggerLanguage_Language, false, false);
	MenuLanguage_1.AddOption(TheText.Get("FEL_FRE"), 0.0f, Y(20.0f), TriggerLanguage_Language, false, false);
	MenuLanguage_1.AddOption(TheText.Get("FEL_GER"), 0.0f, Y(40.0f), TriggerLanguage_Language, false, false);
	MenuLanguage_1.AddOption(TheText.Get("FEL_ITA"), 0.0f, Y(60.0f), TriggerLanguage_Language, false, false);
	MenuLanguage_1.AddOption(TheText.Get("FEL_SPA"), 0.0f, Y(80.0f), TriggerLanguage_Language, false, false);
	MenuPage_Language.AddMenu(&MenuLanguage_1);
	MenuPage_Language.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR);
	MenuPage_Language.ActivatePage();


	pActiveMenuPage = &MenuPage_Stats;
	pActiveMenuPage->ActivatePage();

	InitialiseMenuContents();

	m_bWantToUpdateContent = false;
}

void
CMenuManager::InitialiseChangedLanguageSettings(void)
{
	if ( bFrontEnd_ReloadObrTxtGxt )
	{
		bFrontEnd_ReloadObrTxtGxt = false;

		CTimer::Stop();
		TheText.Unload();
		TheText.Load();
		CTimer::Update();

		FrontEndMenuManager.AnaliseMenuContents();
		CGame::frenchGame = false;
		CGame::germanGame = false;
		if ( m_PrefsAllowNastyGame )
			CGame::nastyGame = true;

		for ( int32 i = 0; i < NUM_PAGES; i++ )
		{
			BUTTONTAB_TEXT_X_SCALES[i] = 1.0f;
			PANEL_TEXT_X_SCALES[i] = 1.0f;
		}

		switch ( m_PrefsLanguage )
		{
			case LANGUAGE_AMERICAN:
			{
				MENU_TEXT_SIZE_X = 0.644f;
				MENU_TEXT_SIZE_Y = 0.84f;//0.96f;

				BUTTONTAB_TEXT_SIZE_X = 0.35f;
				BUTTONTAB_TEXT_SIZE_Y = 0.7f;//0.8f;

				BUTTONTAB_TEXT_X_SCALES[6] = 0.94f;

				CONTR_DESCR_NEW_TEXTSCALE.x = 0.4564f;
				CONTR_DESCR_NEW_TEXTSCALE.y = 0.63f;//0.72f;

				CONFIGS_NEW_TEXTSCALE.x = 0.49f;
				CONFIGS_NEW_TEXTSCALE.y = 0.7f;//0.8f;

				AUDIO_OUTPUT_POS.x = 0.0f;
				AUDIO_OUTPUT_POS.y = 0.0f;

				AUDIO_RSTATION_POS.x = 154.0f;
				AUDIO_RSTATION_POS.y = 0.0f;

				DISPLAY_BRIGHTNESS_POS.x = 0.0f;
				DISPLAY_BRIGHTNESS_POS.y = 0.0f;


				break;
			}

			case LANGUAGE_FRENCH:
			{
				CGame::frenchGame = true;
				if ( m_PrefsAllowNastyGame )
					CGame::nastyGame = false;

				MENU_TEXT_SIZE_X = 0.504f;
				MENU_TEXT_SIZE_Y = 0.84f;//0.96f;

				BUTTONTAB_TEXT_SIZE_X = 0.32f;
				BUTTONTAB_TEXT_SIZE_Y = 0.7f;//0.8f;

				BUTTONTAB_TEXT_X_SCALES[0] = 0.84f;
				BUTTONTAB_TEXT_X_SCALES[3] = 0.84f;
				PANEL_TEXT_X_SCALES[1] = 0.8f;

				CONTR_DESCR_NEW_TEXTSCALE.x = 0.385f;
				CONTR_DESCR_NEW_TEXTSCALE.y = 0.63f;//0.72f;

				CONFIGS_NEW_TEXTSCALE.x = 0.455f;
				CONFIGS_NEW_TEXTSCALE.y = 0.7f;//0.8f;

				AUDIO_OUTPUT_POS.x = -15.0f;
				AUDIO_OUTPUT_POS.y = 0.0f;

				AUDIO_RSTATION_POS.x = 184.0f;
				AUDIO_RSTATION_POS.y = 0.0f;

				DISPLAY_BRIGHTNESS_POS.x = 20.0f;
				DISPLAY_BRIGHTNESS_POS.y = 0.0f;


				break;
			}

			case LANGUAGE_GERMAN:
			{
				CGame::germanGame = true;
				if ( m_PrefsAllowNastyGame )
					CGame::nastyGame = false;

				MENU_TEXT_SIZE_X = 0.546f;
				MENU_TEXT_SIZE_Y = 0.84f;//0.96f;

				BUTTONTAB_TEXT_SIZE_X = 0.32f;
				BUTTONTAB_TEXT_SIZE_Y = 0.7f;//0.8f;

				CONTR_DESCR_NEW_TEXTSCALE.x = 0.35f;
				CONTR_DESCR_NEW_TEXTSCALE.y = 0.63f;//0.72f;

				CONFIGS_NEW_TEXTSCALE.x = 0.434f;
				CONFIGS_NEW_TEXTSCALE.y = 0.7f;//0.8f;

				AUDIO_OUTPUT_POS.x = -15.0f;
				AUDIO_OUTPUT_POS.y = 0.0f;

				AUDIO_RSTATION_POS.x = 154.0f;
				AUDIO_RSTATION_POS.y = 0.0f;

				DISPLAY_BRIGHTNESS_POS.x = 20.0f;
				DISPLAY_BRIGHTNESS_POS.y = 0.0f;


				break;
			}

			case LANGUAGE_ITALIAN:
			{
				MENU_TEXT_SIZE_X = 0.574f;
				MENU_TEXT_SIZE_Y = 0.84f;//0.96f;

				BUTTONTAB_TEXT_SIZE_X = 0.32f;
				BUTTONTAB_TEXT_SIZE_Y = 0.7f;//0.8f;

				BUTTONTAB_TEXT_X_SCALES[0] = 0.86f;
				PANEL_TEXT_X_SCALES[1] = 0.9f;

				CONTR_DESCR_NEW_TEXTSCALE.x = 0.385f;
				CONTR_DESCR_NEW_TEXTSCALE.y = 0.63f;//0.72f;

				CONFIGS_NEW_TEXTSCALE.x = 0.42f;
				CONFIGS_NEW_TEXTSCALE.y = 0.7f;//0.8f;

				AUDIO_OUTPUT_POS.x = 10.0f;
				AUDIO_OUTPUT_POS.y = 0.0f;

				AUDIO_RSTATION_POS.x = 194.0f;
				AUDIO_RSTATION_POS.y = 0.0f;

				DISPLAY_BRIGHTNESS_POS.x = 10.0f;
				DISPLAY_BRIGHTNESS_POS.y = 0.0f;


				break;
			}

			case LANGUAGE_SPANISH:
			{
				MENU_TEXT_SIZE_X = 0.546f;
				MENU_TEXT_SIZE_Y = 0.84f;//0.96f;

				BUTTONTAB_TEXT_SIZE_X = 0.35f;
				BUTTONTAB_TEXT_SIZE_Y = 0.7f;//0.8f;

				BUTTONTAB_TEXT_X_SCALES[0] = 0.78f;
				PANEL_TEXT_X_SCALES[1] = 0.95f;

				CONTR_DESCR_NEW_TEXTSCALE.x = 0.364f;
				CONTR_DESCR_NEW_TEXTSCALE.y = 0.63f;//0.72f;

				CONFIGS_NEW_TEXTSCALE.x = 0.455f;
				CONFIGS_NEW_TEXTSCALE.y = 0.7f;//0.8f;

				AUDIO_OUTPUT_POS.x = 10.0f;
				AUDIO_OUTPUT_POS.y = 0.0f;

				AUDIO_RSTATION_POS.x = 124.0f;
				AUDIO_RSTATION_POS.y = 0.0f;

				DISPLAY_BRIGHTNESS_POS.x = 30.0f;
				DISPLAY_BRIGHTNESS_POS.y = 0.0f;


				break;
			}
		}
	}
}

void
CMenuManager::InitialiseMenuContents(void)
{
	if ( m_bWantToUpdateContent == false )
	{
		m_bWantToUpdateContent = true;

		m_pageState = PAGESTATE_NORMAL;

		switch ( CPad::GetPad(0)->GetMode() )
		{
			case 3: m_PrefsControllerConfig = CONFIG_4; break;
			case 2: m_PrefsControllerConfig = CONFIG_3; break;
			case 1: m_PrefsControllerConfig = CONFIG_2; break;
			case 0: m_PrefsControllerConfig = CONFIG_1; break;
		}

		MenuControls_1.SetMenuSelection(m_PrefsControllerConfig);
		MenuControls_5.SetMenuSelection(m_PrefsUseVibration);

		MenuAudio_1.SetMenuSelection(m_PrefsMusicVolume / 127.0f * 100.0f + 0.5f);
		MenuAudio_2.SetMenuSelection(m_PrefsSfxVolume   / 127.0f * 100.0f + 0.5f);
		MenuAudio_3.SetMenuSelection(m_PrefsRadioStation);
		MenuAudio_4.SetMenuSelection(m_PrefsStereoMono);

		MenuDisplay_1.SetMenuSelection(m_PrefsBrightness / 512.0f * 100.0f + 0.5f);
#ifdef PS2
		m_PrefsShowTrails = BlurOn;
#else
		m_PrefsShowTrails = CMBlur::BlurOn;
#endif
		MenuDisplay_2.SetMenuSelection(m_PrefsShowTrails);
		MenuDisplay_3.SetMenuSelection(m_PrefsShowSubtitles);
		MenuDisplay_4.SetMenuSelection(m_PrefsUseWideScreen);

		MenuLanguage_1.SetMenuSelection(m_PrefsLanguage);

		MenuBriefs_1.m_numTexts = 0;
		MenuBriefs_1.AddText(TheText.Get("FEB_PMB"), 0.0f, 0.0f, TITLE_TEXT_COLOR, 0); // Previous Mission Briefs:

		static wchar StringsToDisplay[NUMPREVIOUSBRIEFS][256];

		CRGBA newColor;
		int32 brierY = 36;

		for ( int32 i = NUMPREVIOUSBRIEFS-1; i >= 0; i-- )
		{
			tPreviousBrief &brief = CMessages::PreviousBriefs[i];
			if (brief.m_pText)
			{
				CMessages::InsertNumberInString(brief.m_pText,
					brief.m_nNumber[0], brief.m_nNumber[1],
					brief.m_nNumber[2], brief.m_nNumber[3],
					brief.m_nNumber[4], brief.m_nNumber[5], StringsToDisplay[i]);
				CMessages::InsertStringInString(StringsToDisplay[i], brief.m_pString);

				newColor = TEXT_COLOR;
				FilterOutColorMarkersFromString(StringsToDisplay[i], newColor);

				if (newColor != TEXT_COLOR)
				{
					newColor.r /= 2;
					newColor.g /= 2;
					newColor.b /= 2;
				}
				MenuBriefs_1.AddText(StringsToDisplay[i], 0.0f, YF((float)brierY), newColor, 0);
				brierY += 54;
			}
		}

		MenuStats_1.m_scrollPosition = 0.0f;
		MenuStats_1.ResetNumberOfTextLines();

		nStatLinesIndex = 0;

		#define STAT_HEADER(str) do { MenuStats_1.AddTextLine(TheText.Get(str), nil); } while(0)
		#define STAT_PARAM(str) do { MenuStats_1.AddTextLine(nil, TheText.Get(str)); } while(0)
		#define STAT_LINE(str, left, isFloat, right) do { MenuStats_1.AddTextLine(TheText.Get(str), PrintStatLine(str, left, isFloat, right)); } while(0)

		int32 nTemp;

		STAT_HEADER("PL_STAT");

		int32 percentCompleted = (CStats::TotalProgressInGame == 0 ? 0 : CStats::ProgressMade * 100.0f / (CGame::nastyGame ? CStats::TotalProgressInGame : CStats::TotalProgressInGame - 1));
		percentCompleted = Min(percentCompleted, 100);

		STAT_LINE("PER_COM", &percentCompleted,       0, nil);

		STAT_LINE("NMISON",  &CStats::MissionsGiven,  0, nil);

		STAT_LINE("FEST_MP", &CStats::MissionsPassed, 0, &CStats::TotalNumberMissions);

		if ( CGame::nastyGame )
			STAT_LINE("FEST_RP", &CStats::NumberKillFrenziesPassed, 0, &CStats::TotalNumberKillFrenzies);

		CPlayerInfo &player = CWorld::Players[CWorld::PlayerInFocus];
		float packagesPercent = 0.0f;
		if (player.m_nTotalPackages != 0)
			packagesPercent = player.m_nCollectedPackages * 100.0f / player.m_nTotalPackages;
		int32 nPackagesPercent = packagesPercent;
		nTemp = 100;

		STAT_LINE("PERPIC", &nPackagesPercent,       0, &nTemp);

		STAT_LINE("NOUNIF", &CStats::NumberOfUniqueJumpsFound, 0, &CStats::TotalNumberOfUniqueJumps);

		STAT_LINE("DAYSPS", &CStats::DaysPassed, 0, nil);

		if ( CGame::nastyGame )
		{
			STAT_LINE("PE_WAST", &CStats::PeopleKilledByPlayer, 0, nil);
			STAT_LINE("PE_WSOT", &CStats::PeopleKilledByOthers, 0, nil);
		}

		STAT_LINE("CAR_EXP", &CStats::CarsExploded, 0, nil);

		STAT_LINE("TM_BUST", &CStats::TimesArrested, 0, nil);

		STAT_LINE("TM_DED", &CStats::TimesDied, 0, nil);

		nTemp = CStats::PedsKilledOfThisType[PEDTYPE_GANG9] + CStats::PedsKilledOfThisType[PEDTYPE_GANG8]
			+ CStats::PedsKilledOfThisType[PEDTYPE_GANG7] + CStats::PedsKilledOfThisType[PEDTYPE_GANG6]
			+ CStats::PedsKilledOfThisType[PEDTYPE_GANG5] + CStats::PedsKilledOfThisType[PEDTYPE_GANG4]
			+ CStats::PedsKilledOfThisType[PEDTYPE_GANG3] + CStats::PedsKilledOfThisType[PEDTYPE_GANG2]
			+ CStats::PedsKilledOfThisType[PEDTYPE_GANG1];
		STAT_LINE("GNG_WST", &nTemp, 0, nil);

		nTemp = CStats::PedsKilledOfThisType[PEDTYPE_CRIMINAL];
		STAT_LINE("DED_CRI", &nTemp, 0, nil);

		STAT_LINE("HEL_DST", &CStats::HelisDestroyed, 0, nil);

		STAT_LINE("KGS_EXP", &CStats::KgsOfExplosivesUsed, 0, nil);

		if (CStats::LongestFlightInDodo > 0)
			STAT_LINE("FEST_LF", &CStats::LongestFlightInDodo, 0, nil);

		STAT_LINE("CAR_CRU", &CStats::CarsCrushed, 0, nil);

		if (CStats::HighestScores[0] > 0)
		{
			STAT_HEADER("FEST_BB");
			STAT_LINE("FEST_H0", &CStats::HighestScores[0], 0, nil);
		}

		int32 hs = 0;
		for ( int32 i = 1; i < 5; i++ )
			hs += CStats::HighestScores[i];

		if (hs > 0)
			STAT_HEADER("FEST_GC");

		if (CStats::HighestScores[1] > 0)
			STAT_LINE("FEST_H1", &CStats::HighestScores[1], 0, nil);

		if (CStats::HighestScores[2] > 0)
			STAT_LINE("FEST_H2", &CStats::HighestScores[2], 0, nil);

		if (CStats::HighestScores[3] > 0)
			STAT_LINE("FEST_H3", &CStats::HighestScores[3], 0, nil);

		if (CStats::HighestScores[4] > 0)
			STAT_LINE("FEST_H4", &CStats::HighestScores[4], 0, nil);

		STAT_LINE("FESTDFM", &CStats::DistanceTravelledOnFoot, 0, nil);
		STAT_LINE("FESTDCM", &CStats::DistanceTravelledByCar, 0, nil);
		STAT_LINE("DISTBIM", &CStats::DistanceTravelledByBike, 0, nil);
		STAT_LINE("DISTBOM", &CStats::DistanceTravelledByBoat, 0, nil);
		STAT_LINE("DISTGOM", &CStats::DistanceTravelledByGolfCart, 0, nil);
		STAT_LINE("DISTHEM", &CStats::DistanceTravelledByHelicoptor, 0, nil);
		STAT_LINE("MMRAIN", &CStats::mmRain, 0, nil);
		nTemp = (int32)CStats::MaximumJumpDistance;
		STAT_LINE("MXCARDM", &nTemp, 0, nil);
		nTemp = (int32)CStats::MaximumJumpHeight;
		STAT_LINE("MXCARJM", &nTemp, 0, nil);

		STAT_LINE("MXFLIP", &CStats::MaximumJumpFlips, 0, nil);
		STAT_LINE("MXJUMP", &CStats::MaximumJumpSpins, 0, nil);

		STAT_HEADER("BSTSTU");

		switch (CStats::BestStuntJump)
		{
			case 1:  STAT_PARAM("INSTUN"); break;
			case 2:  STAT_PARAM("PRINST"); break;
			case 3:  STAT_PARAM("DBINST"); break;
			case 4:  STAT_PARAM("DBPINS"); break;
			case 5:  STAT_PARAM("TRINST"); break;
			case 6:  STAT_PARAM("PRTRST"); break;
			case 7:  STAT_PARAM("QUINST"); break;
			case 8:  STAT_PARAM("PQUINS"); break;
			default: STAT_PARAM("NOSTUC"); break;
		}

		STAT_LINE("PASDRO", &CStats::PassengersDroppedOffWithTaxi, 0, nil);
		STAT_LINE("MONTAX", &CStats::MoneyMadeWithTaxi, 0, nil);
		STAT_LINE("FEST_LS", &CStats::LivesSavedWithAmbulance, 0, nil);
		STAT_LINE("FEST_HA", &CStats::HighestLevelAmbulanceMission, 0, nil);
		STAT_LINE("FEST_CC", &CStats::CriminalsCaught, 0, nil);
		STAT_LINE("FEST_FE", &CStats::FiresExtinguished, 0, nil);
		int32 rnd = ((CGeneral::GetRandomNumber() & 255) + 100) * 2384;
		STAT_LINE("DAYPLC", &rnd, 0, nil);

		#undef STAT_LINE

		MenuStats_2.m_numTexts = 0;
		MenuStats_2.AddText(TheText.Get("CRIMRA"), 0.0f, 0.0f, CRIM_RATING_TEXT_COLOR, 0);

		char rating[16];
		wchar urating[16];
		sprintf(rating, "  %d", CStats::FindCriminalRatingNumber());
		AsciiToUnicode(rating, urating);

		wchar *pStatLine = aStatLines[nStatLinesIndex++];
		UnicodeStrcpy(pStatLine, CStats::FindCriminalRatingString());
		UnicodeStrcat(pStatLine, urating);

		MenuStats_2.AddText(pStatLine, X(MenuStats_1.m_width), 0.0f, CRIM_RATING_TEXT_COLOR, 1);

	}
}


void
CMenuManager::AnaliseMenuContents(void)
{
	if ( m_bWantToUpdateContent )
	{
		m_bWantToUpdateContent = false;

		m_PrefsControllerConfig = (CONTRCONFIG)MenuControls_1.GetMenuSelection();
		switch ( m_PrefsControllerConfig )
		{
			case CONFIG_4: CPad::GetPad(0)->SetMode(3); break;
			case CONFIG_3: CPad::GetPad(0)->SetMode(2); break;
			case CONFIG_2: CPad::GetPad(0)->SetMode(1); break;
			case CONFIG_1: CPad::GetPad(0)->SetMode(0); break;
		}

		m_PrefsUseVibration = MenuControls_5.m_title.m_bSelected;

		m_PrefsMusicVolume   = float(MenuAudio_1.GetMenuSelection())/100.0f*127.0f+0.5f;
		m_PrefsSfxVolume     = float(MenuAudio_2.GetMenuSelection())/100.0f*127.0f+0.5f;
		m_PrefsRadioStation  = MenuAudio_3.GetMenuSelection();
		m_PrefsStereoMono    = MenuAudio_4.GetMenuSelection();
		m_PrefsBrightness    = float(MenuDisplay_1.GetMenuSelection()) / 100.0f*512.0f + 0.5f;
		m_PrefsShowTrails    = MenuDisplay_2.GetMenuSelection();
		m_PrefsShowSubtitles = MenuDisplay_3.GetMenuSelection();
		m_PrefsUseWideScreen = MenuDisplay_4.GetMenuSelection();
#ifdef PS2
		BlurOn               = m_PrefsShowTrails;
#else
		CMBlur::BlurOn       = m_PrefsShowTrails;
#endif

		if ( m_PrefsLanguage != MenuLanguage_1.GetMenuSelection() )
		{
			m_PrefsLanguage = MenuLanguage_1.GetMenuSelection();
			m_bInitialised = false;
			bFrontEnd_ReloadObrTxtGxt = true;
		}
	}
}

void
CMenuManager::CheckLanguageSelectionChanged(void)
{
	if ( MenuLanguage_1.GetMenuSelection() != m_PrefsLanguage )
	{
		m_bInitialised = false;
		bFrontEnd_ReloadObrTxtGxt = true;
	}
}

void
CMenuManager::DrawFrontEnd(void)
{
	CFont::SetAlphaFade(255.0f);
	DrawFrontEndNormal();

	DisplayWarningControllerMsg();
}

void
CMenuManager::DrawFrontEndNormal(void)
{
	CSprite2d::InitPerFrame();
	CFont::InitPerFrame();


	m_fade = 255;
	if ( m_nChangePageTimer != 0 && m_nChangePageTimer >= CTimer::GetTimeInMillisecondsPauseMode() )
		m_fade = uint32(float(m_nChangePageTimer - CTimer::GetTimeInMillisecondsPauseMode()) / 250.0f * 255.0f);

	m_someAlpha = 255;

	m_position.x = 0.0f;
	m_position.y = 0.0f;

	if ( m_nStartPauseTimer != 0 && m_nStartPauseTimer >= CTimer::GetTimeInMillisecondsPauseMode() )
	{
		float slide = float(m_nStartPauseTimer - CTimer::GetTimeInMillisecondsPauseMode()) / 800.0f;
		float alpha = 1.0f;

		if ((m_nStartPauseTimer - CTimer::GetTimeInMillisecondsPauseMode()) <= 1600)
			alpha = float(m_nStartPauseTimer - CTimer::GetTimeInMillisecondsPauseMode()) / 400.0f;

		m_someAlpha = 255 - Clamp(alpha, 0.0f, 1.0f) * 255.0f;

		switch ( m_nSlidingDir )
		{
			case SLIDE_TO_RIGHT:  m_position.x =   slide * X(700.0f);  break;
			case SLIDE_TO_TOP:    m_position.y = -(slide * Y(500.0f)); break;
			case SLIDE_TO_LEFT:   m_position.x = -(slide * X(700.0f)); break;
			case SLIDE_TO_BOTTOM: m_position.y =   slide * Y(500.0f);  break;
			default:              m_position.y =   slide * Y(500.0f);  break;
		}
	}

	if ( m_nEndPauseTimer != 0 && m_nEndPauseTimer >= CTimer::GetTimeInMillisecondsPauseMode() )
	{
		float slide = float(m_nEndPauseTimer - CTimer::GetTimeInMillisecondsPauseMode()) / 800.0f;
		float alpha = float((int32)(m_nEndPauseTimer - CTimer::GetTimeInMillisecondsPauseMode()) + -266) / 533.0f;

		m_someAlpha = Clamp(alpha, 0.0f, 1.0f) * 255.0f;

		switch ( m_nSlidingDir )
		{
			case SLIDE_TO_TOP:    m_position.y =   (1.0f - slide) * Y(500.0f);  break;
			case SLIDE_TO_RIGHT:  m_position.x =   (1.0f - slide) * X(700.0f);  break;
			case SLIDE_TO_LEFT:   m_position.x =   (1.0f - slide) * X(700.0f);  break;
			case SLIDE_TO_BOTTOM: m_position.y = -((1.0f - slide) * Y(500.0f)); break;
			default:              m_position.y = -((1.0f - slide) * Y(500.0f)); break;
		}
	}

	if ( m_someAlpha < 255 )
		m_fade = m_someAlpha;

	float posX, posY;

	/* Draw splash */
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	CSprite2d *splash = LoadSplash(nil);
	if(splash)
		splash->Draw(CRect(0.0f, 0.0f, SCRW, SCRH), BACKGROUND_SPLASH_COLOR);
	else
		// doesn't exist!!
		CHud::Sprites[19].Draw(CRect(0.0f, 0.0f, SCRW, SCRH), BACKGROUND_SPLASH_COLOR);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERMIPNEAREST);

	/* Draw main panel */
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);
	CRGBA panelColor(255, 255, 255, m_someAlpha);
	m_sprites[FE2_MAINPANEL_UL].Draw(
		CRect(m_position.x, m_position.y, m_position.x+SCRW/2.0f, m_position.y+SCRH/2.0f),
		panelColor);
	m_sprites[FE2_MAINPANEL_UR].Draw(
		CRect(m_position.x+SCRW/2.0f, m_position.y, m_position.x+SCRW, m_position.y+SCRH/2.0f),
		panelColor);
	m_sprites[FE2_MAINPANEL_DL].Draw(
		CRect(m_position.x, m_position.y+SCRH/2.0f, m_position.x+SCRW/2.0f, m_position.y+SCRH),
		panelColor);
	m_sprites[FE2_MAINPANEL_DR].Draw(
		CRect(m_position.x+SCRW/2.0f, m_position.y+SCRH/2.0f, m_position.x+SCRW, m_position.y+SCRH),
		panelColor);

	/* Draw icon backdrop */
	CRGBA iconColor(255, 255, 255, m_fade*0.75f);
	float iconX = 48.0f;
	float iconY = 54.0f;
	float iconWidth = 540.0f;
	float iconHeight = 296.0f;
	int32 sprite = FE_ICONBRIEF;

#ifdef PS2_MENU_USEALLPAGEICONS
	switch(m_currentPage)
	{
	case PAGE_STATS:
		sprite = FE_ICONSTATS;
		break;
	case PAGE_NEW_GAME:
		sprite = FE_ICONNEWGAME;
		break;
	case PAGE_CONTROLS:
		sprite = FE_ICONCONTROLS;
		break;
	case PAGE_BRIEFS:
		sprite = FE_ICONBRIEF;
		break;
	case PAGE_AUDIO:
		sprite = FE_ICONAUDIO;
		break;
	case PAGE_DISPLAY:
		sprite = FE_ICONDISPLAY;
		break;
	case PAGE_LANGUAGE:
		sprite = FE_ICONLANGUAGE;
		break;
	}
#else
	switch(m_currentPage)
	{
	case PAGE_STATS:
	case PAGE_NEW_GAME:
	case PAGE_CONTROLS:
		sprite = FE_ICONSTATS;	// PS2 has the same texture for stats and brief
		//sprite = FE_ICONBRIEF;
		break;
	case PAGE_BRIEFS:
		sprite = FE_ICONBRIEF;
		break;
	case PAGE_AUDIO:
		sprite = FE_ICONAUDIO;
		break;
	case PAGE_DISPLAY:
		sprite = FE_ICONDISPLAY;
		break;
	case PAGE_LANGUAGE:
		sprite = FE_ICONLANGUAGE;
		break;
	}
#endif
	m_sprites[sprite].Draw(
		CRect_SZ(m_position.x+X(iconX), m_position.y+Y(iconY), X(iconWidth), Y(iconHeight)),
		iconColor);

	/* Overwrite tab buttons if entered page */
	bool bOverwriteTab = false;

	switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
		case PAGESTATE_HIGHLIGHTED:
			break;

		case PAGESTATE_SELECTED:
			bOverwriteTab = true;
			break;
	}

	if( bOverwriteTab )
	{
		CRGBA shadow(41, 101, 102, m_someAlpha);
		CRGBA green(40, 48, 57, m_someAlpha);
		CSprite2d::DrawRect(
			CRect_SZ(m_position.x+X(82.0f), m_position.y+Y(408.0f), X(476.0f), Y(18.0f)),
			shadow);
		CSprite2d::DrawRect(
			CRect_SZ(m_position.x+X(82.0f), m_position.y+Y(408.0f), X(476.0f), Y(5.0f)),
			green);
	}
// stats, load, briefs, controls, audio, display, language

	/* Shadow of panel on top of tab buttons */
	CRGBA panelShadow(96, 96, 96, m_someAlpha*0.375f);
	CSprite2d::DrawRect(CRect_SZ(m_position.x+X(87.0f), m_position.y+Y(408.0f), X(464.0f), Y(3.0f)), panelShadow);
	/* Draw second shadow - seems unused */
	if ( m_nChangePageTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() < m_nChangePageTimer )
	{
		posX = 0.0f;
		switch(field_18)
		{
		case PAGE_STATS: posX = 88.0f; break;
		case PAGE_NEW_GAME: posX = 286.0f; break;	// actually controls
		case PAGE_BRIEFS: posX = 154.0f; break;	// actually load
		case PAGE_CONTROLS: posX = 220.0f; break;	// actually briefs
		case PAGE_AUDIO: posX = 352.0f; break;
		case PAGE_DISPLAY: posX = 418.0f; break;
		case PAGE_LANGUAGE: posX = 484.0f; break;
		}
		CSprite2d::DrawRect(CRect_SZ(m_position.x+X(posX), m_position.y+Y(411.0f), X(65.0f), Y(3.0f)), panelShadow);
	}

	/* Active tab */
	posX = 0.0f;
	switch(m_currentPage)
	{
	case PAGE_STATS: posX = 88.0f; break;
	case PAGE_NEW_GAME: posX = 154.0f; break;
	case PAGE_BRIEFS: posX = 220.0f; break;
	case PAGE_CONTROLS: posX = 286.0f; break;
	case PAGE_AUDIO: posX = 352.0f; break;
	case PAGE_DISPLAY: posX = 418.0f; break;
	case PAGE_LANGUAGE: posX = 484.0f; break;
	}
	// PAL has 465 for 407 here - and actually 406 seems right
	m_sprites[FE2_TABACTIVE].Draw(CRect_SZ(m_position.x+X(posX), m_position.y+YF(465.0f), X(128.0f), Y(32.0f)), CRGBA(255, 255, 255, m_someAlpha));

	/* Draw page title */
	posX = m_position.x + X(592.0f);
	posY = m_position.y + Y(376.0f);
	CRGBA fontCol1(255, 193, 71, m_someAlpha);
	CRGBA fontCol2(0, 0, 0, m_someAlpha);
	CFont::SetFontStyle(FONT_HEADING);
	CFont::SetBackgroundOff();
	CFont::SetScale(X(PANEL_TEXT_SIZE_X), Y(PANEL_TEXT_SIZE_Y));
	CFont::SetPropOn();
	CFont::SetCentreOff();
	CFont::SetJustifyOn();
	CFont::SetRightJustifyWrap(0.0f);
	CFont::SetRightJustifyOn();
	CFont::SetBackGroundOnlyTextOn();
	CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f
	const char *key = nil;
	switch(m_currentPage)
	{
	case PAGE_STATS:    key = "FEP_STA"; break;
	case PAGE_NEW_GAME:     key = "FEP_SAV"; break;
	case PAGE_BRIEFS:   key = "FEP_BRI"; break;
	case PAGE_CONTROLS: key = "FEP_CON"; break;
	case PAGE_AUDIO:    key = "FEP_AUD"; break;
	case PAGE_DISPLAY:  key = "FEP_DIS"; break;
	case PAGE_LANGUAGE: key = "FEP_LAN"; break;
	}
	CFont::SetScale(X(PANEL_TEXT_SIZE_X*PANEL_TEXT_X_SCALES[m_currentPage]), Y(PANEL_TEXT_SIZE_Y));
	CFont::SetColor(fontCol1);
	CFont::PrintString(posX, posY, TheText.Get(key));
	CFont::SetColor(fontCol2);
	CFont::PrintString(posX-X(1.0f), posY-Y(1.0f), TheText.Get(key));
	CFont::DrawFonts();

	/* Draw controller buttons */
	CFont::SetFontStyle(FONT_BANK);
	CFont::SetBackgroundOff();
	CFont::SetScale(X(0.35f), Y(0.64f));
	CFont::SetPropOn();
	CFont::SetCentreOff();
	CFont::SetJustifyOn();
	CFont::SetRightJustifyOff();
	CFont::SetBackGroundOnlyTextOn();
	CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f
	CFont::SetColor(CRGBA(16, 16, 16, m_someAlpha));
	switch(m_currentPage)
	{
		case PAGE_STATS:
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(360.0f), TheText.Get("FEDS_ST"));
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(372.0f), TheText.Get("FEDS_AM"));
			CFont::PrintString(m_position.x+X(242.0f), m_position.y+Y(360.0f), TheText.Get("FEDSSC1"));
			CFont::PrintString(m_position.x+X(242.0f), m_position.y+Y(372.0f), TheText.Get("FEDSSC2"));
			break;

		case PAGE_BRIEFS:
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(360.0f), TheText.Get("FEDS_ST"));
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(372.0f), TheText.Get("FEDS_AM"));
			break;

		case PAGE_NEW_GAME:
		case PAGE_CONTROLS:
		case PAGE_AUDIO:
		case PAGE_DISPLAY:
		case PAGE_LANGUAGE:
		{
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(360.0f), TheText.Get("FEDS_SE"));
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(372.0f), TheText.Get("FEDS_BA"));
			CFont::PrintString(m_position.x+X(52.0f), m_position.y+Y(384.0f), TheText.Get("FEDS_ST"));

			switch ( m_pageState )
			{
				case PAGESTATE_NORMAL:
					CFont::PrintString(m_position.x+X(242.0f), m_position.y+Y(372.0f), TheText.Get("FEDS_AM")); // <>-CHANGE MENU
					break;

				case PAGESTATE_HIGHLIGHTED:
				case PAGESTATE_SELECTED:
				{
					CFont::PrintString(m_position.x+X(242.0f), m_position.y+Y(360.0f+3.5f), TheText.Get("FEA_UP")); // ;
					CFont::PrintString(m_position.x+X(242.0f), m_position.y+Y(384.0f-3.5f), TheText.Get("FEA_DO")); // =
					CFont::PrintString(m_position.x+X(242.0f-10.0f), m_position.y+Y(372.0f), TheText.Get("FEA_LE")); // <
					CFont::PrintString(m_position.x+X(242.0f+11.0f), m_position.y+Y(372.0f), TheText.Get("FEA_RI")); // >
					CFont::PrintString(m_position.x+X(242.0f+20.0f), m_position.y+Y(372.0f), TheText.Get("FEDSAS3")); // - CHANGE SELECTION

					break;
				}
			}

			break;
		}
	}

	CFont::DrawFonts();

	/* Draw tab button texts */
	CFont::SetFontStyle(FONT_BANK);
	CFont::SetBackgroundOff();
	CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X), Y(BUTTONTAB_TEXT_SIZE_Y));
	CFont::SetPropOn();
	CFont::SetCentreOn();
	CFont::SetRightJustifyOff();
	CFont::SetBackGroundOnlyTextOn();
	CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f

	switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
		{
			CFont::SetColor(CRGBA(16, 16, 16, m_someAlpha));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_STATS]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(92.0f), m_position.y+Y(408.0f), TheText.Get("FEB_STA"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_NEW_GAME]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(158.0f), m_position.y+Y(408.0f), TheText.Get("FEB_SAV"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_BRIEFS]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(224.0f), m_position.y+Y(408.0f), TheText.Get("FEB_BRI"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_CONTROLS]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(290.0f), m_position.y+Y(408.0f), TheText.Get("FEB_CON"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_AUDIO]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(356.0f), m_position.y+Y(408.0f), TheText.Get("FEB_AUD"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_DISPLAY]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(422.0f), m_position.y+Y(408.0f), TheText.Get("FEB_DIS"));
			CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_LANGUAGE]), Y(BUTTONTAB_TEXT_SIZE_Y));
			CFont::PrintString(m_position.x+X(488.0f), m_position.y+Y(408.0f), TheText.Get("FEB_LAN"));

			break;
		}

		case PAGESTATE_HIGHLIGHTED:
		case PAGESTATE_SELECTED:
		{
			CFont::SetColor(CRGBA(16, 16, 16, m_someAlpha));
			switch(m_currentPage)
			{
			// PAL has 466 for 408...probably rounded?
			case PAGE_STATS:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_STATS]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(92.0f), m_position.y+Y(408.0f), TheText.Get("FEB_STA"));
				break;
			case PAGE_NEW_GAME:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_NEW_GAME]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(158.0f), m_position.y+Y(408.0f), TheText.Get("FEB_SAV"));
				break;
			case PAGE_BRIEFS:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_BRIEFS]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(224.0f), m_position.y+Y(408.0f), TheText.Get("FEB_BRI"));
				break;
			case PAGE_CONTROLS:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_CONTROLS]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(290.0f), m_position.y+Y(408.0f), TheText.Get("FEB_CON"));
				break;
			case PAGE_AUDIO:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_AUDIO]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(356.0f), m_position.y+Y(408.0f), TheText.Get("FEB_AUD"));
				break;
			case PAGE_DISPLAY:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_DISPLAY]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(422.0f), m_position.y+Y(408.0f), TheText.Get("FEB_DIS"));
				break;
			case PAGE_LANGUAGE:
				CFont::SetScale(X(BUTTONTAB_TEXT_SIZE_X*BUTTONTAB_TEXT_X_SCALES[PAGE_LANGUAGE]), Y(BUTTONTAB_TEXT_SIZE_Y));
				CFont::PrintString(m_position.x+X(488.0f), m_position.y+Y(408.0f), TheText.Get("FEB_LAN"));
				break;
			}

			break;
		}
	}

	CFont::DrawFonts();

	pActiveMenuPage = nil;
	switch(m_currentPage)
	{
		case PAGE_STATS:	pActiveMenuPage = &MenuPage_Stats; break;
		case PAGE_NEW_GAME: pActiveMenuPage = pMenuNewGame; break;
		case PAGE_BRIEFS:	pActiveMenuPage = &MenuPage_Briefs; break;
		case PAGE_CONTROLS:	pActiveMenuPage = &MenuPage_Controls; break;
		case PAGE_AUDIO:	pActiveMenuPage = &MenuPage_Audio; break;
		case PAGE_DISPLAY:	pActiveMenuPage = &MenuPage_Display; break;
		case PAGE_LANGUAGE:	pActiveMenuPage = &MenuPage_Language; break;
	}

	CFont::SetFontStyle(FONT_BANK);
	CFont::SetBackgroundOff();
	CFont::SetScale(X(MENU_TEXT_SIZE_X), Y(MENU_TEXT_SIZE_Y));
	CFont::SetPropOn();
	CFont::SetCentreOff();
	CFont::SetJustifyOn();
	CFont::SetRightJustifyOff();
	CFont::SetBackGroundOnlyTextOn();
	CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f
	CFont::SetRightJustifyWrap(X(38.0f));

	if(m_currentPage == PAGE_LANGUAGE)
	{
		CFont::SetCentreOn();
		CFont::SetCentreSize(SCRW-X(40.0f)); // 600.0f
	}

	if ( m_nEndPauseTimer != 0 )
	{
		switch ( m_currentPage )
		{
			case PAGE_NEW_GAME:
			case PAGE_BRIEFS:
			case PAGE_CONTROLS:
				break;

			default:
				CFont::SetWrapx(X(1200.0f));
				break;
		}
	}

	if(pActiveMenuPage)
	{
		pActiveMenuPage->SetAlpha(m_fade);

		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
				pActiveMenuPage->DrawNormal(m_position.x, m_position.y);
				break;

			case PAGESTATE_HIGHLIGHTED:
				pActiveMenuPage->DrawHighlighted(CRGBA(rgbaATC.r, rgbaATC.g, rgbaATC.b, m_fade), m_position.x, m_position.y);
				break;

			case PAGESTATE_SELECTED:
				pActiveMenuPage->Draw(CRGBA(rgbaATC.r, rgbaATC.g, rgbaATC.b, m_fade), CRGBA(MENU_SELECTED_COLOR.r, MENU_SELECTED_COLOR.g, MENU_SELECTED_COLOR.b, m_fade), m_position.x, m_position.y);
				break;
		}
	}

	CFont::DrawFonts();
	CFont::DrawFonts();
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSWRAP);
}



void
CMenuManager::Process(void)
{
	if ( m_bMenuActive || TheCamera.GetScreenFadeStatus() == FADE_0 )
	{
		InitialiseMenusOnce();
		m_bWantToRestart = false;
		WorkOutMenuState(false);

		if ( m_bMenuActive )
		{
			LoadAllTextures();
			InitialiseMenuContents();
			SetRandomActiveTextlineColor(0);
			ProcessControllerInput();
		}
		else
		{
			AnaliseMenuContents();
			pMenuNewGame = &MenuPage_NewGame;
			m_pageState = PAGESTATE_NORMAL;
			bIgnoreTriangleButton = false;
			UnloadTextures();
			m_bRenderGameInMenu = false;
			gErrorSampleTriggered = true;
		}
	}
}

void
CMenuManager::WorkOutMenuState(uint8 bExit)
{
#ifdef GTA_PC
	bool bIsStartPressed = CPad::GetPad(0)->GetStartJustDown() || (m_pageState == PAGESTATE_NORMAL && CPad::GetPad(0)->GetEscapeJustDown());
#else
	bool bIsStartPressed = CPad::GetPad(0)->GetStartJustDown();
#endif
	bool bIsCreditsOrDraw      = CCredits::AreCreditsDone() || m_bMenuActive;
	bool bIsDemoOrDraw         = m_bMenuActive || CGame::bDemoMode;

	if ( (bIsStartPressed && bIsCreditsOrDraw) || bExit || (!bIsDemoOrDraw && CPad::IsNoOrObsolete()) )
	{
		if ( m_nStartPauseTimer == 0 && m_nEndPauseTimer == 0 )
		{
			m_bMenuActive = !m_bMenuActive;

			if ( !m_bMenuActive )
			{
				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_STARTING, 0);
				DMAudio.ChangeMusicMode(MUSICMODE_GAME);
				gMusicPlaying = false;
				bIgnoreTriangleButton = false;

				m_bMenuActive = true;

				m_nEndPauseTimer = CTimer::GetTimeInMillisecondsPauseMode() + 800;

				if ( m_currentPage == PAGE_CONTROLS || m_currentPage == PAGE_BRIEFS || m_currentPage == PAGE_NEW_GAME )
				{
					m_nSlidingDir = CGeneral::GetRandomNumber() & (SLIDE_MAX-1);

					switch ( m_nSlidingDir ) //m_nSlidingDir &= ~1;
					{
						case SLIDE_TO_LEFT:  m_nSlidingDir = SLIDE_TO_TOP;    break;
						case SLIDE_TO_RIGHT: m_nSlidingDir = SLIDE_TO_BOTTOM; break;
					}

					m_position.y = Y(500.0f); // 571.428589f;
				}
			}
			else
			{
				DMAudio.ChangeMusicMode(MUSICMODE_FRONTEND);

				if ( DMAudio.GetRadioInCar() < 9 )
					m_PrefsRadioStation = DMAudio.GetRadioInCar();
				else
					m_PrefsRadioStation = CGeneral::GetRandomNumber() % 9;

				CTimer::StartUserPause();
				CPad::StopPadsShaking();
				m_nStartPauseTimer = CTimer::GetTimeInMillisecondsPauseMode() + 800;
				m_nSlidingDir = CGeneral::GetRandomNumber() & (SLIDE_MAX-1);

				switch ( m_nSlidingDir )
				{
					case SLIDE_TO_RIGHT:  m_position.y = Y(612.5f); break;
					case SLIDE_TO_LEFT:   m_position.y = Y(612.5f); break;
					case SLIDE_TO_BOTTOM: m_position.y = Y(500.0f); break;
					case SLIDE_TO_TOP:    m_position.y = Y(500.0f); break;
					default:              m_position.y = Y(500.0f); break;
				}

				if ( m_currentPage == PAGE_CONTROLS || m_currentPage == PAGE_BRIEFS )
				{
					m_nSlidingDir = CGeneral::GetRandomNumber() & (SLIDE_MAX-1);

					switch ( m_nSlidingDir ) //m_nSlidingDir &= ~1;
					{
						case SLIDE_TO_LEFT:  m_nSlidingDir = SLIDE_TO_TOP;    break;
						case SLIDE_TO_RIGHT: m_nSlidingDir = SLIDE_TO_BOTTOM; break;
					}

					m_position.y = Y(500.0f); //571.428589f
				}
			}
		}
	}


	if ( m_pageState == PAGESTATE_NORMAL && gMusicPlaying )
	{
		DMAudio.StopFrontEndTrack();
		gMusicPlaying = false;
	}

	if ( m_nChangePageTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() >= m_nChangePageTimer )
	{
		m_nChangePageTimer = 0;
		pMenuNewGame = &MenuPage_NewGame;
		m_currentPage = m_newPage;
	}

	if ( m_nPageLeftTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() >= m_nPageLeftTimer )
		m_nPageLeftTimer = 0;

	if ( m_nPageRightTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() >= m_nPageRightTimer )
		m_nPageRightTimer = 0;

	if ( m_nStartPauseTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() >= m_nStartPauseTimer )
		m_nStartPauseTimer = 0;

	if ( m_nEndPauseTimer != 0 && CTimer::GetTimeInMillisecondsPauseMode() >= m_nEndPauseTimer )
	{
		m_nEndPauseTimer = 0;
		m_bMenuActive = false;
		m_bMenuActive = false;
		CTimer::EndUserPause();
	}
}

void
CMenuManager::ProcessControllerInput(void)
{
	if ( TimeToStopPadShaking != 0 && TimeToStopPadShaking < CTimer::GetTimeInMillisecondsPauseMode() )
	{
		CPad::StopPadsShaking();
		TimeToStopPadShaking = 0;
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadLeft() || CPad::GetPad(0)->GetLeft() )
#else
	if ( CPad::GetPad(0)->GetDPadLeft() )
#endif
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
			case PAGESTATE_HIGHLIGHTED:
				break;

			case PAGESTATE_SELECTED:
			{
				if ( pActiveMenuPage )
					pActiveMenuPage->GoLeftStill();
				break;
			}
		}
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadRight() || CPad::GetPad(0)->GetRight() )
#else
	if ( CPad::GetPad(0)->GetDPadRight() )
#endif
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
			case PAGESTATE_HIGHLIGHTED:
				break;

			case PAGESTATE_SELECTED:
			{
				if ( pActiveMenuPage )
					pActiveMenuPage->GoRightStill();
				break;
			}
		}
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadLeftJustDown() || CPad::GetPad(0)->GetLeftJustDown() )
#else
	if ( CPad::GetPad(0)->GetDPadLeftJustDown() )
#endif
		ProcessDPadLeftJustDown();

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadRightJustDown() || CPad::GetPad(0)->GetRightJustDown() )
#else
	if ( CPad::GetPad(0)->GetDPadRightJustDown() )
#endif
		ProcessDPadRightJustDown();

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadUp() || CPad::GetPad(0)->GetUp() )
#else
	if ( CPad::GetPad(0)->GetDPadUp() )
#endif
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
			{
				if ( m_currentPage == PAGE_STATS )
				{
					if ( pActiveMenuPage )
						pActiveMenuPage->GoUpStill();
				}
				break;
			}

			case PAGESTATE_HIGHLIGHTED:
				break;

			case PAGESTATE_SELECTED:
			{
				if ( pActiveMenuPage )
					pActiveMenuPage->GoUpStill();
				break;
			}
		}
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadDown() || CPad::GetPad(0)->GetDown() )
#else
	if ( CPad::GetPad(0)->GetDPadDown() )
#endif
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
			{
				if ( m_currentPage == PAGE_STATS )
				{
					if ( pActiveMenuPage )
						pActiveMenuPage->GoDownStill();
				}

				break;
			}
			case PAGESTATE_HIGHLIGHTED:
				break;

			case PAGESTATE_SELECTED:
			{
				if ( pActiveMenuPage )
					pActiveMenuPage->GoDownStill();
				break;
			}
		}
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadUpJustDown() || CPad::GetPad(0)->GetUpJustDown() )
#else
	if ( CPad::GetPad(0)->GetDPadUpJustDown() )
#endif
		ProcessDPadUpJustDown();

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetDPadDownJustDown() || CPad::GetPad(0)->GetDownJustDown() )
#else
	if ( CPad::GetPad(0)->GetDPadDownJustDown() )
#endif
		ProcessDPadDownJustDown();

	if ( CPad::GetPad(0)->GetLeftShoulder1JustDown() )
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
				ProcessDPadLeftJustDown();
				break;

			case PAGESTATE_HIGHLIGHTED:
			case PAGESTATE_SELECTED:
				break;
		}
	}

	if ( CPad::GetPad(0)->GetRightShoulder1JustDown() )
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
				ProcessDPadRightJustDown();
				break;

			case PAGESTATE_HIGHLIGHTED:
			case PAGESTATE_SELECTED:
				break;
		}
	}

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetCrossJustDown() || CPad::GetPad(0)->GetEnterJustDown() || CPad::GetPad(0)->GetRightMouseJustDown() )
#else
	if ( CPad::GetPad(0)->GetCrossJustDown() )
#endif
		ProcessDPadCrossJustDown();

#ifdef GTA_PC
	if ( CPad::GetPad(0)->GetTriangleJustDown() || CPad::GetPad(0)->GetBackspaceJustDown() || (m_pageState != PAGESTATE_NORMAL && CPad::GetPad(0)->GetEscapeJustDown()) )
#else
	if ( CPad::GetPad(0)->GetTriangleJustDown() )
#endif
		ProcessDPadTriangleJustDown();
}


void
CMenuManager::ProcessDPadLeftJustDown(void)
{
		switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
		{
			if ( m_nChangePageTimer == 0 )
			{
				if ( --m_newPage < PAGE_FIRST ) m_newPage = PAGE_LAST;

				m_nPageLeftTimer   = CTimer::GetTimeInMillisecondsPauseMode() + 300;
				m_nPageRightTimer  = 0;
				m_nChangePageTimer = CTimer::GetTimeInMillisecondsPauseMode() + 250;
				field_18 = m_newPage;
				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
			}

			break;
		}

		case PAGESTATE_HIGHLIGHTED:
		{
			if ( pActiveMenuPage )
				pActiveMenuPage->GoLeftMenuOnPage();

			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);

			break;
		}

		case PAGESTATE_SELECTED:
		{
			if ( pActiveMenuPage )
			{
				pActiveMenuPage->GoLeft();

				if ( m_currentPage == PAGE_AUDIO)
				{
					if ( pActiveMenuPage->m_pCurrentControl == &MenuAudio_1 )
						;
					else if ( pActiveMenuPage->m_pCurrentControl == &MenuAudio_2 )
						;
					else
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
				else if ( m_currentPage == PAGE_DISPLAY)
				{
					if ( pActiveMenuPage->m_pCurrentControl == &MenuDisplay_1 )
						;
					else
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
				else
				{
					DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
			}

			break;
		}
	}

}

void
CMenuManager::ProcessDPadRightJustDown(void)
{
		switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
		{
			if ( m_nChangePageTimer == 0 )
			{
				if ( ++m_newPage > PAGE_LAST ) m_newPage = PAGE_FIRST;

				m_nPageLeftTimer   = 0;
				m_nPageRightTimer  = CTimer::GetTimeInMillisecondsPauseMode() + 300;
				m_nChangePageTimer = CTimer::GetTimeInMillisecondsPauseMode() + 250;
				field_18 = m_newPage;
				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
			}

			break;
		}

		case PAGESTATE_HIGHLIGHTED:
		{
			if ( pActiveMenuPage )
				pActiveMenuPage->GoRightMenuOnPage();

			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);

			break;
		}

		case PAGESTATE_SELECTED:
		{
			if ( pActiveMenuPage )
			{
				pActiveMenuPage->GoRight();

				if ( m_currentPage == PAGE_AUDIO)
				{
					if ( pActiveMenuPage->m_pCurrentControl == &MenuAudio_1 )
						;
					else if ( pActiveMenuPage->m_pCurrentControl == &MenuAudio_2 )
						;
					else
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
				else if ( m_currentPage == PAGE_DISPLAY)
				{
					if ( pActiveMenuPage->m_pCurrentControl == &MenuDisplay_1 )
						;
					else
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
				else
				{
					DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
			}

			break;
		}
	}

}

void
CMenuManager::ProcessDPadUpJustDown(void)
{
		switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
			break;

		case PAGESTATE_HIGHLIGHTED:
		{
			if ( pActiveMenuPage )
				pActiveMenuPage->GoUpMenuOnPage();

			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
			break;
		}

		case PAGESTATE_SELECTED:
		{
			if ( pActiveMenuPage )
			{
				pActiveMenuPage->GoUp();

				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
			}

			break;
		}

}

void
CMenuManager::ProcessDPadDownJustDown(void)
{
		switch ( m_pageState )
	{
		case PAGESTATE_NORMAL:
			break;

		case PAGESTATE_HIGHLIGHTED:
		{
			if ( pActiveMenuPage )
				pActiveMenuPage->GoDownMenuOnPage();

			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);

			break;
		}

		case PAGESTATE_SELECTED:
		{
			if ( pActiveMenuPage )
			{
				pActiveMenuPage->GoDown();

				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NAVIGATION, 0);
				}
			}
			break;
		}

}

void
CMenuManager::ProcessDPadTriangleJustDown(void)
{
	if ( pActiveMenuPage )
	{
		pActiveMenuPage->SelectDefaultCancelAction();

		if ( m_bMenuActive )
		{
			if ( !bIgnoreTriangleButton )
			{
				switch ( m_pageState )
				{
					case PAGESTATE_NORMAL:
						WorkOutMenuState(true);
						break;

					case PAGESTATE_HIGHLIGHTED:
						m_pageState = PAGESTATE_NORMAL;
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
						break;

					case PAGESTATE_SELECTED:
					{
						m_pageState = PAGESTATE_HIGHLIGHTED;
						if ( pActiveMenuPage )
						{
							if ( pActiveMenuPage->m_numControls == 1 )
							{
								m_pageState = PAGESTATE_NORMAL;
								DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
							}
							else
								DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_BACK, 0);
						}
						break;
					}
				}
			}
		}
	}
	else
	{
		if ( !bIgnoreTriangleButton )
		{
			switch ( m_pageState )
			{
				case PAGESTATE_NORMAL:
					WorkOutMenuState(false);
					break;

				case PAGESTATE_HIGHLIGHTED:
					m_pageState = PAGESTATE_NORMAL;
					DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
					break;

				case PAGESTATE_SELECTED:
				{
					m_pageState = PAGESTATE_HIGHLIGHTED;
					if ( pActiveMenuPage )
					{
						if ( pActiveMenuPage->m_numControls == 1 )
						{
							m_pageState = PAGESTATE_NORMAL;
							DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_NEW_PAGE, 0);
						}
						else
							DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_BACK, 0);
					}
					break;
				}
			}
		}
	}
}

void
CMenuManager::ProcessDPadCrossJustDown(void)
{
		if ( m_currentPage != PAGE_STATS && m_currentPage != PAGE_BRIEFS)
	{
		switch ( m_pageState )
		{
			case PAGESTATE_NORMAL:
			{
				m_pageState = PAGESTATE_HIGHLIGHTED;
				if ( pActiveMenuPage )
				{
					if ( pActiveMenuPage->m_numControls == 1 )
						m_pageState = PAGESTATE_SELECTED;
				}

				switch ( m_currentPage )
				{
					case PAGE_AUDIO:
					{
						if (   pActiveMenuPage->m_pCurrentControl == &MenuAudio_1
							|| pActiveMenuPage->m_pCurrentControl == &MenuAudio_2
							|| pActiveMenuPage->m_pCurrentControl == &MenuAudio_3
							|| pActiveMenuPage->m_pCurrentControl == &MenuAudio_4 )
						{
							if ( !gMusicPlaying )
							{
								DMAudio.PlayFrontEndTrack(m_PrefsRadioStation, TRUE);
								gMusicPlaying = true;
							}
						}
						else
						{
							DMAudio.StopFrontEndTrack();
							gMusicPlaying = false;
						}
						break;
					}
				}

				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_SETTING_CHANGE, 0);
				break;
			}

			case PAGESTATE_HIGHLIGHTED:
			{
				m_pageState = PAGESTATE_SELECTED;
				DoHackingMenusAtPageBrowse();
				if ( pActiveMenuPage )
				{
					if ( pActiveMenuPage->IsActiveMenuTwoState())
					{
						m_pageState = PAGESTATE_HIGHLIGHTED;
						pActiveMenuPage->ActiveMenuTwoState_SelectNextPosition();
					}
				}

				switch ( m_currentPage )
				{
					case PAGE_AUDIO:
					{
						if ( pActiveMenuPage->m_pCurrentControl != &MenuAudio_4 )
							DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_SETTING_CHANGE, 0);

						break;
					}

					default:
					{
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_SETTING_CHANGE, 0);
						DMAudio.StopFrontEndTrack();
						gMusicPlaying = false;
						break;
					}
				}
				break;
			}

			case PAGESTATE_SELECTED:
			{
				if ( pActiveMenuPage )
				{
					pActiveMenuPage->SelectCurrentOptionUnderCursor();

					switch ( m_currentPage )
					{
						case PAGE_AUDIO:
						{
							if ( pActiveMenuPage->m_pCurrentControl != &MenuAudio_3 )
								m_pageState = PAGESTATE_HIGHLIGHTED;
							break;
						}

						case PAGE_NEW_GAME:
						case PAGE_LANGUAGE:
							break;

						default:
							m_pageState = PAGESTATE_HIGHLIGHTED;
							break;
					}
				}

				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MENU_SETTING_CHANGE, 0);
				break;
			}
		}
	}

}

void
CMenuManager::DoHackingMenusAtPageBrowse(void)
{
	if ( pActiveMenuPage )
	{
		switch ( m_currentPage )
		{
			case PAGE_CONTROLS:
			{
				if ( pActiveMenuPage->m_pCurrentControl == &MenuControls_1  )
				{
					int32 sel = MenuControls_1.GetMenuSelection();
					MenuControls_1.GoFirst();

					for ( int32 i = 0; i < sel; i++ )
						MenuControls_1.GoNext();
				}
				break;
			}

			case PAGE_AUDIO:
			{
				if ( pActiveMenuPage->m_pCurrentControl == &MenuAudio_3  )
				{
					int32 sel = MenuAudio_3.GetMenuSelection();
					MenuAudio_3.GoFirst();

					for ( int32 i = 0; i < sel; i++ )
						MenuAudio_3.GoNext();
				}
				break;
			}
		}
	}
}

void
CMenuManager::SetSoundLevelsForMusicMenu(void)
{
	DMAudio.SetMusicMasterVolume(m_PrefsMusicVolume);
	DMAudio.SetEffectsMasterVolume(m_PrefsSfxVolume);
}

void
CMenuManager::FilterOutColorMarkersFromString(wchar *string, CRGBA &color)
{
	wchar buf[300];
	UnicodeStrcpy(buf, string);

	wchar *src = buf;
	wchar *dst = string;
	while ( *src != '\0' )
	{
		if ( *src == '~' )
		{
			src++;

			if ( *src == 'l' )      color = CRGBA(0, 0, 0, 255);
			else if ( *src == 'p' ) color = CRGBA(255, 0, 255, 255);
			else if ( *src == 'y' ) color = CRGBA(255, 255, 0, 255);
			else if ( *src == 'w' ) color = CRGBA(255, 255, 255, 255);
			else if ( *src == 'b' ) color = CRGBA(40, 40, 255, 255);
			else if ( *src == 'g' ) color = CRGBA(40, 235, 40, 255);
			else if ( *src == 'r' ) color = CRGBA(255, 0, 0, 255);

			while ( *src++ != '~' )
				;
		}
		else
			*dst++ = *src++;
	}

	*dst = '\0';
}

#endif
