#ifndef __GTA_SUNSHADOWMATH_H__
#define __GTA_SUNSHADOWMATH_H__

#include <cmath>
#include <cstring>

namespace SunShadowMath
{

struct Vec3
{
	float x, y, z;
};

struct ShadowSetup
{
	bool active;
	Vec3 lightDir;       // Effective unit vector toward sun (z >= minSunZ)
	Vec3 rayDir;         // Unit light propagation direction (-lightDir)
	Vec3 right;          // Light-space X axis
	Vec3 up;             // Light-space Y axis
	Vec3 focusWorld;     // World-space center of ortho volume
	float centerX;       // Texel-snapped light-space X center
	float centerY;       // Texel-snapped light-space Y center
	float centerZ;       // Light-space Z center
	float halfExtentXY;  // Ortho half-width/height in world units
	float halfDepthZ;    // Ortho half-depth in world units
	float invMapSize;    // 1.0f / mapSize
	float fadeStrength;  // Smooth fade near sunrise/sunset [0..1]
	float lightView[16]; // Column-major 4x4
	float lightProj[16]; // Column-major 4x4
	float shadowMatrix[16]; // Column-major lightProj * lightView
};

inline Vec3 MakeVec3(float x, float y, float z)
{
	Vec3 v = { x, y, z };
	return v;
}

inline float Dot3(const Vec3 &a, const Vec3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross3(const Vec3 &a, const Vec3 &b)
{
	return MakeVec3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

inline bool IsFiniteVec3(const Vec3 &v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline void MultiplyColMajor4x4(float out[16], const float a[16], const float b[16])
{
	float res[16];
	for(int col = 0; col < 4; col++){
		for(int row = 0; row < 4; row++){
			res[col * 4 + row] =
				a[0 * 4 + row] * b[col * 4 + 0] +
				a[1 * 4 + row] * b[col * 4 + 1] +
				a[2 * 4 + row] * b[col * 4 + 2] +
				a[3 * 4 + row] * b[col * 4 + 3];
		}
	}
	std::memcpy(out, res, sizeof(res));
}

inline ShadowSetup ComputeSunShadowSetup(
	const Vec3 &sunToSky,
	const Vec3 &camPos,
	const Vec3 &camForward,
	float halfExtentXY = 88.0f,
	float halfDepthZ = 240.0f,
	int mapSize = 2048)
{
	ShadowSetup out;
	std::memset(&out, 0, sizeof(out));
	out.lightView[0] = out.lightView[5] = out.lightView[10] = out.lightView[15] = 1.0f;
	out.lightProj[0] = out.lightProj[5] = out.lightProj[10] = out.lightProj[15] = 1.0f;
	out.shadowMatrix[0] = out.shadowMatrix[5] = out.shadowMatrix[10] = out.shadowMatrix[15] = 1.0f;

	if(!IsFiniteVec3(sunToSky) || !IsFiniteVec3(camPos) || !IsFiniteVec3(camForward) ||
	   !(halfExtentXY > 1.0f) || !(halfDepthZ > 1.0f) || mapSize < 16)
		return out;

	float sunLenSq = Dot3(sunToSky, sunToSky);
	if(!(sunLenSq > 1.0e-8f))
		return out;

	float invSunLen = 1.0f / std::sqrt(sunLenSq);
	Vec3 L = MakeVec3(sunToSky.x * invSunLen, sunToSky.y * invSunLen, sunToSky.z * invSunLen);
	if(L.z <= 0.03f)
		return out;

	// Smoothly fade shadow strength in/out near the horizon
	float horizonFade = (L.z - 0.03f) / (0.16f - 0.03f);
	if(horizonFade < 0.0f) horizonFade = 0.0f;
	if(horizonFade > 1.0f) horizonFade = 1.0f;
	out.fadeStrength = horizonFade * horizonFade * (3.0f - 2.0f * horizonFade);

	// Clamp grazing sun elevation to keep ortho texel density crisp at sunrise/sunset
	const float minElevZ = 0.25f;
	if(L.z < minElevZ){
		float horizLen = std::sqrt(L.x * L.x + L.y * L.y);
		if(horizLen > 1.0e-5f){
			float targetHoriz = std::sqrt(1.0f - minElevZ * minElevZ) / horizLen;
			L.x *= targetHoriz;
			L.y *= targetHoriz;
			L.z = minElevZ;
		}else{
			L = MakeVec3(0.0f, 0.0f, 1.0f);
		}
	}

	Vec3 F = MakeVec3(-L.x, -L.y, -L.z);
	Vec3 ref = (std::fabs(F.z) < 0.95f) ? MakeVec3(0.0f, 0.0f, 1.0f) : MakeVec3(0.0f, 1.0f, 0.0f);
	Vec3 R = Cross3(F, ref);
	float rLen = std::sqrt(Dot3(R, R));
	if(!(rLen > 1.0e-6f))
		return out;
	R = MakeVec3(R.x / rLen, R.y / rLen, R.z / rLen);
	Vec3 U = Cross3(R, F);

	// Shift focus forward along horizontal camera view so ~70% of the shadow box lies ahead
	float fwdH = std::sqrt(camForward.x * camForward.x + camForward.y * camForward.y);
	float fwdX = (fwdH > 1.0e-4f) ? (camForward.x / fwdH) : 0.0f;
	float fwdY = (fwdH > 1.0e-4f) ? (camForward.y / fwdH) : 0.0f;
	float leadDist = halfExtentXY * 0.40f;
	Vec3 focus = MakeVec3(
		camPos.x + fwdX * leadDist,
		camPos.y + fwdY * leadDist,
		camPos.z);

	float cx = Dot3(focus, R);
	float cy = Dot3(focus, U);
	float cz = Dot3(focus, F);

	// Snap light-space XY origin to shadow-map texel increments to eliminate shimmering
	float texelStep = (2.0f * halfExtentXY) / (float)mapSize;
	if(texelStep > 1.0e-5f){
		cx = std::floor(cx / texelStep) * texelStep;
		cy = std::floor(cy / texelStep) * texelStep;
	}

	out.active = true;
	out.lightDir = L;
	out.rayDir = F;
	out.right = R;
	out.up = U;
	out.focusWorld = focus;
	out.centerX = cx;
	out.centerY = cy;
	out.centerZ = cz;
	out.halfExtentXY = halfExtentXY;
	out.halfDepthZ = halfDepthZ;
	out.invMapSize = 1.0f / (float)mapSize;

	// Column-major lightView
	out.lightView[0]  = R.x;  out.lightView[1]  = U.x;  out.lightView[2]  = F.x;  out.lightView[3]  = 0.0f;
	out.lightView[4]  = R.y;  out.lightView[5]  = U.y;  out.lightView[6]  = F.y;  out.lightView[7]  = 0.0f;
	out.lightView[8]  = R.z;  out.lightView[9]  = U.z;  out.lightView[10] = F.z;  out.lightView[11] = 0.0f;
	out.lightView[12] = -cx;  out.lightView[13] = -cy;  out.lightView[14] = -cz;  out.lightView[15] = 1.0f;

	// Column-major orthographic lightProj (OpenGL NDC [-1..+1] on X, Y, Z)
	std::memset(out.lightProj, 0, sizeof(out.lightProj));
	out.lightProj[0]  = 1.0f / halfExtentXY;
	out.lightProj[5]  = 1.0f / halfExtentXY;
	out.lightProj[10] = 1.0f / halfDepthZ;
	out.lightProj[15] = 1.0f;

	MultiplyColMajor4x4(out.shadowMatrix, out.lightProj, out.lightView);
	return out;
}

inline Vec3 ProjectPointToShadowUVDepth(const float shadowMatrix[16], const Vec3 &worldPos)
{
	float cx = shadowMatrix[0] * worldPos.x + shadowMatrix[4] * worldPos.y + shadowMatrix[8]  * worldPos.z + shadowMatrix[12];
	float cy = shadowMatrix[1] * worldPos.x + shadowMatrix[5] * worldPos.y + shadowMatrix[9]  * worldPos.z + shadowMatrix[13];
	float cz = shadowMatrix[2] * worldPos.x + shadowMatrix[6] * worldPos.y + shadowMatrix[10] * worldPos.z + shadowMatrix[14];
	float cw = shadowMatrix[3] * worldPos.x + shadowMatrix[7] * worldPos.y + shadowMatrix[11] * worldPos.z + shadowMatrix[15];
	float invW = (std::fabs(cw) > 1.0e-6f) ? (1.0f / cw) : 1.0f;
	return MakeVec3(
		(cx * invW) * 0.5f + 0.5f,
		(cy * invW) * 0.5f + 0.5f,
		(cz * invW) * 0.5f + 0.5f);
}

inline bool DoesSphereOverlapShadowVolume(const ShadowSetup &setup, const Vec3 &center, float radius)
{
	if(!setup.active)
		return false;
	float r = radius > 0.0f ? radius : 0.0f;
	float lx = Dot3(center, setup.right) - setup.centerX;
	if(std::fabs(lx) > setup.halfExtentXY + r)
		return false;
	float ly = Dot3(center, setup.up) - setup.centerY;
	if(std::fabs(ly) > setup.halfExtentXY + r)
		return false;
	float lz = Dot3(center, setup.rayDir) - setup.centerZ;
	if(std::fabs(lz) > setup.halfDepthZ + r)
		return false;
	return true;
}

} // namespace SunShadowMath

#endif // __GTA_SUNSHADOWMATH_H__
