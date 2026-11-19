// Save-block tests for the script layer, retargeted at the post-story build.
//
// The old version of this file checked that a save's script block restored
// running CRunningScript objects and that a mismatched main.scm was refused.
// Both ideas are gone: there is no interpreter, no main.scm is read, and no
// script threads exist to restore. What still has to hold is the container
// contract, because the save format reserves a slot for script state and the
// loader path must not corrupt a game load when it sees one:
//
//   1. SaveAllScripts produces a well-formed SCR block of a stable size.
//   2. LoadAllScripts accepts exactly that block and is a no-op on world state.
//   3. Truncated, mis-headed or oversized blocks are refused cleanly, without
//      touching OnAMissionFlag or leaving half-restored script state.

#include "common.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "SaveBuf.h"
#include "Script.h"

static void Check(bool ok, const char *what)
{
	if (!ok) {
		printf("FAILED: %s\n", what);
		assert(ok);
	}
}

// The suite links against the script save code without the rest of the engine,
// so the handful of globals it touches are defined here.
const uint32 CRunningScript::nSaveStructSize =
#ifdef COMPATIBLE_SAVES
	136;
#else
	sizeof(CRunningScript);
#endif

uint8 CTheScripts::ScriptSpace[SIZE_SCRIPT_SPACE];
uint32 CTheScripts::OnAMissionFlag = 0;
uint32 CTheScripts::LastMissionPassedTime = 0;

int
main(void)
{
	static uint8 saveBuf[65536];
	uint32 savedSize = 0;

	// 1. A save writes a block with the SCR header and a sensible length.
	CTheScripts::OnAMissionFlag = 0;
	CTheScripts::LastMissionPassedTime = 0;
	CTheScripts::SaveAllScripts(saveBuf, &savedSize);
	Check(savedSize > SAVE_HEADER_SIZE, "SaveAllScripts produced a non-empty block");
	Check(savedSize < sizeof(saveBuf), "SaveAllScripts stayed inside the buffer");
	Check(saveBuf[0] == 'S' && saveBuf[1] == 'C' && saveBuf[2] == 'R' && saveBuf[3] == '\0',
		"SCR header written");

	// 2. The same block loads, and loading is inert: it must not resurrect a
	//    mission flag that the build no longer tracks.
	CTheScripts::OnAMissionFlag = 0xDEADBEEF;
	Check(CTheScripts::LoadAllScripts(saveBuf, savedSize), "own block accepted");
	Check(CTheScripts::OnAMissionFlag == 0, "load leaves no mission flag behind");

	// 3. Truncated block: refused, and nothing is written.
	Check(!CTheScripts::LoadAllScripts(saveBuf, 32), "truncated SCR block rejected");

	// 4. Wrong header: refused.
	uint8 badHeader[65536];
	memcpy(badHeader, saveBuf, savedSize);
	badHeader[0] = 'X';
	Check(!CTheScripts::LoadAllScripts(badHeader, savedSize), "bad SCR header rejected");

	// 5. Length field disagreeing with the block: refused, because that is how a
	//    scripted-build save (far larger than ours) announces itself.
	uint8 badLength[65536];
	memcpy(badLength, saveBuf, savedSize);
	uint32 inflated = savedSize * 4;
	memcpy(badLength + 4, &inflated, sizeof(inflated));
	Check(!CTheScripts::LoadAllScripts(badLength, savedSize), "length mismatch rejected");

	// 6. Overlong internal size field: refused rather than trusted.
	uint8 badVarSpace[65536];
	memcpy(badVarSpace, saveBuf, savedSize);
	uint32 huge = SIZE_SCRIPT_SPACE * 2;
	memcpy(badVarSpace + SAVE_HEADER_SIZE, &huge, sizeof(huge));
	Check(!CTheScripts::LoadAllScripts(badVarSpace, savedSize), "oversized variable space rejected");

	printf("Script save validation tests OK\n");
	return 0;
}
