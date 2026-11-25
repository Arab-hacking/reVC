#pragma once

#include "AnimationEnums.h"
#include "eAnimBlendCallbackType.h"

class CAnimBlendNode;
class CAnimBlendHierarchy;
class CAnimBlendStaticAssociation;

enum eAnimationFlags {
	ANIMATION_DEFAULT = 0,                    //0x0,
	ANIMATION_IS_PLAYING = 1 << 0,            //0x1,
	ANIMATION_IS_LOOPED = 1 << 1,             //0x2,
	ANIMATION_IS_BLEND_AUTO_REMOVE = 1 << 2,  //!< (0x4) Automatically `delete this` once faded out (`m_BlendAmount <= 0 && m_BlendDelta <= 0`)
	ANIMATION_IS_FINISH_AUTO_REMOVE = 1 << 3, //0x8,  // Animation will be stuck on last frame, if not set
	ANIMATION_IS_PARTIAL = 1 << 4,            //0x10,
	ANIMATION_IS_SYNCRONISED = 1 << 5,        //0x20,
	ANIMATION_CAN_EXTRACT_VELOCITY = 1 << 6,  //0x40,
	ANIMATION_CAN_EXTRACT_X_VELOCITY = 1 << 7, //0x80,

	// ** User defined flags **
	ANIMATION_WALK = 1 << 8,                       //0x100,
	ANIMATION_200 = 1 << 9,                        //0x200,
	ANIMATION_DONT_ADD_TO_PARTIAL_BLEND = 1 << 10, //0x400,
	ANIMATION_IS_FRONT = 1 << 11,                  //0x800,
	ANIMATION_SECONDARY_TASK_ANIM = 1 << 12,       //0x1000,
	// **

	ANIMATION_IGNORE_ROOT_TRANSLATION = 1 << 13, //0x2000,
	ANIMATION_REFERENCE_BLOCK = 1 << 14,         //0x4000,
	ANIMATION_FACIAL = 1 << 15                   //0x8000 // The animation is never destroyed if this flag is set, NO MATTER WHAT
};

class CDefaultAnimCallback
{
public:
	static void DefaultAnimCB(class CAnimBlendAssociation *animAssoc, void *something)
	{
		// nothing here
		(void)animAssoc;
		(void)something;
	}
};

class CAnimBlendLink
{
public:
	CAnimBlendLink *next;
	CAnimBlendLink *prev;

	CAnimBlendLink() : next(nil), prev(nil) {}

	void Init()
	{
		next = nil;
		prev = nil;
	}

	void Prepend(CAnimBlendLink *link)
	{
		if (next)
			next->prev = link;
		link->next = next;
		link->prev = this;
		next = link;
	}

	void Remove()
	{
		if (prev)
			prev->next = next;
		if (next)
			next->prev = prev;
		Init();
	}

	bool IsEmpty() const { return !next; }
};

/*!
 * @brief Running animation of a clump (Usually peds), created from an `CAnimBlendHierarchy`.
 *
 * @details The sequence/frames data is copied from `CAnimBlendHierarchy` to `CAnimBlendAssociation` when a clump requests an animation.
 * @details The instance of this class gets destroyed when the ped/clump stops playing the animation.
 * @details But `CAnimBlendHierarchy` is never destroyed and stays in memory unless `CStreaming` forces the IFP to unload (to create space in memory)
 *
 * @details A clump can have one, or more, instances of this class. Usually there's only 1 primary animation,
 * @details but there are also partial animations, which can be played alongside primary animations, like hand gestures or smoking.
 */
class CAnimBlendAssociation
{
public:
	CAnimBlendLink m_Link;          //!< Link to the next association of the clump
	uint16 m_NumBlendNodes;         //!< Number of bones this anim moves
	int16 m_AnimGroupId;            //!< Anim's group
	CAnimBlendNode *m_BlendNodes;   //!< Node per-node animations - NOTE: Order of these depends on order of nodes in Clump this was built from
	CAnimBlendHierarchy *m_BlendHier; //!< The animation hierarchy this association was created from
	float m_BlendAmount;            //!< How much this animation is blended
	float m_BlendDelta;             //!< How much `BlendAmount` changes over time
	float m_CurrentTime;            //!< Current play time
	float m_Speed;                  //!< Play speed
	float m_TimeStep;               //!< Time-per-tick
	int16 m_AnimId;                 //!< Anim's ID
	uint16 m_Flags;

	// Callback shit
	eAnimBlendCallbackType m_nCallbackType;
	void (*m_pCallbackFunc)(CAnimBlendAssociation *, void *);
	void *m_pCallbackData;

public:
	CAnimBlendAssociation();
	CAnimBlendAssociation(RpClump *clump, CAnimBlendHierarchy *animHierarchy);
	CAnimBlendAssociation(CAnimBlendAssociation &assoc);
	explicit CAnimBlendAssociation(CAnimBlendStaticAssociation &assoc);

	virtual ~CAnimBlendAssociation();

	float GetTimeProgress() const;
	void SetBlendAmount(float a) { m_BlendAmount = a; }
	float GetBlendAmount(float weight = 1.0f) const { return IsPartial() ? m_BlendAmount : m_BlendAmount * weight; }
	float GetBlendDelta() const { return m_BlendDelta; }

	AnimationId GetAnimId() const { return (AnimationId)m_AnimId; }

	bool IsPlaying() const { return (m_Flags & ANIMATION_IS_PLAYING) != 0; }
	bool IsLooped() const { return (m_Flags & ANIMATION_IS_LOOPED) != 0; }
	bool IsPartial() const { return (m_Flags & ANIMATION_IS_PARTIAL) != 0; }
	bool IsSyncronised() const { return (m_Flags & ANIMATION_IS_SYNCRONISED) != 0; }
	bool CanExtractXVelocity() const { return (m_Flags & ANIMATION_CAN_EXTRACT_X_VELOCITY) != 0; }
	bool CanExtractVelocity() const { return (m_Flags & ANIMATION_CAN_EXTRACT_VELOCITY) != 0; }
	bool IsFacial() const { return (m_Flags & ANIMATION_FACIAL) != 0; }

	void AllocateAnimBlendNodeArray(int32 count);
	void FreeAnimBlendNodeArray();

	void Init(RpClump *clump, CAnimBlendHierarchy *hierarchy);
	void Init(CAnimBlendAssociation &source);
	void Init(CAnimBlendStaticAssociation &source);

	void ReferenceAnimBlock();
	void SetBlendDelta(float value) { m_BlendDelta = value; }
	void SetBlend(float blendAmount, float blendDelta);
	void SetBlendTo(float blendAmount, float blendDelta);
	void SetCurrentTime(float currentTime);
	float GetCurrentTime() const { return m_CurrentTime; }

	void SetDeleteCallback(void (*callback)(CAnimBlendAssociation *, void *), void *data = nil);
	void SetDefaultDeleteCallback() { SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nil); }

	void SetFinishCallback(void (*callback)(CAnimBlendAssociation *, void *), void *data = nil);
	void SetDefaultFinishCallback() { SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil); }

	void Start(float currentTime = 0.0f);

	/*!
	 * @addr 0x4CEB40
	 * @brief Sync the play time of this animation with another
	 */
	void SyncAnimation(CAnimBlendAssociation *syncWith);
	bool UpdateBlend(float timeStep);
	bool UpdateTime(float timeStep, float timeMult);
	void UpdateTimeStep(float speedMult, float timeMult);
	bool HasFinished() const;
	uint32 GetHashKey() const;

	void SetFlag(eAnimationFlags flag, bool value = true)
	{
		if (value)
			m_Flags |= (int)flag;
		else
			m_Flags &= ~(int)flag;
	}

	bool HasFlag(eAnimationFlags flag) const { return (m_Flags & flag) != 0; }

	static CAnimBlendAssociation *FromLink(CAnimBlendLink *link)
	{
		return (CAnimBlendAssociation *)((uint8 *)link - offsetof(CAnimBlendAssociation, m_Link));
	}

	float GetSpeed() const { return m_Speed; }
	void SetSpeed(float speed) { m_Speed = speed; }

	CAnimBlendNode *GetNode(int32 nodeIndex);
	CAnimBlendNode *GetNodesPtr() { return m_BlendNodes; }

	CAnimBlendLink &GetLink() { return m_Link; }
	CAnimBlendHierarchy *GetHier() const { return m_BlendHier; }
};

VALIDATE_SIZE(CAnimBlendAssociation, 0x3C);
