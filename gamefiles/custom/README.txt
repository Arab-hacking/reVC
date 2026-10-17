Custom models and animations
============================

Place ZIP archives here (subdirectories up to eight levels are scanned).
Loose models/textures/animations also work. Filenames are case-insensitive:

    <model>.mod / <model>.dff       model (BR / RenderWare)
    <texture>.btx / <slot>.txd     texture / texture dictionary
    *.cls / *.col                 collision containers INSIDE ZIPs only
    <block>.ifp / <block>.ani     animation dictionary

For BR world geometry, place common.zip here with IDE/IPL files anywhere
inside its directory tree (for example, maps/IDE/*.ide and
maps/Level_Events/*.ipl). The supplied reVC.txt is actually a ZIP with
maps/ at its root: rename it to common.zip without changing its contents.
Loose *.ide and *.ipl files in custom/ or
its subdirectories also work; a loose file overrides the matching relative
path from common.zip. Put any number of ZIPs containing CED2 *.cls or VC/SA
*.col containers here, including br_collision.zip. Collision is matched by
its INTERNAL model name, not by the .cls/.col filename; equal container
filenames in different ZIPs do not hide each other's models. No collision is
read from img/*.img. The stock VC map remains present. See CUSTOM_MODELS.md.

If the map pack contains mutually exclusive IPLs (event/race/old foliage),
create custom/active_ipl.txt: one FULL path inside common.zip per line, e.g.
    maps/Level_Events/BRmap_Event_9May.ipl
Paths are case-insensitive; # and // comments are allowed. Only listed IPLs
are loaded. Without this file all IPLs are loaded, but identical placements
are deduplicated. IDEs are always indexed so reused IDs can be resolved by
model name. The building and sector-node pools grow on demand for large maps;
this does not change texture/model cache limits or the world boundary.

For example, custom/ped.ifp (or anim/ped.ifp in a zip inside custom/) is
considered when the game loads the PED animation block. An SA ANP2/ANP3 file is loaded as an additional sa_ped bank and leaves the
original VC block intact. Custom ANPK files do not replace VC animation;
invalid custom dictionaries fall back to the game's IFP.
Models in img/*.img (VC IMG v1 + .dir) take priority over old .mod/.btx in
ZIPs. See ../img/README.txt for IMG placement and format requirements.
The custom/ folder is not MODELS/CUSTOM.IMG; that is an unrelated IMG archive.

The scan happens once at startup; when the same filename occurs more than once,
the source from the newest archive wins within each format; a valid .col
block overrides a same-named .cls collision. REVC_CUSTOM_DIR=<path> selects
another folder. All startup, custom-file, streaming and crash diagnostics go into the
single debug.log next to the game (no REVC_CUSTOM_LOG setting needed). On
macOS and Android the log follows the user-files / storage-root location.

See CUSTOM_MODELS.md for the full format and compatibility details.
