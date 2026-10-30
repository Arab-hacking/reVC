#ifdef LIBRW
#include "SAStaticModel.h"
#include <rw.h>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace SAStaticModel {
namespace {

bool Finite(const rw::V3d &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool Finite(const rw::Matrix &m)
{
    return Finite(m.right) && Finite(m.up) && Finite(m.at) && Finite(m.pos);
}

// Compare the actual matrix, not the IDENTITY flag: imported DFF frames need
// not have had their flags optimized. Even a single translated child must be
// baked before CSimpleModelInfo::CreateInstance replaces its frame.
bool Identity(const rw::Matrix &m)
{
    return m.right.x == 1 && m.right.y == 0 && m.right.z == 0 &&
           m.up.x == 0 && m.up.y == 1 && m.up.z == 0 &&
           m.at.x == 0 && m.at.y == 0 && m.at.z == 1 &&
           m.pos.x == 0 && m.pos.y == 0 && m.pos.z == 0;
}

struct Part {
    rw::Atomic *atomic;
    rw::Matrix matrix;
};

rw::Atomic *Fail(const char **error, const char *message)
{
    if(error) *error = message;
    return nil;
}

} // namespace

rw::Atomic *TakeAtomic(rw::Clump *clump, const char **error)
{
    if(error) *error = nil;
    if(clump == nil) return Fail(error, "missing clump");

    std::vector<Part> parts;
    FORLIST(lnk, clump->atomics){
        rw::Atomic *a = rw::Atomic::fromClump(lnk);
        if(!(a->object.object.flags & rw::Atomic::RENDER)) continue;
        rw::Geometry *g = a->geometry;
        if(g == nil || a->getFrame() == nil || g->numMorphTargets < 1 ||
           g->morphTargets == nil || g->numVertices < 0 || g->numTriangles < 0)
            return Fail(error, "renderable atomic has no geometry or frame");
        if(g->numTriangles == 0) continue; // empty render dummy
        if(g->numVertices == 0 || (!(g->flags & rw::Geometry::NATIVE) &&
           (g->morphTargets[0].vertices == nil || g->triangles == nil)))
            return Fail(error, "renderable atomic has no triangles/vertices");
        if(rw::skinGlobals.geoOffset > 0 && rw::Skin::get(g))
            return Fail(error, "skinned geometry is not a static object");
        rw::Matrix matrix = *a->getFrame()->getLTM();
        if(!Finite(matrix)) return Fail(error, "non-finite frame transform");
        parts.push_back({ a, matrix });
    }
    if(parts.empty()) return Fail(error, "clump has no visible geometry");

    if(parts.size() == 1 && Identity(parts[0].matrix)){
        // InitClump must first uninstance foreign native geometry. Otherwise
        // detaching it here leaves an atomic that the current renderer cannot
        // draw, even though the single-atomic fast path needs no CPU copy.
        rw::Geometry *g = parts[0].atomic->geometry;
        if((g->flags & rw::Geometry::NATIVE) &&
           (g->instData == nil || g->instData->platform != rw::platform))
            return Fail(error, "cannot render foreign native geometry");
        // The overwhelmingly common SA static DFF needs no extra vertex copy.
        // Material->texture still points into the IDE-selected current TXD.
        rw::Frame *frame = rw::Frame::create();
        if(frame == nil) return Fail(error, "cannot allocate atomic frame");
        rw::Atomic *a = parts[0].atomic;
        clump->removeAtomic(a);
        a->setFrame(frame);
        return a;
    }

    // reVC renders a simple model as an RpAtomic, not a clump. Multiple SA
    // atomics and non-identity child frames therefore need one CPU geometry.
    // InitClump has already converted foreign native data to portable data.
    // The same-platform D3D8/9 case still needs explicit uninstancing before
    // we can access vertices/triangles. Don't call GL3's unsupported native
    // uninstancer (or a pipeline for another platform).
    int totalVertices = 0, totalTriangles = 0, uvSets = 0;
    rw::uint32 flags = rw::Geometry::POSITIONS;
    for(size_t p = 0; p < parts.size(); p++){
        rw::Atomic *a = parts[p].atomic;
        rw::Geometry *g = a->geometry;
        if(g->flags & rw::Geometry::NATIVE){
            if(g->instData == nil || g->instData->platform != rw::platform ||
               (rw::platform != rw::PLATFORM_D3D8 && rw::platform != rw::PLATFORM_D3D9))
                return Fail(error, "cannot flatten native geometry on this renderer");
            a->uninstance();
        }
        if(g->flags & rw::Geometry::NATIVE || g->numMorphTargets != 1 ||
           g->numVertices <= 0 || g->numTriangles <= 0 ||
           g->numTexCoordSets < 0 || g->numTexCoordSets > 8 ||
           g->morphTargets == nil || g->morphTargets[0].vertices == nil ||
           g->triangles == nil || g->matList.numMaterials <= 0 ||
           g->matList.materials == nil)
            return Fail(error, "static geometry has no portable triangles/vertices/materials");
        // Triangle indices and mesh indices are 16-bit; avoid wrapping and
        // bound worst-case allocations from malformed or huge DFFs.
        if(g->numVertices > 65535 - totalVertices ||
           g->numTriangles > 200000 - totalTriangles)
            return Fail(error, "flattened geometry exceeds 16-bit vertices or 200k triangles");
        for(int t = 0; t < g->numTriangles; t++){
            const rw::Triangle &tri = g->triangles[t];
            if(tri.v[0] >= g->numVertices || tri.v[1] >= g->numVertices ||
               tri.v[2] >= g->numVertices || tri.matId >= g->matList.numMaterials ||
               g->matList.materials[tri.matId] == nil)
                return Fail(error, "invalid triangle or material index");
        }
        for(int uv = 0; uv < g->numTexCoordSets; uv++)
            if(g->texCoords[uv] == nil)
                return Fail(error, "missing texture coordinates");
        if((g->flags & rw::Geometry::PRELIT) && g->colors == nil)
            return Fail(error, "missing prelight colors");
        totalVertices += g->numVertices;
        totalTriangles += g->numTriangles;
        if(uvSets < g->numTexCoordSets) uvSets = g->numTexCoordSets;
        flags |= g->flags & (rw::Geometry::PRELIT | rw::Geometry::NORMALS |
                             rw::Geometry::LIGHT | rw::Geometry::MODULATE);
    }
    if(uvSets) flags |= rw::Geometry::TEXTURED;
    if(uvSets > 1) flags |= rw::Geometry::TEXTURED2;
    flags |= (rw::uint32)uvSets << 16;
    // No TRISTRIP or NATIVE flag: buildMeshes constructs a triangle list.
    rw::Geometry *merged = rw::Geometry::create(totalVertices, totalTriangles, flags);
    if(merged == nil) return Fail(error, "cannot allocate static geometry");

    std::unordered_map<rw::Material*, int> materials;
    int vbase = 0, tbase = 0;
    for(size_t p = 0; p < parts.size(); p++){
        const rw::Geometry *g = parts[p].atomic->geometry;
        const rw::Matrix &m = parts[p].matrix;
        const rw::V3d &r = m.right, &u = m.up, &a = m.at;
        const float det = rw::dot(r, rw::cross(u, a));
        if(!std::isfinite(det) || std::fabs(det) < 1.0e-10f){
            merged->destroy();
            return Fail(error, "singular frame transform");
        }
        // The columns of inverse-transpose transform normals, even for
        // nonuniform scaling. A reflection also reverses triangle winding.
        const rw::V3d nr = rw::scale(rw::cross(u, a), 1.0f/det);
        const rw::V3d nu = rw::scale(rw::cross(a, r), 1.0f/det);
        const rw::V3d na = rw::scale(rw::cross(r, u), 1.0f/det);
        for(int i = 0; i < g->numVertices; i++){
            rw::V3d &dst = merged->morphTargets[0].vertices[vbase+i];
            rw::V3d::transformPoints(&dst, &g->morphTargets[0].vertices[i], 1, &m);
            if(!Finite(dst)){
                merged->destroy();
                return Fail(error, "non-finite transformed vertex");
            }
            if(flags & rw::Geometry::NORMALS){
                rw::V3d normal = g->morphTargets[0].normals ?
                    g->morphTargets[0].normals[i] : rw::makeV3d(0, 0, 1);
                normal = rw::add(rw::add(rw::scale(nr, normal.x),
                                          rw::scale(nu, normal.y)), rw::scale(na, normal.z));
                const float length = rw::length(normal);
                merged->morphTargets[0].normals[vbase+i] =
                    std::isfinite(length) && length > 1.0e-10f ?
                    rw::scale(normal, 1.0f/length) : rw::makeV3d(0, 0, 1);
            }
        }
        if(flags & rw::Geometry::PRELIT){
            if(g->colors)
                std::memcpy(merged->colors+vbase, g->colors,
                            g->numVertices*sizeof(rw::RGBA));
            else
                for(int i = 0; i < g->numVertices; i++)
                    merged->colors[vbase+i] = { 255, 255, 255, 255 };
        }
        for(int uv = 0; uv < uvSets; uv++){
            if(uv < g->numTexCoordSets)
                std::memcpy(merged->texCoords[uv]+vbase, g->texCoords[uv],
                            g->numVertices*sizeof(rw::TexCoords));
            else
                std::memset(merged->texCoords[uv]+vbase, 0,
                            g->numVertices*sizeof(rw::TexCoords));
        }
        for(int t = 0; t < g->numTriangles; t++){
            const rw::Triangle &src = g->triangles[t];
            rw::Material *mat = g->matList.materials[src.matId];
            std::unordered_map<rw::Material*, int>::iterator found = materials.find(mat);
            int id;
            if(found == materials.end()){
                if(merged->matList.numMaterials >= 65535 ||
                   (id = merged->matList.appendMaterial(mat)) < 0){
                    merged->destroy();
                    return Fail(error, "too many materials in static geometry");
                }
                materials[mat] = id; // appendMaterial keeps its own reference
            }else id = found->second;
            rw::Triangle &dst = merged->triangles[tbase+t];
            dst.v[0] = (rw::uint16)(src.v[0] + vbase);
            dst.v[1] = (rw::uint16)(src.v[det < 0 ? 2 : 1] + vbase);
            dst.v[2] = (rw::uint16)(src.v[det < 0 ? 1 : 2] + vbase);
            dst.matId = (rw::uint16)id;
        }
        vbase += g->numVertices;
        tbase += g->numTriangles;
    }
    // Calculate bounds from the *baked* vertices. librw's generic helper
    // initializes extrema to +/-1e6, which can give wrong bounds for an
    // unusually large frame offset (and thus prematurely cull the object).
    const rw::V3d *verts = merged->morphTargets[0].vertices;
    rw::V3d lo = verts[0], hi = verts[0];
    for(int i = 1; i < totalVertices; i++){
        if(verts[i].x < lo.x) lo.x = verts[i].x;
        if(verts[i].y < lo.y) lo.y = verts[i].y;
        if(verts[i].z < lo.z) lo.z = verts[i].z;
        if(verts[i].x > hi.x) hi.x = verts[i].x;
        if(verts[i].y > hi.y) hi.y = verts[i].y;
        if(verts[i].z > hi.z) hi.z = verts[i].z;
    }
    rw::V3d center = rw::scale(rw::add(lo, hi), 0.5f);
    double maxRadiusSq = 0;
    for(int i = 0; i < totalVertices; i++){
        const double x = (double)verts[i].x - center.x;
        const double y = (double)verts[i].y - center.y;
        const double z = (double)verts[i].z - center.z;
        const double radiusSq = x*x + y*y + z*z;
        if(radiusSq > maxRadiusSq) maxRadiusSq = radiusSq;
    }
    const double radius = std::sqrt(maxRadiusSq);
    if(!Finite(center) || !std::isfinite(radius) || radius > 1.0e30){
        merged->destroy();
        return Fail(error, "invalid flattened bounding sphere");
    }
    merged->morphTargets[0].boundingSphere.center = center;
    merged->morphTargets[0].boundingSphere.radius = (float)radius;
    merged->unlock(); // create a mesh for each material before rendering
    rw::Atomic *atomic = rw::Atomic::create();
    rw::Frame *frame = rw::Frame::create();
    if(atomic == nil || frame == nil){
        if(atomic) atomic->destroy();
        if(frame) frame->destroy();
        merged->destroy();
        return Fail(error, "cannot allocate static atomic/frame");
    }
    atomic->setGeometry(merged, 0);
    merged->destroy(); // the atomic holds the geometry (and its TXD materials)
    atomic->setFrame(frame);
    return atomic;
}

} // namespace SAStaticModel
#endif // LIBRW
