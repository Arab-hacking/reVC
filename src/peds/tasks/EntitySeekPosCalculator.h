#pragma once

// SA CEntitySeekPosCalculator: where exactly to stand relative to the
// entity being sought. SA's CTaskComplexSeekEntity is templated on this;
// here only the standard calculator is ported (the XY-offset and
// radius/angle variants plug into the same interface later).

#include "Vector.h"

class CEntity;
class CPed;

class CEntitySeekPosCalculator
{
public:
	virtual ~CEntitySeekPosCalculator() {}
	virtual void ComputeEntitySeekPos(const CPed &seeker, const CEntity &target, CVector &outPos) = 0;
};

VALIDATE_SIZE(CEntitySeekPosCalculator, 0x4);

class CEntitySeekPosCalculatorStandard : public CEntitySeekPosCalculator
{
public:
	void ComputeEntitySeekPos(const CPed &seeker, const CEntity &target, CVector &outPos);
};

VALIDATE_SIZE(CEntitySeekPosCalculatorStandard, 0x4);
