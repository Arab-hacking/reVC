#include "common.h"
#include "VuVector.h"

float
CVuVector::MagnitudeSqr(void) const
{
	return x*x + y*y + z*z;
}

void TransformPoint(CVuVector &out, const CMatrix &mat, const CVuVector &in)
{
	out = mat * in;
}

void TransformPoint(CVuVector &out, const CMatrix &mat, const RwV3d &in)
{
	out = mat * in;
}

void TransformPoints(CVuVector *out, int n, const CMatrix &mat, const RwV3d *in, int stride)
{
	while(n--){
		*out = mat * *in;
		in = (RwV3d*)((uint8*)in + stride);
		out++;
	}
}

void TransformPoints(CVuVector *out, int n, const CMatrix &mat, const CVuVector *in)
{
	while(n--){
		*out = mat * *in;
		in++;
		out++;
	}
}
