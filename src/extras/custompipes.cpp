#define WITHD3D
#include "common.h"

#ifdef EXTENDED_PIPELINES

#include "main.h"
#include "RwHelper.h"
#include "Lights.h"
#include "Timecycle.h"
#include "FileMgr.h"
#include "Clock.h"
#include "Weather.h"
#include "TxdStore.h"
#include "Renderer.h"
#include "World.h"
#include "Camera.h"
#include "Vehicle.h"
#include "Clouds.h"
#include "CustomSky.h"
#ifdef ANDROID
#include "Frontend.h"
#include "Timer.h"
#endif
#ifdef CUSTOM_MODELS
#include "SAStaticModel.h"
#endif
#include "custompipes.h"
#include "TestGraphics.h"

#ifndef LIBRW
#error "Need librw for EXTENDED_PIPELINES"
#endif

namespace CustomPipes {

rw::int32 CustomMatOffset;

void*
CustomMatCtor(void *object, int32, int32)
{
	CustomMatExt *ext = GetCustomMatExt((rw::Material*)object);
	ext->glossTex = nil;
	ext->haveGloss = false;
	ext->hasSaEnv = false;
	ext->saShininess = 0.0f;
	ext->hasSaSpec = false;
	ext->saSpecularity = 0.0f;
	memset(ext->saSpecTex, 0, sizeof(ext->saSpecTex));
	return object;
}

void*
CustomMatCopy(void *dst, void *src, int32, int32)
{
	CustomMatExt *srcext = GetCustomMatExt((rw::Material*)src);
	CustomMatExt *dstext = GetCustomMatExt((rw::Material*)dst);
	dstext->glossTex = srcext->glossTex;
	dstext->haveGloss = srcext->haveGloss;
	dstext->hasSaEnv = srcext->hasSaEnv;
	dstext->saShininess = srcext->saShininess;
	dstext->hasSaSpec = srcext->hasSaSpec;
	dstext->saSpecularity = srcext->saSpecularity;
	memcpy(dstext->saSpecTex, srcext->saSpecTex, sizeof(dstext->saSpecTex));
	return dst;
}

// SA CustomCarEnvMap (0x0253F2FC, 24 bytes) & CustomCarSpecMap (0x0253F2F6, 28 bytes)
static rw::Stream*
SaCarEnvMatRead(rw::Stream *stream, int32 len, void *object, int32, int32)
{
	CustomMatExt *ext = GetCustomMatExt((rw::Material*)object);
	if(len >= 24){
		float vals[5];
		stream->read32(vals, 20);
		stream->seek(len - 20);
		if(vals[4] >= 0.0f && vals[4] <= 10.0f){
			ext->hasSaEnv = true;
			ext->saShininess = vals[4];
		}
	}else if(len > 0){
		stream->seek(len);
	}
	return stream;
}

static rw::Stream*
SaCarSpecMatRead(rw::Stream *stream, int32 len, void *object, int32, int32)
{
	CustomMatExt *ext = GetCustomMatExt((rw::Material*)object);
	if(len >= 28){
		float spec = stream->readF32();
		stream->read8(ext->saSpecTex, 24);
		ext->saSpecTex[23] = '\0';
		if(len > 28)
			stream->seek(len - 28);
		if(spec >= 0.0f && spec <= 10.0f){
			ext->hasSaSpec = true;
			ext->saSpecularity = spec;
		}
	}else if(len > 0){
		stream->seek(len);
	}
	return stream;
}



rw::TexDictionary *neoTxd;

bool bRenderingEnvMap;
#ifdef ANDROID
int32 EnvMapSize = 256;
#else
int32 EnvMapSize = 512;
#endif
// Value EnvMapSize had before any menu touched it; EnvMapReinit(nil) restores
// it after a failed allocation at a smaller size.
static const int32 DefaultEnvMapSize = EnvMapSize;
rw::Camera *EnvMapCam;
rw::Texture *EnvMapTex;
rw::Texture *EnvMaskTex;
// Stand-in bound to stage 1 while `refl = 0`. The vehicle shader always samples
// tex1, so dropping the texture would make it sample whatever was left bound
// (the previous material) instead of "no reflection".
static rw::Texture *BlackEnvTex;
static rw::Texture *CreateBlackEnvTex(void);
static bool zipCarMaskOwned;

void
SetCarReflectionMask(rw::Texture *mask)
{
	if(zipCarMaskOwned && EnvMaskTex) EnvMaskTex->destroy();
	zipCarMaskOwned = mask != nil;
	EnvMaskTex = mask ? mask : (neoTxd ? neoTxd->find("CarReflectionMask") : nil);
}

static rw::RWDEVICE::Im2DVertex EnvScreenQuad[4];
static int16 QuadIndices[6] = { 0, 1, 2, 0, 2, 3 };

static rw::Camera*
CreateEnvMapCam(rw::World *world)
{
	rw::Raster *fbuf = rw::Raster::create(EnvMapSize, EnvMapSize, 0, rw::Raster::CAMERATEXTURE);
	if(fbuf){
		rw::Raster *zbuf = rw::Raster::create(EnvMapSize, EnvMapSize, 0, rw::Raster::ZBUFFER);
		if(zbuf){
			rw::Frame *frame = rw::Frame::create();
			if(frame){
				rw::Camera *cam = rw::Camera::create();
				if(cam){
					cam->frameBuffer = fbuf;
					cam->zBuffer = zbuf;
					cam->setFrame(frame);
					cam->setNearPlane(0.1f);
#ifdef ANDROID
					cam->setFarPlane(300.0f);
#else
					cam->setFarPlane(350.0f);
#endif
					rw::V2d vw = { 1.35f, 1.35f };
					cam->setViewWindow(&vw);
					EnvMapTex = rw::Texture::create(fbuf);
					if(EnvMapTex) {
						world->addCamera(cam);
						EnvMapTex->setFilter(rw::Texture::LINEAR);
						EnvMapTex->setAddressU(rw::Texture::CLAMP);
						EnvMapTex->setAddressV(rw::Texture::CLAMP);
						frame->matrix.right.x = -1.0f;
						frame->matrix.up.y = -1.0f;
						frame->matrix.update();
						return cam;
					}
					cam->destroy();
				}
				frame->destroy();
			}
			zbuf->destroy();
		}
		fbuf->destroy();
	}
	return nil;
}

static void
DestroyCam(rw::Camera *cam)
{
	if(cam == nil)
		return;
	if(cam->frameBuffer){
		cam->frameBuffer->destroy();
		cam->frameBuffer = nil;
	}
	if(cam->zBuffer){
		cam->zBuffer->destroy();
		cam->zBuffer = nil;
	}
	rw::Frame *f = cam->getFrame();
	if(f){
		cam->setFrame(nil);
		f->destroy();
	}
	cam->world->removeCamera(cam);
	cam->destroy();
}

void
RenderEnvMapScene(void)
{
	CRenderer::RenderRoads();
	CRenderer::RenderEverythingBarRoads();
#ifdef ANDROID
	if(FrontEndMenuManager.m_PrefsMobileGraphics >= CMenuManager::MOBILE_GFX_HIGH)
		CRenderer::RenderFadingInEntities();
#else
	CRenderer::RenderFadingInEntities();
#endif
}

void
EnvMapRender(void)
{
	if(VehiclePipeSwitch != VEHICLEPIPE_NEO || EnvMapCam == nil || EnvMapTex == nil)
		return;
	// TEST GRAPHICS `refl = 0`: the client's getter 0x1894a08 returns NULL and
	// the cache is never rendered. Skipping the pass here is the same saving -
	// the scene is not drawn a second time for a map nobody samples.
	if(!TestGraphicsReflectionEnabled())
		return;
#ifdef ANDROID
	int8 mobTier = FrontEndMenuManager.m_PrefsMobileGraphics;
	static uint32 s_lastEnvFrame = 0xFFFFFFFFu;
	uint32 curFrame = CTimer::GetFrameCounter();
	uint32 stride = (mobTier >= CMenuManager::MOBILE_GFX_HIGH) ? 2u :
	                ((mobTier == CMenuManager::MOBILE_GFX_MEDIUM) ? 3u : 5u);
	if(s_lastEnvFrame != 0xFFFFFFFFu && (uint32)(curFrame - s_lastEnvFrame) < stride)
		return;
	s_lastEnvFrame = curFrame;
#endif

	RwCameraEndUpdate(Scene.camera);

	// Place the upward probe right above the player's vehicle roof (or follow
	// the active camera when free-flying far from the ped in NoClip/cutscenes)
	// so the rendered scene matches CRenderer's camera-culled render list.
	CVector probePos = FindPlayerCoors();
	if(CVehicle *veh = FindPlayerVehicle()){
		probePos = veh->GetPosition();
		probePos.z += 0.8f;
	}else if((TheCamera.GetPosition() - probePos).MagnitudeSqr() > 900.0f){
		probePos = TheCamera.GetPosition();
	}else{
		probePos.z += 0.5f;
	}
	rw::V3d camPos = probePos;
	EnvMapCam->getFrame()->matrix.pos = camPos;
	EnvMapCam->getFrame()->transform(&EnvMapCam->getFrame()->matrix, rw::COMBINEREPLACE);

	rw::RGBA skycol;
	skycol.red = (CTimeCycle::GetSkyTopRed() + CTimeCycle::GetSkyBottomRed()) / 2;
	skycol.green = (CTimeCycle::GetSkyTopGreen() + CTimeCycle::GetSkyBottomGreen()) / 2;
	skycol.blue = (CTimeCycle::GetSkyTopBlue() + CTimeCycle::GetSkyBottomBlue()) / 2;
	skycol.alpha = 255;
	EnvMapCam->clear(&skycol, rwCAMERACLEARZ|rwCAMERACLEARIMAGE);

	// The client renders its env map with the paraboloid projection printed
	// straight into NDC (0x42b527), not with a perspective camera, and the
	// material shaders sample it with the matching polar mapping. Bind that
	// capture to the same raster the vehicle pipeline reads as tex1 so the
	// producer and the consumer finally agree. The reflection mask below is
	// part of the env map, so it is drawn while the raster is still bound.
	bool paraboloid = false;
	uint32 envFbo = 0;
#ifdef RW_GL3
	envFbo = rw::gl3::rasterFramebuffer((rw::Raster*)EnvMapCam->frameBuffer);
	if(envFbo != 0)
		paraboloid = rw::gl3::beginParaboloidPass(envFbo, EnvMapSize, EnvMapSize, &skycol);
#endif
	if(!paraboloid){
		// Non-GL3 backends (and a raster without an FBO) keep the perspective
		// camera path.
		RwCameraBeginUpdate(EnvMapCam);
	}else{
#ifdef RW_GL3
		rw::V3d envEye;
		envEye.x = camPos.x;
		envEye.y = camPos.y;
		envEye.z = camPos.z;
		rw::gl3::setCaptureEyePosition(envEye);
#endif
	}
	bRenderingEnvMap = true;
	CCustomSky::Render(CVector(camPos.x, camPos.y, camPos.z), true);
	RenderEnvMapScene();
	bRenderingEnvMap = false;

	// Only apply an explicitly supplied custom reflection mask; neo.txd's stock
	// CarReflectionMask is 0 in the zenith center and <= 74/255 everywhere else,
	// which blackens car roofs/hoods and kills 71% of sky reflection brightness.
	if(zipCarMaskOwned && EnvMaskTex){
		rw::SetRenderState(rw::VERTEXALPHA, TRUE);
		rw::SetRenderState(rw::SRCBLEND, rw::BLENDZERO);
		rw::SetRenderState(rw::DESTBLEND, rw::BLENDSRCCOLOR);
		rw::SetRenderStatePtr(rw::TEXTURERASTER, EnvMaskTex->raster);
		rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, EnvScreenQuad, 4, QuadIndices, 6);
		rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
		rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
	}
	if(paraboloid){
#ifdef RW_GL3
		rw::gl3::endParaboloidPass();
#endif
	}else{
		RwCameraEndUpdate(EnvMapCam);
	}
	RwCameraBeginUpdate(Scene.camera);

	// debug env map
//	rw::SetRenderStatePtr(rw::TEXTURERASTER, EnvMapTex->raster);
//	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, EnvScreenQuad, 4, QuadIndices, 6);
}

static void
EnvMapInit(void)
{
	if(neoTxd)
		EnvMaskTex = neoTxd->find("CarReflectionMask");

	if(BlackEnvTex == nil)
		BlackEnvTex = CreateBlackEnvTex();

	EnvMapCam = CreateEnvMapCam(Scene.world);
	if(EnvMapCam == nil || EnvMapTex == nil) {
		VehiclePipeSwitch = VEHICLEPIPE_MATFX;
		return; // unsupported/failed RT, do not dereference an absent camera
	}

	int width = EnvMapCam->frameBuffer->width;
	int height = EnvMapCam->frameBuffer->height;
	float screenZ = RwIm2DGetNearScreenZ();
	float recipZ = 1.0f/EnvMapCam->nearPlane;

	EnvScreenQuad[0].setScreenX(0.0f);
	EnvScreenQuad[0].setScreenY(0.0f);
	EnvScreenQuad[0].setScreenZ(screenZ);
	EnvScreenQuad[0].setCameraZ(EnvMapCam->nearPlane);
	EnvScreenQuad[0].setRecipCameraZ(recipZ);
	EnvScreenQuad[0].setColor(255, 255, 255, 255);
	EnvScreenQuad[0].setU(0.0f, recipZ);
	EnvScreenQuad[0].setV(0.0f, recipZ);

	EnvScreenQuad[1].setScreenX(0.0f);
	EnvScreenQuad[1].setScreenY(height);
	EnvScreenQuad[1].setScreenZ(screenZ);
	EnvScreenQuad[1].setCameraZ(EnvMapCam->nearPlane);
	EnvScreenQuad[1].setRecipCameraZ(recipZ);
	EnvScreenQuad[1].setColor(255, 255, 255, 255);
	EnvScreenQuad[1].setU(0.0f, recipZ);
	EnvScreenQuad[1].setV(1.0f, recipZ);

	EnvScreenQuad[2].setScreenX(width);
	EnvScreenQuad[2].setScreenY(height);
	EnvScreenQuad[2].setScreenZ(screenZ);
	EnvScreenQuad[2].setCameraZ(EnvMapCam->nearPlane);
	EnvScreenQuad[2].setRecipCameraZ(recipZ);
	EnvScreenQuad[2].setColor(255, 255, 255, 255);
	EnvScreenQuad[2].setU(1.0f, recipZ);
	EnvScreenQuad[2].setV(1.0f, recipZ);

	EnvScreenQuad[3].setScreenX(width);
	EnvScreenQuad[3].setScreenY(0.0f);
	EnvScreenQuad[3].setScreenZ(screenZ);
	EnvScreenQuad[3].setCameraZ(EnvMapCam->nearPlane);
	EnvScreenQuad[3].setRecipCameraZ(recipZ);
	EnvScreenQuad[3].setColor(255, 255, 255, 255);
	EnvScreenQuad[3].setU(1.0f, recipZ);
	EnvScreenQuad[3].setV(0.0f, recipZ);
}

static rw::Texture*
CreateBlackEnvTex(void)
{
	rw::Raster *ras = rw::Raster::create(1, 1, 32, rw::Raster::C8888);
	if(ras == nil)
		return nil;
	uint8 *px = ras->lock(0, rw::Raster::LOCKWRITE | rw::Raster::LOCKNOFETCH);
	if(px) {
		// RGBA order matches C8888; alpha 255 so alpha blending stays sane.
		px[0] = 0; px[1] = 0; px[2] = 0; px[3] = 255;
		ras->unlock(0);
	}
	rw::Texture *tex = rw::Texture::create(ras);
	if(tex == nil) {
		ras->destroy();
		return nil;
	}
	tex->setFilter(rw::Texture::LINEAR);
	tex->setAddressU(rw::Texture::CLAMP);
	tex->setAddressV(rw::Texture::CLAMP);
	return tex;
}

static void
EnvMapShutdownNoMask(void)
{
	if(BlackEnvTex) {
		BlackEnvTex->raster = nil;
		BlackEnvTex->destroy();
		BlackEnvTex = nil;
	}
	if(EnvMapTex) {
		EnvMapTex->raster = nil;
		EnvMapTex->destroy();
		EnvMapTex = nil;
	}
	DestroyCam(EnvMapCam);
	EnvMapCam = nil;
}

static void
EnvMapShutdown(void)
{
	SetCarReflectionMask(nil); // release any optional mask; neo.txd owns its fallback
	EnvMapShutdownNoMask();
}

rw::Texture*
EnvMapTextureForShading(void)
{
	return EnvMapTex ? EnvMapTex : BlackEnvTex;
}

void
EnvMapReinit(int32 size)
{
	// Before RwEngineInit (LoadINISettings runs first on the boot path) the
	// allocator below is a nil function pointer, so creating or destroying a
	// raster here jumps to address 0. The caller keeps the request pending.
	if(rw::Engine::state < rw::Engine::Opened)
		return;

	if(size <= 0) {
		// `refl = 0`: the client's getter 0x1894a08 returns NULL below level 1
		// and no map is ever rendered, so the camera and its raster go away.
		EnvMapShutdownNoMask();
		return;
	}

	// The mask lives in an external TXD and survives the RT; only the camera
	// and its textures are tied to the size.
	EnvMapShutdownNoMask();

	int32 prev = EnvMapSize;
	EnvMapSize = size;
	EnvMapInit();

	if(EnvMapCam == nil || EnvMapTex == nil) {
		// A 512x512 RGBA8 + depth pair can fail on a tight device. Falling
		// back to the platform default keeps reflections working instead of
		// silently turning them off for the rest of the session.
		EnvMapSize = prev;
		EnvMapInit();
	}
}

/*
 * Tweak values
 */

#define INTERP_SETUP \
		int h1 = CClock::GetHours();								  \
		int h2 = (h1+1)%24;										  \
		int w1 = CWeather::OldWeatherType;								  \
		int w2 = CWeather::NewWeatherType;								  \
		float timeInterp = (CClock::GetSeconds()/60.0f + CClock::GetMinutes())/60.0f;	  \
		float c0 = (1.0f-timeInterp)*(1.0f-CWeather::InterpolationValue);				  \
		float c1 = timeInterp*(1.0f-CWeather::InterpolationValue);					  \
		float c2 = (1.0f-timeInterp)*CWeather::InterpolationValue;					  \
		float c3 = timeInterp*CWeather::InterpolationValue;
#define INTERP(v) v[h1][w1]*c0 + v[h2][w1]*c1 + v[h1][w2]*c2 + v[h2][w2]*c3;
#define INTERPF(v,f) v[h1][w1].f*c0 + v[h2][w1].f*c1 + v[h1][w2].f*c2 + v[h2][w2].f*c3;

InterpolatedFloat::InterpolatedFloat(float init)
{
	curInterpolator = -1.0f;
	for(int h = 0; h < 24; h++)
		for(int w = 0; w < NUMWEATHERS; w++)
			data[h][w] = init;
}

void
InterpolatedFloat::Read(char *s, int line, int field)
{
	sscanf(s, "%f", &data[line][field]);
}

float
InterpolatedFloat::Get(void)
{
	float key = (float)(CClock::GetHours() * 3600 + CClock::GetMinutes() * 60 + CClock::GetSeconds());
	if(curInterpolator != key){
		INTERP_SETUP
		curVal = INTERP(data);
		curInterpolator = key;
	}
	return curVal;
}

InterpolatedColor::InterpolatedColor(const Color &init)
{
	curInterpolator = -1.0f;
	for(int h = 0; h < 24; h++)
		for(int w = 0; w < NUMWEATHERS; w++)
			data[h][w] = init;
}

void
InterpolatedColor::Read(char *s, int line, int field)
{
	int r, g, b, a;
	sscanf(s, "%i, %i, %i, %i", &r, &g, &b, &a);
	data[line][field] = Color(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
}

Color
InterpolatedColor::Get(void)
{
	float key = (float)(CClock::GetHours() * 3600 + CClock::GetMinutes() * 60 + CClock::GetSeconds());
	if(curInterpolator != key){
		INTERP_SETUP
		curVal.r = INTERPF(data, r);
		curVal.g = INTERPF(data, g);
		curVal.b = INTERPF(data, b);
		curVal.a = INTERPF(data, a);
		curInterpolator = key;
	}
	return curVal;
}

void
InterpolatedLight::Read(char *s, int line, int field)
{
	int r, g, b, a;
	sscanf(s, "%i, %i, %i, %i", &r, &g, &b, &a);
	data[line][field] = Color(r/255.0f, g/255.0f, b/255.0f, a/100.0f);
}

char*
ReadTweakValueTable(char *fp, InterpolatedValue &interp)
{
	char buf[24], *p;
	int c;
	int line, field;

	line = 0;
	c = *fp++;
	while(c != '\0' && line < 24){
		field = 0;
		if(c != '\0' && c != '#'){
			while(c != '\0' && c != '\n' && field < NUMWEATHERS){
				p = buf;
				while(c != '\0' && c == '\t')
					c = *fp++;
				*p++ = c;
				while(c = *fp++, c != '\0' && c != '\t' && c != '\n')
					*p++ = c;
				*p++ = '\0';
				interp.Read(buf, line, field);
				field++;
			}
			line++;
		}
		while(c != '\0' && c != '\n')
			c = *fp++;
		c = *fp++;
	}
	return fp-1;
}



/*
 * Neo Vehicle pipe
 */

int32 VehiclePipeSwitch = VEHICLEPIPE_NEO;
float VehicleShininess = 1.0f;
float VehicleSpecularity = 1.0f;
InterpolatedFloat Fresnel(0.4f);
InterpolatedFloat Power(18.0f);
InterpolatedLight DiffColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
InterpolatedLight SpecColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
rw::ObjPipeline *vehiclePipe;

void
AttachVehiclePipe(rw::Atomic *atomic)
{
	atomic->pipeline = vehiclePipe;
}

void
AttachVehiclePipe(rw::Clump *clump)
{
	FORLIST(lnk, clump->atomics)
		AttachVehiclePipe(rw::Atomic::fromClump(lnk));
}



/*
 * Neo World pipe
 */

bool LightmapEnable;
float LightmapMult = 1.0f;
InterpolatedFloat WorldLightmapBlend(1.0f);
rw::ObjPipeline *worldPipe;

void
AttachWorldPipe(rw::Atomic *atomic)
{
	atomic->pipeline = worldPipe;
}

void
AttachWorldPipe(rw::Clump *clump)
{
	FORLIST(lnk, clump->atomics)
		AttachWorldPipe(rw::Atomic::fromClump(lnk));
}




/*
 * Neo Gloss pipe
 */

bool GlossEnable;
float GlossMult = 1.0f;
rw::ObjPipeline *glossPipe;

rw::Texture*
GetGlossTex(rw::Material *mat)
{
	if(neoTxd == nil)
		return nil;
	CustomMatExt *ext = GetCustomMatExt(mat);
	if(!ext->haveGloss){
		char glossname[128];
		strcpy(glossname, mat->texture->name);
		strcat(glossname, "_gloss");
		ext->glossTex = neoTxd->find(glossname);
		ext->haveGloss = true;
	}
	return ext->glossTex;
}

void
AttachGlossPipe(rw::Atomic *atomic)
{
	atomic->pipeline = glossPipe;
}

void
AttachGlossPipe(rw::Clump *clump)
{
	FORLIST(lnk, clump->atomics)
		AttachWorldPipe(rw::Atomic::fromClump(lnk));
}



/*
 * Neo Rim pipes
 */

bool RimlightEnable;
float RimlightMult = 1.0f;
InterpolatedColor RampStart(Color(0.0f, 0.0f, 0.0f, 1.0f));
InterpolatedColor RampEnd(Color(1.0f, 1.0f, 1.0f, 1.0f));
InterpolatedFloat Offset(0.5f);
InterpolatedFloat Scale(1.5f);
InterpolatedFloat Scaling(2.0f);
rw::ObjPipeline *rimPipe;
rw::ObjPipeline *rimSkinPipe;

void
AttachRimPipe(rw::Atomic *atomic)
{
	if(rw::Skin::get(atomic->geometry))
		atomic->pipeline = rimSkinPipe;
	else
		atomic->pipeline = rimPipe;
}

void
AttachRimPipe(rw::Clump *clump)
{
	FORLIST(lnk, clump->atomics)
		AttachRimPipe(rw::Atomic::fromClump(lnk));
}

/*
 * High level stuff
 */

void
CustomPipeInit(void)
{
	RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, "neo/neo.txd");
	if(stream == nil)
		printf("Error: couldn't open 'neo/neo.txd'\n");
	else{
		if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nil, nil))
			neoTxd = RwTexDictionaryGtaStreamRead(stream);
		RwStreamClose(stream, nil);
	}

	EnvMapInit();

	CreateVehiclePipe();
	CreateWorldPipe();
	CreateGlossPipe();
	CreateRimLightPipes();
}

void
CustomPipeShutdown(void)
{
	DestroyVehiclePipe();
	DestroyWorldPipe();
	DestroyGlossPipe();
	DestroyRimLightPipes();

	EnvMapShutdown();

	if(neoTxd){
		neoTxd->destroy();
		neoTxd = nil;
	}
}

void
CustomPipeRegister(void)
{
#ifdef RW_OPENGL
	CustomPipeRegisterGL();
#endif

	CustomMatOffset = rw::Material::registerPlugin(sizeof(CustomMatExt), MAKECHUNKID(rwVENDORID_ROCKSTAR, 0x80),
		CustomMatCtor, nil, CustomMatCopy);
	rw::Material::registerPluginStream(0x0253F2FC, SaCarEnvMatRead, nil, nil);
	rw::Material::registerPluginStream(0x0253F2F6, SaCarSpecMatRead, nil, nil);
#ifdef CUSTOM_MODELS
	SAStaticModel::RegisterPlugins();
#endif
}


// Load textures from TXD parent chain / vehicle.txd / generic as fallback

rw::TexDictionary *genericTxd;
rw::Texture *(*defaultFindCB)(const char *name);

static rw::Texture*
customFindCB(const char *name)
{
	rw::Texture *res = defaultFindCB(name);
	if(res == nil)
		res = CTxdStore::FindTextureInHierarchy(CTxdStore::GetCurrentTxdSlot(), name);
	if(res == nil && genericTxd)
		res = genericTxd->find(name);
	return res;
}

void
SetTxdFindCallback(void)
{
	int slot = CTxdStore::FindTxdSlot("generic");
	CTxdStore::AddRef(slot);
	// TODO: function for this
	genericTxd = CTxdStore::GetSlot(slot)->texDict;
	assert(genericTxd);
	if(defaultFindCB == nil)
		defaultFindCB = rw::Texture::findCB;
	rw::Texture::findCB = customFindCB;
}

}

#endif
