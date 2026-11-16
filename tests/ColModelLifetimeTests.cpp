// Exercise real CColModel cleanup with a checked allocator and cache stub.
#include "common.h"
#include "ColModel.h"
#include "Collision.h"
#include <cstdlib>
#include <cstdio>
#include <set>

static std::set<void *> live;
static unsigned cacheUnlinks;
static void Check(bool ok, const char *why)
{
	if(!ok){ std::fprintf(stderr, "ColModelLifetimeTests: %s\n", why); std::abort(); }
}
void *RwMalloc(size_t size)
{
	void *p = std::malloc(size);
	Check(p != nullptr, "allocation failed");
	live.insert(p);
	return p;
}
void RwFree(void *p)
{
	if(!p) return;
	Check(live.erase(p) == 1, "double free / invalid allocation");
	std::free(p);
}
void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void CCollision::RemoveTrianglePlanes(CColModel *model)
{
	if(!model->planes) return;
	Check(live.count(model->planes) == 1, "cache unlinked after planes were freed");
	Check(model->numTriangles == 1, "triangle count reset before cache unlink");
	Check(model->GetLinkPtr() == reinterpret_cast<CLink<CColModel*> *>(uintptr_t(0x1234)),
		"cache link must still be readable");
	++cacheUnlinks;
	model->RemoveTrianglePlanes();
}
int main()
{
	{
		CColModel model;
		model.numTriangles = 1;
		model.spheres = (CColSphere *)RwMalloc(sizeof(CColSphere));
		model.planes = (CColTrianglePlane *)RwMalloc(2 * sizeof(CColTrianglePlane) + sizeof(void *));
		model.SetLinkPtr(reinterpret_cast<CLink<CColModel*> *>(uintptr_t(0x1234)));
		model.RemoveCollisionVolumes();
		Check(cacheUnlinks == 1 && live.empty(), "each owned allocation freed once");
		Check(model.numTriangles == 0 && !model.planes && !model.spheres, "state reset");
		model.RemoveCollisionVolumes();
		Check(cacheUnlinks == 1 && live.empty(), "repeated cleanup is harmless");
	}
	{
		CColModel borrowed;
		borrowed.ownsCollisionVolumes = false;
		borrowed.spheres = (CColSphere *)RwMalloc(sizeof(CColSphere));
		void *allocation = borrowed.spheres;
		borrowed.RemoveCollisionVolumes();
		Check(live.count(allocation) == 1, "borrowed memory must not be freed");
		RwFree(allocation);
	}
	Check(live.empty(), "no leaks");
	std::puts("ColModelLifetimeTests: ALL PASSED");
}
