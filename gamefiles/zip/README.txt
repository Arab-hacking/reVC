No ZIP import from this folder
==============================

No ZIP file in this folder is scanned. The ONE exception elsewhere is
../custom/common.zip: it supplies only data/maps IDE/IPL, data/config.dat
for IPL selection and data/timecyc.json for lighting. Other ZIPs in custom/
are ignored. Reflection-mask and UV2/lightmap .btx inside ANY ZIP are ignored;
there is no graphics-only ZIP path or automatic mask/lightmap injection.
The game's native Vice City files and built-in graphics still work.

Put compatible DFF/TXD/COL/CLS in VC IMG v1 + DIR pairs under ../custom/img/;
IFP under ../custom/anim/; and loose IDE/IPL under ../custom/data/maps/.
Optional loose ../custom/data/timecyc.json takes precedence over the entry
in ../custom/common.zip. See ../../CUSTOM_MODELS.md.
