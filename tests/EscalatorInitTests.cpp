// Link the real VC escalator initializer without game assets. In the
// LoadViceCityMap=0 case Esc_step is absent from native DATA/MAPS IDEs; the
// old AddThisOne -> GetColModel path dereferenced its missing model info.
#include "common.h"
#include "Fluff.h"
#include "Game.h"
#include "ModelIndices.h"
#include "ColModel.h"
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
CColModel::~CColModel(void) {}

bool CGame::bLoadViceCityMap = true;
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
    // No native map and no Esc_step name: nothing is registered or updated.
    CGame::bLoadViceCityMap = false;
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
    CGame::bLoadViceCityMap = true;
    MI_ESCALATORSTEP = -1;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    MI_ESCALATORSTEP = 101;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    MI_ESCALATORSTEP = 100;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);

    // With native map and valid step collision the original 22 VC sites
    // are registered. The fake model is not streamed or rendered.
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
    assert(CEscalators::NumEscalators == NUM_ESCALATORS);
    assert(CEscalators::GetEscalator(0).GetPosition().z > 0.0f);

    // Simulate a restart with the map disabled: registered VC sites disappear.
    CGame::bLoadViceCityMap = false;
    CEscalators::Init();
    assert(CEscalators::NumEscalators == 0);
    std::puts("VC escalators: no-map, missing model/col, wrong type and native cases passed");
}
