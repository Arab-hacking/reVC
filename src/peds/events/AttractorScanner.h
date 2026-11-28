#pragma once

#include "common.h"
#include "TaskTimer.h"

class C2dEffect;

// SA CAttractorScanner: tracks the ped-attractor 2d effect (chairs,
// benches, phones...) the ped is currently using, if any.
class CAttractorScanner
{
public:
	char field_0;
	char _pad[3];
	CTaskTimer field_4;
	C2dEffect *m_pEffectInUse;
	int32 field_14;
	int32 field_18[10];
	int32 field_40[10];
	int32 field_68[10];

	void Clear();
};

VALIDATE_SIZE(CAttractorScanner, 0x90);
