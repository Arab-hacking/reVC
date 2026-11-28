#include "common.h"

#include "Script.h"
#include "Weather.h"

// Weather is the one scripted feature this build keeps. Missions, shops,
// traffic and the police dispatcher were all driven by opcodes from
// data/main.scm and are gone, but being able to change the sky is core world
// behaviour, so the four weather commands survive here.
//
// They are reachable exactly as before: a script thread calls the weather
// opcode through CRunningScript::ProcessOneCommand. What no longer happens is
// the engine loading main.scm to reach them. A multiplayer layer starts its
// own threads and issues these commands itself, or sets CWeather directly.
//
// The opcode numbers are spelled out rather than taken from an enum because
// ScriptCommands.h - the 1500-line table the interpreter needed - was removed
// along with the rest of the bytecode.

enum {
	COMMAND_FORCE_WEATHER      = 446,
	COMMAND_FORCE_WEATHER_NOW  = 447,
	COMMAND_RELEASE_WEATHER    = 448,
	COMMAND_FORCE_RAIN         = 1066,
};

int8 CRunningScript::ProcessWeatherCommands(int32 command)
{
	switch (command) {
	case COMMAND_FORCE_WEATHER:
		CollectParameters(&m_nIp, 1);
		CWeather::ForceWeather(ScriptParams[0]);
		return 0;
	case COMMAND_FORCE_WEATHER_NOW:
		CollectParameters(&m_nIp, 1);
		CWeather::ForceWeatherNow(ScriptParams[0]);
		return 0;
	case COMMAND_RELEASE_WEATHER:
		CWeather::ReleaseWeather();
		return 0;
	case COMMAND_FORCE_RAIN:
		CollectParameters(&m_nIp, 1);
		CWeather::bScriptsForceRain = !!ScriptParams[0];
		return 0;
	}
	return -1;
}
