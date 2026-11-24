#ifndef __GTA_PED_H__
#define __GTA_PED_H__

#include "RwHelper.h"
#include "AnimManager.h"
#include "Crime.h"
#include "PedIK.h"
#include "VehicleScanner.h"
#include "PedModelInfo.h"
#include "PedStdBonePositions.h"
#include "Physical.h"
#include "Weapon.h"
#include "WeaponInfo.h"
#include "Collision.h"

#define FEET_OFFSET	1.04f
#define CHECK_NEARBY_THINGS_MAX_DIST	15.0f
#define ENTER_CAR_MAX_DIST	30.0f
#define CAN_SEE_ENTITY_ANGLE_THRESHOLD	DEGTORAD(60.0f)

class CAccident;
class CObject;
class CFire;
struct AnimBlendFrameData;
class CAnimBlendAssociation;
class CTaskSimpleClimb;
class CTaskSimpleSwim;

struct PedAudioData
{
	int m_nFixedDelayTime;
	int m_nOverrideFixedDelayTime;
	int m_nOverrideMaxRandomDelayTime;
	int m_nMaxRandomDelayTime;
};

enum
{
	ATTACK_IN_PROGRESS,
	CANT_ATTACK,
	WATCH_UNTIL_HE_DISAPPEARS,
};

enum FightState {
	FIGHTSTATE_MOVE_FINISHED = -2,
	FIGHTSTATE_JUST_ATTACKED,
	FIGHTSTATE_NO_MOVE,
	FIGHTSTATE_1
};

enum
{
	ENDFIGHT_NORMAL,
	ENDFIGHT_WITH_A_STEP,
	ENDFIGHT_FAST
};

enum PedRouteType
{
	PEDROUTE_STOP_WHEN_DONE = 1,
	PEDROUTE_GO_BACKWARD_WHEN_DONE,
	PEDROUTE_GO_TO_START_WHEN_DONE
};

enum FightMoveHitLevel
{
	HITLEVEL_NULL,
	HITLEVEL_GROUND,
	HITLEVEL_LOW,
	HITLEVEL_MEDIUM,
	HITLEVEL_HIGH
};

struct FightMove
{
	AnimationId animId;
	float startFireTime;
	float endFireTime;
	float comboFollowOnTime;
	float strikeRadius;
	float extendReachMultiplier;
	uint8 hitLevel; // FightMoveHitLevel
	uint8 damage;
	uint8 flags;
	// The SA melee clips live in their own groups (melee_1..4, kick_std), not
	// in the standard ped set, so the group a clip was found in travels with
	// the move and is the one used to play it.
	AssocGroupId animGroup;
};

// TODO: This is eFightState on mobile.
enum PedFightMoves
{
	FIGHTMOVE_NULL,
	// Attacker
	FIGHTMOVE_STDPUNCH,
	FIGHTMOVE_IDLE,
	FIGHTMOVE_SHUFFLE_F,
	FIGHTMOVE_KNEE,
	FIGHTMOVE_PUNCHHOOK,
	FIGHTMOVE_PUNCHJAB,
	FIGHTMOVE_PUNCH,
	FIGHTMOVE_LONGKICK,
	FIGHTMOVE_ROUNDHOUSE,
	// Directionals
	FIGHTMOVE_FWDLEFT,
	FIGHTMOVE_FWDRIGHT,
	FIGHTMOVE_BACKKICK,
	FIGHTMOVE_BACKFLIP,
	FIGHTMOVE_BACKLEFT,
	FIGHTMOVE_BACKRIGHT,
	FIGHTMOVE_RIGHTSWEEP,
	// Special
	FIGHTMOVE_GROUNDKICK,
	// Opponent
	FIGHTMOVE_HITFRONT,
	FIGHTMOVE_HITBACK,
	FIGHTMOVE_HITRIGHT,
	FIGHTMOVE_HITLEFT,
	FIGHTMOVE_HITBODY,
	FIGHTMOVE_HITCHEST,
	FIGHTMOVE_HITHEAD,
	FIGHTMOVE_HITBIGSTEP,
	FIGHTMOVE_HITONFLOOR,
	FIGHTMOVE_HITBEHIND,
	FIGHTMOVE_MELEE1,
	FIGHTMOVE_MELEE2,
	FIGHTMOVE_MELEE3,
	FIGHTMOVE_IDLE2NORM,
	NUM_FIGHTMOVES
};

enum eObjective {
	OBJECTIVE_NONE,
	OBJECTIVE_LEAVE_CAR,
	OBJECTIVE_ENTER_CAR_AS_DRIVER,
	OBJECTIVE_ENTER_CAR_AS_PASSENGER,
	OBJECTIVE_LEAVE_CAR_AND_DIE
};

enum {
	RANDOM_CHAR = 1,
	MISSION_CHAR,
	UNK_CHAR,
};

enum PedLineUpPhase {
	LINE_UP_TO_CAR_START,
	LINE_UP_TO_CAR_END,
	LINE_UP_TO_CAR_2, // Buggy. Used for cops arresting you from passenger door
	LINE_UP_TO_CAR_FALL
};

enum PedOnGroundState {
	NO_PED,
	PED_IN_FRONT_OF_ATTACKER,
	PED_ON_THE_FLOOR,
	PED_DEAD_ON_THE_FLOOR
};

enum PointBlankNecessity {
	NO_POINT_BLANK_PED,
	POINT_BLANK_FOR_WANTED_PED,
	POINT_BLANK_FOR_SOMEONE_ELSE
};

// ePedState, 65 values, numbered exactly as San Andreas numbers them. The
// order is part of the save format: CPed::m_nPedState is written raw, so an
// entry inserted anywhere but the end changes the meaning of every value
// stored after it. PED_DIE_BY_STEALTH is the entry Vice City does not have -
// it sits between PED_DEAD and PED_CARJACK and pushes the last seven states
// up by one - and SAVE_VERSION below is what keeps an old save readable
// through that shift.
enum PedState
{
	PED_NONE,
	PED_IDLE,
	PED_LOOK_ENTITY,
	PED_LOOK_HEADING,
	PED_WANDER_RANGE,
	PED_WANDER_PATH,
	PED_SEEK_POSITION,
	PED_SEEK_ENTITY,
	PED_FLEE_POSITION,
	PED_FLEE_ENTITY,
	PED_PURSUE,
	PED_FOLLOW_PATH,
	PED_SNIPER_MODE,
	PED_ROCKETLAUNCHER_MODE,
	PED_DUMMY,
	PED_PAUSE,
	PED_ATTACK,
	PED_FIGHT,
	PED_CHAT,
	PED_MUG,
	PED_AIMGUN,
	PED_AI_CONTROL,
	PED_SEEK_CAR,
	PED_SEEK_BOAT_POSITION,
	PED_FOLLOW_ROUTE,
	PED_CPR,
	PED_SOLICIT,
	PED_BUY_ICE_CREAM,
	PED_EVADE_STEP,
	PED_ON_FIRE,
	PED_SUNBATHE,
	PED_FLASH,
	PED_JOG,
	PED_ANSWER_MOBILE,

	PED_HANG_OUT,	// Same with IDLE, but also infects up to 5 peds with same pedType and WANDER_PATH, so they become stone too. HANG_OUT in Fire_Head's idb

	PED_STATES_NO_AI,

	PED_SIT,
	PED_JUMP,
	PED_FALL,
	PED_GETUP,
	PED_STAGGER,
	PED_EVADE_DIVE,

	PED_STATES_CAN_SHOOT,
	PED_ARREST_PLAYER,

	PED_DRIVING,
	PED_PASSENGER,
	PED_TAXI_PASSENGER,
	PED_OPEN_DOOR,
	PED_DIE,
	PED_DEAD,
	PED_DIE_BY_STEALTH,
	PED_CARJACK,
	PED_DRAGGED_FROM_CAR,
	PED_ENTER_CAR,
	PED_STEAL_CAR,
	PED_EXIT_CAR,
	PED_HANDS_UP,
	PED_ARRESTED,
	PED_DEPLOY_STINGER
};

extern float gfTommyFatness;

class CVehicle;

// Index into the PedType array. The San Andreas set differs from the Vice City
// one from the third slot on: the two extra players become the network player
// and an unused slot, and the tail carries dealers, medics, bums and the eight
// mission types. This is the numbering `ped.dat` uses.
enum ePedType
{
	PEDTYPE_PLAYER1 = 0,
	PEDTYPE_PLAYER2,
	PEDTYPE_PLAYER_NETWORK,
	PEDTYPE_PLAYER_UNUSED,
	PEDTYPE_CIVMALE,
	PEDTYPE_CIVFEMALE,

	PEDTYPE_COP,

	PEDTYPE_GANG1,		// Ballas
	PEDTYPE_GANG2,		// Grove Street Families
	PEDTYPE_GANG3,		// Los Santos Vagos
	PEDTYPE_GANG4,		// San Fierro Rifa
	PEDTYPE_GANG5,		// Da Nang Boys
	PEDTYPE_GANG6,		// Mafia
	PEDTYPE_GANG7,		// Mountain Cloud Triad
	PEDTYPE_GANG8,		// Varrio Los Aztecas
	PEDTYPE_GANG9,		// Russian Mafia
	PEDTYPE_GANG10,		// Bikers

	PEDTYPE_DEALER,
	PEDTYPE_MEDIC,
	PEDTYPE_FIREMAN,
	PEDTYPE_CRIMINAL,
	PEDTYPE_BUM,
	PEDTYPE_PROSTITUTE,
	PEDTYPE_SPECIAL,
	PEDTYPE_MISSION1,
	PEDTYPE_MISSION2,
	PEDTYPE_MISSION3,
	PEDTYPE_MISSION4,
	PEDTYPE_MISSION5,
	PEDTYPE_MISSION6,
	PEDTYPE_MISSION7,
	PEDTYPE_MISSION8,

	NUM_PEDTYPES		// 32
};

enum eMoveState
{
	PEDMOVE_NONE = 0,
	PEDMOVE_STILL,
	PEDMOVE_TURN_L,
	PEDMOVE_TURN_R,
	PEDMOVE_WALK,
	PEDMOVE_JOG,
	PEDMOVE_RUN,
	PEDMOVE_SPRINT
};

class CPedModelInfo;

class CPed : public CPhysical
{
public:
	// The model a ped was created from may not be in the loaded data set, so
	// every field read goes through a lookup that can answer nil.
	CPedModelInfo *PedModelInfo(void) const;
	// Answer the quaternion of a node's animated frame, or nil when the model
	// has no such node.
	RtQuat *FrameQuat(int32 node) const;
#ifdef USE_CUTSCENE_SHADOW_FOR_PED
	class CCutsceneShadow *m_pRTShadow;
#endif
	// 0x128
	CStoredCollPoly m_collPoly;
	float m_fCollisionSpeed;

	// cf. https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CPed.h from R*
	uint32 bIsStanding : 1;
	uint32 bWasStanding : 1;
	uint32 bIsAttacking : 1;		// doesn't reset after fist fight
	uint32 bIsPointingGunAt : 1;
	uint32 bIsLooking : 1;
	uint32 bKeepTryingToLook : 1; // if we can't look somewhere due to unreachable angles
	uint32 bIsRestoringLook : 1;
	uint32 bIsAimingGun : 1;

	uint32 bIsRestoringGun : 1;
	uint32 bCanPointGunAtTarget : 1;
	uint32 bIsTalking : 1;
	uint32 bIsInTheAir : 1;
	uint32 bIsLanding : 1;
	uint32 bIsRunning : 1; // on some conditions
	uint32 bHitSomethingLastFrame : 1;
	uint32 bVehEnterDoorIsBlocked : 1; // because someone else enters/exits from there

	uint32 bCanPedEnterSeekedCar : 1;
	uint32 bRespondsToThreats : 1;
	uint32 bRenderPedInCar : 1;
	uint32 bChangedSeat : 1;
	uint32 bUpdateAnimHeading : 1;
	uint32 bBodyPartJustCameOff : 1;
	uint32 bIsShooting : 1;

	uint32 bHasACamera : 1; // does ped possess a camera to document accidents involves fire/explosion
	uint32 bPedIsBleeding : 1;
	uint32 bStopAndShoot : 1; // Ped cannot reach target to attack with fist, need to use gun
	uint32 bIsPedDieAnimPlaying : 1;
	uint32 bObjectiveCompleted : 1;

	uint32 bKindaStayInSamePlace : 1;
	uint32 bBeingChasedByPolice : 1;
	uint32 bNotAllowedToDuck : 1;
	uint32 bCrouchWhenShooting : 1;
	uint32 bIsDucking : 1;
	uint32 bGetUpAnimStarted : 1;
	uint32 bDoBloodyFootprints : 1;

	uint32 bIsLeader : 1;
	uint32 bDontDragMeOutCar : 1; // unfinished feature
	uint32 m_ped_flagF8 : 1;
	uint32 bWillBeQuickJacked : 1;
	uint32 bCancelEnteringCar : 1; // after door is opened or couldn't be opened due to it's locked
	uint32 bObstacleShowedUpDuringKillObjective : 1;
	uint32 bDuckAndCover : 1;

	uint32 bStillOnValidPoly : 1; // set if the polygon the ped is on is still valid for collision
	uint32 bAllowMedicsToReviveMe : 1;
	uint32 bResetWalkAnims : 1;
	uint32 bOnBoat : 1; // not just driver, may be just standing
	uint32 bFadeOut : 1;

	uint32 bKnockedUpIntoAir : 1; // has ped been knocked up into the air by a car collision
	uint32 bHitSteepSlope : 1; // has ped collided/is standing on a steep slope (surface type)
	uint32 bCullExtraFarAway : 1; // special ped only gets culled if it's extra far away (for roadblocks)
	uint32 bClearObjective : 1;
	uint32 bTryingToReachDryLand : 1; // has ped just exited boat and trying to get to dry land
	uint32 bCollidedWithMyVehicle : 1;
	uint32 bRichFromMugging : 1; // ped has lots of cash cause they've been mugging people
	uint32 bChrisCriminal : 1; // Is a criminal as killed during Chris' police mission (should be counted as such)

	uint32 bShakeFist : 1;  // test shake hand at look entity
	uint32 bNoCriticalHits : 1; // if set, limbs won't came off
	uint32 bVehExitWillBeInstant : 1;
	uint32 bFallenDown : 1;
	uint32 bDontAcceptIKLookAts : 1;

	uint32 bMoneyHasBeenGivenByScript : 1; //
	uint32 bHasBeenPhotographed : 1;  //

	uint32 bIsDrowning : 1;
	uint32 bDrownsInWater : 1;
	uint32 bWaitForLeaderToComeCloser : 1;
	uint32 bIsPlayerFriend : 1;
	uint32 bHeadStuckInCollision : 1;
	uint32 bDeadPedInFrontOfCar : 1;
	uint32 bStayInCarOnJack : 1;

	uint32 bDontFight : 1;
	uint32 bDoomAim : 1;
	uint32 bCanBeShotInVehicle : 1;
	uint32 bCanGiveUpSunbathing : 1;
	uint32 bPushedAlongByCar : 1;
	uint32 bRemoveMeWhenIGotIntoCar : 1;
	uint32 bIgnoreThreatsBehindObjects : 1;

	uint32 bNeverEverTargetThisPed : 1;
	uint32 bCrouchWhenScared : 1;
	uint32 bKnockedOffBike : 1;
	uint32 b158_8 : 1;
	uint32 bCollectBusFare : 1;
	uint32 bDonePositionOutOfCollision : 1;
	uint32 bCanAttackPlayerWithCops : 1;

#ifdef KANGAROO_CHEAT
	// our own flags
	uint32 m_ped_flagI80 : 1; // KANGAROO_CHEAT define makes use of this as cheat toggle 
#endif

	uint8 m_gangFlags;
	uint8 m_unused15D; // these 3 can't be padding but had to actually have been members ...
	uint8 m_unused15E;
	uint8 m_unused15F;
	uint8 CharCreatedBy;
	eObjective m_objective;
	eObjective m_prevObjective;
	CVehicle *m_carInObjective;
	uint32 m_eventType;
	AnimBlendFrameData *m_pFrames[TOTAL_PED_NODES];
	RpAtomic *m_pWeaponModel;
	AssocGroupId m_animGroup;
	CAnimBlendAssociation *m_pVehicleAnim;
	CVector2D m_vecAnimMoveDelta;
	CVector m_vecOffsetSeek;
	CPedIK m_pedIK; 
	float m_actionX;
	float m_actionY;
	uint32 m_nPedStateTimer;
	PedState m_nPedState;
	PedState m_nLastPedState;
	eMoveState m_nMoveState;
	int32 m_nStoredMoveState;
	int32 m_nPrevMoveState;
	uint32 m_animTimer;
	float m_fHealth;
	float m_fArmour;
	// When this ped's water-splash particle bursts are due, as absolute
	// CTimer::GetTimeInMilliseconds values; 0 means "not scheduled". File statics
	// in ProcessBuoyancy, so one ped in the water reset another's timers.
	uint32 m_nWaterCirclesTime;
	uint32 m_nRaindropsTime;
	// SA CPed::m_nSwimmingMoveState: the swim clip the ped is currently in. It
	// is a move state rather than a ped state, because swimming does not stop
	// the ped from fighting or dying; SA keeps the two apart and so does this.
	eMoveState m_nSwimmingMoveState;
	// The active climb, if any. reVC has no task manager, so the climb is an
	// object the ped owns and steps from ProcessControl; SA keeps the same
	// state inside CTaskSimpleClimb and reaches it through the task.
	CTaskSimpleClimb *m_pClimbTask;
	// The active swim, if any. Created on the frame the ped's origin drops far
	// enough under water and destroyed when it gets out; see CPed::UpdateSwim.
	CTaskSimpleSwim *m_pSwimTask;
	// SA CPed::m_fSwimStopTime, in frames: how long a ped standing in the
	// shallows keeps the swim task before it gives up and walks out.
	float m_fSwimStopTime;
	uint32 m_nExtendedRangeTimer;
	uint16 m_routeStartPoint;
	int16 m_routePointsPassed;
	int16 m_routePointsBeingPassed;
	CVector2D m_moved;
	float m_fRotationCur;
	float m_fRotationDest;
	float m_headingRate;
	uint16 m_vehDoor;
	int16 m_walkAroundType;
	CPhysical *m_pCurrentPhysSurface;
	CVector m_vecOffsetFromPhysSurface;
	CEntity *m_pCurSurface;
	CVector m_vecSeekPos;
	CEntity *m_pSeekTarget;
	CVehicle *m_pMyVehicle;
	bool bInVehicle;
	// SA keeps the scanners on CPedIntelligence (m_vehicleScanner, m_pedScanner,
	// m_objectScanner). reVC has no intelligence object, so the one scanner the
	// ped code actually uses sits here directly. It is stateful - it holds the
	// tick counter and the entity references - so it cannot be a local.
	CVehicleScanner m_vehicleScanner;
	float m_distanceToCountSeekDone;
	float m_acceptableHeadingOffset;
	uint32 m_nPedType;
	CEntity *m_pWalkAroundObstacle;
	uint32 m_walkAroundObstacleTimer;
	CEntity *m_pCollidingEntity;
	uint8 m_stateUnused;
	uint32 m_timerUnused;
	CWeapon m_weapons[TOTAL_WEAPON_SLOTS];
	eWeaponType m_storedWeapon;
	eWeaponType m_delayedWeapon;
	uint32 m_delayedWeaponAmmo;
	uint8 m_currentWeapon;			// eWeaponType
	uint8 m_maxWeaponTypeAllowed;	// eWeaponType
	uint8 m_wepSkills;
	uint8 m_wepAccuracy;
	CEntity *m_pPointGunAt;
	CVector m_vecHitLastPos;
	uint32 m_curFightMove;
	uint32 m_lastFightMove;
	uint8 m_fightButtonPressure;
	int8 m_fightState;
	bool m_takeAStepAfterAttack;
	uint8 m_bleedCounter;
	CFire *m_pFire;
	CEntity *m_pLookTarget;
	float m_fLookDirection;
	int32 m_wepModelID;
	uint32 m_leaveCarTimer;
	uint32 m_getUpTimer;
	uint32 m_lookTimer;
	uint32 m_chatTimer;
	uint32 m_attackTimer;
	uint32 m_shootTimer; // shooting is a part of attack
	uint32 m_carJackTimer;
	uint32 m_duckTimer;
	uint32 m_duckAndCoverTimer;
	uint32 m_bloodyFootprintCountOrDeathTime;	// Death time when bDoBloodyFootprints is false. Weird decision
	uint32 m_shotTime;
	uint32 m_ceaseAttackTimer;
	bool m_deadBleeding;
	int8 m_bodyPartBleeding;		// PedNode, but -1 if there isn't
	uint16 m_nPedMoney;
 	int8 m_lastWepDam;
	CEntity *m_lastDamEntity;
	CEntity *m_attachedTo;
	CVector m_vecAttachOffset;
	uint16 m_attachType;
	float m_attachRotStep;
	uint32 m_attachWepAmmo;
	uint32 m_threatFlags;
	uint32 m_threatCheckTimer;
	uint32 m_threatCheckInterval;
	int32 m_delayedSoundID;
	uint32 m_delayedSoundTimer;
	uint32 m_lastSoundStart;
	uint32 m_soundStart;
	uint16 m_lastQueuedSound;
	uint16 m_queuedSound;
	bool m_canTalk;
	uint32 m_lastComment;

	static void *operator new(size_t) throw();
	static void *operator new(size_t, int) throw();
	static void operator delete(void*, size_t) throw();
	static void operator delete(void*, int) throw();

	CPed(uint32 pedType);
	~CPed(void);

	void DeleteRwObject();
	void SetModelIndex(uint32 mi);
	void ProcessControl(void);
	void Teleport(CVector);
	void PreRender(void);
	// SA runs the IK slope lean here, every frame, immediately before the bone
	// matrices are built. Called from PreRender, and by a vehicle for its
	// seated occupants.
	void PreRenderAfterTest(void);
	// Set once the skin of this frame has been posed. GetBonePosition and
	// GetTransformedBonePosition fall back to the standard bone table while it
	// is still clear, because a hierarchy that has not been updated answers
	// with last frame's matrices.
	bool bCalledPreRender;
	// SA: keeps the player's chest bones half-rotated with the arms.
	void ShoulderBoneRotation(RpClump *clump);
	void Render(void);
	bool SetupLighting(void);
	void RemoveLighting(bool);
	void FlagToDestroyWhenNextProcessed(void);
	int32 ProcessEntityCollision(CEntity*, CColPoint*);

	virtual void SetMoveAnim(void);
	void SetMoveAnimSpeed(CAnimBlendAssociation *assoc);

	void AddWeaponModel(int id);
	void AimGun(void);
	float GetAimAnimBlendAmount(void);
	void KillPedWithCar(CVehicle *veh, float impulse);
	void Say(uint16 audio);
	void Say(uint16 audio, int32 time);
	void SetLookFlag(CEntity* target, bool keepTryingToLook, bool cancelPrevious = false);
	void SetLookFlag(float direction, bool keepTryingToLook, bool cancelPrevious = false);
	void SetLookTimer(int time);
	void SetDie(AnimationId anim = ANIM_ID_KO_SHOT_FRONT_0, float arg1 = 4.0f, float arg2 = 0.0f);
	void SetDead(void);
	void ApplyHeadShot(eWeaponType weaponType, CVector pos, bool evenOnPlayer);
	void RemoveBodyPart(ePedNode nodeId, int8 direction);
	bool OurPedCanSeeThisOne(CEntity *target, bool shootablesDoBlock = false);
	void Attack(void);
	void ClearAimFlag(void);
	void ClearLookFlag(void);
	void RestorePreviousState(void);
	void ClearAttack(void);
	bool IsPedHeadAbovePos(float zOffset);
	void RemoveWeaponModel(int modelId);
	void SetCurrentWeapon(eWeaponType weaponType);
	void SetCurrentWeapon(int weapon);
	void Duck(void);
	void ClearDuck(bool = false);
	void ClearPointGunAt(void);
	void BeingDraggedFromCar(void);
	void RestartNonPartialAnims(void);
	void LineUpPedWithCar(PedLineUpPhase phase);
	void SetPedPositionInCar(void);
	void PlayFootSteps(void);
	void QuitEnteringCar(void);
	int16 FindNearPeds(const CVector &centre, float radius, CPed **out, int16 maxPeds);
	int32 GiveWeapon(eWeaponType weaponType, uint32 ammo, bool unused = true);
	void CalculateNewOrientation(void);
	float WorkOutHeadingForMovingFirstPerson(float);
	void CalculateNewVelocity(void);
	bool CanSeeEntity(CEntity*, float threshold = CAN_SEE_ENTITY_ANGLE_THRESHOLD);
	void RestorePreviousObjective(void);
	void SetIdle(void);
#ifdef _MSC_VER
#if _MSC_VER >= 1920 && _MSC_VER < 1929
	__declspec(noinline) // workaround for a compiler bug, hooray MS :P
#endif
#endif
	void SetObjective(eObjective, void*);
	void SetObjective(eObjective);
	void SetDuck(uint32, bool = false);
	bool TurnBody(void);
	void SetSeek(CVector, float);
	void SetSeek(CEntity*, float);
	uint8 CheckForPointBlankPeds(CPed*);
	bool CheckIfInTheAir(void);
	void ClearAll(void);
	void SetPointGunAt(CEntity*);
	bool Seek(void);
	void ClearAttackByRemovingAnim(void);
	void SetStoredState(void);
	void StopNonPartialAnims(void);
	bool InflictDamage(CEntity*, eWeaponType, float, ePedPieceTypes, uint8);
	// SA CPedDamageResponseCalculator::ComputeWillForceDeath (0x4AD610): the hit
	// is lethal by rule rather than by arithmetic - a headshot from a gun, a
	// katana combo, an explosion on a ped that is nearly dead.
	bool WillForceDeathFrom(eWeaponType, ePedPieceTypes, CEntity *damager, float damage);
	// The health multiplier a hit carries for the body part it landed on.
	static float GetBodyPartDamageMult(ePedPieceTypes);
	void ClearFall(void);
	void SetGetUp(void);
	void ClearLook(void);
	void ClearObjective(void);
	void ClearSeek(void);
	void ClearWeapons(void);
	void RestoreGunPosition(void);
	void RestoreHeadingRate(void);
	void SetAimFlag(CEntity* to);
	void SetAimFlag(float angle);
	void SetAmmo(eWeaponType weaponType, uint32 ammo);
	void SetEvasiveStep(CPhysical*, uint8);
	void GrantAmmo(eWeaponType, uint32);
	void SetEvasiveDive(CPhysical*, uint8);
	void SetAttack(CEntity*);
	void StartFightAttack(uint8);
	bool FightStrike(CVector&, bool);
	void FightHitPed(CPed*, CVector&, CVector&, int16);
	int32 ChooseAttackPlayer(uint8, bool);
	int GetLocalDirection(const CVector2D &);
	void StartFightDefend(uint8, uint8, uint8);
	void PlayHitSound(CPed*);
	void SetFall(int, AnimationId, uint8);
	void RemoveDrivebyAnims(void);
	void RemoveInCarAnims(void);
	void SetDirectionToWalkAroundObject(CEntity*);
	void RemoveWeaponAnims(int, float);
	void CreateDeadPedMoney(void);
	void CreateDeadPedWeaponPickups(void);
	void CreateDeadPedPickupCoors(float *x, float *y, float *z);
	void SetAttackTimer(uint32);
	void SetBeingDraggedFromCar(CVehicle*, uint32, bool);
	void SetRadioStation(void);
	void DeadPedMakesTyresBloody(void);
	void MakeTyresMuddySectorList(CPtrList&);
	void EndFight(uint8);
	void EnterCar(void);
	void ExitCar(void);
	void Fight(void);
	void ProcessObjective(void);
	void GetNearestDoor(CVehicle*, CVector&);
	bool GetNearestPassengerDoor(CVehicle*, CVector&);
	int GetWeaponSlot(eWeaponType);
	bool CanWeRunAndFireWithWeapon(void);
	void GoToNearestDoor(CVehicle*);
	void Idle(void);
	void InTheAir(void);
	void SetLanding(void);
	bool IsPedDoingDriveByShooting(void);
	bool IsRoomToBeCarJacked(void);
	void MoveHeadToLook(void);
	void ProcessBuoyancy(void);
	// SA CPed::ProcessBuoyancy's in-water half: enter the swim when the origin
	// goes under, step the swim task while it lasts, and leave it when the ped
	// can stand again. Split out so that ProcessControl can call it right after
	// the buoyancy pass, which is where SA does it.
	void UpdateSwim(void);
	// SA CTaskSimpleClimb's entry point as CPed uses it: try to grab a ledge in
	// front of the ped and, on success, take over the ped for the climb.
	bool TryToClimb(void);
	void ServiceTalking(void);
	void SetJump(void);
	void SetJump(bool highJump, bool climbJump = false);
	void ApplyLaunchForce(void);
	void ApplyLaunchForce(bool highJump, bool climbJump = false);
	void SeekCar(void);
	bool PositionPedOutOfCollision(void);
	bool PositionAnyPedOutOfCollision(void);
	bool PlacePedOnDryLand(void);
	bool PossiblyFindBetterPosToSeekCar(CVector*, CVehicle*);
	void SetEnterCar(CVehicle*, uint32);
	void SetExitCar(CVehicle*, uint32);
	void SetEnterCar_AllClear(CVehicle*, uint32, uint32);
	void WarpPedIntoCar(CVehicle*);
	void SetCarJack(CVehicle*);
	void SetExitBoat(CVehicle*);
	void GiveDelayedWeapon(eWeaponType weapon, uint32 ammo);
	void RequestDelayedWeapon();
	void AddInCarAnims(CVehicle* car, bool isDriver);
	void AttachPedToEntity(CEntity*, CVector, uint16, float, eWeaponType);
	void DettachPedFromEntity();
	void PedShuffle();
	void DriveVehicle();
	void PositionAttachedPed();
	bool CanUseTorsoWhenLooking();

	// Static methods
	// With a ped, approach offsets come from that ped's VC or SA animation
	// bank; the no-ped overload remains native VC for vehicle-only callers.
	static CVector GetLocalPositionToOpenCarDoor(CVehicle *veh, uint32 component, float offset, CPed *ped = nil);
	static CVector GetPositionToOpenCarDoor(CVehicle *veh, uint32 component, float seatPosMult, CPed *ped = nil);
	static CVector GetPositionToOpenCarDoor(CVehicle* veh, uint32 component, CPed *ped = nil);
	static void Initialise(void);
	static void SetAnimOffsetForEnterOrExitVehicle(void);
	static void LoadFightData(void);

	// Callbacks
	static void PedGetupCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedStaggerCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedEvadeCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishDieAnimCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishLaunchCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishHitHeadCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimGetInCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimDoorOpenCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimPullPedOutCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimDoorCloseCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetInCarCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetOutCarCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimAlignCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetDraggedOutCarCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimStepOutCarCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishedAttackCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishedReloadCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishFightMoveCB(CAnimBlendAssociation *assoc, void *arg);
	static void FinishJumpCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedLandCB(CAnimBlendAssociation *assoc, void *arg);
	static void RestoreHeadingRateCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetQuickDraggedOutCarPositionCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetDraggedOutCarPositionCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedSetPreviousStateCB(CAnimBlendAssociation *assoc, void *arg);
	static void PedAnimShuffleCB(CAnimBlendAssociation *assoc, void *arg);

	bool IsPlayer(void) const;
	bool IsFemale(void) { return m_nPedType == PEDTYPE_CIVFEMALE || m_nPedType == PEDTYPE_PROSTITUTE; }
	bool UseGroundColModel(void);
	bool CanSetPedState(void);
	bool IsPedInControl(void);
	bool CanBeDeleted(void);
	bool CanBeDeletedEvenInVehicle(void);
	bool CanStrafeOrMouseControl(void);
	bool CanPedReturnToState(void);
	void SetMoveState(eMoveState);
	bool IsTemporaryObjective(eObjective objective);
	bool IsPointerValid(void);
	void Die(void);
	void Fall(void);
	bool IsPedShootable(void);
	void Look(void);
	void SetInTheAir(void);
	void RestoreHeadPosition(void);
	void PointGunAt(void);
	bool ServiceTalkingWhenDead(void);
	void SetShootTimer(uint32);
	void SetSeekCar(CVehicle*, uint32);
	void SetSeekBoatPosition(CVehicle*);
	void SeekBoatPosition(void);
	void UpdatePosition(void);
	CObject *SpawnFlyingComponent(int, int8);
	void SetCarJack_AllClear(CVehicle*, uint32, uint32);
	bool CanPedJumpThis(CEntity *unused, CVector *damageNormal = nil);
	void Undress(const char*);
	void Dress(void);
	void SetLook(CEntity* to);
	void SetLook(float direction);

	bool HasWeaponSlot(uint8 slot) { return m_weapons[slot].m_eWeaponType != WEAPONTYPE_UNARMED; }
	CWeapon& GetWeapon(uint8 slot) { return m_weapons[slot]; }
	CWeapon *GetWeapon(void) { return &m_weapons[m_currentWeapon]; }

	PedState GetPedState(void) { return m_nPedState; }
	void SetPedState(PedState state)
	{
		m_nPedState = state;
	}
	// A stealth kill is a third way to be dead, so every "is this ped dead"
	// question has to admit PED_DIE_BY_STEALTH. SA keeps the same set in
	// CPed::IsStateDeadForScript and in the script opcode that asks it.
	bool Dead(void) { return m_nPedState == PED_DEAD || m_nPedState == PED_DIE_BY_STEALTH; }
	bool Dying(void) { return m_nPedState == PED_DIE || m_nPedState == PED_DIE_BY_STEALTH; }
	bool DyingOrDead(void) { return m_nPedState == PED_DIE || m_nPedState == PED_DEAD || m_nPedState == PED_DIE_BY_STEALTH; }
	bool OnGround(void) { return m_nPedState == PED_FALL || m_nPedState == PED_DIE || m_nPedState == PED_DEAD || m_nPedState == PED_DIE_BY_STEALTH; }
	bool OnGroundOrGettingUp(void) { return OnGround() || m_nPedState == PED_GETUP; }
	
	bool Driving(void) { return m_nPedState == PED_DRIVING; }
	bool InVehicle(void) { return bInVehicle && m_pMyVehicle; } // True when ped is sitting/standing in vehicle, not in enter/exit state.
	bool EnteringCar(void) { return m_nPedState == PED_ENTER_CAR || m_nPedState == PED_CARJACK; }

	void ReplaceWeaponWhenExitingVehicle(void);
	void RemoveWeaponWhenEnteringVehicle(void);
	bool IsNotInWreckedVehicle()
	{
		return m_pMyVehicle != nil && ((CEntity*)m_pMyVehicle)->GetStatus() != STATUS_WRECKED;
	}

	// My names. Inlined in VC
	AnimationId GetFireAnimNotDucking(CWeaponInfo* weapon) {
		if (m_nPedType == PEDTYPE_COP && weapon->IsFlagSet(WEAPONFLAG_COP3_RD))
			return Get3rdFireAnim(weapon);
		else
			return GetPrimaryFireAnim(weapon);
	}

	static AnimationId Get3rdFireAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_COP3_RD))
			return ANIM_ID_FIRE;
		else
			return (AnimationId)0;
	}

	static AnimationId GetFireAnimGround(CWeaponInfo* weapon, bool kickFloorIfNone = true) {
		if (weapon->IsFlagSet(WEAPONFLAG_GROUND_2ND))
			return ANIM_ID_CROUCHFIRE;
		else if (weapon->IsFlagSet(WEAPONFLAG_GROUND_3RD))
			return ANIM_ID_FIRE;
		else if (kickFloorIfNone)
			return ANIM_ID_FIGHT_1;
		else
			return (AnimationId)0;
	}

	static AnimationId GetPrimaryFireAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_ANIMDETONATE))
			return ANIM_ID_BOMBER;
		else
			return ANIM_ID_FIRE;
	}

	static AnimationId GetCrouchReloadAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_RELOAD))
			return ANIM_ID_CROUCHRELOAD;
		else
			return (AnimationId)0;
	}

	static AnimationId GetCrouchFireAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_CROUCHFIRE))
			return ANIM_ID_CROUCHFIRE;
		else
			return (AnimationId)0;
	}

	static AnimationId GetReloadAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_RELOAD))
			return ANIM_ID_RELOAD;
		else
			return (AnimationId)0;
	}

	static AnimationId GetFightIdleWithMeleeAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_FIGHTMODE))
			return ANIM_ID_FIGHT_IDLE;
		else
			return (AnimationId)0;
	}

	static AnimationId GetFinishingAttackAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_FINISH_3RD))
			return ANIM_ID_FIGHT_M;
		else
			return (AnimationId)0;
	}

	static AnimationId GetSecondFireAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_USE_2ND))
			return ANIM_ID_FIRE;
		else
			return (AnimationId)0;
	}
	
	static AnimationId GetMeleeStartAnim(CWeaponInfo* weapon) {
		if (weapon->IsFlagSet(WEAPONFLAG_PARTIALATTACK))
			return ANIM_ID_FIGHT_3;
		else
			return (AnimationId)0;
	}

	static AnimationId GetThrowAnim(CWeaponInfo *weapon)
	{
		if (weapon->IsFlagSet(WEAPONFLAG_THROW))
			return ANIM_ID_GRENADE_WEAPON_START_THROW;
		else
			return (AnimationId)0;
	}
	// --

	inline void SetWeaponLockOnTarget(CEntity *target)
	{
		if (m_pPointGunAt)
			m_pPointGunAt->CleanUpOldReference(&m_pPointGunAt);

		m_pPointGunAt = (CPed*)target;
		if (target)
			((CEntity*)target)->RegisterReference(&m_pPointGunAt);
	}

	// CPed::GetBonePosition, 0x5E4280. Until the hierarchy of this frame has
	// been posed, a bone still has to have a position; the standard table
	// answers where it sits on the reference skeleton. Once the skin is
	// updated the real matrix is read instead.
	CVector GetBonePosition(eBoneTag bone, bool updateSkinBones = true)
	{
		if(!bCalledPreRender){
			if(!updateSkinBones)
				{
				CVector p = GetPedBoneStdPosition(bone);
				GetMatrix().Update();
				RwV3d temp;
				RwV3dTransformPoints(&temp, &p, 1, GetMatrix().m_attachment);
				return CVector(temp);
			}
			UpdateRpHAnim();
			bCalledPreRender = true;
		}
		RwMatrix *m = GetBoneMatrix(bone);
		if(m)
			return m->pos;
		return GetPosition();
	}

	RwMatrix *GetBoneMatrix(eBoneTag bone)
	{
		if(GetClump() == nil || !IsClumpSkinned(GetClump()))
			return nil;
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(GetClump());
		if(hier == nil)
			return nil;
		int32 idx = RpHAnimIDGetIndex(hier, bone);
		if(idx < 0 || idx >= hier->numNodes)
			return nil;
		return &RpHAnimHierarchyGetMatrixArray(hier)[idx];
	}

	// CPed::GetTransformedBonePosition, 0x5E01C0. Same rule as GetBonePosition,
	// but the caller hands in the point to carry into bone space.
	void GetTransformedBonePosition(CVector &inOutPos, eBoneTag bone, bool updateSkinBones = true)
	{
		if(updateSkinBones){
			if(!bCalledPreRender){
				UpdateRpHAnim();
				bCalledPreRender = true;
			}
		}else if(!bCalledPreRender){
			CVector p = GetPedBoneStdPosition(bone);
			GetMatrix().Update();
			RwV3d temp;
			RwV3dTransformPoints(&temp, &p, 1, GetMatrix().m_attachment);
			inOutPos = CVector(temp);
			return;
		}
		RwMatrix *m = GetBoneMatrix(bone);
		if(m == nil)
			return;
		RwV3dTransformPoints(&inOutPos, &inOutPos, 1, m);
	}

	// Using this to abstract nodes of skinned and non-skinned meshes
	CVector GetNodePosition(int32 node)
	{
		RwV3d pos = { 0.0f, 0.0f, 0.0f };
		// A node the skeleton does not carry has no frame and no bone; the
		// caller gets the entity origin rather than a matrix read from a
		// negative index.
		if(node < 0 || node >= TOTAL_PED_NODES || m_pFrames[node] == nil)
			return GetPosition();
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(GetClump());
		if(hier == nil)
			return GetPosition();
		int32 idx = RpHAnimIDGetIndex(hier, m_pFrames[node]->nodeID);
		if(idx < 0)
			return GetPosition();
		RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
		if(mats == nil)
			return GetPosition();
		pos = mats[idx].pos;
		return pos;
	}
	void TransformToNode(CVector &pos, int32 node)
	{
		if(node < 0 || node >= TOTAL_PED_NODES || m_pFrames[node] == nil)
			return;
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(GetClump());
		if(hier == nil)
			return;
		int32 idx = RpHAnimIDGetIndex(hier, m_pFrames[node]->nodeID);
		if(idx < 0)
			return;
		RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
		if(mats == nil)
			return;
		RwV3dTransformPoints(&pos, &pos, 1, &mats[idx]);
	}

	static bool bNastyLimbsCheat;

#ifdef COMPATIBLE_SAVES
	virtual void Save(uint8*& buf);
	virtual void Load(uint8*& buf);
#endif
};

void FinishTalkingOnMobileCB(CAnimBlendAssociation* assoc, void* arg);
void StartTalkingOnMobileCB(CAnimBlendAssociation* assoc, void* arg);

VALIDATE_SIZE(CPed, 0x5F4);

bool IsPedPointerValid(CPed*);
bool IsPedPointerValid_NotInWorld(CPed*);

#endif // __GTA_PED_H__
