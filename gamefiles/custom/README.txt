Custom resources: explicit layout
=================================

Place files under these paths (subdirectories under each asset folder are
searched up to eight levels deep):

    custom/anim/*.ifp               SA ANP2/ANP3 animation banks
    custom/img/*.img + *.dir         VC IMG v1 model DFF / texture TXD pairs
    custom/collision/*.col / *.cls   collisions, matched by INTERNAL model name
    custom/data/maps/**/*.ide        world model definitions
    custom/data/maps/**/*.ipl        world placements

An optional custom/data/maps/active_ipl.txt selects IPLs by FULL path relative
to custom/ (e.g. data/maps/level/scene.ipl), one per line. Include every IPL
you need; an empty list disables all custom placements. IDEs are not filtered.
By default all IPLs in custom/data/maps/ are read. The old
custom/active_ipl.txt and common/data/config.dat have no effect.

DFF/TXD files must be inside mounted IMG v1 + external DIR pairs in
custom/img/, not loose in this folder. The paired model.txd may be in a
different user IMG from model.dff; it is still found by name. The normal VC
streamer keeps its own assets and unloads distant models. VC skeletons use
stock VC animation banks; user IFPs serve only compatible SA skeletons.

No ZIP is read for ANY user resource (including animation, collision, map,
model, texture, reflection-mask or lightmap BTX). Do not use custom/*.zip,
zip/*.zip, game-root img/, game-root common/, custom/ped.ifp, loose MOD/BTX,
or .ani as fallback locations. BR .bpc, AUDO, features and vehicle physics
are not imported. Unpacking a ZIP is not enough if its contents are not in
the compatible file formats and locations above. Optional
custom/data/timecyc.json adjusts only light and native shadow strengths.

REVC_CUSTOM_DIR=<path> can replace the custom/ ROOT, subject to the IMG path
length limit of 63 bytes. A fresh debug.log records scanning, IMG validation,
collisions, maps and animations. See ../../CUSTOM_MODELS.md for details.
