#include "common.h"

#include "CustomSky.h"
#include "CustomSkyMath.h"
#include "CustomModels.h"
#include "Clock.h"
#include "Game.h"
#include "Lights.h"
#include "ModelInfo.h"
#include "Streaming.h"
#include "TempColModels.h"
#include "Timecycle.h"
#include "Timer.h"
#include "TxdStore.h"
#include "Weather.h"
#include "RwHelper.h"
#include "DebugLog.h"

#include <cmath>
#include <cstring>

namespace {

struct SkyModelSpec {
	const char *modelName;
	const char *txdName;
	float drawDist;
};

static const SkyModelSpec kSkyBoxBaseSpecs[customsky::PHASE_COUNT] = {
	{ "BR_Sky_Rasv",  "rip", 500.0f }, // 10656
	{ "BR_Sky_Day",   "rip", 500.0f }, // 10657
	{ "BR_Sky_Zak",   "rip", 500.0f }, // 10658
	{ "BR_Sky_Night", "rip", 500.0f }  // 10659
};

static const SkyModelSpec kSkyBoxBlendSpecs[customsky::PHASE_COUNT] = {
	{ "BR_Sky_Rasv_bl",  "rip", 500.0f }, // 10662
	{ "BR_Sky_Day_bl",   "rip", 500.0f }, // 10663
	{ "BR_Sky_Zak_bl",   "rip", 500.0f }, // 10664
	{ "BR_Sky_Night_bl", "rip", 500.0f }  // 10665
};

static const SkyModelSpec kSkyDomeSpec = {
	"BR_SkyDome", "SkyDome", 100.0f // 14389
};

struct SkyModelEntry {
	int32 slot;
	RpAtomic *atomic;
	RwFrame *frame;
	float normScale;
	bool openBottom;
	bool prepared;
	bool loadAttempted;
};

static SkyModelEntry s_skyBoxBase[customsky::PHASE_COUNT];
static SkyModelEntry s_skyBoxBlend[customsky::PHASE_COUNT];
static SkyModelEntry s_skyDome;
static CCustomSky::Mode s_mode = CCustomSky::MODE_AUTO;
static bool s_initialised = false;
static bool s_hasSkyBox = false;
static bool s_hasSkyDome = false;

#ifdef RW_GL3
static rw::gl3::ObjPipeline *s_skyDomeProcPipe = nil;
static rw::gl3::Shader *s_skyDomeProcShader = nil;
static rw::Texture *s_skyDomeMaskTex = nil;
static rw::Texture *s_skyDomeNormalTex = nil;
static bool s_skyDomeUseProcedural = false;
static bool s_skyDomeProcShaderTried = false;
static GLint s_uSkyTopLoc = -1;
static GLint s_uSkyHorizonLoc = -1;
static GLint s_uSunColorLoc = -1;
static GLint s_uSunDirLoc = -1;
static GLint s_uCloudParamsLoc = -1;

static const char *s_skyDomeVertSrc =
"VSIN(ATTRIB_POS) vec3 in_pos;\n"
"VSOUT vec3 v_localDir;\n"
"VSOUT vec2 v_uv;\n"
"void main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * V;\n"
"	vec3 rel = V.xyz - u_world[3].xyz;\n"
"	v_localDir = vec3(rel.x, rel.z, rel.y);\n"
"	v_uv = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"
"}\n";

// SkyboxDynamicClouds.fs + SkyboxStaticClouds.fs + Noise.glsl + UpdateExposure.fs (Shaders For BLAYZ RUSSIA PC)
static const char *s_skyDomeFragSrc =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"
"uniform vec4 u_skyTop;\n"
"uniform vec4 u_skyHorizon;\n"
"uniform vec4 u_sunColor;\n"
"uniform vec4 u_skySunDir;\n"
"uniform vec4 u_cloudParams;\n"
"FSIN vec3 v_localDir;\n"
"FSIN vec2 v_uv;\n"
"float br_sky_hash(float n) { return fract(sin(n) * 43758.5453); }\n"
"float br_sky_noise(vec3 x)\n"
"{\n"
"	vec3 f = fract(x);\n"
"	float n = dot(floor(x), vec3(1.0, 157.0, 113.0));\n"
"	return mix(mix(mix(br_sky_hash(n + 0.0), br_sky_hash(n + 1.0), f.x),\n"
"	               mix(br_sky_hash(n + 157.0), br_sky_hash(n + 158.0), f.x), f.y),\n"
"	           mix(mix(br_sky_hash(n + 113.0), br_sky_hash(n + 114.0), f.x),\n"
"	               mix(br_sky_hash(n + 270.0), br_sky_hash(n + 271.0), f.x), f.y), f.z);\n"
"}\n"
"float br_sky_fbm(vec3 p)\n"
"{\n"
"	mat3 m = mat3(0.0, 1.60, 1.20, -1.60, 0.72, -0.96, -1.20, -0.96, 1.28);\n"
"	float f = 0.0;\n"
"	f += br_sky_noise(p) * 0.5;\n"
"	p = m * p * 1.1;\n"
"	f += br_sky_noise(p) * 0.25;\n"
"	p = m * p * 1.2;\n"
"	f += br_sky_noise(p) * (1.0 / 6.0);\n"
"	p = m * p * 1.3;\n"
"	f += br_sky_noise(p) * (1.0 / 12.0);\n"
"	p = m * p * 1.4;\n"
"	f += br_sky_noise(p) * (1.0 / 24.0);\n"
"	return f;\n"
"}\n"
"void main(void)\n"
"{\n"
"	vec3 pos = v_localDir * inversesqrt(max(dot(v_localDir, v_localDir), 1.0e-6));\n"
"	vec3 fsun = u_skySunDir.xyz * inversesqrt(max(dot(u_skySunDir.xyz, u_skySunDir.xyz), 1.0e-6));\n"
"	float py = max(pos.y, 0.0);\n"
"	float mu = dot(pos, fsun);\n"
"	float nightMix = smoothstep(0.10, -0.22, fsun.y);\n"
"	if(u_atmosphere.x >= 1.5){\n"
"		float highSun = smoothstep(0.02, 0.58, fsun.y);\n"
"		float sunsetBand = smoothstep(-0.18, 0.08, fsun.y) * (1.0 - smoothstep(0.08, 0.42, fsun.y));\n"
"		vec3 dayZenith = mix(vec3(0.18, 0.34, 0.64), vec3(0.16, 0.42, 0.84), highSun);\n"
"		vec3 dayHoriz = mix(vec3(0.84, 0.56, 0.38), vec3(0.64, 0.78, 0.94), highSun);\n"
"		dayHoriz = mix(dayHoriz, clamp(u_skyHorizon.rgb, vec3(0.15), vec3(0.98)), 0.38);\n"
"		vec3 nightZenithM = vec3(0.022, 0.036, 0.082);\n"
"		vec3 nightHorizM = vec3(0.062, 0.090, 0.172);\n"
"		float hGrad = pow(clamp(py, 0.0, 1.0), 0.52);\n"
"		vec3 mSky = mix(mix(dayHoriz, dayZenith, hGrad), mix(nightHorizM, nightZenithM, hGrad), nightMix);\n"
"		float sunGlow = pow(max(mu, 0.0), 6.0) * (1.0 - hGrad * 0.72) * sunsetBand;\n"
"		mSky += vec3(0.62, 0.30, 0.12) * sunGlow;\n"
"		float sunAboveM = smoothstep(-0.03, 0.08, fsun.y) * step(0.0, pos.y);\n"
"		float sunDiscM = clamp((mu - 0.9955) * 210.0, 0.0, 1.0) * sunAboveM;\n"
"		float sunHaloM = pow(max(mu, 0.0), 32.0) * 0.24 * sunAboveM;\n"
"		vec3 sunColM = mix(vec3(1.0, 0.84, 0.64), clamp(u_sunColor.rgb, vec3(0.6), vec3(1.0)), 0.45);\n"
"		mSky += sunColM * (sunDiscM * 0.95 + sunHaloM);\n"
"		if(u_atmosphere.x >= 2.15 && pos.y > 0.03){\n"
"			float cFade = smoothstep(0.03, 0.22, pos.y);\n"
"			vec3 cCoord = (pos / max(pos.y, 0.07)) * 0.85 + vec3(u_cloudParams.x * 0.25, 0.0, u_cloudParams.x * 0.12);\n"
"			float n2 = br_sky_noise(cCoord) * 0.66 + br_sky_noise(cCoord * 2.15) * 0.34;\n"
"			float cMask = smoothstep(1.0 - u_cloudParams.z * 0.92, 0.96, n2) * cFade * 0.68;\n"
"			vec3 cDay = mix(vec3(0.88, 0.74, 0.64), vec3(0.96, 0.97, 0.99), highSun);\n"
"			vec3 cColM = mix(cDay, vec3(0.10, 0.14, 0.24), nightMix);\n"
"			mSky = mix(mSky, cColM, cMask);\n"
"		}\n"
"		if(pos.y < 0.0)\n"
"			mSky *= mix(1.0, 0.86, smoothstep(0.0, -0.40, pos.y));\n"
"		FRAGCOLOR(vec4(BR_ApplyMobileColorGrade(clamp(mSky, 0.0, 1.25)), 1.0));\n"
"		return;\n"
"	}\n"
"	float Br = u_skyTop.x;\n"
"	float Bm = u_skyTop.y;\n"
"	float g = u_skyTop.z;\n"
"	vec3 nitrogen = vec3(0.650, 0.570, 0.475);\n"
"	vec3 Kr = Br / pow(nitrogen, vec3(4.0));\n"
"	vec3 Km = Bm / pow(nitrogen, vec3(0.84));\n"
"	float rayleigh = 3.0 / (8.0 * 3.14159265) * (1.0 + mu * mu);\n"
"	vec3 mie = (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(max(1.0 + g * g - 2.0 * g * mu, 1.0e-4), 1.5)) / (Br + Bm);\n"
"	float e16 = exp(-py * 16.0) + 0.1;\n"
"	float fsunDay = max(fsun.y, -0.01);\n"
"	float innerExp = exp(clamp(-((py + fsunDay * 4.0) * e16 / 80.0) / Br, -40.0, 40.0));\n"
"	vec3 dayExt = exp(-min(innerExp * e16 * Kr / Br, vec3(60.0)))\n"
"	            * exp(-py * exp(-py * 8.0) * 4.0) * exp(-py * 2.0) * 4.0;\n"
"	float dayExp = mix(1.0, 0.28, smoothstep(0.12, 0.78, fsun.y));\n"
"	vec3 dayHdr = max(rayleigh * mie * dayExt, vec3(0.0)) * dayExp;\n"
"	vec3 nightHoriz = vec3(0.052, 0.076, 0.145);\n"
"	vec3 nightZenith = vec3(0.018, 0.030, 0.068);\n"
"	vec3 nightHdr = mix(nightHoriz, nightZenith, smoothstep(0.0, 0.70, py));\n"
"	vec3 hdr = mix(dayHdr, nightHdr, nightMix);\n"
"	// Subtle solar disc + corona above horizon\n"
"	float sunAbove = smoothstep(-0.03, 0.08, fsun.y) * step(0.0, pos.y);\n"
"	float sunDisc = smoothstep(0.9992, 0.99988, mu) * sunAbove;\n"
"	hdr += dayExt * dayExp * (0.85 * sunDisc);\n"
"	// Subtle night stars\n"
"	float nightStarMask = smoothstep(-0.06, -0.28, fsun.y) * smoothstep(0.05, 0.35, pos.y);\n"
"	if(nightStarMask > 0.001){\n"
"		float starField = smoothstep(0.9962, 0.9993, br_sky_noise(pos * 190.0));\n"
"		hdr += vec3(0.60, 0.72, 0.95) * (starField * nightStarMask * 0.65);\n"
"	}\n"
"	float time = u_cloudParams.x;\n"
"	float horizonFade = smoothstep(0.02, 0.16, pos.y);\n"
"	if(horizonFade > 0.001){\n"
"		vec3 cloudCoord = pos / max(pos.y, 0.05);\n"
"		float sunUp = smoothstep(-0.08, 0.35, fsun.y);\n"
"		vec3 dayCloudTop = dayExt * dayExp * 1.45 + vec3(0.55 * sunUp);\n"
"		vec3 nightCloudTop = vec3(0.065, 0.090, 0.165);\n"
"		vec3 cloudTop = mix(dayCloudTop, nightCloudTop, nightMix);\n"
"		vec3 cloudBase = mix(hdr * 0.58 + vec3(0.09 * sunUp), nightHdr * 0.72, nightMix);\n"
"		float cirrus = smoothstep(1.0 - u_cloudParams.y, 1.0, br_sky_fbm(cloudCoord * 2.0 + vec3(time * 0.05))) * 0.32;\n"
"		hdr = mix(hdr, cloudTop, cirrus * py * horizonFade);\n"
"		for(int i = 0; i < 3; i++){\n"
"			float cum = smoothstep(1.0 - u_cloudParams.z, 1.0, br_sky_fbm((0.7 + float(i) * 0.01) * cloudCoord + vec3(time * 0.30)));\n"
"			float cLit = smoothstep(0.15, 0.78, cum);\n"
"			vec3 cCol = mix(cloudBase, cloudTop, cLit);\n"
"			hdr = mix(hdr, cCol, min(cum, 1.0) * 0.75 * py * horizonFade);\n"
"		}\n"
"	}\n"
"	if(pos.y > 0.0 && u_skyHorizon.w > 0.5 && v_uv.y >= 0.218 && v_uv.y <= 0.480){\n"
"		vec2 uv = vec2(v_uv.x + 0.876, (v_uv.y - 0.218) * (1.0 / (0.480 - 0.218)));\n"
"		vec2 sampleUV = vec2(uv.x, 1.0 - uv.y);\n"
"		vec3 nrmRaw = texture(tex1, sampleUV).rgb * 2.0 - 1.0;\n"
"		vec3 cloudsNrm = nrmRaw * inversesqrt(max(dot(nrmRaw, nrmRaw), 1.0e-6));\n"
"		float cloudMask = texture(tex0, sampleUV).g;\n"
"		vec3 sunToGround = vec3(-fsun.x, -fsun.y, -fsun.z);\n"
"		float cloudLit = max(dot(sunToGround, cloudsNrm), 0.0);\n"
"		vec3 baseCloud = BR_SRGBtoLINEAR(vec4(u_skyHorizon.rgb, 1.0)).rgb * 0.65;\n"
"		vec3 topCloud = BR_SRGBtoLINEAR(vec4(u_sunColor.rgb, 1.0)).rgb * 0.95;\n"
"		vec3 staticCol = mix(baseCloud, topCloud, cloudLit);\n"
"		hdr = mix(staticCol, hdr, clamp(mix(1.0, cloudMask, u_cloudParams.w), 0.0, 1.0));\n"
"	}\n"
"	// Smooth atmospheric horizon-to-ground continuation when pos.y < 0 (mirrored lower hemisphere)\n"
"	if(pos.y < 0.0)\n"
"		hdr *= mix(1.0, 0.88, smoothstep(0.0, -0.40, pos.y));\n"
"	hdr += vec3((br_sky_noise(pos * 256.0) - 0.5) * 0.008);\n"
"	float wp = u_skyTop.w > 0.5 ? u_skyTop.w : 4.0;\n"
"	vec3 ldr = BR_TonemapAces(max(hdr, vec3(0.0)), wp, 2.2);\n"
"	FRAGCOLOR(vec4(clamp(ldr, 0.0, 1.0), 1.0));\n"
"}\n";

static void
SkyDomeProcRenderCB(rw::Atomic *atomic, rw::gl3::InstanceDataHeader *header)
{
	using namespace rw;
	using namespace rw::gl3;

	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);
	setupVertexInput(header);

	s_skyDomeProcShader->use();

	float botR = CTimeCycle::GetSkyBottomRed() / 255.0f;
	float botG = CTimeCycle::GetSkyBottomGreen() / 255.0f;
	float botB = CTimeCycle::GetSkyBottomBlue() / 255.0f;
	float sunR = CTimeCycle::GetSunCoronaRed() / 255.0f;
	float sunG = CTimeCycle::GetSunCoronaGreen() / 255.0f;
	float sunB = CTimeCycle::GetSunCoronaBlue() / 255.0f;

	CVector sunV = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
	float sunVis = sunV.z > -0.15f ? Clamp((sunV.z + 0.15f) / 0.45f, 0.0f, 1.0f) : 0.0f;
	float tSec = (float)(CTimer::GetTimeInMilliseconds() % 1000000u) * 0.001f;
	float weatherCloud = Clamp(CWeather::CloudCoverage, 0.0f, 1.0f);
	float cirrusCov = Clamp(0.46f + 0.20f * weatherCloud, 0.35f, 0.72f);
	float cumulusCov = Clamp(0.42f + 0.22f * weatherCloud, 0.32f, 0.70f);
	float hasNormalBand = (s_skyDomeMaskTex != nil && s_skyDomeNormalTex != nil && s_skyDomeMaskTex != s_skyDomeNormalTex) ? 1.0f : 0.0f;

	// SkyboxDynamicClouds.fs scattering with tamed Mie forward peak (g=0.74) so sunset sky does not white-out
	float uScat[4] = { 0.0025f, 0.00024f, 0.74f, 4.0f };
	float uBot[4] = { botR, botG, botB, hasNormalBand };
	float uSunCol[4] = { sunR, sunG, sunB, 1.0f };
	// Swizzle GTA Z-up sun vector into SkyboxDynamicClouds Y-up convention: (x, z, y)
	float uSunDir[4] = { sunV.x, sunV.z, sunV.y, sunVis };
	float uCloud[4] = { tSec * 0.035f, cirrusCov, cumulusCov, 0.35f };

	if(s_uSkyTopLoc >= 0) glUniform4fv(s_uSkyTopLoc, 1, uScat);
	if(s_uSkyHorizonLoc >= 0) glUniform4fv(s_uSkyHorizonLoc, 1, uBot);
	if(s_uSunColorLoc >= 0) glUniform4fv(s_uSunColorLoc, 1, uSunCol);
	if(s_uSunDirLoc >= 0) glUniform4fv(s_uSunDirLoc, 1, uSunDir);
	if(s_uCloudParamsLoc >= 0) glUniform4fv(s_uCloudParamsLoc, 1, uCloud);

	InstanceData *inst = header->inst;
	for(rw::uint32 i = 0; i < header->numMeshes; i++){
		Texture *t0 = s_skyDomeMaskTex ? s_skyDomeMaskTex : inst->material->texture;
		Texture *t1 = s_skyDomeNormalTex ? s_skyDomeNormalTex : t0;
		setTexture(0, t0);
		setTexture(1, t1);
		rw::SetRenderState(VERTEXALPHA, FALSE);
		flushCache();
		drawInst(header, inst);
		inst++;
	}
	setTexture(1, nil);
	setTexture(0, nil);
	teardownVertexInput(header);
}

static void
EnsureSkyDomeProcShader(void)
{
	if(s_skyDomeProcShader != nil || s_skyDomeProcShaderTried)
		return;
	s_skyDomeProcShaderTried = true;
	const char *vs[] = { rw::gl3::shaderDecl, rw::gl3::header_vert_src, s_skyDomeVertSrc, nil };
	const char *fs[] = { rw::gl3::shaderDecl, rw::gl3::header_frag_src, s_skyDomeFragSrc, nil };
	s_skyDomeProcShader = rw::gl3::Shader::create(vs, fs);
	if(s_skyDomeProcShader){
		s_uSkyTopLoc = glGetUniformLocation(s_skyDomeProcShader->program, "u_skyTop");
		s_uSkyHorizonLoc = glGetUniformLocation(s_skyDomeProcShader->program, "u_skyHorizon");
		s_uSunColorLoc = glGetUniformLocation(s_skyDomeProcShader->program, "u_sunColor");
		s_uSunDirLoc = glGetUniformLocation(s_skyDomeProcShader->program, "u_skySunDir");
		s_uCloudParamsLoc = glGetUniformLocation(s_skyDomeProcShader->program, "u_cloudParams");
		s_skyDomeProcPipe = rw::gl3::ObjPipeline::create();
		s_skyDomeProcPipe->instanceCB = rw::gl3::defaultInstanceCB;
		s_skyDomeProcPipe->uninstanceCB = rw::gl3::defaultUninstanceCB;
		s_skyDomeProcPipe->renderCB = SkyDomeProcRenderCB;
	}
}
#endif

static void
ResetEntry(SkyModelEntry &e)
{
	e.slot = -1;
	e.atomic = nil;
	e.frame = nil;
	e.normScale = 1.0f;
	e.openBottom = false;
	e.prepared = false;
	e.loadAttempted = false;
}

static void
DestroyEntryFrame(SkyModelEntry &e)
{
	if(e.atomic && e.frame){
		RpAtomicSetFrame(e.atomic, nil);
	}
	if(e.frame){
		RwFrameDestroy(e.frame);
		e.frame = nil;
	}
	if(e.atomic){
		RpAtomicDestroy(e.atomic);
		e.atomic = nil;
	}
	e.prepared = false;
	e.loadAttempted = false;
}

static int32
FindOrRegisterModel(const SkyModelSpec &spec, int32 &nextFreeSlot)
{
	int32 existingSlot = -1;
	if(CModelInfo::GetModelInfo(spec.modelName, &existingSlot) != nil && existingSlot >= 0)
		return existingSlot;

#ifdef CUSTOM_MODELS
	if(!CCustomModels::HasModelFile(spec.modelName))
		return -1;
#else
	return -1;
#endif

	while(nextFreeSlot < MODELINFOSIZE && CModelInfo::GetModelInfo(nextFreeSlot) != nil)
		nextFreeSlot++;
	if(nextFreeSlot >= MODELINFOSIZE || !CModelInfo::CanAddSimpleModel())
		return -1;

	const char *txdToUse = spec.txdName;
#ifdef CUSTOM_MODELS
	bool paired = CCustomModels::PairedImageTxd(spec.modelName) >= 0;
	if(paired)
		txdToUse = spec.modelName;
#endif
	if(CTxdStore::FindTxdSlot(txdToUse) == -1 && !CTxdStore::HasFreeCustomSlot())
		return -1;

	CSimpleModelInfo *mi = CModelInfo::AddSimpleModel(nextFreeSlot);
	if(mi == nil)
		return -1;
	mi->SetModelName(spec.modelName);
	mi->SetTexDictionary(txdToUse);
#ifdef CUSTOM_MODELS
	if(paired && strcasecmp(spec.modelName, spec.txdName) != 0 &&
	   (CTxdStore::FindTxdSlot(spec.txdName) >= 0 || CCustomModels::HasImageTxd(spec.txdName))){
		int parentSlot = CTxdStore::FindTxdSlot(spec.txdName);
		if(parentSlot == -1 && CTxdStore::HasFreeCustomSlot())
			parentSlot = CTxdStore::AddTxdSlot(spec.txdName);
		if(parentSlot >= 0)
			CTxdStore::SetTxdParent(mi->GetTxdSlot(), parentSlot);
	}
#endif
	mi->SetNumAtomics(1);
	float dists[3] = { spec.drawDist, spec.drawDist, spec.drawDist };
	mi->SetLodDistances(dists);
	mi->m_alpha = 255;
	mi->m_noFade = true;
	mi->m_ignoreLight = true;
	mi->SetColModel(&CTempColModels::ms_colModelBBox);
	REVC_LOG_INFO("CustomSky: auto-registered %s (TXD %s) at slot %d",
		spec.modelName, txdToUse, nextFreeSlot);
	return nextFreeSlot++;
}

static RpMaterial*
NormalizeSkyMaterialCB(RpMaterial *mat, void *data)
{
	RwUInt8 alpha = (RwUInt8)(uintptr)data;
	RwRGBA col = { 255, 255, 255, alpha };
	RpMaterialSetColor(mat, &col);
	RwSurfaceProperties surf = { 1.0f, 0.0f, 1.0f };
	RpMaterialSetSurfaceProperties(mat, &surf);
	if(mat->texture){
		RwTextureSetAddressing(mat->texture, rwTEXTUREADDRESSWRAP);
		RwTextureSetFilterMode(mat->texture, rwFILTERLINEAR);
	}
	return mat;
}

static RpAtomic*
FindFirstClumpAtomicCB(RpAtomic *atomic, void *data)
{
	RpAtomic **out = (RpAtomic**)data;
	if(*out == nil)
		*out = atomic;
	return atomic;
}

static bool
IsSlotStreamable(int32 slot)
{
	if(slot < 0 || slot >= MODELINFOSIZE)
		return false;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(slot);
	if(mi == nil)
		return false;
	if(mi->GetRwObject() != nil)
		return true;
	if(CStreaming::IsObjectInCdImage(slot))
		return true;
#ifdef CUSTOM_MODELS
	if(CCustomModels::CanServe(slot))
		return true;
#endif
	return false;
}

static int32
EnsureAuxTxdLoaded(const char *txdName)
{
	int32 slot = CTxdStore::FindTxdSlot(txdName);
#ifdef CUSTOM_MODELS
	if(slot == -1 && CCustomModels::HasImageTxd(txdName) && CTxdStore::HasFreeCustomSlot())
		slot = CTxdStore::AddTxdSlot(txdName);
#endif
	if(slot < 0)
		return -1;
	TxdDef *def = CTxdStore::GetSlot(slot);
	if(def && def->texDict == nil){
		bool canLoad = CStreaming::IsObjectInCdImage(slot + STREAM_OFFSET_TXD);
#ifdef CUSTOM_MODELS
		if(!canLoad && CCustomModels::CanServe(slot + STREAM_OFFSET_TXD))
			canLoad = true;
#endif
		if(canLoad){
			CStreaming::RequestTxd(slot, STREAMFLAGS_DEPENDENCY | STREAMFLAGS_KEEP_IN_MEMORY | STREAMFLAGS_DONT_REMOVE);
			CStreaming::LoadAllRequestedModels(false);
		}
	}
	return (def && def->texDict != nil) ? slot : -1;
}

static bool
HasValidRaster(RwTexture *t)
{
#ifdef LIBRW
	return t != nil && t->raster != nil && t->raster->width > 0 && t->raster->height > 0;
#else
	return t != nil && RwTextureGetRaster(t) != nil;
#endif
}

static bool
ContainsCI(const char *hay, const char *needle)
{
	if(hay == nil || needle == nil || needle[0] == '\0')
		return false;
	for(const char *p = hay; *p != '\0'; p++){
		const char *a = p;
		const char *b = needle;
		while(*a != '\0' && *b != '\0'){
			unsigned char ca = (unsigned char)*a;
			unsigned char cb = (unsigned char)*b;
			if(ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
			if(cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
			if(ca != cb)
				break;
			a++;
			b++;
		}
		if(*b == '\0')
			return true;
	}
	return false;
}

static RwTexture*
FindFallbackSkyTextureInSlot(int32 txdSlot, const char *modelName, const char *matTexName, bool dedicatedSkyTxd)
{
	if(txdSlot < 0)
		return nil;
	TxdDef *def = CTxdStore::GetSlot(txdSlot);
	if(def == nil || def->texDict == nil)
		return nil;
#ifdef LIBRW
	const bool wantNight = ContainsCI(modelName, "night");
	const bool wantRasv  = ContainsCI(modelName, "rasv");
	const bool wantZak   = ContainsCI(modelName, "zak");
	const bool wantDay   = ContainsCI(modelName, "day");

	if(matTexName != nil && matTexName[0] != '\0'){
		rw::Texture *byMat = def->texDict->find(matTexName);
		if(HasValidRaster(byMat))
			return byMat;
	}
	if(modelName != nil && modelName[0] != '\0'){
		rw::Texture *exact = def->texDict->find(modelName);
		if(HasValidRaster(exact))
			return exact;
	}
	rw::Texture *first = nil;
	FORLIST(lnk, def->texDict->textures){
		rw::Texture *t = rw::Texture::fromDict(lnk);
		if(!HasValidRaster(t))
			continue;
		if(first == nil)
			first = t;
		const bool isSkyNamed = ContainsCI(t->name, "sky") || dedicatedSkyTxd;
		if(!isSkyNamed)
			continue;
		if(wantNight && (ContainsCI(t->name, "night") || ContainsCI(t->name, "brsky")))
			return t;
		if(wantRasv && (ContainsCI(t->name, "rasv") || ContainsCI(t->name, "dawn") || ContainsCI(t->name, "morn")))
			return t;
		if(wantZak && (ContainsCI(t->name, "zak") || ContainsCI(t->name, "dusk") || ContainsCI(t->name, "even")))
			return t;
		if(wantDay && ContainsCI(t->name, "day"))
			return t;
	}
	return dedicatedSkyTxd ? first : nil;
#else
	(void)modelName;
	(void)matTexName;
	(void)dedicatedSkyTxd;
	return nil;
#endif
}

static bool
PrepareSkyEntry(SkyModelEntry &e, bool isSkyDome)
{
	if(e.prepared)
		return e.atomic != nil;
	if(e.loadAttempted || e.slot < 0)
		return false;
	e.loadAttempted = true;
	if(!IsSlotStreamable(e.slot))
		return false;

	CBaseModelInfo *mi = CModelInfo::GetModelInfo(e.slot);
	if(mi == nil)
		return false;

	const int32 brskySlot = EnsureAuxTxdLoaded("brsky");
	const int32 ripSlot   = EnsureAuxTxdLoaded("rip");
	const int32 domeSlot  = EnsureAuxTxdLoaded("SkyDome");
	const int32 modelTxd  = mi->GetTxdSlot();
	if(!isSkyDome){
		if(ripSlot >= 0 && brskySlot >= 0 && ripSlot != brskySlot &&
		   CTxdStore::GetParentTxdSlot(ripSlot) == -1 && CTxdStore::GetParentTxdSlot(brskySlot) != ripSlot)
			CTxdStore::SetTxdParent(ripSlot, brskySlot);
		if(modelTxd >= 0 && CTxdStore::GetParentTxdSlot(modelTxd) == -1){
			if(ripSlot >= 0 && ripSlot != modelTxd)
				CTxdStore::SetTxdParent(modelTxd, ripSlot);
			else if(brskySlot >= 0 && brskySlot != modelTxd)
				CTxdStore::SetTxdParent(modelTxd, brskySlot);
		}
	}else if(modelTxd >= 0 && domeSlot >= 0 && modelTxd != domeSlot &&
	         CTxdStore::GetParentTxdSlot(modelTxd) == -1){
		CTxdStore::SetTxdParent(modelTxd, domeSlot);
	}

	if(mi->GetRwObject() == nil){
		CStreaming::RequestModel(e.slot, STREAMFLAGS_DEPENDENCY | STREAMFLAGS_KEEP_IN_MEMORY | STREAMFLAGS_DONT_REMOVE);
		CStreaming::LoadAllRequestedModels(false);
	}
	RwObject *rwObj = mi->GetRwObject();
	if(rwObj == nil)
		return false;

	RpAtomic *srcAtomic = nil;
	if(RwObjectGetType(rwObj) == rpATOMIC){
		srcAtomic = (RpAtomic*)rwObj;
	}else if(RwObjectGetType(rwObj) == rpCLUMP){
		RpClumpForAllAtomics((RpClump*)rwObj, FindFirstClumpAtomicCB, &srcAtomic);
	}
	if(srcAtomic == nil || RpAtomicGetGeometry(srcAtomic) == nil)
		return false;

	mi->AddRef();
	e.atomic = RpAtomicClone(srcAtomic);
	if(e.atomic == nil)
		return false;

	e.frame = RwFrameCreate();
	RpAtomicSetFrame(e.atomic, e.frame);
#ifdef LIBRW
	e.atomic->pipeline = nil;
#endif

	bool hasAnyTexture = false;
	RpGeometry *geom = RpAtomicGetGeometry(e.atomic);
	if(geom){
		uint32 flags = RpGeometryGetFlags(geom);
		// Keep rpGEOMETRYLIGHT enabled so SetFullAmbient() + DeActivateDirectional()
		// supplies pure white (1,1,1) ambient in both GL3 and D3D9 without dark prelight.
		flags &= ~rpGEOMETRYPRELIT;
		flags |= rpGEOMETRYLIGHT | rpGEOMETRYMODULATEMATERIALCOLOR;
		RpGeometrySetFlags(geom, flags);

		const int32 nMat = RpGeometryGetNumMaterials(geom);
		for(int32 miIdx = 0; miIdx < nMat; miIdx++){
			RpMaterial *mat = RpGeometryGetMaterial(geom, miIdx);
			if(mat == nil)
				continue;
			if(!HasValidRaster(mat->texture)){
				const char *wantedTex = nil;
#ifdef LIBRW
				if(mat->texture != nil && mat->texture->name[0] != '\0')
					wantedTex = mat->texture->name;
#endif
				if(isSkyDome){
					const int32 domeAux[2] = { modelTxd, domeSlot };
					for(int s = 0; s < 2 && !HasValidRaster(mat->texture); s++){
						RwTexture *fb = FindFallbackSkyTextureInSlot(domeAux[s], mi->GetModelName(), wantedTex, true);
						if(fb)
							RpMaterialSetTexture(mat, fb);
					}
				}else{
					const int32 boxAux[3] = { modelTxd, brskySlot, ripSlot };
					const bool boxDedicated[3] = {
						(modelTxd >= 0 && modelTxd != ripSlot),
						true,
						false
					};
					for(int s = 0; s < 3 && !HasValidRaster(mat->texture); s++){
						RwTexture *fb = FindFallbackSkyTextureInSlot(boxAux[s], mi->GetModelName(), wantedTex, boxDedicated[s]);
						if(fb)
							RpMaterialSetTexture(mat, fb);
					}
				}
			}
			if(HasValidRaster(mat->texture))
				hasAnyTexture = true;
		}
		RpGeometryForAllMaterials(geom, NormalizeSkyMaterialCB, (void*)(uintptr)255);

		RpMorphTarget *mt = RpGeometryGetMorphTarget(geom, 0);
		if(mt){
			RwSphere bs = mt->boundingSphere;
			float centerLen = std::sqrt(bs.center.x * bs.center.x +
			                            bs.center.y * bs.center.y +
			                            bs.center.z * bs.center.z);
			e.normScale = customsky::ComputeNormalizedSkyScale(centerLen, bs.radius, 50.0f);

			RwV3d *verts = RpMorphTargetGetVertices(mt);
			int32 nVerts = RpGeometryGetNumVertices(geom);
			if(verts && nVerts > 0 && bs.radius > 1.0e-3f){
				float minZ = verts[0].z;
				float maxZ = verts[0].z;
				for(int32 vi = 1; vi < nVerts; vi++){
					if(verts[vi].z < minZ) minZ = verts[vi].z;
					if(verts[vi].z > maxZ) maxZ = verts[vi].z;
				}
				e.openBottom = (maxZ > 0.25f * bs.radius) && (minZ > -0.25f * bs.radius);
			}
		}
	}

#ifdef RW_GL3
	if(isSkyDome){
		s_skyDomeUseProcedural = false;
		s_skyDomeMaskTex = nil;
		s_skyDomeNormalTex = nil;
		int txdSlot = mi->GetTxdSlot();
		if(txdSlot >= 0 && CTxdStore::GetSlot(txdSlot) && CTxdStore::GetSlot(txdSlot)->texDict){
			rw::TexDictionary *dict = CTxdStore::GetSlot(txdSlot)->texDict;
			rw::Texture *maskCand = nil;
			rw::Texture *normCand = nil;
			FORLIST(lnk, dict->textures){
				rw::Texture *t = rw::Texture::fromDict(lnk);
				if(!HasValidRaster(t))
					continue;
				if(ContainsCI(t->name, "nrm") || ContainsCI(t->name, "norm"))
					normCand = t;
				else if(ContainsCI(t->name, "mask") || ContainsCI(t->name, "cloud"))
					maskCand = t;
			}
			if(maskCand != nil && normCand != nil && maskCand != normCand){
				s_skyDomeMaskTex = maskCand;
				s_skyDomeNormalTex = normCand;
			}
		}
		// Always bind SkyboxDynamicClouds procedural pipeline on BR_SkyDome (#NewSkyDome)
		// so dynamic sky & clouds render cleanly day and night even without static textures.
		EnsureSkyDomeProcShader();
		if(s_skyDomeProcPipe != nil){
			e.atomic->pipeline = s_skyDomeProcPipe;
			s_skyDomeUseProcedural = true;
		}
	}
#else
	(void)isSkyDome;
#endif

	if(!hasAnyTexture && !isSkyDome){
		DestroyEntryFrame(e);
		e.loadAttempted = true;
		return false;
	}

	e.prepared = true;
	return true;
}

static void
SetAtomicTintAndAlpha(RpAtomic *atomic, uint8 r, uint8 g, uint8 b, uint8 a)
{
	RpGeometry *geom = RpAtomicGetGeometry(atomic);
	if(geom == nil)
		return;
	int32 nMat = RpGeometryGetNumMaterials(geom);
	for(int32 i = 0; i < nMat; i++){
		RpMaterial *mat = RpGeometryGetMaterial(geom, i);
		if(mat){
			RwRGBA c = { r, g, b, a };
			RpMaterialSetColor(mat, &c);
		}
	}
}

static void
RenderSkyEntry(SkyModelEntry &e, const CVector &camPos, uint8 r, uint8 g, uint8 b, uint8 alpha, bool blended)
{
	if(e.atomic == nil || e.frame == nil)
		return;

	RwMatrix *m = RwFrameGetMatrix(e.frame);
	RwMatrixSetIdentity(m);
	m->right.x = e.normScale;
	m->up.y = e.normScale;
	m->at.z = e.normScale;
	m->pos.x = camPos.x;
	m->pos.y = camPos.y;
	m->pos.z = camPos.z;
	RwMatrixUpdate(m);
	RwFrameUpdateObjects(e.frame);

	SetAtomicTintAndAlpha(e.atomic, r, g, b, alpha);

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)(uintptr)(blended ? TRUE : FALSE));
	if(blended){
		RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
		RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	}
#ifdef LIBRW
	uint32 prevAlphaFunc = rw::GetRenderState(rw::ALPHATESTFUNC);
	rw::SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);
#endif

	RpAtomicRender(e.atomic);

	// Mirror open-bottom sky hemispheres below Z=0 so the horizon has no clear-color seam
	if(e.openBottom){
		m->at.z = -e.normScale;
		RwMatrixUpdate(m);
		RwFrameUpdateObjects(e.frame);
		RpAtomicRender(e.atomic);
		m->at.z = e.normScale;
		RwMatrixUpdate(m);
		RwFrameUpdateObjects(e.frame);
	}

#ifdef LIBRW
	rw::SetRenderState(rw::ALPHATESTFUNC, prevAlphaFunc);
#endif
}

} // namespace

void
CCustomSky::RegisterModels(void)
{
	for(int i = 0; i < customsky::PHASE_COUNT; i++){
		ResetEntry(s_skyBoxBase[i]);
		ResetEntry(s_skyBoxBlend[i]);
	}
	ResetEntry(s_skyDome);
	s_initialised = false;
	s_hasSkyBox = false;
	s_hasSkyDome = false;

	int32 nextFree = 4900;
	for(int i = 0; i < customsky::PHASE_COUNT; i++){
		s_skyBoxBase[i].slot = FindOrRegisterModel(kSkyBoxBaseSpecs[i], nextFree);
		s_skyBoxBlend[i].slot = FindOrRegisterModel(kSkyBoxBlendSpecs[i], nextFree);
	}
	s_skyDome.slot = FindOrRegisterModel(kSkyDomeSpec, nextFree);
}

void
CCustomSky::InitAfterStreaming(void)
{
	s_hasSkyBox = false;
	s_hasSkyDome = false;
	for(int i = 0; i < customsky::PHASE_COUNT; i++){
		if(s_skyBoxBase[i].slot == -1)
			CModelInfo::GetModelInfo(kSkyBoxBaseSpecs[i].modelName, &s_skyBoxBase[i].slot);
		if(s_skyBoxBlend[i].slot == -1)
			CModelInfo::GetModelInfo(kSkyBoxBlendSpecs[i].modelName, &s_skyBoxBlend[i].slot);
		if(IsSlotStreamable(s_skyBoxBase[i].slot))
			s_hasSkyBox = true;
	}
	if(s_skyDome.slot == -1)
		CModelInfo::GetModelInfo(kSkyDomeSpec.modelName, &s_skyDome.slot);
	if(IsSlotStreamable(s_skyDome.slot))
		s_hasSkyDome = true;

	s_initialised = true;
	if(s_hasSkyBox || s_hasSkyDome)
		REVC_LOG_INFO("CustomSky: available sources skybox=%d (rip) skydome=%d (SkyDome)",
			s_hasSkyBox ? 1 : 0, s_hasSkyDome ? 1 : 0);
}

void
CCustomSky::Shutdown(void)
{
	for(int i = 0; i < customsky::PHASE_COUNT; i++){
		DestroyEntryFrame(s_skyBoxBase[i]);
		DestroyEntryFrame(s_skyBoxBlend[i]);
	}
	DestroyEntryFrame(s_skyDome);
#ifdef RW_GL3
	if(s_skyDomeProcShader){
		s_skyDomeProcShader->destroy();
		s_skyDomeProcShader = nil;
	}
	if(s_skyDomeProcPipe){
		s_skyDomeProcPipe->destroy();
		s_skyDomeProcPipe = nil;
	}
	s_skyDomeMaskTex = nil;
	s_skyDomeNormalTex = nil;
	s_skyDomeUseProcedural = false;
	s_skyDomeProcShaderTried = false;
	s_uSkyTopLoc = -1;
	s_uSkyHorizonLoc = -1;
	s_uSunColorLoc = -1;
	s_uSunDirLoc = -1;
	s_uCloudParamsLoc = -1;
#endif
	s_initialised = false;
	s_hasSkyBox = false;
	s_hasSkyDome = false;
}

bool
CCustomSky::IsActive(void)
{
	if(!s_initialised || s_mode == MODE_OFF)
		return false;
	if(s_mode == MODE_SKYBOX)
		return s_hasSkyBox;
	if(s_mode == MODE_SKYDOME)
		return s_hasSkyDome;
	return s_hasSkyBox || s_hasSkyDome;
}

CCustomSky::Mode
CCustomSky::GetMode(void)
{
	return s_mode;
}

const char*
CCustomSky::GetModeName(Mode mode)
{
	switch(mode){
	case MODE_AUTO: return "AUTO";
	case MODE_SKYBOX: return "SKYBOX";
	case MODE_SKYDOME: return "SKYDOME";
	default: return "VANILLA";
	}
}

CCustomSky::Mode
CCustomSky::CycleMode(void)
{
	if(s_hasSkyBox && s_hasSkyDome){
		if(s_mode == MODE_AUTO || s_mode == MODE_SKYBOX)
			s_mode = MODE_SKYDOME;
		else if(s_mode == MODE_SKYDOME)
			s_mode = MODE_OFF;
		else
			s_mode = MODE_SKYBOX;
	}else if(s_hasSkyBox){
		s_mode = (s_mode == MODE_OFF) ? MODE_SKYBOX : MODE_OFF;
	}else if(s_hasSkyDome){
		s_mode = (s_mode == MODE_OFF) ? MODE_SKYDOME : MODE_OFF;
	}
	return s_mode;
}

bool
CCustomSky::Render(const CVector &camPos, bool forEnvMap)
{
	(void)forEnvMap;
	if(!IsActive() || !CGame::CanSeeOutSideFromCurrArea())
		return false;

	bool wantSkyBox = (s_mode == MODE_SKYBOX) || (s_mode == MODE_AUTO && s_hasSkyBox);
	bool wantSkyDome = (s_mode == MODE_SKYDOME) || (s_mode == MODE_AUTO && !s_hasSkyBox && s_hasSkyDome);

	RwBool prevZTest = TRUE, prevZWrite = TRUE, prevVertAlpha = FALSE;
	int32 prevCull = rwCULLMODECULLBACK;
	int32 prevSrcBlend = rwBLENDSRCALPHA, prevDstBlend = rwBLENDINVSRCALPHA;
	RwRenderStateGet(rwRENDERSTATEZTESTENABLE, &prevZTest);
	RwRenderStateGet(rwRENDERSTATEZWRITEENABLE, &prevZWrite);
	RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &prevVertAlpha);
	RwRenderStateGet(rwRENDERSTATECULLMODE, &prevCull);
	RwRenderStateGet(rwRENDERSTATESRCBLEND, &prevSrcBlend);
	RwRenderStateGet(rwRENDERSTATEDESTBLEND, &prevDstBlend);

	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);

#if defined(LIBRW) && defined(RW_GL3)
	rw::RGBAf prevSunMat = rw::getSunMaterialParams();
	rw::RGBAf prevAtmos = rw::getAtmosphereParams();
	rw::setSunMaterialParams(rw::makeRGBAf(0.0f, 0.0f, 0.0f, 0.0f));
	rw::setAtmosphereParams(rw::makeRGBAf(0.0f, 0.0f, 0.0f, 0.0f));
#endif
	SetFullAmbient();
	DeActivateDirectional();

	bool rendered = false;
	if(wantSkyBox){
		customsky::BlendState bs = customsky::EvaluateSkyBlend(
			CClock::GetHours(), CClock::GetMinutes(), (float)CClock::GetSeconds());

		SkyModelEntry *baseEntry = &s_skyBoxBase[bs.base];
		if(!PrepareSkyEntry(*baseEntry, false)){
			// Fallback to any available phase if one phase DFF is absent
			for(int i = 0; i < customsky::PHASE_COUNT; i++){
				if(PrepareSkyEntry(s_skyBoxBase[i], false)){
					baseEntry = &s_skyBoxBase[i];
					break;
				}
			}
		}
		if(baseEntry->atomic != nil){
			RenderSkyEntry(*baseEntry, camPos, 255, 255, 255, 255, false);
			rendered = true;

			int alphaByte = (int)(bs.alpha * 255.0f + 0.5f);
			if(alphaByte > 1 && bs.overlay != bs.base){
				SkyModelEntry *ovEntry = &s_skyBoxBlend[bs.overlay];
				if(!PrepareSkyEntry(*ovEntry, false))
					ovEntry = &s_skyBoxBase[bs.overlay];
				if(PrepareSkyEntry(*ovEntry, false) && ovEntry->atomic != nil){
					uint8 clampedAlpha = (uint8)(alphaByte > 254 ? 254 : alphaByte);
					RenderSkyEntry(*ovEntry, camPos, 255, 255, 255, clampedAlpha, true);
				}
			}
		}else if(s_mode == MODE_AUTO && s_hasSkyDome){
			wantSkyDome = true;
		}
	}

	if(!rendered && wantSkyDome){
		if(PrepareSkyEntry(s_skyDome, true) && s_skyDome.atomic != nil){
			uint8 tr = 255, tg = 255, tb = 255;
#ifdef RW_GL3
			bool proc = s_skyDomeUseProcedural;
#else
			bool proc = false;
#endif
			if(!proc){
				// Modulate single-texture panoramic SkyDome smoothly across time of day
				customsky::BlendState bs = customsky::EvaluateSkyBlend(
					CClock::GetHours(), CClock::GetMinutes(), (float)CClock::GetSeconds());
				struct RGBf { float r, g, b; };
				static const RGBf phaseTints[customsky::PHASE_COUNT] = {
					{ 1.00f, 0.84f, 0.74f }, // Rasv (warm dawn)
					{ 1.00f, 1.00f, 1.00f }, // Day  (pure daylight)
					{ 1.00f, 0.76f, 0.62f }, // Zak  (sunset glow)
					{ 0.24f, 0.28f, 0.42f }  // Night
				};
				RGBf a = phaseTints[bs.base];
				RGBf b = phaseTints[bs.overlay];
				float rr = a.r + (b.r - a.r) * bs.alpha;
				float gg = a.g + (b.g - a.g) * bs.alpha;
				float bb = a.b + (b.b - a.b) * bs.alpha;
				tr = (uint8)(rr * 255.0f + 0.5f);
				tg = (uint8)(gg * 255.0f + 0.5f);
				tb = (uint8)(bb * 255.0f + 0.5f);
			}
			RenderSkyEntry(s_skyDome, camPos, tr, tg, tb, 255, false);
			rendered = true;
		}else if(s_mode == MODE_AUTO && s_hasSkyBox){
			wantSkyBox = true;
		}
	}

	if(!rendered && wantSkyBox){
		customsky::BlendState bs = customsky::EvaluateSkyBlend(
			CClock::GetHours(), CClock::GetMinutes(), (float)CClock::GetSeconds());
		SkyModelEntry *baseEntry = &s_skyBoxBase[bs.base];
		if(PrepareSkyEntry(*baseEntry, false) && baseEntry->atomic != nil){
			RenderSkyEntry(*baseEntry, camPos, 255, 255, 255, 255, false);
			rendered = true;
		}
	}

	SetAmbientColours();
	ActivateDirectional();
#if defined(LIBRW) && defined(RW_GL3)
	rw::setSunMaterialParams(prevSunMat);
	rw::setAtmosphereParams(prevAtmos);
#endif
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)(uintptr)prevZTest);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)(uintptr)prevZWrite);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)(uintptr)prevVertAlpha);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)(uintptr)prevCull);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)(uintptr)prevSrcBlend);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)(uintptr)prevDstBlend);

	return rendered;
}
