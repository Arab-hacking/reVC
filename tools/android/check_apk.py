#!/usr/bin/env python3
"""Reject an Android APK that contains only the Java launcher (no native game)."""

import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

# These are provided by Android itself, not packaged in the APK. Any other
# DT_NEEDED dependency must be present under lib/<abi>/ in the package.
ANDROID_SYSTEM_LIBS = {
    "libandroid.so", "libc.so", "libdl.so", "libm.so", "liblog.so",
    "libz.so", "libstdc++.so", "libEGL.so", "libGLESv1_CM.so",
    "libGLESv2.so", "libGLESv3.so", "libOpenSLES.so", "libatomic.so",
}

REQUIRED_LIBRARIES = {
    "armeabi-v7a": (1, 40, "libOpenAL32.so"),
    "arm64-v8a": (2, 183, "libOpenAL64.so"),
}


def check(apk_path: Path) -> None:
    with zipfile.ZipFile(apk_path) as apk:
        files = set(apk.namelist())
        for abi, (elf_class, machine, openal) in REQUIRED_LIBRARIES.items():
            for library in ("librevc.so", "libSDL2.so", "libmpg123.so", openal):
                name = f"lib/{abi}/{library}"
                if name not in files:
                    raise ValueError(f"Missing native library: {name}")
                info = apk.getinfo(name)
                if info.file_size < 4096:
                    raise ValueError(f"Native library is implausibly small: {name}")
                with apk.open(name) as library_file:
                    header = library_file.read(20)
                if (len(header) < 20 or header[:4] != b"\x7fELF" or
                        header[4] != elf_class or
                        int.from_bytes(header[18:20], "little") != machine):
                    raise ValueError(f"Wrong ELF ABI: {name}")

            # Existence alone is insufficient: SDLActivity invokes SDL_main
            # by name, and the Android linker resolves ELF DT_NEEDED by SONAME.
            with tempfile.TemporaryDirectory() as temp:
                executable = Path(temp) / "librevc.so"
                with apk.open(f"lib/{abi}/librevc.so") as source, executable.open("wb") as target:
                    shutil.copyfileobj(source, target)
                symbols = subprocess.run(
                    ["readelf", "--dyn-syms", "--wide", str(executable)],
                    check=True, capture_output=True, text=True).stdout
                if not re.search(r"\bSDL_main\b", symbols):
                    raise ValueError(f"SDL_main is not exported by {abi} game library")
                if "Java_org_libsdl_app_SDLActivity_nativeSetupJNI" in symbols:
                    raise ValueError(f"reVC shadows libSDL2's JNI setup on {abi}")
                dynamic = subprocess.run(
                    ["readelf", "-d", str(executable)],
                    check=True, capture_output=True, text=True).stdout
                for dependency in re.findall(r"Shared library: \[([^\]]+)\]", dynamic):
                    if (dependency not in ANDROID_SYSTEM_LIBS and
                            f"lib/{abi}/{dependency}" not in files):
                        raise ValueError(f"Unpackaged dependency of {abi} game: {dependency}")

        for name in ("assets/data/freeroam_miami.scm",
                     "assets/models/generic.txd",
                     "assets/gamecontrollerdb.txt"):
            if name not in files:
                raise ValueError(f"Missing reVC support file: {name}")
        for name in ("assets/data/gta_vc.dat", "assets/models/gta3.img"):
            if name in files:
                raise ValueError(f"Copyrighted game data must not be bundled: {name}")
    print(f"Native reVC and dependencies (both Android ABIs) found in {apk_path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: check_apk.py <apk path>")
    check(Path(sys.argv[1]))
