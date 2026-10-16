Custom models and animations
============================

Place loose files here or put them in .zip archives (subdirectories up to eight
levels are scanned). Models/textures use the final filename, case-insensitively:

    <model>.mod / <model>.dff       model (BR / RenderWare)
    <texture>.btx / <slot>.txd     texture / texture dictionary
    <model>.cls / <slot>.col       collision
    <block>.ifp / <block>.ani     animation dictionary

For BR world geometry, place common.zip here with IDE/IPL files anywhere
inside its directory tree (for example, data/maps/IDE/*.ide and
 data/maps/Level_Events/*.ipl). Loose *.ide and *.ipl files in custom/ or
its subdirectories also work; a loose file overrides the matching relative
path from common.zip. Put br_collision.zip here for multi-model CED2 *.cls
containers; the collision is matched by its INTERNAL model name, not by the
.cls filename. The stock VC map remains present. See CUSTOM_MODELS.md.

For example, custom/ped.ifp (or anim/ped.ifp in a zip inside custom/) is
considered when the game loads the PED animation block. An SA ANP2/ANP3 file is loaded as an additional sa_ped bank and leaves the
original VC block intact. Custom ANPK files do not replace VC animation;
invalid custom dictionaries fall back to the game's IFP.
The custom/ folder is not MODELS/CUSTOM.IMG; that is an unrelated IMG archive.

The scan happens once at startup; when the same filename occurs more than once,
the source from the newest archive wins. REVC_CUSTOM_DIR=<path> selects another
folder. All startup, custom-file, streaming and crash diagnostics go into the
single debug.log next to the game (no REVC_CUSTOM_LOG setting needed). On
macOS and Android the log follows the user-files / storage-root location.

See CUSTOM_MODELS.md for the full format and compatibility details.
