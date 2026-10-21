#include "common.h"
#include "CustomWorld.h"

#ifdef CUSTOM_MODELS

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <cctype>

#include "BrCollisionFormat.h"
#include "CustomColFormat.h"
#include "CustomModels.h"
#include "CustomWorldFormat.h"
#include "ModelNameIndex.h"
#include "SavehDiag.h"
#include "ModelInfo.h"
#include "TimeModelInfo.h"
#include "PedType.h"
#include "AnimManager.h"
#include "HandlingMgr.h"
#ifdef EXTENDED_PIPELINES
#include "custompipes.h"
#endif
#include "FileLoader.h"
#include "TxdStore.h"
#include "TempColModels.h"
#include "ColModel.h"
#include "Pools.h"
#include "Streaming.h"
#include "World.h"

namespace {

// CEntity::m_modelIndex is signed int16; BR IDs such as 42927 cannot be used
// as indices. Leave the stock VC ID range intact and pack BR models into
// unused int16 slots. Neither the original BR ID nor LOD indices are ever
// handed directly to CModelInfo or CStreaming.
const int FIRST_BR_SLOT = 4900;
std::map<int, int> modelSlots;
std::map<std::string, int> nameSlots;
std::vector<bool> brSlots; // allocated after the game's heap is initialized
bool ideLoaded = false, collisionLoaded = false;
#ifdef EXTENDED_PIPELINES
bool graphicsActive = false;
int32 previousVehiclePipe = CustomPipes::VEHICLEPIPE_MATFX;
bool previousLightmap = false;
#endif

struct IdeContext : customworld::ModelIndex {
    unsigned files, parsed, bad, overlong;
    IdeContext() : files(0), parsed(0), bad(0), overlong(0) {}
};

void ReadIde(const char *path, const unsigned char *data, size_t size, void *user)
{
    IdeContext &ctx = *(IdeContext *)user;
    ctx.files++;
    customworld::Stats stats = customworld::ParseIde(data, size,
        [&ctx](const customworld::Model &m) { ctx.Add(m); });
    ctx.parsed += (unsigned)stats.records;
    ctx.bad += (unsigned)stats.rejected;
    ctx.overlong += (unsigned)stats.overlong;
    if(stats.rejected || stats.overlong)
        CUSTOM_LOG("BR IDE %s: %u accepted, %u invalid, %u long line(s)\n",
            path, (unsigned)stats.records, (unsigned)stats.rejected, (unsigned)stats.overlong);
}

struct ColContext {
    unsigned installed, replaced, invalid, missing, full;
    int freeModels;
    ColContext() : installed(0), replaced(0), invalid(0), missing(0), full(0), freeModels(0) {}
};

// A complete, checked VC COLL block is passed here by both archive formats.
// These models own their collisions independently of RW object streaming.
void InstallCollision(const char *name, const uint8_t *game, size_t size, ColContext &ctx)
{
    std::string key = customworld::Lower(customworld::ModelName(name));
    std::map<std::string, int>::const_iterator it = nameSlots.find(key);
    if(it == nameSlots.end()) { ctx.missing++; return; }
    CBaseModelInfo *mi = CModelInfo::GetModelInfo(it->second);
    if(!mi || !mi->IsBuilding()) return;
    std::vector<customcolformat::Block> blocks;
    if(customcolformat::Index(game, size, blocks) || blocks.size() != 1 ||
       customworld::Lower(customworld::ModelName(blocks[0].name)) != key ||
       !customcolformat::ValidVC(game, size)) { ctx.invalid++; return; }
    bool replacing = mi->DoesOwnColModel() && mi->GetColModel() != nil;
    if(ctx.freeModels + (int)replacing <= 100) { ctx.full++; return; }
    if(replacing) { ctx.freeModels++; ctx.replaced++; }
    mi->DeleteCollisionModel();
    CColModel *col = new CColModel;
    col->level = 0; // not an IMG collision slot that the engine may evict
    char modelName[24];
    std::snprintf(modelName, sizeof(modelName), "%s", name);
    CFileLoader::LoadCollisionModel((uint8_t *)game+32, *col, modelName);
    mi->SetColModel(col, true);
    ctx.freeModels--;
    ctx.installed++;
}

void ReadCollision(const char *name, const unsigned char *block, size_t size, void *user)
{
    ColContext &ctx = *(ColContext *)user;
    std::map<std::string, int>::const_iterator it =
        nameSlots.find(customworld::Lower(customworld::ModelName(name)));
    if(it == nameSlots.end()) { ctx.missing++; return; }
    CBaseModelInfo *mi = CModelInfo::GetModelInfo(it->second);
    if(!mi || !mi->IsBuilding()) return;
    std::vector<uint8_t> game;
    if(!brcollision::Convert(block, size, game)) { ctx.invalid++; return; }
    InstallCollision(name, game.data(), game.size(), ctx);
}

void ReadGameCollision(const char *name, const unsigned char *block, size_t size, void *user)
{
    InstallCollision(name, block, size, *(ColContext *)user);
}

bool HasWorldCollisionTarget(const char *name, void *)
{
    std::map<std::string, int>::const_iterator it =
        nameSlots.find(customworld::Lower(customworld::ModelName(name)));
    return it != nameSlots.end() && CModelInfo::GetModelInfo(it->second)->IsBuilding();
}

struct IplContext {
    unsigned files, parsed, bad, overlong, placed, duplicates, linkedLod, noModel, noAsset, outside, capacity;
    std::unordered_set<customworld::PlacementKey, customworld::PlacementHash> seen;
    std::map<std::string, unsigned> unknownNames, missingDffNames;
    customimg::ModelNameIndex nativeBuildings;
    IplContext() : files(0), parsed(0), bad(0), overlong(0), placed(0), duplicates(0), linkedLod(0),
                   noModel(0), noAsset(0), outside(0), capacity(0), nativeBuildings(MAX_MODEL_NAME) {
        // A partial IMG pack can leave most of the 100k IPL placements without
        // a new IDE slot. Searching 32k model-info entries for each miss would
        // be quadratic. Resolve by *name* among already registered VC buildings;
        // unlike source IPL IDs, these slots can safely reach CFileLoader.
        nativeBuildings.Reserve(FIRST_BR_SLOT);
        for(int id = 0; id < FIRST_BR_SLOT; id++) {
            CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
            if(mi && mi->IsBuilding()) nativeBuildings.Add(mi->GetModelName(), id);
        }
    }
};

int FindSlot(const customworld::Instance &inst,
             const customimg::ModelNameIndex *nativeBuildings = nullptr)
{
    std::string key = customworld::Lower(inst.name);
    std::map<int, int>::const_iterator id = modelSlots.find(inst.id);
    if(id != modelSlots.end() &&
       customworld::Lower(CModelInfo::GetModelInfo(id->second)->GetModelName()) == key)
        return id->second;
    // A missing IDE id may still be identified by its name; never pass an
    // untranslated BR numeric ID into the native VC model-info array.
    std::map<std::string, int>::const_iterator named = nameSlots.find(key);
    if(named != nameSlots.end()) return named->second;
    // An IPL can legitimately name a VC building absent from the selected
    // common/ IDE. Do not rewire its LOD metadata; placements alone may use
    // the pre-indexed stock building slot. PlaceInstance still checks DFF and
    // collision. Never use an unregistered custom BR slot or its source ID.
    return nativeBuildings ? nativeBuildings->Find(inst.name.c_str()) : -1;
}

void LinkLods(const std::vector<customworld::Instance> &instances, IplContext &ctx)
{
    for(size_t n = 0; n < instances.size(); n++) {
        const customworld::Instance &near = instances[n];
        if(near.lod < 0 || (size_t)near.lod >= instances.size() || (size_t)near.lod == n)
            continue;
        int nearSlot = FindSlot(near), farSlot = FindSlot(instances[near.lod]);
        if(nearSlot < 0 || farSlot < 0 || nearSlot == farSlot) continue;
        CBaseModelInfo *nearInfo = CModelInfo::GetModelInfo(nearSlot);
        CBaseModelInfo *farInfo = CModelInfo::GetModelInfo(farSlot);
        if(!nearInfo || !farInfo || !nearInfo->IsBuilding() || !farInfo->IsBuilding()) continue;
        CSimpleModelInfo *nearModel = (CSimpleModelInfo *)nearInfo;
        CSimpleModelInfo *farModel = (CSimpleModelInfo *)farInfo;
        farModel->m_isBigBuilding = true;
        farModel->SetRelatedModel(nearModel);
        farModel->m_lodDistances[2] = nearModel->m_lodDistances[0];
        ctx.linkedLod++;
    }
}

void PlaceInstance(const customworld::Instance &inst, IplContext &ctx)
{
    int slot = FindSlot(inst, &ctx.nativeBuildings);
    if(slot < 0) {
        ctx.noModel++;
        ctx.unknownNames[customworld::Lower(inst.name)]++;
        return;
    }

    CBaseModelInfo *mi = CModelInfo::GetModelInfo(slot);
    if(!mi || !mi->IsBuilding() || mi->GetColModel() == nil) {
        ctx.noModel++;
        ctx.unknownNames[customworld::Lower(inst.name)]++;
        return;
    }
    if(!CCustomModels::HasModelFile(mi->GetModelName()) &&
       !CStreaming::ms_aInfoForModel[slot].m_size) {
        ctx.noAsset++;
        ctx.missingDffNames[customworld::Lower(inst.name)]++;
        return;
    }
    CColModel *col = mi->GetColModel();
    float limit[3];
    for(int k = 0; k < 3; k++) {
        float lo = k == 0 ? col->boundingBox.min.x : k == 1 ? col->boundingBox.min.y : col->boundingBox.min.z;
        float hi = k == 0 ? col->boundingBox.max.x : k == 1 ? col->boundingBox.max.y : col->boundingBox.max.z;
        limit[k] = std::max(std::fabs(lo), std::fabs(hi));
    }
    // Reject invalid geometry/coordinates, not positions outside the stock
    // grid: CWorld allocates extra sectors on demand in either direction.
    float radius = std::sqrt(limit[0]*limit[0]+limit[1]*limit[1]+limit[2]*limit[2]) + 1.0f;
    if(!std::isfinite(radius) ||
       !std::isfinite(inst.pos[0]-radius) || !std::isfinite(inst.pos[0]+radius) ||
       !std::isfinite(inst.pos[1]-radius) || !std::isfinite(inst.pos[1]+radius) ||
       WorldSectorIndex(inst.pos[0]-radius, WORLD_MIN_X, SECTOR_SIZE_X) == INT_MIN ||
       WorldSectorIndex(inst.pos[0]+radius, WORLD_MIN_X, SECTOR_SIZE_X) == INT_MAX ||
       WorldSectorIndex(inst.pos[1]-radius, WORLD_MIN_Y, SECTOR_SIZE_Y) == INT_MIN ||
       WorldSectorIndex(inst.pos[1]+radius, WORLD_MIN_Y, SECTOR_SIZE_Y) == INT_MAX) {
        ctx.outside++; return;
    }
    customworld::PlacementKey key(slot, inst);
    if(ctx.seen.find(key) != ctx.seen.end()) { ctx.duplicates++; return; }

    float axis[3], w;
    customworld::ViceAxis(inst, axis, w);
    char vcLine[512];
    int n = std::snprintf(vcLine, sizeof(vcLine),
        "%d %s %d %.9g %.9g %.9g 1 1 1 %.9g %.9g %.9g %.9g",
        slot, mi->GetModelName(), inst.interior, inst.pos[0], inst.pos[1], inst.pos[2],
        axis[0], axis[1], axis[2], w);
    if(n < 0 || n >= (int)sizeof(vcLine)) { ctx.bad++; return; }
    // All BR models installed here have m_objectId == -1, i.e. CBuilding.
    // Grow stable pool blocks BEFORE entering CEntity::Add: it creates one
    // pointer node per covered sector, and those nodes may already be linked
    // from vanilla IPLs. A rotated bbox is contained by this sphere. Leave a
    // small reserve for the rest of the game; physical allocation/handle
    // failure is still reported instead of overflowing a fixed pool.
    double nx = std::ceil(2.0 * radius / SECTOR_SIZE_X) + 2.0;
    double ny = std::ceil(2.0 * radius / SECTOR_SIZE_Y) + 2.0;
    // Bound allocation for a single corrupt/giant collision, not map location.
    if(nx * ny > 8192.0) { ctx.capacity++; return; }
    int nodes = (int)nx * (int)ny;
    if(!CPools::GetBuildingPool()->EnsureFree(101) ||
       !CPools::GetPtrNodePool()->EnsureFree(nodes + 10000)) {
        ctx.capacity++; return;
    }
    CFileLoader::LoadObjectInstance(vcLine);
    ctx.seen.insert(key);
    ctx.placed++;
}

void ReadIpl(const char *path, const unsigned char *data, size_t size, void *user)
{
    IplContext &ctx = *(IplContext *)user;
    ctx.files++;
    std::vector<customworld::Instance> instances;
    customworld::Stats stats = customworld::ParseIpl(data, size,
        [&instances](const customworld::Instance &i) { instances.push_back(i); });
    // IPL LOD fields are zero-based indexes in this file's inst section. A
    // malformed line would shift all subsequent indexes: do not link that
    // file's LODs to the wrong model, but still place its good instances.
    if(!stats.rejected && !stats.overlong) LinkLods(instances, ctx);
    for(size_t n = 0; n < instances.size(); n++) PlaceInstance(instances[n], ctx);
    ctx.parsed += (unsigned)stats.records;
    ctx.bad += (unsigned)stats.rejected;
    ctx.overlong += (unsigned)stats.overlong;
    if(stats.rejected || stats.overlong)
        CUSTOM_LOG("BR IPL %s: %u accepted, %u invalid, %u long line(s)\n",
            path, (unsigned)stats.records, (unsigned)stats.rejected, (unsigned)stats.overlong);
}

ModelInfoType ModelType(customworld::Model::Kind kind)
{
    switch(kind) {
    case customworld::Model::TIME_OBJECT: return MITYPE_TIME;
    case customworld::Model::PED: return MITYPE_PED;
    case customworld::Model::VEHICLE: return MITYPE_VEHICLE;
    case customworld::Model::WEAPON: return MITYPE_WEAPON;
    default: return MITYPE_SIMPLE;
    }
}

// BR uses SA vehicle classes. Only instantiate classes that have a matching
// native VC vehicle implementation; never reinterpret a trailer/quad/BMX as a
// car or install SA handling data into VC's fixed-size handling tables.
int VehicleType(const std::string &s)
{
    const std::string type = customworld::Lower(s);
    if(type == "car") return VEHICLE_TYPE_CAR;
    if(type == "bike") return VEHICLE_TYPE_BIKE;
    if(type == "boat") return VEHICLE_TYPE_BOAT;
    if(type == "heli") return VEHICLE_TYPE_HELI;
    if(type == "plane") return VEHICLE_TYPE_PLANE;
    return -1; // also excludes trains: the BR track/path format is not VC's
}

bool HandlingMatches(int type, int id)
{
    if(id < 0 || id >= NUMHANDLINGS) return false;
    if(type == VEHICLE_TYPE_CAR)
        return id < HANDLING_BIKE && !(mod_HandlingManager.GetHandlingData((tVehicleType)id)->Flags &
            (HANDLING_IS_BIKE | HANDLING_IS_BOAT | HANDLING_IS_HELI | HANDLING_IS_PLANE));
    if(type == VEHICLE_TYPE_BIKE) return id >= HANDLING_BIKE && id <= HANDLING_FREEWAY;
    if(type == VEHICLE_TYPE_BOAT) return id >= HANDLING_PREDATOR && id <= HANDLING_CUPBOAT;
    if(type == VEHICLE_TYPE_HELI)
        return id >= HANDLING_SPARROW && id <= HANDLING_RCCOPTER && id != HANDLING_RCBARON;
    if(type == VEHICLE_TYPE_PLANE)
        return id == HANDLING_SEAPLANE || id == HANDLING_RCBARON;
    return false;
}

int BuiltinHandling(int type, const std::string &name, bool &fallback)
{
    std::string id = name;
    for(char &c : id) c = (char)std::toupper((unsigned char)c);
    int result = mod_HandlingManager.GetHandlingId(id.c_str());
    fallback = !HandlingMatches(type, result);
    if(!fallback) return result;
    switch(type) {
    case VEHICLE_TYPE_BIKE: return HANDLING_BIKE;
    case VEHICLE_TYPE_BOAT: return HANDLING_PREDATOR;
    case VEHICLE_TYPE_HELI: return HANDLING_MAVERICK;
    case VEHICLE_TYPE_PLANE: return HANDLING_SEAPLANE;
    default: return HANDLING_LANDSTAL;
    }
}

ePedType SafePedType(const std::string &s)
{
    std::string type = customworld::Lower(s);
    if(type == "player1" || type == "player") return PEDTYPE_PLAYER1;
    if(type == "player2") return PEDTYPE_PLAYER2;
    if(type == "player3") return PEDTYPE_PLAYER3;
    if(type == "player4") return PEDTYPE_PLAYER4;
    if(type == "civfemale") return PEDTYPE_CIVFEMALE;
    if(type == "cop") return PEDTYPE_COP;
    if(type == "emergency" || type == "medic") return PEDTYPE_EMERGENCY;
    if(type == "fireman") return PEDTYPE_FIREMAN;
    if(type == "criminal") return PEDTYPE_CRIMINAL;
    if(type == "prostitute") return PEDTYPE_PROSTITUTE;
    if(type == "special") return PEDTYPE_SPECIAL;
    if(type.size() == 5 && type.substr(0, 4) == "gang" && type[4] >= '1' && type[4] <= '9')
        return (ePedType)(PEDTYPE_GANG1 + type[4] - '1');
    return PEDTYPE_CIVMALE; // SA's numerous civilian/ped types have no VC counterpart
}

AssocGroupId SafePedAnimGroup(const std::string &s)
{
    std::string group = customworld::Lower(s);
    for(int id = 0; id < NUM_ANIM_ASSOC_GROUPS; id++)
        if(group == customworld::Lower(CAnimManager::GetAnimGroupName((AssocGroupId)id)))
            return (AssocGroupId)id;
    if(group == "man" || group == "default") return ASSOCGRP_STD;
    if(group == "fatman") return ASSOCGRP_FAT;
    if(group == "oldman") return ASSOCGRP_OLD;
    if(group == "fatwoman") return ASSOCGRP_FATWOMAN;
    if(group == "jogger" || group == "jogman") return ASSOCGRP_JOGGER;
    if(group == "skater") return ASSOCGRP_SKATE;
    return ASSOCGRP_STD;
}

bool CanAddModel(ModelInfoType type)
{
    switch(type) {
    case MITYPE_TIME: return CModelInfo::CanAddTimeModel();
    case MITYPE_PED: return CModelInfo::CanAddPedModel();
    case MITYPE_VEHICLE: return CModelInfo::CanAddVehicleModel();
    case MITYPE_WEAPON: return CModelInfo::CanAddWeaponModel();
    default: return CModelInfo::CanAddSimpleModel();
    }
}

void LogUnresolved(const char *reason, const std::map<std::string, unsigned> &counts)
{
    if(counts.empty()) return;
    std::vector<std::pair<unsigned, std::string> > names;
    for(const auto &entry : counts) names.emplace_back(entry.second, entry.first);
    std::sort(names.begin(), names.end(), [](const std::pair<unsigned, std::string> &a,
                                              const std::pair<unsigned, std::string> &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    const size_t shown = std::min(names.size(), size_t(16));
    for(size_t i = 0; i < shown; i++)
        CUSTOM_LOG("BR IPL: %s %s: %u placement(s)\n",
            reason, names[i].second.c_str(), names[i].first);
    if(names.size() > shown)
        CUSTOM_LOG("BR IPL: %s: %u more unique name(s), showing the 16 most frequent\n",
            reason, (unsigned)(names.size() - shown));
}

} // namespace

void CCustomWorld::LoadIde(void)
{
    if(ideLoaded) return; // DEFAULT.DAT and the level DAT each have an IPL boundary
    ideLoaded = true;
    modelSlots.clear();
    nameSlots.clear();
#ifdef EXTENDED_PIPELINES
    const bool commonWorld = CCustomModels::HasCommonWorld();
    const bool reflections = CCustomModels::HasGraphicsReflections();
    const bool lightmaps = CCustomModels::HasGraphicsLightmaps();
    if(commonWorld || reflections || lightmaps) {
        if(!graphicsActive) {
            previousVehiclePipe = CustomPipes::VehiclePipeSwitch;
            previousLightmap = CustomPipes::LightmapEnable;
            graphicsActive = true;
        }
        // zip/ is an isolated graphics index; even without common/ IDE/IPL,
        // select only the effects for which scene assets were discovered.
        if((commonWorld || reflections) && CustomPipes::vehiclePipe &&
           CustomPipes::EnvMapCam && CustomPipes::EnvMapTex) {
            if(reflections) {
                RwTexture *mask = CCustomModels::CarReflectionMaskFromZip();
                if(mask) CustomPipes::SetCarReflectionMask((rw::Texture*)mask);
                else CUSTOM_LOG("zip/ graphics: no full-sized car mask; other reflection .btx cannot replace a dynamic scene envmap\n");
            }
            CustomPipes::VehiclePipeSwitch = CustomPipes::VEHICLEPIPE_NEO;
            CUSTOM_LOG("graphics: vehicle scene RT enabled (Neo projection; BR paraboloid pass not yet implemented)\n");
        } else if(commonWorld || reflections)
            CUSTOM_LOG("graphics: no reflection render target; leaving vehicle MatFX pipeline\n");
        if((commonWorld || lightmaps) && CustomPipes::worldPipe)
            CustomPipes::LightmapEnable = true;
        CUSTOM_LOG("graphics: UV2/DUAL lightmaps %s; native blob shadows use timecycle strengths\n",
            CustomPipes::LightmapEnable ? "enabled where materials support them" : "unavailable");
    }
#endif
    if(!CCustomModels::HasWorldFiles()) return;
    brSlots.assign(MODELINFOSIZE, false);
    IdeContext ctx;
    CCustomModels::ForEachWorldFile(true, ReadIde, &ctx);
    int next = FIRST_BR_SLOT;
    unsigned installed = 0, full = 0, paired = 0;
    std::vector<const customworld::Model *> ordered;
    for(std::map<std::string, customworld::Model>::const_iterator it = ctx.definitions.begin();
        it != ctx.definitions.end(); ++it)
        ordered.push_back(&it->second);
    // Rebind duplicate stock names before allocating int16 slots or consuming
    // a type-specific model store. Existing script IDs and native metadata stay
    // intact; mismatched types must never be loaded through the wrong handler.
    std::unordered_map<std::string, int> existing;
    for(int id = 0; id < MODELINFOSIZE; id++) {
        CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
        if(mi && mi->GetModelName()[0]) existing.emplace(customworld::Lower(mi->GetModelName()), id);
    }
    std::sort(ordered.begin(), ordered.end(), [](const customworld::Model *a,
                                                  const customworld::Model *b) {
        return a->id != b->id ? a->id < b->id : customworld::Lower(a->name) < customworld::Lower(b->name);
    });
    unsigned reusedNative = 0, incompatible = 0, unsupported = 0, noAsset = 0, fallbackHandling = 0;
    for(const customworld::Model *item : ordered) {
        const customworld::Model &m = *item;
        const std::string key = customworld::Lower(m.name);
        const ModelInfoType type = ModelType(m.kind);
        const int vehicleType = type == MITYPE_VEHICLE ? VehicleType(m.subtype) : -1;
        if(type == MITYPE_VEHICLE && vehicleType < 0) { unsupported++; continue; }
        std::unordered_map<std::string, int>::const_iterator old = existing.find(key);
        if(old != existing.end()) {
            CBaseModelInfo *mi = CModelInfo::GetModelInfo(old->second);
            if(mi->GetModelType() != type ||
               (type == MITYPE_VEHICLE && ((CVehicleModelInfo *)mi)->m_vehicleType != vehicleType)) {
                incompatible++;
                continue;
            }
            nameSlots[key] = old->second;
            reusedNative++;
            continue; // keep known-safe native collision, scripts, ped/vehicle data
        }
        // The manifest has more names than the archive has actual assets.
        // An orphan definition would exhaust model/TXD slots without ever
        // appearing. Native models above can still reuse their stock DFFs.
        if(!CCustomModels::HasModelFile(m.name.c_str())) { noAsset++; continue; }
        // A same-name TXD in any user IMG takes precedence over the IDE TXD,
        // even if the DFF lives in another mounted IMG.
        bool hasPair = CCustomModels::PairedImageTxd(m.name.c_str()) >= 0;
        const char *txdName = hasPair ? m.name.c_str() : m.txd.c_str();
        while(next < MODELINFOSIZE && CModelInfo::GetModelInfo(next) != nil) next++;
        if(next == MODELINFOSIZE || !CanAddModel(type) ||
           (CTxdStore::FindTxdSlot(txdName) == -1 && !CTxdStore::HasFreeCustomSlot())) {
            full++;
            continue;
        }
        CBaseModelInfo *mi = nil;
        if(type == MITYPE_TIME) {
            CTimeModelInfo *time = CModelInfo::AddTimeModel(next);
            time->SetTimes(m.timeOn, m.timeOff);
            mi = time;
        } else if(type == MITYPE_SIMPLE) {
            mi = CModelInfo::AddSimpleModel(next);
        } else if(type == MITYPE_PED) {
            CPedModelInfo *ped = CModelInfo::AddPedModel(next);
            ped->m_pedType = SafePedType(m.subtype);
            // SA skeletons select the separate custom animation bank when
            // their DFF is loaded; VC skeletons keep the VC bank. No raw SA
            // stat/group/animation indices are passed to the engine.
            std::string stat = m.pedStats;
            for(char &c : stat) c = (char)std::toupper((unsigned char)c);
            ped->m_pedStatType = CPedStats::GetPedStatType(&stat[0]);
            if(ped->m_pedStatType == NUM_PEDSTATS) ped->m_pedStatType = PEDSTAT_STD_MISSION;
            ped->m_animGroup = SafePedAnimGroup(m.animGroup);
            ped->m_carsCanDrive = 0; // SA vehicle-category mask != VC mask
            ped->radio1 = ped->radio2 = -1;
            ped->SetColModel(&CTempColModels::ms_colModelPed1);
            mi = ped;
        } else if(type == MITYPE_VEHICLE) {
            CVehicleModelInfo *vehicle = CModelInfo::AddVehicleModel(next);
            bool fallback = false;
            vehicle->m_vehicleType = vehicleType;
            vehicle->m_handlingId = BuiltinHandling(vehicleType, m.handling, fallback);
            fallbackHandling += fallback;
            // Only built-in VC handling is used. No SA physics, wheel model
            // IDs, component rules or traffic spawning data are imported.
            vehicle->m_wheelId = -1; // SA wheels are embedded in the DFF
            vehicle->m_wheelScale = 1.0f;
            vehicle->m_bikeSteerAngle = vehicleType == VEHICLE_TYPE_BIKE ? 35.0f : 0.0f;
            vehicle->m_compRules = 0;
            vehicle->m_vehicleClass = -1;
            vehicle->m_level = 0;
            vehicle->m_frequency = 0;
            std::snprintf(vehicle->m_gameName, sizeof(vehicle->m_gameName), "%.9s", m.gameName.c_str());
            vehicle->SetColModel(&CTempColModels::ms_colModelBBox);
            mi = vehicle;
        } else {
            CWeaponModelInfo *weapon = CModelInfo::AddWeaponModel(next);
            weapon->SetNumAtomics(1);
            weapon->m_lodDistances[0] = m.distance;
            weapon->SetColModel(&CTempColModels::ms_colModelWeapon);
            // Weapon behavior/animation remains VC weapon.dat, not SA fields.
            mi = weapon;
        }
        if(!mi) { full++; continue; }
        mi->SetModelName(m.name.c_str());
        mi->SetTexDictionary(txdName);
        if(hasPair) paired++;
        if(type == MITYPE_SIMPLE || type == MITYPE_TIME) {
            CSimpleModelInfo *object = (CSimpleModelInfo *)mi;
            object->SetNumAtomics(1);
            float distances[3] = { m.distance, m.distance, m.distance };
            object->SetLodDistances(distances);
            object->m_alpha = 255;
            // Only VC-compatible object flags; BR flags are not LOD IDs.
            object->m_noFade = !!(m.flags & 2);
            object->m_drawLast = !!(m.flags & 4);
            object->m_additive = !!(m.flags & 8);
            object->m_ignoreLight = !!(m.flags & 0x20);
            object->m_noZwrite = !!(m.flags & 0x40);
            object->m_noShadows = !!(m.flags & 0x80);
            object->SetColModel(&CTempColModels::ms_colModelBBox);
        }
        existing.emplace(key, next);
        nameSlots[key] = next;
        brSlots[next] = true;
        next++;
        installed++;
    }
    for(std::map<int, std::string>::const_iterator it = ctx.lastNameForId.begin();
        it != ctx.lastNameForId.end(); ++it) {
        std::map<std::string, int>::const_iterator slot = nameSlots.find(it->second);
        if(slot != nameSlots.end()) modelSlots[it->first] = slot->second;
    }
    // VC tobj pairs refer to their day/night sibling; source numeric IDs
    // have changed, so resolve the companion using our remapped name table.
    for(std::map<std::string, customworld::Model>::const_iterator it = ctx.definitions.begin();
        it != ctx.definitions.end(); ++it) {
        if(it->second.kind != customworld::Model::TIME_OBJECT) continue;
        std::map<std::string, int>::const_iterator slot = nameSlots.find(it->first);
        if(slot == nameSlots.end()) continue;
        std::string other = it->first;
        size_t suffix = other.find("_nt");
        if(suffix != std::string::npos) other.replace(suffix, 3, "_dy");
        else if((suffix = other.find("_dy")) != std::string::npos) other.replace(suffix, 3, "_nt");
        else continue;
        std::map<std::string, int>::const_iterator related = nameSlots.find(other);
        if(related != nameSlots.end() && related->second != slot->second &&
           CModelInfo::GetModelInfo(slot->second)->GetModelType() == MITYPE_TIME &&
           CModelInfo::GetModelInfo(related->second)->GetModelType() == MITYPE_TIME)
            ((CTimeModelInfo *)CModelInfo::GetModelInfo(slot->second))->SetOtherTimeModel(related->second);
    }
    CUSTOM_LOG("BR IDE: %u file(s), %u record(s), %u invalid, %u long, %u source ID conflicts, %u installed, %u native reused, %u incompatible, %u unsupported vehicle types, %u without DFF, %u over capacity, %u VC handling fallbacks, %u paired IMG TXDs\n",
        ctx.files, ctx.parsed, ctx.bad, ctx.overlong, ctx.conflicts, installed,
        reusedNative, incompatible, unsupported, noAsset, full, fallbackHandling, paired);
    if(full) CUSTOM_LOG("BR IDE: free model, TXD or int16 ID slots exhausted; some records were skipped\n");
}

void CCustomWorld::LoadCollisions(void)
{
    if(collisionLoaded || modelSlots.empty()) return;
    collisionLoaded = true;
    ColContext ctx;
    ctx.freeModels = CPools::GetColModelPool()->GetNoOfFreeSpaces();
    CCustomModels::ForEachBrCollision(ReadCollision, &ctx); // CED2 .cls containers
    CCustomModels::ForEachGameCollision(ReadGameCollision, &ctx, HasWorldCollisionTarget); // .col and legacy .cls
    CUSTOM_LOG("custom ZIP collision: %u installed, %u replacements, %u invalid, %u outside IDE, %u over pool capacity\n",
        ctx.installed, ctx.replaced, ctx.invalid, ctx.missing, ctx.full);
}

void CCustomWorld::LoadIpl(void)
{
    if(modelSlots.empty()) return;
    IplContext ctx;
    CCustomModels::ForEachWorldFile(false, ReadIpl, &ctx);
    CUSTOM_LOG("BR IPL: %u file(s), %u record(s), %u placed, %u exact duplicates, %u LOD link(s), %u invalid, %u long, %u unknown models, %u without DFF, %u invalid coordinates, %u allocation/handle failures; pools %d buildings / %d sector nodes\n",
        ctx.files, ctx.parsed, ctx.placed, ctx.duplicates, ctx.linkedLod, ctx.bad, ctx.overlong,
        ctx.noModel, ctx.noAsset, ctx.outside, ctx.capacity,
        CPools::GetBuildingPool()->GetSize(), CPools::GetPtrNodePool()->GetSize());
    LogUnresolved("unresolved model", ctx.unknownNames);
    LogUnresolved("missing DFF", ctx.missingDffNames);
}

bool CCustomWorld::OwnsModelSlot(int slot)
{
    return slot >= 0 && slot < (int)brSlots.size() && brSlots[slot];
}

void CCustomWorld::Shutdown(void)
{
#ifdef EXTENDED_PIPELINES
    if(graphicsActive) {
        // Starting another game in this process must not inherit zip/ or
        // common/ graphics settings, nor retain an owned GPU reflection mask.
        CustomPipes::VehiclePipeSwitch = previousVehiclePipe;
        CustomPipes::LightmapEnable = previousLightmap;
        CustomPipes::SetCarReflectionMask(nil);
        graphicsActive = false;
    }
#endif
    modelSlots.clear();
    nameSlots.clear();
    std::fill(brSlots.begin(), brSlots.end(), false);
    ideLoaded = false;
    collisionLoaded = false;
}

#endif // CUSTOM_MODELS
