#include "common.h"
#include "ColTriangle.h"

void
CColTrianglePlane::Set(const CVector &va, const CVector &vb, const CVector &vc)
{
	normal = CrossProduct(vc-va, vb-va);
	normal.Normalise();
	dist = DotProduct(normal, va);
	CVector an(Abs(normal.x), Abs(normal.y), Abs(normal.z));
	// find out largest component and its direction
	if(an.x > an.y && an.x > an.z)
		dir = normal.x < 0.0f ? DIR_X_NEG : DIR_X_POS;
	else if(an.y > an.z)
		dir = normal.y < 0.0f ? DIR_Y_NEG : DIR_Y_POS;
	else
		dir = normal.z < 0.0f ? DIR_Z_NEG : DIR_Z_POS;
}
