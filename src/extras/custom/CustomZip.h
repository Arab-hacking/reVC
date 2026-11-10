#ifndef __GTA_CUSTOMZIP_H__
#define __GTA_CUSTOMZIP_H__

// Read-only ZIP archive reader. The game uses it ONLY for whitelisted map
// text, data/peds.ide, data/vehicles.ide, data/weapons.ide, and selected
// data/config.dat, data/timecyc.json, data/water.dat, data/carcols.dat inside
// custom/common.zip; it never enables models, textures, collision or other
// user ZIPs. Archives may be several GB, so only
// the central directory is read at startup; file data is decompressed on
// demand, one entry at a time.
//
// Supported: stored and deflated entries, ZIP64 sizes/offsets for big archives.
// Everything else (encryption, other compression methods) is rejected.
//
// The archives are ordinary files in the game folder, so plain file I/O is
// used - nothing here depends on the game's own file manager, which is why the
// module can be built and tested on its own.

#include "common.h"

#define CUSTOMZIP_MAX_NAME 256 // nested data/maps/** paths in common.zip
#define CUSTOMZIP_MAX_ENTRY (64*1024*1024)	// one entry may be this big

// one entry of the central directory
struct CustomZipEntry
{
	char name[CUSTOMZIP_MAX_NAME];	// name as stored in the archive (path included)
	uint64 offset;			// offset of the local file header
	uint64 compressedSize;
	uint64 uncompressedSize;
	uint16 method;			// 0 = stored, 8 = deflate
};

class CustomZip
{
	char m_path[256];
	bool m_opened;
	bool m_useXor;
	int m_fd;
	CustomZipEntry *m_entries;
	int m_numEntries;
	uint64 m_fileSize;

	bool ReadRawAt(uint64 offset, void *dst, uint32 size) const;
	bool ReadAt(uint64 offset, void *dst, uint32 size) const;
public:
	CustomZip(void);
	~CustomZip(void);

	// Applies the 20-byte BPC stream XOR ("1cK1a5UF2tU8*G2lW#&%") in place at arbitrary byte offset.
	static void ApplyBpcXor(uint64 offset, uint8 *data, uint32 size);

	// reads the central directory; when allowXor is true, auto-detects plain ZIP vs 20-byte XOR .bpc
	bool Open(const char *path, bool allowXor = false);
	void Close(void);
	bool IsOpened(void) const { return m_opened; }
	bool IsXorEncrypted(void) const { return m_useXor; }
	const char *GetPath(void) const { return m_path; }
	uint64 GetFileSize(void) const { return m_fileSize; }

	int GetNumEntries(void) const { return m_numEntries; }
	const CustomZipEntry *GetEntry(int i) const { return i >= 0 && i < m_numEntries ? &m_entries[i] : nil; }

	// decompresses an entry into dst (which has to be at least uncompressedSize
	// bytes large); false on error or if the data does not fit
	bool Extract(int index, uint8 *dst, uint32 dstSize) const;
};

// Decompresses a raw DEFLATE stream (RFC 1951) into dst. Returns the number of
// bytes written, or -1 on error.
int32 CustomInflate(const uint8 *src, uint32 srcSize, uint8 *dst, uint32 dstSize);

#endif // __GTA_CUSTOMZIP_H__
