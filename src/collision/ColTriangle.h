#ifndef __GTA_COLTRIANGLE_H__
#define __GTA_COLTRIANGLE_H__

#include "CompressedVector.h"

enum Direction {
	DIR_X_POS,
	DIR_X_NEG,
	DIR_Y_POS,
	DIR_Y_NEG,
	DIR_Z_POS,
	DIR_Z_NEG,
};

struct CColTriangle
{
	uint16 a;
	uint16 b;
	uint16 c;
	uint8 material;
	uint8 surface;

	void Set(int a, int b, int c, uint8 material, uint8 surface)
	{
		this->a = a;
		this->b = b;
		this->c = c;
		this->material = material;
		this->surface = surface;
	}
	void Set(int a, int b, int c, uint8 surf)
	{
		this->a = a;
		this->b = b;
		this->c = c;
		this->surface = surf;
	}
};

struct CColTrianglePlane
{
	CVector normal;
	float dist;
	uint8 dir;

	void Set(const CVector &va, const CVector &vb, const CVector &vc);
	void Set(const CompressedVector *v, CColTriangle &tri) { Set(v[tri.a].Get(), v[tri.b].Get(), v[tri.c].Get()); }
	void GetNormal(CVector &n) const { n = normal; }
	float GetNormalX() const { return normal.x; }
	float GetNormalY() const { return normal.y; }
	float GetNormalZ() const { return normal.z; }
	float CalcPoint(const CVector &v) const { return DotProduct(normal, v) - dist; };
};

#endif // __GTA_COLTRIANGLE_H__
