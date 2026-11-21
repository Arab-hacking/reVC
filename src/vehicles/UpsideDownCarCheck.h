#ifndef __GTA_UPSIDEDOWNCARCHECK_H__
#define __GTA_UPSIDEDOWNCARCHECK_H__

class CVehicle;

// SA UpsideDownCar: one watched vehicle and how long it has been on its roof.
struct UpsideDownCar
{
	int32 m_nHandle;
	uint32 m_nTime;

	UpsideDownCar(void) { Clear(); }

	void Clear(void)
	{
		m_nHandle = -1;
		m_nTime = 0;
	}
};

// SA CUpsideDownCarCheck: the "this car has been on its roof long enough to be
// worth doing something about" service.
//
// reVC carried a Vice City version of this, which decided "upside down" from a
// hard up-vector threshold plus move and turn speed tolerances. Sa does not do
// that: it asks whether the ped could step out (CanPedExitCar) and, if not,
// whether the car is short of four contact wheels or simply facing down. A car
// resting on its roof with three wheels down is not "upside down" in SA; a car
// whose occupants cannot get out is.
class CUpsideDownCarCheck
{
public:
	static const uint32 UPSIDE_DOWN_CAR_MIN_TIME = 2000;

	UpsideDownCar m_aUpsideDownCars[6];

public:
	void Init(void);
	bool IsCarUpsideDown(int32 carHandle);
	bool IsCarUpsideDown(const CVehicle *vehicle);
	bool AreAnyCarsUpsideDown(void);
	void UpdateTimers(void);
	void AddCarToCheck(int32 carHandle);
	void RemoveCarFromCheck(int32 carHandle);
	bool HasCarBeenUpsideDownForAWhile(int32 carHandle);
};

#endif // __GTA_UPSIDEDOWNCARCHECK_H__
