#ifndef __GTA_LOADEDCARGROUP_H__
#define __GTA_LOADEDCARGROUP_H__

#include "ModelIndices.h"

// SA CLoadedCarGroup, ported from gta-reversed
// (source/game_sa/LoadedCarGroup.{h,cpp}).
//
// A small fixed-capacity bag of vehicle model ids. The population code keeps
// one per car class ("normal", "poorfamily", ...), fills it with the models
// whose class the IDE declared and whose models are resident, and then asks it
// for a car to spawn. The picking is weighted by the model's frequency field,
// so a model the data marks as common really does turn up more often.
//
// SA's bag holds 23 entries; the number is what the original binary had room
// for and is what the population code relies on, so it is kept.
class CLoadedCarGroup
{
public:
	static const int32 NUM_MODELS = 23;
	// Empty slots are marked with -1, which is what SA calls MODEL_INVALID.
	static const int16 SENTINEL_VALUE_OF_UNUSED = -1;

	CLoadedCarGroup(void) { Clear(); }

	// Sort from most used to least used, so PickLeastUsedModel is cheap.
	void SortBasedOnUsage(void);

	// Weighted random pick over the group's models. A model whose ref count is
	// above 2 is skipped when bNotTooManyInTheWorld is set, so a busy street
	// does not fill up with copies of the same car.
	int16 PickRandomCar(bool bNotTooManyInTheWorld, bool bOnlyPickNormalCars);

	// The model with the fewest references, ties broken by fewest uses. Returns
	// -1 when even that model has been used more than maxTimesUsed times.
	int16 PickLeastUsedModel(int32 maxTimesUsed);

	int16 GetMember(uint32 idx) const;
	uint32 CountMembers(void) const;
	bool Empty(void) const;

	void Clear(void);
	void RemoveMember(int16 modelIndex);
	void AddMember(int16 member);

	// Iteration helper: the members are packed at the front of the array, so the
	// live range is [begin, begin + CountMembers()).
	int16 *GetAllModels(void) { return m_models; }
	const int16 *GetAllModels(void) const { return m_models; }

private:
	int16 m_models[NUM_MODELS];
};

#endif // __GTA_LOADEDCARGROUP_H__
