#define WITHWINDOWS
#include "common.h"
#include "crossplatform.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#include "FileMgr.h"
#include "Font.h"
#ifdef MORE_LANGUAGES
#include "Game.h"
#endif
#include "GenericGameStorage.h"
#include "Messages.h"
#include "PCSave.h"
#include "Text.h"

const char* _psGetUserFilesFolder();

C_PcSave PcSaveHelper;

#ifndef ANDROID
// Serialize into a pending file first. A pool that outgrows the legacy save
// format must not truncate the user's previous working slot.
static bool
ReplacePendingSave(const char *pending, const char *slot)
{
#ifdef _WIN32
	return MoveFileExA(pending, slot, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return caserename(pending, slot) == 0;
#endif
}

static void
DiscardPendingSave(const char *pending)
{
#ifdef _WIN32
	DeleteFileA(pending);
#else
	char *path = casepath(pending, false);
	if(path){ std::remove(path); free(path); }
#endif
}
#endif

void
C_PcSave::SetSaveDirectory(const char *path)
{
#if defined ANDROID || defined __APPLE__
	sprintf(DefaultPCSaveFileName, "%s/%s", path, "GTAVCsf");
#if defined ANDROID
    debug("SetSaveDirectory: %s", DefaultPCSaveFileName);
#endif
#else
    sprintf(DefaultPCSaveFileName, "%s\\%s", path, "GTAVCsf");
#endif
}

bool
C_PcSave::DeleteSlot(int32 slot)
{
#ifdef FIX_BUGS
	char FileName[MAX_PATH];
#else
	char FileName[200];
#endif

	PcSaveHelper.nErrorCode = SAVESTATUS_SUCCESSFUL;
	sprintf(FileName, "%s%i.b", DefaultPCSaveFileName, slot + 1);
	DeleteFile(FileName);
	SlotSaveDate[slot][0] = '\0';
	return true;
}

int8
C_PcSave::SaveSlot(int32 slot)
{
	MakeValidSaveName(slot);
	PcSaveHelper.nErrorCode = SAVESTATUS_SUCCESSFUL;
	REVC_LOG_INFO("Saving slot %d to %s", slot + 1, ValidSaveName);
	_psGetUserFilesFolder();
#ifndef ANDROID
	std::string pendingName = std::string(ValidSaveName) + ".pending";
	int file = CFileMgr::OpenFile(pendingName.c_str(), "wb");
#else
	int file = CFileMgr::OpenFile(ValidSaveName, "wb");
#endif
	if (file != 0) {
#ifdef MISSION_REPLAY
		if (!IsQuickSave)
#endif
			DoGameSpecificStuffBeforeSave();
		if (GenericSave(file)) {
			if (!!CFileMgr::CloseFile(file))
				nErrorCode = SAVESTATUS_ERR_SAVE_CLOSE;
#ifndef ANDROID
			if(nErrorCode == SAVESTATUS_SUCCESSFUL &&
			   !ReplacePendingSave(pendingName.c_str(), ValidSaveName))
				nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
			if(nErrorCode != SAVESTATUS_SUCCESSFUL)
				DiscardPendingSave(pendingName.c_str());
#endif
			if(nErrorCode == SAVESTATUS_SUCCESSFUL){
				REVC_LOG_INFO("Saved slot %d successfully", slot + 1);
				return 0;
			}
			REVC_LOG_ERROR("Failed to finish save slot %d (error=%d)", slot + 1, nErrorCode);
			return 2;
		}

		CFileMgr::CloseFile(file);
#ifndef ANDROID
		DiscardPendingSave(pendingName.c_str());
#endif
		REVC_LOG_ERROR("Failed to save slot %d (error=%d)", slot + 1, nErrorCode);
		return 2;
	}
	PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_CREATE;
	REVC_LOG_ERROR("Failed to create save slot %d: %s", slot + 1, ValidSaveName);
	return false;
}

bool
C_PcSave::PcClassSaveRoutine(int32 file, uint8 *data, uint32 size)
{
	CFileMgr::Write(file, (const char*)&size, sizeof(size));
	if (CFileMgr::GetErrorReadWrite(file)) {
		nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		strncpy(SaveFileNameJustSaved, ValidSaveName, sizeof(ValidSaveName) - 1);
		return false;
	}

	CFileMgr::Write(file, (const char*)data, align4bytes(size));
	CheckSum += (uint8) size;
	CheckSum += (uint8) (size >> 8);
	CheckSum += (uint8) (size >> 16);
	CheckSum += (uint8) (size >> 24);
	for (int i = 0; i < align4bytes(size); i++) {
		CheckSum += *data++;
	}
	if (CFileMgr::GetErrorReadWrite(file)) {
		nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		strncpy(SaveFileNameJustSaved, ValidSaveName, sizeof(ValidSaveName) - 1);
		return false;
	}

	return true;
}

void
C_PcSave::PopulateSlotInfo()
{
	for (int i = 0; i < SLOT_COUNT; i++) {
		Slots[i] = SLOT_EMPTY;
		SlotFileName[i][0] = '\0';
		SlotSaveDate[i][0] = '\0';
	}
	for (int i = 0; i < SLOT_COUNT; i++) {
#ifdef FIX_BUGS
		char savename[MAX_PATH];
#else
		char savename[52];
#endif
		struct {
			int size;
			wchar FileName[24];
			SYSTEMTIME SaveDateTime;
		} header;
		sprintf(savename, "%s%i%s", DefaultPCSaveFileName, i + 1, ".b");
		int file = CFileMgr::OpenFile(savename, "rb");
		if (file != 0) {
			CFileMgr::Read(file, (char*)&header, sizeof(header));
			if (strncmp((char*)&header, TopLineEmptyFile, sizeof(TopLineEmptyFile)-1) != 0) {
				Slots[i] = SLOT_OK;
				memcpy(SlotFileName[i], &header.FileName, sizeof(header.FileName));
				
				SlotFileName[i][24] = '\0';
			}
			CFileMgr::CloseFile(file);
		}
		if (Slots[i] == SLOT_OK) {
			if (CheckDataNotCorrupt(i, savename)) {
#ifdef FIX_INCOMPATIBLE_SAVES
				if (!FixSave(i, GetSaveType(savename))) {
					CMessages::InsertNumberInString(TheText.Get("FEC_SLC"), i + 1, -1, -1, -1, -1, -1, SlotFileName[i]);
					Slots[i] = SLOT_CORRUPTED;
					continue;
				}
#endif
				SYSTEMTIME st;
				memcpy(&st, &header.SaveDateTime, sizeof(SYSTEMTIME));
				const char *month;
				switch (st.wMonth)
				{
				case 1: month = "JAN"; break;
				case 2: month = "FEB"; break;
				case 3: month = "MAR"; break;
				case 4: month = "APR"; break;
				case 5: month = "MAY"; break;
				case 6: month = "JUN"; break;
				case 7: month = "JUL"; break;
				case 8: month = "AUG"; break;
				case 9: month = "SEP"; break;
				case 10: month = "OCT"; break;
				case 11: month = "NOV"; break;
				case 12: month = "DEC"; break;
				default: assert(0);
				}
				char date[70];
#ifdef MORE_LANGUAGES
				if (CGame::japaneseGame)
					sprintf(date, "%02d %02d %04d %02d:%02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
				else
#endif // MORE_LANGUAGES
					sprintf(date, "%02d %s %04d %02d:%02d:%02d", st.wDay, UnicodeToAsciiForSaveLoad(TheText.Get(month)), st.wYear, st.wHour, st.wMinute, st.wSecond);
				AsciiToUnicode(date, SlotSaveDate[i]);

			} else {
				CMessages::InsertNumberInString(TheText.Get("FEC_SLC"), i + 1, -1, -1, -1, -1, -1, SlotFileName[i]);
				Slots[i] = SLOT_CORRUPTED;
			}
		}
	}
}
