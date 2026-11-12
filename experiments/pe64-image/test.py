"""No external dependencies. Tests a trusted compiled PE fixture and corrupted copies."""
import pathlib
import re
import struct
import subprocess
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
IMAGE = (HERE / "fixture.exe").read_bytes()
PE = struct.unpack_from("<I", IMAGE, 0x3C)[0]
COFF = PE + 4
OPT = COFF + 20
SECTIONS = OPT + struct.unpack_from("<H", IMAGE, COFF + 16)[0]
RELOC = SECTIONS + 2 * 40
RELOC_FILE = struct.unpack_from("<I", IMAGE, RELOC + 20)[0]


def u16(data, at, value):
    struct.pack_into("<H", data, at, value)


def u32(data, at, value):
    struct.pack_into("<I", data, at, value)


def run(data):
    with tempfile.TemporaryDirectory(prefix="pe64-bad-fixture-") as directory:
        path = pathlib.Path(directory) / "test.exe"
        path.write_bytes(data)
        return subprocess.run([str(HERE / "pe64_probe"), str(path)],
                              capture_output=True, text=True, timeout=10)


class MinimalPeLoader(unittest.TestCase):
    def test_valid_fixture_multiple_aslr_runs(self):
        for _ in range(5):
            with self.subTest(iteration=_):
                result = run(IMAGE)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("pe64_entry_return=42", result.stdout)
                self.assertRegex(result.stdout, r"applied_dir64=[1-9][0-9]*")
                base = re.search(r"actual_base=(0x[0-9a-f]+) preferred_base=(0x[0-9a-f]+)",
                                 result.stdout)
                self.assertIsNotNone(base)
                self.assertNotEqual(base.group(1), base.group(2))

    def test_corruptions_rejected(self):
        mutations = {
            "truncated": lambda b: b[:80],
            "bad_dos": lambda b: b.__setitem__(slice(0, 2), b"NO"),
            "bad_pe_signature": lambda b: b.__setitem__(slice(PE, PE + 4), b"NOPE"),
            "bad_machine": lambda b: u16(b, COFF, 0x14c),
            "too_many_sections": lambda b: u16(b, COFF + 2, 97),
            "bad_optional_magic": lambda b: u16(b, OPT, 0x10b),
            "bad_image_size": lambda b: u32(b, OPT + 56, 0),
            "bad_section_alignment": lambda b: u32(b, OPT + 32, 512),
            "entry_outside_executable": lambda b: u32(b, OPT + 16, 0x3000),
            "imports_not_supported": lambda b: u32(b, OPT + 112 + 8, 0x2000),
            "tls_not_supported": lambda b: u32(b, OPT + 112 + 9 * 8, 0x2000),
            "missing_reloc_directory": lambda b: u32(b, OPT + 112 + 5 * 8, 0),
            "reloc_block_too_short": lambda b: u32(b, RELOC_FILE + 4, 6),
            "unsupported_reloc_type": lambda b: u16(b, RELOC_FILE + 8, 0x7000),
            "reloc_target_overflow": lambda b: u16(b, RELOC_FILE + 8, 0xAFFF),
            "overlap_sections": lambda b: u32(b, RELOC + 12, 0x2000),
            "write_executable_section": lambda b: u32(b, SECTIONS + 36, 0xE0000020),
            "section_raw_out_of_range": lambda b: u32(b, RELOC + 16, len(b) + 1024),
            "bad_file_offset_alignment": lambda b: u32(b, RELOC + 20, 0x101),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                b = bytearray(IMAGE)
                changed = mutate(b)
                if isinstance(changed, (bytes, bytearray)):
                    b = changed
                result = run(b)
                self.assertEqual(result.returncode, 1, f"{name}: {result.stdout} {result.stderr}")
                self.assertIn("REJECT:", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
