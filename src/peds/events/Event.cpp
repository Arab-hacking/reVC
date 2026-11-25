#include "common.h"

#include "Event.h"
#include "EventPool.h"
#include "EventSoundQuiet.h"
#include "Entity.h"

#include <math.h>

// 0x4B5620
void*
CEvent::operator new(size_t size)
{
	assert(size <= EVENT_POOL_SLOT_SIZE);
	return GetEventPool()->New();
}

// 0x4B5630
void
CEvent::operator delete(void *object)
{
	GetEventPool()->Delete((CEvent*)object);
}

// 0x4ABFC0
CEvent::CEvent()
{
	m_nTimeActive = 0;
	m_bValid = false;
}

CEvent::~CEvent()
{
}

// 0x4B2850
float
CEvent::GetSoundLevel(const CEntity *entity, CVector &position)
{
	if((entity && entity != GetSourceEntity()) || GetLocalSoundLevel() == 0.0f)
		return 0.0f;

	float level = 1000.0f;
	if(GetEventType() == EVENT_SOUND_QUIET){
		CVector &sourcePos = ((CEventSoundQuiet*)this)->m_position;
		level = Min((position - sourcePos).Magnitude(), 1.0f);
	}else if(GetSourceEntity()){
		const CVector &sourcePos = GetSourceEntity()->GetPosition();
		level = Min((position - sourcePos).Magnitude(), 1.0f);
	}
	return (GetLocalSoundLevel() - (log10f(level * level) * 10.0f)) - (level / 100.0f);
}

// 0x4AC050
float
CEvent::CalcSoundLevelIncrement(float level1, float level2)
{
	if(level2 == 0.0f)
		return 0.0f;

	if(level1 == 0.0f)
		return level2;

	level1 = powf(10.0f, level1 * 0.1f);
	level2 = powf(10.0f, level2 * 0.1f);
	return log10f((level1 + level2) / level1) * 10.0f;
}
