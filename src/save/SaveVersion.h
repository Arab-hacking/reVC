#ifndef __GTA_SAVE_VERSION_H__
#define __GTA_SAVE_VERSION_H__

#include "common.h"

// The save format of this fork is not Vice City's: the block chain and the
// structs it carries have been changed since the first build. A save written
// by an older build therefore cannot be read by guessing - the per-type arrays
// changed width (CStats::PedsKilledOfThisType follows ePedType, 23 -> 32,
// CPedType::Save writes one record per type) and the ped states were
// renumbered. Reading such a file would silently mix the two layouts.
//
// The version is stored as a magic word followed by a number, at the end of
// the simple-vars block, so the loader can tell "written by an older build"
// from "written by this one" before any block beyond that point is parsed.
// The magic is what makes the check reliable: an old file has its scripts
// length in that position, a value that cannot spell the magic.
constexpr uint32 SAVE_VERSION_MAGIC = 'R' | ('V' << 8) | ('S' << 16) | ('V' << 24);
constexpr uint32 SAVE_VERSION = 1;

// Bytes the pair occupies inside the simple-vars block.
constexpr uint32 SAVE_VERSION_SIZE = 2 * sizeof(uint32);

inline void
WriteSaveVersion(uint8 *&buf)
{
	memcpy(buf, &SAVE_VERSION_MAGIC, sizeof(uint32));
	buf += sizeof(uint32);
	memcpy(buf, &SAVE_VERSION, sizeof(uint32));
	buf += sizeof(uint32);
}

// Returns false when the block does not carry the marker of this format at
// all, or carries a version this build does not know how to read.
inline bool
ReadSaveVersion(uint8 *&buf, uint32 *found)
{
	uint32 magic = 0;
	uint32 version = 0;
	memcpy(&magic, buf, sizeof(uint32));
	buf += sizeof(uint32);
	memcpy(&version, buf, sizeof(uint32));
	buf += sizeof(uint32);
	if(found)
		*found = version;
	return magic == SAVE_VERSION_MAGIC && version == SAVE_VERSION;
}

#endif // __GTA_SAVE_VERSION_H__
