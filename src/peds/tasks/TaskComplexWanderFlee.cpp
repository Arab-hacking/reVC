#include "TaskComplexWanderFlee.h"

// SA CTaskComplexWanderFlee (0x65B320): a non-sensible wander - fleeing
// peds don't stop to think about junctions.

CTaskComplexWanderFlee::CTaskComplexWanderFlee(eMoveState moveState, uint8 dir)
: CTaskComplexWander(moveState, dir, false)
{
}

CTaskComplexWanderFlee::CTaskComplexWanderFlee(const CTaskComplexWanderFlee &other)
: CTaskComplexWanderFlee(other.m_nMoveState, other.m_nDir)
{
}
