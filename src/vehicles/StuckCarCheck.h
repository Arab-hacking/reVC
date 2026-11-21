#ifndef __GTA_STUCKCARCHECK_H__
#define __GTA_STUCKCARCHECK_H__

#define STUCK_CAR_CHECK_SIZE 16

class CVehicle;

// SA tStuckCar. One entry per vehicle the script asked to watch.
//
// The Vice City structure reVC carried had six fields and could only answer
// "has this car not moved". SA's entry carries five more: whether to warp at
// all, and three separate reasons to warp (stuck, flipped, in water), plus how
// many path nodes to try when looking for somewhere to put the car back.
struct tStuckCar
{
	int32 CarHandle;
	CVector LastPos;
	int32 LastChecked;
	float StuckRadius;
	int32 CheckTime;
	bool CarStuck;
	bool WarpCar;
	bool WarpIfStuck;
	bool WarpIfFlipped;
	bool WarpIfInWater;
	int8 NumberOfNodesToCheck;
};

// SA CStuckCarCheck: the script's "watch this car and put it back on the road
// if it gets wedged" service.
class CStuckCarCheck
{
public:
	tStuckCar m_aStuckCars[STUCK_CAR_CHECK_SIZE];

public:
	void Init(void);
	void AddCarToCheck(int32 carHandle, float stuckRadius, uint32 time, bool warpCar,
		bool stuck, bool flipped, bool inWater, int8 numberOfNodesToCheck);
	// Put `vehicle` at `origin` facing `orientation` (degrees), but only if the
	// camera cannot see the spot and nothing mission-critical is standing
	// there. Returns whether the warp happened.
	bool AttemptToWarpVehicle(CVehicle *vehicle, CVector *origin, float orientation);
	void ClearStuckFlagForCar(int32 carHandle);
	bool HasCarBeenStuckForAWhile(int32 carHandle);
	bool IsCarInStuckCarArray(int32 carHandle);
	void Process(void);
	void RemoveCarFromCheck(int32 carHandle);
	void ResetArrayElement(uint16 index);
	void ResetArrayElement(tStuckCar &car);
};

#endif // __GTA_STUCKCARCHECK_H__
