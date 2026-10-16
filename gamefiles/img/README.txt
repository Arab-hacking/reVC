Additional VC IMG archives
==========================

Put all additional archives here, next to their matching .dir files:

    img/first.img   img/first.dir
    img/second.img  img/second.dir

Subdirectories are searched up to eight levels deep. Names are matched
case-insensitively. No CDIMAGE directive is needed in a DAT file. The game
mounts the archives before indexing models/textures; the names in the DIR
must match IDE model names / TXD slot names. Each IMG is a Vice City / GTA3
IMG v1 (raw 2048-byte sectors, separate 32-byte-record .dir), NOT a San
Andreas IMG v2 with an embedded VER2 directory. The DFF entries themselves
may be SA RenderWare models; the TXD entries should be VC-compatible TXDs.

Only DFF and TXD entries are used from these IMGs. Collision is read from
.col and .cls files inside ZIP archives under ../custom/, and IDE/IPL from
../custom/common.zip. COL/CLS files in IMGs or loose custom/ files are ignored
for this layout. Malformed or missing DIR files are logged in debug.log and
skipped. If an asset is in both an img/ IMG and custom ZIP, the IMG DFF/TXD is
streamed directly (the old .mod/.btx override is not decoded). Normal RW
streaming/eviction and existing cache budgets are unchanged.

The on-disk stream format limits the game to 256 simultaneous IMG slots
(including stock IMG files); each IMG path must fit within 63 characters.
