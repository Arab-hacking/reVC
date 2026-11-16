// Invoke the real car atomic classifier without a GL context.
#include "common.h"
#include "VehicleModelInfo.h"
#include "VisibilityPlugins.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static const char *nodeName;
static RwRGBA color;
static int flags, renderFlags;
static RpAtomicCallBackRender selected;
char *GetFrameNodeName(RwFrame *) { return const_cast<char*>(nodeName); }
RwFrame *RpAtomicGetFrame(const RpAtomic *) { return nullptr; }
RpGeometry *RpAtomicGetGeometry(const RpAtomic *) { return nullptr; }
RpGeometry *RpGeometryForAllMaterials(RpGeometry *g, RpMaterialCallBack cb, void *data) { cb(nullptr, data); return g; }
const RwRGBA *RpMaterialGetColor(const RpMaterial *) { return &color; }
RpAtomic *RpAtomicSetFlags(RpAtomic *a, RwUInt32 f) { renderFlags = f; return a; }
void CVisibilityPlugins::SetAtomicRenderCallback(RpAtomic *, RpAtomicCallBackRender cb) { selected = cb; }
void CVisibilityPlugins::SetAtomicFlag(RpAtomic *, int f) { flags |= f; }
RpAtomic *CVisibilityPlugins::RenderVehicleHiDetailCB(RpAtomic *a) { return a; }
RpAtomic *CVisibilityPlugins::RenderVehicleHiDetailAlphaCB(RpAtomic *a) { return a; }
RpAtomic *CVisibilityPlugins::RenderVehicleReallyLowDetailCB(RpAtomic *a) { return a; }
static void Check(bool ok, const char *why) { if(!ok) { std::fprintf(stderr, "SACarRenderPolicyTests: %s\n", why); std::abort(); } }
int main() {
    const char *opaque[] = {"body", "chassis_hi", "interior", "part_lo", "door_lf_ok"};
    for(const char *name : opaque) {
        nodeName = name; color = {255, 255, 255, 255}; flags = 0; renderFlags = rpATOMICRENDER;
        CVehicleModelInfo::SetAtomicRendererCB(nullptr, nullptr);
        Check(selected == CVisibilityPlugins::RenderVehicleHiDetailCB && renderFlags == rpATOMICRENDER, "SA renders ordinary meshes independent of _hi/_lo");
    }
    nodeName = "chassis_vlo";
    CVehicleModelInfo::SetAtomicRendererCB(nullptr, nullptr);
    Check(selected == CVisibilityPlugins::RenderVehicleReallyLowDetailCB, "_vlo is the SA low-detail mesh");
    nodeName = "windscreen";
    CVehicleModelInfo::SetAtomicRendererCB(nullptr, nullptr);
    Check(selected == CVisibilityPlugins::RenderVehicleHiDetailAlphaCB, "windscreen uses alpha callback");
    nodeName = "interior"; color.alpha = 100;
    CVehicleModelInfo::SetAtomicRendererCB(nullptr, nullptr);
    Check(selected == CVisibilityPlugins::RenderVehicleHiDetailAlphaCB, "material alpha respected");
    nodeName = "door_lf_dam";
    CVehicleModelInfo::SetAtomicRendererCB(nullptr, nullptr);
    Check(renderFlags == 0 && (flags & ATOMIC_FLAG_DAM), "damage mesh hidden initially");
    std::puts("SACarRenderPolicyTests: ALL PASSED");
}
