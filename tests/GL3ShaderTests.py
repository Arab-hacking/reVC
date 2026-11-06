"""Check the GLSL actually embedded in the GL3 build, not just the .vert/.frag files.

Set GLSLANG_VALIDATOR to glslangValidator (or glslang-standalone) to link the
same shader combinations as librw and CustomPipes. Source/embed parity is
checked even on machines without a GLSL compiler.
"""

import ast
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor/librw/src/gl/shaders"
EXTRAS = ROOT / "src/extras/shaders"
VALIDATOR = (os.environ.get("GLSLANG_VALIDATOR") or
             shutil.which("glslangValidator") or shutil.which("glslang"))

VENDOR_INCLUDES = {
    "header.vert": "header_vs.inc", "header.frag": "header_fs.inc",
    "default.vert": "default_vs_gl.inc", "simple.frag": "simple_fs_gl.inc",
    "skin.vert": "skin_gl.inc", "matfx_env.vert": "matfx_gl.inc",
    "matfx_env.frag": "matfx_gl.inc", "im2d.vert": "im2d_gl.inc",
    "im3d.vert": "im3d_gl.inc",
}


def embedded_source(source):
    """Decode the C strings from the .inc used by the renderer at runtime."""
    if source.parent == VENDOR:
        inc = VENDOR / VENDOR_INCLUDES[source.name]
    else:
        inc = EXTRAS / "obj" / (source.stem + "_" + source.suffix[1:] + ".inc")
    symbol = source.stem + "_" + source.suffix[1:] + "_src"
    block = inc.read_text().split("const char *" + symbol + " =\n", 1)[1]
    block = block.split("\n;", 1)[0]
    return "".join(ast.literal_eval(line) for line in block.splitlines() if line.strip())


def shader_decl(version):
    cpp = (ROOT / "vendor/librw/src/gl/gl3device.cpp").read_text()
    block = cpp.split("const char *shaderDecl" + version + " =\n", 1)[1]
    block = block.split('";', 1)[0] + '"'
    return "".join(ast.literal_eval(token) for token in
                   re.findall(r'"(?:[^"\\]|\\.)*"', block))


# (name, VS, FS, VS defines, FS defines). The extras use their own simple.frag.
FULL_LIGHTS = "#define DIRECTIONALS\n#define POINTLIGHTS\n#define SPOTLIGHTS\n"
PROGRAMS = [
    ("default", VENDOR / "default.vert", VENDOR / "simple.frag", "", "#define BRMATERIAL\n"),
    ("default_full", VENDOR / "default.vert", VENDOR / "simple.frag", FULL_LIGHTS, "#define BRMATERIAL\n"),
    ("skin", VENDOR / "skin.vert", VENDOR / "simple.frag", "", "#define BRMATERIAL\n#define BRSKIN\n"),
    ("skin_full", VENDOR / "skin.vert", VENDOR / "simple.frag", FULL_LIGHTS, "#define BRMATERIAL\n#define BRSKIN\n"),
    ("matfx", VENDOR / "matfx_env.vert", VENDOR / "matfx_env.frag", "", ""),
    ("matfx_full", VENDOR / "matfx_env.vert", VENDOR / "matfx_env.frag", FULL_LIGHTS, ""),
    ("shadow_static", VENDOR / "default.vert", VENDOR / "simple.frag", "", "#define SHADOW_DEPTH_PASS\n"),
    ("shadow_skin", VENDOR / "skin.vert", VENDOR / "simple.frag", "", "#define SHADOW_DEPTH_PASS\n"),
    ("im2d", VENDOR / "im2d.vert", VENDOR / "simple.frag", "", ""),
    ("im3d", VENDOR / "im3d.vert", VENDOR / "simple.frag", "", ""),
    ("neo_vehicle", EXTRAS / "neoVehicle.vert", EXTRAS / "neoVehicle.frag", FULL_LIGHTS, ""),
    ("neo_gloss", EXTRAS / "neoGloss.vert", EXTRAS / "neoGloss.frag", "", ""),
    ("neo_world", EXTRAS / "default_UV2.vert", EXTRAS / "neoWorldVC.frag", FULL_LIGHTS, ""),
    ("neo_rim", EXTRAS / "neoRim.vert", EXTRAS / "simple.frag", FULL_LIGHTS, ""),
    ("neo_rim_skin", EXTRAS / "neoRimSkin.vert", EXTRAS / "simple.frag", FULL_LIGHTS, ""),
    ("postfx_colourfilter", EXTRAS / "im2d.vert", EXTRAS / "colourfilterVC.frag", "", ""),
    ("postfx_contrast", EXTRAS / "im2d.vert", EXTRAS / "contrast.frag", "", ""),
]


class GL3ShaderTests(unittest.TestCase):
    def test_embedded_sources_match(self):
        for directory in (VENDOR, EXTRAS):
            for source in list(directory.glob("*.vert")) + list(directory.glob("*.frag")):
                with self.subTest(source=source.relative_to(ROOT)):
                    # The .inc generators omit empty lines, but not whitespace-only lines.
                    original = "".join(line for line in source.read_text().splitlines(keepends=True)
                                       if line not in ("\n", "\r\n"))
                    self.assertEqual(original, embedded_source(source))

    @unittest.skipUnless(VALIDATOR, "install glslangValidator to link GL3 shaders")
    def test_runtime_shader_programs_link(self):
        # These are the four shader declarations selected by librw/gl3device.cpp.
        for version in ("330", "120", "310es", "100es"):
            decl = shader_decl(version)
            with tempfile.TemporaryDirectory(prefix="revc-gl3-shaders-") as folder:
                vs_path = Path(folder) / "program.vert"
                fs_path = Path(folder) / "program.frag"
                for name, vs, fs, vs_defines, fs_defines in PROGRAMS:
                    alpha_variants = (False, True) if name.startswith(("default", "skin", "matfx", "shadow_")) else (False,)
                    for no_alpha_test in alpha_variants:
                        with self.subTest(version=version, program=name, no_alpha_test=no_alpha_test):
                            vs_path.write_text(decl + vs_defines + embedded_source(VENDOR / "header.vert") + embedded_source(vs))
                            fs_path.write_text(decl + fs_defines + ("#define NO_ALPHATEST\n" if no_alpha_test else "") +
                                               embedded_source(VENDOR / "header.frag") + embedded_source(fs))
                            result = subprocess.run([VALIDATOR, "-l", str(vs_path), str(fs_path)],
                                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                            self.assertEqual(result.returncode, 0, result.stdout)

    @unittest.skipUnless(VALIDATOR, "install glslangValidator to link GL3 shaders")
    def test_custom_skydome_shader_links(self):
        sky_cpp = (ROOT / "src/extras/custom/CustomSky.cpp").read_text()
        def extract_cstr(var):
            blk = sky_cpp.split("static const char *" + var + " =\n", 1)[1].split(";\n", 1)[0]
            return "".join(ast.literal_eval(t) for t in re.findall(r'"(?:[^"\\]|\\.)*"', blk))
        vs_body = extract_cstr("s_skyDomeVertSrc")
        fs_body = extract_cstr("s_skyDomeFragSrc")
        for version in ("330", "120", "310es", "100es"):
            decl = shader_decl(version)
            with tempfile.TemporaryDirectory(prefix="revc-gl3-skydome-") as folder:
                vs_path = Path(folder) / "skydome.vert"
                fs_path = Path(folder) / "skydome.frag"
                with self.subTest(version=version):
                    vs_path.write_text(decl + embedded_source(VENDOR / "header.vert") + vs_body)
                    fs_path.write_text(decl + embedded_source(VENDOR / "header.frag") + fs_body)
                    result = subprocess.run([VALIDATOR, "-l", str(vs_path), str(fs_path)],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout)

    def test_view_matrix_eye_and_env_uniform_sync(self):
        gl3_cpp = (ROOT / "vendor/librw/src/gl/gl3device.cpp").read_text()
        # Column-major view inverse translation: E = -R^T * t (columns 0,1,2 dotted with col 3)
        self.assertIn("eye[0] = -(mat[0]*mat[12] + mat[1]*mat[13] + mat[2]*mat[14]);", gl3_cpp)
        self.assertIn("eye[1] = -(mat[4]*mat[12] + mat[5]*mat[13] + mat[6]*mat[14]);", gl3_cpp)
        self.assertIn("eye[2] = -(mat[8]*mat[12] + mat[9]*mat[13] + mat[10]*mat[14]);", gl3_cpp)
        self.assertIn("syncEnvironmentUniforms()", gl3_cpp)
        engine_cpp = (ROOT / "vendor/librw/src/engine.cpp").read_text()
        self.assertGreaterEqual(engine_cpp.count("gl3::syncEnvironmentUniforms();"), 4)

    def test_blayz_pbr_sky_and_postfx_contracts(self):
        h_vs = embedded_source(VENDOR / "header.vert")
        h_fs = embedded_source(VENDOR / "header.frag")
        skin_vs = embedded_source(VENDOR / "skin.vert")
        rim_skin_vs = embedded_source(EXTRAS / "neoRimSkin.vert")
        veh_fs = embedded_source(EXTRAS / "neoVehicle.frag")
        world_fs = embedded_source(EXTRAS / "neoWorldVC.frag")
        post_fs = embedded_source(EXTRAS / "colourfilterVC.frag")
        sky_cpp = (ROOT / "src/extras/custom/CustomSky.cpp").read_text()
        skin_cpp = (ROOT / "vendor/librw/src/gl/gl3skin.cpp").read_text()

        # Both skin pipelines register u_boneMatrices as UNIFORM_MAT4[64]
        self.assertIn('registerUniform("u_boneMatrices", UNIFORM_MAT4, 64)', skin_cpp)
        self.assertIn("uniform mat4 u_boneMatrices[64];", skin_vs)
        self.assertIn("uniform mat4 u_boneMatrices[64];", rim_skin_vs)
        self.assertNotIn("u_boneMatrices[64*3]", rim_skin_vs)

        for sym in ("BR_EvalBuildingVertexColor", "BR_EvalDynamicVertexColor", "DoPointSpotLight"):
            self.assertIn(sym, h_vs)
        for sym in ("BR_SRGBtoLINEAR", "BR_TonemapAces", "BR_StandardBRDF",
                    "BR_GetEnvBRDFApprox", "BR_GetSkySHDiffuse", "BR_ShadeMaterialHDR",
                    "SampleSunShadow", "BR_INV_PI"):
            self.assertIn(sym, h_fs)
        self.assertIn("SampleVehicleParaboloidEnv", veh_fs)
        self.assertIn("u_brLightmap > 0.5", world_fs)
        self.assertIn("SampleBackbufferFXAA", post_fs)
        self.assertIn("br_sky_fbm", sky_cpp)
        self.assertIn("nightMix", sky_cpp)
        self.assertIn("HasValidRaster", sky_cpp)
        self.assertIn("flags |= rpGEOMETRYLIGHT | rpGEOMETRYMODULATEMATERIALCOLOR;", sky_cpp)
        self.assertIn("openBottom", sky_cpp)
        self.assertIn("#define BRSKIN", skin_cpp)
        for vs_file in (VENDOR / "default.vert", VENDOR / "skin.vert", VENDOR / "matfx_env.vert",
                        EXTRAS / "default_UV2.vert", EXTRAS / "neoRim.vert", EXTRAS / "neoRimSkin.vert"):
            self.assertIn("u_surfProps.w >= 1.5", embedded_source(vs_file))


if __name__ == "__main__":
    unittest.main()
