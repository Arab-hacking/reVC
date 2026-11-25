#include "common.h"

#include "PlayerPedData.h"

// CPlayerPedData's save block.
//
// Every field is written in declaration order through one macro pair, so the
// reader and the writer cannot drift apart: the static_assert at the end turns
// a size mismatch into a compile error rather than into a save that loads
// garbage.
//
// The two pointers (m_pDangerCar, m_LastHSMissileTarget) are not written: an
// entity address means nothing across a load, and both are re-acquired by the
// code that uses them. They keep their place in the block as four zero bytes so
// that the layout stays fixed.
//
// The bitfield pair at the end (m_nLastHSMissileLOSTime / m_bLastHSMissileLOS)
// cannot be addressed, so it is read and written through a temporary word.

