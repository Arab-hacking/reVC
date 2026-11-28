#ifndef __GTA_SPECIALPLATEHANDLER_H__
#define __GTA_SPECIALPLATEHANDLER_H__

// SA CSpecialPlateHandler (0x6F2D10-0x6F2DD0).
//
// SA lets a car generator carry a custom number plate: the script stores a
// plate text keyed by the generator's index, and when that generator spawns a
// car the plate is pushed into the vehicle model's plate material. The table
// is flat and tiny (15 slots), and entries are never compacted downwards on
// removal - only the last entry is moved into the hole.
struct tCarGenPlateText
{
	int32 m_nCarGenId;	// -1 == empty slot
	char m_szPlateText[12];
};

class CSpecialPlateHandler
{
public:
	enum { PLATES_COUNT = 15 };

	tCarGenPlateText m_plateTextEntries[PLATES_COUNT];
	uint32 m_nCount;

	void Init(void);
	void Find(int32 carGenId, char *outText);
	void Add(int32 carGenId, const char *plateText);
	void Remove(int32 plateTextId);
};

#endif // __GTA_SPECIALPLATEHANDLER_H__
