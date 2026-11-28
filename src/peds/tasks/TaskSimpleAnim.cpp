#include "common.h"

#include "TaskSimpleAnim.h"
#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "EventScriptCommand.h"
#include "Ped.h"
#include "RpAnimBlend.h"

// SA CTaskSimpleAnim::CTaskSimpleAnim (0x61A9E0).
CTaskSimpleAnim::CTaskSimpleAnim(bool bHoldLastFrame)
	: CTaskSimple()
{
	m_bIsFinished = false;
	m_bDontInterrupt = false;
	m_bHoldLastFrame = bHoldLastFrame;
	m_pAnim = nil;
}

// SA CTaskSimpleAnim::~CTaskSimpleAnim (0x61AA20): hand the animation back
// to the default callback and let it blend out, unless it must hold its
// last frame.
CTaskSimpleAnim::~CTaskSimpleAnim()
{
	if(m_pAnim == nil)
		return;

	m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
	m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	if(!m_bHoldLastFrame && m_pAnim->m_BlendAmount > 0.0f && m_pAnim->m_BlendDelta >= 0.0f)
		m_pAnim->m_BlendDelta = -4.0f;
	m_pAnim = nil;
}

// SA CTaskSimpleAnim::MakeAbortable (0x61AAA0).
bool
CTaskSimpleAnim::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	bool bSkipBlend = false;
	float fBlend = -4.0f;
	if(priority == ABORT_PRIORITY_IMMEDIATE){
		fBlend = -1000.0f;
		m_bHoldLastFrame = false;
	}else if(m_bDontInterrupt){
		if(event && event->GetEventType() != EVENT_SCRIPT_COMMAND)
			return false;
	}else if(event){
		if(event->GetEventType() == EVENT_SCRIPT_COMMAND){
			const CEventScriptCommand *scriptCommand = (const CEventScriptCommand*)event;
			if(scriptCommand->m_task){
				if(scriptCommand->m_task->GetTaskType() == TASK_SIMPLE_NAMED_ANIM){
					if(m_pAnim)
						m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
					bSkipBlend = true;
				}
			}
		}
	}

	if(!bSkipBlend){
		if(m_pAnim){
			m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			if(!m_bHoldLastFrame){
				if(m_pAnim->m_Flags & ANIMATION_IS_PARTIAL)
					m_pAnim->m_BlendDelta = fBlend;
				else
					CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, -fBlend);
			}
		}
	}

	if(priority == ABORT_PRIORITY_IMMEDIATE || priority == ABORT_PRIORITY_URGENT || m_bHoldLastFrame){
		if(m_pAnim){
			m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
			m_bIsFinished = true;
			m_pAnim = nil;
		}
		return true;
	}

	return false;
}

// SA CTaskSimpleAnim::FinishRunAnimCB (0x61AC20): the animation ended, so
// the task is done.
void
CTaskSimpleAnim::FinishRunAnimCB(CAnimBlendAssociation *blendAssoc, void *data)
{
	(void)blendAssoc;
	((CTaskSimpleAnim*)data)->m_bIsFinished = true;
	((CTaskSimpleAnim*)data)->m_pAnim = nil;
}
