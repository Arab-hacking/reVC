#ifndef __GTA_CUSTOMTIMECYCLE_H__
#define __GTA_CUSTOMTIMECYCLE_H__

// Read optional data/timecyc.json from custom/common.zip (or the loose
// custom/data/timecyc.json fallback) after native TIMECYC.DAT initialization.
// The first four BR weathers and eight day-time slots are interpolated to the
// game's 24-hour weather tables. Every platform receives ambient/directional
// light and native shadow strengths. With [Rendering] GL3Atmosphere=1, GL3
// also imports sky/sun/cloud colours and scales fog start to the existing VC
// far clip; it does not import water, draw distance, PostFX, snow or other ZIPs.
// Without a valid file the native timecycle is left intact.
class CCustomTimecycle
{
public:
	static void Apply(void);
};

#endif // __GTA_CUSTOMTIMECYCLE_H__
