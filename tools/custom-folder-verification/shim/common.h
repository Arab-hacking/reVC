// minimal stand-in for reVC's src/core/common.h, so the custom archive reader
// can be built and tested outside of the game
#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
#define nil NULL

#include <math.h>

// stand-ins for RenderWare types the custom-folder headers mention
typedef void RwTexDictionary;

// minimal stand-in for the game's vector, used by the timecycle header
struct CVector
{
	float x, y, z;
	CVector(void) : x(0.0f), y(0.0f), z(0.0f) {}
	CVector(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
	CVector &operator+=(const CVector &o) { x += o.x; y += o.y; z += o.z; return *this; }
	CVector &operator-=(const CVector &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
	float Magnitude(void) const { return sqrtf(x*x + y*y + z*z); }
};
#endif
#define debug(...) do { } while(0)
#define Const const
