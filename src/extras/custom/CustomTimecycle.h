#ifndef __GTA_CUSTOMTIMECYCLE_H__
#define __GTA_CUSTOMTIMECYCLE_H__

// The lighting and the sky of the BlackRussia client, applied to this game.
//
// The client keeps its time cycle not as TIMECYC.DAT but as a JSON: 22
// weathers, each with 8 day-time slots (the San Andreas clock hours 0, 5, 7,
// 9, 12, 17, 19, 22), one object per slot with named colours and values
// ("AmbientRGB", "DirectionalRGB", "SkyTopRGB", "FogStart", "FarClip"...).
// On top of that, "data/timecyclePresets/*.json" override single slots, and
// "data/weather.json" carries the weather related extras (snow).
//
// This module reads those files out of the custom folder (the archives or
// loose, wherever the common archive was dropped), maps the first four
// weathers onto the four this game has, expands the 8 slots into the 24
// hours the game interpolates over, and fills CTimeCycle's tables in place
// of what TIMECYC.DAT put there. Fields this game does not have (the two
// PostFX layers of the client) are skipped and logged.
//
// Without the files nothing changes - the game keeps its own time cycle.

class CCustomTimecycle
{
public:
	// call after CTimeCycle::Initialise; reads and applies what it finds
	static void Apply(void);
};

#endif // __GTA_CUSTOMTIMECYCLE_H__
