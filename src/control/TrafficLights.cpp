#include "common.h"

#include "Camera.h"
#include "Clock.h"
#include "Coronas.h"
#include "General.h"
#include "PointLights.h"
#include "Shadows.h"
#include "SpecialFX.h"
#include "Timecycle.h"
#include "Timer.h"
#include "TrafficLights.h"
#include "Vehicle.h"
#include "Weather.h"
#include "World.h"

bool CTrafficLights::bGreenLightsCheat;

void
CTrafficLights::DisplayActualLight(CEntity *ent)
{
	if(ent->GetUp().z < 0.96f || ent->bRenderDamaged)
		return;

	int phase;
	if(FindTrafficLightType(ent) == 1)
		phase = LightForCars1_Visual();
	else
		phase = LightForCars2_Visual();

	int i, m = ent->GetModelIndex();
	if (MI_TRAFFICLIGHTS == m) {
		CBaseModelInfo* mi = CModelInfo::GetModelInfo(ent->GetModelIndex());
		float x = mi->Get2dEffect(0)->pos.x;
		float yMin = mi->Get2dEffect(0)->pos.y;
		float yMax = mi->Get2dEffect(0)->pos.y;
		float zMin = mi->Get2dEffect(0)->pos.z;
		float zMax = mi->Get2dEffect(0)->pos.z;
		for (i = 1; i < 6; i++) {
			assert(mi->Get2dEffect(i));
			yMin = Min(yMin, mi->Get2dEffect(i)->pos.y);
			yMax = Max(yMax, mi->Get2dEffect(i)->pos.y);
			zMin = Min(zMin, mi->Get2dEffect(i)->pos.z);
			zMax = Max(zMax, mi->Get2dEffect(i)->pos.z);
		}

		CVector pos1, pos2;
		uint8 r, g;
		int id;
		switch (phase) {
		case CAR_LIGHTS_GREEN:
			r = 0;
			g = 255;
			pos1 = ent->GetMatrix() * CVector(x, yMax, zMin);
			pos2 = ent->GetMatrix() * CVector(x, yMin, zMin);
			id = 0;
			break;
		case CAR_LIGHTS_YELLOW:
			r = 255;
			g = 128;
			pos1 = ent->GetMatrix() * CVector(x, yMax, (zMin + zMax) / 2.0f);
			pos2 = ent->GetMatrix() * CVector(x, yMin, (zMin + zMax) / 2.0f);
			id = 1;
			break;
		case CAR_LIGHTS_RED:
			r = 255;
			g = 0;
			pos1 = ent->GetMatrix() * CVector(x, yMax, zMax);
			pos2 = ent->GetMatrix() * CVector(x, yMin, zMax);
			id = 2;
			break;
		default:
			r = 0;
			g = 0;
			pos1 = ent->GetMatrix() * CVector(x, yMax, (zMin + zMax) / 2.0f);
			pos2 = ent->GetMatrix() * CVector(x, yMin, (zMin + zMax) / 2.0f);
			id = -1;
			break;
		}

		if (CWeather::TrafficLightBrightness > 0.5f)
			CPointLights::AddLight(CPointLights::LIGHT_POINT,
				pos1, CVector(0.0f, 0.0f, 0.0f), 8.0f,
				r / 255.0f, g / 255.0f, 0 / 255.0f, CPointLights::FOG_NORMAL, true);

		if (CWeather::TrafficLightBrightness > 0.05f)
			CShadows::StoreStaticShadow((uintptr)ent,
				SHADOWTYPE_ADDITIVE, gpShadowExplosionTex, &pos1,
				8.0f, 0.0f, 0.0f, -8.0f, 128,
				r * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				g * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				0 * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				12.0f, 1.0f, 40.0f, false, 0.0f);

		if (DotProduct(TheCamera.GetForward(), ent->GetForward()) < 0.0f)
			CCoronas::RegisterCorona((uintptr)ent + id,
				r * CTimeCycle::GetSpriteBrightness() * 0.7f,
				g * CTimeCycle::GetSpriteBrightness() * 0.7f,
				0 * CTimeCycle::GetSpriteBrightness() * 0.7f,
				255,
				pos1, 1.75f * CTimeCycle::GetSpriteSize(), 50.0f,
				CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
				CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, 0.0f);
		else
			CCoronas::RegisterCorona((uintptr)ent + id + 3,
				r * CTimeCycle::GetSpriteBrightness() * 0.7f,
				g * CTimeCycle::GetSpriteBrightness() * 0.7f,
				0 * CTimeCycle::GetSpriteBrightness() * 0.7f,
				255,
				pos2, 1.75f * CTimeCycle::GetSpriteSize(), 50.0f,
				CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
				CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, 0.0f);

		// SA CTrafficLights::DisplayActualLight guards this with
		// `iLightState < eTrafficLightsState::LIGHT_OFF`: an off light must not be
		// drawn at all. Without the guard id 3 walks past the traffic-light
		// colours into whatever comes next in the enum. The base colour is also
		// the *small* variant for these short lights, as in SA.
		if (id >= 0) {
			CBrightLights::RegisterOne(pos1, ent->GetUp(), ent->GetRight(), CVector(0.0f, 0.0f, 0.0f), id + BRIGHTLIGHT_GREEN_SMALL);
			CBrightLights::RegisterOne(pos2, ent->GetUp(), -ent->GetRight(), CVector(0.0f, 0.0f, 0.0f), id + BRIGHTLIGHT_GREEN_SMALL);
		}
	}
	else if (MI_TRAFFICLIGHTS_VERTICAL == m) {
		CBaseModelInfo* mi = CModelInfo::GetModelInfo(ent->GetModelIndex());
		float x = mi->Get2dEffect(0)->pos.x;
		float yMin = mi->Get2dEffect(0)->pos.y;
		float yMax = mi->Get2dEffect(0)->pos.y;
		float zMin = mi->Get2dEffect(0)->pos.z;
		float zMax = mi->Get2dEffect(0)->pos.z;
		for (i = 1; i < 6; i++) {
			assert(mi->Get2dEffect(i));
			yMin = Min(yMin, mi->Get2dEffect(i)->pos.y);
			yMax = Max(yMax, mi->Get2dEffect(i)->pos.y);
			zMin = Min(zMin, mi->Get2dEffect(i)->pos.z);
			zMax = Max(zMax, mi->Get2dEffect(i)->pos.z);
		}

		CVector pos1;
		uint8 r, g;
		int id;
		switch (phase) {
		case CAR_LIGHTS_GREEN:
			r = 0;
			g = 255;
			pos1 = ent->GetMatrix() * mi->Get2dEffect(2)->pos;
			id = 0;
			break;
		case CAR_LIGHTS_YELLOW:
			r = 255;
			g = 128;
			pos1 = ent->GetMatrix() * mi->Get2dEffect(1)->pos;
			id = 1;
			break;
		case CAR_LIGHTS_RED:
			r = 255;
			g = 0;
			pos1 = ent->GetMatrix() * mi->Get2dEffect(0)->pos;
			id = 2;
			break;
		default:
			r = 0;
			g = 0;
			pos1 = ent->GetMatrix() * mi->Get2dEffect(1)->pos;
			id = -1;
			break;
		}

		if (id >= 0)
			CBrightLights::RegisterOne(pos1, ent->GetUp(), ent->GetRight(), CVector(0.0f, 0.0f, 0.0f), id + BRIGHTLIGHT_GREEN_SMALL);

		if (CWeather::TrafficLightBrightness > 0.5f)
			CPointLights::AddLight(CPointLights::LIGHT_POINT,
				pos1, CVector(0.0f, 0.0f, 0.0f), 8.0f,
				r / 255.0f, g / 255.0f, 0 / 255.0f, CPointLights::FOG_NORMAL, true);

		if (CWeather::TrafficLightBrightness > 0.05f)
			CShadows::StoreStaticShadow((uintptr)ent,
				SHADOWTYPE_ADDITIVE, gpShadowExplosionTex, &pos1,
				8.0f, 0.0f, 0.0f, -8.0f, 128,
				r * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				g * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				0 * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				12.0f, 1.0f, 40.0f, false, 0.0f);

		if (DotProduct(TheCamera.GetForward(), ent->GetForward()) < 0.0f)
			CCoronas::RegisterCorona((uintptr)ent + id,
				r * CTimeCycle::GetSpriteBrightness() * 0.7f,
				g * CTimeCycle::GetSpriteBrightness() * 0.7f,
				0 * CTimeCycle::GetSpriteBrightness() * 0.7f,
				255,
				pos1, 1.75f * CTimeCycle::GetSpriteSize(), 50.0f,
				CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
				CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, 0.0f);
	}
	else if (MI_TRAFFICLIGHTS_MIAMI == m || MI_TRAFFICLIGHTS_TWOVERTICAL == m) {
		CBaseModelInfo* mi = CModelInfo::GetModelInfo(ent->GetModelIndex());
		CVector pos1, pos2;
		uint8 r, g;
		int id;
		if (MI_TRAFFICLIGHTS_MIAMI == m) {
			switch (phase) {
			case CAR_LIGHTS_GREEN:
				r = 0;
				g = 255;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(4)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(5)->pos;
				id = 0;
				break;
			case CAR_LIGHTS_YELLOW:
				r = 255;
				g = 128;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(2)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(3)->pos;
				id = 1;
				break;
			case CAR_LIGHTS_RED:
				r = 255;
				g = 0;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(0)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(1)->pos;
				id = 2;
				break;
			default:
				r = 0;
				g = 0;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(2)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(3)->pos;
				id = -1;
				break;
			}
		}
		else {
			switch (phase) {
			case CAR_LIGHTS_GREEN:
				r = 0;
				g = 255;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(2)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(5)->pos;
				id = 0;
				break;
			case CAR_LIGHTS_YELLOW:
				r = 255;
				g = 128;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(1)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(4)->pos;
				id = 1;
				break;
			case CAR_LIGHTS_RED:
				r = 255;
				g = 0;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(0)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(3)->pos;
				id = 2;
				break;
			default:
				r = 0;
				g = 0;
				pos1 = ent->GetMatrix() * mi->Get2dEffect(1)->pos;
				pos2 = ent->GetMatrix() * mi->Get2dEffect(4)->pos;
				id = -1;
				break;
			}
		}

		CVector pos = (pos1 + pos2) / 2;
		if (id >= 0) {
			if (id >= 0)
			CBrightLights::RegisterOne(pos1, ent->GetUp(), ent->GetRight(), CVector(0.0f, 0.0f, 0.0f), id + BRIGHTLIGHT_GREEN_SMALL);
			if (id >= 0)
				CBrightLights::RegisterOne(pos2, ent->GetUp(), ent->GetRight(), CVector(0.0f, 0.0f, 0.0f), id + BRIGHTLIGHT_GREEN_SMALL);
		}

		if (CWeather::TrafficLightBrightness > 0.5f)
			CPointLights::AddLight(CPointLights::LIGHT_POINT,
				pos, CVector(0.0f, 0.0f, 0.0f), 8.0f,
				r / 255.0f, g / 255.0f, 0 / 255.0f, CPointLights::FOG_NORMAL, true);

		if (CWeather::TrafficLightBrightness > 0.05f)
			CShadows::StoreStaticShadow((uintptr)ent,
				SHADOWTYPE_ADDITIVE, gpShadowExplosionTex, &pos,
				8.0f, 0.0f, 0.0f, -8.0f, 128,
				r * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				g * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				0 * CTimeCycle::GetLightOnGroundBrightness() * CWeather::TrafficLightBrightness / 8.0f,
				12.0f, 1.0f, 40.0f, false, 0.0f);

		if (id >= 0) {
			if (DotProduct(TheCamera.GetForward(), ent->GetForward()) > 0.0f)
				CCoronas::RegisterCorona((uintptr)ent + id,
					r * CTimeCycle::GetSpriteBrightness() * 0.7f,
					g * CTimeCycle::GetSpriteBrightness() * 0.7f,
					0 * CTimeCycle::GetSpriteBrightness() * 0.7f,
					255,
					pos1, 1.75f * CTimeCycle::GetSpriteSize(), 50.0f,
					CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, 0.0f);
			else
				CCoronas::RegisterCorona((uintptr)ent + id,
					r * CTimeCycle::GetSpriteBrightness() * 0.7f,
					g * CTimeCycle::GetSpriteBrightness() * 0.7f,
					0 * CTimeCycle::GetSpriteBrightness() * 0.7f,
					255,
					pos2, 1.75f * CTimeCycle::GetSpriteSize(), 50.0f,
					CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, 0.0f);
		}
	}
}

int
CTrafficLights::FindTrafficLightType(CEntity *light)
{
	float orientation = RADTODEG(CGeneral::GetATanOfXY(light->GetForward().x, light->GetForward().y));
	if((orientation > 60.0f && orientation < 60.0f + 90.0f) ||
	   (orientation > 240.0f && orientation < 240.0f + 90.0f))
		return 1;
	return 2;
}

uint8
CTrafficLights::LightForCars1(void)
{
	if (CWeather::Wind > 1.1f)
		return CAR_LIGHTS_GREEN;

	if (bGreenLightsCheat)
		return CAR_LIGHTS_GREEN;

	uint32 period = CTimer::GetTimeInMilliseconds() % 16384;

	if(period < 5000)
		return CAR_LIGHTS_GREEN;
	else if(period < 5000 + 1000)
		return CAR_LIGHTS_YELLOW;
	else
		return CAR_LIGHTS_RED;
}

uint8
CTrafficLights::LightForCars2(void)
{
	if (CWeather::Wind > 1.1f)
		return CAR_LIGHTS_GREEN;

	if (bGreenLightsCheat)
		return CAR_LIGHTS_GREEN;

	uint32 period = CTimer::GetTimeInMilliseconds() % 16384;

	if(period < 6000)
		return CAR_LIGHTS_RED;
	else if(period < 12000 - 1000)
		return CAR_LIGHTS_GREEN;
	else if(period < 12000)
		return CAR_LIGHTS_YELLOW;
	else
		return CAR_LIGHTS_RED;
}

uint8
CTrafficLights::LightForCars1_Visual(void)
{
	if (CWeather::Wind <= 1.1f)
		return LightForCars1();
	return (CTimer::GetTimeInMilliseconds() & 0x400 ? CAR_LIGHTS_NONE : CAR_LIGHTS_YELLOW);
}

uint8
CTrafficLights::LightForCars2_Visual(void)
{
	if (CWeather::Wind <= 1.1f)
		return LightForCars2();
	return (CTimer::GetTimeInMilliseconds() & 0x400 ? CAR_LIGHTS_NONE : CAR_LIGHTS_YELLOW);
}
