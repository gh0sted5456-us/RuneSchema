#!/usr/bin/env python3
"""Inventory fresh FModel evidence for every RuneSchema loader.

The fresh export is authoritative.  The RSDW input is a Git tree path list and
is used only to detect archive coverage and path drift; it is never treated as
newer than the local export.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import Counter
from pathlib import Path
from typing import Any


LOADERS = {
    "assets": {
        "paths": [r"(^|/)Gameplay/Items/", r"(^|/)Items/"],
        "types": [r"ItemData$", r"WearableData$", r"WeaponData$", r"CapeData$"],
    },
    "blueprints": {"paths": [r"/Blueprints?/"], "types": [r"BlueprintGeneratedClass$", r"WidgetBlueprintGeneratedClass$"]},
    "buildings": {"paths": [r"Building", r"BaseBuilding"], "types": [r"BuildingPieceData$", r"Building.*Data$"]},
    "courses": {"paths": [r"Course", r"Skill.*Training"], "types": [r"CourseData$", r".*Course.*Data$"]},
    "dialogue": {"paths": [r"Dialogue", r"Conversation"], "types": [r".*Dialogue.*", r".*Conversation.*"]},
    "effects": {"paths": [r"GameplayEffect", r"/Effects/", r"/StatusEffects/"], "types": [r"GameplayEffect.*", r".*EffectData$"]},
    "enums": {"paths": [r"/Enums?/", r"(^|/)E[A-Z][^/]*\.json$"], "types": [r"UserDefinedEnum$", r"Enum$"]},
    "equipment": {"paths": [r"Equipment", r"Wearable", r"Armor", r"Armour"], "types": [r"WearableData$", r"Equipment.*", r"Armor.*Data$"]},
    "events": {"paths": [r"WorldEvents?", r"/Events?/"], "types": [r".*WorldEvent.*", r"EventData$"]},
    "journal": {"paths": [r"JournalData"], "types": [r"Journal.*Data$"]},
    "lore": {"paths": [r"JournalData.*Lore", r"/Lore/"], "types": [r"JournalEntryKnowLoreData$"]},
    "nameplates": {"paths": [r"Nameplate"], "types": [r".*Nameplate.*"]},
    "niagara": {"paths": [r"Niagara", r"/VFX/", r"/FX/"], "types": [r"NiagaraSystem$", r"NiagaraEmitter$"]},
    "npc": {"paths": [r"/NPCs?/", r"/AI/", r"Characters?/.*/AI"], "types": [r"AIData$", r"NPC.*Data$", r"CharacterData$"]},
    "players": {"paths": [r"Gameplay/Character/Player", r"/Player/Customization"], "types": [r"Player.*", r".*Player.*Component$"]},
    "quests": {"paths": [r"/Quests?/"], "types": [r"QuestData$", r".*Quest.*Data$"]},
    "raw": {"paths": [r"(^|/)DT_[^/]*\.json$", r"DataTable"], "types": [r"DataTable$"]},
    "recipes": {"paths": [r"(^|/)RECIPE_", r"/Recipes?/"], "types": [r"RecipeData$"]},
    "spawns": {"paths": [r"Spawn", r"Spawning"], "types": [r".*Spawn.*", r"AISpawnPoint.*"]},
    "strings": {"paths": [r"StringTable", r"(^|/)ST_[^/]*\.json$"], "types": [r"StringTable$"]},
    "vendors": {"paths": [r"/Vendors?/", r"VendorData", r"Merchant"], "types": [r".*Vendor.*", r"CraftingStationData.*"]},
}


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def object_types(value: Any) -> list[str]:
    values = value if isinstance(value, list) else [value]
    return [entry["Type"] for entry in values if isinstance(entry, dict) and isinstance(entry.get("Type"), str)]


def matches(value: str, patterns: list[str]) -> bool:
    return any(re.search(pattern, value, re.IGNORECASE) for pattern in patterns)


def normalize_rsdw_paths(path_file: Path) -> set[str]:
    marker = "1.0.0.2/json/RSDragonwilds/"
    result = set()
    for raw in path_file.read_text(encoding="utf-8-sig").splitlines():
        value = raw.strip().replace("\\", "/")
        if marker in value:
            value = value.split(marker, 1)[1]
        if value.endswith(".json"):
            result.add(value)
    return result


def rsdw_git_paths(repository: Path) -> set[str]:
    result = subprocess.run(
        ["git", "-C", str(repository), "ls-tree", "-r", "--name-only", "HEAD"],
        check=True, text=True, capture_output=True,
    )
    marker = "1.0.0.2/json/RSDragonwilds/"
    return {
        value.split(marker, 1)[1]
        for raw in result.stdout.splitlines()
        if marker in (value := raw.strip().replace("\\", "/")) and value.endswith(".json")
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fmodel", type=Path, required=True)
    sources = parser.add_mutually_exclusive_group(required=True)
    sources.add_argument("--rsdw-paths", type=Path)
    sources.add_argument("--rsdw-git", type=Path)
    parser.add_argument("--rsdw-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    files = sorted(path for path in args.fmodel.rglob("*.json") if path.is_file())
    fresh_paths = {path.relative_to(args.fmodel).as_posix() for path in files}
    archived_paths = rsdw_git_paths(args.rsdw_git) if args.rsdw_git else normalize_rsdw_paths(args.rsdw_paths)
    parsed: dict[str, list[str]] = {}
    parse_errors: list[dict[str, str]] = []
    all_types: Counter[str] = Counter()
    for path in files:
        relative = path.relative_to(args.fmodel).as_posix()
        try:
            types = object_types(read_json(path))
            parsed[relative] = types
            all_types.update(types)
        except Exception as error:
            parse_errors.append({"path": relative, "error": str(error)})

    coverage = {}
    for loader, spec in LOADERS.items():
        candidates = []
        candidate_types: Counter[str] = Counter()
        for relative in sorted(fresh_paths):
            types = parsed.get(relative, [])
            if matches(relative, spec["paths"]) or any(matches(kind, spec["types"]) for kind in types):
                candidates.append(relative)
                candidate_types.update(types)
        archived_scope = {path for path in archived_paths if matches(path, spec["paths"])}
        fresh_path_scope = {path for path in fresh_paths if matches(path, spec["paths"])}
        coverage[loader] = {
            "fresh_candidates": len(candidates),
            "sample_paths": candidates[:12],
            "top_types": [{"type": name, "count": count} for name, count in candidate_types.most_common(12)],
            "path_scope": {
                "fresh": len(fresh_path_scope),
                "rsdw": len(archived_scope),
                "fresh_only": sorted(fresh_path_scope - archived_scope)[:50],
                "rsdw_only": sorted(archived_scope - fresh_path_scope)[:50],
            },
        }

    output = {
        "authority": "Fresh local FModel export; RSDW is a prior-export path cross-check only.",
        "rsdw_commit": args.rsdw_commit,
        "inventory": {
            "fresh_json_files": len(fresh_paths),
            "rsdw_json_files": len(archived_paths),
            "fresh_only": sorted(fresh_paths - archived_paths),
            "rsdw_only": sorted(archived_paths - fresh_paths),
            "parse_errors": parse_errors,
            "top_export_types": [{"type": name, "count": count} for name, count in all_types.most_common(100)],
        },
        "loaders": coverage,
        "method_limit": "Candidate scopes are discovery aids. Runtime class/property checks remain authoritative for patch/create support.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "fresh": len(fresh_paths),
        "rsdw": len(archived_paths),
        "fresh_only": len(fresh_paths - archived_paths),
        "rsdw_only": len(archived_paths - fresh_paths),
        "parse_errors": len(parse_errors),
        "loader_candidates": {name: row["fresh_candidates"] for name, row in coverage.items()},
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
