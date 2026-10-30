User IMGs moved to custom/img/
==============================

This game-root img/ folder is NOT scanned for additional custom assets.
Move each compatible IMG/DIR pair to ../custom/img/ instead, for example:

    custom/img/first.img   custom/img/first.dir
    custom/img/second.img  custom/img/second.dir

Pairs may be in subdirectories (up to eight levels). Each is a VC/GTA3 IMG
v1: raw 2048-byte sectors with a separate 32-byte-record .dir. San Andreas
IMG v2 with an embedded VER2 directory is not mounted. The DIR names must
match IDE model names. Only DFF and TXD entries are used from user IMGs;
COL and IFP entries belong in custom/collision/ and custom/anim/.

An exact-name model.txd is searched across ALL mounted custom/img/ files,
even if the DFF is in another IMG and IDE specifies another TXD. If no
same-name TXD exists, the original IDE TXD name is retained. Multiple IMGs
use the normal streamer priority. Native Vice City IMG archives declared by
the game remain available; they are not replaced by this custom layout.

The IMG stream format has 256 simultaneous slots (including stock archives)
and each IMG path must fit in 63 bytes. Invalid/missing DIR pairs, IMG v2,
slot overflows and unsupported entries are logged in debug.log. Models still
stream/evict normally; cache ceilings are not increased. Place compatible
IDE/IPL under ../custom/data/maps/ and collisions under ../custom/collision/.
See ../../CUSTOM_MODELS.md and ../../PC_WORLD_MODDING.md.
