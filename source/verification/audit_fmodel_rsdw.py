#!/usr/bin/env python3
"""Compare selected fresh FModel exports with the pinned RSDW archive dataset.

The FModel export is authoritative.  RSDW is used as a path and prior-shape
cross-check.  Exporter-only metadata such as Package is ignored when deciding
whether an asset changed semantically.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


SCOPES = {
    "quests": Path("Content/Gameplay/Quests"),
    "vendors": Path("Content/Gameplay/World/Vendors"),
    "journal": Path("Content/UI/JournalData"),
    "ums_npcs": Path("Plugins/GameFeatures/UmbralSands/Content/Gameplay/NPCs"),
}


def load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def normalized(value: Any) -> Any:
    if isinstance(value, dict):
        return {
            key: normalized(child)
            for key, child in sorted(value.items())
            if key != "Package"
        }
    if isinstance(value, list):
        return [normalized(child) for child in value]
    return value


def inventory(root: Path) -> dict[str, Path]:
    if not root.exists():
        return {}
    return {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*.json")
        if path.is_file()
    }


def compare_scope(fmodel_root: Path, rsdw_root: Path, relative: Path) -> dict[str, Any]:
    fresh = inventory(fmodel_root / relative)
    archived = inventory(rsdw_root / relative)
    names = sorted(fresh.keys() | archived.keys())
    added = [name for name in names if name not in archived]
    removed = [name for name in names if name not in fresh]
    semantic_changes: list[str] = []
    exporter_only: list[str] = []
    parse_errors: list[dict[str, str]] = []

    for name in names:
        if name not in fresh or name not in archived:
            continue
        try:
            new_value = load(fresh[name])
            old_value = load(archived[name])
        except Exception as exc:  # audit must continue across malformed exports
            parse_errors.append({"file": name, "error": str(exc)})
            continue
        if new_value == old_value:
            continue
        if normalized(new_value) == normalized(old_value):
            exporter_only.append(name)
        else:
            semantic_changes.append(name)

    return {
        "fmodel_files": len(fresh),
        "rsdw_files": len(archived),
        "added": added,
        "removed": removed,
        "semantic_changes": semantic_changes,
        "exporter_only_changes": exporter_only,
        "parse_errors": parse_errors,
    }


def walk(value: Any):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def collect_native_contracts(fmodel_root: Path) -> dict[str, Any]:
    quest_files = inventory(fmodel_root / SCOPES["quests"])
    quest_property_sets: Counter[tuple[str, ...]] = Counter()
    quest_regions: Counter[str] = Counter()
    quest_assets = 0
    quest_type_fields: Counter[str] = Counter()

    for path in quest_files.values():
        value = load(path)
        for obj in value if isinstance(value, list) else []:
            if obj.get("Type") != "QuestData":
                continue
            quest_assets += 1
            properties = obj.get("Properties", {})
            quest_property_sets[tuple(sorted(properties))] += 1
            region = properties.get("QuestRegion")
            if isinstance(region, str):
                quest_regions[region] += 1
            for key in properties:
                if "type" in key.lower() or "category" in key.lower() or "color" in key.lower() or "colour" in key.lower():
                    quest_type_fields[key] += 1

    vendor_path = fmodel_root / SCOPES["vendors"] / "DT_VendorDataTable.json"
    vendor_rows: dict[str, Any] = {}
    if vendor_path.exists():
        value = load(vendor_path)
        rows = value[0].get("Rows", {}) if isinstance(value, list) and value else {}
        for name, row in rows.items():
            vendor_rows[name] = {
                "properties": sorted(row),
                "vendor_tag": row.get("VendorTag", {}).get("TagName"),
                "is_vendor": row.get("bIsVendor"),
                "repair": row.get("bHaveRepairOption"),
                "masterwork": row.get("bHaveMasterworkOption"),
                "recipe_groups": len(row.get("LabeledRecipes", [])),
                "sellable_items": len(row.get("SellableItems", [])),
                "reputation_thresholds": len(row.get("VendorLevelReputationThresholds", [])),
            }

    journal_files = inventory(fmodel_root / SCOPES["journal"])
    journal_types: Counter[str] = Counter()
    journal_unlock_types: Counter[str] = Counter()
    journal_property_sets: Counter[tuple[str, ...]] = Counter()
    for path in journal_files.values():
        value = load(path)
        for obj in value if isinstance(value, list) else []:
            asset_type = obj.get("Type")
            if not isinstance(asset_type, str) or "Journal" not in asset_type:
                continue
            journal_types[asset_type] += 1
            properties = obj.get("Properties", {})
            journal_property_sets[tuple(sorted(properties))] += 1
            unlock = properties.get("UnlockCondition", {})
            if isinstance(unlock, dict) and isinstance(unlock.get("UnlockType"), str):
                journal_unlock_types[unlock["UnlockType"]] += 1

    merchant_components: dict[str, list[dict[str, Any]]] = defaultdict(list)
    npc_files = inventory(fmodel_root / SCOPES["ums_npcs"])
    for name, path in npc_files.items():
        value = load(path)
        for obj in value if isinstance(value, list) else []:
            if obj.get("Type") != "CraftingStationComponent":
                continue
            props = obj.get("Properties", {})
            handle = props.get("CraftingDataRowHandle", {})
            merchant_components[name].append({
                "row": handle.get("RowName") if isinstance(handle, dict) else None,
                "data_table": (
                    handle.get("DataTable", {}).get("ObjectPath")
                    if isinstance(handle, dict) and isinstance(handle.get("DataTable"), dict)
                    else None
                ),
                "display_name": props.get("DisplayName", {}).get("SourceString")
                if isinstance(props.get("DisplayName"), dict) else None,
            })

    return {
        "quests": {
            "quest_assets": quest_assets,
            "regions": dict(sorted(quest_regions.items())),
            "type_category_color_fields": dict(sorted(quest_type_fields.items())),
            "property_shapes": [
                {"count": count, "properties": list(properties)}
                for properties, count in quest_property_sets.most_common()
            ],
        },
        "vendors": {"rows": vendor_rows},
        "journal": {
            "asset_types": dict(sorted(journal_types.items())),
            "unlock_types": dict(sorted(journal_unlock_types.items())),
            "property_shapes": [
                {"count": count, "properties": list(properties)}
                for properties, count in journal_property_sets.most_common()
            ],
        },
        "merchant_npcs": dict(sorted(merchant_components.items())),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fmodel", type=Path, required=True)
    parser.add_argument("--rsdw", type=Path, required=True)
    parser.add_argument("--rsdw-commit", default="unknown")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    result = {
        "authority": "Fresh FModel export; RSDW is a prior-export cross-check only.",
        "rsdw_commit": args.rsdw_commit,
        "comparison": {
            name: compare_scope(args.fmodel, args.rsdw, relative)
            for name, relative in SCOPES.items()
        },
        "native_contracts": collect_native_contracts(args.fmodel),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    summary = {
        name: {
            "fmodel_files": details["fmodel_files"],
            "rsdw_files": details["rsdw_files"],
            "added": len(details["added"]),
            "removed": len(details["removed"]),
            "semantic_changes": len(details["semantic_changes"]),
            "exporter_only_changes": len(details["exporter_only_changes"]),
            "parse_errors": len(details["parse_errors"]),
        }
        for name, details in result["comparison"].items()
    }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
