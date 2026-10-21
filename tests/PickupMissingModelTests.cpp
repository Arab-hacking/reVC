// Exercise the real pickup-model preflight used by creation/respawning after
// disabling VC DATA/MAPS IDEs. The -1 MODELINDICES entry formerly made the
// first pickup dereference a null model-info (Release D3D9 Pickups.cpp:188).
#include "common.h"
#include "Pickups.h"
#include "PickupModelValidation.h"
#include "ModelIndices.h"

#include <cstdio>
#include <cstdlib>

void re3_assert(const char *expr, const char *, unsigned int, const char *)
{
    std::fprintf(stderr, "Pickup test assertion failed: %s\n", expr);
    std::abort();
}
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}

CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE] = {};
static CBaseModelInfo *testModel;
void CModelInfo::Initialise(void) { ms_modelInfoPtrs[123] = testModel; }
CBaseModelInfo::CBaseModelInfo(ModelInfoType type) : m_type(type), m_colModel(nil) {}
void CBaseModelInfo::Shutdown(void) {}

class TestModel : public CBaseModelInfo {
public:
    explicit TestModel(ModelInfoType type) : CBaseModelInfo(type) {}
    void DeleteRwObject(void) override {}
    RwObject *CreateInstance(void) override { return nil; }
    RwObject *CreateInstance(RwMatrix *) override { return nil; }
    RwObject *GetRwObject(void) override { return nil; }
};

int main()
{
    InitModelIndices(); // all VC native pickup names start unset
    assert(MI_PICKUP_PROPERTY == -1 && MI_PICKUP_BRIBE == -1);
    assert(GetPickupModelInfo(MI_PICKUP_PROPERTY) == nil);
    assert(GetPickupModelInfo(MI_PICKUP_BRIBE) == nil);
    assert(GetPickupModelInfo(-10) == nil);
    assert(GetPickupModelInfo(123) == nil); // in range but no IDE
    assert(GetPickupModelInfo(MODELINFOSIZE) == nil);
    assert(GetPickupModelInfo(0xFFFF) == nil);

    TestModel ped(MITYPE_PED), simple(MITYPE_SIMPLE), weapon(MITYPE_WEAPON);
    testModel = &ped;
    CModelInfo::Initialise();
    assert(GetPickupModelInfo(123) == nil); // incompatible type
    testModel = &simple;
    CModelInfo::Initialise();
    assert(GetPickupModelInfo(123) == &simple);
    testModel = &weapon;
    CModelInfo::Initialise();
    assert(GetPickupModelInfo(123) == &weapon);

    CPickups::Init();
    const CVector pos(120.0f, 40.0f, 15.0f);
    assert(CPickups::GetActualPickupIndex(-1) == -1);
    assert(CPickups::GetActualPickupIndex((1 << 16) | NUMPICKUPS) == -1);
    assert(!CPickups::TestForPickupsInBubble(pos, 1.0f));
    CPickups::aPickUps[0].m_vecPos = pos;
    CPickups::aPickUps[0].m_eType = PICKUP_ONCE;
    assert(CPickups::TestForPickupsInBubble(pos, 1.0f));
    CPickups::aPickUps[0].m_eType = PICKUP_NONE;
    assert(!CPickups::TestForPickupsInBubble(pos, 1.0f));

    std::puts("Pickup model preflight: missing/out-of-range/wrong-type/valid and inactive slots passed");
}
