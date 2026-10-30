User ZIP import disabled
========================

No ZIP file in this folder (or in custom/) is scanned or read for user
resources. This also includes reflection-mask and UV2/lightmap .btx files:
there is no graphics-only ZIP exception or automatic mask/lightmap injection.
The game's native Vice City files and built-in graphics still work.

Put compatible DFF/TXD/COL/CLS in VC IMG v1 + DIR pairs under ../custom/img/;
IFP under ../custom/anim/; and IDE/IPL under ../custom/data/maps/.
Optional loose ../custom/data/timecyc.json
adjusts light and native shadow strengths. See ../../CUSTOM_MODELS.md.
