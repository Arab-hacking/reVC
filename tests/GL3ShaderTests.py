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


def strip_comments(source):
    """GLSL comments are not code: a uniform named in a comment is not a use."""
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


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
        # These are the shader declarations selected by librw/gl3device.cpp (including ES 3.0).
        for version in ("330", "120", "310es", "300es", "100es"):
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
        for version in ("330", "120", "310es", "300es", "100es"):
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

    @unittest.skipUnless(VALIDATOR, "install glslangValidator to link GL3 shaders")
    def test_mobile_terrain_and_water_shaders_link(self):
        pipes_cpp = (ROOT / "src/extras/custompipes_gl.cpp").read_text()
        water_cpp = (ROOT / "src/renderer/WaterLevel.cpp").read_text()
        def extract_cstr(text, var):
            blk = text.split("static const char *" + var + " =\n", 1)[1].split(";\n", 1)[0]
            return "".join(ast.literal_eval(t) for t in re.findall(r'"(?:[^"\\]|\\.)*"', blk))
        ter_vs = extract_cstr(pipes_cpp, "neoTerrain_vert_src")
        ter_fs = extract_cstr(pipes_cpp, "neoTerrain_frag_src")
        wat_vs = extract_cstr(water_cpp, "s_mobileWaterVS")
        wat_fs = extract_cstr(water_cpp, "s_mobileWaterFS")
        for version in ("330", "120", "310es", "300es", "100es"):
            decl = shader_decl(version)
            with tempfile.TemporaryDirectory(prefix="revc-gl3-mobile-") as folder:
                vs_path = Path(folder) / "mob.vert"
                fs_path = Path(folder) / "mob.frag"
                with self.subTest(version=version, program="neoTerrain"):
                    vs_path.write_text(decl + FULL_LIGHTS + embedded_source(VENDOR / "header.vert") + ter_vs)
                    fs_path.write_text(decl + embedded_source(VENDOR / "header.frag") + ter_fs)
                    result = subprocess.run([VALIDATOR, "-l", str(vs_path), str(fs_path)],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout)
                with self.subTest(version=version, program="mobileWater"):
                    vs_path.write_text(decl + embedded_source(VENDOR / "header.vert") + wat_vs)
                    fs_path.write_text(decl + embedded_source(VENDOR / "header.frag") + wat_fs)
                    result = subprocess.run([VALIDATOR, "-l", str(vs_path), str(fs_path)],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout)

    def test_all_used_uniforms_are_declared(self):
        """Every uniform a body reads must be declared by its header.

        This catches the class of bug where a body starts using a new uniform
        (e.g. u_paraboloid for the paraboloid capture) but only the vertex
        header declares it - the program then fails to link on the device.
        """
        declared = set()
        for header in ("header.vert", "header.frag"):
            for line in strip_comments(embedded_source(VENDOR / header)).splitlines():
                m = re.match(r"\s*uniform\s+(?:\w+\s+)*(\w+)\s*\[", line)
                if m:
                    declared.add(m.group(1))
                m = re.match(r"\s*uniform\s+(?:\w+\s+)*(\w+)\s*(?:=|;)", line)
                if m:
                    declared.add(m.group(1))
        # locals/defines that look like uniforms but are not
        allow = {"u_materialAmbientInstancing"}
        bodies = [VENDOR / n for n in ("default.vert", "skin.vert", "simple.frag",
                                       "matfx_env.vert", "matfx_env.frag", "im2d.vert", "im3d.vert")]
        bparts = ", ".join(map(str, bodies))
        declared_extra = set(declared)
        for body in bodies:
            for line in strip_comments(embedded_source(body)).splitlines():
                m = re.match(r"\s*uniform\s+(?:\w+\s+)*(\w+)", line)
                if m:
                    declared_extra.add(m.group(1))
        for body in bodies:
            text = strip_comments(embedded_source(body))
            used = set(re.findall(r"\bu_[A-Za-z0-9_]+", text))
            for name in sorted(used):
                if name in declared_extra or name in allow:
                    continue
                self.fail("%s uses undeclared uniform %s" % (body.name, name))

    def test_paraboloid_capture_contract(self):
        """The capture pass and the material sampling must use one mapping.

        The client writes the env map with the polar projection (0x42b527) and
        samples it with the matching mapping (0x434bb8). Getting the producer
        and the consumer out of step is invisible in a build and only shows up
        as "reflections look wrong", so pin both halves here.
        """
        h_vs = embedded_source(VENDOR / "header.vert")
        # 0x42b527, verbatim arithmetic
        self.assertIn("vec3 ReflVector = worldPos - u_eye.xyz;", h_vs)
        self.assertIn("ReflPos.xy = planar * (ReflPos.z * 0.5 + 0.5);", h_vs)
        self.assertIn("length(ReflVector) * 0.002", h_vs)
        self.assertIn("uniform vec4 u_paraboloid;", h_vs)
        self.assertIn("uniform vec4 u_paraboloid;", embedded_source(VENDOR / "header.frag"))

        # Every vertex stage that computes gl_Position from a perspective
        # matrix must honour the capture pass, otherwise that geometry lands in
        # the env map with the wrong projection (and the map is garbage).
        stages = [VENDOR / "default.vert", VENDOR / "skin.vert", VENDOR / "matfx_env.vert"] + \
                 [EXTRAS / n for n in ("neoVehicle.vert", "neoRim.vert", "neoRimSkin.vert",
                                       "default_UV2.vert", "neoGloss.vert")]
        for vs in stages:
            text = embedded_source(vs)
            with self.subTest(stage=vs.name):
                self.assertIn("u_paraboloid.x > 0.5 ? ParaboloidProjection(", text)

        # The vehicle material shader samples the captured map with the polar
        # mapping the capture writes (not a perspective UV matrix).
        veh = embedded_source(EXTRAS / "neoVehicle.frag")
        self.assertIn("SampleVehicleParaboloidEnv", veh)
        # ...using the capture's own polar mapping, not a stereographic one.
        self.assertIn("rplanar * (r.z * 0.5 + 0.5) * 0.5 + 0.5", veh)
        self.assertNotIn("1.0 + rp.z", veh)
        # ...and custompipes actually feeds the capture into that texture unit.
        pipes = (ROOT / "src/extras/custompipes.cpp").read_text()
        self.assertIn("beginParaboloidPass", pipes)
        self.assertIn("setCaptureEyePosition", pipes)
        pipes_gl = (ROOT / "src/extras/custompipes_gl.cpp").read_text()
        # Unit 1 must receive the texture the capture writes, or a stand-in that
        # the accessor returns when reflections are switched off. Both come from
        # the same helper, so the unit can never inherit the previous material.
        self.assertIn("setTexture(1, EnvMapTextureForShading())", pipes_gl)
        pipes_cpp = (ROOT / "src/extras/custompipes.cpp").read_text()
        self.assertIn("EnvMapTextureForShading(void)", pipes_cpp)
        pipes_d3d = (ROOT / "src/extras/custompipes_d3d9.cpp").read_text()
        self.assertIn("d3d::setTexture(1, EnvMapTextureForShading())", pipes_d3d)
        self.assertIn("return EnvMapTex ? EnvMapTex : BlackEnvTex;", pipes_cpp)

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
                    "SampleSunShadow", "BR_INV_PI", "u_colorGrade",
                    "BR_ApplyMobileColorGrade", "BR_ShadeMobileWorldLinear"):
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

    def test_inline_mobile_shader_sampler_and_varying_contracts(self):
        h_fs = embedded_source(VENDOR / "header.frag")
        pipes_cpp = (ROOT / "src/extras/custompipes_gl.cpp").read_text()
        water_cpp = (ROOT / "src/renderer/WaterLevel.cpp").read_text()
        sky_cpp = (ROOT / "src/extras/custom/CustomSky.cpp").read_text()

        def extract_cstr(text, var):
            blk = text.split("static const char *" + var + " =\n", 1)[1].split(";\n", 1)[0]
            return "".join(ast.literal_eval(t) for t in re.findall(r'"(?:[^"\\]|\\.)*"', blk))

        pairs = [
            ("skyDome", extract_cstr(sky_cpp, "s_skyDomeVertSrc"), extract_cstr(sky_cpp, "s_skyDomeFragSrc")),
            ("neoTerrain", extract_cstr(pipes_cpp, "neoTerrain_vert_src"), extract_cstr(pipes_cpp, "neoTerrain_frag_src")),
            ("mobileWater", extract_cstr(water_cpp, "s_mobileWaterVS"), extract_cstr(water_cpp, "s_mobileWaterFS")),
        ]
        for label, vs_body, fs_body in pairs:
            combined_fs = h_fs + "\n" + fs_body
            for sampler in set(re.findall(r"\btexture\s*\(\s*(tex\d+)\b", fs_body)):
                self.assertRegex(
                    combined_fs,
                    rf"uniform\s+sampler2D(?:Shadow)?\s+{re.escape(sampler)}\s*;",
                    f"{label} uses undeclared sampler {sampler}",
                )
            vs_outs = set(re.findall(r"VSOUT\s+(\w+\s+\w+)\s*;", vs_body))
            fs_ins = set(re.findall(r"FSIN\s+(\w+\s+\w+)\s*;", fs_body))
            self.assertEqual(fs_ins, vs_outs, f"{label} VSOUT/FSIN mismatch")

        self.assertNotIn("registerUniform(", sky_cpp)
        self.assertIn("s_mobileWaterShaderTried", water_cpp)

    def test_mobile_matfx_vehicle_and_sky_contracts(self):
        h_fs = embedded_source(VENDOR / "header.frag")
        matfx_fs = embedded_source(VENDOR / "matfx_env.frag")
        veh_fs = embedded_source(EXTRAS / "neoVehicle.frag")
        world_fs = embedded_source(EXTRAS / "neoWorldVC.frag")
        sky_cpp = (ROOT / "src/extras/custom/CustomSky.cpp").read_text()
        renderer_cpp = (ROOT / "src/renderer/Renderer.cpp").read_text()
        postfx_cpp = (ROOT / "src/extras/postfx.cpp").read_text()

        # Mobile world/shadow/gamma parity: 0.30 shadow cap, pure pow(color, u_localInvGamma), lightmapMulti=0.65
        self.assertIn("0.30 * (1.0 - mVis)", h_fs)
        self.assertNotIn("over / (vec3(1.0) + over * 4.0)", h_fs)
        self.assertIn("0.65", world_fs)
        self.assertIn("0.65f", renderer_cpp)
        self.assertIn("int CPostFX::EffectSwitch = POSTFX_OFF;", postfx_cpp)

        # Mobile matfx_env & neoVehicle use Fresnel [0.15, 0.70] + pow(env, 0.8) + additive clearcoat
        self.assertIn("if(u_atmosphere.x >= 1.5)", matfx_fs)
        self.assertIn("0.15, 0.70", matfx_fs)
        self.assertIn("BR_ShadeMobileWorldLinear(", matfx_fs)
        self.assertIn("BR_ApplyMobileColorGrade(", matfx_fs)

        self.assertIn("0.15, 0.70", veh_fs)
        self.assertIn("vec3(0.8)", veh_fs)
        self.assertIn("u_sceneLightPosRad[li]", veh_fs)
        self.assertIn("u_sceneLightColor[li]", veh_fs)
        self.assertIn("if(isGlassM > 0.05)", veh_fs)

        # Procedural SkyDome restores prevAtmos and gates mobile stars strictly to hours 0..4 and 23
        self.assertIn("uniform vec4 u_skyTopColor;", sky_cpp)
        self.assertIn("rw::setAtmosphereParams(prevAtmos);", sky_cpp)
        self.assertIn("starVis", sky_cpp)
        self.assertIn("(curHour <= 4 || curHour > 22) ? 1.0f : 0.0f", sky_cpp)

        for name, src in (("header.frag", h_fs), ("matfx_env.frag", matfx_fs),
                          ("neoVehicle.frag", veh_fs), ("neoWorldVC.frag", world_fs)):
            self.assertEqual(src.count("{"), src.count("}"), f"{name} unbalanced braces")
            self.assertEqual(src.count("("), src.count(")"), f"{name} unbalanced parens")


if __name__ == "__main__":
    unittest.main()
