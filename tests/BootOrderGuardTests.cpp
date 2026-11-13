// Nothing on the boot path may touch RenderWare before the engine exists.
//
// The order on Android is:
//
//   RsInitialize()                 <- psInitialize() -> FrontEndMenuManager.LoadSettings()
//       LoadINISettings()          <- reads reVC.ini, then TestGraphicsApply()
//   RsRwInitialize()
//       RwEngineInit()             <- Engine::memfuncs is finally filled in
//       RwEngineOpen() / RwEngineStart()
//       InitialiseRenderWare()
//           CustomPipeInit()       <- builds the reflection map
//
// Until `RwEngineInit`, `rw::Engine::memfuncs.rwmalloc` is a nil function
// pointer, so any `rwMalloc` from the settings path is an indirect call to
// address 0. On device that showed up as
//
//   CRASH-CTX: ... fault_addr=0x0 pc=0x0 ...
//   CRASH-LR: base.apk+0x50bf70
//   #00 librevc.so+0x3afee4
//
// logged straight after `LoadINISettings` and before `Platform initialized`,
// with no line from the steps that follow in LoadSettings. The trigger was the
// `refl` row of the TEST GRAPHICS page: a non-zero value made the apply pass
// rebuild the reflection render target while the engine was still dead.
//
// The fix is a gate: TestGraphics records the requested size but only rebuilds
// after CustomPipeInit (TestGraphicsEngineReady). This test pins the gate to the
// code, because the bug is invisible on the host and only reproduces on a device
// whose reVC.ini asks for reflections.
//
// Source-level on purpose: TestGraphics.cpp needs the full RenderWare headers,
// which the host test build does not have.

#include <cstdio>
#include <cstring>
#include <string>

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

static bool
Has(const std::string &text, const char *needle)
{
	return text.find(needle) != std::string::npos;
}

// The apply pass must not reach the reflection target while the pipelines are
// not up, and Game.cpp must hand over once CustomPipeInit has run.
static void
TestApplyIsGated(void)
{
	bool ok;
	std::string gfx = ReadFile("../src/extras/custom/TestGraphics.cpp", ok);
	Check(ok, "src/extras/custom/TestGraphics.cpp is readable");
	if(!ok)
		return;

	Check(Has(gfx, "s_pipesReady"),
	      "TestGraphics keeps an explicit \"pipelines exist\" flag");
	Check(Has(gfx, "if(!CanTouchRenderWare())"),
	      "the reflection target is only rebuilt after the gate opens");
	Check(Has(gfx, "TestGraphicsEngineReady"),
	      "TestGraphicsEngineReady opens the gate");

	// The gate must be checked before EnvMapReinit, not after.
	size_t gate = gfx.find("if(!CanTouchRenderWare())");
	size_t call = gfx.find("EnvMapReinit(");
	Check(gate != std::string::npos && call != std::string::npos && gate < call,
	      "the gate is tested before EnvMapReinit is called");

	std::string hdr = ReadFile("../src/extras/custom/TestGraphics.h", ok);
	Check(ok && Has(hdr, "TestGraphicsEngineReady(void)"),
	      "TestGraphicsEngineReady is declared in the header");
}

// The reflection map is created by CustomPipeInit, which runs from
// InitialiseRenderWare; Game.cpp has to call back once it has.
static void
TestGameCallsBack(void)
{
	bool ok;
	std::string game = ReadFile("../src/core/Game.cpp", ok);
	Check(ok, "src/core/Game.cpp is readable");
	if(!ok)
		return;

	size_t init = game.find("CustomPipes::CustomPipeInit();");
	size_t ready = game.find("TestGraphicsEngineReady();");
	Check(init != std::string::npos && ready != std::string::npos && init < ready,
	      "Game.cpp opens the gate after CustomPipeInit");
}

// Belt and braces: the raster helper itself refuses to run while the engine is
// not open, so a future caller cannot reintroduce the crash.
static void
TestEnvMapReinitIsSafe(void)
{
	bool ok;
	std::string pipes = ReadFile("../src/extras/custompipes.cpp", ok);
	Check(ok, "src/extras/custompipes.cpp is readable");
	if(!ok)
		return;

	size_t fn = pipes.find("EnvMapReinit(int32 size)");
	Check(fn != std::string::npos, "EnvMapReinit is still the entry point");
	if(fn == std::string::npos)
		return;

	size_t state = pipes.find("rw::Engine::state < rw::Engine::Opened", fn);
	size_t shutdown = pipes.find("EnvMapShutdownNoMask();", fn);
	Check(state != std::string::npos,
	      "EnvMapReinit returns early while the engine is not open");
	Check(state != std::string::npos && shutdown != std::string::npos && state < shutdown,
	      "the engine check comes before the first raster is touched");
}

int
main(void)
{
	TestApplyIsGated();
	TestGameCallsBack();
	TestEnvMapReinitIsSafe();

	if(failures == 0)
		printf("boot order guard: %d checks ok\n", checks);
	else
		printf("boot order guard: %d of %d checks FAILED\n", failures, checks);
	return failures != 0;
}
