Custom models and animations
============================

Place loose files here or put them in .zip archives (subdirectories up to four
levels are scanned). Files are indexed by the final filename, case-insensitively:

    <model>.mod / <model>.dff       model (BR / RenderWare)
    <texture>.btx / <slot>.txd     texture / texture dictionary
    <model>.cls / <slot>.col       collision
    <block>.ifp / <block>.ani     animation dictionary

For example, custom/ped.ifp (or anim/ped.ifp in a zip inside custom/) is
considered when the game loads the PED animation block. A valid Vice City
ANPK file with matching block name replaces that VC block; an SA ANP2/ANP3
file is loaded as an additional sa_ped bank and leaves the VC block intact.
Invalid or incompatible custom animation files fall back to the game's IFP.
The custom/ folder is not MODELS/CUSTOM.IMG; that is an unrelated IMG archive.

The scan happens once at startup; when the same filename occurs more than once,
the source from the newest archive wins. REVC_CUSTOM_DIR=<path> selects another
folder. All startup, custom-file, streaming and crash diagnostics go into the
single debug.log next to the game (no REVC_CUSTOM_LOG setting needed). On
macOS and Android the log follows the user-files / storage-root location.

See CUSTOM_MODELS.md for the full format and compatibility details.
