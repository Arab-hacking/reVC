#ifndef __GTA_ANIMFILEHEADER_H__
#define __GTA_ANIMFILEHEADER_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ANP2/ANP3 files have two on-disk headers. PC IFPs have a length before the
// group name; SA mobile / BR .ani files have the group name immediately after
// the magic and an ignored group value after the animation count. In
// particular, that value is NOT another animation count (it can be 0 or 1).
namespace animfile {

struct ANP23Header {
	char blockName[24];
	uint32_t numAnims;
	bool mobile;
};

inline uint32_t
ReadU32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline bool
ReadGroupName(const uint8_t *src, char (&name)[24])
{
	if(!((src[0] >= 'a' && src[0] <= 'z') ||
	     (src[0] >= 'A' && src[0] <= 'Z') ||
	     (src[0] >= '0' && src[0] <= '9') || src[0] == '_'))
		return false;
	// CAnimBlock::name holds 20 bytes, including the terminator. Do not let
	// the game compare a truncated block name against the actual dictionary.
	size_t i = 0;
	for(; i < 20; i++){
		if(src[i] == 0)
			break;
		if(src[i] < 0x20 || src[i] >= 0x7f)
			return false;
	}
	if(i == 20)
		return false;
	memcpy(name, src, i);
	name[i] = '\0';
	return true;
}

// bytes points to the 32 bytes following the FourCC. fileSize may be zero
// when a stream does not expose its length; otherwise a PC root must fit in
// it. Both layouts have a 36-byte header and animation data starts at +36.
inline bool
ParseANP23Header(const uint8_t *bytes, size_t available, size_t fileSize, ANP23Header &header)
{
	if(bytes == NULL || available < 32 || (fileSize != 0 && fileSize < 36))
		return false;
	char mobileName[24], pcName[24];
	uint32_t mobileCount = ReadU32(bytes + 24);
	uint32_t pcCount = ReadU32(bytes + 28);
	uint32_t pcSize = ReadU32(bytes);
	bool mobile = ReadGroupName(bytes, mobileName) && mobileCount > 0 && mobileCount <= 2500;
	bool pc = ReadGroupName(bytes + 4, pcName) && pcCount > 0 && pcCount <= 2500 &&
		pcSize >= 28 && (fileSize == 0 || pcSize <= fileSize - 8);
	if(!mobile && !pc)
		return false;
	// A real PC root length takes precedence if both candidates appear valid.
	// For a mobile file with a long name, its ASCII first word is far larger
	// than the actual file and the PC candidate cannot pass the size check.
	header.mobile = mobile && (!pc || (fileSize == 0 && pcSize > 64u * 1024u * 1024u));
	memcpy(header.blockName, header.mobile ? mobileName : pcName, sizeof(header.blockName));
	header.numAnims = header.mobile ? mobileCount : pcCount;
	return true;
}

} // namespace animfile

#endif // __GTA_ANIMFILEHEADER_H__
