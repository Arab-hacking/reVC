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
"	v_localDir = in_pos;\n"
"	v_uv = in_tex0;\n"
"}\n";

// Reconstructed BR procedural SkyDome + normal-lit cloud dome shader
// (brso/world_shader_pieces.txt:547-715).
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
"void main(void)\n"
"{\n"
"	vec3 dir = v_localDir * inversesqrt(max(dot(v_localDir, v_localDir), 1.0e-8));\n"
"	float h = clamp(dir.z * 1.35 + 0.12, 0.0, 1.0);\n"
"	vec3 sky = mix(u_skyHorizon.rgb, u_skyTop.rgb, pow(h, 0.65));\n"
"	vec3 sunD = u_skySunDir.xyz * inversesqrt(max(dot(u_skySunDir.xyz, u_skySunDir.xyz), 1.0e-8));\n"
"	float cosTheta = max(dot(dir, sunD), 0.0);\n"
"	float sunHalo = pow(cosTheta, 8.0) * 0.35 + pow(cosTheta, 96.0) * 0.85;\n"
"	sky += u_sunColor.rgb * sunHalo * u_skySunDir.w;\n"
"	vec2 uvScroll = vec2(v_uv.x + u_cloudParams.x, 1.0 - (v_uv.y + u_cloudParams.y));\n"
"	vec4 maskSample = texture(tex0, uvScroll);\n"
"	vec3 nrmRaw = texture(tex1, uvScroll).xyz * 2.0 - 1.0;\n"
"	vec3 nrmSample = nrmRaw * inversesqrt(max(dot(nrmRaw, nrmRaw), 1.0e-8));\n"
"	vec3 cloudL = vec3(sunD.xy, max(sunD.z, 0.25));\n"
"	cloudL *= inversesqrt(max(dot(cloudL, cloudL), 1.0e-8));\n"
"	float cloudDensity = clamp(maskSample.r * u_cloudParams.z, 0.0, 1.0) * smoothstep(-0.05, 0.22, dir.z);\n"
"	float cloudLit = clamp(dot(nrmSample, cloudL) * 0.5 + 0.5, 0.25, 1.0);\n"
"	vec3 cloudCol = mix(u_skyHorizon.rgb * 0.88, vec3(1.0, 0.99, 0.97) * max(u_cloudParams.w, 0.18), cloudLit);\n"
"	vec3 finalCol = mix(sky, cloudCol, cloudDensity * 0.82);\n"
"	FRAGCOLOR(vec4(clamp(finalCol, 0.0, 1.0), 1.0));\n"
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

	float topR = CTimeCycle::GetSkyTopRed() / 255.0f;
	float topG = CTimeCycle::GetSkyTopGreen() / 255.0f;
	float topB = CTimeCycle::GetSkyTopBlue() / 255.0f;
	float botR = CTimeCycle::GetSkyBottomRed() / 255.0f;
	float botG = CTimeCycle::GetSkyBottomGreen() / 255.0f;
	float botB = CTimeCycle::GetSkyBottomBlue() / 255.0f;
	float sunR = CTimeCycle::GetSunCoronaRed() / 255.0f;
	float sunG = CTimeCycle::GetSunCoronaGreen() / 255.0f;
	float sunB = CTimeCycle::GetSunCoronaBlue() / 255.0f;

	CVector sunV = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
	float sunVis = sunV.z > -0.15f ? Clamp((sunV.z + 0.15f) / 0.45f, 0.0f, 1.0f) : 0.0f;
	float tSec = (float)(CTimer::GetTimeInMilliseconds() % 1000000u) * 0.001f;
	float skyLuma = Max(topR, Max(topG, topB));
	float cloudCov = Clamp(0.45f + 0.55f * CWeather::CloudCoverage, 0.25f, 1.0f);

	float uTop[4] = { topR, topG, topB, 1.0f };
	float uBot[4] = { botR, botG, botB, 1.0f };
	float uSunCol[4] = { sunR, sunG, sunB, 1.0f };
	float uSunDir[4] = { sunV.x, sunV.y, sunV.z, sunVis };
	float uCloud[4] = { tSec * 0.0045f, tSec * 0.0018f, cloudCov, skyLuma };

	glUniform4fv(s_skyDomeProcShader->uniformLocations[s_uSkyTop], 1, uTop);
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
				EnsureSkyDomeProcShader();
				if(s_skyDomeProcPipe != nil){
					s_skyDomeMaskTex = maskCand ? maskCand : first;
					s_skyDomeNormalTex = normCand ? normCand : (s_skyDomeMaskTex == first ? second : first);
					e.atomic->pipeline = s_skyDomeProcPipe;
					s_skyDomeUseProcedural = true;
				}
			}
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
	rw::setSunMaterialParams(rw::makeRGBAf(0.0f, 0.0f, 0.0f, 0.0f));
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
		}
	}

	SetAmbientColours();
	ActivateDirectional();
#if defined(LIBRW) && defined(RW_GL3)
	rw::setSunMaterialParams(prevSunMat);
#endif
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)(uintptr)prevZTest);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)(uintptr)prevZWrite);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)(uintptr)prevVertAlpha);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)(uintptr)prevCull);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)(uintptr)prevSrcBlend);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)(uintptr)prevDstBlend);

	return rendered;
}
