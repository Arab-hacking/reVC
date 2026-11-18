#ifndef __REVC_CAMERA_VIEW_TOGGLE_H__
#define __REVC_CAMERA_VIEW_TOGGLE_H__

// Vanilla only ever entered the pedestrian first person cam while the player
// held the look-around axis, and on PC that entry is unreachable: it requires
// !Using3rdPersonMouseCam(), which is true in MODE_FOLLOWPED whenever the mouse
// third person cam is enabled. The sticky view below is the toggled variant,
// so the player can stay in first person while walking.
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

// The sticky view survives movement and has no input timeout, but must drop
// as soon as the camera stops following the player, the target stops being a
// pedestrian, the widescreen/cutscene rail takes over or the ped loses control.
inline bool KeepPedestrianFirstPersonView(bool cameraLookingAtPlayer,
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
