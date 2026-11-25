#include "common.h"

#include <ctype.h>

#include "Zones.h"

#include "Clock.h"
#include "Text.h"
#include "World.h"
#include "Timer.h"

eLevelName CTheZones::m_CurrLevel;
int16 CTheZones::FindIndex;

uint16 CTheZones::NumberOfAudioZones;
int16 CTheZones::AudioZoneArray[NUMAUDIOZONES];
uint16 CTheZones::TotalNumberOfMapZones;
uint16 CTheZones::TotalNumberOfInfoZones;
uint16 CTheZones::TotalNumberOfNavigationZones;
CZone CTheZones::InfoZoneArray[NUMINFOZONES];
CZone CTheZones::MapZoneArray[NUMMAPZONES];
CZone CTheZones::NavigationZoneArray[NUMNAVIGZONES];


#define SWAPF(a, b) { float t; t = a; a = b; b = t; }

wchar*
CZone::GetTranslatedName(void)
{
	return TheText.Get(name);
}

void
CTheZones::Init(void)
{
	int i;
	for(i = 0; i < NUMAUDIOZONES; i++)
		AudioZoneArray[i] = -1;
	NumberOfAudioZones = 0;

	for(i = 0; i < NUMNAVIGZONES; i++)
		memset(&NavigationZoneArray[i], 0, sizeof(CZone));

	for(i = 0; i < NUMINFOZONES; i++)
		memset(&InfoZoneArray[i], 0, sizeof(CZone));

	TotalNumberOfNavigationZones = 1;
	TotalNumberOfInfoZones = 1;

	strcpy(InfoZoneArray[0].name, "CITYINF");
	InfoZoneArray[0].minx = -2400.0f;
	InfoZoneArray[0].miny = -2000.0f;
	InfoZoneArray[0].minz = -500.0f;
	InfoZoneArray[0].maxx =  1600.0f;
	InfoZoneArray[0].maxy =  2000.0f;
	InfoZoneArray[0].maxz =  500.0f;
	InfoZoneArray[0].level = LEVEL_GENERIC;
	InfoZoneArray[0].type = ZONE_INFO;

	strcpy(NavigationZoneArray[0].name, "VICE_C");
	NavigationZoneArray[0].minx = -2400.0f;
	NavigationZoneArray[0].miny = -2000.0f;
	NavigationZoneArray[0].minz = -500.0f;
	NavigationZoneArray[0].maxx =  1600.0f;
	NavigationZoneArray[0].maxy =  2000.0f;
	NavigationZoneArray[0].maxz =  500.0f;
	NavigationZoneArray[0].level = LEVEL_GENERIC;
	NavigationZoneArray[0].type = ZONE_DEFAULT;

	m_CurrLevel = LEVEL_GENERIC;

	for(i = 0; i < NUMMAPZONES; i++){
		memset(&MapZoneArray[i], 0, sizeof(CZone));
		MapZoneArray[i].type = ZONE_MAPZONE;
	}
	TotalNumberOfMapZones = 1;
	strcpy(MapZoneArray[0].name, "THEMAP");
	MapZoneArray[0].minx = -2400.0f;
	MapZoneArray[0].miny = -2000.0f;
	MapZoneArray[0].minz = -500.0f;
	MapZoneArray[0].maxx =  1600.0f;
	MapZoneArray[0].maxy =  2000.0f;
	MapZoneArray[0].maxz =  500.0f;
	MapZoneArray[0].level = LEVEL_GENERIC;
}

void
CTheZones::Update(void)
{
#ifdef SQUEEZE_PERFORMANCE
	if (CTimer::GetFrameCounter() % 5 != 0)
		return;
#endif
	CVector pos;
	pos = FindPlayerCoors();
	m_CurrLevel = GetLevelFromPosition(&pos);
}

void
CTheZones::CreateZone(char *name, eZoneType type,
	              float minx, float miny, float minz,
	              float maxx, float maxy, float maxz,
	              eLevelName level)
{
	char tmpname[24];
	char *p;

	if(minx > maxx) SWAPF(minx, maxx);
	if(miny > maxy) SWAPF(miny, maxy);
	if(minz > maxz) SWAPF(minz, maxz);

	// make upper case
	for(p = name; *p; p++) if(islower(*p)) *p = toupper(*p);

	strncpy(tmpname, name, 7);
	tmpname[7] = '\0';

	// add zone
	switch(type){
	case ZONE_DEFAULT:
	case ZONE_NAVIG:
		assert(TotalNumberOfNavigationZones < NUMNAVIGZONES);
		strcpy(NavigationZoneArray[TotalNumberOfNavigationZones].name, tmpname);
		NavigationZoneArray[TotalNumberOfNavigationZones].type = type;
		NavigationZoneArray[TotalNumberOfNavigationZones].minx = minx;
		NavigationZoneArray[TotalNumberOfNavigationZones].miny = miny;
		NavigationZoneArray[TotalNumberOfNavigationZones].minz = minz;
		NavigationZoneArray[TotalNumberOfNavigationZones].maxx = maxx;
		NavigationZoneArray[TotalNumberOfNavigationZones].maxy = maxy;
		NavigationZoneArray[TotalNumberOfNavigationZones].maxz = maxz;
		NavigationZoneArray[TotalNumberOfNavigationZones].level = level;
		TotalNumberOfNavigationZones++;
		break;
	case ZONE_INFO:
		assert(TotalNumberOfInfoZones < NUMINFOZONES);
		strcpy(InfoZoneArray[TotalNumberOfInfoZones].name, tmpname);
		InfoZoneArray[TotalNumberOfInfoZones].type = type;
		InfoZoneArray[TotalNumberOfInfoZones].minx = minx;
		InfoZoneArray[TotalNumberOfInfoZones].miny = miny;
		InfoZoneArray[TotalNumberOfInfoZones].minz = minz;
		InfoZoneArray[TotalNumberOfInfoZones].maxx = maxx;
		InfoZoneArray[TotalNumberOfInfoZones].maxy = maxy;
		InfoZoneArray[TotalNumberOfInfoZones].maxz = maxz;
		InfoZoneArray[TotalNumberOfInfoZones].level = level;
		TotalNumberOfInfoZones++;
		break;
	case ZONE_MAPZONE:
		assert(TotalNumberOfMapZones < NUMMAPZONES);
		strcpy(MapZoneArray[TotalNumberOfMapZones].name, tmpname);
		MapZoneArray[TotalNumberOfMapZones].type = type;
		MapZoneArray[TotalNumberOfMapZones].minx = minx;
		MapZoneArray[TotalNumberOfMapZones].miny = miny;
		MapZoneArray[TotalNumberOfMapZones].minz = minz;
		MapZoneArray[TotalNumberOfMapZones].maxx = maxx;
		MapZoneArray[TotalNumberOfMapZones].maxy = maxy;
		MapZoneArray[TotalNumberOfMapZones].maxz = maxz;
		MapZoneArray[TotalNumberOfMapZones].level = level;
		TotalNumberOfMapZones++;
		break;
	}
}

void
CTheZones::PostZoneCreation(void)
{
	int i;
	for(i = 1; i < TotalNumberOfNavigationZones; i++)
		InsertZoneIntoZoneHierarchy(&NavigationZoneArray[i]);
	InitialiseAudioZoneArray();
#ifndef MASTER
	CheckZonesForOverlap();
#endif
}

void
CTheZones::CheckZonesForOverlap(void)
{
	int i, j;
	char str[116];

	for(i = 1; i < TotalNumberOfInfoZones; i++){
		ZoneIsEntirelyContainedWithinOtherZone(&InfoZoneArray[i], &InfoZoneArray[0]);
		
		for(j = 1; j < TotalNumberOfInfoZones; j++)
			if(i != j && ZoneIsEntirelyContainedWithinOtherZone(&InfoZoneArray[i], &InfoZoneArray[j]))
				sprintf(str, "Info zone %s contains %s\n", InfoZoneArray[j].name, InfoZoneArray[i].name);
	}
}

void
CTheZones::InsertZoneIntoZoneHierarchy(CZone *zone)
{
	zone->child = nil;
	zone->parent = nil;
	zone->next = nil;
	InsertZoneIntoZoneHierRecursive(zone, &NavigationZoneArray[0]);
}

bool
CTheZones::InsertZoneIntoZoneHierRecursive(CZone *inner, CZone *outer)
{
	int n;
	CZone *child, *next, *insert;

	// return false if inner was not inserted into outer
	if(outer == nil ||
	   !ZoneIsEntirelyContainedWithinOtherZone(inner, outer))
		return false;

	// try to insert inner into children of outer
	for(child = outer->child; child; child = child->next)
		if(InsertZoneIntoZoneHierRecursive(inner, child))
			return true;

	// insert inner as child of outer
	// count number of outer's children contained within inner
	n = 0;
	for(child = outer->child; child; child = child->next)
		if(ZoneIsEntirelyContainedWithinOtherZone(child, inner))
			n++;
	inner->next = outer->child;
	inner->parent = outer;
	outer->child = inner;
	// move children from outer to inner
	if(n){
		insert = inner;
		for(child = inner->next; child; child = next){
			next = child->next;
			if(ZoneIsEntirelyContainedWithinOtherZone(child,inner)){
				insert->next = child->next;
				child->parent = inner;
				child->next = inner->child;
				inner->child = child;
			}else
				insert = child;
		}
	}

	return true;
}

bool
CTheZones::ZoneIsEntirelyContainedWithinOtherZone(CZone *inner, CZone *outer)
{
	char tmp[100];

	if(inner->minx < outer->minx ||
	   inner->maxx > outer->maxx ||
	   inner->miny < outer->miny ||
	   inner->maxy > outer->maxy ||
	   inner->minz < outer->minz ||
	   inner->maxz > outer->maxz){
		CVector vmin(inner->minx, inner->miny, inner->minz);
		if(PointLiesWithinZone(&vmin, outer))
			sprintf(tmp, "Overlapping zones %s and %s\n",
			      inner->name, outer->name);
		CVector vmax(inner->maxx, inner->maxy, inner->maxz);
		if(PointLiesWithinZone(&vmax, outer))
			sprintf(tmp, "Overlapping zones %s and %s\n",
			      inner->name, outer->name);
		return false;
	}
	return true;
}

bool
CTheZones::PointLiesWithinZone(const CVector *v, CZone *zone)
{
	return zone->minx <= v->x && v->x <= zone->maxx &&
	       zone->miny <= v->y && v->y <= zone->maxy &&
	       zone->minz <= v->z && v->z <= zone->maxz;
}

eLevelName
CTheZones::GetLevelFromPosition(CVector const *v)
{
	int i;
//	char tmp[116];
//	if(!PointLiesWithinZone(v, &MapZoneArray[0]))
//		sprintf(tmp, "x = %.3f y= %.3f z = %.3f\n", v.x, v.y, v.z);
	for(i = 1; i < TotalNumberOfMapZones; i++)
		if(PointLiesWithinZone(v, &MapZoneArray[i]))
			return MapZoneArray[i].level;
	return MapZoneArray[0].level;
}

CZone*
CTheZones::FindInformationZoneForPosition(const CVector *v)
{
	int i;
//	char tmp[116];
//	if(!PointLiesWithinZone(v, &InfoZoneArray[0]))
//		sprintf(tmp, "x = %.3f y= %.3f z = %.3f\n", v.x, v.y, v.z);
	for(i = 1; i < TotalNumberOfInfoZones; i++)
		if(PointLiesWithinZone(v, &InfoZoneArray[i]))
			return &InfoZoneArray[i];
	return &InfoZoneArray[0];
}

CZone*
CTheZones::FindSmallestNavigationZoneForPosition(const CVector *v, bool findDefault, bool findNavig)
{
	CZone *best = nil;
	if(findDefault && NavigationZoneArray[0].type == ZONE_DEFAULT ||
	   findNavig && NavigationZoneArray[0].type == ZONE_NAVIG)
		best = &NavigationZoneArray[0];
	// zone to test next
	CZone *zone = NavigationZoneArray[0].child;
	while(zone)
		// if in zone, descent into children
		if(PointLiesWithinZone(v, zone)){
			if(findDefault && zone->type == ZONE_DEFAULT ||
			   findNavig && zone->type == ZONE_NAVIG)
				best = zone;
			zone = zone->child;
		// otherwise try next zone
		}else
			zone = zone->next;
	return best;
}

int16
CTheZones::FindZoneByLabelAndReturnIndex(char *name, eZoneType type)
{
	char str[8];
	memset(str, 0, 8);
	strncpy(str, name, 8);
	switch(type){
	case ZONE_DEFAULT:
	case ZONE_NAVIG:
		for(FindIndex = 0; FindIndex < TotalNumberOfNavigationZones; FindIndex++)
			if(strcmp(GetNavigationZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;

	case ZONE_INFO:
		for(FindIndex = 0; FindIndex < TotalNumberOfInfoZones; FindIndex++)
			if(strcmp(GetInfoZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;

	case ZONE_MAPZONE:
		for(FindIndex = 0; FindIndex < TotalNumberOfMapZones; FindIndex++)
			if(strcmp(GetMapZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;
	}
	return -1;
}

int16
CTheZones::FindNextZoneByLabelAndReturnIndex(char *name, eZoneType type)
{
	char str[8];
	++FindIndex;
	memset(str, 0, 8);
	strncpy(str, name, 8);
	switch(type){
	case ZONE_DEFAULT:
	case ZONE_NAVIG:
		for(; FindIndex < TotalNumberOfNavigationZones; FindIndex++)
			if(strcmp(GetNavigationZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;

	case ZONE_INFO:
		for(; FindIndex < TotalNumberOfInfoZones; FindIndex++)
			if(strcmp(GetInfoZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;

	case ZONE_MAPZONE:
		for(; FindIndex < TotalNumberOfMapZones; FindIndex++)
			if(strcmp(GetMapZone(FindIndex)->name, name) == 0)
				return FindIndex;
		break;
	}
	return -1;
}

int16
CTheZones::FindAudioZone(CVector *pos)
{
	int i;

	for(i = 0; i < NumberOfAudioZones; i++)
		if(PointLiesWithinZone(pos, GetAudioZone(i)))
			return i;
	return -1;
}

void
CTheZones::AddZoneToAudioZoneArray(CZone *zone)
{
	int i, z;

	if(zone->type != ZONE_DEFAULT)
		return;

	/* This is a bit stupid */
	z = -1;
	for(i = 0; i < NUMNAVIGZONES; i++)
		if(&NavigationZoneArray[i] == zone)
			z = i;
	assert(NumberOfAudioZones < NUMAUDIOZONES);
	AudioZoneArray[NumberOfAudioZones++] = z;
}

void
CTheZones::InitialiseAudioZoneArray(void)
{
	bool gonext;
	CZone *zone;

	gonext = false;
	zone = &NavigationZoneArray[0];
	// Go deep first,
	// set gonext when backing up a level to visit the next child
	while(zone)
		if(gonext){
			AddZoneToAudioZoneArray(zone);
			if(zone->next){
				gonext = false;
				zone = zone->next;
			}else
				zone = zone->parent;
		}else if(zone->child)
			zone = zone->child;
		else{
			AddZoneToAudioZoneArray(zone);
			if(zone->next)
				zone = zone->next;
			else{
				gonext = true;
				zone = zone->parent;
			}
		}
}
