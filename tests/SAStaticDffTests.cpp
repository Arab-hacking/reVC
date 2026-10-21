// Headless synthetic RW 3.6 DFF + a real native D3D8 (VC-style) TXD stream.
// The game uses this librw clump reader, and binds materials by name to the
// IDE-selected current TXD. No user's game/IMG assets are required.
#include <rw.h>
#include "SAStaticModel.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace rw;

static bool Near(float a, float b) { return std::fabs(a-b) < 0.0001f; }

static void AddPCTexture(TexDictionary *dict, const char *name, uint8 pixel,
                         int nativePlatform) {
    Raster *raster = Raster::create(2, 2, 32, Raster::C8888 | Raster::TEXTURE,
                                    nativePlatform);
    assert(raster);
    uint8 *data = raster->lock(0, Raster::LOCKWRITE | Raster::LOCKNOFETCH);
    assert(data && raster->stride >= 8);
    for(int y = 0; y < 2; y++)
        std::memset(data + y*raster->stride, pixel, 8);
    raster->unlock(0);
    Texture *tex = Texture::create(raster);
    assert(tex);
    std::strncpy(tex->name, name, sizeof(tex->name)-1);
    dict->add(tex);
}

static TexDictionary *ReadNativeTxd(uint8 pixel, int nativePlatform) {
    TexDictionary *src = TexDictionary::create();
    AddPCTexture(src, "sand", pixel, nativePlatform);
    AddPCTexture(src, "roof", pixel+1, nativePlatform);
    std::vector<uint8> bytes(4096);
    StreamMemory out;
    out.open(bytes.data(), 0, (uint32)bytes.size());
    src->streamWrite(&out);
    assert(!out.eof());
    bytes.resize(out.getLength());
    src->destroy();
    // SA's TXD header carries the D3D9 device ID in the high word of the
    // dictionary STRUCT, unlike VC's zero (the texture count stays 16-bit).
    assert(bytes.size() > 28 && bytes[26] == 0 && bytes[27] == 0);
    if(nativePlatform == PLATFORM_D3D9) bytes[26] = 2;
    StreamMemory in;
    in.open(bytes.data(), (uint32)bytes.size());
    assert(findChunk(&in, ID_TEXDICTIONARY, nil, nil));
    TexDictionary *txd = TexDictionary::streamRead(&in);
    assert(txd && txd->find("sand") && txd->find("roof"));
    assert(txd->find("sand")->raster->platform == nativePlatform);
    uint8 *pixels = txd->find("roof")->raster->lock(0, Raster::LOCKREAD);
    assert(pixels && pixels[0] == (uint8)(pixel+1));
    txd->find("roof")->raster->unlock(0);
    return txd;
}

static Geometry *MakeTriangle(Texture *texture, int color, float u) {
    Geometry *g = Geometry::create(3, 1, Geometry::POSITIONS | Geometry::NORMALS |
                                      Geometry::PRELIT | Geometry::TEXTURED |
                                      Geometry::LIGHT | Geometry::MODULATE);
    assert(g);
    g->morphTargets[0].vertices[0] = makeV3d(0, 0, 0);
    g->morphTargets[0].vertices[1] = makeV3d(1, 0, 0);
    g->morphTargets[0].vertices[2] = makeV3d(0, 1, 0);
    for(int i = 0; i < 3; i++) {
        g->morphTargets[0].normals[i] = normalize(makeV3d(1, 1, 0));
        g->colors[i] = { (uint8)color, 50, 60, 255 };
        g->texCoords[0][i] = { u + (float)i, 0.25f };
    }
    Material *m = Material::create();
    assert(m);
    m->setTexture(texture);
    assert(g->matList.appendMaterial(m) == 0);
    m->destroy(); // the geometry has a ref
    g->triangles[0].v[0] = 0;
    g->triangles[0].v[1] = 1;
    g->triangles[0].v[2] = 2;
    g->triangles[0].matId = 0;
    g->calculateBoundingSphere();
    g->unlock();
    return g;
}

static void SetTransform(Frame *f, const V3d &r, const V3d &u,
                         const V3d &a, const V3d &pos) {
    f->matrix.right = r;
    f->matrix.up = u;
    f->matrix.at = a;
    f->matrix.pos = pos;
    f->matrix.update();
    f->updateObjects();
}

static Atomic *AddPart(Clump *clump, Frame *frame, Texture *tex, int color, float u) {
    Atomic *atomic = Atomic::create();
    assert(atomic);
    Geometry *g = MakeTriangle(tex, color, u);
    atomic->setGeometry(g, 0);
    g->destroy();
    atomic->setFrame(frame);
    clump->addAtomic(atomic);
    return atomic;
}

static Clump *MakeClump(Texture *sand, Texture *roof, bool multi, bool moved) {
    Clump *clump = Clump::create();
    Frame *root = Frame::create();
    clump->setFrame(root);
    if(moved)
        SetTransform(root, makeV3d(1,0,0), makeV3d(0,1,0),
                     makeV3d(0,0,1), makeV3d(10,0,0));
    Frame *one = Frame::create();
    root->addChild(one);
    if(multi)
        SetTransform(one, makeV3d(1,0,0), makeV3d(0,1,0),
                     makeV3d(0,0,1), makeV3d(2,3,0));
    AddPart(clump, one, sand, 120, 0.1f);
    if(multi) {
        Frame *two = Frame::create();
        root->addChild(two);
        // Mirrored non-uniform scale: vertex winding and normals must follow.
        SetTransform(two, makeV3d(-2,0,0), makeV3d(0,3,0),
                     makeV3d(0,0,1), makeV3d(0,5,0));
        AddPart(clump, two, roof, 220, 0.8f);
    }
    return clump;
}

static std::vector<uint8> WriteDff(Clump *clump) {
    std::vector<uint8> bytes(16384);
    StreamMemory stream;
    stream.open(bytes.data(), 0, (uint32)bytes.size());
    assert(clump->streamWrite(&stream));
    assert(!stream.eof());
    bytes.resize(stream.getLength());
    stream.close();
    return bytes;
}

static Clump *ReadDff(const std::vector<uint8> &bytes) {
    StreamMemory stream;
    stream.open((uint8*)bytes.data(), (uint32)bytes.size());
    assert(findChunk(&stream, ID_CLUMP, nil, nil));
    Clump *clump = Clump::streamRead(&stream);
    assert(clump);
    stream.close();
    return clump;
}

static void DestroyAtomic(Atomic *atomic) {
    Frame *frame = atomic->getFrame();
    atomic->destroy();
    frame->destroy();
}

int main() {
    assert(Engine::init());
    registerMeshPlugin();
    registerNativeDataPlugin(); // same attachments as RpWorldPluginAttach in the game
    registerAtomicRightsPlugin();
    registerMaterialRightsPlugin();
    assert(Engine::open(nil));
    assert(Engine::start());
    version = 0x36003; // SA RW 3.6.0.3, not the VC 3.4 writer

    TexDictionary *ideTxd = ReadNativeTxd(0x44, PLATFORM_D3D8);
    TexDictionary *unrelatedTxd = ReadNativeTxd(0x88, PLATFORM_D3D8);
    TexDictionary *saTxd = ReadNativeTxd(0x66, PLATFORM_D3D9);
    Texture *sand = ideTxd->find("sand");
    Texture *roof = ideTxd->find("roof");
    Texture *otherSand = unrelatedTxd->find("sand");
    Texture *otherRoof = unrelatedTxd->find("roof");
    uint8 *pixels = sand->raster->lock(0, Raster::LOCKREAD);
    assert(pixels && pixels[0] == 0x44);
    sand->raster->unlock(0);
    pixels = otherSand->raster->lock(0, Raster::LOCKREAD);
    assert(pixels && pixels[0] == 0x88);
    otherSand->raster->unlock(0);
    TexDictionary::setCurrent(ideTxd);

    Clump *source = MakeClump(sand, roof, true, true);
    std::vector<uint8> dff = WriteDff(source);
    const std::vector<uint8> multiDff = dff;
    source->destroy();
    // Writing the DFF does NOT embed the pixels: texture names are resolved
    // when streaming it, so use the IDE TXD even if a different dictionary
    // with identical names was current previously.
    TexDictionary::setCurrent(unrelatedTxd);
    TexDictionary::setCurrent(ideTxd);
    Clump *read = ReadDff(dff);
    assert(read->countAtomics() == 2);
    const char *error = nil;
    Atomic *merged = SAStaticModel::TakeAtomic(read, &error);
    assert(merged && error == nil);
    read->destroy(); // input frames and atomics are gone; output owns material refs
    Geometry *g = merged->geometry;
    assert(merged->clump == nil && g->numVertices == 6 && g->numTriangles == 2);
    assert(g->numTexCoordSets == 1 && g->matList.numMaterials == 2);
    assert(g->matList.materials[g->triangles[0].matId]->texture == sand);
    assert(g->matList.materials[g->triangles[1].matId]->texture == roof);
    assert(g->matList.materials[0]->texture->dict == ideTxd);
    assert(g->matList.materials[0]->texture != otherSand);
    assert(g->matList.materials[1]->texture != otherRoof);
    assert(Near(g->morphTargets[0].vertices[0].x, 12));
    assert(Near(g->morphTargets[0].vertices[0].y, 3));
    assert(Near(g->morphTargets[0].vertices[3].x, 10));
    assert(Near(g->morphTargets[0].vertices[3].y, 5));
    assert(Near(g->morphTargets[0].vertices[4].x, 8));
    assert(g->triangles[0].v[0] == 0 && g->triangles[0].v[1] == 1 && g->triangles[0].v[2] == 2);
    assert(g->triangles[1].v[0] == 3 && g->triangles[1].v[1] == 5 && g->triangles[1].v[2] == 4);
    assert(g->meshHeader && Near(g->texCoords[0][3].u, 0.8f));
    assert(g->colors[0].red == 120 && g->colors[3].red == 220);
    const V3d n = g->morphTargets[0].normals[3];
    assert(Near(n.x, -0.8320503f) && Near(n.y, 0.5547002f));
    assert(g->morphTargets[0].boundingSphere.radius > 2.0f);
    Atomic *instance = merged->clone(); // CSimpleModelInfo::CreateInstance shares geometry
    Frame *instanceFrame = Frame::create();
    instance->setFrame(instanceFrame);
    assert(instance->geometry == g);
    DestroyAtomic(instance);
    DestroyAtomic(merged);
    assert(sand->refCount == 1 && roof->refCount == 1);

    // One identity-frame static takes the zero-copy fast path; cloned
    // instances still own their own frames and keep the IDE TXD alive.
    source = MakeClump(sand, roof, false, false);
    dff = WriteDff(source);
    source->destroy();
    read = ReadDff(dff);
    Atomic *original = Atomic::fromClump(read->atomics.link.next);
    Atomic *single = SAStaticModel::TakeAtomic(read, &error);
    assert(single == original && read->countAtomics() == 0);
    read->destroy();
    assert(single->geometry->matList.materials[0]->texture == sand);
    DestroyAtomic(single);
    assert(sand->refCount == 1);

    // One child translated within the DFF cannot use the fast path: the VC
    // instance frame is the IPL transform, so the local transform is baked.
    source = MakeClump(sand, roof, false, true);
    dff = WriteDff(source);
    source->destroy();
    read = ReadDff(dff);
    single = SAStaticModel::TakeAtomic(read, &error);
    assert(single && read->countAtomics() == 1);
    read->destroy();
    assert(Near(single->geometry->morphTargets[0].vertices[0].x, 10));
    assert(single->geometry->matList.materials[0]->texture == sand);
    DestroyAtomic(single);

    // A clump with no visible atomics is rejected without installing a
    // partial model or changing any TXD references.
    source = MakeClump(sand, roof, false, false);
    Atomic *hidden = Atomic::fromClump(source->atomics.link.next);
    hidden->object.object.flags &= ~Atomic::RENDER;
    assert(SAStaticModel::TakeAtomic(source, &error) == nil);
    assert(error && source->countAtomics() == 1);
    source->destroy();
    source = MakeClump(sand, roof, true, true);
    Atomic *bad = Atomic::fromClump(source->atomics.link.next);
    bad->geometry->triangles[0].matId = 17;
    assert(SAStaticModel::TakeAtomic(source, &error) == nil);
    assert(error && source->countAtomics() == 2); // no partial installation
    source->destroy();
    assert(sand->refCount == 1 && roof->refCount == 1);

    // A same-name texture in another TXD must not replace the IDE texture.
    TexDictionary::setCurrent(unrelatedTxd);
    read = ReadDff(dff);
    single = SAStaticModel::TakeAtomic(read, &error);
    read->destroy();
    assert(single->geometry->matList.materials[0]->texture == otherSand);
    DestroyAtomic(single);
    assert(sand->refCount == 1 && otherSand->refCount == 1);

    // The IDE slot can also contain SA D3D9-native TXD pixels. Verify the
    // same SA DFF resolves both materials from it, not the earlier VC TXD.
    TexDictionary::setCurrent(saTxd);
    read = ReadDff(multiDff);
    merged = SAStaticModel::TakeAtomic(read, &error);
    assert(merged && merged->geometry->matList.materials[0]->texture == saTxd->find("sand"));
    assert(merged->geometry->matList.materials[1]->texture == saTxd->find("roof"));
    read->destroy();
    DestroyAtomic(merged);
    assert(saTxd->find("sand")->refCount == 1 && saTxd->find("roof")->refCount == 1);

    // SA's pre-instanced D3D9 geometry has no portable vertex/triangle arrays
    // until uninstancing. On the same renderer, InitClump leaves it native;
    // the static-object combiner must recover all the parts itself.
    TexDictionary::setCurrent(ideTxd);
    source = MakeClump(sand, roof, true, true);
    platform = PLATFORM_D3D9;
    FORLIST(lnk, source->atomics) {
        Atomic *a = Atomic::fromClump(lnk);
        a->instance();
        assert(a->geometry->flags & Geometry::NATIVE);
    }
    dff = WriteDff(source);
    source->destroy();
    read = ReadDff(dff);
    Geometry *nativeGeo = Atomic::fromClump(read->atomics.link.next)->geometry;
    assert(nativeGeo->flags & Geometry::NATIVE);
    assert(nativeGeo->instData && nativeGeo->instData->platform == PLATFORM_D3D9);
    merged = SAStaticModel::TakeAtomic(read, &error);
    if(!merged || error) std::fprintf(stderr, "native merge: %s\n", error ? error : "unknown");
    assert(merged && error == nil && merged->geometry->numTriangles == 2);
    read->destroy();
    assert(merged->geometry->matList.materials[0]->texture == sand);
    assert(merged->geometry->matList.materials[1]->texture == roof);
    assert(Near(merged->geometry->morphTargets[0].vertices[4].x, 8));
    DestroyAtomic(merged);
    platform = PLATFORM_NULL;
    // On GL3, InitClump first uninstances the foreign D3D9 data. Repeat that
    // transition under the headless driver and verify the combined output.
    read = ReadDff(dff);
    FORLIST(lnk, read->atomics) {
        Atomic *a = Atomic::fromClump(lnk);
        platform = PLATFORM_D3D9;
        a->uninstance();
        platform = PLATFORM_NULL;
        assert(!(a->geometry->flags & Geometry::NATIVE));
    }
    merged = SAStaticModel::TakeAtomic(read, &error);
    assert(merged && merged->geometry->numTriangles == 2);
    read->destroy();
    assert(merged->geometry->matList.materials[1]->texture == roof);
    DestroyAtomic(merged);
    assert(sand->refCount == 1 && roof->refCount == 1);

    // A one-atomic native DFF must not bypass InitClump on a different
    // renderer simply because it qualifies for the zero-copy fast path.
    source = MakeClump(sand, roof, false, false);
    platform = PLATFORM_D3D9;
    Atomic::fromClump(source->atomics.link.next)->instance();
    dff = WriteDff(source);
    source->destroy();
    platform = PLATFORM_NULL;
    read = ReadDff(dff);
    assert(SAStaticModel::TakeAtomic(read, &error) == nil);
    assert(error && read->countAtomics() == 1);
    platform = PLATFORM_D3D9;
    Atomic::fromClump(read->atomics.link.next)->uninstance();
    platform = PLATFORM_NULL;
    single = SAStaticModel::TakeAtomic(read, &error);
    assert(single && error == nil && read->countAtomics() == 0);
    read->destroy();
    DestroyAtomic(single);
    assert(sand->refCount == 1);

    TexDictionary::setCurrent(nil);
    ideTxd->destroy();
    unrelatedTxd->destroy();
    saTxd->destroy();
    Engine::stop();
    Engine::close();
    Engine::term();
    std::puts("RW 3.6 static DFF: all parts, frames, UVs, IDE TXD binding and eviction passed");
}
