// Real native COLL loader, checked allocator; no game assets or GL context.
#include "common.h"
#include "FileLoader.h"
#include "ColModel.h"
#include "Collision.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

static std::set<void *> live;
static void Check(bool ok, const char *why) {
    if(!ok) { std::fprintf(stderr, "CollisionRecordLoadTests: %s\n", why); std::abort(); }
}
void *RwMalloc(size_t n) { void *p = std::malloc(n); Check(p != nullptr, "allocation"); live.insert(p); return p; }
void RwFree(void *p) { if(p) { Check(live.erase(p) == 1, "double free"); std::free(p); } }
void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
void CCollision::RemoveTrianglePlanes(CColModel *m) { if(m->planes) m->RemoveTrianglePlanes(); }
static void W32(std::vector<uint8> &b, size_t at, uint32 v) {
    for(int i = 0; i < 4; i++) b[at+i] = uint8(v >> (8*i));
}
static void WF(std::vector<uint8> &b, size_t at, float f) { uint32 v; std::memcpy(&v, &f, 4); W32(b, at, v); }
int main() {
    // >55 KB body, >127 lines (model's in-memory line count is int8),
    // >32 boxes, and float vertices outside +/-256 m must all remain intact.
    const size_t lines = 40+4, boxes = lines+4+200*24;
    const size_t vertices = boxes+4+2000*28, faces = vertices+4+3*12;
    std::vector<uint8> body(faces+4+16, 0);
    W32(body, lines, 200); W32(body, boxes, 2000); W32(body, vertices, 3); W32(body, faces, 1);
    WF(body, vertices+4, -420.0f); WF(body, vertices+16, 420.0f);
    W32(body, faces+8, 1); W32(body, faces+12, 2);
    char name[] = "wide_road";
    {
        CColModel model;
        CFileLoader::LoadCollisionModel(body.data(), body.size(), model, name);
        Check(model.numBoxes == 2000 && model.numVertices == 3 && model.numTriangles == 1, "full section counts");
        Check(model.numLines == 0 && !model.lines, "disk lines skipped without truncating their count");
        Check(model.vertices[0].Get().x == -420.0f && model.vertices[1].Get().x == 420.0f, "float vertices preserved");
        void *oldBoxes = model.boxes;
        for(size_t n = 0; n < body.size(); ++n) {
            CFileLoader::LoadCollisionModel(body.data(), n, model, name);
            Check(model.boxes == oldBoxes && model.numTriangles == 1, "truncation retains previous model");
        }
        W32(body, faces+4, 3); // out-of-range triangle index
        CFileLoader::LoadCollisionModel(body.data(), body.size(), model, name);
        Check(model.boxes == oldBoxes, "bad triangle rejected before mutation");
        W32(body, faces+4, 0);
        W32(body, vertices+4, 0x7FC00000u); // NaN must not enter physics
        CFileLoader::LoadCollisionModel(body.data(), body.size(), model, name);
        Check(model.boxes == oldBoxes, "non-finite geometry rejected");
        WF(body, vertices+4, -420.0f);
        CFileLoader::LoadCollisionModel(body.data(), body.size(), model, name);
        Check(live.size() == 3, "reload releases previous allocations");
        W32(body, boxes, 0xFFFFFFFFu);
        CFileLoader::LoadCollisionModel(body.data(), body.size(), model, name);
        Check(model.numBoxes == 2000 && live.size() == 3, "invalid count not clamped");
    }
    Check(live.empty(), "all allocations freed");
    std::puts("CollisionRecordLoadTests: ALL PASSED");
}
