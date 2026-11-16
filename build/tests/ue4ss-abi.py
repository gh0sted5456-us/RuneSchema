"""Synthetic PE checks, with no game binaries or third-party dependencies."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("abi", Path(__file__).parents[1] / "check-ue4ss-abi.py")
abi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(abi)


def image():
    data = bytearray(4096)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x8664, 1)
    struct.pack_into("<H", data, 0x94, 240)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<I", data, optional + 108, 16)
    struct.pack_into("<4I", data, optional + 112, 0x1000, 0x100, 0x1200, 40)
    section = optional + 240
    data[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<4I", data, section + 8, 0xE00, 0x1000, 0xE00, 0x200)
    struct.pack_into("<IIHH7I", data, 0x200, 0, 0, 0, 0, 0x10E0, 1, 2, 2, 0x1040, 0x1050, 0x1060)
    struct.pack_into("<2I", data, 0x240, 0x1800, 0)
    struct.pack_into("<2I", data, 0x250, 0x1080, 0x10A0)
    struct.pack_into("<2H", data, 0x260, 0, 1)
    for offset, text in ((0x280, b"?Required@@\0"), (0x2A0, b"?Ghost@@\0"), (0x2E0, b"UE4SS.dll\0"), (0x428, b"UE4SS.dll\0")):
        data[offset:offset + len(text)] = text
    struct.pack_into("<5I", data, 0x400, 0x1240, 0, 0, 0x1228, 0x1240)
    struct.pack_into("<3Q", data, 0x440, 0x1280, 0x8000000000000001, 0)
    data[0x482:0x48E] = b"?Required@@\0"
    return data


class Check(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="RuneSchema-PE-test-")
        self.addCleanup(self.temp.cleanup)
        self.core = Path(self.temp.name) / "main.dll"
        self.host = Path(self.temp.name) / "UE4SS.dll"
        self.core.write_bytes(image())
        self.host.write_bytes(image())

    def test_names_and_ordinals(self):
        result = abi.check(self.core, self.host)
        self.assertTrue(result["CompatibleExports"])
        self.assertEqual(result["RequiredExports"], 2)
        self.assertNotIn("?Ghost@@", abi.PE(self.host).exports())

    def test_missing_exports(self):
        data = image()
        struct.pack_into("<I", data, 0x240, 0)
        self.host.write_bytes(data)
        result = abi.check(self.core, self.host)
        self.assertFalse(result["CompatibleExports"])
        self.assertEqual(result["MissingExports"], ["#1", "?Required@@"])

    def test_packed_core_cannot_false_pass(self):
        data = image()
        data[0x188:0x190] = b"UPX0\0\0\0\0"
        self.core.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "UPX-packed"):
            abi.check(self.core, self.host)

    def test_bad_addresses(self):
        data = image()
        struct.pack_into("<Q", data, 0x440, 0xFFFFFF00)
        self.core.write_bytes(data)
        with self.assertRaises(ValueError):
            abi.check(self.core, self.host)

    def test_invalid_export_ordinal(self):
        data = image()
        struct.pack_into("<H", data, 0x260, 9)
        self.host.write_bytes(data)
        with self.assertRaises(ValueError):
            abi.check(self.core, self.host)

    def test_truncated_or_wrong_architecture(self):
        self.core.write_bytes(b"MZ")
        with self.assertRaises(ValueError):
            abi.check(self.core, self.host)
        data = image()
        struct.pack_into("<H", data, 0x84, 0x14C)
        self.core.write_bytes(data)
        with self.assertRaises(ValueError):
            abi.check(self.core, self.host)


if __name__ == "__main__":
    unittest.main()
