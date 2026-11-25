#include "common.h"

#include "NodeAddress.h"
#include "PathFind.h"

// SA addresses path nodes by (area, node); reVC's network is one global
// array, so the node id is the global index and area 0 is "valid".

CPathNode*
NodeFromAddress(const CNodeAddress &address)
{
	return address.IsValid() ? ThePaths.GetNode(address.m_wNodeId) : nil;
}

CNodeAddress
AddressFromNode(CPathNode *node)
{
	return node ? CNodeAddress(0, ThePaths.GetIndex(node)) : CNodeAddress();
}
