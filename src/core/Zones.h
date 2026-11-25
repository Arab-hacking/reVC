#ifndef __GTA_ZONES_H__
#define __GTA_ZONES_H__

#include "Game.h"

enum eZoneType
{
	ZONE_DEFAULT,
	ZONE_NAVIG,
	ZONE_INFO,
	ZONE_MAPZONE,
};

class CZone
{
public:
	char       name[8];
	float      minx;
	float      miny;
	float      minz;
	float      maxx;
	float      maxy;
	float      maxz;
	eZoneType  type;
	eLevelName level;
	CZone     *child;
	CZone     *parent;
	CZone     *next;

	wchar *GetTranslatedName(void);
};


class CTheZones
{
	static int16 FindIndex;

	static uint16 NumberOfAudioZones;
	static int16 AudioZoneArray[NUMAUDIOZONES];
	static uint16 TotalNumberOfMapZones;
	static uint16 TotalNumberOfInfoZones;
	static uint16 TotalNumberOfNavigationZones;
	static CZone InfoZoneArray[NUMINFOZONES];
	static CZone MapZoneArray[NUMMAPZONES];
	static CZone NavigationZoneArray[NUMNAVIGZONES];
public:
	static eLevelName m_CurrLevel;

	static void Init(void);
	static void Update(void);
	static void CreateZone(char *name, eZoneType type,
	                       float minx, float miny, float minz,
	                       float maxx, float maxy, float maxz,
	                       eLevelName level);
	static CZone *GetInfoZone(uint16 i) { return &InfoZoneArray[i]; }
	static CZone *GetNavigationZone(uint16 i) { return &NavigationZoneArray[i]; }
	static CZone *GetMapZone(uint16 i) { return &MapZoneArray[i]; }
	static CZone *GetAudioZone(uint16 i) { return &NavigationZoneArray[AudioZoneArray[i]]; }
	static void PostZoneCreation(void);
	static void CheckZonesForOverlap(void);
	static void InsertZoneIntoZoneHierarchy(CZone *zone);
	static bool InsertZoneIntoZoneHierRecursive(CZone *z1, CZone *z2);
	static bool ZoneIsEntirelyContainedWithinOtherZone(CZone *z1, CZone *z2);
	static bool PointLiesWithinZone(const CVector *v, CZone *zone);
	static eLevelName GetLevelFromPosition(const CVector *v);
	static CZone *FindInformationZoneForPosition(const CVector *v);
	static CZone *FindSmallestNavigationZoneForPosition(const CVector *v, bool findDefault, bool findNavig);
	static int16 FindZoneByLabelAndReturnIndex(char *name, eZoneType type);
	static int16 FindNextZoneByLabelAndReturnIndex(char *name, eZoneType type);
	static int16 FindAudioZone(CVector *pos);
	static void AddZoneToAudioZoneArray(CZone *zone);
	static void InitialiseAudioZoneArray(void);
};

#endif // __GTA_ZONES_H__
