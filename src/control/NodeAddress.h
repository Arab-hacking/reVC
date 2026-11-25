#ifndef __GTA_NODEADDRESS_H__
#define __GTA_NODEADDRESS_H__

// SA CNodeAddress: identifies one path node. SA's path network is split
// into areas, so the address is (area, node); reVC's CPathFind keeps every
// node in one global array, so the node id is the global node index and
// the area id is only carried as the validity marker SA uses (ResetAreaId
// is how SA code says "this address no longer points anywhere").
class CNodeAddress
{
public:
	int16 m_wAreaId;
	int16 m_wNodeId;

	CNodeAddress() : m_wAreaId(-1), m_wNodeId(-1) {}
	CNodeAddress(int16 areaId, int16 nodeId) : m_wAreaId(areaId), m_wNodeId(nodeId) {}

	bool IsValid() const { return m_wAreaId != -1 && m_wNodeId >= 0; }
	bool IsAreaValid() const { return m_wAreaId != -1; }
	void ResetAreaId() { m_wAreaId = -1; }

	bool operator==(const CNodeAddress &other) const { return m_wAreaId == other.m_wAreaId && m_wNodeId == other.m_wNodeId; }
	bool operator!=(const CNodeAddress &other) const { return !(*this == other); }
};

VALIDATE_SIZE(CNodeAddress, 0x4);

#endif // __GTA_NODEADDRESS_H__
