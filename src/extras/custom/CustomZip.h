#ifndef __GTA_CUSTOMZIP_H__
#define __GTA_CUSTOMZIP_H__

// Read-only ZIP archive reader that keeps everything in memory.
//
// The custom model feature reads .mod/.btx/.cls files straight out of the zip
// archives the player drops into the game folder, so the archives have to be
// readable at runtime without any external tooling. The archives can be big
// (map packs are several GB), so only the central directory is read at start
// up; file data is decompressed on demand, one entry at a time.
//
// Supported: stored and deflated entries, ZIP64 sizes/offsets for big archives.
// Everything else (encryption, other compression methods) is rejected.
//
// The archives are ordinary files in the game folder, so plain file I/O is
// used - nothing here depends on the game's own file manager, which is why the
// module can be built and tested on its own.

#include "common.h"

#define CUSTOMZIP_MAX_NAME 128
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
	CustomZipEntry *m_entries;
	int m_numEntries;
	uint64 m_fileSize;

	bool ReadAt(uint64 offset, void *dst, uint32 size) const;
public:
	CustomZip(void);
	~CustomZip(void);

	// reads the central directory; false if the file is not a readable zip
	bool Open(const char *path);
	void Close(void);
	bool IsOpened(void) const { return m_opened; }
	const char *GetPath(void) const { return m_path; }

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
