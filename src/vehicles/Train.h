#ifndef __GTA_TRAIN_H__
#define __GTA_TRAIN_H__

#include "Vehicle.h"
#include "Door.h"
#include "TrainNode.h"

// SA train track ids. Note these are the *track file slots*, not the VC pair:
// SA ships four tracks (the two elevated lines, the subway, and the fourth
// short section), and the enumeration order matches pTrackNodes[].
enum
{
	TRACK_ELTRAIN = 0,
	TRACK_SUBWAY,
	TRACK_ELTRAIN_2,
	TRACK_SUBWAY_2,
	NUM_TRAIN_TRACKS
};

// SA eTrainNodes (Train.h), verbatim. SA models twelve wheel nodes and the
// two bogies in addition to the doors.
enum eTrainNodes
{
	TRAIN_NODE_NONE   = 0,
	TRAIN_DOOR_LF     = 1,
	TRAIN_DOOR_RF     = 2,
	TRAIN_WHEEL_RF1   = 3,
	TRAIN_WHEEL_RF2   = 4,
	TRAIN_WHEEL_RF3   = 5,
	TRAIN_WHEEL_RB1   = 6,
	TRAIN_WHEEL_RB2   = 7,
	TRAIN_WHEEL_RB3   = 8,
	TRAIN_WHEEL_LF1   = 9,
	TRAIN_WHEEL_LF2   = 10,
	TRAIN_WHEEL_LF3   = 11,
	TRAIN_WHEEL_LB1   = 12,
	TRAIN_WHEEL_LB2   = 13,
	TRAIN_WHEEL_LB3   = 14,
	TRAIN_BOGIE_FRONT = 15,
	TRAIN_BOGIE_REAR  = 16,

	NUM_TRAIN_NODES
};

// SA eTrainPassengersGenerationState.
enum eTrainPassengersGenerationState
{
	TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE = 0,
	TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE = 1,
	TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER = 2,
	TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER = 3,
	TRAIN_PASSENGERS_GENERATION_FINISHED = 4
};

class CTrain : public CVehicle
{
public:
	// SA CTrain fields, in SA order.
	int16 m_nNodeIndex;					// index into the track node array
	float m_fTrainSpeed;				// -1.0 .. 1.0, drives the wheel spin; 1.0 derails
	float m_fCurrentRailDistance;		// distance along the track
	float m_fLength;					// length of this carriage along the rail
	float m_fTrainGas;					// 255 forward, 0 coasting, -255 reverse
	float m_fTrainBrake;				// 255 while braking

	union
	{
		struct
		{
			uint16 b01 : 1;						// initialised to 1
			uint16 bStoppedAtStation : 1;
			uint16 bPassengersCanEnterAndLeave : 1;
			uint16 bIsFrontCarriage : 1;
			uint16 bIsLastCarriage : 1;
			uint16 bMissionTrain : 1;
			uint16 bClockwiseDirection : 1;
			uint16 bStopsAtStations : 1;

			uint16 bNotOnARailRoad : 1;
			uint16 bForceSlowDown : 1;
			uint16 bIsStreakModel : 1;
		} trainFlags;
		uint16 m_nTrainFlags;
	};

	uint32 m_nTimeWhenStoppedAtStation;
	int8 m_nTrackId;
	uint32 m_nTimeWhenCreated;
	int16 m_nWagonGroup;				// which convoy this carriage belongs to
	// True when the train is far enough from the camera to skip the expensive
	// per-frame update and unload its model.
	bool m_bFarAway;
	uint8 m_nPassengersGenerationState;
	uint8 m_nNumPassengersToLeave : 4;
	uint8 m_nNumPassengersToEnter : 4;
	CPed *m_pTemporaryPassenger;
	CTrain *m_pPrevCarriage;
	CTrain *m_pNextCarriage;

	CDoor m_aDoors[6];
	RwFrame *m_aTrainNodes[NUM_TRAIN_NODES];

	// SA keeps six station positions; the two elevated lines and the subway
	// each stop at some of them.
	static CVector aStationCoors[6];

	// SA CTrain statics. The four track node arrays are owned by the class.
	static CTrainNode *pTrackNodes[NUM_TRAIN_TRACKS];
	static int32 NumTrackNodes[NUM_TRAIN_TRACKS];
	static float TotalTrackLength[NUM_TRAIN_TRACKS];
	static float StationDist[6];

	static bool bDisableRandomTrains;

	CTrain(int32 id, uint8 CreatedBy);

	// from CEntity
	void SetModelIndex(uint32 id);
	void ProcessControl(void);
	void PreRender(void);
	void Render(void);
	void SetupModelNodes(void);

	// SA per-carriage work.
	void AddPassenger(CPed *ped);
	void RemovePassenger(CPed *ped);
	void OpenTrainDoor(float state);
	uint32 FindNumCarriagesPulled(void);
	bool FindMaximumSpeedToStopAtStations(float *speed);
	bool FindSideStationIsOn(void) const;
	bool IsInTunnel(void) const;
	void FindPositionOnTrackFromCoors(void);
	void AddNearbyPedAsRandomPassenger(void);
	void RemoveRandomPassenger(void);

	// SA statics.
	static void InitTrains(void);
	static void Shutdown(void);
	static void UpdateTrains(void);
	static void ReadAndInterpretTrackFile(const char *filename, int32 trackId);
	static void FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector *outCoors);
	static void DisableRandomTrains(bool disable);
	static void RemoveOneMissionTrain(CTrain *train);
	static void ReleaseOneMissionTrain(CTrain *train);
	static void SetTrainSpeed(CTrain *train, float speed);
	static void SetTrainCruiseSpeed(CTrain *train, float speed);
	static CTrain *FindCaboose(CTrain *train);
	static CTrain *FindEngine(CTrain *train);
	static CTrain *FindCarriage(CTrain *train, uint8 carriage);
	static void FindNextStationPositionInDirection(bool clockwiseDirection, float distance, float *distanceToStation, int32 *numStations);
	static void RemoveMissionTrains(void);
	static void ReleaseMissionTrains(void);
	static void RemoveAllTrains(void);
	static int32 FindClosestTrackNode(CVector posn, int32 *outTrackId);
	static void FindNearestTrain(CVector posn, bool mustBeMainTrain);
	static void SetNewTrainPosition(CTrain *train, CVector posn);
	static bool IsNextStationAllowed(CTrain *train);
	static void SkipToNextAllowedStation(CTrain *train);
	static void CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType,
		CTrain **outFirstCarriage, CTrain **outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain);
	static void DoTrainGenerationAndRemoval(void);

private:
	void TrainHitStuff(CPtrList &list);
};

// SA globals, owned by the train module.
void MarkSurroundingEntitiesForCollisionWithTrain(CVector pos, float radius, CEntity *entity, bool bOnlyVehicles);
void ProcessTrainAnnouncements(void);
void PlayAnnouncement(uint8 arg0, uint8 arg1);

#endif // __GTA_TRAIN_H__
