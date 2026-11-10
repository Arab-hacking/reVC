// Host tests for the Black Russia graphics parity module.
//
// Every check pins a value or a formula to the address it was recovered from in
// `libblackrussia-client.so` (PUNCT ANALYSE v38, graphics chapters 09/12/13).
// Build: g++ -std=c++11 -o /tmp/gfx tests/GraphicsParityTests.cpp -Isrc/extras/custom

#include <cstdio>
#include <cmath>
#include "GraphicsParity.h"

using namespace brgfx;

static int failures = 0;
static int checks = 0;

static void
Check(bool cond, const char *what)
{
	checks++;
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void
CheckNear(float got, float want, float eps, const char *what)
{
	checks++;
	if(!(std::fabs(got - want) <= eps)){
		printf("FAIL: %s (got %.6f want %.6f)\n", what, got, want);
		failures++;
	}
}

// --- shader key (ch. 13.8) -------------------------------------------------
static void
TestShaderKey(void)
{
	// Bit 2 = skinning (0x4), bit 4 = u_xform (0x10), bit 6 = reflection (0x40).
	Check(KeyBit(KEY_SKINNING) == 0x4, "bit 2 is 0x4 (skinning, 0x17326c0)");
	Check(KeyBit(KEY_XFORM) == 0x10, "bit 4 is 0x10 (u_xform, 0x17326c4)");
	Check(KeyBit(KEY_REFLECTION) == 0x40, "bit 6 is 0x40 (reflection, 0x1734af8)");
	Check(KeyBit(KEY_WORLDNAM20) == 0x100000, "bit 20 is 0x100000");
	Check(KEY_WORLDNAMAL_MASK == 0x100040, "normal declaration mask 0x100040 (0x1732514)");

	// The normal is declared under either bit of the pair, but the reflection
	// block itself hangs on bit 6 only (`tbz w19, #6`).
	Check(KeyHasWorldNormal(0x100040), "normal declared when both bits set");
	Check(KeyHasWorldNormal(0x4 | 0x100000), "normal declared with bit 20 alone");
	Check(!KeyHasReflection(0x100000), "bit 20 alone does not enable reflection");
	Check(KeyHasReflection(0x40), "bit 6 enables reflection");
	Check(!KeyHasReflection(0x4), "skinning does not imply reflection");
	Check(KeyHasSkinning(0x4) && KeyHasReflection(0x40),
	      "skinning and reflection are independent bits (13.8.4)");
}

// --- paraboloid mapping (ch. 13.1: 0x43be0b / 0x434bb8 / 0x42b527) ---------
static void
TestParaboloid(void)
{
	float u, v;

	// Straight forward (+Z) lands in the centre of the map.
	ParaboloidUV(0.0f, 0.0f, 1.0f, u, v);
	CheckNear(u, 0.0f, 1e-6f, "forward reflection u");
	CheckNear(v, 0.0f, 1e-6f, "forward reflection v");

	// +X at horizon: normalize(1,0) * (0*0.5+0.5) = (0.5, 0).
	ParaboloidUV(1.0f, 0.0f, 0.0f, u, v);
	CheckNear(u, 0.5f, 1e-6f, "horizon +X u");
	CheckNear(v, 0.0f, 1e-6f, "horizon +X v");

	// -Y at horizon: (0, -0.5).
	ParaboloidUV(0.0f, -1.0f, 0.0f, u, v);
	CheckNear(u, 0.0f, 1e-6f, "horizon -Y u");
	CheckNear(v, -0.5f, 1e-6f, "horizon -Y v");

	// Backwards (-Z) collapses to the rim: |xy| normalised * (0.5 - 0.5) = 0.
	ParaboloidUV(0.6f, 0.8f, -1.0f, u, v);
	CheckNear(u, 0.0f, 1e-6f, "backwards reflection collapses u");
	CheckNear(v, 0.0f, 1e-6f, "backwards reflection collapses v");

	// The client does NOT clamp the paraboloid UV (that clamp belongs to the
	// screen-space tex8 branch). Negative coordinates are expected and valid.
	ParaboloidUV(0.0f, -1.0f, 0.0f, u, v);
	Check(v < 0.0f, "paraboloid UV is not clamped to [0,1] (13.1)");

	// UV mapping for the texture fetch is the plain [-1,1] -> [0,1] remap.
	float su, sv;
	ParaboloidToUV(0.0f, 0.0f, su, sv);
	CheckNear(su, 0.5f, 1e-6f, "polar -> UV centre");
	ParaboloidToUV(0.5f, -0.5f, su, sv);
	CheckNear(su, 0.75f, 1e-6f, "polar -> UV u");
	CheckNear(sv, 0.25f, 1e-6f, "polar -> UV v");

	// Any unit direction stays inside the unit disc: the map is one hemisphere.
	for(int i = 0; i < 32; i++){
		float a = (float)i * 0.19634954f;
		ParaboloidUV(std::cos(a), std::sin(a), 0.25f, u, v);
		Check(u * u + v * v <= 1.0001f, "paraboloid UV inside unit disc");
	}

	// Depth: length(ReflVector) * 0.002 (0x42b527).
	CheckNear(ParaboloidDepth(0.0f, 0.0f, 500.0f), 1.0f, 1e-4f,
	          "paraboloid depth = |v| * 0.002");
	CheckNear(ParaboloidDepth(3.0f, 4.0f, 0.0f), 5.0f * PARABOLOID_DEPTH_SCALE, 1e-5f,
	          "paraboloid depth scales with length");

	// The screen-space mirror branch clamps to 0.001..0.999 before the fetch
	// (0x3e11d5); it is a separate mechanism from the paraboloid map.
	float cu = 1.5f, cv = -0.5f;
	ClampUV(cu, cv);
	CheckNear(cu, REFLECT_UV_MAX, 0.0f, "screen mirror UV clamped to 0.999");
	CheckNear(cv, REFLECT_UV_MIN, 0.0f, "screen mirror UV clamped to 0.001");
}

// --- reflection chain (ch. 13.1) -------------------------------------------
static void
TestReflectionChain(void)
{
	// fresnelFactor = 1 - |dot(-viewVec, v_worldNormal)|   (0x43f19d)
	// Looking head-on along -Z at a +Z normal: dot(-view, n) = (0,0,1).(0,0,1) = 1
	CheckNear(Fresnel(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f), 0.0f, 1e-6f,
	          "head-on fresnel is 0");
	// Grazing: |dot| = 0 -> fresnel 1.
	CheckNear(Fresnel(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f), 1.0f, 1e-6f,
	          "grazing fresnel is 1");
	// Sign of the incoming view does not matter (abs()).
	CheckNear(Fresnel(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	          Fresnel(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f), 1e-6f,
	          "fresnel is symmetric in view direction");

	// envmap = texel * fresnel * u_envMapCoef; then pow(.., 0.8)  (0x40b632, 0x43f1e5)
	CheckNear(EnvMapTerm(1.0f, 1.0f, 1.0f), 1.0f, 1e-6f, "pow(1, 0.8) = 1");
	CheckNear(EnvMapTerm(0.25f, 1.0f, 1.0f), std::pow(0.25f, 0.8f), 1e-5f,
	          "envmap pow exponent 0.8");
	CheckNear(EnvMapTerm(1.0f, 0.5f, 0.5f), std::pow(0.25f, 0.8f), 1e-5f,
	          "fresnel and coefficient both multiply");
	CheckNear(EnvMapTerm(1.0f, 1.0f, 0.0f), 0.0f, 1e-6f,
	          "zero coefficient kills the reflection (material switch)");

	// color.rgb += (diffuseVec + specular + envmap.rgb)          (0x4309d9)
	CheckNear(ReflectedColor(0.2f, 0.3f, 0.5f), 1.0f, 1e-6f,
	          "reflection is added to diffuse and specular");

	// Near-mirror branch: mix(color, reflectColour, 0.8)         (0x3e1220)
	CheckNear(MirrorMix(0.0f, 1.0f), 0.8f, 1e-6f, "mirror mix weight is 0.8");
	CheckNear(MirrorMix(0.4f, 0.4f), 0.4f, 1e-6f, "mirror mix keeps equal inputs");
}

// --- sun specular (ch. 13.5: 0x40f29d) -------------------------------------
static void
TestSunSpecular(void)
{
	CheckNear(SUN_SPEC_POW, 50.0f, 0.0f, "specular exponent 50");
	CheckNear(SUN_SPEC_SCALE, 0.4f, 0.0f, "specular scale 0.4");
	// pow(1, 50) * 0.4 at perfect alignment.
	float spec = std::pow(1.0f, SUN_SPEC_POW) * SUN_SPEC_SCALE;
	CheckNear(spec, 0.4f, 1e-6f, "peak specular equals 0.4");
	// pow(0.9, 50) is ~0.005 - the highlight is tight.
	Check(std::pow(0.9f, SUN_SPEC_POW) < 0.01f, "specular lobe is narrow");
}

// --- fog (ch. 12) -----------------------------------------------------------
static void
TestFog(void)
{
	// v_fog = clamp((distance - FogStart) * 1/(FarClip-FogStart), 0, 1)
	float inv = FogInvRange(100.0f, 600.0f);
	CheckNear(inv, 1.0f / 500.0f, 1e-9f, "inverse fog range = 1/(Far-FogStart)");
	CheckNear(FogFactor(50.0f, 100.0f, inv), 0.0f, 0.0f, "before FogStart = clear");
	CheckNear(FogFactor(600.0f, 100.0f, inv), 1.0f, 1e-6f, "at FarClip = full fog");
	CheckNear(FogFactor(350.0f, 100.0f, inv), 0.5f, 1e-6f, "linear halfway");
	CheckNear(FogFactor(5000.0f, 100.0f, inv), 1.0f, 0.0f, "beyond FarClip clamped");

	// mix(color, fogColor, factor)
	CheckNear(FogMix(0.0f, 1.0f, 0.25f), 0.25f, 1e-6f, "fog mix blends toward fog colour");
	CheckNear(FogMix(0.3f, 0.9f, 0.0f), 0.3f, 1e-6f, "clear keeps object colour");

	// A degenerate FarClip must not divide by zero.
	CheckNear(FogInvRange(100.0f, 100.0f), 1.0f / 1e-4f, 1e-3f,
	          "zero span clamped");
	// librw stores 1/(start-end); our helper is independent of the sign.
	CheckNear(FogInvRange(600.0f, 100.0f), FogInvRange(100.0f, 600.0f), 1e-9f,
	          "inverse range sign independent");
}

// --- post process (ch. 09.2: blur_0x404248, vignette_0x3fd3ac) -------------
static void
TestPostProcess(void)
{
	Check(BLUR_KERNEL_SIZE == 5, "blur kernel size 5");
	CheckNear(BLUR_SIGMA, 7.0f, 0.0f, "blur sigma 7");

	// normpdf(0, sigma) = 0.39894/sigma
	CheckNear(BlurNormPdf(0.0f, BLUR_SIGMA), 0.39894f / 7.0f, 1e-6f,
	          "normpdf at the centre");

	float kernel[BLUR_KERNEL_SIZE];
	float Z;
	BlurKernel(kernel, Z);
	CheckNear(kernel[1], kernel[3], 1e-9f, "kernel is symmetric (kSize-j = kSize+j)");
	CheckNear(kernel[2], BlurNormPdf(0.0f, BLUR_SIGMA), 1e-9f, "kernel centre");
	CheckNear(kernel[0], BlurNormPdf(2.0f, BLUR_SIGMA), 1e-9f, "kernel tail at j=2");
	CheckNear(Z, kernel[0] + kernel[1] + kernel[2] + kernel[3] + kernel[4], 1e-9f,
	          "Z is the kernel sum");
	Check(Z > 0.0f, "kernel normaliser is positive");

	// Vignette: radius from the centre, scaled per axis, then a linear fade.
	float centre = VignetteFade(0.5f, 0.5f, 1.0f, 2.0f, -0.1f);
	CheckNear(centre, 0.0f, 1e-6f, "vignette centre has radius 0, fade clamps to 0");
	// radius(1,1) = sqrt(0.5) = 0.7071; *2.0 = 1.414 -> the formula's own
	// clamp(0,1) pins it to 1. Verify the raw radius via a scale that stays < 1.
	float corner = VignetteFade(1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	CheckNear(corner, std::sqrt(0.5f), 1e-6f, "vignette radius at the corner");
	CheckNear(VignetteFade(1.0f, 1.0f, 1.0f, 2.0f, 0.0f), 1.0f, 0.0f,
	          "vignette fade clamps at 1");
	float saturated = VignetteFade(1.0f, 1.0f, 1.0f, 40.0f, 0.0f);
	CheckNear(saturated, 1.0f, 0.0f, "vignette fade clamps to 1");
	// params.x stretches the horizontal term only.
	Check(VignetteFade(1.0f, 0.5f, 4.0f, 1.0f, 0.0f) > VignetteFade(1.0f, 0.5f, 1.0f, 1.0f, 0.0f),
	      "params.x scales the horizontal radius");

	// Outline composite: outline over scene by its alpha (0x43baf8).
	CheckNear(OutlineBlend(0.0f, 1.0f, 1.0f), 1.0f, 1e-6f, "outline fully covers");
	CheckNear(OutlineBlend(0.0f, 1.0f, 0.0f), 0.0f, 1e-6f, "zero alpha keeps scene");
	CheckNear(OutlineBlend(0.2f, 0.6f, 0.5f), 0.4f, 1e-6f, "half alpha blends");
}

// --- material maps (ch. 13.3) ---------------------------------------------
static void
TestMaterialMaps(void)
{
	// The client's PBR material carries 13 maps; metalness and reflectance are
	// what modulate the reflection on a given model.
	Check(MATMAP_COUNT == 13, "13 PBR material maps");
	Check(MATMAP_METALNESS == 4 && MATMAP_ROUGHNESS == 3 && MATMAP_REFLECTANCE == 6,
	      "metalness/roughness/reflectance slots");
}

int
main(void)
{
	printf("Black Russia graphics parity tests\n");
	TestShaderKey();
	TestParaboloid();
	TestReflectionChain();
	TestSunSpecular();
	TestFog();
	TestPostProcess();
	TestMaterialMaps();

	if(failures){
		printf("\n%d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("\n%d checks passed\n", checks);
	return 0;
}
