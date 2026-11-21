#ifndef __GTA_TRAILER_H__
#define __GTA_TRAILER_H__

#include "Automobile.h"

// SA eTrailerNodes (Trailer.h), verbatim.
enum eTrailerNodes {
	TRAILER_NODE_NONE  = 0,
	TRAILER_CHASSIS    = 1,
	TRAILER_WHEEL_RF   = 2,
	TRAILER_WHEEL_RM   = 3,
	TRAILER_WHEEL_RB   = 4,
	TRAILER_WHEEL_LF   = 5,
	TRAILER_WHEEL_LM   = 6,
	TRAILER_WHEEL_LB   = 7,
	TRAILER_DOOR_RF    = 8,
	TRAILER_DOOR_RR    = 9,
	TRAILER_DOOR_LF    = 10,
	TRAILER_DOOR_LR    = 11,
	TRAILER_BUMP_FRONT = 12,
	TRAILER_BUMP_REAR  = 13,
	TRAILER_WING_RF    = 14,
	TRAILER_WING_LF    = 15,
	TRAILER_BONNET     = 16,
	TRAILER_BOOT       = 17,
	TRAILER_WINDSCREEN = 18,
	TRAILER_EXHAUST    = 19,
	TRAILER_MISC_A     = 20,
	TRAILER_MISC_B     = 21,
	TRAILER_MISC_C     = 22,

	TRAILER_NUM_NODES
};

// SA CTrailer (0x9F4 bytes).
//
// A trailer is a car with no drivetrain and two extra legs: the four wheels
// carry it while it rolls, and when it is left standing two support struts
// come down at the rear so it does not tip backwards. Whether the struts are
// down is m_fTrailerTowedRatio - 1 fully extended, 0 retracted for towing.
class CTrailer : public CAutomobile
{
	static const int32 NUM_TRAILER_WHEELS = 4;
	static const int32 NUM_TRAILER_SUPPORTS = 2;
	static const int32 NUM_TRAILER_SUSP_LINES = NUM_TRAILER_WHEELS + NUM_TRAILER_SUPPORTS;

public:
	CColPoint m_aSupportColPoints[NUM_TRAILER_SUPPORTS];
	float m_aSupportRatios[NUM_TRAILER_SUPPORTS];
	float m_fHeight;				// where the support leg tops out
	float m_fTrailerTowedRatio;		// 1 = legs down, 0 = raising/retracted
	float m_fTrailerTowedRatio2;	// the ratio the supports are actually asked to give

	static constexpr float m_fTrailerSuspensionForce = 1.5f;
	static constexpr float m_fTrailerDampingForce = 0.1f;

	CTrailer(int32 id, uint8 CreatedBy);
	~CTrailer(void);

	void SetupSuspensionLines(void);
	void ProcessControl(void);
	bool ProcessAI(uint32 &extraHandlingFlags);
	void ProcessSuspension(void);
	void PreRender(void);
	void ResetSuspension(void);

	bool GetTowHitchPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle);
	bool GetTowBarPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle);
	bool SetTowLink(CVehicle *tractor, bool setMyPosToTowBar);
	bool BreakTowLink(void);

	void ScanForTowLink(void);

	int32 ProcessEntityCollision(CEntity *entity, CColPoint *colpoint);
};

// A luggage trailer has no supports to speak of: SA marks it with a ratio
// outside the 0..1 range so every support code path is skipped by one compare.
constexpr float BAGGAGE_TRAILER_TOWED_RATIO = -1000.0f;

#endif // __GTA_TRAILER_H__
