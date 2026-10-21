// Stand-ins for the game-side symbols CustomTimecycle.cpp needs when it runs
// outside of the game: the debug log prints to stdout, and ReadDataFile
// serves the files of a real BlackRussia "common" folder (TC_DATA_DIR).
#include "common.h"

#include <stdarg.h>
#include <string>
#include <vector>

#include "CustomModels.h"
#include "DebugLog.h"

void
DebugLogInit(void) {}

void
DebugLogEnsure(void) {}

void
DebugLogPrintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

const char *
DebugLogPath(void)
{
	return "<stdout>";
}

bool
CCustomModels::ReadDataFile(const char *relpath, std::vector<uint8> &out)
{
	const char *dir = getenv("TC_DATA_DIR");
	if(dir == nil || dir[0] == '\0')
		return false;
	std::string path = std::string(dir) + "/" + relpath;
	FILE *f = fopen(path.c_str(), "rb");
	if(f == nil)
		return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if(size <= 0){
		fclose(f);
		return false;
	}
	out.resize(size);
	bool ok = fread(out.data(), 1, size, f) == (size_t)size;
	fclose(f);
	if(ok)
		printf("data file %s: read (%u bytes)\n", relpath, (unsigned)out.size());
	return ok;
}
