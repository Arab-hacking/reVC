#!/usr/bin/env python3
"""Regression test for the standalone custom ZIP reader.

Builds ordinary STORE/DEFLATE archives and a sparse >2 GiB ZIP to exercise
extraction, overlapping DEFLATE matches, and 64-bit file offsets without
requiring a game build or a large physical fixture.
"""

import os
import random
import struct
import subprocess
import tempfile
import time
import zlib
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/extras/custom/CustomZip.cpp"
HEADER_DIR = ROOT / "src/extras/custom"
SHIM_DIR = ROOT / "tools/custom-folder-verification/shim"
TEST_SOURCE = ROOT / "tools/custom-folder-verification/test_customzip.cpp"


def write_expected(directory, name, payload):
    (directory / name).write_bytes(payload)


def make_sparse_zip(path, name, payload, central_offset):
    """Write one stored entry with its central directory beyond the 2 GiB mark."""
    encoded_name = name.encode("ascii")
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    local = struct.pack(
        "<IHHHHHIIIHH",
        0x04034B50, 20, 0, 0, 0, 0, crc, len(payload), len(payload),
        len(encoded_name), 0,
    ) + encoded_name + payload
    central = struct.pack(
        "<IHHHHHHIIIHHHHHII",
        0x02014B50, 20, 20, 0, 0, 0, 0, crc, len(payload), len(payload),
        len(encoded_name), 0, 0, 0, 0, 0, 0,
    ) + encoded_name
    eocd = struct.pack(
        "<IHHHHIIH", 0x06054B50, 0, 0, 1, 1, len(central), central_offset, 0
    )
    if len(local) > central_offset:
        raise ValueError("sparse central directory overlaps the local entry")
    with path.open("wb") as archive:
        archive.write(local)
        archive.seek(central_offset)
        archive.write(central)
        archive.write(eocd)


def main():
    rng = random.Random(0x5EED)
    with tempfile.TemporaryDirectory(prefix="revc-customzip-") as temp:
        temp = Path(temp)
        binary = temp / "test_customzip"
        subprocess.run(
            [
                "g++", "-std=c++11", "-O2", "-Wall", "-Wextra",
                "-I", str(SHIM_DIR), "-I", str(HEADER_DIR),
                str(TEST_SOURCE), str(SOURCE), "-o", str(binary),
            ],
            check=True,
            cwd=ROOT,
        )

        expected_dir = temp / "expected"
        expected_dir.mkdir()
        archive_path = temp / "cases.zip"
        cases = [
            ("empty.bin", b"", zipfile.ZIP_STORED, None),
            ("stored-small.bin", b"stored entry\x00with bytes", zipfile.ZIP_STORED, None),
            ("stored-random.bin", rng.randbytes(256 * 1024), zipfile.ZIP_STORED, None),
            ("repeat-distance-one.bin", b"A" * (1024 * 1024), zipfile.ZIP_DEFLATED, 9),
            ("repeat-periodic.bin", (b"VC-Streaming/Texture/\x00" * 50000), zipfile.ZIP_DEFLATED, 6),
            ("mixed-pattern.bin", bytes((i * i + 31 * i + 7) % 251 for i in range(300000)), zipfile.ZIP_DEFLATED, 1),
            ("random-deflate.bin", rng.randbytes(384 * 1024), zipfile.ZIP_DEFLATED, 9),
        ]
        with zipfile.ZipFile(archive_path, "w", allowZip64=True) as archive:
            for name, payload, method, level in cases:
                if level is None:
                    archive.writestr(name, payload, compress_type=method)
                else:
                    archive.writestr(name, payload, compress_type=method, compresslevel=level)
                write_expected(expected_dir, name, payload)
        subprocess.run([str(binary), str(archive_path), str(expected_dir)], check=True)

        # Keep the archive sparse: its physical data is tiny, while the central
        # directory offset exercises the 64-bit seek path used by large packs.
        sparse_name = "sparse-entry.bin"
        sparse_data = b"offsets beyond 2 GiB work"
        sparse_path = temp / "sparse.zip"
        sparse_expected = temp / "sparse-expected"
        sparse_expected.mkdir()
        write_expected(sparse_expected, sparse_name, sparse_data)
        make_sparse_zip(sparse_path, sparse_name, sparse_data, 2 * 1024**3 + 4096)
        subprocess.run([str(binary), str(sparse_path), str(sparse_expected)], check=True)


if __name__ == "__main__":
    main()
