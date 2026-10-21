// Graphics-only ZIP policy and BTX preview guard: pure CPU, no game files.
#include "../tools/custom-folder-verification/shim/common.h"
#include "GraphicsBtx.h"
#include "brtex.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

static void U32(std::vector<uint8_t> &bytes, size_t offset, uint32_t x)
{
    bytes[offset] = (uint8_t)x;
    bytes[offset+1] = (uint8_t)(x >> 8);
    bytes[offset+2] = (uint8_t)(x >> 16);
    bytes[offset+3] = (uint8_t)(x >> 24);
}
static std::vector<uint8_t> Chunk(uint32_t kind, const std::vector<uint8_t> &body)
{
    std::vector<uint8_t> chunk(12 + body.size(), 0);
    U32(chunk, 0, kind); U32(chunk, 4, (uint32_t)body.size());
    U32(chunk, 8, 0x1803FFFF); // RenderWare 3.6 stream header
    std::memcpy(chunk.data() + 12, body.data(), body.size());
    return chunk;
}
static void Append(std::vector<uint8_t> &to, const std::vector<uint8_t> &from)
{
    to.insert(to.end(), from.begin(), from.end());
}
static std::vector<uint8_t> DualGeometry(const char *name, unsigned uvSets, bool dual)
{
    std::vector<uint8_t> texStruct(4, 0);
    std::vector<uint8_t> texture = Chunk(1, texStruct);
    Append(texture, Chunk(2, std::vector<uint8_t>(name, name + std::strlen(name) + 1)));
    Append(texture, Chunk(2, std::vector<uint8_t>(4, 0)));
    std::vector<uint8_t> matfx(20, 0);
    U32(matfx, 0, dual ? 4 : 2); // DUAL or ENVMAP; only DUAL is a lightmap
    U32(matfx, 4, dual ? 4 : 2);
    U32(matfx, 16, 1); // source/destination blend (or coefficient) + hasTexture
    Append(matfx, Chunk(6, texture));
    std::vector<uint8_t> extension = Chunk(0x120, matfx);
    std::vector<uint8_t> material = Chunk(3, extension);
    std::vector<uint8_t> geometry = Chunk(1, std::vector<uint8_t>(16, 0));
    U32(geometry, 12, (uvSets << 16) | (uvSets >= 2 ? 0x80u : 0x04u));
    Append(geometry, Chunk(8, Chunk(7, material)));
    return Chunk(0x0F, geometry);
}
static std::vector<uint8_t> RawBtx(uint32_t w, uint32_t h)
{
    std::vector<uint8_t> file(4 + 64 + 4 + (size_t)w*h*4, 0);
    const uint8_t magic[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };
    std::memcpy(file.data() + 4, magic, 12);
    U32(file, 4+12, 0x04030201); U32(file, 4+16, 0x1401);
    U32(file, 4+20, 1); U32(file, 4+24, 0x1908);
    U32(file, 4+28, 0x8058); U32(file, 4+32, 0x1908);
    U32(file, 4+36, w); U32(file, 4+40, h);
    U32(file, 4+52, 1); U32(file, 4+56, 1);
    U32(file, 4+64, w*h*4);
    file[4+68+3] = 128; // BR lightmap alpha carries the blend strength
    return file;
}

int main()
{
    assert(graphicsbtx::Key("Vehicles\\CARReflectionMask.BTX") == "carreflectionmask");
    assert(graphicsbtx::IsCarMask("vehicles/CARReflectionMask.BTX"));
    assert(graphicsbtx::IsReflection("vehicles/vehicle_reflection.btx"));
    assert(graphicsbtx::IsVehicleReflection("vehicles/vehicle_reflection.btx"));
    assert(!graphicsbtx::IsCarMask("water_reflection.btx"));
    assert(!graphicsbtx::IsVehicleReflection("water_reflection.btx"));
    assert(!graphicsbtx::IsReflection("house_diffuse.btx"));
    assert(graphicsbtx::IsLightmap("baked/window_lightmap.BTX"));
    assert(graphicsbtx::IsLightmap("building_LM.btx"));
    assert(graphicsbtx::IsLightmap("LM_street.btx"));
    assert(!graphicsbtx::IsLightmap("house_diffuse.btx"));
    assert(!graphicsbtx::IsLightmap("emissive_skin.btx"));
    assert(graphicsbtx::MaterialCanUseLightmap(2, true, "window_lightmap"));
    assert(!graphicsbtx::MaterialCanUseLightmap(1, true, "window_lightmap"));
    assert(!graphicsbtx::MaterialCanUseLightmap(2, false, "window_lightmap"));
    assert(!graphicsbtx::MaterialCanUseLightmap(2, true, "window_albedo"));
    assert(graphicsbtx::IsMarkedLightmap(graphicsbtx::LightmapMarker));
    assert(!graphicsbtx::IsMarkedLightmap("generic_dual"));
    assert(!graphicsbtx::FullSize(8, 8) && !graphicsbtx::FullSize(16, 8));
    assert(graphicsbtx::FullSize(16, 16) && graphicsbtx::FullSize(2048, 512));
    assert(graphicsbtx::FullSize(4096, 4096) && !graphicsbtx::FullSize(8192, 8192));

    brtex::Texture preview, full;
    std::vector<uint8_t> tiny = RawBtx(8, 8), image = RawBtx(32, 16);
    std::string error;
    assert(brtex::parseBtx(tiny.data(), tiny.size(), preview, &error));
    assert(brtex::parseBtx(image.data(), image.size(), full, &error));
    assert(!graphicsbtx::FullSize(preview.mips[0].w, preview.mips[0].h));
    assert(graphicsbtx::FullSize(full.mips[0].w, full.mips[0].h));
    assert(full.hasAlpha && full.mips[0].rgba[3] == 128);

    std::vector<uint8_t> model = DualGeometry("roof_lightmap", 2, true);
    Append(model, DualGeometry("uv1_lightmap", 1, true));
    Append(model, DualGeometry("env_lightmap", 2, false));
    std::vector<std::string> selected;
    brtex::collectUv2DualTextureNames(model.data(), model.size(), selected);
    assert(selected.size() == 1 && selected[0] == "roof_lightmap");
    std::puts("zip/ graphics names, UV2 DUAL references and .tmb preview rejection passed");
}
