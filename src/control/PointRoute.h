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
	void AddUnlessFull(const CVector &entry) { if (!IsFull()) Add(entry); }
	void Clear() { m_NumEntries = 0; }
	void PopFront(void)
	{
		for (int32 i = 1; i < m_NumEntries; i++)
			m_Entries[i - 1] = m_Entries[i];
		if (m_NumEntries > 0)
			m_NumEntries--;
	}
	void PopBack(void)
	{
		if (m_NumEntries > 0)
			m_NumEntries--;
	}
	void Reverse(void)
	{
		for (int32 i = 0; i < m_NumEntries / 2; i++) {
			CVector tmp = m_Entries[i];
			m_Entries[i] = m_Entries[m_NumEntries - 1 - i];
			m_Entries[m_NumEntries - 1 - i] = tmp;
		}
	}
	float GetLength(void) const
	{
		float length = 0.0f;
		for (int32 i = 1; i < m_NumEntries; i++)
			length += (m_Entries[i] - m_Entries[i - 1]).Magnitude();
		return length;
	}
	int32 GetSize() const { return m_NumEntries; }
	void ResizeTo(uint32 size) { m_NumEntries = (int32)size; }

	CVector &operator[](int32 index) { return m_Entries[index]; }
	const CVector &operator[](int32 index) const { return m_Entries[index]; }
};

VALIDATE_SIZE(CPointRoute, 0x64);

#endif // __GTA_POINTROUTE_H__
