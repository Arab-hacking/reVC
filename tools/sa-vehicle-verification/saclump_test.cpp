// saclump_test.cpp
//
// Runs reVC's real vehicle-model loading path on a DFF file and prints what the
// game ends up with. This is not a simulation: it calls the same functions the
// streaming system calls (CFileLoader::StartLoadClumpFile /
// FinishLoadClumpFile), so CVehicleModelInfo::SetClump, the SA wheel handling
// (CloneSAWheelMeshes) and the embedded collision hook all run for real.
//
// usage: saclump_test <file.dff> [ideWheelScale] [ideWheelModelId]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "main.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"
#include "FileLoader.h"
#include "RwHelper.h"
#include "NodeName.h"
#include "VisibilityPlugins.h"
#include "HandlingMgr.h"
#include "Pools.h"
#include "AnimBlendHierarchy.h"
#include "RpAnimBlend.h"
#include <rpmatfx.h>
#include "TxdStore.h"
#include "TempColModels.h"

using namespace rw;

static int32 gModelId = 1000;

static RwFrame *gFound;
static const char *gSearchName;

static RwFrame *FindFrameCB(RwFrame *frame, void *data)
{
	char *fn = GetFrameNodeName(frame);
	if(fn && strcasecmp(fn, gSearchName) == 0){
		gFound = frame;
		return nil;
	}
	RwFrameForAllChildren(frame, FindFrameCB, nil);
	return gFound ? nil : frame;
}

static RwFrame *FindFrameByName(RwFrame *root, const char *name)
{
	gFound = nil;
	gSearchName = name;
	FindFrameCB(root, nil);
	return gFound;
}

static int gObjCount;
static RwFrame *CountCB(RwFrame *frame, void *data)
{
	FORLIST(lnk, frame->objectList)
		gObjCount++;
	RwFrameForAllChildren(frame, CountCB, nil);
	return frame;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/home/user/admiral_extracted/admiral.dff";
	float ideWheelScale = argc > 2 ? (float)atof(argv[2]) : 0.7f;
	int wheelModelId = argc > 3 ? atoi(argv[3]) : -1;
	int handlingId = argc > 4 ? atoi(argv[4]) : 0;

	setvbuf(stdout, nil, _IONBF, 0);
	printf("step: Engine::init\n");
	if(!Engine::init()){ printf("rw engine init failed\n"); return 1; }
	printf("step: Engine::open\n");
	if(!Engine::open(nil)){ printf("rw engine open failed\n"); return 1; }
	printf("step: Engine::start\n");
	if(!Engine::start()){ printf("rw engine start failed\n"); return 1; }
	// the same plugin attach sequence the game does (main.cpp PluginAttach)
	printf("step: plugins\n");
	RpWorldPluginAttach();
	RpSkinPluginAttach();
	RpHAnimPluginAttach();
	NodeNamePluginAttach();
	CVisibilityPlugins::PluginAttach();
	RpAnimBlendPluginAttach();
	RpMatFXPluginAttach();
	printf("step: CVisibilityPlugins::Initialise\n");
	CVisibilityPlugins::Initialise();
	printf("step: CPools::Initialise\n");
	CPools::Initialise();
	printf("step: CTxdStore::Initialise\n");
	CTxdStore::Initialise();
	printf("step: CTempColModels::Initialise\n");
	CTempColModels::Initialise();
	printf("step: CModelInfo::Initialise\n");
	CModelInfo::Initialise();
	printf("step: AddVehicleModel\n");
	CVehicleModelInfo *mi = CModelInfo::AddVehicleModel(gModelId);
	mi->m_vehicleType = VEHICLE_TYPE_CAR;
	printf("step: fields\n");
	mi->SetModelName("admiral");
	mi->m_vehicleType = VEHICLE_TYPE_CAR;
	mi->m_wheelScale = ideWheelScale;
	mi->m_wheelId = (int16)wheelModelId;
	mi->m_handlingId = (int16)handlingId;
	// the IDE also gives the model a tex dictionary slot
	if(CTxdStore::FindTxdSlot("admiral") == -1)
		CTxdStore::AddTxdSlot("admiral");
	mi->SetTexDictionary("admiral");

	printf("step: open file\n");
	StreamFile sf;
	if(!sf.open(path, "rb")){
		printf("cannot open %s\n", path);
		return 2;
	}
	printf("pos before part1 = %d\n", (int)sf.tell());
	if(!CFileLoader::StartLoadClumpFile(&sf, gModelId)){
		printf("part 1 (frames) failed\n");
		return 2;
	}
	printf("pos before part2 = %d\n", (int)sf.tell());
	if(!CFileLoader::FinishLoadClumpFile(&sf, gModelId)){
		printf("part 2 (atomics) failed\n");
		return 2;
	}

	printf("pos after part2 = %d\n", (int)sf.tell());
	printf("\n== what the game ends up with ==\n");
	printf("ide wheelScale %.4f, wheel model id %d, handling id %d\n",
		ideWheelScale, wheelModelId, handlingId);
	printf("own wheel meshes (SA branch): %s   m_wheelScale %.4f  physics radius %.4f\n",
		mi->HasOwnWheelMeshes() ? "YES" : "NO", mi->m_wheelScale, 0.5f*mi->m_wheelScale);
	printf("embedded collision: %s\n", mi->HasEmbeddedColModel() ? "YES" : "NO");
	CColModel *cm = mi->GetColModel();
	if(cm)
		printf("col model: spheres %d boxes %d triangles %d lines %d  bbox z %.4f .. %.4f\n",
			cm->numSpheres, cm->numBoxes, cm->numTriangles, cm->numLines,
			cm->boundingBox.min.z, cm->boundingBox.max.z);
	else
		printf("col model: NONE\n");

	RpClump *clump = (RpClump*)mi->GetRwObject();
	RwFrame *root = RpClumpGetFrame(clump);
	printf("\nwheel dummies (name / local z / meshes in subtree):\n");
	static const char *wheelNames[4] = { "wheel_lf_dummy", "wheel_lb_dummy", "wheel_rb_dummy", "wheel_rf_dummy" };
	for(int i = 0; i < 4; i++){
		RwFrame *f = FindFrameByName(root, wheelNames[i]);
		if(f == nil){ printf("  %-16s NOT FOUND\n", wheelNames[i]); continue; }
		gObjCount = 0;
		CountCB(f, nil);
		printf("  %-16s z %+.4f  id %d  meshes %d\n", wheelNames[i],
			RwMatrixGetPos(RwFrameGetMatrix(f))->z,
			CVisibilityPlugins::GetFrameHierarchyId(f), gObjCount);
	}
	printf("total atomics in clump: %d\n", RpClumpGetNumAtomics(clump));
	return 0;
}
