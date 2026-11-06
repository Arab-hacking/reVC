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
static int32 s_uSkyTop = -1;
static int32 s_uSkyHorizon = -1;
static int32 s_uSunColor = -1;
static int32 s_uSunDir = -1;
static int32 s_uCloudParams = -1;

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
"	float Br = u_skyTop.x;\n"
"	float Bm = u_skyTop.y;\n"
"	float g = u_skyTop.z;\n"
"	vec3 nitrogen = vec3(0.650, 0.570, 0.475);\n"
"	vec3 Kr = Br / pow(nitrogen, vec3(4.0));\n"
"	vec3 Km = Bm / pow(nitrogen, vec3(0.84));\n"
"	vec3 pos = v_localDir * inversesqrt(max(dot(v_localDir, v_localDir), 1.0e-6));\n"
"	vec3 fsun = u_skySunDir.xyz * inversesqrt(max(dot(u_skySunDir.xyz, u_skySunDir.xyz), 1.0e-6));\n"
"	float py = max(pos.y, 0.0);\n"
"	float mu = dot(pos, fsun);\n"
"	float rayleigh = 3.0 / (8.0 * 3.14159265) * (1.0 + mu * mu);\n"
"	vec3 mie = (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(max(1.0 + g * g - 2.0 * g * mu, 1.0e-4), 1.5)) / (Br + Bm);\n"
"	float e16 = exp(-py * 16.0) + 0.1;\n"
"	vec3 dayExt = exp(-exp(-((py + fsun.y * 4.0) * e16 / 80.0) / Br) * e16 * Kr / Br)\n"
"	            * exp(-py * exp(-py * 8.0) * 4.0) * exp(-py * 2.0) * 4.0;\n"
"	float nightVal = max(1.0 - exp(min(fsun.y, 0.0)), 0.0) * 0.12;\n"
"	vec3 nightExt = vec3(nightVal * 0.65, nightVal * 0.80, nightVal * 1.15);\n"
"	vec3 extinction = mix(dayExt, nightExt, clamp(-fsun.y * 0.2 + 0.5, 0.0, 1.0));\n"
"	// Solar-elevation auto-exposure matching UpdateExposure.fs + Tonemapping.fs\n"
"	float dayExp = mix(1.0, 0.28, smoothstep(0.12, 0.78, fsun.y));\n"
"	vec3 hdr = max(rayleigh * mie * extinction, vec3(0.0)) * dayExp;\n"
"	// Subtle solar disc + corona above horizon\n"
"	float sunAbove = smoothstep(-0.03, 0.08, fsun.y) * step(0.0, pos.y);\n"
"	float sunDisc = smoothstep(0.9991, 0.99985, mu) * sunAbove;\n"
"	hdr += extinction * dayExp * (2.6 * sunDisc);\n"
"	// Subtle night stars\n"
"	float nightStarMask = smoothstep(-0.08, -0.32, fsun.y) * smoothstep(0.05, 0.35, pos.y);\n"
"	if(nightStarMask > 0.001){\n"
"		float starField = smoothstep(0.9965, 0.9994, br_sky_noise(pos * 190.0));\n"
"		hdr += vec3(0.55, 0.65, 0.85) * (starField * nightStarMask * 0.55);\n"
"	}\n"
"	float time = u_cloudParams.x;\n"
"	float horizonFade = smoothstep(0.02, 0.16, pos.y);\n"
"	if(horizonFade > 0.001){\n"
"		vec3 cloudCoord = pos / max(pos.y, 0.05);\n"
"		float sunUp = smoothstep(-0.08, 0.35, fsun.y);\n"
"		vec3 cloudTop = extinction * dayExp * 1.45 + vec3(0.55 * sunUp);\n"
"		vec3 cloudBase = hdr * 0.58 + vec3(0.09 * sunUp);\n"
"		float cirrus = smoothstep(1.0 - u_cloudParams.y, 1.0, br_sky_fbm(cloudCoord * 2.0 + vec3(time * 0.05))) * 0.32;\n"
"		hdr = mix(hdr, cloudTop, cirrus * py * horizonFade);\n"
"		for(int i = 0; i < 3; i++){\n"
"			float cum = smoothstep(1.0 - u_cloudParams.z, 1.0, br_sky_fbm((0.7 + float(i) * 0.01) * cloudCoord + vec3(time * 0.30)));\n"
"			float cLit = smoothstep(0.15, 0.78, cum);\n"
"			vec3 cCol = mix(cloudBase, cloudTop, cLit);\n"
"			hdr = mix(hdr, cCol, min(cum, 1.0) * 0.78 * py * horizonFade);\n"
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
"	// Smooth atmospheric horizon-to-ground fade when pos.y < 0 (mirrored lower hemisphere)\n"
"	if(pos.y < 0.0)\n"
"		hdr *= mix(1.0, 0.52, smoothstep(0.0, -0.35, pos.y));\n"
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

	// SkyboxDynamicClouds.fs scattering: Br=0.0025, Bm=0.0003, g=0.92, whitePoint=4.0
	float uScat[4] = { 0.0025f, 0.0003f, 0.92f, 4.0f };
	float uBot[4] = { botR, botG, botB, hasNormalBand };
	float uSunCol[4] = { sunR, sunG, sunB, 1.0f };
	// Swizzle GTA Z-up sun vector into SkyboxDynamicClouds Y-up convention: (x, z, y)
	float uSunDir[4] = { sunV.x, sunV.z, sunV.y, sunVis };
	float uCloud[4] = { tSec * 0.035f, cirrusCov, cumulusCov, 0.35f };

	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uSkyTop], 1, uScat);
	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uSkyHorizon], 1, uBot);
	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uSunColor], 1, uSunCol);
	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uSunDir], 1, uSunDir);
	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uCloudParams], 1, uCloud);

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
	teardownVertexInput(header);
}

static void
EnsureSkyDomeProcShader(void)
{
	if(s_skyDomeProcShader != nil)
		return;
	if(s_uSkyTop < 0){
		s_uSkyTop = rw::gl3::registerUniform("u_skyTop");
		s_uSkyHorizon = rw::gl3::registerUniform("u_skyHorizon");
		s_uSunColor = rw::gl3::registerUniform("u_sunColor");
		s_uSunDir = rw::gl3::registerUniform("u_skySunDir");
		s_uCloudParams = rw::gl3::registerUniform("u_cloudParams");
	}
	const char *vs[] = { rw::gl3::shaderDecl, rw::gl3::header_vert_src, s_skyDomeVertSrc, nil };
	const char *fs[] = { rw::gl3::shaderDecl, rw::gl3::header_frag_src, s_skyDomeFragSrc, nil };
	s_skyDomeProcShader = rw::gl3::Shader::create(vs, fs);
	if(s_skyDomeProcShader){
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

	RpGeometry *geom = RpAtomicGetGeometry(e.atomic);
	if(geom){
		uint32 flags = RpGeometryGetFlags(geom);
		// Remove baked dark vertex colors if any so time-of-day sky textures render pure
		flags &= ~rpGEOMETRYPRELIT;
		flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
		RpGeometrySetFlags(geom, flags);
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
			int count = 0;
			rw::Texture *first = nil;
			rw::Texture *second = nil;
			rw::Texture *maskCand = nil;
			rw::Texture *normCand = nil;
			FORLIST(lnk, dict->textures){
				rw::Texture *t = rw::Texture::fromDict(lnk);
				count++;
				if(first == nil) first = t;
				else if(second == nil) second = t;
				if(strstr(t->name, "nrm") || strstr(t->name, "norm") || strstr(t->name, "NRM") || strstr(t->name, "Normal"))
					normCand = t;
				else if(strstr(t->name, "mask") || strstr(t->name, "cloud") || strstr(t->name, "Cloud"))
					maskCand = t;
			}
			if(count >= 2){
				s_skyDomeMaskTex = maskCand ? maskCand : first;
				s_skyDomeNormalTex = normCand ? normCand : (s_skyDomeMaskTex == first ? second : first);
			}else if(count == 1){
				s_skyDomeMaskTex = first;
			}
		}
		EnsureSkyDomeProcShader();
		if(s_skyDomeProcPipe != nil){
			e.atomic->pipeline = s_skyDomeProcPipe;
			s_skyDomeUseProcedural = true;
		}
	}
#else
	(void)isSkyDome;
#endif

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
#ifdef RW_GL3
		if(s_mode == MODE_AUTO || s_mode == MODE_SKYDOME)
			s_mode = MODE_SKYBOX;
		else if(s_mode == MODE_SKYBOX)
			s_mode = MODE_OFF;
		else
			s_mode = MODE_SKYDOME;
#else
		if(s_mode == MODE_AUTO || s_mode == MODE_SKYBOX)
			s_mode = MODE_SKYDOME;
		else if(s_mode == MODE_SKYDOME)
			s_mode = MODE_OFF;
		else
			s_mode = MODE_SKYBOX;
#endif
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

#ifdef RW_GL3
	bool wantSkyDome = (s_mode == MODE_SKYDOME) || (s_mode == MODE_AUTO && s_hasSkyDome);
	bool wantSkyBox = (s_mode == MODE_SKYBOX) || (s_mode == MODE_AUTO && !s_hasSkyDome && s_hasSkyBox);
#else
	bool wantSkyBox = (s_mode == MODE_SKYBOX) || (s_mode == MODE_AUTO && s_hasSkyBox);
	bool wantSkyDome = (s_mode == MODE_SKYDOME) || (s_mode == MODE_AUTO && !s_hasSkyBox && s_hasSkyDome);
#endif

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
