#include <cassert>
#include "../src/core/CameraViewToggle.h"

// The pedestrian camera toggle only applies to an on-foot player whose camera
// is really following him: vehicles cycle their own views and must be left
// alone, and menus, cutscenes, replays and script cameras all own the input.
static void TestToggleOnlyOnFoot()
{
    assert(CanTogglePedestrianCameraView(true, false, true, false, false, false, false));
    // In a vehicle the camera has its own cycling, so the key must do nothing.
    assert(!CanTogglePedestrianCameraView(true, true, true, false, false, false, false));
    // No player ped at all.
    assert(!CanTogglePedestrianCameraView(false, false, true, false, false, false, false));
    // The camera is not looking at the player, for example during an obbe cam.
    assert(!CanTogglePedestrianCameraView(true, false, false, false, false, false, false));
    // Any input owner that is not the player blocks the toggle.
    assert(!CanTogglePedestrianCameraView(true, false, true, true, false, false, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, true, false, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, false, true, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, false, false, true));
}

// The sticky view must not survive a context change. Movement is deliberately
// absent from the rules, otherwise first person walking would be impossible.
static void TestStickyViewSurvivesWalking()
{
    assert(KeepPedestrianFirstPersonView(true, false, true, false, false, true));
    // A script may keep the first person cam alive without looking at the player.
    assert(KeepPedestrianFirstPersonView(false, true, true, false, false, true));
    assert(!KeepPedestrianFirstPersonView(false, false, true, false, false, true));
    // Leaving the pedestrian target drops the view.
    assert(!KeepPedestrianFirstPersonView(true, false, false, false, false, true));
    assert(!KeepPedestrianFirstPersonView(true, false, true, true, false, true));
    assert(!KeepPedestrianFirstPersonView(true, false, true, false, true, true));
    // A dead or otherwise out of control ped must not stay in first person.
    assert(!KeepPedestrianFirstPersonView(true, false, true, false, false, false));
    // The script escape hatches override the widescreen and free cam refusals.
    assert(KeepPedestrianFirstPersonView(true, true, true, true, true, true));
}

int main()
{
    TestToggleOnlyOnFoot();
    TestStickyViewSurvivesWalking();
    return 0;
}
