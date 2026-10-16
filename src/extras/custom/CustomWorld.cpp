#include "common.h"
#include "CustomWorld.h"

#ifdef CUSTOM_MODELS

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "BrCollisionFormat.h"
#include "CustomColFormat.h"
#include "CustomModels.h"
#include "CustomWorldFormat.h"
#include "SavehDiag.h"
#include "ModelInfo.h"
#include "TimeModelInfo.h"
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

struct IdeContext {
    std::map<int, customworld::Model> definitions;
    unsigned files, parsed, bad, overlong, conflicts;
    IdeContext() : files(0), parsed(0), bad(0), overlong(0), conflicts(0) {}
};

void ReadIde(const char *path, const unsigned char *data, size_t size, void *user)
{
    IdeContext &ctx = *(IdeContext *)user;
    ctx.files++;
    customworld::Stats stats = customworld::ParseIde(data, size,
        [&ctx](const customworld::Model &m) {
            std::map<int, customworld::Model>::iterator prev = ctx.definitions.find(m.id);
            if(prev != ctx.definitions.end() && prev->second.name != m.name)
                ctx.conflicts++;
            ctx.definitions[m.id] = m; // deterministic path order; last IDE wins
        });
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
    std::map<std::string, int>::const_iterator it = nameSlots.find(customworld::Lower(name));
    if(it == nameSlots.end()) { ctx.missing++; return; }
    CBaseModelInfo *mi = CModelInfo::GetModelInfo(it->second);
    if(!mi || !mi->IsBuilding()) return;
    std::vector<customcolformat::Block> blocks;
    if(customcolformat::Index(game, size, blocks) || blocks.size() != 1 ||
       customworld::Lower(blocks[0].name) != customworld::Lower(name) ||
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
    std::map<std::string, int>::const_iterator it = nameSlots.find(customworld::Lower(name));
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
    std::map<std::string, int>::const_iterator it = nameSlots.find(customworld::Lower(name));
    return it != nameSlots.end() && CModelInfo::GetModelInfo(it->second)->IsBuilding();
}

struct IplContext {
    unsigned files, parsed, bad, overlong, placed, linkedLod, noModel, noAsset, outside, capacity;
    int buildingsFree, nodesFree;
    IplContext() : files(0), parsed(0), bad(0), overlong(0), placed(0), linkedLod(0),
                   noModel(0), noAsset(0), outside(0), capacity(0),
                   buildingsFree(0), nodesFree(0) {}
};

int FindSlot(const customworld::Instance &inst)
{
    std::string key = customworld::Lower(inst.name);
    std::map<int, int>::const_iterator id = modelSlots.find(inst.id);
    if(id != modelSlots.end() &&
       customworld::Lower(CModelInfo::GetModelInfo(id->second)->GetModelName()) == key)
        return id->second;
    // A missing IDE id may still be identified by its name; never pass an
    // untranslated BR numeric ID into the native VC model-info array.
    std::map<std::string, int>::const_iterator named = nameSlots.find(key);
    return named == nameSlots.end() ? -1 : named->second;
}

void LinkLods(const std::vector<customworld::Instance> &instances, IplContext &ctx)
{
    for(size_t n = 0; n < instances.size(); n++) {
        const customworld::Instance &near = instances[n];
        if(near.lod < 0 || (size_t)near.lod >= instances.size() || (size_t)near.lod == n)
            continue;
        int nearSlot = FindSlot(near), farSlot = FindSlot(instances[near.lod]);
        if(nearSlot < 0 || farSlot < 0 || nearSlot == farSlot) continue;
        CSimpleModelInfo *nearModel = (CSimpleModelInfo *)CModelInfo::GetModelInfo(nearSlot);
        CSimpleModelInfo *farModel = (CSimpleModelInfo *)CModelInfo::GetModelInfo(farSlot);
        if(!nearModel->IsBuilding() || !farModel->IsBuilding()) continue;
        farModel->m_isBigBuilding = true;
        farModel->SetRelatedModel(nearModel);
        farModel->m_lodDistances[2] = nearModel->m_lodDistances[0];
        ctx.linkedLod++;
    }
}

void PlaceInstance(const customworld::Instance &inst, IplContext &ctx)
{
    int slot = FindSlot(inst);
    if(slot < 0) { ctx.noModel++; return; }

    CBaseModelInfo *mi = CModelInfo::GetModelInfo(slot);
    if(!mi || !mi->IsBuilding() || mi->GetColModel() == nil) { ctx.noModel++; return; }
    if(!CCustomModels::HasModelFile(mi->GetModelName()) &&
       !CStreaming::ms_aInfoForModel[slot].m_size) { ctx.noAsset++; return; }
    CColModel *col = mi->GetColModel();
    float limit[3];
    for(int k = 0; k < 3; k++) {
        float lo = k == 0 ? col->boundingBox.min.x : k == 1 ? col->boundingBox.min.y : col->boundingBox.min.z;
        float hi = k == 0 ? col->boundingBox.max.x : k == 1 ? col->boundingBox.max.y : col->boundingBox.max.z;
        limit[k] = std::max(std::fabs(lo), std::fabs(hi));
    }
    // A circumscribed radius guarantees CEntity::Add/Remove's sector indices
    // remain inside the grid even after quaternion rotation.
    float radius = std::sqrt(limit[0]*limit[0]+limit[1]*limit[1]+limit[2]*limit[2]) + 1.0f;
    if(!std::isfinite(radius) || inst.pos[0]-radius < WORLD_MIN_X ||
       inst.pos[0]+radius >= WORLD_MAX_X || inst.pos[1]-radius < WORLD_MIN_Y ||
       inst.pos[1]+radius >= WORLD_MAX_Y) { ctx.outside++; return; }

    float axis[3], w;
    customworld::ViceAxis(inst, axis, w);
    char vcLine[512];
    int n = std::snprintf(vcLine, sizeof(vcLine),
        "%d %s %d %.9g %.9g %.9g 1 1 1 %.9g %.9g %.9g %.9g",
        slot, mi->GetModelName(), inst.interior, inst.pos[0], inst.pos[1], inst.pos[2],
        axis[0], axis[1], axis[2], w);
    if(n < 0 || n >= (int)sizeof(vcLine)) { ctx.bad++; return; }
    // All BR models installed here have m_objectId == -1, i.e. CBuilding.
    // A corrupt or unexpectedly large IPL must not assert in the pool or
    // sector-list allocator. Leave room for the rest of the running game.
    // CPool::GetNoOfFreeSpaces walks its entire pool. Cache the free counts
    // across ~40k placements and subtract a safe upper bound on sector nodes
    // (the rotated bbox is contained by this bounding sphere). Refresh once
    // per IPL file to avoid rejecting objects unnecessarily.
    int nx = std::min(NUMSECTORS_X, (int)std::ceil(2.0f*radius/SECTOR_SIZE_X)+2);
    int ny = std::min(NUMSECTORS_Y, (int)std::ceil(2.0f*radius/SECTOR_SIZE_Y)+2);
    int nodes = nx*ny;
    if(ctx.buildingsFree <= 100 || ctx.nodesFree - nodes <= 10000) {
        ctx.capacity++; return;
    }
    CFileLoader::LoadObjectInstance(vcLine);
    ctx.buildingsFree--;
    ctx.nodesFree -= nodes;
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
    ctx.buildingsFree = CPools::GetBuildingPool()->GetNoOfFreeSpaces();
    ctx.nodesFree = CPools::GetPtrNodePool()->GetNoOfFreeSpaces();
    ctx.parsed += (unsigned)stats.records;
    ctx.bad += (unsigned)stats.rejected;
    ctx.overlong += (unsigned)stats.overlong;
    if(stats.rejected || stats.overlong)
        CUSTOM_LOG("BR IPL %s: %u accepted, %u invalid, %u long line(s)\n",
            path, (unsigned)stats.records, (unsigned)stats.rejected, (unsigned)stats.overlong);
}

} // namespace

void CCustomWorld::LoadIde(void)
{
    if(ideLoaded) return; // DEFAULT.DAT and the level DAT each have an IPL boundary
    ideLoaded = true;
    modelSlots.clear();
    nameSlots.clear();
    if(!CCustomModels::HasWorldFiles()) return;
    brSlots.assign(MODELINFOSIZE, false);
    IdeContext ctx;
    CCustomModels::ForEachWorldFile(true, ReadIde, &ctx);
    int next = FIRST_BR_SLOT;
    unsigned installed = 0, full = 0, merged = 0;
    for(std::map<int, customworld::Model>::const_iterator it = ctx.definitions.begin();
        it != ctx.definitions.end(); ++it) {
        const customworld::Model &m = it->second;
        std::string key = customworld::Lower(m.name);
        std::map<std::string, int>::const_iterator existing = nameSlots.find(key);
        if(existing != nameSlots.end()) {
            modelSlots[m.id] = existing->second;
            merged++;
            continue;
        }
        while(next < MODELINFOSIZE && CModelInfo::GetModelInfo(next) != nil) next++;
        if(next == MODELINFOSIZE ||
           !(m.kind == customworld::Model::TIME_OBJECT ? CModelInfo::CanAddTimeModel() : CModelInfo::CanAddSimpleModel()) ||
           (CTxdStore::FindTxdSlot(m.txd.c_str()) == -1 && !CTxdStore::HasFreeSlot())) {
            full++;
            continue;
        }
        CSimpleModelInfo *mi;
        if(m.kind == customworld::Model::TIME_OBJECT) {
            CTimeModelInfo *time = CModelInfo::AddTimeModel(next);
            time->SetTimes(m.timeOn, m.timeOff);
            mi = time;
        } else {
            mi = CModelInfo::AddSimpleModel(next);
        }
        mi->SetModelName(m.name.c_str());
        mi->SetTexDictionary(m.txd.c_str());
        mi->SetNumAtomics(1);
        float distances[3] = { m.distance, m.distance, m.distance };
        mi->SetLodDistances(distances);
        mi->m_alpha = 255;
        // Only directly compatible VC flags. Do not interpret BR's additional
        // SA flags as an object ID, collision slot, or native LOD pointer.
        mi->m_noFade = !!(m.flags & 2);
        mi->m_drawLast = !!(m.flags & 4);
        mi->m_additive = !!(m.flags & 8);
        mi->m_ignoreLight = !!(m.flags & 0x20);
        mi->m_noZwrite = !!(m.flags & 0x40);
        mi->m_noShadows = !!(m.flags & 0x80);
        mi->SetColModel(&CTempColModels::ms_colModelBBox);
        modelSlots[m.id] = next;
        nameSlots[key] = next;
        brSlots[next] = true;
        next++;
        installed++;
    }
    // VC tobj pairs refer to their day/night sibling; source numeric IDs
    // have changed, so resolve the companion using our remapped name table.
    for(std::map<int, customworld::Model>::const_iterator it = ctx.definitions.begin();
        it != ctx.definitions.end(); ++it) {
        if(it->second.kind != customworld::Model::TIME_OBJECT) continue;
        std::map<int, int>::const_iterator slot = modelSlots.find(it->first);
        if(slot == modelSlots.end()) continue;
        std::string other = customworld::Lower(it->second.name);
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
    CUSTOM_LOG("BR IDE: %u file(s), %u record(s), %u invalid, %u long, %u ID conflict(s), %u installed, %u shared names, %u over capacity\n",
        ctx.files, ctx.parsed, ctx.bad, ctx.overlong, ctx.conflicts, installed, merged, full);
    if(full) CUSTOM_LOG("BR IDE: more unique models/TXDs than free int16 slots; some records could not be installed\n");
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
    ctx.buildingsFree = CPools::GetBuildingPool()->GetNoOfFreeSpaces();
    ctx.nodesFree = CPools::GetPtrNodePool()->GetNoOfFreeSpaces();
    CCustomModels::ForEachWorldFile(false, ReadIpl, &ctx);
    CUSTOM_LOG("BR IPL: %u file(s), %u record(s), %u placed, %u LOD link(s), %u invalid, %u long, %u unknown models, %u missing assets, %u outside world, %u over pool capacity\n",
        ctx.files, ctx.parsed, ctx.placed, ctx.linkedLod, ctx.bad, ctx.overlong,
        ctx.noModel, ctx.noAsset, ctx.outside, ctx.capacity);
}

bool CCustomWorld::OwnsModelSlot(int slot)
{
    return slot >= 0 && slot < (int)brSlots.size() && brSlots[slot];
}

void CCustomWorld::Shutdown(void)
{
    modelSlots.clear();
    nameSlots.clear();
    std::fill(brSlots.begin(), brSlots.end(), false);
    ideLoaded = false;
    collisionLoaded = false;
}

#endif // CUSTOM_MODELS
