#pragma once

#include "Quaternion.h"
#include "Vector.h"

// Fixed point number with implicit conversion to float. The stored value is
// the float value multiplied by CompressValue.
template<typename T, int CompressValue, bool UseRoundingWhenConverting = false>
class FixedFloat
{
public:
	FixedFloat() : value(0) {}
	FixedFloat(float v) { Set(v, UseRoundingWhenConverting); }
	template<typename Y>
	explicit FixedFloat(Y x) : value((T)x) {}

	operator float() const { return (float)value / (float)CompressValue; }

	void Set(float v, bool round = UseRoundingWhenConverting)
	{
		value = round ? (T)(v * (float)CompressValue + 0.5f) : (T)(v * (float)CompressValue);
	}

	// Arithmetic on the raw stored values, avoiding float round-trips.
	FixedFloat operator+(const FixedFloat &o) const { return FixedFloat((T)(value + o.value), RawTag()); }
	FixedFloat operator-(const FixedFloat &o) const { return FixedFloat((T)(value - o.value), RawTag()); }
	FixedFloat operator*(const FixedFloat &o) const { return FixedFloat((T)(value * o.value), RawTag()); }
	FixedFloat operator/(const FixedFloat &o) const { return FixedFloat((T)(value / o.value), RawTag()); }

private:
	struct RawTag {};
	FixedFloat(T v, RawTag) : value(v) {}
	T value;
};

template<typename T, int CompressValue>
struct FixedQuat
{
	FixedQuat() {}
	FixedQuat(const CQuaternion &q) : x(q.x), y(q.y), z(q.z), w(q.w) {}
	FixedQuat(T X, T Y, T Z, T W) : x(X), y(Y), z(Z), w(W) {}

	operator CQuaternion() const { return CQuaternion(x, y, z, w); }
	FixedQuat &operator=(const CQuaternion &q)
	{
		x.Set(q.x);
		y.Set(q.y);
		z.Set(q.z);
		w.Set(q.w);
		return *this;
	}

	FixedFloat<T, CompressValue> x, y, z, w;
};

template<typename T, int CompressValue>
struct FixedVector
{
	FixedVector() {}
	FixedVector(const CVector &v) : x(v.x), y(v.y), z(v.z) {}
	FixedVector(T X, T Y, T Z) : x(X), y(Y), z(Z) {}

	operator CVector() const { return CVector(x, y, z); }
	FixedVector &operator=(const CVector &v)
	{
		x.Set(v.x);
		y.Set(v.y);
		z.Set(v.z);
		return *this;
	}

	FixedFloat<T, CompressValue> x, y, z;
};

/*
 * KeyFrame -> CAnimSequenceChildFrameUncompressed
 * KeyFrameTrans -> CAnimSequenceRootFrameUncompressed
 *
 * KeyFrameCompressed -> CAnimSequenceChildFrameCompressed
 * KeyFrameTransCompressed -> CAnimSequenceRootFrameCompressed
 */
struct KeyFrame
{
	CQuaternion Rot;
	float DeltaTime; // Relative to previous key frame
};

struct KeyFrameTrans : KeyFrame
{
	CVector Trans;
};

struct KeyFrameCompressed
{
	FixedQuat<int16, 4096> Rot;
	FixedFloat<int16, 60, true> DeltaTime;
};

struct KeyFrameTransCompressed : KeyFrameCompressed
{
	FixedVector<int16, 1024> Trans;
};

VALIDATE_SIZE(KeyFrameCompressed, 0xA);
VALIDATE_SIZE(KeyFrameTransCompressed, 0x10);
VALIDATE_SIZE(KeyFrame, 0x14);
VALIDATE_SIZE(KeyFrameTrans, 0x20);
