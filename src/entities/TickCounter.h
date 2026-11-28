#ifndef __GTA_TICKCOUNTER_H__
#define __GTA_TICKCOUNTER_H__

// SA CTickCounter. A down-counter that fires once every `period` calls.
//
// Entities are scanned for nearby peds, vehicles and objects at a fixed rate,
// not every frame: with a few hundred peds a per-frame sector scan costs more
// than everything else in the ped update. SA staggers the peds with this
// counter so only a slice of them scans each frame. The semantics are unusual
// and worth stating: the first call fires, then it goes quiet for `period`
// calls, so `Tick` returns true on calls 1, period+2, 2*period+3, ...
class CTickCounter
{
	int32 m_nCount;
	int32 m_nPeriod;

public:
	CTickCounter(void) : m_nCount(0), m_nPeriod(0) {}

	void SetPeriod(int32 period) { m_nPeriod = period; }
	int32 GetPeriod(void) const { return m_nPeriod; }
	void SetCount(int32 count) { m_nCount = count; }

	bool Tick(void)
	{
		const bool shouldTrigger = m_nCount == 0;
		m_nCount++;
		if(m_nCount > m_nPeriod)
			m_nCount = 0;
		return shouldTrigger;
	}
};

#endif // __GTA_TICKCOUNTER_H__
