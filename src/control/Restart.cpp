#include "common.h"

#include "Restart.h"
#include "Zones.h"
#include "PathFind.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "CustomWorld.h"
#endif

uint8 CRestart::OverrideHospitalLevel;
uint8 CRestart::OverridePoliceStationLevel;
bool CRestart::bFadeInAfterNextArrest;
bool CRestart::bFadeInAfterNextDeath;

bool CRestart::bOverrideRestart;
CVector CRestart::OverridePosition;
float CRestart::OverrideHeading;

CVector CRestart::HospitalRestartPoints[NUM_RESTART_POINTS];
float CRestart::HospitalRestartHeadings[NUM_RESTART_POINTS];
uint16 CRestart::NumberOfHospitalRestarts;

CVector CRestart::PoliceRestartPoints[NUM_RESTART_POINTS];
float CRestart::PoliceRestartHeadings[NUM_RESTART_POINTS];
uint16 CRestart::NumberOfPoliceRestarts;

void
CRestart::Initialise()
{
	OverridePoliceStationLevel = LEVEL_GENERIC;
	OverrideHospitalLevel = LEVEL_GENERIC;
	bFadeInAfterNextArrest = true;
	bFadeInAfterNextDeath = true;
	OverrideHeading = 0.0f;
	OverridePosition = CVector(0.0f, 0.0f, 0.0f);
	bOverrideRestart = false;
	NumberOfPoliceRestarts = 0;
	NumberOfHospitalRestarts = 0;

	for (int i = 0; i < NUM_RESTART_POINTS; i++) {
		HospitalRestartPoints[i] = CVector(0.0f, 0.0f, 0.0f);
		HospitalRestartHeadings[i] = 0.0f;
		PoliceRestartPoints[i] = CVector(0.0f, 0.0f, 0.0f);
		PoliceRestartHeadings[i] = 0.0f;
	}
}

void
CRestart::AddHospitalRestartPoint(const CVector &pos, float heading)
{
	HospitalRestartPoints[NumberOfHospitalRestarts] = pos;
	HospitalRestartHeadings[NumberOfHospitalRestarts++] = heading;
}

void
CRestart::AddPoliceRestartPoint(const CVector &pos, float heading)
{
	PoliceRestartPoints[NumberOfPoliceRestarts] = pos;
	PoliceRestartHeadings[NumberOfPoliceRestarts++] = heading;
}

void
CRestart::OverrideNextRestart(const CVector &pos, float heading)
{
	bOverrideRestart = true;
	OverridePosition = pos;
	OverrideHeading = heading;
}

void
CRestart::CancelOverrideRestart()
{
	bOverrideRestart = false;
}

void
CRestart::FindClosestHospitalRestartPoint(const CVector &pos, CVector *outPos, float *outHeading)
{
	if (bOverrideRestart) {
		*outPos = OverridePosition;
		*outHeading = OverrideHeading;
		CancelOverrideRestart();
		return;
	}

	eLevelName curlevel = CTheZones::GetLevelFromPosition(&pos);
	float fMinDist = SQR(4000.0f);
	int closestPoint = NUM_RESTART_POINTS;

	// find closest point on this level
	for (int i = 0; i < NumberOfHospitalRestarts; i++) {
		if (CTheZones::GetLevelFromPosition(&HospitalRestartPoints[i]) == (OverrideHospitalLevel != LEVEL_GENERIC ? OverrideHospitalLevel : curlevel)) {
			float dist = (pos - HospitalRestartPoints[i]).MagnitudeSqr();
			if (fMinDist >= dist) {
				fMinDist = dist;
				closestPoint = i;
			}
		}
	}

	// if we didn't find anything, find closest point on any level
	if (closestPoint == NUM_RESTART_POINTS) {
		for (int i = 0; i < NumberOfHospitalRestarts; i++) {
			float dist = (pos - HospitalRestartPoints[i]).MagnitudeSqr();
			if (fMinDist >= dist) {
				fMinDist = dist;
				closestPoint = i;
			}
		}
	}

	// if we still didn't find anything, find closest path node
	if (closestPoint == NUM_RESTART_POINTS) {
		int32 node = ThePaths.FindNodeClosestToCoors(pos, PATH_PED, 999999.9f);
		if (node >= 0 && node < ThePaths.m_numPathNodes)
			*outPos = ThePaths.m_pathNodes[node].GetPosition();
		else
			*outPos = pos;
		*outHeading = 0.0f;
		printf("Couldn't find a hospital restart zone near the player %f %f %f->%f %f %f\n", pos.x, pos.y, pos.z, outPos->x, outPos->y, outPos->z);
	} else {
		*outPos = HospitalRestartPoints[closestPoint];
		*outHeading = HospitalRestartHeadings[closestPoint];
	}
#ifdef CUSTOM_MODELS
	if (CCustomModels::ReplacesNativeWorldMap())
		CCustomWorld::ResolveRestartPosition(pos, *outPos, outHeading);
#endif
}

void
CRestart::FindClosestPoliceRestartPoint(const CVector &pos, CVector *outPos, float *outHeading)
{
	if (bOverrideRestart) {
		*outPos = OverridePosition;
		*outHeading = OverrideHeading;
		CancelOverrideRestart();
		return;
	}

	eLevelName curlevel = CTheZones::GetLevelFromPosition(&pos);
	float fMinDist = SQR(4000.0f);
	int closestPoint = NUM_RESTART_POINTS;

	// find closest point on this level
	for (int i = 0; i < NumberOfPoliceRestarts; i++) {
		if (CTheZones::GetLevelFromPosition(&PoliceRestartPoints[i]) == (OverridePoliceStationLevel != LEVEL_GENERIC ? OverridePoliceStationLevel : curlevel)) {
			float dist = (pos - PoliceRestartPoints[i]).MagnitudeSqr();
			if (fMinDist >= dist) {
				fMinDist = dist;
				closestPoint = i;
			}
		}
	}

	// if we didn't find anything, find closest point on any level
	if (closestPoint == NUM_RESTART_POINTS) {
		for (int i = 0; i < NumberOfPoliceRestarts; i++) {
			float dist = (pos - PoliceRestartPoints[i]).MagnitudeSqr();
			if (fMinDist >= dist) {
				fMinDist = dist;
				closestPoint = i;
			}
		}
	}

	// if we still didn't find anything, find closest path node
	if (closestPoint == NUM_RESTART_POINTS) {
		printf("Couldn't find a police restart zone near the player\n");
		int32 node = ThePaths.FindNodeClosestToCoors(pos, PATH_PED, 999999.9f);
		if (node >= 0 && node < ThePaths.m_numPathNodes)
			*outPos = ThePaths.m_pathNodes[node].GetPosition();
		else
			*outPos = pos;
		*outHeading = 0.0f;
	} else {
		*outPos = PoliceRestartPoints[closestPoint];
		*outHeading = PoliceRestartHeadings[closestPoint];
	}
#ifdef CUSTOM_MODELS
	if (CCustomModels::ReplacesNativeWorldMap())
		CCustomWorld::ResolveRestartPosition(pos, *outPos, outHeading);
#endif
}


