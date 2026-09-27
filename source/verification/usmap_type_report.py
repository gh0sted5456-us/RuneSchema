#!/usr/bin/env python3
"""Print reflected property layouts from an uncompressed UE4SS USMAP."""

import argparse
import json
import struct
from pathlib import Path


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def read(self, fmt: str):
        size = struct.calcsize(fmt)
        value = struct.unpack_from(fmt, self.data, self.offset)[0]
        self.offset += size
        return value

    def text(self, size: int) -> str:
        value = self.data[self.offset:self.offset + size].decode("utf-8")
        self.offset += size
        return value


LABELS = (
    "Byte", "Bool", "Int", "Float", "Object", "Name", "Delegate", "Double",
    "Array", "Struct", "String", "Text", "Interface", "MulticastDelegate",
    "WeakObject", "LazyObject", "AssetObject", "SoftObject", "UInt64", "UInt32",
    "UInt16", "Int64", "Int16", "Int8", "Map", "Set", "Enum", "FieldPath",
    "Optional", "Utf8String", "AnsiString",
)


def property_type(reader: Reader, names: list[str]) -> str:
    kind = reader.read("<B")
    if kind == 0xFF:
        return "Unknown"
    if kind == 26:
        inner = property_type(reader, names)
        return f"Enum<{names[reader.read('<i')]}:{inner}>"
    if kind == 9:
        return f"Struct<{names[reader.read('<i')]}>"
    if kind in (8, 25, 28):
        return f"{LABELS[kind]}<{property_type(reader, names)}>"
    if kind == 24:
        return f"Map<{property_type(reader, names)},{property_type(reader, names)}>"
    return LABELS[kind]


def parse(path: Path) -> tuple[list[dict], list[dict]]:
    header = Reader(path.read_bytes())
    if header.read("<H") != 0x30C4:
        raise ValueError("not a USMAP file")
    version = header.read("<B")
    if version >= 1 and header.read("<i") != 0:
        raise ValueError("versioned USMAP headers are unsupported")
    if header.read("<B") != 0:
        raise ValueError("use an uncompressed UE4SS USMAP")
    compressed = header.read("<I")
    decompressed = header.read("<I")
    if compressed != decompressed:
        raise ValueError("invalid uncompressed USMAP sizes")
    reader = Reader(header.data[header.offset:])
    names = []
    for _ in range(reader.read("<I")):
        length = reader.read("<H" if version >= 2 else "<B")
        names.append(reader.text(length))
    enum_count = reader.read("<I")
    enums = []
    for _ in range(enum_count):
        enum = {"Name": names[reader.read("<i")], "Members": []}
        count = reader.read("<H" if version >= 3 else "<B")
        for _ in range(count):
            if version >= 4:
                value = reader.read("<Q")
            else:
                value = len(enum["Members"])
            enum["Members"].append({"Name": names[reader.read("<i")], "Value": value})
        enums.append(enum)
    types = []
    for _ in range(reader.read("<I")):
        item = {
            "Name": names[reader.read("<i")],
            "SuperIndex": reader.read("<i"),
            "PropertySlots": reader.read("<H"),
            "Properties": [],
            "Path": "",
        }
        count = reader.read("<H")
        for _ in range(count):
            index = reader.read("<H")
            array_dim = reader.read("<B")
            name = names[reader.read("<i")]
            item["Properties"].append({
                "Index": index, "ArrayDim": array_dim,
                "Name": name, "Type": property_type(reader, names),
            })
        types.append(item)
    if len(reader.data) - reader.offset >= 9 and reader.read("<I") == 0x54584543:
        reader.read("<B")
        for _ in range(reader.read("<I")):
            extension_id = reader.read("<I")
            size = reader.read("<I")
            end = reader.offset + size
            if extension_id == 0x48545050 and size:
                reader.read("<B")
                for _ in range(reader.read("<I")):
                    reader.read("<i")
                for index in range(reader.read("<I")):
                    types[index]["Path"] = names[reader.read("<i")]
            reader.offset = end
    for item in types:
        super_index = item.pop("SuperIndex")
        item["Super"] = "" if super_index < 0 else names[super_index]
    return types, enums


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mapping", type=Path)
    parser.add_argument("query", nargs="+")
    args = parser.parse_args()
    types, enums = parse(args.mapping)
    for query in args.query:
        key = query.casefold()
        matches = [item for item in types if key in item["Name"].casefold()
                   or key in item["Path"].casefold()]
        enum_matches = [item for item in enums if key in item["Name"].casefold()]
        print(json.dumps({"Query": query, "Types": matches, "Enums": enum_matches}, indent=2))


if __name__ == "__main__":
    main()
