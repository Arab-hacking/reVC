#include "common.h"

#include "CustomZip.h"

#include <stdio.h>

// DEFLATE (RFC 1951) decompressor. Small and self contained: the archives are
// read from the game process, so no external dependency is wanted here.

namespace deflate {

struct BitReader
{
	const uint8 *data;
	uint32 size;
	uint32 pos;
	uint32 buf;
	int cnt;

	void Init(const uint8 *d, uint32 n) { data = d; size = n; pos = 0; buf = 0; cnt = 0; }
	bool Fill(int n)
	{
		while(cnt < n){
			if(pos >= size) return false;
			buf |= (uint32)data[pos++] << cnt;
			cnt += 8;
		}
		return true;
	}
	uint32 Get(int n)
	{
		if(n == 0) return 0;
		if(!Fill(n)){ cnt = 0; buf = 0; return 0; }
		uint32 v = buf & ((1u << n) - 1);
		buf >>= n; cnt -= n;
		return v;
	}
	int Align(void)
	{
		int r = cnt & 7;
		if(r){ buf >>= r; cnt -= r; }
		return r;
	}
};

// canonical Huffman code, decoded bit by bit (files are small, simplicity wins)
struct Huffman
{
	uint16 counts[16];
	uint16 symbols[288];

	void Build(const uint8 *lengths, int n)
	{
		int i, l;
		uint16 offs[16];
		for(i = 0; i < 16; i++) counts[i] = 0;
		for(i = 0; i < n; i++) counts[lengths[i]]++;
		counts[0] = 0;
		offs[1] = 0;
		for(l = 1; l < 15; l++) offs[l+1] = offs[l] + counts[l];
		for(i = 0; i < n; i++) if(lengths[i]) symbols[offs[lengths[i]]++] = (uint16)i;
	}

	int Decode(BitReader &br) const
	{
		int code = 0, first = 0, index = 0, len;
		for(len = 1; len <= 15; len++){
			code |= (int)br.Get(1);
			int count = counts[len];
			if(code - first < count) return symbols[index + (code - first)];
			index += count;
			first = (first + count) << 1;
			code <<= 1;
		}
		return -1;
	}
};

static const uint16 lenBase[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8 lenExtra[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16 distBase[30] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
	193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8 distExtra[30] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static void
FixedTables(Huffman &lit, Huffman &dist)
{
	uint8 lengths[288];
	int i;
	for(i = 0; i < 144; i++) lengths[i] = 8;
	for(; i < 256; i++) lengths[i] = 9;
	for(; i < 280; i++) lengths[i] = 7;
	for(; i < 288; i++) lengths[i] = 8;
	lit.Build(lengths, 288);
	for(i = 0; i < 30; i++) lengths[i] = 5;
	dist.Build(lengths, 30);
}

static bool
Codes(BitReader &br, Huffman &lit, Huffman &dist)
{
	static const uint8 order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	uint8 lengths[320];
	int nlen, i;

	nlen = (int)br.Get(5) + 257;
	int ndist = (int)br.Get(5) + 1;
	int ncode = (int)br.Get(4) + 4;
	if(nlen > 286 || ndist > 30) return false;

	for(i = 0; i < 19; i++) lengths[i] = 0;
	for(i = 0; i < ncode; i++) lengths[order[i]] = (uint8)br.Get(3);
	Huffman codelen;
	codelen.Build(lengths, 19);

	i = 0;
	while(i < nlen + ndist){
		int sym = codelen.Decode(br);
		if(sym < 0) return false;
		if(sym < 16){
			lengths[i++] = (uint8)sym;
		}else{
			int len = 0, rep;
			if(sym == 16){
				if(i == 0) return false;
				len = lengths[i-1];
				rep = 3 + (int)br.Get(2);
			}else if(sym == 17){
				rep = 3 + (int)br.Get(3);
			}else{
				rep = 11 + (int)br.Get(7);
			}
			if(i + rep > nlen + ndist) return false;
			while(rep--) lengths[i++] = (uint8)len;
		}
	}
	if(lengths[256] == 0) return false;	// no end of block code
	lit.Build(lengths, nlen);
	dist.Build(lengths + nlen, ndist);
	return true;
}

} // namespace deflate

int32
CustomInflate(const uint8 *src, uint32 srcSize, uint8 *dst, uint32 dstSize)
{
	using namespace deflate;

	BitReader br;
	Huffman fixedLit, fixedDist, lit, dist;
	bool fixedReady = false;
	uint32 out = 0;

	br.Init(src, srcSize);
	for(;;){
		int last = (int)br.Get(1);
		int type = (int)br.Get(2);
		if(type == 0){
			br.Align();
			if(br.pos + 4 > br.size) return -1;
			uint32 len = (uint32)br.data[br.pos] | ((uint32)br.data[br.pos+1] << 8);
			br.pos += 4;
			if(len > br.size - br.pos || len > dstSize - out) return -1;
			memcpy(dst + out, br.data + br.pos, len);
			br.pos += len;
			out += len;
		}else{
			if(type == 1){
				if(!fixedReady){ FixedTables(fixedLit, fixedDist); fixedReady = true; }
				lit = fixedLit; dist = fixedDist;
			}else if(type == 2){
				if(!Codes(br, lit, dist)) return -1;
			}else{
				return -1;	// reserved
			}
			for(;;){
				int sym = lit.Decode(br);
				if(sym < 0) return -1;
				if(sym < 256){
					if(out >= dstSize) return -1;
					dst[out++] = (uint8)sym;
				}else if(sym == 256){
					break;
				}else{
					sym -= 257;
					if(sym >= 29) return -1;
					uint32 length = lenBase[sym] + br.Get(lenExtra[sym]);
					int dsym = dist.Decode(br);
					if(dsym < 0 || dsym >= 30) return -1;
					uint32 distance = distBase[dsym] + br.Get(distExtra[dsym]);
					if(distance > out || length > dstSize - out) return -1;
					uint32 from = out - distance;
					while(length--) dst[out++] = dst[from++];
				}
			}
		}
		if(last) break;
	}
	return (int32)out;
}


CustomZip::CustomZip(void)
{
	m_path[0] = '\0';
	m_opened = false;
	m_entries = nil;
	m_numEntries = 0;
	m_fileSize = 0;
}

CustomZip::~CustomZip(void)
{
	Close();
}

void
CustomZip::Close(void)
{
	delete[] m_entries;
	m_entries = nil;
	m_numEntries = 0;
	m_opened = false;
}

static uint32
GetU16(const uint8 *p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8);
}

static uint32
GetU32(const uint8 *p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

// The archives are plain files in the game folder. 64 bit file offsets are
// used so that map packs of several gigabytes work.
#if defined(_WIN32)
#define CUSTOMZIP_SEEK(f, o) (_fseeki64(f, (int64)(o), SEEK_SET) == 0)
#define CUSTOMZIP_TELL(f)    ((int64)_ftelli64(f))
#else
#define CUSTOMZIP_SEEK(f, o) (fseeko(f, (off_t)(o), SEEK_SET) == 0)
#define CUSTOMZIP_TELL(f)    ((int64)ftello(f))
#endif

bool
CustomZip::ReadAt(uint64 offset, void *dst, uint32 size) const
{
	FILE *f;
	bool ok;

	if(size == 0)
		return true;
	if(m_path[0] == '\0')
		return false;
	f = fopen(m_path, "rb");
	if(f == nil)
		return false;
	ok = CUSTOMZIP_SEEK(f, offset) && fread(dst, 1, size, f) == size;
	fclose(f);
	return ok;
}

// size of a file, without opening it twice
static bool
FileSize(const char *path, uint64 &size)
{
	FILE *f = fopen(path, "rb");
	if(f == nil)
		return false;
	if(fseek(f, 0, SEEK_END) != 0){
		fclose(f);
		return false;
	}
	int64 n = CUSTOMZIP_TELL(f);
	fclose(f);
	if(n < 0)
		return false;
	size = (uint64)n;
	return true;
}

static uint64
GetU64(const uint8 *p)
{
	return (uint64)GetU32(p) | ((uint64)GetU32(p+4) << 32);
}

bool
CustomZip::Open(const char *path)
{
	uint32 i, size;
	uint8 *tail = nil;
	uint64 eocdPos = 0, cdOffset = 0, cdSize = 0;
	uint32 numEntries = 0;
	uint32 eocdInTail = 0;
	bool found = false;

	Close();
	strncpy(m_path, path, sizeof(m_path)-1);
	m_path[sizeof(m_path)-1] = '\0';

	if(!FileSize(m_path, m_fileSize)){
		debug("custom: cannot open archive %s\n", m_path);
		return false;
	}
	if(m_fileSize < 22) return false;

	// The end of central directory record sits at the end of the file, behind a
	// comment of unknown length; scan back for its signature.
	size = m_fileSize < 66560 ? (uint32)m_fileSize : 66560;	// EOCD + comment + zip64 locator
	tail = new uint8[size];
	if(!ReadAt(m_fileSize - size, tail, size)){
		delete[] tail;
		return false;
	}
	// The record itself has to be read where it is in the tail, not where the
	// tail is in the file.
	for(i = size - 22; i != (uint32)-1; i--){
		if(GetU32(tail + i) == 0x06054b50){ eocdInTail = i; found = true; break; }
	}
	if(!found){ delete[] tail; debug("custom: %s is not a zip (no EOCD)\n", m_path); return false; }
	numEntries = GetU16(tail + eocdInTail + 10);
	cdSize = GetU32(tail + eocdInTail + 12);
	cdOffset = GetU32(tail + eocdInTail + 16);
	eocdPos = m_fileSize - size + eocdInTail;
	delete[] tail;

	// ZIP64: the fields above are 0xFFFFFFFF when they do not fit
	if(cdOffset == 0xFFFFFFFF || cdSize == 0xFFFFFFFF || numEntries == 0xFFFF){
		uint8 loc[20];
		uint8 rec[56];
		uint64 locOff;
		if(eocdInTail < 20 || eocdPos < 20 || cdSize == 0) return false;
		if(!ReadAt(eocdPos - 20, loc, 20) || GetU32(loc) != 0x07064b50) return false;
		locOff = GetU64(loc + 8);
		if(locOff + 56 > m_fileSize) return false;
		if(!ReadAt(locOff, rec, 56) || GetU32(rec) != 0x06064b50) return false;
		numEntries = GetU32(rec + 32);	// the low half of the 64 bit count
		cdSize = GetU64(rec + 40);
		cdOffset = GetU64(rec + 48);
	}

	if(cdOffset > m_fileSize || cdSize > m_fileSize - cdOffset){
		debug("custom: %s has a broken central directory\n", m_path);
		return false;
	}
	if(numEntries == 0) return false;
	if(numEntries > 100000) numEntries = 100000;		// sanity
	if(cdSize > 64*1024*1024) return false;			// sanity

	m_entries = new CustomZipEntry[numEntries];
	m_numEntries = 0;

	uint8 *cd = new uint8[(uint32)cdSize];
	if(!ReadAt(cdOffset, cd, (uint32)cdSize)){
		delete[] cd;
		Close();
		return false;
	}

	uint32 p = 0;
	for(i = 0; i < numEntries && p + 46 <= (uint32)cdSize; i++){
		if(GetU32(cd + p) != 0x02014b50) break;
		uint16 method = (uint16)GetU16(cd + p + 10);
		uint64 compSize = GetU32(cd + p + 20);
		uint64 uncompSize = GetU32(cd + p + 24);
		uint32 nameLen = GetU16(cd + p + 28);
		uint32 extraLen = GetU16(cd + p + 30);
		uint32 commentLen = GetU16(cd + p + 32);
		uint64 localOff = GetU32(cd + p + 42);
		if(p + 46 + nameLen + extraLen + commentLen > (uint32)cdSize) break;

		// ZIP64 extended information (used when the 32 bit fields overflow)
		if(compSize == 0xFFFFFFFF || uncompSize == 0xFFFFFFFF || localOff == 0xFFFFFFFF){
			uint32 e = p + 46 + nameLen;
			uint32 eend = e + extraLen;
			while(e + 4 <= eend){
				uint16 id = (uint16)GetU16(cd + e);
				uint16 len = (uint16)GetU16(cd + e + 2);
				if(e + 4 + len > eend) break;
				if(id == 0x0001){
					uint32 q = e + 4;
					if(uncompSize == 0xFFFFFFFF && q + 8 <= e + 4 + len){ uncompSize = GetU64(cd + q); q += 8; }
					if(compSize == 0xFFFFFFFF && q + 8 <= e + 4 + len){ compSize = GetU64(cd + q); q += 8; }
					if(localOff == 0xFFFFFFFF && q + 8 <= e + 4 + len){ localOff = GetU64(cd + q); q += 8; }
					break;
				}
				e += 4 + len;
			}
		}

		CustomZipEntry &ent = m_entries[m_numEntries];
		uint32 copy = nameLen < CUSTOMZIP_MAX_NAME-1 ? nameLen : CUSTOMZIP_MAX_NAME-1;
		memcpy(ent.name, cd + p + 46, copy);
		ent.name[copy] = '\0';
		ent.method = method;
		ent.compressedSize = compSize;
		ent.uncompressedSize = uncompSize;
		ent.offset = localOff;
		m_numEntries++;
		p += 46 + nameLen + extraLen + commentLen;
	}
	delete[] cd;

	m_opened = m_numEntries > 0;
	debug("custom: archive %s: %d entries\n", m_path, m_numEntries);
	return m_opened;
}

bool
CustomZip::Extract(int index, uint8 *dst, uint32 dstSize) const
{
	const CustomZipEntry *ent = GetEntry(index);
	uint8 header[30];
	uint32 nameLen, extraLen;
	uint64 dataOffset;
	uint8 *comp = nil;
	bool ok = false;

	if(ent == nil || !m_opened) return false;
	if(ent->uncompressedSize > dstSize) return false;
	if(ent->compressedSize > CUSTOMZIP_MAX_ENTRY + 1024*1024) return false;
	if(ent->compressedSize == 0){
		if(ent->uncompressedSize != 0) return false;
		return true;
	}
	if(!ReadAt(ent->offset, header, 30) || GetU32(header) != 0x04034b50) return false;
	nameLen = GetU16(header + 26);
	extraLen = GetU16(header + 28);
	dataOffset = ent->offset + 30 + nameLen + extraLen;
	if(dataOffset > m_fileSize || ent->compressedSize > m_fileSize - dataOffset) return false;

	comp = new uint8[(uint32)ent->compressedSize];
	if(!ReadAt(dataOffset, comp, (uint32)ent->compressedSize)){
		delete[] comp;
		return false;
	}

	if(ent->method == 0){
		if(ent->compressedSize == ent->uncompressedSize){
			memcpy(dst, comp, (uint32)ent->uncompressedSize);
			ok = true;
		}
	}else if(ent->method == 8){
		int32 n = CustomInflate(comp, (uint32)ent->compressedSize, dst, dstSize);
		ok = n >= 0 && (uint32)n == (uint32)ent->uncompressedSize;
	}
	delete[] comp;
	return ok;
}
