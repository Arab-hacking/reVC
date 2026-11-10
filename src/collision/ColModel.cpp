#include "common.h"
#include "ColModel.h"
#include "Collision.h"
#include "Game.h"
#include "MemoryHeap.h"
#include "Pools.h"

CColModel::CColModel(void)
{
	numSpheres = 0;
	spheres = nil;
	sphereW = nil;
	numLines = 0;
	lines = nil;
	numBoxes = 0;
	boxes = nil;
	numVertices = 0;
	numTriangles = 0;
	vertices = nil;
	triangles = nil;
	planes = nil;
	tree = nil;
	vectorBegin = nil;
	vectorEnd = nil;
	vectorCap = nil;
	flags = 0;
	level = LEVEL_GENERIC;	// generic col slot
	ownsCollisionVolumes = true;
}

CColModel::~CColModel(void)
{
	RemoveCollisionVolumes();
}

void*
CColModel::operator new(size_t) throw()
{
	CColModel* node = CPools::GetColModelPool()->New();
	assert(node);
	return node;
}

void
CColModel::operator delete(void *p, size_t) throw()
{
	CPools::GetColModelPool()->Delete((CColModel*)p);
}

void
CColModel::RemoveCollisionVolumes(void)
{
	if(ownsCollisionVolumes){
		RwFree(spheres);
		RwFree(sphereW);
		RwFree(lines);
		RwFree(boxes);
		RwFree(vertices);
		RwFree(triangles);
		RwFree(planes);
		RwFree(tree);
		CCollision::RemoveTrianglePlanes(this);
	}
	numSpheres = 0;
	numLines = 0;
	numBoxes = 0;
	numVertices = 0;
	numTriangles = 0;
	spheres = nil;
	sphereW = nil;
	lines = nil;
	boxes = nil;
	vertices = nil;
	triangles = nil;
	planes = nil;
	tree = nil;
}

void
CColModel::CalculateTrianglePlanes(void)
{
	PUSH_MEMID(MEMID_COLLISION);

	// HACK: allocate space for one more element to stuff the link pointer into
	planes = (CColTrianglePlane*)RwMalloc(sizeof(CColTrianglePlane) * (numTriangles+1));
	REGISTER_MEMPTR(&planes);
	for(int i = 0; i < numTriangles; i++)
		planes[i].Set(vertices, triangles[i]);

	POP_MEMID();
}

void
CColModel::RemoveTrianglePlanes(void)
{
	RwFree(planes);
	planes = nil;
}

void
CColModel::SetLinkPtr(CLink<CColModel*> *lptr)
{
	assert(planes);
	*(CLink<CColModel*>**)ALIGNPTR(&planes[numTriangles]) = lptr;
}

CLink<CColModel*>*
CColModel::GetLinkPtr(void)
{
	assert(planes);
	return *(CLink<CColModel*>**)ALIGNPTR(&planes[numTriangles]);
}

void
CColModel::GetTrianglePoint(CVector &v, int i) const
{
	v = vertices[i].Get();
}

CColModel&
CColModel::operator=(const CColModel &other)
{
	int i;
	int numVerts;

	boundingSphere = other.boundingSphere;
	boundingBox = other.boundingBox;

	// copy spheres
	if(other.numSpheres){
		if(numSpheres != other.numSpheres){
			numSpheres = other.numSpheres;
			if(spheres)
				RwFree(spheres);
			spheres = (CColSphere*)RwMalloc(numSpheres*sizeof(CColSphere));
		}
		for(i = 0; i < numSpheres; i++)
			spheres[i] = other.spheres[i];
	}else{
		numSpheres = 0;
		if(spheres)
			RwFree(spheres);
		spheres = nil;
	}

	// copy sphereW (light spheres)
	if(other.sphereW){
		sphereW = (CColSphere*)RwMalloc(other.numSpheres*sizeof(CColSphere));
		for(i = 0; i < other.numSpheres; i++)
			sphereW[i] = other.sphereW[i];
	}else{
		sphereW = nil;
	}

	// copy lines
	if(other.numLines){
		if(numLines != other.numLines){
			numLines = other.numLines;
			if(lines)
				RwFree(lines);
			lines = (CColLine*)RwMalloc(numLines*sizeof(CColLine));
		}
		for(i = 0; i < numLines; i++)
			lines[i] = other.lines[i];
	}else{
		numLines = 0;
		if(lines)
			RwFree(lines);
		lines = nil;
	}

	// copy boxes
	if(other.numBoxes){
		if(numBoxes != other.numBoxes){
			numBoxes = other.numBoxes;
			if(boxes)
				RwFree(boxes);
			boxes = (CColBox*)RwMalloc(numBoxes*sizeof(CColBox));
		}
		for(i = 0; i < numBoxes; i++)
			boxes[i] = other.boxes[i];
	}else{
		numBoxes = 0;
		if(boxes)
			RwFree(boxes);
		boxes = nil;
	}

	// copy mesh
	if(other.numTriangles){
		// copy vertices
		numVerts = 0;
		for(i = 0; i < other.numTriangles; i++){
			if(other.triangles[i].a > numVerts)
				numVerts = other.triangles[i].a;
			if(other.triangles[i].b > numVerts)
				numVerts = other.triangles[i].b;
			if(other.triangles[i].c > numVerts)
				numVerts = other.triangles[i].c;
		}
		numVerts++;
		if(vertices)
			RwFree(vertices);
		if(numVerts){
			vertices = (CompressedVector*)RwMalloc(numVerts*sizeof(CompressedVector));
			for(i = 0; i < numVerts; i++)
				vertices[i] = other.vertices[i];
		}
		numVertices = numVerts;

		// copy triangles
		if(numTriangles != other.numTriangles){
			numTriangles = other.numTriangles;
			if(triangles)
				RwFree(triangles);
			triangles = (CColTriangle*)RwMalloc(numTriangles*sizeof(CColTriangle));
		}
		for(i = 0; i < numTriangles; i++)
			triangles[i] = other.triangles[i];

		// copy planes
		if(other.planes){
			if(planes)
				RwFree(planes);
			planes = (CColTrianglePlane*)RwMalloc(numTriangles*sizeof(CColTrianglePlane));
			for(i = 0; i < numTriangles; i++)
				planes[i] = other.planes[i];
		}else{
			planes = nil;
		}
	}else{
		numTriangles = 0;
		numVertices = 0;
		if(triangles)
			RwFree(triangles);
		triangles = nil;
		if(vertices)
			RwFree(vertices);
		vertices = nil;
		if(planes)
			RwFree(planes);
		planes = nil;
	}

	flags = other.flags;
	tree = other.tree;
	vectorBegin = other.vectorBegin;
	vectorEnd = other.vectorEnd;
	vectorCap = other.vectorCap;
	return *this;
}
