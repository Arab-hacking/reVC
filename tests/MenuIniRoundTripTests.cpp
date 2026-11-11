// The menu's .ini round-trip must survive a row that has a save key but no
// value pointer.
//
// The "FPS LIMIT" row on the TEST GRAPHICS page draws its label from a callback
// and used to be built as `CCFODynamic(nil, "TestGraphics", "MaxFps", ...)`.
// Both LoadINISettings() and SaveINISettings() read and write the value through
// `*(int8*)option.m_CFO->value` for every row that has a save key, so the null
// pointer dereferenced during startup and the game died with SIGSEGV right
// after `CFileMgr::OpenFile failed: gta_vc.set`.
//
// Two things are checked here:
//   1. the guard the round-trip now applies (`value != nil`) is present, so the
//      same mistake can never crash the boot again;
//   2. no row in MenuScreensCustom.cpp is built with a save key and no value.
//
// It is a source-level test on purpose: the menu is a translation unit full of
// RenderWare state that cannot be instantiated on the host, but the mistake is
// visible in the initialiser list.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;
static int checks = 0;

static void
Check(bool cond, const std::string &what)
{
	checks++;
	if(!cond){
		printf("FAIL: %s\n", what.c_str());
		failures++;
	}
}

static std::string
ReadFile(const char *path, bool &ok)
{
	ok = false;
	FILE *f = fopen(path, "rb");
	if(!f)
		return std::string();
	std::string out;
	char buf[4096];
	size_t n;
	while((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	fclose(f);
	ok = true;
	return out;
}

// True when `text` contains `needle` (kept explicit so failures print nicely).
static bool
Has(const std::string &text, const char *needle)
{
	return text.find(needle) != std::string::npos;
}

static void
TestRoundTripGuards(void)
{
	bool ok;
	std::string re3 = ReadFile("../src/core/re3.cpp", ok);
	Check(ok, "src/core/re3.cpp is readable from the test working directory");
	if(!ok)
		return;

	// Load path: the migrated/legacy-read branch.
	Check(Has(re3, "option.m_CFO->save && option.m_CFO->value"),
	      "LoadINISettings requires a non-nil value before reading a row");
	// Save path: the crash that was reported.
	Check(Has(re3, "StoreIni(option.m_CFO->saveCat, option.m_CFO->save, *(int8*)option.m_CFO->value);"),
	      "SaveINISettings still stores int8 rows");
	// The guard must appear at least twice: once per direction.
	size_t first = re3.find("option.m_CFO->save && option.m_CFO->value");
	size_t second = first == std::string::npos ? std::string::npos
	                                          : re3.find("option.m_CFO->save && option.m_CFO->value", first + 1);
	Check(first != std::string::npos && second != std::string::npos,
	      "both the load and the save loop guard against a nil value");
}

static void
TestNoSaveWithoutValue(void)
{
	bool ok;
	std::string menu = ReadFile("../src/core/MenuScreensCustom.cpp", ok);
	Check(ok, "src/core/MenuScreensCustom.cpp is readable from the test working directory");
	if(!ok)
		return;

	// Walk the initialiser lists and look for `new CCFO...(nil, "<cat>", "<key>"`
	// -- i.e. a nil value together with a save category and key.
	static const char *ctors[] = { "new CCFOSelect(", "new CCFOSlider(", "new CCFODynamic(" };
	int offenders = 0;
	for(size_t i = 0; i < sizeof(ctors)/sizeof(ctors[0]); i++){
		size_t pos = 0;
		while((pos = menu.find(ctors[i], pos)) != std::string::npos){
			pos += strlen(ctors[i]);
			size_t close = menu.find(')', pos);
			if(close == std::string::npos)
				break;
			std::string args = menu.substr(pos, close - pos);
			if(args.compare(0, 5, "nil, ") == 0){
				// nil value: the next two arguments must also be nil/nil.
				size_t comma = args.find(',');
				size_t comma2 = comma == std::string::npos ? std::string::npos
				                                           : args.find(',', comma + 1);
				bool noSave = comma2 != std::string::npos &&
				              args.find("nil", comma + 1) == comma + 2 &&
				              args.find("nil", comma2 + 1) == comma2 + 2;
				if(!noSave){
					printf("  offending row: %s%s)\n", ctors[i], args.c_str());
					offenders++;
				}
			}
		}
	}
	Check(offenders == 0, "every CFO row with a save key has a value pointer");
}

int
main(void)
{
	TestRoundTripGuards();
	TestNoSaveWithoutValue();

	if(failures == 0)
		printf("menu ini round-trip: %d checks ok\n", checks);
	else
		printf("menu ini round-trip: %d of %d checks FAILED\n", failures, checks);
	return failures != 0;
}
