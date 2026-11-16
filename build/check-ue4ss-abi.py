"""Read-only x64 PE import/export check; never loads or executes either DLL."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


class PE:
    def __init__(self, path):
        self.path = Path(path)
        with self.path.open("rb") as stream:
            self.data = stream.read(256 * 1024 * 1024 + 1)
        if len(self.data) > 256 * 1024 * 1024 or self.data[:2] != b"MZ":
            raise ValueError("Expected a bounded Windows DLL")
        pe = self.number(0x3C, "I")
        if self.bytes(pe, 4) != b"PE\0\0" or self.number(pe + 4, "H") != 0x8664:
            raise ValueError("Expected an x64 Windows PE image")
        count, optional = self.number(pe + 6, "H"), pe + 24
        size = self.number(pe + 20, "H")
        if not 1 <= count <= 96 or size < 128 or self.number(optional, "H") != 0x20B:
            raise ValueError("Unsupported PE header layout")
        self.bytes(optional, size)
        self.directories = optional + 112
        self.directory_count = min(self.number(optional + 108, "I"), (size - 112) // 8)
        self.headers = self.number(optional + 60, "I")
        self.sections = []
        for index in range(count):
            at = optional + size + index * 40
            name = self.bytes(at, 8).split(b"\0", 1)[0]
            address, length, offset = (self.number(at + field, "I") for field in (12, 16, 20))
            self.bytes(offset, length)
            self.sections.append((name, address, length, offset))

    def bytes(self, offset, length):
        if offset < 0 or length < 0 or offset > len(self.data) - length:
            raise ValueError("PE field exceeds the input file")
        return self.data[offset:offset + length]

    def number(self, offset, kind):
        return struct.unpack("<" + kind, self.bytes(offset, struct.calcsize("<" + kind)))[0]

    def offset(self, address, length=1):
        if address < self.headers:
            self.bytes(address, length)
            return address
        for _, base, size, offset in self.sections:
            if base <= address and address - base <= size - length:
                self.bytes(offset + address - base, length)
                return offset + address - base
        raise ValueError("PE address has no file-backed section")

    def string(self, address):
        at = self.offset(address)
        end = self.data.find(b"\0", at, min(len(self.data), at + 8192))
        if end < 0:
            raise ValueError("PE symbol is not bounded/terminated")
        self.offset(address, end - at + 1)
        return self.data[at:end].decode("ascii")

    def directory(self, index):
        if index >= self.directory_count:
            return 0, 0
        return self.number(self.directories + index * 8, "I"), self.number(self.directories + index * 8 + 4, "I")

    def imports(self, library):
        if any(name.startswith(b"UPX") for name, *_ in self.sections):
            raise ValueError("Core DLL is UPX-packed; use the Wine compatibility copy for a complete API check")
        address, size = self.directory(1)
        if not address:
            return set()
        for index in range(min(size // 20 + 1, 4096)):
            at = self.offset(address + index * 20, 20)
            original, _, _, name, first = struct.unpack("<5I", self.bytes(at, 20))
            if not any((original, name, first)):
                return set()
            if self.string(name).lower() != library.lower():
                continue
            result = set()
            for item in range(131072):
                value = self.number(self.offset((original or first) + item * 8, 8), "Q")
                if not value:
                    return result
                result.add("#" + str(value & 0xFFFF) if value >> 63 else self.string(value + 2))
            raise ValueError("PE import list has no bounded terminator")
        raise ValueError("PE import descriptors have no bounded terminator")

    def exports(self):
        address, _ = self.directory(0)
        if not address:
            return set()
        at = self.offset(address, 40)
        base, functions, names, table, strings, ordinals = struct.unpack("<6I", self.bytes(at + 16, 24))
        if max(functions, names) > 262144:
            raise ValueError("PE export table exceeds its limit")
        result = {"#" + str(base + index) for index in range(functions)
                  if self.number(self.offset(table + index * 4, 4), "I")}
        for index in range(names):
            slot = self.number(self.offset(ordinals + index * 2, 2), "H")
            if slot >= functions:
                raise ValueError("PE named export has an invalid ordinal")
            if self.number(self.offset(table + slot * 4, 4), "I"):
                result.add(self.string(self.number(self.offset(strings + index * 4, 4), "I")))
        return result


def check(core, host):
    core, host = PE(core), PE(host)
    required = core.imports("UE4SS.dll")
    if not required:
        raise ValueError("Core image has no UE4SS imports; check the selected file")
    missing = sorted(required - host.exports())
    return {"CompatibleExports": not missing, "RequiredExports": len(required), "MissingExports": missing,
            "CoreSHA256": hashlib.sha256(core.data).hexdigest(), "UE4SSSHA256": hashlib.sha256(host.data).hexdigest(),
            "Scope": "Export names/ordinals only; not proof of C++ layout, runtime dependencies or Proton gameplay"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", type=Path, help="Unpacked RuneSchema/dlls/main.dll")
    parser.add_argument("ue4ss", type=Path, help="UE4SS.dll from the same game installation")
    args = parser.parse_args()
    try:
        result = check(args.core, args.ue4ss)
    except (OSError, ValueError, struct.error) as error:
        print(json.dumps({"Error": str(error), "CompatibleExports": False}))
        return 1
    print(json.dumps(result, indent=2))
    return 0 if result["CompatibleExports"] else 2


if __name__ == "__main__":
    sys.exit(main())
