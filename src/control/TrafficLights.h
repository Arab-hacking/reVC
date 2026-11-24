#ifndef __GTA_TRAFFICLIGHTS_H__
#define __GTA_TRAFFICLIGHTS_H__

class CEntity;

enum {
	CAR_LIGHTS_GREEN = 0,
	CAR_LIGHTS_YELLOW,
	CAR_LIGHTS_RED,
	CAR_LIGHTS_NONE
};

class CTrafficLights
{
public:
	static bool bGreenLightsCheat;

	static void DisplayActualLight(CEntity *ent);
	static int FindTrafficLightType(CEntity *light);
	static uint8 LightForCars1(void);
	static uint8 LightForCars2(void);
	static uint8 LightForCars1_Visual(void);
	static uint8 LightForCars2_Visual(void);
};

#endif // __GTA_TRAFFICLIGHTS_H__
