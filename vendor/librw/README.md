librw
=====

This library is supposed to be a re-implementation of RenderWare graphics,
or a good part of it anyway.

It is intended to be cross-platform in two senses:
support rendering on different platforms similar to RW;
supporting all file formats for all platforms at all times and provide
way to convert to all other platforms.

Supported file formats are DFF and TXD for D3D8, D3D9 and Xbox.
BSP is not supported at all.

For rendering we have D3D9 and OpenGL (>=2.1, ES >= 2.0) backends.

# Uses

librw can be used for rendering [GTA](https://github.com/gtamodding/re3).

# Building

Get premake5. Generate a config, e.g. with ``premake5 gmake``,
and look in the build directory.
