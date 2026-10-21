#ifndef REVC_GRAPHICS_BTX_H
#define REVC_GRAPHICS_BTX_H

// Only metadata from zip/*.zip is indexed. A texture from that folder may be
// DECODED only for an explicitly selected vehicle reflection mask or for a
// lightmap referenced by a material with UV2; it must never become a model,
// skin, collision, diffuse texture, or a standalone replacement TXD.
#include <string>
#include <cctype>
#include <cstring>
#include <stdint.h>

namespace graphicsbtx {

static const char LightmapMarker[] = "revc_zip_lightmap";
inline bool IsMarkedLightmap(const char *mask)
{
	return mask && std::strcmp(mask, LightmapMarker) == 0;
}

inline std::string Key(const std::string &path)
{
	size_t slash = path.find_last_of("/\\");
	std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
	for(size_t i = 0; i < name.size(); i++)
		name[i] = (char)std::tolower((unsigned char)name[i]);
	size_t dot = name.find_last_of('.');
	if(dot != std::string::npos && name.substr(dot) == ".btx") name.resize(dot);
	return name;
}

inline bool IsCarMask(const std::string &name)
{
	const std::string key = Key(name);
	return key == "carreflectionmask" || key == "car_reflection_mask" ||
	       key == "car_refl_mask" || key == "vehiclereflectionmask" ||
	       key == "vehicle_reflection_mask";
}

inline bool IsReflection(const std::string &name)
{
	const std::string key = Key(name);
	return IsCarMask(key) || key.find("reflection") != std::string::npos ||
	       key.find("reflect") != std::string::npos ||
	       key.find("envmap") != std::string::npos ||
	       key.find("_refl") != std::string::npos ||
	       key.compare(0, 5, "refl_") == 0;
}

inline bool IsVehicleReflection(const std::string &name)
{
	const std::string key = Key(name);
	return IsCarMask(key) ||
	       (IsReflection(key) &&
	        (key.compare(0, 4, "car_") == 0 || key.compare(0, 7, "carrefl") == 0 ||
	         key.compare(0, 6, "carenv") == 0 || key.find("vehicle") != std::string::npos ||
	         key.find("veh_") != std::string::npos));
}

inline bool IsLightmap(const std::string &name)
{
	const std::string key = Key(name);
	return key.find("lightmap") != std::string::npos ||
	       key.find("light_map") != std::string::npos ||
	       key.find("lmap") != std::string::npos ||
	       key.compare(0, 3, "lm_") == 0 ||
	       (key.size() > 3 && key.compare(key.size() - 3, 3, "_lm") == 0);
}

inline bool MaterialCanUseLightmap(unsigned numUVs, bool dual, const std::string &name)
{
	return numUVs >= 2 && dual && IsLightmap(name);
}

// Only the marker on the *actual bound texture* proves that it was already
// sourced from zip/. A model's TXD plan may exist while the loaded dictionary
// is still native to an IMG and must be augmented, not skipped.
inline bool NeedsLightmapReplacement(unsigned numUVs, bool dual, const std::string &name,
                                     const char *boundMask)
{
	return MaterialCanUseLightmap(numUVs, dual, name) && !IsMarkedLightmap(boundMask);
}

// .tmb ZIPs described in the BR report contain 8x8 preview BTX entries. Do
// not make them look like high-resolution lightmaps or reflection masks.
inline bool FullSize(uint32_t w, uint32_t h)
{
	// At most a 4096x4096-equivalent RGBA raster for one optional effect.
	// This is a per-image safety guard, NOT an increase in any cache budget.
	return w >= 16 && h >= 16 && w <= 8192 && h <= 8192 &&
	       (uint64_t)w * h <= 4096u * 4096u;
}

} // namespace graphicsbtx
#endif
