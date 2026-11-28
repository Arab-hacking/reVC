#include "TaskSimpleTired.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Ped.h"
#include "RpAnimBlend.h"
#include "Timer.h"

// SA CTaskSimpleTired (0x630F50). The player's fat-clothes motion-group
// variant is absent here (no clothing system), so the default group is
// always used.

CTaskSimpleTired::CTaskSimpleTired(uint32 tiredDurationMs)
: m_TiredDurationMs(tiredDurationMs)
{
	m_AnimStartMs = 0;
	m_TiredDurationMs_Copy = 0;
	m_AnimHasStarted = false;
	m_NeedsToStartAnim = false;
	m_TiredAnim = nil;
}

// 0x630F70
void
CTaskSimpleTired::StartAnim(CPed *ped)
{
	ped->SetMoveState(PEDMOVE_STILL);
	m_AnimStartMs = CTimer::GetTimeInMilliseconds();
	m_TiredDurationMs_Copy = m_TiredDurationMs;
	m_AnimHasStarted = true;
	m_TiredAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE_TIRED, 4.0f);
}

// 0x630FF0
bool
CTaskSimpleTired::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)priority; (void)event;
	if(m_TiredAnim && m_TiredAnim == RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_IDLE_TIRED))
		m_TiredAnim->m_BlendDelta = -4.0f;
	return true;
}

// 0x637970
bool
CTaskSimpleTired::ProcessPed(CPed *ped)
{
	if(!m_TiredAnim)
		StartAnim(ped);

	if(!m_AnimHasStarted)
		return false;

	if(m_NeedsToStartAnim){
		m_AnimStartMs = CTimer::GetTimeInMilliseconds();
		m_NeedsToStartAnim = false;
	}

	if(CTimer::GetTimeInMilliseconds() < m_AnimStartMs + m_TiredDurationMs_Copy)
		return false;

	MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);
	return true;
}
