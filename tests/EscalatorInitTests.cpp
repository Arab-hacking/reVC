// Link the real VC escalator initializer without game assets. The native map
// is no longer loaded at all, so Esc_step is normally absent; the old
// AddThisOne -> GetColModel path dereferenced its missing model info.
#include "common.h"
#include "Fluff.h"
#include "Game.h"
#include "ModelIndices.h"
#include "ColModel.h"
#include "CustomModels.h"
#include "World.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
// No RW world/objects are created: an accidental step update is a test error.
void CWorld::Remove(CEntity *) { std::abort(); }
RwBool RwMatrixDestroy(RwMatrix *) { std::abort(); return false; }
CColModel::CColModel(void) {}
// The real check lives in CustomModels.cpp; the test has no archive. The native
// map is never loaded now, so the escalators are only reachable through this
// guard - report "replaced" to keep the hard-coded VC sites out.
bool CCustomModels::ReplacesNativeWorldMap(void) { return true; }
CColModel::~CColModel(void) {}

int16 MI_ESCALATORSTEP = -1;
CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE] = {};
static CBaseModelInfo *testStepModel;
void CModelInfo::Initialise(void) { ms_modelInfoPtrs[100] = testStepModel; }

// Only the metadata/colmodel getters of CBaseModelInfo are exercised here.
CBaseModelInfo::CBaseModelInfo(ModelInfoType type) : m_type(type), m_colModel(nil) {}
void CBaseModelInfo::Shutdown(void) {}

class StepModel : public CBaseModelInfo {
public:
    explicit StepModel(ModelInfoType type = MITYPE_SIMPLE) : CBaseModelInfo(type) {}
    void DeleteRwObject(void) override {}
    RwObject *CreateInstance(void) override { return nil; }
    RwObject *CreateInstance(RwMatrix *) override { return nil; }
    RwObject *GetRwObject(void) override { return nil; }
};

int main()
{
    // No Esc_step name at all: nothing is registered or updated.
    MI_ESCALATORSTEP = -1;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    // A custom step model must not revive hard-coded Vice City locations.
    MI_ESCALATORSTEP = 100;
    StepModel step;
    testStepModel = &step;
    CModelInfo::Initialise();
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    // A broken VC installation must not read ms_modelInfoPtrs[-1] or
    // dereference a model that has no collision volume.
    MI_ESCALATORSTEP = -1;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    MI_ESCALATORSTEP = 101;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    MI_ESCALATORSTEP = 100;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    // The 22 hard-coded sites were part of the native island, which is gone.
    // Even a perfectly valid Esc_step must not rebuild them.
    CColModel col;
    col.boundingBox.max.z = 0.5f;
    StepModel wrongType(MITYPE_PED);
    wrongType.SetColModel(&col);
    testStepModel = &wrongType;
    CModelInfo::Initialise();
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    step.SetColModel(&col);
    testStepModel = &step;
    CModelInfo::Initialise();
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    // Simulate a restart with the step model gone: registered VC sites disappear.
    MI_ESCALATORSTEP = -1;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    std::puts("VC escalators: never rebuilt from the native island");
}
