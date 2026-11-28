#ifndef __GTA_VEHICLERECORDING_H__
#define __GTA_VEHICLERECORDING_H__

#include "common.h"
#include "Matrix.h"
#include "Vector.h"

class CVehicle;
class CEntity;

// SA CVehicleRecording (VehicleRecording.{h,cpp}), ported from gta-reversed.
//
// Missions in SA drive cars along hand-made paths: the ".rrr" files in the
// game's data are frame-by-frame recordings of a car's position, heading,
// velocity and control inputs, and a scripted car replays them. The system has
// two layers and they are easy to confuse:
//
//   * A *recording* is one file. It is loaded once into a fixed streaming slot
//     and shared by every car that plays it back. Its number is the number in
//     the file's name ("carrec12"), not its slot.
//   * A *playback* is one car playing one recording. There are sixteen of them,
//     so up to sixteen cars can be replaying at once, and each has its own
//     cursor, speed, pause flag and loop flag.
//
// Playback can run in two modes. In the plain mode the car is moved by hand
// every frame with collision switched off - it is a puppet on a rail. In AI
// mode the car is left to the driving AI, which is told to follow the recorded
// path (MISSION_FOLLOW_RECORDED_PATH) and only gets a position to aim for; the
// advantage is that the car reacts to the world and cannot be pushed through a
// wall by a player standing in the way.

// SA CVehicleStateEachFrame (0x20 bytes).
//
// One frame of a recording. The fields are stored far more compactly than the
// values they represent, because a recording is one line of a file per frame
// and SA's own files budget about eight of them per second of playback:
//
//   * velocity      - three signed 16-bit values, 1/16383.5 of a unit each
//   * right/up      - two signed bytes each, 1/127 of a unit
//   * steering      - one byte, 1/20 of a radian
//   * gas/brake     - one byte each, 1/100 of full pedal
//   * handbrake     - a bit
struct CVehicleStateEachFrame
{
	// Fractional fixed-point scales. A value is stored as the float times the
	// scale, rounded.
	static const float VELOCITY_SCALE;
	static const float DIRECTION_SCALE;
	static const float STEERING_SCALE;
	static const float PEDAL_SCALE;

	uint32 m_nTime;			// milliseconds since the start of the recording

	int16 m_nVelocityX, m_nVelocityY, m_nVelocityZ;
	int8 m_nRightX, m_nRightY, m_nRightZ;
	int8 m_nTopX, m_nTopY, m_nTopZ;
	uint8 m_nSteeringAngle;
	uint8 m_nGasPedalPower;
	uint8 m_nBreakPedalPower;
	bool m_bHandbrakeUsed;
	uint8 _pad0;
	uint8 _pad1;
	uint8 _pad2;
	CVector m_vecPosn;

	// The decoded values, which is what the playback code actually uses.
	CVector GetVelocity(void) const;
	CVector GetRight(void) const;
	CVector GetTop(void) const;
	float GetSteeringAngle(void) const;
	float GetGasPedal(void) const;
	float GetBrakePedal(void) const;

	// The inverse, used when a recording is written.
	void SetVelocity(const CVector &v);
	void SetRight(const CVector &v);
	void SetTop(const CVector &v);
	void SetSteeringAngle(float f);
	void SetGasPedal(float f);
	void SetBrakePedal(float f);
};

// How many playbacks can run at once.
enum { TOTAL_VEHICLE_RECORDS = 16 };
// How many recordings can be loaded at once. SA reserves 475 streaming slots
// for them; this fork keeps the same ceiling so the index arithmetic matches.
enum { TOTAL_RRR_MODEL_IDS = 475 };

// SA CPath (0x10 bytes). One loaded recording.
class CPath
{
public:
	int32 m_nNumber;			// the number in the file's name
	CVehicleStateEachFrame *m_pData;
	int32 m_nSize;				// size in bytes
	int8 m_nRefCount;			// playbacks currently using it

	void AddRef(void);
	void RemoveRef(void);

	// Number of frames, which is not the byte size.
	int32 NumFrames(void) const { return m_nSize / (int32)sizeof(CVehicleStateEachFrame); }

	// Drop the recording's memory. Its streaming slot is not touched here: the
	// caller owns the model reference (see RemoveRecordingFile).
	void Remove(void);
};

class CVehicleRecording
{
public:
	static int32 NumPlayBackFiles;
	static CPath StreamingArray[TOTAL_RRR_MODEL_IDS];

	static CVehicle *pVehicleForPlayback[TOTAL_VEHICLE_RECORDS];
	static CVehicleStateEachFrame *pPlaybackBuffer[TOTAL_VEHICLE_RECORDS];
	static int32 PlaybackIndex[TOTAL_VEHICLE_RECORDS];
	static int32 PlaybackBufferSize[TOTAL_VEHICLE_RECORDS];
	static float PlaybackRunningTime[TOTAL_VEHICLE_RECORDS];
	static float PlaybackSpeed[TOTAL_VEHICLE_RECORDS];
	static bool bPlaybackGoingOn[TOTAL_VEHICLE_RECORDS];
	static bool bPlaybackLooped[TOTAL_VEHICLE_RECORDS];
	static bool bPlaybackPaused[TOTAL_VEHICLE_RECORDS];
	static bool bUseCarAI[TOTAL_VEHICLE_RECORDS];
	// Which recording each playback is playing.
	static uint32 PlayBackStreamingIndex[TOTAL_VEHICLE_RECORDS];

	static void Init(void);
	static void InitAtStartOfGame(void);
	static void ShutDown(void);
	static void Render(void);

	// Hand a car over to the driving AI, which then follows the recording it is
	// already playing instead of being moved frame by frame.
	static void ChangeCarPlaybackToUseAI(CVehicle *vehicle);

	// The slot whose recording is the given file number, or 0 when there is no
	// such recording (SA returns 0 in both the "first slot" and "not found"
	// cases, which is why the callers check the number again).
	static uint32 FindIndexWithFileNameNumber(int32 fileNumber);

	static void InterpolateInfoForCar(CVehicle *vehicle, const CVehicleStateEachFrame &frame, float interpValue);

	static bool HasRecordingFileBeenLoaded(int32 fileNumber);
	static void Load(RwStream *stream, int32 recordId, int32 totalSize);

	// Not a SA function: read a recording straight from a .rrr file on disk and
	// put it in the slot registered for that file number. SA only ever got
	// recordings through its streaming system, which this fork's VC data has no
	// entry for; this is the path that actually works here.
	static bool LoadFromFile(int32 fileNumber, const char *path);
	static void SmoothRecording(int32 recordId);

	// Called for every "carrec<number>.rrr" the streaming code finds.
	static int32 RegisterRecordingFile(const char *name);
	static void RemoveRecordingFile(int32 fileNumber);
	static void RequestRecordingFile(int32 fileNumber);

	static void SetPlaybackSpeed(CVehicle *vehicle, float speed);
	static void RenderLineSegment(int32 &numVertices);

	static void RemoveAllRecordingsThatArentUsed(void);
	static void RestoreInfoForCar(CVehicle *vehicle, const CVehicleStateEachFrame &frame, bool pause);
	static void RestoreInfoForMatrix(CMatrix &matrix, const CVehicleStateEachFrame &frame);
	static void SaveOrRetrieveDataForThisFrame(void);
	static void SetRecordingToPointClosestToCoors(int32 playbackId, CVector posn);
	static void SkipForwardInRecording(CVehicle *vehicle, float distance);
	static void SkipToEndAndStopPlaybackRecordedCar(CVehicle *vehicle);

	static void StopPlaybackWithIndex(int32 playbackId);
	static void StartPlaybackRecordedCar(CVehicle *vehicle, int32 fileNumber, bool useCarAI, bool looped);
	static void StopPlaybackRecordedCar(CVehicle *vehicle);
	static void PausePlaybackRecordedCar(CVehicle *vehicle);
	static void UnpausePlaybackRecordedCar(CVehicle *vehicle);

	static bool IsPlaybackGoingOnForCar(CVehicle *vehicle);
	static bool IsPlaybackPausedForCar(CVehicle *vehicle);

	// The recording with this file number, or nil when it is not loaded.
	static CPath *FindRecording(int32 number);

	// Which playback slot is playing for this car, or -1.
	static int32 FindVehicleRecordingIndex(CVehicle *vehicle);

	// The frames of one playback, and where in them the cursor is.
	static CVehicleStateEachFrame *GetPlaybackFrames(int32 playbackId);
	static int32 GetPlaybackNumFrames(int32 playbackId);
	static int32 GetPlaybackFrameIndex(int32 playbackId);
};

#endif // __GTA_VEHICLERECORDING_H__
