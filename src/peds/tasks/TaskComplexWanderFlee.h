#pragma once

// SA CTaskComplexWanderFlee: wander with no scanning at all - the ped is
// running for its life along the path network (driven by the smart flee
// tasks, which keep steering its direction and speed).

#include "TaskComplexWander.h"

class CTaskComplexWanderFlee : public CTaskComplexWander
{
public:
	CTaskComplexWanderFlee(eMoveState moveState, uint8 dir);
	CTaskComplexWanderFlee(const CTaskComplexWanderFlee &other);

	CTask *Clone() const { return new CTaskComplexWanderFlee(*this); }
	eWanderType GetWanderType() { return WANDER_TYPE_FLEE; }
	void ScanForStuff(CPed *ped) { (void)ped; }
};

VALIDATE_SIZE(CTaskComplexWanderFlee, 0x28);
