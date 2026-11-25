#ifndef __GTA_MOUSE_CONTROLLER_STATE_H__
#define __GTA_MOUSE_CONTROLLER_STATE_H__

class CMouseControllerState
{
public:
	bool LMB;
	bool RMB;
	bool MMB;
	bool WHEELUP;
	bool WHEELDN;
	bool MXB1;
	bool MXB2;
	char _pad0;

	float x, y;

	CMouseControllerState()
		: LMB(false), RMB(false), MMB(false), WHEELUP(false), WHEELDN(false),
		  MXB1(false), MXB2(false), _pad0(0), x(0.0f), y(0.0f)
	{}

	void Clear()
	{
		LMB = RMB = MMB = WHEELUP = WHEELDN = MXB1 = MXB2 = false;
		_pad0 = 0;
		x = y = 0.0f;
	}
};

// Roll the per-frame sample forward and clear the current sample. Mouse motion
// is a delta, so an unavailable sample must never replay the previous frame.
inline void BeginMouseControllerFrame(CMouseControllerState &oldState,
                                     CMouseControllerState &newState)
{
	oldState = newState;
	newState.Clear();
}

#endif // __GTA_MOUSE_CONTROLLER_STATE_H__
