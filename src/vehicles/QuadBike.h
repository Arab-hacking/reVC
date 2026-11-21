#ifndef __GTA_QUADBIKE_H__
#define __GTA_QUADBIKE_H__

#include "Automobile.h"
#include "RideAnimData.h"

// SA eQuadBikeNodes (QuadBike.h), verbatim. A quad is a car chassis with bike
// controls: the rear axle hangs outside the body on a live shaft, the two front
// suspension arms move with their wheels, and the handlebars turn with the bars.
enum eQuadBikeNodes {
	QUAD_NODE_NONE   = 0,
	QUAD_CHASSIS     = 1,
	QUAD_WHEEL_RF    = 2,
	QUAD_WHEEL_RM    = 3,
	QUAD_WHEEL_RB    = 4,
	QUAD_WHEEL_LF    = 5,
	QUAD_WHEEL_LM    = 6,
	QUAD_WHEEL_LB    = 7,
	QUAD_DOOR_RF     = 8,
	QUAD_DOOR_RR     = 9,
	QUAD_DOOR_LF     = 10,
	QUAD_DOOR_LR     = 11,
	QUAD_BODY_FRONT  = 12,
	QUAD_BODY_REAR   = 13,
	QUAD_SUSPENSION_RF = 14,
	QUAD_SUSPENSION_LF = 15,
	QUAD_REAR_AXLE   = 16,
	QUAD_HANDLEBARS  = 17,
	QUAD_MISC_A      = 18,
	QUAD_MISC_B      = 19,

	QUAD_NUM_NODES
};

// SA CQuadBike (0x9BC bytes).
class CQuadBike : public CAutomobile
{
public:
	tBikeHandlingData *m_pBikeHandling;
	CRideAnimData m_sRideAnimData;
	float field_9A8[4];	// unused by the game, kept for layout parity
	uint8 m_nQuadFlags;

	CQuadBike(int32 id, uint8 CreatedBy);
	~CQuadBike(void);

	void Fix(void);
	CRideAnimData *GetRideAnimData(void);
	void PreRender(void);
	bool ProcessAI(uint32 &extraHandlingFlags);
	void ProcessControl(void);
	void ProcessControlInputs(uint8 playerNum);
	void ProcessDrivingAnims(CPed *driver, bool blend);
	void ProcessSuspension(void);
	void ResetSuspension(void);
	void SetupDamageAfterLoad(void);
	void SetupSuspensionLines(void);
};

extern bool bDoQuadDamping;			// true
extern float QUAD_HBSTEER_ANIM_MULT;	// -0.4f
extern CVector vecQuadResistance;		// { 0.995f, 0.995f, 1.0f }

#endif // __GTA_QUADBIKE_H__
