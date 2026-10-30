Custom resources: explicit layout
=================================

Place files under these paths (subdirectories under each asset folder are
searched up to eight levels deep):

    custom/anim/*.ifp               SA ANP2/ANP3 animation banks
    custom/img/*.img + *.dir         VC IMG v1 with DFF/TXD and COL/CLS entries
    custom/data/maps/**/*.ide        world model definitions
    custom/data/maps/**/*.ipl        world placements
    custom/common.zip                optional data/maps IDE/IPL + config/timecyc

An optional custom/data/maps/active_ipl.txt selects IPLs by FULL path relative
to custom/ (e.g. data/maps/level/scene.ipl), one per line. Include every IPL
you need; an empty list disables all custom placements. IDEs are not filtered.
Without active_ipl.txt, data/config.dat INSIDE custom/common.zip selects
IPLs from both sources; if absent/invalid, all available IPLs are read.
All IDEs are read regardless of that selection. Loose files override ZIP
entries with the same relative path (including the old maps/ alias).
The old custom/active_ipl.txt and game-root common/data/config.dat have no effect.

DFF/TXD/COL/CLS must be inside mounted IMG v1 + external DIR pairs in
custom/img/, not loose in this folder or in custom/collision/. A collision-only
IMG such as br_collision.img + br_collision.dir is supported. The paired
model.txd may be in a different user IMG from model.dff; it is still found
by name. The normal VC streamer keeps its own assets and unloads distant
models. VC skeletons use stock VC animation banks; user IFPs serve only
compatible SA skeletons.

Only custom/common.zip is read: data/maps/**/*.ide/.ipl (old maps/ aliases),
data/config.dat for optional IPL selection and data/timecyc.json for lighting.
Loose custom/data/timecyc.json takes precedence over the ZIP entry. No model,
texture, collision, animation, reflection-mask or lightmap BTX is served from
a ZIP. Other custom/*.zip, zip/*.zip, game-root img/, game-root common/,
custom/collision/, custom/ped.ifp, loose MOD/BTX, and .ani are not fallback
locations. BR .bpc, AUDO, features and vehicle physics are not imported.
The timecyc file adjusts only light and native shadow strengths.

REVC_CUSTOM_DIR=<path> can replace the custom/ ROOT, subject to the IMG path
length limit of 63 bytes. A fresh debug.log records scanning, IMG validation,
collisions, maps and animations. See ../../CUSTOM_MODELS.md for details.
