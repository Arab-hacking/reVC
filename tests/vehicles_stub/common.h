// Minimal stand-in for src/core/common.h so HandlingMgr.cpp can be compiled and
// exercised outside the engine (no RenderWare / SDL / platform headers here).
#ifndef __STUB_COMMON_H__
#define __STUB_COMMON_H__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

typedef int8_t int8; typedef uint8_t uint8;
typedef int16_t int16; typedef uint16_t uint16;
typedef int32_t int32; typedef uint32_t uint32;
typedef int64_t int64; typedef uint64_t uint64;
typedef uint8 bool8;

#ifndef nil
#define nil nullptr
#endif

#define GRAVITY 0.008f
#define PI 3.14159265f
#define DEGTORAD(x) ((x) * PI / 180.0f)
#define SQR(x) ((x) * (x))
#define sq(x) ((x) * (x))
#include <cassert>
#include "Timer.h"

template<class T> inline T Max(T a, T b) { return a > b ? a : b; }
template<class T> inline T Min(T a, T b) { return a < b ? a : b; }
template<class T> inline T Abs(T a) { return a < 0 ? -a : a; }
// Same macro as src/core/common.h.
#define Clamp(v, low, high) ((v)<(low) ? (low) : (v)>(high) ? (high) : (v))
inline float Sin(float x) { return sinf(x); }
inline float Cos(float x) { return cosf(x); }

#define REVC_LOG_INFO(f, ...)  do { } while(0)
#define REVC_LOG_ERROR(f, ...) do { } while(0)
#define REVC_LOG_WARNING(f, ...) do { } while(0)

class CVector
{
public:
	float x, y, z;
	CVector(void) : x(0.0f), y(0.0f), z(0.0f) {}
	CVector(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
	float Magnitude(void) const { return sqrtf(x*x + y*y + z*z); }
	float MagnitudeSqr(void) const { return x*x + y*y + z*z; }
	void Normalise(void) { float m = Magnitude(); if(m > 0.0f){ x /= m; y /= m; z /= m; } }
	CVector operator*(float f) const { return CVector(x*f, y*f, z*f); }
	CVector operator+(const CVector &o) const { return CVector(x+o.x, y+o.y, z+o.z); }
	CVector operator-(const CVector &o) const { return CVector(x-o.x, y-o.y, z-o.z); }
};

// File access as used by cHandlingDataMgr::LoadHandlingData.
class CFileMgr
{
public:
	static void SetDir(const char *dir) { (void)dir; }
	static int32 LoadFile(const char *name, uint8 *buf, int32 maxSize, const char *mode);
};

#endif
