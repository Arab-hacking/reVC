#include "common.h"

#include "EntitySeekPosCalculator.h"

#include "Entity.h"

// SA CEntitySeekPosCalculatorStandard::ComputeEntitySeekPos: seek the
// entity's own position.
void
CEntitySeekPosCalculatorStandard::ComputeEntitySeekPos(const CPed &seeker, const CEntity &target, CVector &outPos)
{
	(void)seeker;
	outPos = target.GetPosition();
}
