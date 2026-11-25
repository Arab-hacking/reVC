#ifndef __GTA_POINTROUTE_H__
#define __GTA_POINTROUTE_H__

#include "Vector.h"

// SA CPointRoute: the point-space twin of CNodeRoute (notsa::Route<CVector, 8>) -
// the same fixed 8-slot list, holding positions instead of node addresses.
class CPointRoute
{
public:
	CVector m_Entries[8];
	int32 m_NumEntries;

	CPointRoute() : m_NumEntries(0) {}

	bool IsEmpty() const { return m_NumEntries == 0; }
	bool IsFull() const { return m_NumEntries >= 8; }
	void Add(const CVector &entry) { m_Entries[m_NumEntries++] = entry; }
	void Clear() { m_NumEntries = 0; }
	int32 GetSize() const { return m_NumEntries; }
	void ResizeTo(uint32 size) { m_NumEntries = (int32)size; }

	CVector &operator[](int32 index) { return m_Entries[index]; }
	const CVector &operator[](int32 index) const { return m_Entries[index]; }
};

VALIDATE_SIZE(CPointRoute, 0x64);

#endif // __GTA_POINTROUTE_H__
