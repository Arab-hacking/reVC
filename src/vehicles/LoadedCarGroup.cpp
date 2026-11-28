// SA CLoadedCarGroup, ported from gta-reversed
// (source/game_sa/LoadedCarGroup.cpp).
// Original reverse engineering: GTA Community / gta-reversed contributors.
//
// The group is a packed array of model ids with a -1 sentinel at the end. SA
// never stores a hole: RemoveMember shifts the tail down, AddMember appends at
// the first sentinel. That is what makes CountMembers a linear scan and every
// iteration a plain loop over a contiguous run.
#include "common.h"

#include "LoadedCarGroup.h"

#include "CarCtrl.h"
#include "General.h"
#include "Streaming.h"
#include "VehicleModelInfo.h"
#include "World.h"

// Not a SA function: the model info of a vehicle, or nil for an id the model
// table cannot resolve. SA indexes its array without checking; this fork's
// array has holes for models that were never loaded, so the check is here.
static CVehicleModelInfo *
GetVehicleMI(int16 model)
{
	if(model < 0 || model >= MODELINFOSIZE)
		return nil;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(model);
	if(mi == nil)
		return nil;
	return (CVehicleModelInfo*)mi;
}

// SA CLoadedCarGroup::SortBasedOnUsage (0x611E10).
//
// Insertion sort, highest usage first. SA sorts its own array with a comparator
// over m_nTimesUsed; the array is at most 23 long, so the sort is kept
// straightforward rather than pulled in from <algorithm>, which this file does
// not otherwise need.
void
CLoadedCarGroup::SortBasedOnUsage(void)
{
	const uint32 count = CountMembers();
	for(uint32 i = 1; i < count; i++){
		const int16 key = m_models[i];
		CVehicleModelInfo *keyMI = GetVehicleMI(key);
		const int32 keyTimesUsed = keyMI ? keyMI->m_nTimesUsed : 0;

		uint32 j = i;
		while(j > 0){
			CVehicleModelInfo *prevMI = GetVehicleMI(m_models[j - 1]);
			const int32 prevTimesUsed = prevMI ? prevMI->m_nTimesUsed : 0;
			if(prevTimesUsed >= keyTimesUsed)
				break;
			m_models[j] = m_models[j - 1];
			j--;
		}
		m_models[j] = key;
	}
}

// SA CLoadedCarGroup::RemoveMember (0x611BD0).
//
// The members are packed, so removing one means shifting the rest down and
// writing a sentinel at the new end - SA does the search itself and then fixes
// up the tail.
void
CLoadedCarGroup::RemoveMember(int16 modelIndex)
{
	const uint32 count = CountMembers();
	for(uint32 i = 0; i < count; i++){
		if(m_models[i] != modelIndex)
			continue;

		for(uint32 j = i + 1; j < count; j++)
			m_models[j - 1] = m_models[j];
		m_models[count - 1] = SENTINEL_VALUE_OF_UNUSED;
		return;
	}
}

// SA CLoadedCarGroup::PickRandomCar (0x611C50).
//
// Two stages. First the candidates are narrowed (either every member, or only
// the members whose IDE class is a passenger-car class), then a model is picked
// with probability proportional to its frequency field. The pick is retried up
// to ten times against the world's vetoes - a model the script suppressed, a
// model the script blocked, a model streaming is trying to phase out, or one
// that already has too many copies on the street. Ten tries then give up and
// report "no car", which SA treats as a normal outcome.
int16
CLoadedCarGroup::PickRandomCar(bool bNotTooManyInTheWorld, bool bOnlyPickNormalCars)
{
	if(Empty())
		return SENTINEL_VALUE_OF_UNUSED;

	// Build the candidate list. SA allocates a local array of the same size as
	// the group and copies the eligible members into it.
	int16 candidates[NUM_MODELS];
	uint32 numCandidates = 0;
	const uint32 count = CountMembers();

	for(uint32 i = 0; i < count; i++){
		const int16 model = m_models[i];

		if(bOnlyPickNormalCars){
			CVehicleModelInfo *mi = GetVehicleMI(model);
			if(mi == nil)
				continue;
			// A car the player can drive on a normal street: the three saloon
			// classes plus motorbikes. SA matches exactly this set.
			switch(mi->m_vehicleClass){
			case CCarCtrl::NORMAL:
			case CCarCtrl::POOR:
			case CCarCtrl::RICH:
			case CCarCtrl::MOTORBIKE:
				break;
			default:
				continue;
			}
		}

		candidates[numCandidates++] = model;
	}

	if(numCandidates == 0)
		return SENTINEL_VALUE_OF_UNUSED;

	// Total frequency, used as the weight space.
	int32 weightSum = 0;
	for(uint32 i = 0; i < numCandidates; i++){
		CVehicleModelInfo *mi = GetVehicleMI(candidates[i]);
		if(mi)
			weightSum += mi->m_frequency;
	}
	if(weightSum <= 0)
		return SENTINEL_VALUE_OF_UNUSED;

	for(int32 attempt = 0; attempt < 10; attempt++){
		// Draw a point in the weight space and walk the candidates until the
		// running total passes it.
		int32 pickedWeight = (int32)CGeneral::GetRandomNumber() % weightSum;
		int16 pickedModel = candidates[numCandidates - 1];
		for(uint32 i = 0; i < numCandidates; i++){
			CVehicleModelInfo *mi = GetVehicleMI(candidates[i]);
			if(mi == nil)
				continue;
			if(mi->m_frequency >= pickedWeight){
				pickedModel = candidates[i];
				break;
			}
			pickedWeight -= mi->m_frequency;
		}

		CVehicleModelInfo *pickedMI = GetVehicleMI(pickedModel);
		if(pickedMI == nil)
			continue;

		// The world's vetoes.
		if(pickedMI->GetRefCount() > 2 && bNotTooManyInTheWorld)
			continue;
		// A model streaming is trying to unload must not be chosen again.
		if(!CStreaming::HasModelLoaded(pickedModel))
			continue;

		return pickedModel;
	}

	return SENTINEL_VALUE_OF_UNUSED;
}

// SA CLoadedCarGroup::PickLeastUsedModel (0x611E90).
//
// What a script gets when it asks "give me a car nobody is using": the model
// with the smallest ref count, and on a tie the one that has been spawned the
// fewest times overall. If even that one has been used more than the caller
// allows, there is no acceptable car and the answer is -1.
int16
CLoadedCarGroup::PickLeastUsedModel(int32 maxTimesUsed)
{
	if(Empty())
		return SENTINEL_VALUE_OF_UNUSED;

	const uint32 count = CountMembers();

	int16 best = m_models[0];
	CVehicleModelInfo *bestMI = GetVehicleMI(best);
	if(bestMI == nil)
		return SENTINEL_VALUE_OF_UNUSED;

	for(uint32 i = 1; i < count; i++){
		CVehicleModelInfo *mi = GetVehicleMI(m_models[i]);
		if(mi == nil)
			continue;

		// Primary criterion is the world's reference count, the tie-breaker is
		// how often the population code has already spawned this model.
		if(mi->GetRefCount() < bestMI->GetRefCount() ||
		   (mi->GetRefCount() == bestMI->GetRefCount() && mi->m_nTimesUsed < bestMI->m_nTimesUsed)){
			best = m_models[i];
			bestMI = mi;
		}
	}

	if(bestMI->m_nTimesUsed <= maxTimesUsed)
		return best;

	return SENTINEL_VALUE_OF_UNUSED;
}

// SA CLoadedCarGroup::GetMember (0x611C20).
int16
CLoadedCarGroup::GetMember(uint32 idx) const
{
	if(idx >= CountMembers())
		return SENTINEL_VALUE_OF_UNUSED;
	return m_models[idx];
}

// SA CLoadedCarGroup::CountMembers (0x611C30).
uint32
CLoadedCarGroup::CountMembers(void) const
{
	uint32 n = 0;
	while(n < (uint32)NUM_MODELS && m_models[n] != SENTINEL_VALUE_OF_UNUSED)
		n++;
	return n;
}

// Not a SA function: SA tests m_models[0] directly, which is the same test.
bool
CLoadedCarGroup::Empty(void) const
{
	return m_models[0] == SENTINEL_VALUE_OF_UNUSED;
}

// SA CLoadedCarGroup::Clear (0x611B90).
void
CLoadedCarGroup::Clear(void)
{
	for(int32 i = 0; i < NUM_MODELS; i++)
		m_models[i] = SENTINEL_VALUE_OF_UNUSED;
}

// SA CLoadedCarGroup::AddMember (0x611BB0).
//
// SA appends at the first sentinel and silently does nothing when the group is
// full - the group is a fixed-size bag, and the population code adds models it
// already knows fit.
void
CLoadedCarGroup::AddMember(int16 member)
{
	for(int32 i = 0; i < NUM_MODELS; i++){
		if(m_models[i] == SENTINEL_VALUE_OF_UNUSED){
			m_models[i] = member;
			return;
		}
	}
}
