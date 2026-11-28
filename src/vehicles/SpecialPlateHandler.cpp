#include "common.h"

#include "SpecialPlateHandler.h"

// SA CSpecialPlateHandler::Init (0x6F2D10). Unused by the game itself, kept
// because the car generator's own Init clears the table the same way.
void
CSpecialPlateHandler::Init(void)
{
	m_nCount = 0;

	for(int32 i = 0; i < PLATES_COUNT; i++){
		m_plateTextEntries[i].m_nCarGenId = -1;
		m_plateTextEntries[i].m_szPlateText[0] = '\0';
	}
}

// SA CSpecialPlateHandler::Find (0x6F2D30). Writes the plate text to outText if
// one is registered for this generator, "\0" otherwise.
void
CSpecialPlateHandler::Find(int32 carGenId, char *outText)
{
	outText[0] = '\0';
	if(m_nCount == 0)
		return;

	for(int32 i = 0; i < PLATES_COUNT; i++){
		if(m_plateTextEntries[i].m_nCarGenId == carGenId){
			strncpy(outText, m_plateTextEntries[i].m_szPlateText, sizeof(m_plateTextEntries[i].m_szPlateText));
			return;
		}
	}
}

// SA CSpecialPlateHandler::Add (0x6F2D90). The table is a stack: a full table
// silently drops the request, as in SA.
void
CSpecialPlateHandler::Add(int32 carGenId, const char *plateText)
{
	if(m_nCount == PLATES_COUNT)
		return;

	tCarGenPlateText *entry = &m_plateTextEntries[m_nCount++];
	entry->m_nCarGenId = carGenId;
	strncpy(entry->m_szPlateText, plateText, sizeof(entry->m_szPlateText));
	entry->m_szPlateText[sizeof(entry->m_szPlateText) - 1] = '\0';
}

// SA CSpecialPlateHandler::Remove (0x6F2DD0). Unused by the game itself. SA
// moves the last entry into the hole rather than shifting the tail down,
// which keeps insertion order meaningless but makes removal O(1).
void
CSpecialPlateHandler::Remove(int32 plateTextId)
{
	if(plateTextId < 0 || m_nCount == 0)
		return;

	tCarGenPlateText *plateText = &m_plateTextEntries[plateTextId];
	plateText->m_nCarGenId = -1;
	plateText->m_szPlateText[0] = '\0';

	if((uint32)plateTextId < m_nCount - 1)
		*plateText = m_plateTextEntries[m_nCount - 1];

	m_nCount--;
}
