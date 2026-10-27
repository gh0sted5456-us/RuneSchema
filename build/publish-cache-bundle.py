"""Stage an EXPORTED vanilla cache bundle and index; never downloads or pushes."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAX_FILE = 256 * 1024 * 1024  # Current GitHub download client bound.
MAX_MANIFEST = 80 * 1024 * 1024
HEX = re.compile(r"[0-9a-f]{64}")
PATH = re.compile(r"/[A-Za-z0-9_/+\-]+\.[A-Za-z0-9_+\-]+")
KINDS = {"items", "journal", "art", "portraits", "resources"}
DATA = {"Path", "Name", "PersistenceID", "PersistenceIDKnown", "ObservedCurrent",
        "BundleReference", "CacheOwner", "SourceKind", "SourceName", "SourceEvidence",
        "Native", "Icon", "Description", "Type", "Category", "Section", "InternalName",
        "PowerLevel", "MaxStackSize", "StackSize", "Pages", "Tags"}

def inspect_map(value, blob):
    fields = {"Version", "Asset", "Projection", "Bytes", "SHA256"}
    if not isinstance(value, dict) or set(value) != fields or len(json.dumps(value).encode()) > 8192:
        raise ValueError("Unexpected/private world-map metadata")
    if type(value["Version"]) is not int or value["Version"] != 1 or value["Asset"] != "/Game/Art/UI/Map/T_WorldMap_Landmass.T_WorldMap_Landmass":
        raise ValueError("Unsupported world-map asset/version")
    count, digest = value["Bytes"], value["SHA256"]
    if type(count) is not int or not 20 <= count <= 16+2048*2048*4 or not isinstance(digest, str) or not HEX.fullmatch(digest):
        raise ValueError("Invalid world-map bounds/checksum")
    projection = value["Projection"]
    if not isinstance(projection, dict) or set(projection) != {"Center", "Right", "Up", "HalfWidth", "HalfHeight", "MinZ", "MaxZ"}:
        raise ValueError("Unexpected/private world-map alignment")
    numbers = []
    for key in ("Center", "Right", "Up"):
        vector = projection[key]
        if not isinstance(vector, list) or len(vector) != 2:
            raise ValueError("Invalid world-map alignment vector")
        numbers.extend(vector)
    numbers.extend(projection[key] for key in ("HalfWidth", "HalfHeight", "MinZ", "MaxZ"))
    if not all(type(number) in (int, float) and math.isfinite(number) for number in numbers):
        raise ValueError("Invalid world-map alignment number")
    right, up = projection["Right"], projection["Up"]
    if not (all(1 < projection[key] < 10000000 for key in ("HalfWidth", "HalfHeight"))
            and projection["MinZ"] < projection["MaxZ"]
            and abs(sum(x*x for x in right)-1) < .001 and abs(sum(x*x for x in up)-1) < .001
            and abs(sum(x*y for x, y in zip(right, up))) < .001):
        raise ValueError("Invalid world-map alignment")
    if len(blob) != count or blob[:4] != b"RSI3" or hashlib.sha256(blob).hexdigest() != digest:
        raise ValueError("Corrupt world-map picture")
    width, height, size = struct.unpack_from("<III", blob, 4)
    if not (0 < width <= 2048 and 0 < height <= 2048 and size == width*height*4 and count == 16+size):
        raise ValueError("Invalid world-map dimensions")

def inspect(data):
    if not 12 <= len(data) <= MAX_FILE or data[:8] != b"RSCACHE1":
        raise ValueError("Not a supported bundle, or exceeds the 256 MiB download bound")
    length, = struct.unpack_from("<I", data, 8)
    if not 0 < length <= MAX_MANIFEST or 12 + length > len(data):
        raise ValueError("Invalid manifest bounds")
    doc = json.loads(data[12:12+length])
    if doc.get("Version") != 2 or doc.get("Kind") != "RuneSchemaCacheBundle":
        raise ValueError("Wrong manifest version/kind")
    if doc.get("ContentOwner"):
        raise ValueError("Mod-scoped bundles cannot be published as the native base cache")
    if not isinstance(doc.get("Locale"), str) or not re.fullmatch(r"[A-Za-z0-9-]{1,64}", doc["Locale"]):
        raise ValueError("Missing locale")
    sources, records, icons = doc["Sources"], doc["Records"], doc["Icons"]
    if not isinstance(sources, dict) or not 0 < len(sources) <= 512:
        raise ValueError("Invalid source list")
    if not all(re.fullmatch(r"[a-z0-9_/-]+", key) and isinstance(value, str) and HEX.fullmatch(value) for key, value in sources.items()):
        raise ValueError("Invalid portable source fingerprint")
    if not isinstance(records, list) or not isinstance(icons, list) or len(records) > 65536 or len(icons) > 32768 or not (records or icons or "WorldMap" in doc):
        raise ValueError("Invalid record/picture count")
    if len(json.dumps(records, separators=(",", ":")).encode()) > 64 * 1024 * 1024:
        raise ValueError("Metadata exceeds 64 MiB")
    native = {"game", "engine"}
    seen = set()
    for row in records:
        if not isinstance(row, dict) or set(row) - {"Kind", "Owner", "Source", "Path", "PersistenceID", "Data"}:
            raise ValueError("Unexpected/private record fields")
        owner, source, path = row["Owner"], row["Source"], row["Path"]
        if row["Kind"] not in KINDS or not isinstance(path, str) or not PATH.fullmatch(path) or ".." in path or path.lower().startswith(("/engine/transient", "/temp/")):
            raise ValueError("Invalid category/path")
        if source not in sources or path.split("/")[1].lower() != source:
            raise ValueError("Source/path mismatch")
        if owner == "core":
            if source not in {"game", "engine"}:
                raise ValueError("Core record uses a non-native namespace")
        elif owner == "plugins/" + source:
            native.add(source)
        else:
            raise ValueError("Vanilla publishing excludes mod/unknown owners")
        key = (owner, row["Kind"], path)
        if key in seen:
            raise ValueError("Duplicate record")
        seen.add(key)
        pid = row.get("PersistenceID", "")
        if not isinstance(pid, str) or len(pid.encode()) > 256 or "\0" in pid:
            raise ValueError("Invalid persistence ID")
        presentation = row["Data"]
        if not isinstance(presentation, dict) or set(presentation) - DATA or not isinstance(presentation.get("Name"), str):
            raise ValueError("Unexpected/private/executable metadata")
        if presentation.get("ObservedCurrent", False) or presentation.get("SourceKind") == "modded":
            raise ValueError("Cannot publish runtime state or modded metadata as vanilla")
    if set(sources) - native:
        raise ValueError("Vanilla bundle contains an undeclared non-native source")
    at = 12 + length
    paths = set()
    for icon in icons:
        path, count, digest = icon["Icon"], icon["Bytes"], icon["SHA256"]
        if not isinstance(path, str) or not PATH.fullmatch(path) or path in paths or icon["Source"] not in sources:
            raise ValueError("Invalid/duplicate icon path")
        paths.add(path)
        if not isinstance(count, int) or not 20 <= count <= 16+256*256*4 or not isinstance(digest, str) or not HEX.fullmatch(digest):
            raise ValueError("Invalid icon bounds")
        for owner in icon.get("Origins", []):
            if owner != "core" and not (owner.startswith("plugins/") and owner[8:] in native):
                raise ValueError("Mod artwork cannot be published as a vanilla cache")
        blob = data[at:at+count]
        if len(blob) != count or hashlib.sha256(blob).hexdigest() != digest or blob[:4] != b"RSI3":
            raise ValueError("Corrupt picture")
        width, height, size = struct.unpack_from("<III", blob, 4)
        if not (0 < width <= 256 and 0 < height <= 256 and size == width*height*4 and count == 16+size):
            raise ValueError("Invalid picture dimensions")
        at += count
    if "WorldMap" in doc:
        inspect_map(doc["WorldMap"], data[at:])
        at = len(data)
    if at != len(data):
        raise ValueError("Unexpected trailing payload")
    return doc

def commit(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".bundle-stage-", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    try:
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--game-version", required=True)
    parser.add_argument("--runtime", required=True, choices=("Steam", "Game Pass", "Epic Games", "Dedicated server"))
    parser.add_argument("--output", type=Path, default=ROOT/"resources/cache-bundles")
    args = parser.parse_args()
    with args.bundle.open("rb") as stream:
        data = stream.read(MAX_FILE+1)
    doc = inspect(data)
    digest = hashlib.sha256(data).hexdigest()
    file = args.output/"index.json"
    index = json.loads(file.read_text(encoding="utf-8")) if file.exists() else {"Format": 2, "Kind": "RuneSchemaCacheBundles", "Packs": []}
    if index.get("Format") != 2 or index.get("Kind") != "RuneSchemaCacheBundles" or not isinstance(index.get("Packs"), list):
        raise ValueError("Existing index is invalid; unchanged")
    entry = {"Runtime": args.runtime, "GameVersion": args.game_version,
             "Locale": doc["Locale"], "Sources": doc["Sources"], "SHA256": digest,
             "Bytes": len(data), "RecordCount": len(doc["Records"]),
             "IconCount": len(doc["Icons"]), "RecordCoverage": "Partial"}
    if "WorldMap" in doc:
        entry["WorldMapSHA256"] = doc["WorldMap"]["SHA256"]
    index["Packs"] = [entry] + [row for row in index["Packs"] if not (row.get("SHA256") == digest and row.get("Runtime") == args.runtime)]
    if len(index["Packs"]) > 128:
        raise ValueError("Index exceeds client limit")
    target = args.output/(digest+".rsmk-cache")
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise ValueError("Existing content-addressed file is corrupt; unchanged")
    if not target.exists():
        commit(target, data)
    commit(file, (json.dumps(index, indent=2)+"\n").encode())
    map_note = " / world map" if "WorldMap" in doc else ""
    print(f"Staged {len(doc['Records'])} records / {len(doc['Icons'])} pictures{map_note}; partial coverage. No GitHub upload or push performed.")

if __name__ == "__main__":
    main()

