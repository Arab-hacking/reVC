#ifndef __GTA_NOCLIP_H__
#define __GTA_NOCLIP_H__

// F10 toggles player noclip while playing. The player (or their vehicle when
// driving) flies through world geometry; this does not pin streamed objects.
class CNoClip
{
public:
	static void ProcessInput(void); // after CPad::UpdatePads, before world physics
	static void Move(void);         // after world physics, before camera update
	static void Reset(void);        // before the player/world is destroyed
	static bool IsActive(void);
};

#endif
