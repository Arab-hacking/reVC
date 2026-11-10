#include "common.h"

#ifdef RW_GL3

#include "main.h"
#include "Timer.h"
#include "Camera.h"
#include "Renderer.h"
#include "RwHelper.h"
#include "Timecycle.h"
#include "Weather.h"
#include "World.h"
#include "ParaboloidEnvMap.h"
#ifdef ANDROID
#include "Frontend.h"
#endif

// renderer/ParaboloidEnvMap — параболоидальная карта окружения клиента.
//
// Архив (глава 13, 0x42b527) доказывает механизм:
//   vec3 ReflVector = Vertex.xyz - u_eye.xyz;
//   vec3 ReflPos = normalize(ReflVector);
//   ReflPos.xy = normalize(ReflPos.xy) * (ReflPos.z * 0.5 + 0.5);
//   gl_Position = vec4(ReflPos.xy, length(ReflVector) * 0.002, 1.0);
//
// Позиция вершины считается прямо в вершинном шейдере и сразу попадает в NDC,
// поэтому проходу НЕ нужны ни матрица вида, ни проекция, ни отдельная камера:
// достаточно кадрового буфера 256x256, флага u_paraboloid и точки захвата,
// которая обязана совпадать с u_eye при выборке.

static RwRaster *pEnvRaster;
static RwTexture *pEnvTexture;
static bool s_ready;
static const char *s_error;
static uint32 s_lastFrame = 0xFFFFFFFFu;

bool
CParaboloidEnvMap::Init(void)
{
	if(s_ready)
		return true;

	s_error = nil;

	pEnvRaster = RwRasterCreate(MAP_SIZE, MAP_SIZE, 0, rwRASTERTYPECAMERATEXTURE);
	if(pEnvRaster == nil){
		s_error = "envmap raster";
		return false;
	}

	pEnvTexture = RwTextureCreate(pEnvRaster);
	if(pEnvTexture == nil){
		s_error = "envmap texture";
		RwRasterDestroy(pEnvRaster);
		pEnvRaster = nil;
		return false;
	}
	RwTextureSetFilterMode(pEnvTexture, rwFILTERLINEAR);
	RwTextureSetAddressing(pEnvTexture, rwTEXTUREADDRESSCLAMP);
	RwTextureSetAddressingV(pEnvTexture, rwTEXTUREADDRESSCLAMP);

	s_ready = true;
	return true;
}

void
CParaboloidEnvMap::Shutdown(void)
{
	if(pEnvTexture){
		// The raster is owned by the texture, exactly as in game code.
		RwTextureDestroy(pEnvTexture);
		pEnvTexture = nil;
	}
	pEnvRaster = nil;
	s_ready = false;
	s_lastFrame = 0xFFFFFFFFu;
}

bool
CParaboloidEnvMap::IsReady(void)
{
	return s_ready && pEnvTexture != nil;
}

const char *
CParaboloidEnvMap::GetError(void)
{
	return s_error;
}

int32
CParaboloidEnvMap::UpdateStride(void)
{
#ifdef ANDROID
	// Карта дешёвая (256x256), но полный обход сцены — нет: на слабых
	// телефонах обновляем реже, на высоком уровне графики — каждый второй кадр.
	if(FrontEndMenuManager.m_PrefsMobileGraphics >= CMenuManager::MOBILE_GFX_HIGH)
		return 2;
	if(FrontEndMenuManager.m_PrefsMobileGraphics == CMenuManager::MOBILE_GFX_MEDIUM)
		return 3;
	return 5;
#else
	return 1;
#endif
}

void
CParaboloidEnvMap::Invalidate(void)
{
	s_lastFrame = 0xFFFFFFFFu;
}

void *
CParaboloidEnvMap::GetTexture(void)
{
	return pEnvTexture;
}

bool
CParaboloidEnvMap::Capture(void (*renderScene)(void), int32 stride)
{
	if(!Init() || pEnvRaster == nil || pEnvTexture == nil || renderScene == nil)
		return false;

	uint32 frame = CTimer::GetFrameCounter();
	if(stride < 1)
		stride = 1;
	if(s_lastFrame != 0xFFFFFFFFu && (uint32)(frame - s_lastFrame) < (uint32)stride)
		return true;

	uint32 fbo = rw::gl3::rasterFramebuffer(pEnvRaster);
	if(fbo == 0)
		return false;

	// Точка захвата — позиция игровой камеры: при выборке направление
	// отражения отсчитывается от той же u_eye, иначе карта «съедет».
	CVector pos = TheCamera.GetPosition();
	rw::V3d eye;
	eye.x = pos.x;
	eye.y = pos.y;
	eye.z = pos.z;

	// Небо — среднее между верхом и низом, как в клиенте (13.1).
	RwRGBA sky;
	sky.red   = (uint8)((CTimeCycle::GetSkyTopRed()   + CTimeCycle::GetSkyBottomRed())   / 2);
	sky.green = (uint8)((CTimeCycle::GetSkyTopGreen() + CTimeCycle::GetSkyBottomGreen()) / 2);
	sky.blue  = (uint8)((CTimeCycle::GetSkyTopBlue()  + CTimeCycle::GetSkyBottomBlue())  / 2);
	sky.alpha = 255;

	if(!rw::gl3::beginParaboloidPass(fbo, MAP_SIZE, MAP_SIZE, &sky))
		return false;

	rw::gl3::setCaptureEyePosition(eye);

	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)FALSE);

	renderScene();

	rw::gl3::endParaboloidPass();
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)TRUE);

	s_lastFrame = frame;
	return true;
}

#endif // RW_GL3
