// saphys_test.cpp - checks the SA vehicle suspension setup against the engine's
// own collision code.
//
// It builds a CAutomobile for a real SA model (which runs the same
// SetupSuspensionLines the game runs when the car is created) and then uses
// CCollision::ProcessColModels - the very function that fills
// m_aSuspensionSpringRatio in the game - with the car over a flat ground
// triangle. For a range of ride heights it prints the raw ratios the physics
// would get and the resulting spring force balance, so the height at which the
// car comes to rest can be read off directly.
//
// The model's own lowest point (its tyres) must end up exactly on the ground.
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
#include "TxdStore.h"
#include "TempColModels.h"
#include "RpAnimBlend.h"
#include <rpmatfx.h>

using namespace rw;

static int32 gModelId = 1000;

// the same sequence the game uses (src/core/main.cpp PluginAttach) plus the
// stores a model needs
static bool
InitEngine(void)
{
	if(!Engine::init()){ printf("rw init failed\n"); return false; }
	if(!Engine::open(nil)){ printf("rw open failed\n"); return false; }
	if(!Engine::start()){ printf("rw start failed\n"); return false; }
	RpWorldPluginAttach();
	RpSkinPluginAttach();
	RpHAnimPluginAttach();
	NodeNamePluginAttach();
	CVisibilityPlugins::PluginAttach();
	RpAnimBlendPluginAttach();
	RpMatFXPluginAttach();
	CPools::Initialise();
	CCollision::Init();
	CTxdStore::Initialise();
	CTempColModels::Initialise();
	CModelInfo::Initialise();
	// no game files here: the handling array is a zeroed global, the test sets the fields it needs
	return true;
}

static CVehicleModelInfo*
LoadVehicle(const char *path, float ideWheelScale, int wheelModelId, int handlingId)
{
	CVehicleModelInfo *mi = CModelInfo::AddVehicleModel(gModelId);
	mi->SetModelName("admiral");
	mi->m_vehicleType = VEHICLE_TYPE_CAR;
	mi->m_wheelScale = ideWheelScale;
	mi->m_wheelId = (int16)wheelModelId;
	mi->m_handlingId = (int16)handlingId;
	if(CTxdStore::FindTxdSlot("admiral") == -1)
		CTxdStore::AddTxdSlot("admiral");
	mi->SetTexDictionary("admiral");

	StreamFile sf;
	if(!sf.open(path, "rb")){ printf("cannot open %s\n", path); return nil; }
	if(!CFileLoader::StartLoadClumpFile(&sf, gModelId)){ printf("part 1 failed\n"); return nil; }
	if(!CFileLoader::FinishLoadClumpFile(&sf, gModelId)){ printf("part 2 failed\n"); return nil; }
	return mi;
}

// a big flat triangle at z = 0, i.e. a road
static CColModel gGround;
static CompressedVector gGroundVerts[3];
static CColTriangle gGroundTri;

static void
MakeGround(void)
{
	// a big triangle that covers the whole car (also the left wheels)
	gGroundVerts[0].Set(-100.0f, -100.0f, 0.0f);
	gGroundVerts[1].Set( 100.0f, -100.0f, 0.0f);
	gGroundVerts[2].Set(   0.0f,  100.0f, 0.0f);
	gGroundTri.Set(0, 1, 2, SURFACE_TARMAC);
	gGround.numSpheres = 0;
	gGround.spheres = nil;
	gGround.numBoxes = 0;
	gGround.boxes = nil;
	gGround.numLines = 0;
	gGround.lines = nil;
	gGround.numTriangles = 1;
	gGround.triangles = &gGroundTri;
	gGround.vertices = gGroundVerts;
	gGround.ownsCollisionVolumes = false;	// the data is on the stack here
	gGround.level = LEVEL_GENERIC;
	gGround.boundingBox.Set(CVector(-100.0f, -100.0f, -1.0f), CVector(100.0f, 100.0f, 0.0f));
	gGround.boundingSphere.Set(200.0f, CVector(0.0f, 0.0f, -1.0f));
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/home/user/admiral_extracted/admiral.dff";
	float ideWheelScale = argc > 2 ? (float)atof(argv[2]) : 0.7f;
	float upper = argc > 3 ? (float)atof(argv[3]) : 0.20f;
	float lower = argc > 4 ? (float)atof(argv[4]) : -0.20f;
	float forceLevel = argc > 5 ? (float)atof(argv[5]) : 1.25f;

	setvbuf(stdout, nil, _IONBF, 0);
	if(!InitEngine()) return 1;

	// a plausible handling row
	tHandlingData *h = mod_HandlingManager.GetHandlingData((tVehicleType)0);
	h->fSuspensionForceLevel = forceLevel;
	h->fSuspensionDampingLevel = 0.15f;
	h->fSuspensionUpperLimit = upper;
	h->fSuspensionLowerLimit = lower;
	h->fSuspensionBias = 0.5f;
	h->fMass = 1700.0f;
	h->fInvMass = 1.0f/1700.0f;
	h->fTurnMass = 4000.0f;
	h->fBuoyancy = 1.0f;
	h->fTractionMultiplier = 0.7f;
	h->fTractionLoss = 0.7f;
	h->fTractionBias = 0.5f;
	h->nPercentSubmerged = 70;
	h->fCollisionDamageMultiplier = 1.0f;
	h->CentreOfMass = CVector(0.0f, 0.0f, -0.1f);

	CVehicleModelInfo *mi = LoadVehicle(path, ideWheelScale, -1, 0);
	if(mi == nil) return 2;

	printf("\n== model ==\n");
	printf("saMesh %d  m_wheelScale %.4f (radius %.4f)  embedded col %d\n",
		mi->HasOwnWheelMeshes(), mi->m_wheelScale, 0.5f*mi->m_wheelScale,
		mi->HasEmbeddedColModel());
	CColModel *modelCol = mi->GetColModel();
	if(modelCol)
		printf("col volumes: %d spheres, %d boxes, %d triangles\n",
			modelCol->numSpheres, modelCol->numBoxes, modelCol->numTriangles);

	// now create the car the way the game does; this runs SetupSuspensionLines
	CAutomobile *car = new CAutomobile(gModelId, 1);
	CColModel *carCol = car->GetColModel();
	printf("\n== suspension (from the engine's SetupSuspensionLines) ==\n");
	printf("m_fHeightAboveRoad = %.4f\n", car->m_fHeightAboveRoad);
	printf("model lowest point (tyre) = %.4f  (origin must be %+.4f above the road)\n",
		-0.5f*mi->m_wheelScale - car->m_aWheelPosition[0] + car->m_aWheelPosition[0] - 0.5f*mi->m_wheelScale,
		0.0f);
	for(int i = 0; i < 4; i++)
		printf("  wheel %d: m_aWheelPosition %.4f   line z %.4f .. %.4f (len %.4f)  springLen %.4f\n",
			i, car->m_aWheelPosition[i], carCol->lines[i].p0.z, carCol->lines[i].p1.z,
			car->m_aSuspensionLineLength[i], car->m_aSuspensionSpringLength[i]);

	// where does the mesh actually end? (frame applied bounding box was measured
	// with the geodump tool, here we just report the physics line's lower end)
	float tyreBottom = car->m_aWheelPosition[0] - 0.5f*mi->m_wheelScale;
	printf("  physics: wheel centre %.4f - radius %.4f = tyre bottom %.4f\n",
		car->m_aWheelPosition[0], 0.5f*mi->m_wheelScale, tyreBottom);

	// ---- the actual experiment: what ratios does the physics get? ----
	MakeGround();
	CMatrix carMat;
	carMat.SetUnity();
	CMatrix groundMat;
	groundMat.SetUnity();

	printf("\n== spring balance over a flat road (engine collision code) ==\n");
	printf("  z0     raw ratios (lf lb rb rf)         compression   F*sum(compression)\n");
	float best = -1.0f, bestErr = 1e9f;
	for(float z0 = 0.06f; z0 <= 0.401f; z0 += 0.005f){
		float ratios[4];
		CColPoint spherePoints[32];
		CColPoint linePoints[4];
		for(int i = 0; i < 4; i++)
			ratios[i] = 1.0f;
		carMat.GetPosition() = CVector(0.0f, 0.0f, z0);
		int bodyHits = CCollision::ProcessColModels(carMat, *carCol, groundMat, gGround,
			spherePoints, linePoints, ratios);

		float rFrac = 1.0f - car->m_aSuspensionSpringLength[0]/car->m_aSuspensionLineLength[0];
		float sumCompression = 0.0f;
		printf("  %5.3f  ", z0);
		for(int i = 0; i < 4; i++){
			float ratio = (ratios[i] - rFrac)/(1.0f - rFrac);
			if(ratio < 0.0f) ratio = 0.0f;
			if(ratio > 1.0f) ratio = 1.0f;
			sumCompression += 1.0f - ratio;
			printf(" %5.3f", ratios[i]);
		}
		printf("      ");
		for(int i = 0; i < 4; i++){
			float ratio = (ratios[i] - rFrac)/(1.0f - rFrac);
			if(ratio < 0.0f) ratio = 0.0f;
			if(ratio > 1.0f) ratio = 1.0f;
			printf(" %5.3f", 1.0f - ratio);
		}
		float balance = forceLevel*sumCompression;
		printf("   %.4f   body volumes touching the road: %d%s\n", balance, bodyHits,
			(fabs(balance - 1.0f) < 0.03f) ? "   <- resting here" : "");
		if(fabs(balance - 1.0f) < bestErr){ bestErr = fabs(balance - 1.0f); best = z0; }
	}
	printf("\nresting height (balance of weight and springs): %.4f\n", best);
	printf("model says the car must be %.4f above the road\n", car->m_fHeightAboveRoad);
	return 0;
}
