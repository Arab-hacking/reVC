enum {
	WEATHER_RANDOM = -1,
	WEATHER_SUNNY = 0,
	WEATHER_CLOUDY,
	WEATHER_RAINY,
	WEATHER_FOGGY,
	WEATHER_EXTRA_SUNNY,
	WEATHER_HURRICANE,
	WEATHER_TOTAL,

	WEATHER_EXTRACOLOURS = 6
};

#include <string>
#include <vector>

class CWeather
{
public:
	static int32 SoundHandle;

	static int32 WeatherTypeInList;
	static int16 OldWeatherType;
	static int16 NewWeatherType;
	static int16 ForcedWeatherType;

	static bool LightningFlash;
	static bool LightningBurst;
	static uint32 LightningStart;
	static uint32 LightningFlashLastChange;
	static uint32 WhenToPlayLightningSound;
	static uint32 LightningDuration;
	static int32 StreamAfterRainTimer;

	static float ExtraSunnyness;
	static float Foggyness;
	static float CloudCoverage;
	static float Wind;
	static float Rain;
	static float InterpolationValue;
	static float WetRoads;
	static float Rainbow;
	static float SunGlare;
	static float WindClipped;
	static float TrafficLightBrightness;

	static bool bScriptsForceRain;

	static void RenderRainStreaks(void);
	static void Update(void);
	static void Init(void);
	
	static void ReleaseWeather();
	static void ForceWeather(int16);
	static void ForceWeatherNow(int16);
	static void AddSplashesDuringHurricane();
	static void AddStreamAfterRain();
	static void AddRain();
	static void AddHeatHaze();
	static void AddBeastie();

	static void ForceHurricaneWeather();

// BR snow configuration (from weather.json)
static bool SnowEnabled;
static std::string SnowTexture;
static float SnowColor[4];
static float SnowAmount;
static float SnowTile;
static std::vector<std::string> SnowExcludeTextures;

// BR weather.json integration
static void ApplyWeatherJson(const uint8_t* data, size_t size);
};

enum {
	NUM_RAIN_STREAKS = 35
};

struct tRainStreak
{
	CVector position;
	CVector direction;
	uint32 timer;
};

extern RwTexture* gpRainDropTex;
