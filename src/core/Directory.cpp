#include "common.h"

#include "General.h"
#include "FileMgr.h"
#include "Directory.h"

CDirectory::CDirectory(int32 capacity)
 : entries(nil), maxEntries(capacity > 0 ? capacity : 1), numEntries(0)
{
	entries = new DirectoryInfo[maxEntries];
}

CDirectory::~CDirectory(void)
{
	delete[] entries;
}

void
CDirectory::ReadDirFile(const char *filename)
{
	int fd;
	DirectoryInfo dirinfo;

	fd = CFileMgr::OpenFile(filename, "rb");
	while(CFileMgr::Read(fd, (char*)&dirinfo, sizeof(dirinfo)))
		AddItem(dirinfo);
	CFileMgr::CloseFile(fd);
}

bool
CDirectory::WriteDirFile(const char *filename)
{
	int fd;
	size_t n;
	fd = CFileMgr::OpenFileForWriting(filename);
	n = CFileMgr::Write(fd, (char*)entries, numEntries*sizeof(DirectoryInfo));
	CFileMgr::CloseFile(fd);
	return n == numEntries*sizeof(DirectoryInfo);
}

void
CDirectory::AddItem(const DirectoryInfo &dirinfo)
{
#ifdef FIX_BUGS
	// Keep the first copy (and its IMG offset) when multiple images contain
	// the same model. A duplicate must not consume a directory slot.
	uint32 offset, size;
	if(FindItem(dirinfo.name, offset, size))
		return;
#endif
	// The source may itself be one of entries; copy before any reallocation.
	DirectoryInfo toAdd = dirinfo;
	// Native DFFs without a loaded IDE still belong in the extra-objects
	// directory: scripts can request them later as special models. Disabling
	// the VC map leaves more such entries than the original 256-slot reserve.
	if(numEntries == maxEntries){
		if(maxEntries == INT32_MAX){
			REVC_LOG_ERROR("CD directory cannot hold more than %d entries", maxEntries);
			return;
		}
		int32 newCapacity = maxEntries <= INT32_MAX / 2 ? maxEntries * 2 : INT32_MAX;
		DirectoryInfo *grown = new DirectoryInfo[newCapacity];
		memcpy(grown, entries, (size_t)numEntries * sizeof(*entries));
		delete[] entries;
		entries = grown;
		REVC_LOG_INFO("CD directory capacity increased from %d to %d entries", maxEntries, newCapacity);
		maxEntries = newCapacity;
	}
	entries[numEntries++] = toAdd;
}

void
CDirectory::AddItem(const DirectoryInfo &dirinfo, int32 imgId)
{
	DirectoryInfo di = dirinfo;
	di.offset |= (uint32)imgId<<24;
	AddItem(di);
}

bool
CDirectory::FindItem(const char *name, uint32 &offset, uint32 &size)
{
	int i;

	for(i = 0; i < numEntries; i++)
		if(!CGeneral::faststricmp(entries[i].name, name)){
			offset = entries[i].offset;
			size = entries[i].size;
			return true;
		}
	return false;
}
