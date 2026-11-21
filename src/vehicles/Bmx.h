#ifndef __GTA_BMX_H__
#define __GTA_BMX_H__

#include "Bike.h"

// SA eBmxNodes (Bmx.h), verbatim. A BMX is a bike with the drivetrain exposed:
// the two wheels and the two fork halves are the bike set, and SA adds the
// handlebars, the chainset and the two pedals so the crank can turn with the
// rider's legs.
enum eBmxNodes {
	BMX_NODE_NONE   = 0,
	BMX_CHASSIS     = 1,
	BMX_FORKS_FRONT = 2,
	BMX_FORKS_REAR  = 3,
	BMX_WHEEL_FRONT = 4,
	BMX_WHEEL_REAR  = 5,
	BMX_HANDLEBARS  = 6,
	BMX_CHAINSET    = 7,
	BMX_PEDAL_R     = 8,
	BMX_PEDAL_L     = 9,

	BMX_NUM_NODES
};

// SA CBmx (0x838 bytes). Only the BMX family - BMX and MTB - is built on this
// class; the motorbikes stay on CBike, which is why every BMX-specific field
// here is about pedalling and bunny hopping rather than about the engine.
class CBmx : public CBike
{
public:
	float m_fControlJump;		// bunny hop charge, 0 .. 25
	float m_fControlPedaling;	// always 0; SA kept it for the unbuilt pedal input
	float m_fSprintLeanAngle;	// lean added by the sprint animation
	float m_fCrankAngle;		// chainset rotation
	float m_fPedalAngleL;
	float m_fPedalAngleR;
	// The two vertical offsets the wheel geometry gives the middle of the
	// frame, measured once in the constructor from the model hierarchy.
	float m_fMidWheelDistY;
	float m_fMidWheelFracY;
	bool m_bIsFreewheeling;

	CBmx(int32 id, uint8 CreatedBy);
	~CBmx(void);

	// from CVehicle
	void BlowUpCar(CEntity *ent);
	bool SetUpWheelColModel(CColModel *colModel);
	void BurstTyre(uint8 tyre, bool applyForces);
	void ProcessControl(void);
	void ProcessDrivingAnims(CPed *driver, bool blend);
	float FindWheelWidth(bool bRear);
	void PreRender(void);
	bool ProcessAI(uint32 &extraHandlingFlags);

	void GetFrameOffset(float &fZOffset, float &fAngleOffset);
	void ProcessBunnyHop(void);
	static void LaunchBunnyHopCB(CAnimBlendAssociation *assoc, void *data);
};

#endif // __GTA_BMX_H__
