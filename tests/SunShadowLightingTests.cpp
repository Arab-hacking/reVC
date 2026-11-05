#include "../src/renderer/SunShadowMath.h"
#include "../src/renderer/ShadowTriWinding.h"
#include "../src/extras/custom/CustomTimecycleShadow.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond) do { \
	if(!(cond)){ \
		std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		std::abort(); \
	} \
} while(0)

namespace
{

struct Pt2 { float x, y; };

static float Cross2D(const Pt2 &a, const Pt2 &b)
{
	return a.x * b.y - a.y * b.x;
}

static int ClipPolyAgainstEdgeCCW(const Pt2 *inPts, int inCount, const Pt2 &edgeA, const Pt2 &edgeB, Pt2 *outPts)
{
	if(inCount < 3)
		return 0;
	Pt2 edge = { edgeB.x - edgeA.x, edgeB.y - edgeA.y };
	int outCount = 0;
	for(int i = 0; i < inCount; i++){
		const Pt2 &prev = inPts[(i + inCount - 1) % inCount];
		const Pt2 &curr = inPts[i];
		float cpPrev = Cross2D({ prev.x - edgeA.x, prev.y - edgeA.y }, edge);
		float cpCurr = Cross2D({ curr.x - edgeA.x, curr.y - edgeA.y }, edge);
		bool prevIn = cpPrev > 0.0f;
		bool currIn = cpCurr > 0.0f;
		if(currIn){
			if(!prevIn){
				float t = std::fabs(cpPrev) / (std::fabs(cpPrev) + std::fabs(cpCurr));
				outPts[outCount++] = { prev.x + (curr.x - prev.x) * t, prev.y + (curr.y - prev.y) * t };
			}
			outPts[outCount++] = curr;
		}else if(prevIn){
			float t = std::fabs(cpPrev) / (std::fabs(cpPrev) + std::fabs(cpCurr));
			outPts[outCount++] = { prev.x + (curr.x - prev.x) * t, prev.y + (curr.y - prev.y) * t };
		}
	}
	return outCount;
}

static int ClipQuadToRoadTriangle(Pt2 a, Pt2 b, Pt2 c)
{
	Pt2 quad[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
	Pt2 buf1[16], buf2[16], buf3[16];
	int n1 = ClipPolyAgainstEdgeCCW(quad, 4, a, b, buf1);
	int n2 = ClipPolyAgainstEdgeCCW(buf1, n1, b, c, buf2);
	return ClipPolyAgainstEdgeCCW(buf2, n2, c, a, buf3);
}

static void TestSunShadowBasisAndDepthOrdering(void)
{
	using namespace SunShadowMath;

	// 1) Nighttime sun below horizon disables shadow map cleanly
	ShadowSetup night = ComputeSunShadowSetup(
		MakeVec3(0.4f, 0.3f, -0.5f),
		MakeVec3(100.0f, 200.0f, 12.0f),
		MakeVec3(0.0f, 1.0f, 0.0f));
	CHECK(!night.active);
	CHECK(night.fadeStrength == 0.0f);

	// 2) Daytime afternoon sun (like 14:57) builds orthonormal basis & valid depth ordering
	ShadowSetup day = ComputeSunShadowSetup(
		MakeVec3(0.45f, -0.38f, 0.81f),
		MakeVec3(250.0f, -140.0f, 14.0f),
		MakeVec3(0.6f, 0.8f, -0.1f),
		88.0f, 240.0f, 2048);
	CHECK(day.active);
	CHECK(day.fadeStrength > 0.99f);
	CHECK(std::fabs(Dot3(day.right, day.up)) < 1.0e-5f);
	CHECK(std::fabs(Dot3(day.right, day.rayDir)) < 1.0e-5f);
	CHECK(std::fabs(Dot3(day.up, day.rayDir)) < 1.0e-5f);

	// Ground receiver inside camera look-ahead vs car roof / tree crown 6m toward sun
	Vec3 ground = day.focusWorld;
	Vec3 roof = MakeVec3(
		ground.x + day.lightDir.x * 6.0f,
		ground.y + day.lightDir.y * 6.0f,
		ground.z + day.lightDir.z * 6.0f);

	Vec3 uvGround = ProjectPointToShadowUVDepth(day.shadowMatrix, ground);
	Vec3 uvRoof   = ProjectPointToShadowUVDepth(day.shadowMatrix, roof);

	// Same shadow-map texel UV along the sun ray, strictly smaller depth for roof occluder
	CHECK(std::fabs(uvGround.x - uvRoof.x) < 1.0e-5f);
	CHECK(std::fabs(uvGround.y - uvRoof.y) < 1.0e-5f);
	CHECK(uvRoof.z < uvGround.z);
	float expectedDeltaZ = 6.0f / (2.0f * 240.0f);
	CHECK(std::fabs((uvGround.z - uvRoof.z) - expectedDeltaZ) < 1.0e-4f);

	// 3) Low sunrise elevation clamps shadow Z >= 0.25f and smoothly fades strength
	ShadowSetup dawn = ComputeSunShadowSetup(
		MakeVec3(0.99f, 0.0f, 0.08f),
		MakeVec3(0.0f, 0.0f, 10.0f),
		MakeVec3(1.0f, 0.0f, 0.0f));
	CHECK(dawn.active);
	CHECK(std::fabs(dawn.lightDir.z - 0.25f) < 1.0e-4f);
	CHECK(dawn.fadeStrength > 0.05f && dawn.fadeStrength < 0.95f);

	// 4) Sub-texel camera movement preserves stationary world-point fractional texel phase
	Vec3 worldPost = MakeVec3(262.3f, -118.7f, 14.5f);
	ShadowSetup jitter = ComputeSunShadowSetup(
		MakeVec3(0.45f, -0.38f, 0.81f),
		MakeVec3(250.012f, -139.991f, 14.0f),
		MakeVec3(0.6f, 0.8f, -0.1f),
		88.0f, 240.0f, 2048);
	Vec3 p0 = ProjectPointToShadowUVDepth(day.shadowMatrix, worldPost);
	Vec3 p1 = ProjectPointToShadowUVDepth(jitter.shadowMatrix, worldPost);
	float dTexX = (p1.x - p0.x) * 2048.0f;
	float dTexY = (p1.y - p0.y) * 2048.0f;
	CHECK(std::fabs(dTexX - std::round(dTexX)) < 1.0e-3f);
	CHECK(std::fabs(dTexY - std::round(dTexY)) < 1.0e-3f);

	// 5) Volume sphere culling
	CHECK(DoesSphereOverlapShadowVolume(day, day.focusWorld, 2.0f));
	Vec3 farAway = MakeVec3(
		day.focusWorld.x + day.right.x * 200.0f,
		day.focusWorld.y + day.right.y * 200.0f,
		day.focusWorld.z + day.right.z * 200.0f);
	CHECK(!DoesSphereOverlapShadowVolume(day, farAway, 5.0f));
}

static void TestCollisionShadowTriangleWinding(void)
{
	// GTA VC CColTrianglePlane::Set computes normal = CrossProduct(vc - va, vb - va).
	// Upward-normal (normal.z > 0) road triangle covering [-1..1]^2:
	Pt2 a = { -4.0f, -4.0f };
	Pt2 b = {  0.0f,  6.0f };
	Pt2 c = {  4.0f, -4.0f };
	float upNormalZ = Cross2D({ c.x - a.x, c.y - a.y }, { b.x - a.x, b.y - a.y });
	CHECK(upNormalZ > 0.0f);
	CHECK(ClipQuadToRoadTriangle(a, b, c) == 4);

	// Reversed-winding custom collision triangle (normal.z < 0)
	Pt2 revB = c;
	Pt2 revC = b;
	float revNormalZ = Cross2D({ revC.x - a.x, revC.y - a.y }, { revB.x - a.x, revB.y - a.y });
	CHECK(revNormalZ < 0.0f);
	// Without orientation fix, normal.z < 0 clips away 100% of the shadow quad
	CHECK(ClipQuadToRoadTriangle(a, revB, revC) == 0);

	// With OrientShadowTriangleCCW, reversed collision triangles clip identically to upward ones
	ShadowTriWinding::OrientShadowTriangleCCW(revNormalZ, revB, revC);
	CHECK(ClipQuadToRoadTriangle(a, revB, revC) == 4);
}

static void TestTimecycleShadowImportNormalization(void)
{
	bool present[8] = { true, true, true, true, true, true, true, true };
	bool hasAll[8]  = { true, true, true, true, true, true, true, true };

	// 1) BR timecyc.json with all-zero "Shad": 0 must NOT overwrite VC's shadow table with zeros
	float zeroShad[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	float scale = -1.0f;
	CHECK(!CustomTimecycleShadow::DetermineShadowFieldScale(present, hasAll, zeroShad, scale));

	// 2) Normalized [0..1] float "Shad" values scale up into byte range
	float unitShad[8] = { 0.35f, 0.5f, 0.75f, 0.85f, 0.9f, 0.8f, 0.6f, 0.4f };
	CHECK(CustomTimecycleShadow::DetermineShadowFieldScale(present, hasAll, unitShad, scale));
	CHECK(std::fabs(scale - 180.0f) < 1.0e-4f);

	// 3) Standard [0..255] byte "Shad" values keep 1.0 scale
	float byteShad[8] = { 80, 90, 120, 145, 160, 140, 110, 85 };
	CHECK(CustomTimecycleShadow::DetermineShadowFieldScale(present, hasAll, byteShad, scale));
	CHECK(std::fabs(scale - 1.0f) < 1.0e-4f);

	// 4) Sunny daytime floor protects against under-tuned daytime values without touching night or rain
	CHECK(CustomTimecycleShadow::EnsureDaytimeShadowFloor(20, 14, 0, 180.0f) == 115);
	CHECK(CustomTimecycleShadow::EnsureDaytimeShadowFloor(155, 14, 0, 180.0f) == 155);
	CHECK(CustomTimecycleShadow::EnsureDaytimeShadowFloor(20, 2, 0, 180.0f) == 20);
	CHECK(CustomTimecycleShadow::EnsureDaytimeShadowFloor(40, 14, 2, 180.0f) == 40);
}

} // namespace

int main()
{
	TestSunShadowBasisAndDepthOrdering();
	TestCollisionShadowTriangleWinding();
	TestTimecycleShadowImportNormalization();
	std::puts("Sun shadow math, collision triangle winding, and timecycle shadow tests passed");
	return 0;
}
