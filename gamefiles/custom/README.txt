Custom models folder
====================

Drop zip archives with the community's custom model containers into this folder:

    <model name>.mod   a model,
    <texture name>.btx a texture,
    <collision name>.cls collision.

The names have to be the names the game asks its files for (the model name from
the IDE / vehicles.ide, and the texture names the model references). Everything
is converted in memory at runtime - the game never writes a temporary file, and
the folder wins over the game's own IMG archives.

Archives are scanned once, at the start of the game; sub folders are scanned as
well (up to four levels). If the same file is in several archives, the newest
archive wins.

Set REVC_CUSTOM_LOG=1 to get a log of what is read and converted
(custom_models.log next to the game), or REVC_CUSTOM_DIR=<path> to use another
folder.

See CUSTOM_MODELS.md for the whole story.
