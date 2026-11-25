#include <cassert>
#include "../src/core/MouseControllerState.h"

static_assert(sizeof(CMouseControllerState) == 0x10,
              "mouse controller ABI layout must remain unchanged");

static void TestClearResetsMotionAndButtons()
{
    CMouseControllerState state;
    state.x = 17.0f;
    state.y = -9.0f;
    state.LMB = true;
    state.WHEELUP = true;

    state.Clear();

    assert(state.x == 0.0f);
    assert(state.y == 0.0f);
    assert(!state.LMB);
    assert(!state.WHEELUP);
}

static void TestMissingSampleCannotReplayPreviousDelta()
{
    CMouseControllerState oldState;
    CMouseControllerState newState;
    newState.x = 12.0f;
    newState.y = -5.0f;

    // First frame rolls the actual movement into oldState and clears the
    // current sample. A missing input sample therefore reads as no motion.
    BeginMouseControllerFrame(oldState, newState);
    assert(oldState.x == 12.0f);
    assert(oldState.y == -5.0f);
    assert(newState.x == 0.0f);
    assert(newState.y == 0.0f);

    // Another frame with no sample must not replay that same delta.
    BeginMouseControllerFrame(oldState, newState);
    assert(oldState.x == 0.0f);
    assert(oldState.y == 0.0f);
    assert(newState.x == 0.0f);
    assert(newState.y == 0.0f);
}

int main()
{
    TestClearResetsMotionAndButtons();
    TestMissingSampleCannotReplayPreviousDelta();
    return 0;
}
