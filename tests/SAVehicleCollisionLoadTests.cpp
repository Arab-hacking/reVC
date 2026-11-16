// Real embedded SA collision loading, including the lifetime of parser views.
#include "common.h"
#include "FileLoader.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"
#include "ColModel.h"
#include "Collision.h"
#include "Pools.h"
#include "SACol3.h"
#include <set>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>

static void Check(bool ok, const char *why) { if(!ok) { std::fprintf(stderr, "SAVehicleCollisionLoadTests: %s\n", why); std::abort(); } }
static std::set<void*> live;
void *RwMalloc(size_t n) { void *p = std::malloc(n); Check(p != nullptr, "allocation"); live.insert(p); return p; }
void RwFree(void *p) { if(p) { Check(live.erase(p) == 1, "double free"); std::memset(p, 0xDD, 1); std::free(p); } }
void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
void CCollision::RemoveTrianglePlanes(CColModel *m) { if(m->planes) m->RemoveTrianglePlanes(); }
CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE];
static CVehicleModelInfo *testVehicle;
CVehicleModelInfo *CModelInfo::AddVehicleModel(int id) { ms_modelInfoPtrs[id] = testVehicle; return testVehicle; }
CColModelPool *CPools::ms_pColModelPool = nullptr;
static CColModelPool *testPool;
void CPools::Initialise() { ms_pColModelPool = testPool; }
CBaseModelInfo::CBaseModelInfo(ModelInfoType type) : m_type(type), m_bOwnsColModel(false), m_colModel(nullptr) { m_name[0] = 0; }
void CBaseModelInfo::Shutdown() {}
void CClumpModelInfo::DeleteRwObject() {}
RwObject *CClumpModelInfo::CreateInstance() { return nullptr; }
RwObject *CClumpModelInfo::CreateInstance(RwMatrix *) { return nullptr; }
void CClumpModelInfo::SetClump(RpClump *) {}
void CClumpModelInfo::SetAnimFile(const char *) {}
void CClumpModelInfo::ConvertAnimFileIndex() {}
CVehicleModelInfo::CVehicleModelInfo() : CClumpModelInfo(MITYPE_VEHICLE) {}
void CVehicleModelInfo::DeleteRwObject() {}
RwObject *CVehicleModelInfo::CreateInstance() { return nullptr; }
void CVehicleModelInfo::SetClump(RpClump *) {}
void CVehicleModelInfo::SetAnimFile(const char *) {}
void CVehicleModelInfo::ConvertAnimFileIndex() {}
void CVehicleModelInfo::SetEmbeddedColModel(CColModel *m) { if(m_colModel) delete m_colModel; m_colModel = m; }
static const uint8 *input;
static size_t inputSize;
RwUInt32 RwStreamRead(RwStream *, void *dst, RwUInt32 n) { if(n > inputSize) return 0; std::memcpy(dst, input, n); return n; }
static void W16(std::vector<uint8> &b, size_t at, uint16 v) { b[at] = v; b[at+1] = v >> 8; }
static void W32(std::vector<uint8> &b, size_t at, uint32 v) { for(int i = 0; i < 4; i++) b[at+i] = v >> (8*i); }
static void WF(std::vector<uint8> &b, size_t at, float f) { uint32 v; std::memcpy(&v, &f, 4); W32(b, at, v); }
int main() {
    CColModelPool pool(8, "SA collision test");
    testPool = &pool;
    CPools::Initialise();
    CVehicleModelInfo vehicle;
    testVehicle = &vehicle; CModelInfo::AddVehicleModel(100);
    const size_t spheres = 120, verts = 140, faces = 164; // six bytes of padding before faces
    std::vector<uint8> block(faces+8, 0);
    std::memcpy(block.data(), "COL3", 4); W32(block, 4, block.size()-8);
    WF(block, 0x2C, 1); WF(block, 0x30, 1); WF(block, 0x34, 1); WF(block, 0x44, 2);
    W16(block, 0x48, 1); W16(block, 0x4C, 1);
    W32(block, 0x54, spheres-4); W32(block, 0x60, verts-4); W32(block, 0x64, faces-4);
    WF(block, spheres, 0.25f); WF(block, spheres+12, 0.5f);
    W16(block, verts, (uint16)-128); W16(block, verts+6, 128); W16(block, verts+14, 128);
    W16(block, faces+2, 1); W16(block, faces+4, 2);
    SACol3 parsed;
    Check(SACol3Parse(block.data(), block.size(), parsed) && parsed.numVerts == 3, "indices, not padding, determine vertex count");
    for(size_t n = 0; n < block.size(); n++) Check(!SACol3Parse(block.data(), n, parsed), "truncated block rejected");
    // Sphere-only SA files may have zero offsets for unused vertex/face arrays.
    std::vector<uint8> sphereOnly = block;
    W16(sphereOnly, 0x4C, 0); W32(sphereOnly, 0x60, 0); W32(sphereOnly, 0x64, 0);
    Check(SACol3Parse(sphereOnly.data(), sphereOnly.size(), parsed) && parsed.numVerts == 0, "sphere-only model");
    sphereOnly[3] = '2';
    Check(SACol3Parse(sphereOnly.data(), sphereOnly.size(), parsed), "SA COL2 also accepted");
    input = block.data(); inputSize = block.size();
    for(int reload = 0; reload < 3; reload++) {
        Check(CFileLoader::LoadSAVehicleColModel(nullptr, 100, block.size()), "embedded collision loaded");
        CColModel *col = vehicle.GetColModel();
        Check(col && col->numVertices == 3 && col->numTriangles == 1 && col->numSpheres == 1, "geometry counters");
        Check(col->vertices[0].Get().x == -1 && col->spheres[0].center.x == 0.25f && col->spheres[0].radius == 0.5f, "geometry copied before temporary buffer is freed");
        Check(live.size() == 3, "temporary buffer freed, no reload leak");
    }
    CColModel *old = vehicle.GetColModel();
    W32(block, 0x60, 0xFFFFFFFFu);
    Check(!CFileLoader::LoadSAVehicleColModel(nullptr, 100, block.size()) && vehicle.GetColModel() == old, "overflowing offset rejected without replacement");
    W32(block, 0x60, verts-4); W16(block, faces+4, 0xFFFF);
    Check(!SACol3Parse(block.data(), block.size(), parsed), "invalid indices rejected");
    delete old; vehicle.SetColModel(nullptr);
    Check(live.empty(), "all allocations released");
    testPool = nullptr; CPools::Initialise();
    std::puts("SAVehicleCollisionLoadTests: ALL PASSED");
}
