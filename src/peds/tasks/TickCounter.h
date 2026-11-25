#pragma once

#include "common.h"

// SA CTickCounter: fires once every Period ticks. The entity scanners use
// it to spread their work over several frames.
class CTickCounter
{
public:
	CTickCounter() : Count(0), Period(0) {}

	void SetPeriod(int32 period) { Period = period; }
	int32 GetPeriod() { return Period; }

	bool Tick()
	{
		bool shouldTrigger = Count == 0;
		Count++;
		if(Count > Period)
			Count = 0;
		return shouldTrigger;
	}

	void SetCount(int32 count) { Count = count; }

private:
	int32 Count;
	int32 Period;
};

VALIDATE_SIZE(CTickCounter, 0x8);
