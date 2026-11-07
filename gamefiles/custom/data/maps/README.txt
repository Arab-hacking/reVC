Place loose *.ide and *.ipl world files here or in subdirectories.
They can also be read from custom/common.zip as data/maps/... entries.
Loose files with the same relative path win over the ZIP copy.
Optional active_ipl.txt selects IPLs by paths relative to custom/:
    data/maps/level/scene.ipl
It overrides data/config.dat inside custom/common.zip; without either,
all IPLs are read. All IDEs are always read. The game-root common/ config.dat
is not used. See ../../../../CUSTOM_MODELS.md.
