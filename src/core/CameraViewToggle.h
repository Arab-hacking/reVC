#ifndef __REVC_CAMERA_VIEW_TOGGLE_H__
#define __REVC_CAMERA_VIEW_TOGGLE_H__

// Vanilla only ever entered the pedestrian first person cam while the player
// held the look-around axis, and on PC that entry is unreachable: it requires
// !Using3rdPersonMouseCam(), which is true in MODE_FOLLOWPED whenever the mouse
// third person cam is enabled. The sticky view below is the toggled variant,
// so the player can stay in first person while walking.
//
// The rules are split on purpose:
//
//   * the *preference* is what the player asked for. It only flips on a real
//     key press and on an explicit context change (getting into a vehicle),
//     so a single frame where a condition happens to be false cannot silently
//     turn the mode back off.
//   * the *effective* view is the preference ANDed with the per-frame
//     conditions. Those conditions legitimately flicker (a script grabs the
//     camera for one frame, the ped is briefly airborne, a cutscene rail comes
//     up), and computing them every frame keeps the mode consistent without
//     losing the player's choice.
//
// The rules are kept free of engine types so they can be unit tested.

// A toggle request only makes sense for an on-foot player whose camera is
// actually following him. Vehicles have their own view cycling and must be
// left alone, otherwise the two bindings fight each other.
inline bool CanTogglePedestrianCameraView(bool hasPlayerPed, bool inVehicle,
                                          bool cameraLookingAtPlayer,
                                          bool menuActive, bool cutsceneRunning,
                                          bool replayPlayingBack,
                                          bool scriptControlsCamera)
{
    if(!hasPlayerPed || inVehicle)
        return false;
    if(!cameraLookingAtPlayer)
        return false;
    if(menuActive || cutsceneRunning || replayPlayingBack)
        return false;
    if(scriptControlsCamera)
        return false;
    return true;
}

// Whether the player's first person preference may be applied this frame.
// Note that walking is deliberately absent: the sticky view must survive
// movement, unlike the vanilla look-around peek.
inline bool CanApplyPedestrianFirstPersonView(bool cameraLookingAtPlayer,
                                              bool scriptFirstPersonEnabled,
                                              bool targetIsPed, bool wideScreenOn,
                                              bool freeCamActive,
                                              bool pedInControl)
{
    if(!cameraLookingAtPlayer && !scriptFirstPersonEnabled)
        return false;
    if(!targetIsPed)
        return false;
    if(wideScreenOn && !scriptFirstPersonEnabled)
        return false;
    if(freeCamActive && !scriptFirstPersonEnabled)
        return false;
    if(!pedInControl)
        return false;
    return true;
}

#endif
