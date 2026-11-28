#pragma once

#include "Task.h"

class CPed;

// SA CTaskSimple: a leaf task. ProcessPed runs it for one tick and reports
// whether it has finished; the task manager then asks the parent complex
// task for the next sub-task.
class CTaskSimple : public CTask
{
public:
	CTaskSimple() {}
	~CTaskSimple() {}

	CTask *GetSubTask() const { return nil; }
	bool IsSimple() const { return true; }

	/*!
	 * @brief Process this task
	 * @param ped The ped this task belongs to
	 * @return true if the task has finished, false otherwise
	 */
	virtual bool ProcessPed(CPed *ped) = 0;

	/*!
	 * @brief Set the ped's position (used while the task tree settles)
	 * @return If the ped's position was changed
	 */
	virtual bool SetPedPosition(CPed *ped) { return false; }
};

VALIDATE_SIZE(CTaskSimple, 0x8);
