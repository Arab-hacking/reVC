#ifndef __GTA_MATHS_H__
#define __GTA_MATHS_H__

// wrapper around float versions of functions
// in gta they are in CMaths but that makes the code rather noisy

inline float Sin(float x) { return sinf(x); }
// The inverse trig functions are fed dot products and file quaternions,
// whose rounding can land a hair outside [-1, 1]; acosf/asinf answer NaN
// there and the NaN goes straight into a matrix. Clamp to the domain.
inline float Asin(float x) { return asinf(x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x)); }
inline float Cos(float x) { return cosf(x); }
inline float Acos(float x) { return acosf(x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x)); }
inline float Tan(float x) { return tanf(x); }
inline float Atan(float x) { return atanf(x); }
inline float Atan2(float y, float x) { return atan2f(y, x); }
inline float Abs(float x) { return fabsf(x); }
inline float Sqrt(float x) { return sqrtf(x); }
inline float RecipSqrt(float x, float y) { return x/Sqrt(y); }
inline float RecipSqrt(float x) { return RecipSqrt(1.0f, x); }
inline float Pow(float x, float y) { return powf(x, y); }
inline float Floor(float x) { return floorf(x); }
inline float Ceil(float x) { return ceilf(x); }
inline bool IsFinite(float x) { return isfinite(x) != 0; }
inline bool IsFinite(double x) { return isfinite(x) != 0; }

#endif // __GTA_MATHS_H__
