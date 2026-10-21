Vehicle reflection masks and UV2 lightmaps (graphics only)
=========================================================

Place your ZIP archives here. Only selected .btx entries are used for these
effects; models, general textures, audio, collision, and map files in this
folder are ignored. The ZIPs are indexed at startup and individual selected
BTX entries are decoded on demand. Do not use .tmb 8x8 preview textures in
place of full-sized assets, and do not put .bpc files here.

A CarReflectionMask.btx can replace the Neo render target's mask. A model's
UV2 + MatFX DUAL material can use a referenced *_lightmap.btx or *_lm.btx.
The environment reflection itself is rendered from the game scene, not read
from an image file. The current camera projection is Neo, not the separate
Black Russia paraboloid pass: visual parity has not been verified.

See PC_GRAPHICS_ZIP.md at the repository root and debug.log next to the game.
