Additional VC IMG archives
==========================

Put all additional archives here, next to their matching .dir files:

    img/first.img   img/first.dir
    img/second.img  img/second.dir

Subdirectories are searched up to eight levels deep. Names are matched
case-insensitively. No CDIMAGE directive is needed in a DAT file. The game
mounts the archives before indexing models/textures; the names in the DIR
must match IDE model names. Each IMG is a Vice City / GTA3 IMG v1 (raw
2048-byte sectors, separate 32-byte-record .dir), NOT a San Andreas IMG v2
with an embedded VER2 directory. The DFF entries themselves may be SA
RenderWare static models; TXDs may contain PC-native D3D8 (VC) or D3D9 (SA)
textures. For a model.dff + model.txd pair in the SAME auto-mounted IMG,
the game picks model.txd even if the IDE's second column names a different
TXD. No IDE edit or separate "slot" file is required. If no pair exists,
the original IDE TXD name is used. Duplicate names follow IMG priority;
matching names from different images do not count as a pair. Texture names
INSIDE the selected TXD must match DFF material names. The TXD is loaded
before the DFF. Multi-part static clumps are
flattened into one VC atomic with their child-frame transforms, materials
and UVs preserved. Stock VC multi-LOD objects still use their original path.
Skinned/oversized/invalid static clumps are rejected and logged in debug.log.
Real archives still need to be checked in the running game.

Only DFF and TXD entries are used from these IMGs. Collision is read from
.col and .cls files inside ZIP archives under ../custom/, and IDE/IPL from
../custom/common.zip. COL/CLS files in IMGs or loose custom/ files are ignored
for this layout. Malformed or missing DIR files are logged in debug.log and
skipped. If an asset is in both an img/ IMG and custom ZIP, the IMG DFF/TXD is
streamed directly (the old .mod/.btx override is not decoded). Normal RW
streaming/eviction and existing cache budgets are unchanged.

The on-disk stream format limits the game to 256 simultaneous IMG slots
(including stock IMG files); each IMG path must fit within 63 characters.
The TXD name pool holds up to 32768 distinct names (including the stock game),
not 32768 resident textures. Stock DIR names are reserved before these IMG
entries are indexed; 32 slots are kept for later UI/script dictionaries.
Excess new IMG TXD entries are skipped and counted in debug.log.
