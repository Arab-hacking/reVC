// SA CVehicleRecording (VehicleRecording.cpp), ported from gta-reversed.
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// The tricky part of this system is that two different index spaces are in play
// and both are called an "index" in SA's own code:
//
//   recordId   - a slot in StreamingArray, i.e. which recording is loaded
//   playbackId - a slot in the sixteen playback arrays, i.e. which car is
//                replaying something right now
//
// A recording is named by the number in its file name, a playback by its slot,
// and PlayBackStreamingIndex bridges the two. Every "WithIndex" function here
// takes a playbackId.
#include "common.h"

#include "VehicleRecording.h"

#include "AutoPilot.h"
#include "CarCtrl.h"
#include "Entity.h"
#include "FileMgr.h"
#include "General.h"
#include "MemoryMgr.h"
#include "ModelInfo.h"
#include "Replay.h"
#include "Streaming.h"
#include "Timer.h"
#include "Train.h"
#include "Vehicle.h"
#include "World.h"

// The scale factors SA's recordings use. They are powers of two plus a half in
// the original - 16383.5 is 0x3FFF.8 - which is why a velocity is stored as a
// signed 16-bit value: the encoder kept one bit of the fraction.
const float CVehicleStateEachFrame::VELOCITY_SCALE = 16383.5f;
const float CVehicleStateEachFrame::DIRECTION_SCALE = 127.0f;
const float CVehicleStateEachFrame::STEERING_SCALE = 20.0f;
const float CVehicleStateEachFrame::PEDAL_SCALE = 100.0f;

int32 CVehicleRecording::NumPlayBackFiles;
CPath CVehicleRecording::StreamingArray[TOTAL_RRR_MODEL_IDS];

CVehicle *CVehicleRecording::pVehicleForPlayback[TOTAL_VEHICLE_RECORDS];
CVehicleStateEachFrame *CVehicleRecording::pPlaybackBuffer[TOTAL_VEHICLE_RECORDS];
int32 CVehicleRecording::PlaybackIndex[TOTAL_VEHICLE_RECORDS];
int32 CVehicleRecording::PlaybackBufferSize[TOTAL_VEHICLE_RECORDS];
float CVehicleRecording::PlaybackRunningTime[TOTAL_VEHICLE_RECORDS];
float CVehicleRecording::PlaybackSpeed[TOTAL_VEHICLE_RECORDS];
bool CVehicleRecording::bPlaybackGoingOn[TOTAL_VEHICLE_RECORDS];
bool CVehicleRecording::bPlaybackLooped[TOTAL_VEHICLE_RECORDS];
bool CVehicleRecording::bPlaybackPaused[TOTAL_VEHICLE_RECORDS];
bool CVehicleRecording::bUseCarAI[TOTAL_VEHICLE_RECORDS];
uint32 CVehicleRecording::PlayBackStreamingIndex[TOTAL_VEHICLE_RECORDS];

//
// Frame decoding. The stored values are fixed point; these are the only places
// the scales appear.
//

CVector
CVehicleStateEachFrame::GetVelocity(void) const
{
	return CVector(m_nVelocityX / VELOCITY_SCALE,
	               m_nVelocityY / VELOCITY_SCALE,
	               m_nVelocityZ / VELOCITY_SCALE);
}

CVector
CVehicleStateEachFrame::GetRight(void) const
{
	return CVector(m_nRightX / DIRECTION_SCALE,
	               m_nRightY / DIRECTION_SCALE,
	               m_nRightZ / DIRECTION_SCALE);
}

CVector
CVehicleStateEachFrame::GetTop(void) const
{
	return CVector(m_nTopX / DIRECTION_SCALE,
	               m_nTopY / DIRECTION_SCALE,
	               m_nTopZ / DIRECTION_SCALE);
}

float
CVehicleStateEachFrame::GetSteeringAngle(void) const
{
	return m_nSteeringAngle / STEERING_SCALE;
}

float
CVehicleStateEachFrame::GetGasPedal(void) const
{
	return m_nGasPedalPower / PEDAL_SCALE;
}

float
CVehicleStateEachFrame::GetBrakePedal(void) const
{
	return m_nBreakPedalPower / PEDAL_SCALE;
}

void
CVehicleStateEachFrame::SetVelocity(const CVector &v)
{
	m_nVelocityX = (int16)Clamp(v.x * VELOCITY_SCALE, -32768.0f, 32767.0f);
	m_nVelocityY = (int16)Clamp(v.y * VELOCITY_SCALE, -32768.0f, 32767.0f);
	m_nVelocityZ = (int16)Clamp(v.z * VELOCITY_SCALE, -32768.0f, 32767.0f);
}

void
CVehicleStateEachFrame::SetRight(const CVector &v)
{
	m_nRightX = (int8)Clamp(v.x * DIRECTION_SCALE, -128.0f, 127.0f);
	m_nRightY = (int8)Clamp(v.y * DIRECTION_SCALE, -128.0f, 127.0f);
	m_nRightZ = (int8)Clamp(v.z * DIRECTION_SCALE, -128.0f, 127.0f);
}

void
CVehicleStateEachFrame::SetTop(const CVector &v)
{
	m_nTopX = (int8)Clamp(v.x * DIRECTION_SCALE, -128.0f, 127.0f);
	m_nTopY = (int8)Clamp(v.y * DIRECTION_SCALE, -128.0f, 127.0f);
	m_nTopZ = (int8)Clamp(v.z * DIRECTION_SCALE, -128.0f, 127.0f);
}

void
CVehicleStateEachFrame::SetSteeringAngle(float f)
{
	m_nSteeringAngle = (uint8)Clamp(f * STEERING_SCALE, 0.0f, 255.0f);
}

void
CVehicleStateEachFrame::SetGasPedal(float f)
{
	m_nGasPedalPower = (uint8)Clamp(f * PEDAL_SCALE, 0.0f, 255.0f);
}

void
CVehicleStateEachFrame::SetBrakePedal(float f)
{
	m_nBreakPedalPower = (uint8)Clamp(f * PEDAL_SCALE, 0.0f, 255.0f);
}

//
// CPath
//

// SA CPath::AddRef, inlined into StartPlaybackRecordedCar.
void
CPath::AddRef(void)
{
	m_nRefCount++;
}

// SA CPath::RemoveRef, inlined into StopPlaybackWithIndex.
//
// The last playback to let go of a recording frees it: a recording is only kept
// in memory while something is playing it back.
void
CPath::RemoveRef(void)
{
	m_nRefCount--;
	if(m_nRefCount <= 0){
		m_nRefCount = 0;
		Remove();
	}
}

// SA CPath::Remove, inlined into several places.
void
CPath::Remove(void)
{
	if(m_pData == nil)
		return;

	MemoryMgrFree(m_pData);
	m_pData = nil;
	m_nSize = 0;
}

//
// CVehicleRecording
//

// SA CVehicleRecording::Init (0x459390).
void
CVehicleRecording::Init(void)
{
	for(int32 i = 0; i < TOTAL_VEHICLE_RECORDS; i++){
		bPlaybackGoingOn[i] = false;
		bPlaybackPaused[i] = false;
		pPlaybackBuffer[i] = nil;
		pVehicleForPlayback[i] = nil;
		PlaybackIndex[i] = 0;
		PlaybackBufferSize[i] = 0;
		PlaybackRunningTime[i] = 0.0f;
		PlaybackSpeed[i] = 1.0f;
		bPlaybackLooped[i] = false;
		bUseCarAI[i] = false;
		PlayBackStreamingIndex[i] = 0;
	}

	for(int32 i = 0; i < TOTAL_RRR_MODEL_IDS; i++){
		StreamingArray[i].m_pData = nil;
		StreamingArray[i].m_nRefCount = 0;
	}
}

// SA CVehicleRecording::InitAtStartOfGame (0x45A1B0).
void
CVehicleRecording::InitAtStartOfGame(void)
{
	// The numbers stay registered (they came from the file system at start-up),
	// only the loaded data is dropped.
	for(int32 i = 0; i < TOTAL_RRR_MODEL_IDS; i++)
		StreamingArray[i].m_pData = nil;
	Init();
}

// SA CVehicleRecording::ShutDown (0x459400).
void
CVehicleRecording::ShutDown(void)
{
	for(int32 i = 0; i < TOTAL_RRR_MODEL_IDS; i++)
		StreamingArray[i].Remove();
}

// SA CVehicleRecording::Render (0x459F70).
//
// Empty in SA as well: the debug path rendering that used it lives in the
// separate RenderLineSegment below.
void
CVehicleRecording::Render(void)
{
}

// SA CVehicleRecording::ChangeCarPlaybackToUseAI (0x45A360).
void
CVehicleRecording::ChangeCarPlaybackToUseAI(CVehicle *vehicle)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i == -1)
		return;

	bUseCarAI[i] = true;

	// The AI needs to know which mission it is on before it can follow anything.
	vehicle->AutoPilot.m_nCarMission = MISSION_FOLLOW_RECORDED_PATH;
	SetRecordingToPointClosestToCoors(i, vehicle->GetPosition());
	// A car being driven by the AI has to collide with the world again.
	vehicle->SetStatus(STATUS_PHYSICS);
	vehicle->bCollisionProcessed = false;
}

// SA CVehicleRecording::FindIndexWithFileNameNumber (0x459FF0).
//
// Returns 0 both when the recording is in slot 0 and when there is no such
// recording at all; the caller has to check the number itself.
uint32
CVehicleRecording::FindIndexWithFileNameNumber(int32 fileNumber)
{
	for(int32 i = 0; i < NumPlayBackFiles; i++)
		if(StreamingArray[i].m_nNumber == fileNumber)
			return (uint32)i;
	return 0;
}

CPath *
CVehicleRecording::FindRecording(int32 number)
{
	if(NumPlayBackFiles == 0)
		return nil;

	const uint32 idx = FindIndexWithFileNameNumber(number);
	// Slot 0 is also the "not found" answer, so the number has to be checked.
	if(idx != 0 || StreamingArray[idx].m_nNumber == number)
		return &StreamingArray[idx];
	return nil;
}

// SA CVehicleRecording::InterpolateInfoForCar (0x459B30).
//
// Move the car part of the way towards the next frame. Both the matrix and the
// velocity are interpolated: interpolating the matrix alone makes a car that is
// turning look like it is sliding sideways, because its velocity stays pointing
// where it was.
void
CVehicleRecording::InterpolateInfoForCar(CVehicle *vehicle, const CVehicleStateEachFrame &frame, float interpValue)
{
	CMatrix target;
	RestoreInfoForMatrix(target, frame);

	// Linear interpolation of the basis vectors, then the position. The result
	// is not orthonormal, which is why the matrix is normalised afterwards.
	const CVector r0 = vehicle->GetRight();
	const CVector f0 = vehicle->GetForward();
	const CVector p0 = vehicle->GetPosition();

	vehicle->GetRight() = CVector(r0.x + (target.GetRight().x - r0.x) * interpValue,
	                              r0.y + (target.GetRight().y - r0.y) * interpValue,
	                              r0.z + (target.GetRight().z - r0.z) * interpValue);
	vehicle->GetForward() = CVector(f0.x + (target.GetForward().x - f0.x) * interpValue,
	                                f0.y + (target.GetForward().y - f0.y) * interpValue,
	                                f0.z + (target.GetForward().z - f0.z) * interpValue);
	vehicle->GetUp() = CrossProduct(vehicle->GetRight(), vehicle->GetForward());
	vehicle->SetPosition(p0.x + (target.GetPosition().x - p0.x) * interpValue,
	                     p0.y + (target.GetPosition().y - p0.y) * interpValue,
	                     p0.z + (target.GetPosition().z - p0.z) * interpValue);
	vehicle->GetMatrix().UpdateRW();

	const CVector v0 = vehicle->GetMoveSpeed();
	const CVector v1 = frame.GetVelocity();
	vehicle->SetMoveSpeed(v0.x + (v1.x - v0.x) * interpValue,
	                      v0.y + (v1.y - v0.y) * interpValue,
	                      v0.z + (v1.z - v0.z) * interpValue);
}

// SA CVehicleRecording::HasRecordingFileBeenLoaded (0x45A060).
bool
CVehicleRecording::HasRecordingFileBeenLoaded(int32 fileNumber)
{
	const CPath *recording = FindRecording(fileNumber);
	return recording != nil && recording->m_pData != nil;
}

// SA CVehicleRecording::Load (0x45A8F0).
//
// Read a whole recording into its slot. The size limits are SA's: it asks the
// stream for at most 9'999'999 bytes, which is more than any real recording.
void
CVehicleRecording::Load(RwStream *stream, int32 recordId, int32 totalSize)
{
	if(stream == nil || recordId < 0 || recordId >= TOTAL_RRR_MODEL_IDS)
		return;

	void *allocated = MemoryMgrMalloc(totalSize);
	if(allocated == nil)
		return;

	StreamingArray[recordId].m_pData = (CVehicleStateEachFrame*)allocated;
	const uint32 size = RwStreamRead(stream, allocated, 9999999u);
	StreamingArray[recordId].m_nSize = (int32)Min(size, (uint32)totalSize);
	RwStreamClose(stream, nil);

	// A frame with time zero that is not the first frame is not a real frame:
	// the file's tail is padding. Trim it rather than replaying junk.
	const int32 numFrames = StreamingArray[recordId].NumFrames();
	for(int32 i = 1; i < numFrames; i++){
		if(StreamingArray[recordId].m_pData[i].m_nTime == 0){
			StreamingArray[recordId].m_nSize = i * (int32)sizeof(CVehicleStateEachFrame);
			break;
		}
	}

	SmoothRecording(recordId);
}

// Not a SA function: load a recording from a .rrr file.
//
// The file is a bare array of CVehicleStateEachFrame records with no header, so
// the size on disk is the size of the data. SA reads the same bytes through its
// streaming channels; this is the direct equivalent for a game install that
// ships the files loose.
bool
CVehicleRecording::LoadFromFile(int32 fileNumber, const char *path)
{
	if(path == nil)
		return false;

	const uint32 idx = FindIndexWithFileNameNumber(fileNumber);
	if(idx >= (uint32)NumPlayBackFiles || StreamingArray[idx].m_nNumber != fileNumber)
		return false;

	const int32 fd = CFileMgr::OpenFile(path);
	if(fd < 0)
		return false;

	// The file has no header, so there is no length to read up front. SA asks
	// its stream for a fixed ceiling and trims the result; the same ceiling is
	// used here, and the excess allocation is handed back below.
	const size_t maxBytes = 9999999u;
	void *data = MemoryMgrMalloc(maxBytes);
	if(data == nil){
		CFileMgr::CloseFile(fd);
		return false;
	}

	const size_t read = CFileMgr::Read(fd, (char*)data, maxBytes);
	CFileMgr::CloseFile(fd);

	if(read == 0){
		MemoryMgrFree(data);
		return false;
	}

	// A partial trailing record is not a frame; drop it.
	const int32 usable = (int32)(read / sizeof(CVehicleStateEachFrame)) * (int32)sizeof(CVehicleStateEachFrame);

	StreamingArray[idx].Remove();
	StreamingArray[idx].m_pData = (CVehicleStateEachFrame*)data;
	StreamingArray[idx].m_nSize = usable;

	// Same tail trim as Load: a frame with zero time that is not the first one
	// is padding, not data.
	const int32 numFrames = StreamingArray[idx].NumFrames();
	for(int32 i = 1; i < numFrames; i++){
		if(StreamingArray[idx].m_pData[i].m_nTime == 0){
			StreamingArray[idx].m_nSize = i * (int32)sizeof(CVehicleStateEachFrame);
			break;
		}
	}

	SmoothRecording((int32)idx);
	return true;
}

// SA CVehicleRecording::SmoothRecording (0x45A0F0).
//
// Recordings are sampled at whatever rate the recorder managed, so their frame
// times jitter. Replacing each interior time by the mean of its neighbours
// takes the jitter out; the first three and the last frame are left alone
// because they have no full neighbourhood.
void
CVehicleRecording::SmoothRecording(int32 recordId)
{
	CVehicleStateEachFrame *frames = StreamingArray[recordId].m_pData;
	const int32 numFrames = StreamingArray[recordId].NumFrames();

	if(frames == nil)
		return;

	for(int32 i = 4; i < numFrames; i++){
		frames[i - 1].m_nTime = (uint32)((frames[i].m_nTime + frames[i - 2].m_nTime) / 2);
	}
}

// SA CVehicleRecording::RegisterRecordingFile (0x459F80).
//
// Called when the streaming code finds a recording file. The name is
// "carrec<number>", and the number is what scripts use to refer to it.
int32
CVehicleRecording::RegisterRecordingFile(const char *name)
{
	if(name == nil || NumPlayBackFiles >= TOTAL_RRR_MODEL_IDS)
		return -1;

	int32 fileNumber = 850;	// SA's fallback when the name does not parse
	// Both spellings occur in the data files.
	if(sscanf(name, "carrec%d", &fileNumber) != 1)
		sscanf(name, "CARREC%d", &fileNumber);

	StreamingArray[NumPlayBackFiles].m_nNumber = fileNumber;
	StreamingArray[NumPlayBackFiles].m_pData = nil;
	StreamingArray[NumPlayBackFiles].m_nSize = 0;
	StreamingArray[NumPlayBackFiles].m_nRefCount = 0;
	return NumPlayBackFiles++;
}

// SA CVehicleRecording::RemoveRecordingFile (0x45A0A0).
void
CVehicleRecording::RemoveRecordingFile(int32 fileNumber)
{
	CPath *recording = FindRecording(fileNumber);
	if(recording == nil)
		return;

	// A recording that a playback is using cannot go: the playback holds a
	// pointer into it.
	if(recording->m_pData && recording->m_nRefCount == 0)
		recording->Remove();
}

// SA CVehicleRecording::RequestRecordingFile (0x45A020).
void
CVehicleRecording::RequestRecordingFile(int32 fileNumber)
{
	CPath *rec = FindRecording(fileNumber);
	if(rec == nil)
		return;

	// The recording is about to be streamed in, so the copy in memory is stale.
	rec->Remove();
}

// SA CVehicleRecording::StopPlaybackWithIndex (0x459440).
void
CVehicleRecording::StopPlaybackWithIndex(int32 playbackId)
{
	if(playbackId < 0 || playbackId >= TOTAL_VEHICLE_RECORDS)
		return;

	if(pVehicleForPlayback[playbackId]){
		pVehicleForPlayback[playbackId]->AutoPilot.m_vehicleRecordingId = -1;
		// Hand the car back to the world: a car that stops following a
		// recording must collide again, or it will sink through the road.
		pVehicleForPlayback[playbackId]->bCollisionProcessed = false;
	}

	pVehicleForPlayback[playbackId] = nil;
	pPlaybackBuffer[playbackId] = nil;
	PlaybackBufferSize[playbackId] = 0;
	bPlaybackGoingOn[playbackId] = false;

	// The recording this playback was using may now be free.
	const uint32 streamingIdx = PlayBackStreamingIndex[playbackId];
	if(streamingIdx < (uint32)TOTAL_RRR_MODEL_IDS)
		StreamingArray[streamingIdx].RemoveRef();
}

// SA CVehicleRecording::StartPlaybackRecordedCar (0x45A980).
void
CVehicleRecording::StartPlaybackRecordedCar(CVehicle *vehicle, int32 fileNumber, bool useCarAI, bool looped)
{
	if(vehicle == nil)
		return;

	// The first slot that is not playing anything.
	int32 playbackId = -1;
	for(int32 i = 0; i < TOTAL_VEHICLE_RECORDS; i++){
		if(!bPlaybackGoingOn[i]){
			playbackId = i;
			break;
		}
	}
	if(playbackId == -1){
		// All sixteen are busy. SA does not look for the oldest one, it simply
		// does nothing, and the script's car stays where it is.
		return;
	}

	const uint32 recordId = FindIndexWithFileNameNumber(fileNumber);
	if(recordId >= (uint32)NumPlayBackFiles || StreamingArray[recordId].m_pData == nil)
		return;

	pVehicleForPlayback[playbackId] = vehicle;
	vehicle->RegisterReference((CEntity**)&pVehicleForPlayback[playbackId]);
	bPlaybackLooped[playbackId] = looped;
	PlayBackStreamingIndex[playbackId] = recordId;
	pPlaybackBuffer[playbackId] = StreamingArray[recordId].m_pData;
	PlaybackBufferSize[playbackId] = StreamingArray[recordId].m_nSize;
	bUseCarAI[playbackId] = useCarAI;
	PlaybackIndex[playbackId] = 0;
	PlaybackRunningTime[playbackId] = 0.0f;
	PlaybackSpeed[playbackId] = 1.0f;
	bPlaybackGoingOn[playbackId] = true;
	bPlaybackPaused[playbackId] = false;
	StreamingArray[recordId].AddRef();

	if(useCarAI){
		vehicle->AutoPilot.m_nCarMission = MISSION_FOLLOW_RECORDED_PATH;
		SetRecordingToPointClosestToCoors(playbackId, vehicle->GetPosition());
	}else{
		// A puppet on a rail: it must not collide with anything, because it is
		// going to be moved through the world by hand every frame.
		vehicle->bCollisionProcessed = true;
	}

	vehicle->AutoPilot.m_vehicleRecordingId = playbackId;
}

// SA CVehicleRecording::StopPlaybackRecordedCar (0x45A280).
void
CVehicleRecording::StopPlaybackRecordedCar(CVehicle *vehicle)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i != -1)
		StopPlaybackWithIndex(i);
}

// SA CVehicleRecording::PausePlaybackRecordedCar (0x459740).
void
CVehicleRecording::PausePlaybackRecordedCar(CVehicle *vehicle)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i != -1)
		bPlaybackPaused[i] = true;
}

// SA CVehicleRecording::UnpausePlaybackRecordedCar (0x459850).
void
CVehicleRecording::UnpausePlaybackRecordedCar(CVehicle *vehicle)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i != -1)
		bPlaybackPaused[i] = false;
}

// SA CVehicleRecording::SetPlaybackSpeed (0x459660).
void
CVehicleRecording::SetPlaybackSpeed(CVehicle *vehicle, float speed)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i != -1)
		PlaybackSpeed[i] = speed;
}

// SA CVehicleRecording::RenderLineSegment (0x459F00). Debug only.
void
CVehicleRecording::RenderLineSegment(int32 &numVertices)
{
	// The debug ghost of a recording's path is not drawn in this fork: the
	// render buffer it used belongs to the debug overlay, which is not ported.
	// Clearing the count keeps the caller's contract - it always ends at zero.
	numVertices = 0;
}

// SA CVehicleRecording::RemoveAllRecordingsThatArentUsed (0x45A160).
void
CVehicleRecording::RemoveAllRecordingsThatArentUsed(void)
{
	for(int32 i = 0; i < NumPlayBackFiles; i++){
		CPath &recording = StreamingArray[i];
		// m_nNumber == i means the recording was never renamed by a file and is
		// a leftover slot rather than a real recording.
		if(recording.m_nNumber == i && recording.m_pData && recording.m_nRefCount == 0)
			recording.Remove();
	}
}

// SA CVehicleRecording::RestoreInfoForCar (0x459A30).
//
// Put one recorded frame onto a car: the transform, the controls, and the
// handbrake. When the playback is paused the controls are cleared as well, so a
// paused recorded car sits still instead of rolling on its last throttle.
void
CVehicleRecording::RestoreInfoForCar(CVehicle *vehicle, const CVehicleStateEachFrame &frame, bool pause)
{
	RestoreInfoForMatrix(vehicle->GetMatrix(), frame);
	vehicle->GetMatrix().UpdateRW();

	// The angular velocity is a property of the frame transition, not of the
	// frame, so it is cleared here and left to the interpolation.
	vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);

	vehicle->m_fSteerAngle = frame.GetSteeringAngle();
	vehicle->m_fGasPedal = frame.GetGasPedal();
	vehicle->m_fBrakePedal = frame.GetBrakePedal();
	vehicle->bIsHandbrakeOn = pause || frame.m_bHandbrakeUsed;

	if(pause){
		vehicle->m_fSteerAngle = 0.0f;
		vehicle->m_fGasPedal = 0.0f;
		vehicle->m_fBrakePedal = 0.0f;
		vehicle->SetMoveSpeed(0.0f, 0.0f, 0.0f);
	}
}

// SA CVehicleRecording::RestoreInfoForMatrix (0x459960).
//
// A recorded frame stores the right and forward vectors of the car's matrix,
// not a yaw angle: a car on a slope is not level, and a yaw angle cannot say
// that. Up is rebuilt as the cross product, which keeps the matrix orthonormal.
void
CVehicleRecording::RestoreInfoForMatrix(CMatrix &matrix, const CVehicleStateEachFrame &frame)
{
	matrix.GetRight() = frame.GetRight();
	matrix.GetForward() = frame.GetTop();
	matrix.GetUp() = CrossProduct(matrix.GetRight(), matrix.GetForward());
	matrix.GetPosition() = frame.m_vecPosn;
}

// SA CVehicleRecording::SaveOrRetrieveDataForThisFrame (0x45A610).
//
// One step of every running playback. The name is SA's and is misleading: this
// only retrieves; saving is the recorder's job and is not part of this class.
void
CVehicleRecording::SaveOrRetrieveDataForThisFrame(void)
{
	if(CReplay::IsPlayingBack())
		return;

	for(int32 i = 0; i < TOTAL_VEHICLE_RECORDS; i++){
		if(!bPlaybackGoingOn[i])
			continue;

		CVehicle *vehicle = pVehicleForPlayback[i];

		// A destroyed or burnt car stops following its recording.
		if(vehicle == nil || vehicle->bRenderScorched || vehicle->GetStatus() == STATUS_WRECKED){
			StopPlaybackWithIndex(i);
			continue;
		}

		// In AI mode the car drives itself; the recording only supplies the
		// target point the autopilot steers towards.
		if(bUseCarAI[i]){
			const int32 numFrames = GetPlaybackNumFrames(i);
			if(numFrames < 2){
				StopPlaybackWithIndex(i);
				continue;
			}

			const uint32 prevTime = CTimer::GetPreviousTimeInMilliseconds();
			const uint32 nowTime = CTimer::GetTimeInMilliseconds();
			PlaybackRunningTime[i] += (float)(nowTime - prevTime) * PlaybackSpeed[i] / 4.0f;

			CVehicleStateEachFrame *frames = GetPlaybackFrames(i);
			int32 current = GetPlaybackFrameIndex(i);
			while(current + 1 < numFrames && (float)frames[current + 1].m_nTime < PlaybackRunningTime[i])
				current++;

			if(current + 1 >= numFrames){
				// The car has reached the end of its route: it goes back to the
				// start if the recording loops, and otherwise keeps driving
				// where it is - the AI is not stopped, because stopping it in
				// the middle of traffic is worse than letting it carry on.
				if(bPlaybackLooped[i]){
					PlaybackRunningTime[i] = 0.0f;
					PlaybackIndex[i] = 0;
					current = 0;
				}else{
					StopPlaybackWithIndex(i);
					continue;
				}
			}

			PlaybackIndex[i] = current * (int32)sizeof(CVehicleStateEachFrame);

			// The AI is handed the next frame's position to aim at. Velocity is
			// deliberately not imposed: the whole point of this mode is that the
			// car finds its own way there.
			vehicle->AutoPilot.m_vecDestinationCoors = frames[current + 1].m_vecPosn;
			continue;
		}

		if(bPlaybackPaused[i])
			continue;

		const int32 numFrames = GetPlaybackNumFrames(i);
		if(numFrames < 2){
			StopPlaybackWithIndex(i);
			continue;
		}

		// How far into the recording this frame is. SA runs the clock at a
		// quarter of real time here, which is what makes a recorded car drive
		// at the speed the recording was made at.
		const uint32 prevTime = CTimer::GetPreviousTimeInMilliseconds();
		const uint32 nowTime = CTimer::GetTimeInMilliseconds();
		const float delta = (float)(nowTime - prevTime);
		const float step = delta * PlaybackSpeed[i] / 4.0f;
		PlaybackRunningTime[i] += step;

		CVehicleStateEachFrame *frames = GetPlaybackFrames(i);
		int32 current = GetPlaybackFrameIndex(i);
		if(current >= numFrames - 1){
			if(bPlaybackLooped[i]){
				PlaybackRunningTime[i] = 0.0f;
				PlaybackIndex[i] = 0;
				current = 0;
			}else{
				StopPlaybackRecordedCar(vehicle);
				continue;
			}
		}

		// Walk forward to the frame pair the running time falls between, then
		// back if the speed was negative and it overshot.
		while(current + 1 < numFrames && (float)frames[current + 1].m_nTime < PlaybackRunningTime[i])
			current++;
		while(current > 0 && (float)frames[current].m_nTime > PlaybackRunningTime[i])
			current--;

		PlaybackIndex[i] = current * (int32)sizeof(CVehicleStateEachFrame);

		if(current + 1 < numFrames){
			const CVehicleStateEachFrame &frameCurrent = frames[current];
			const CVehicleStateEachFrame &frameNext = frames[current + 1];

			RestoreInfoForCar(vehicle, frameCurrent, false);

			// Where between the two frames the running time falls. A pair of
			// frames with the same time gives no interval to interpolate over,
			// so the car simply sits on the later one.
			const uint32 span = frameNext.m_nTime - frameCurrent.m_nTime;
			const float interp = span == 0 ? 0.0f :
				(PlaybackRunningTime[i] - (float)frameCurrent.m_nTime) / (float)span;
			InterpolateInfoForCar(vehicle, frameNext, Min(Max(interp, 0.0f), 1.0f));

			// A carriage of a train has to be put back on its rails; on a curve
			// the recorded position drifts off the track.
			if(vehicle->IsSubTrain())
				((CTrain*)vehicle)->FindPositionOnTrackFromCoors();

			vehicle->RemoveAndAdd();
			vehicle->GetMatrix().UpdateRW();
			vehicle->UpdateRwFrame();
		}else if(bPlaybackLooped[i]){
			PlaybackRunningTime[i] = 0.0f;
			PlaybackIndex[i] = 0;
		}else{
			StopPlaybackRecordedCar(vehicle);
		}
	}
}

// SA CVehicleRecording::SetRecordingToPointClosestToCoors (0x45A1E0).
//
// Used when a car is dropped onto a recording part-way through: find the frame
// nearest to where the car is and start from there instead of from the
// beginning, so a car spawned in the middle of a route does not drive back.
void
CVehicleRecording::SetRecordingToPointClosestToCoors(int32 playbackId, CVector posn)
{
	if(playbackId < 0 || playbackId >= TOTAL_VEHICLE_RECORDS)
		return;

	CVehicleStateEachFrame *frames = GetPlaybackFrames(playbackId);
	const int32 numFrames = GetPlaybackNumFrames(playbackId);
	if(frames == nil || numFrames == 0)
		return;

	float minDist = 1000000.0f;
	int32 closest = 0;
	for(int32 i = 0; i < numFrames; i++){
		const CVector d = frames[i].m_vecPosn - posn;
		const float dist = d.MagnitudeSqr();
		if(dist < minDist){
			minDist = dist;
			closest = i;
		}
	}

	PlaybackIndex[playbackId] = closest * (int32)sizeof(CVehicleStateEachFrame);
	PlaybackRunningTime[playbackId] = (float)frames[closest].m_nTime;
}

// SA CVehicleRecording::IsPlaybackGoingOnForCar (0x4594C0).
bool
CVehicleRecording::IsPlaybackGoingOnForCar(CVehicle *vehicle)
{
	return FindVehicleRecordingIndex(vehicle) != -1;
}

// SA CVehicleRecording::IsPlaybackPausedForCar (0x4595A0).
//
// SA walks the playbacks and then always returns false, which is reproduced
// here: scripts that ask this get an answer that never changes, and that is
// what the original game gives them.
bool
CVehicleRecording::IsPlaybackPausedForCar(CVehicle *vehicle)
{
	return false;
}

// SA CVehicleRecording::SkipForwardInRecording (0x459D10). Unused in SA.
void
CVehicleRecording::SkipForwardInRecording(CVehicle *vehicle, float distance)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i == -1)
		return;

	CVehicleStateEachFrame *frames = GetPlaybackFrames(i);
	const int32 numFrames = GetPlaybackNumFrames(i);
	if(frames == nil || numFrames == 0)
		return;

	int32 index = GetPlaybackFrameIndex(i);
	float pace = 0.0f;

	// Walk forward until the accumulated distance passes the wanted one...
	while(pace < distance && index + 1 < numFrames){
		const CVector d = frames[index + 1].m_vecPosn - frames[index].m_vecPosn;
		pace += Sqrt(d.x * d.x + d.y * d.y);
		index++;
	}
	// ...then step back if that overshot, so the result never lands beyond the
	// requested distance.
	while(pace > distance && index > 1){
		const CVector d = frames[index].m_vecPosn - frames[index - 1].m_vecPosn;
		pace -= Sqrt(d.x * d.x + d.y * d.y);
		index--;
	}

	PlaybackRunningTime[i] = (float)frames[index].m_nTime;

	if(bUseCarAI[i]){
		RestoreInfoForCar(vehicle, frames[index], false);
	}
}

// SA CVehicleRecording::SkipToEndAndStopPlaybackRecordedCar (0x45A4A0). Unused.
void
CVehicleRecording::SkipToEndAndStopPlaybackRecordedCar(CVehicle *vehicle)
{
	const int32 i = FindVehicleRecordingIndex(vehicle);
	if(i == -1)
		return;

	const int32 numFrames = GetPlaybackNumFrames(i);
	if(numFrames == 0)
		return;

	// The car is put on its last recorded frame and then released, without
	// going through StopPlaybackWithIndex: SA leaves it collidable so that it
	// settles onto the ground where the recording left it.
	RestoreInfoForCar(vehicle, GetPlaybackFrames(i)[numFrames - 1], false);
	vehicle->bCollisionProcessed = false;

	pVehicleForPlayback[i] = nil;
	pPlaybackBuffer[i] = nil;
	PlaybackBufferSize[i] = 0;
	bPlaybackGoingOn[i] = false;
	vehicle->AutoPilot.m_vehicleRecordingId = -1;

	const uint32 streamingIdx = PlayBackStreamingIndex[i];
	if(streamingIdx < (uint32)TOTAL_RRR_MODEL_IDS)
		StreamingArray[streamingIdx].RemoveRef();
}

int32
CVehicleRecording::FindVehicleRecordingIndex(CVehicle *vehicle)
{
	for(int32 i = 0; i < TOTAL_VEHICLE_RECORDS; i++)
		if(bPlaybackGoingOn[i] && pVehicleForPlayback[i] == vehicle)
			return i;
	return -1;
}

CVehicleStateEachFrame *
CVehicleRecording::GetPlaybackFrames(int32 playbackId)
{
	if(playbackId < 0 || playbackId >= TOTAL_VEHICLE_RECORDS)
		return nil;
	return pPlaybackBuffer[playbackId];
}

int32
CVehicleRecording::GetPlaybackNumFrames(int32 playbackId)
{
	if(playbackId < 0 || playbackId >= TOTAL_VEHICLE_RECORDS)
		return 0;
	return PlaybackBufferSize[playbackId] / (int32)sizeof(CVehicleStateEachFrame);
}

int32
CVehicleRecording::GetPlaybackFrameIndex(int32 playbackId)
{
	if(playbackId < 0 || playbackId >= TOTAL_VEHICLE_RECORDS)
		return 0;
	return PlaybackIndex[playbackId] / (int32)sizeof(CVehicleStateEachFrame);
}
