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
#include <functional>

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
    IplContext() : files(0), parsed(0), bad(0), overlong(0), placed(0), duplicates(0), linkedLod(0),
                   noModel(0), noAsset(0), outside(0), capacity(0) {}
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
    unsigned installed = 0, full = 0, paired = 0;
    std::vector<const customworld::Model *> ordered;
    for(std::map<std::string, customworld::Model>::const_iterator it = ctx.definitions.begin();
        it != ctx.definitions.end(); ++it)
        ordered.push_back(&it->second);
    // Preserve the former numeric-ID installation order where possible. Only
    // the extra, previously discarded name in a conflicting ID consumes a new
    // slot; the IPL still resolves by name when its ID was reused.
    std::sort(ordered.begin(), ordered.end(), [](const customworld::Model *a,
                                                  const customworld::Model *b) {
        return a->id != b->id ? a->id < b->id : customworld::Lower(a->name) < customworld::Lower(b->name);
    });
    for(size_t n = 0; n < ordered.size(); n++) {
        const customworld::Model &m = *ordered[n];
        std::string key = customworld::Lower(m.name);
        // Some world packs put model.dff and model.txd in different IMGs,
        // while the IDE's TXD column names another dictionary. Pick the
        // same-name TXD from any mounted user IMG before creating a TXD slot
        // or queueing either resource. Otherwise keep the IDE association.
        bool hasPair = CCustomModels::PairedImageTxd(m.name.c_str()) >= 0;
        const char *txdName = hasPair ? m.name.c_str() : m.txd.c_str();
        while(next < MODELINFOSIZE && CModelInfo::GetModelInfo(next) != nil) next++;
        if(next == MODELINFOSIZE ||
           !(m.kind == customworld::Model::TIME_OBJECT ? CModelInfo::CanAddTimeModel() : CModelInfo::CanAddSimpleModel()) ||
           (CTxdStore::FindTxdSlot(txdName) == -1 && !CTxdStore::HasFreeCustomSlot())) {
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
        mi->SetTexDictionary(txdName);
        if(hasPair) paired++;
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
    CUSTOM_LOG("BR IDE: %u file(s), %u record(s), %u invalid, %u long, %u ID conflict(s) preserved by model name, %u installed (%u paired model.dff/model.txd), %u reused names, %u over capacity\n",
        ctx.files, ctx.parsed, ctx.bad, ctx.overlong, ctx.conflicts, installed, paired, ctx.sharedNames, full);
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
    CCustomModels::ForEachWorldFile(false, ReadIpl, &ctx);
    CUSTOM_LOG("BR IPL: %u file(s), %u record(s), %u placed, %u exact duplicates, %u LOD link(s), %u invalid, %u long, %u unknown models, %u invalid coordinates, %u allocation/handle failures; pools %d buildings / %d sector nodes\n",
        ctx.files, ctx.parsed, ctx.placed, ctx.duplicates, ctx.linkedLod, ctx.bad, ctx.overlong,
        ctx.noModel, ctx.noAsset, ctx.outside, ctx.capacity,
        CPools::GetBuildingPool()->GetSize(), CPools::GetPtrNodePool()->GetSize());
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
