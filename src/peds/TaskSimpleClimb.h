#ifndef __GTA_TASKSIMPLECLIMB_H__
#define __GTA_TASKSIMPLECLIMB_H__

#include "SurfaceTable.h"
#include "ColModel.h"
#include "ColPoint.h"
#include "AnimBlendAssociation.h"

class CPed;
class CEntity;

// SA eClimbHeights (TaskSimpleClimb.h). The climb runs through these in order:
// grab the wall, pull up, stand on the edge, then finish. CLIMB_VAULT is the
// short alternative for a waist-high obstacle, which skips the pull-up.
enum eClimbHeights : int8 {
	CLIMB_NOT_READY = 0,
	CLIMB_GRAB,
	CLIMB_PULLUP,
	CLIMB_STANDUP,
	CLIMB_FINISHED,
	CLIMB_VAULT,
	CLIMB_FINISHED_V
};

// SA CTaskSimpleClimb (TaskSimpleClimb.cpp).
//
// reVC has no task manager, so this is the climb itself without the task
// wrapper: a ped-owned object that is stepped once per frame from
// CPed::ProcessControl while it is active. The behaviour is SA's, function for
// function:
//
//   TestForClimb    SphereCast the climb model against the world and decide
//                   whether a wall can be grabbed at all, returning the entity
//                   that was grabbed.
//   ProcessPed      Drive the ped from hold to hold: move it toward the next
//                   SA offset, animate the phase, and finish (or abort) when
//                   the phase ends.
//   StartAnim       Blend the clip for the current phase and set its speed from
//                   the player's body stats.
//   TestForStandUp / TestForVault
//                   Cast the stand-up and vault models at the edge to decide
//                   whether there is room to stand or the climb must become a
//                   vault.
//
// The geometry is SA's: the sphere models below are the ones the original
// builds in CTaskSimpleClimb::CreateColModel, sphere for sphere, with the same
// bound sphere and box. The piece index of each sphere is what ScanToGrab
// tests against to decide whether a hit is a grabbable ledge or one of the
// low spheres that are too close to the ground.
class CTaskSimpleClimb
{
public:
	// SA ms_fMinForStretchGrab.
	static constexpr float ms_fMinForStretchGrab = 1.40f;

	// The handhold offsets, in the ped's local space, for each phase. SA keeps
	// them as file-scope constants next to the code that uses them.
	static constexpr float ms_fHangingOffsetHorz = -0.40f;
	static constexpr float ms_fHangingOffsetVert = -1.10f;
	static constexpr float ms_fAtEdgeOffsetHorz = -0.40f;
	static constexpr float ms_fAtEdgeOffsetVert = 0.00f;
	static constexpr float ms_fStandUpOffsetHorz = 0.15f;
	static constexpr float ms_fStandUpOffsetVert = 1.00f;
	static constexpr float ms_fVaultOffsetHorz = 0.50f;
	static constexpr float ms_fVaultOffsetVert = 0.00f;

	CTaskSimpleClimb(CEntity *climbEntity, const CVector &vecTarget, float fHeading,
	                 eSurfaceType nSurfaceType, eClimbHeights nHeight, bool bForceClimb);
	~CTaskSimpleClimb();

	// Returns true when the climb has finished and the ped may drop this object.
	bool ProcessPed(CPed *ped);

	// SA CTaskSimpleClimb::TestForClimb (0x6803A0). Looks for a grabbable wall
	// in front of the ped and, if it finds one, also checks that there is not a
	// second hold just behind it (which would mean the ped is in a niche).
	static CEntity *TestForClimb(CPed *ped, CVector &outClimbPos, float &outClimbHeading,
	                             eSurfaceType &outSurfaceType, bool bLaunch);

	// Sweeps the climb model over the sectors around the ped. bStandUp and
	// bVault select the stand-up and vault models; pedPosition overrides where
	// the ped is tested from.
	static CEntity *ScanToGrab(CPed *ped, CVector &outClimbPos, float &outClimbAngle,
	                           eSurfaceType &surfaceType, bool bLaunch, bool bStandUp,
	                           bool bVault, CVector *pedPosition);

	// Builds the four collision models SA creates on first use, and releases
	// them at shutdown.
	static bool CreateColModel();
	static void Shutdown();

	bool TestForStandUp(CPed *ped, const CVector &point, float fAngle) const;
	bool TestForVault(CPed *ped, const CVector &point, float fAngle) const;

	void StartAnim(CPed *ped);
	void StartSpeech(CPed *ped) const;
	static void DeleteAnimCB(CAnimBlendAssociation *anim, void *data);

	// The point the camera should look at while the climb plays.
	void GetCameraTargetPos(CPed *ped, CVector &vecTarget) const;

	auto GetHeightForPos() const { return m_HeightForPos; }
	auto GetIsInvalidClimb() const { return m_IsInvalidClimb; }

private:
	static CVector GetClimbOffset3D(CVector2D offset, float angle);
	// A physical climb entity (a car, a boat that stops) carries the handhold
	// in its own space, so it has to be transformed back into the world.
	static void GetHandholdPosAndAngleForEntity(CEntity *entity, const CVector &handPos,
	                                            float handAngle, CVector &outPos, float &outAngle);

	bool m_HasFinished;
	bool m_HasToChangeAnimation;
	bool m_HasToChangePosition;
	bool m_HasToForceClimb;
	bool m_IsInvalidClimb;
	eClimbHeights m_HeightForAnim;
	eClimbHeights m_HeightForPos;
	eSurfaceType m_SurfaceType;
	int8 m_FallAfterVault;
	float m_HandholdHeading;
	CVector m_HandholdPos;
	CEntity *m_ClimbEntity;
	uint16 m_GetToPosCounter;
	CAnimBlendAssociation *m_Anim;
};

#endif // __GTA_TASKSIMPLECLIMB_H__
