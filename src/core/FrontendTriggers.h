void InitialiseTextsInMenuControllerInCar(CMenuPictureAndText *widget, CMenuManager::CONTRCONFIG cont);
void InitialiseTextsInMenuControllerOnFoot(CMenuPictureAndText *widget, CMenuManager::CONTRCONFIG cont);
void TriggerNewGame_BackToStart(CMenuMultiChoiceTriggered *widget);
void TriggerNewGame_Confirm(CMenuMultiChoiceTriggered *widget);
void TriggerNewGame_Start(CMenuMultiChoiceTriggered *widget);

void
DisplayWarningControllerMsg()
{
	if ( CPad::bDisplayNoControllerMessage )
	{
		CSprite2d::DrawRect(CRect(X(20.0f), Y(140.0f), X(620.0f), Y(328.0)), CRGBA(64, 16, 16, 224)); // CRect(20.0f, 160.0f, 620.0f, 374.857117f)
		
		CFont::SetFontStyle(FONT_BANK);
		CFont::SetBackgroundOff();
		CFont::SetScale(X(0.84f), Y(1.26f)); // 1.440000
		CFont::SetPropOn();
		CFont::SetCentreOff();
		CFont::SetJustifyOn();
		CFont::SetRightJustifyOff();
		CFont::SetBackGroundOnlyTextOn();
		CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f
		
		CPlaceableShText text;
		text.SetPosition(X(60.0f), Y(180.0f), false); // 205.714294
		text.SetColor(CRGBA(152, 152, 152, 255));
		text.m_text = TheText.Get("NOCONTE"); // Please re-insert the analog controller (DUALSHOCK@) or analog controller (DUALSHOCK@2) in controller port 1 to continue
		text.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR*2.0f);
		text.SetAlpha(255);
		text.DrawShWrap(0.0f, 0.0f, X(600.0f+SHADOW_VECTOR.x), YF(600.0f)); //TODO check
		
		CFont::DrawFonts();
	}
	else if ( CPad::bObsoleteControllerMessage )
	{
		CSprite2d::DrawRect(CRect(X(20.0f), Y(140.0f), X(620.0f), Y(328.0)), CRGBA(64, 16, 16, 224)); // CRect(20.0f, 160.0f, 620.0f, 374.857117f)
		CFont::SetFontStyle(FONT_BANK);
		CFont::SetBackgroundOff();
		CFont::SetScale(X(0.84f), Y(1.26f)); // 1.440000
		CFont::SetPropOn();
		CFont::SetCentreOff();
		CFont::SetJustifyOn();
		CFont::SetRightJustifyOff();
		CFont::SetBackGroundOnlyTextOn();
		CFont::SetWrapx(SCRW-X(40.0f)); // 600.0f
		
		CPlaceableShText text;
		text.SetPosition(X(60.0f), Y(180.0f), false); // 205.714294
		text.SetColor(CRGBA(152, 152, 152, 255));
		text.m_text = TheText.Get("WRCONTE"); // Please re-insert the analog controller (DUALSHOCK@) or analog controller (DUALSHOCK@2) in controller port 1 to continue
		text.SetShadows(true, TEXT_SHADOW_COLOR, SHADOW_VECTOR*2.0f);
		text.SetAlpha(255);
		text.DrawShWrap(0.0f, 0.0f, X(600.0f+SHADOW_VECTOR.x), YF(600.0f)); //TODO check
		
		CFont::DrawFonts();
	}
	
}


int32 nStatLinesIndex;
wchar aStatLines[50+1][50];
wchar *PrintStatLine(char const *text, void *stat, unsigned char itsFloat, void *stat2)
{
	if (text && stat && nStatLinesIndex < 50)
	{
		char  line [64];
		wchar uline[64];
		
		memset(line, 0, sizeof(line));
		memset(uline, 0, sizeof(uline));

		if (stat2)
		{
			if ( itsFloat )
				sprintf(line, "  %.2f %s %.2f", *(float*)stat, UnicodeToAscii(TheText.Get("FEST_OO")), *(float*)stat2);
			else
				sprintf(line, "  %d %s %d", *(int32*)stat, UnicodeToAscii(TheText.Get("FEST_OO")), *(int32*)stat2);
		}
		else
		{
			if (itsFloat)
				sprintf(line, "  %.2f", *(float*)stat);
			else
				sprintf(line, "  %d", *(int32*)stat);
		}
		
		wchar *pStatLine = aStatLines[nStatLinesIndex++];
		
		AsciiToUnicode(line, uline);
		UnicodeStrcpy(pStatLine, uline);
		
		return pStatLine;
	}

	return nil;
}












void
TriggerControls_Vibrations(CMenuOnOffTriggered *widget)
{
	if ( widget )
	{
		CMenuManager::m_PrefsUseVibration = widget->GetMenuSelection();
		if ( CMenuManager::m_PrefsUseVibration )
		{
			CPad::GetPad(0)->StartShake(300, 150);
			TimeToStopPadShaking = CTimer::GetTimeInMillisecondsPauseMode() + 500;
		}
	}
}

void
TriggerControls_ContrDisplay(CMenuMultiChoiceTriggeredAlways *widget)
{
	if ( widget )
	{
		int32 conf = MenuControls_1.GetMenuSelection();
		int32 i  = MenuControls_2.GetMenuSelection();
		if ( i == 1 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_7;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_4;
		}
		else if ( i == 0 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_6;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_3;
		}
	}
}

void
TriggerControls_DrawHNContrConfig(CMenuMultiChoiceTriggeredAlways *widget)
{
	if ( widget )
	{
		int32 conf = widget->GetMenuSelection();
		
		InitialiseTextsInMenuControllerOnFoot(&MenuControls_3, (CMenuManager::CONTRCONFIG)conf);
		InitialiseTextsInMenuControllerInCar (&MenuControls_4, (CMenuManager::CONTRCONFIG)conf);
		
		int32 i = MenuControls_2.GetMenuSelection();
		if ( i == 1 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_7;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_4;
		}
		else if ( i == 0 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_6;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_3;
		}
	}
}

void
TriggerControls_DrawContrConfig(CMenuMultiChoiceTriggeredAlways *widget)
{
	if ( widget )
	{
		int32 conf = widget->GetMenuSelection();
		if ( widget->m_cursor != -1 )
			conf = widget->m_cursor;
		
		InitialiseTextsInMenuControllerOnFoot(&MenuControls_3, (CMenuManager::CONTRCONFIG)conf);
		InitialiseTextsInMenuControllerInCar(&MenuControls_4, (CMenuManager::CONTRCONFIG)conf);
		
		int32 i = MenuControls_2.GetMenuSelection();
		if ( i == 1 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_7;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_4;
		}
		else if ( i == 0 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_6;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_3;
		}
	}
}

void
TriggerControls_ContrConfig(CMenuMultiChoiceTriggered *widget)
{
	if ( widget )
	{
		int32 conf = widget->GetMenuSelection();
		
		InitialiseTextsInMenuControllerOnFoot(&MenuControls_3, (CMenuManager::CONTRCONFIG)conf);
		InitialiseTextsInMenuControllerInCar(&MenuControls_4, (CMenuManager::CONTRCONFIG)conf);
		
		int32 i = MenuControls_2.GetMenuSelection();
		if ( i == 1 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_7;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_4;
		}
		else if ( i == 0 )
		{
			if ( conf == CMenuManager::CONFIG_2 )
				MenuPage_Controls.m_controls[0] = &MenuControls_6;
			else
				MenuPage_Controls.m_controls[0] = &MenuControls_3;
		}
	}
}

void
TriggerLanguage_Language(CMenuMultiChoiceTriggered *widget)
{	
	if ( widget )
	{
		if ( CMenuManager::m_PrefsLanguage != widget->GetMenuSelection() )
		{
			CMenuManager::m_PrefsLanguage = widget->GetMenuSelection();
			FrontEndMenuManager.m_bInitialised = false;
			bFrontEnd_ReloadObrTxtGxt = true;
		}
	}
}

void
TriggerAudio_RadioStation(CMenuMultiChoicePicturedTriggered *widget)
{
	if ( widget )
	{
		if ( CMenuManager::m_PrefsRadioStation != widget->GetMenuSelection() )
		{
			CMenuManager::m_PrefsRadioStation = widget->GetMenuSelection();
			DMAudio.PlayFrontEndTrack(CMenuManager::m_PrefsRadioStation, TRUE);
			DMAudio.SetRadioInCar(CMenuManager::m_PrefsRadioStation);
		}
	}
}

void
TriggerAudio_StereoMono(CMenuMultiChoiceTriggered *widget)
{
	if ( widget )
	{	
		if (widget->GetMenuSelection() == 1)
		{
			DMAudio.SetMonoMode(TRUE);
			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_MONO, 0);
		}
		else
		{
			DMAudio.SetMonoMode(FALSE);
			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_STEREO, 0);
		}
	}
}

void
TriggerAudio_MusicVolumeAlways(CMenuSliderTriggered *widget)
{
	;
}

void
TriggerAudio_SfxVolumeAlways(CMenuSliderTriggered *widget)
{
	if ( widget )
	{
		static bool bTriggerTest = false;
		
		CMenuManager::m_PrefsSfxVolume = float(widget->GetMenuSelection()) / 100.0f * 127.0f + 0.5f;
		
		if ( CMenuManager::m_PrefsSfxVolume == 102 && !CPad::GetPad(0)->GetDPadLeft()&& !CPad::GetPad(0)->GetDPadRight() )
		{
			if ( bTriggerTest )
			{
				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_AUDIO_TEST, 0);
				bTriggerTest = false;
			}
		}
		else
			bTriggerTest = true;
		
		FrontEndMenuManager.SetSoundLevelsForMusicMenu();
	}
}

void
TriggerAudio_MusicVolume(CMenuSliderTriggered *widget)
{	
	if ( widget )
	{
		CMenuManager::m_PrefsMusicVolume = float(widget->GetMenuSelection()) / 100.0f * 127.0f + 0.5f;
		FrontEndMenuManager.SetSoundLevelsForMusicMenu();
	}
}

void
TriggerAudio_SfxVolume(CMenuSliderTriggered *widget)
{
	;
}

void
TriggerNewGame_Start(CMenuMultiChoiceTriggered *widget)
{
	FrontEndMenuManager.m_bWantToRestart = true;
	FrontEndMenuManager.m_bMenuActive    = false;
	bIgnoreTriangleButton = false;

	CTimer::EndUserPause();

	FrontEndMenuManager.AnaliseMenuContents();

	DMAudio.SetEffectsFadeVol(0);
	DMAudio.SetMusicFadeVol(0);
	DMAudio.ResetTimers(CTimer::GetTimeInMilliseconds());
}

void
TriggerNewGame_Confirm(CMenuMultiChoiceTriggered *widget)
{
	// Are you sure you want to start a new game? All progress will be lost. Proceed?
	MenuNewGameQ_1.m_numTexts = 0;
	MenuNewGameQ_1.AddText(TheText.Get("FESZ_QR"), X(-100.0f), 0.0f, TEXT_COLOR, 0);

	MenuNewGameQ_2.m_numOptions = 0;
	MenuNewGameQ_2.AddOption(TheText.Get("FEM_NO"),  X(80.0f), YF(30.0f), TriggerNewGame_BackToStart, 0, 0);
	MenuNewGameQ_2.AddOption(TheText.Get("FEM_YES"), X(80.0f), YF(10.0f), TriggerNewGame_Start, 0, 0);

	MenuPage_NewGameQuestion.ActivatePage();
	pMenuNewGame = &MenuPage_NewGameQuestion;
	bIgnoreTriangleButton = true;
}

void
TriggerNewGame_BackToStart(CMenuMultiChoiceTriggered *widget)
{
	pMenuNewGame = &MenuPage_NewGame;
	pMenuNewGame->ActivatePage();
	bIgnoreTriangleButton = false;
}








void InitialiseTextsInMenuControllerInCar(CMenuPictureAndText *widget, CMenuManager::CONTRCONFIG cont)
{
	if ( widget )
	{
		widget->m_numTexts = 0;
		
		switch ( cont )
		{
			case CMenuManager::CONFIG_1:
			{
				widget->AddText(TheText.Get("FEC_LL"),  X(50.0f),  Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_RSC"), X(-4.0f),  Y(29.0f),  PAD_TEXT_COLOR, true);  // 33.142860f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_HO3"), X(84.0f),  Y(162.0f), PAD_TEXT_COLOR, false); // 185.142868f
				widget->AddText(TheText.Get("FEC_CAM"), X(103.0f), Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"), X(130.0f), Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_LB"),  X(68.0f),  Y(-6.0f),  PAD_TEXT_COLOR, false); // -6.857143f
				widget->AddText(TheText.Get("FEC_LR"),  X(184.0f), Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_HAB"), X(238.0f), Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_BRA"), X(155.0f), Y(18.0f),  PAD_TEXT_COLOR, true);  //  20.571430f
				widget->AddText(TheText.Get("FEC_EXV"), X(238.0f), Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_CAW"), X(238.0f), Y(65.0f),  PAD_TEXT_COLOR, false); //  74.285721f
				widget->AddText(TheText.Get("FEC_ACC"), X(238.0f), Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_TUC"), X(238.0f), Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_SM3"), X(238.0f), Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				
				break;
			}
			
			case CMenuManager::CONFIG_2:
			{
				widget->AddText(TheText.Get("FEC_LL"),  X(50.0f),  Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_HOR"), X(-4.0f),  Y(29.0f),  PAD_TEXT_COLOR, true);  // 33.142860f
				widget->AddText(TheText.Get("FEC_CAM"), X(-4.0f),  Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_NA"),  X(84.0f),  Y(162.0f), PAD_TEXT_COLOR, false); // 185.142868f
				widget->AddText(TheText.Get("FEC_RSC"), X(103.0f), Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"), X(130.0f), Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_LB"),  X(68.0f),  Y(-6.0f),  PAD_TEXT_COLOR, false); // -6.857143f
				widget->AddText(TheText.Get("FEC_LR"),  X(184.0f), Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_HAB"), X(238.0f), Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_BRA"), X(155.0f), Y(18.0f),  PAD_TEXT_COLOR, true);  //  20.571430f
				widget->AddText(TheText.Get("FEC_EXV"), X(238.0f), Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_CAW"), X(238.0f), Y(65.0f),  PAD_TEXT_COLOR, false); //  74.285721f
				widget->AddText(TheText.Get("FEC_ACC"), X(238.0f), Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_TUC"), X(238.0f), Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_SM3"), X(238.0f), Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				
				break;
			}
			
			case CMenuManager::CONFIG_3:
			{
				widget->AddText(TheText.Get("FEC_LL"),  X(50.0f),  Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_EXV"), X(-4.0f),  Y(29.0f),  PAD_TEXT_COLOR, true);  // 33.142860f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_RS3"), X(84.0f),  Y(162.0f), PAD_TEXT_COLOR, false); // 185.142868f
				widget->AddText(TheText.Get("FEC_CAM"), X(103.0f), Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"), X(130.0f), Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_LB"),  X(68.0f),  Y(-6.0f),  PAD_TEXT_COLOR, false); // -6.857143f
				widget->AddText(TheText.Get("FEC_LR"),  X(184.0f), Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_HOR"), X(238.0f), Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_BRA"), X(155.0f), Y(18.0f),  PAD_TEXT_COLOR, true);  //  20.571430f
				widget->AddText(TheText.Get("FEC_HAB"), X(238.0f), Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_CAW"), X(238.0f), Y(65.0f),  PAD_TEXT_COLOR, false); //  74.285721f
				widget->AddText(TheText.Get("FEC_ACC"), X(238.0f), Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_TUC"), X(238.0f), Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_SM3"), X(238.0f), Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				
				break;
			}
			
			case CMenuManager::CONFIG_4:
			{
				widget->AddText(TheText.Get("FEC_LL"),  X(50.0f),  Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_HAB"), X(-4.0f),  Y(29.0f),  PAD_TEXT_COLOR, true);  // 33.142860f
				widget->AddText(TheText.Get("FEC_TUC"), X(-4.0f),  Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_VES"), X(-4.0f),  Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_HO3"), X(84.0f),  Y(162.0f), PAD_TEXT_COLOR, false); //  185.142868f
				widget->AddText(TheText.Get("FEC_CAM"), X(103.0f), Y(141.0f), PAD_TEXT_COLOR, false); //  161.142868f
				widget->AddText(TheText.Get("FEC_PAU"), X(130.0f), Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_LB"),  X(68.0f),  Y(-6.0f),  PAD_TEXT_COLOR, false); //  -6.857143f
				widget->AddText(TheText.Get("FEC_LR"),  X(184.0f), Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_CAW"), X(238.0f), Y(25.0f),  PAD_TEXT_COLOR, false); //  28.571430f
				widget->AddText(TheText.Get("FEC_SMT"), X(155.0f), Y(18.0f),  PAD_TEXT_COLOR, true);  //  20.571430f
				widget->AddText(TheText.Get("FEC_EXV"), X(238.0f), Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_RSC"), X(238.0f), Y(65.0f),  PAD_TEXT_COLOR, false); //  74.285721f
				widget->AddText(TheText.Get("FEC_NA"),  X(238.0f), Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_ACC"), X(238.0f), Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_BRA"), X(238.0f), Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				
				break;
			}
		}
	}
}

void InitialiseTextsInMenuControllerOnFoot(CMenuPictureAndText *widget, CMenuManager::CONTRCONFIG cont)
{
	if ( widget )
	{
		widget->m_numTexts = 0;
		
		
		switch ( cont )
		{
			case CMenuManager::CONFIG_1:
			{
				widget->AddText(TheText.Get("FEC_CWL"),  X(50.0f),   Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_LOF"),  X(-4.0f),   Y(25.0f),  PAD_TEXT_COLOR, true);  // 28.571430f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_CAM"),  X(103.0f),  Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"),  X(130.0f),  Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_CWR"),  X(184.0f),  Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_TAR"),  X(238.0f),  Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_JUM"),  X(144.0f),  Y(18.0f),  PAD_TEXT_COLOR, true);  // 20.571430f
				widget->AddText(TheText.Get("FEC_ENV"),  X(238.0f),  Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_ATT"),  X(238.0f),  Y(65.0f),  PAD_TEXT_COLOR, false); // 74.285721f
				widget->AddText(TheText.Get("FEC_RUN"),  X(238.0f),  Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_FPC"),  X(238.0f),  Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_LB3"),  X(238.0f),  Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				widget->AddText(TheText.Get("FEC_R3"),   X(238.0f),  Y(122.0f), PAD_TEXT_COLOR, false); // 139.428574f
				
				break;
			}
		
			case CMenuManager::CONFIG_2:
			{
				widget->AddText(TheText.Get("FEC_CWL"),  X(50.0f),   Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_LOF"),  X(-4.0f),   Y(25.0f),  PAD_TEXT_COLOR, true);  // 28.571430f
				widget->AddText(TheText.Get("FEC_CAM"),  X(-4.0f),   Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_NA"),   X(103.0f),  Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"),  X(130.0f),  Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_CWR"),  X(184.0f),  Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_TAR"),  X(238.0f),  Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_JUM"),  X(144.0f),  Y(18.0f),  PAD_TEXT_COLOR, true);  // 20.571430f
				widget->AddText(TheText.Get("FEC_ENV"),  X(238.0f),  Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_ATT"),  X(238.0f),  Y(65.0f),  PAD_TEXT_COLOR, false); // 74.285721f
				widget->AddText(TheText.Get("FEC_RUN"),  X(238.0f),  Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_FPC"),  X(238.0f),  Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_LB3"),  X(238.0f),  Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				widget->AddText(TheText.Get("FEC_R3"),   X(238.0f),  Y(122.0f), PAD_TEXT_COLOR, false); // 139.428574f
				
				break;
			}
		
			case CMenuManager::CONFIG_3:
			{
				widget->AddText(TheText.Get("FEC_CWL"),  X(50.0f),   Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_ENV"),  X(-4.0f),   Y(25.0f),  PAD_TEXT_COLOR, true);  // 28.571430f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_CAM"),  X(103.0f),  Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"),  X(130.0f),  Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_CWR"),  X(184.0f),  Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_TAR"),  X(238.0f),  Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_JUM"),  X(144.0f),  Y(18.0f),  PAD_TEXT_COLOR, true);  // 20.571430f
				widget->AddText(TheText.Get("FEC_LOF"),  X(238.0f),  Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_RUN"),  X(238.0f),  Y(65.0f),  PAD_TEXT_COLOR, false); // 74.285721f
				widget->AddText(TheText.Get("FEC_ATT"),  X(238.0f),  Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_FPC"),  X(238.0f),  Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_LB3"),  X(238.0f),  Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				widget->AddText(TheText.Get("FEC_R3"),   X(238.0f),  Y(122.0f), PAD_TEXT_COLOR, false); // 139.428574f
				
				break;
			}
			
			case CMenuManager::CONFIG_4:
			{
				widget->AddText(TheText.Get("FEC_CWL"),  X(50.0f),   Y(-14.0f), PAD_TEXT_COLOR, true);  // -16.0f
				widget->AddText(TheText.Get("FEC_TAR"),  X(-4.0f),   Y(25.0f),  PAD_TEXT_COLOR, true);  // 28.571430f
				widget->AddText(TheText.Get("FEC_NA"),   X(-4.0f),   Y(65.0f),  PAD_TEXT_COLOR, true);  // 74.285721f
				widget->AddText(TheText.Get("FEC_MOV"),  X(-4.0f),   Y(97.0f),  PAD_TEXT_COLOR, true);  // 110.857147f
				widget->AddText(TheText.Get("FEC_CAM"),  X(103.0f),  Y(141.0f), PAD_TEXT_COLOR, false); // 161.142868f
				widget->AddText(TheText.Get("FEC_PAU"),  X(130.0f),  Y(128.0f), PAD_TEXT_COLOR, false); // 146.285721f
				widget->AddText(TheText.Get("FEC_CWR"),  X(184.0f),  Y(-14.0f), PAD_TEXT_COLOR, false); // -16.0f
				widget->AddText(TheText.Get("FEC_ATT"),  X(238.0f),  Y(25.0f),  PAD_TEXT_COLOR, false); // 28.571430f
				widget->AddText(TheText.Get("FEC_JUM"),  X(144.0f),  Y(18.0f),  PAD_TEXT_COLOR, true);  // 20.571430f
				widget->AddText(TheText.Get("FEC_ENV"),  X(238.0f),  Y(52.0f),  PAD_TEXT_COLOR, false); // 59.428574f
				widget->AddText(TheText.Get("FEC_LOF"),  X(238.0f),  Y(65.0f),  PAD_TEXT_COLOR, false); // 74.285721f
				widget->AddText(TheText.Get("FEC_RUN"),  X(238.0f),  Y(78.0f),  PAD_TEXT_COLOR, false); // 89.142860f
				widget->AddText(TheText.Get("FEC_FPC"),  X(238.0f),  Y(94.0f),  PAD_TEXT_COLOR, false); // 107.428574f
				widget->AddText(TheText.Get("FEC_LB3"),  X(238.0f),  Y(109.0f), PAD_TEXT_COLOR, false); // 124.571434f
				widget->AddText(TheText.Get("FEC_R3"),   X(238.0f),  Y(122.0f), PAD_TEXT_COLOR, false); // 139.428574f
				
				break;
			}
		}
	}
}



void
SetRandomActiveTextlineColor(uint8 bText)
{
	{
		bool bSelected = false;
		bool bHighlignted = false;
		
		switch ( FrontEndMenuManager.m_pageState )
		{
			case PAGESTATE_NORMAL:
				break;
			case PAGESTATE_HIGHLIGHTED:
				bHighlignted = true;
				break;
			case PAGESTATE_SELECTED:
				bSelected = true;
				break;
		}
		
		if ( bSelected || bText )
		{
			static uint32 delayTime = 0;
			static bool bAddVal = true;
			
			if ( delayTime < CTimer::GetTimeInMillisecondsPauseMode() )
			{
				delayTime = CTimer::GetTimeInMillisecondsPauseMode() + 200;
				
				if ( bAddVal )
					rgbaATC = TEXT_COLOR;
				else
					rgbaATC = SELECTED_TEXT_COLOR;
				
				bAddVal = !bAddVal;
			}
		}
		
		if ( bHighlignted )
		{
			static uint32 delayTime = 0;
			static bool bAddVal = true;
			
			if ( delayTime < CTimer::GetTimeInMillisecondsPauseMode() )
			{
				delayTime = CTimer::GetTimeInMillisecondsPauseMode() + 200;
				
				if ( bAddVal )
					rgbaATC = TITLE_TEXT_COLOR;
				else
					rgbaATC = MENU_SELECTED_COLOR;
				
				bAddVal = !bAddVal;
			}
		}
	}
}

#ifdef GTA_PC

void
TriggerDisplay_Trails(CMenuOnOffTriggered *widget)
{
	if ( widget )
	{
		CMenuManager::m_PrefsShowTrails = widget->GetMenuSelection();
		CMBlur::BlurOn = CMenuManager::m_PrefsShowTrails;
		
		if ( CMBlur::BlurOn )
			CMBlur::MotionBlurOpen(Scene.camera);
		else
			CMBlur::MotionBlurClose();
	}
}

#endif
