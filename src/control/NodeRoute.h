#ifndef __GTA_NODEROUTE_H__
#define __GTA_NODEROUTE_H__

#include "NodeAddress.h"

// SA CNodeRoute: a path route as node addresses - a fixed 8-slot list
// (SA's notsa::Route<CNodeAddress, 8>) filled by CPathFind::ComputeRoute
// and walked by CTaskComplexFollowNodeRoute. SA pools these; here they
// are plain heap objects, which the follow-route task owns one of.
class CNodeRoute
{
public:
	CNodeAddress m_Entries[8];
	int32 m_NumEntries;

	CNodeRoute() : m_NumEntries(0) {}

	bool IsEmpty() const { return m_NumEntries == 0; }
	bool IsFull() const { return m_NumEntries >= 8; }
	void Add(const CNodeAddress &entry) { m_Entries[m_NumEntries++] = entry; }
	void Clear() { m_NumEntries = 0; }
	int32 GetSize() const { return m_NumEntries; }
	void ResizeTo(uint32 size) { m_NumEntries = (int32)size; }
	void Reverse()
	{
		for(int32 i = 0; i < m_NumEntries / 2; i++){
			CNodeAddress tmp = m_Entries[i];
			m_Entries[i] = m_Entries[m_NumEntries - 1 - i];
			m_Entries[m_NumEntries - 1 - i] = tmp;
		}
	}

	CNodeAddress &operator[](int32 index) { return m_Entries[index]; }
	const CNodeAddress &operator[](int32 index) const { return m_Entries[index]; }
};

VALIDATE_SIZE(CNodeRoute, 0x24);

#endif // __GTA_NODEROUTE_H__
