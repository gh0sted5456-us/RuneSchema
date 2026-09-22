#!/usr/bin/env python3
"""Check that the full-loader audit covers the shipped loader catalogue."""

import ast
import json
import re
from pathlib import Path


root = Path(__file__).resolve().parents[1]
ast.parse((root / "verification/audit_all_loaders.py").read_text(encoding="utf-8"))
data = json.loads((root / "verification/full-loader-audit.json").read_text(encoding="utf-8"))
capabilities = (root / "raw/include/Generator/LoaderCapabilities.h").read_text(encoding="utf-8")
names = re.findall(r'\{"([a-z]+)","', capabilities)
assert len(names) == 21, names
assert set(names) == set(data["loaders"]), sorted(set(names) ^ set(data["loaders"]))
inventory = data["inventory"]
assert inventory["fresh_json_files"] >= 70000
assert inventory["rsdw_json_files"] >= 70000
assert len(inventory["fresh_only"]) == 7
assert inventory["rsdw_json_files"] - inventory["fresh_json_files"] == len(inventory["rsdw_only"]) - len(inventory["fresh_only"])
assert not inventory["parse_errors"]
report = (root / "FULL-LOADER-FMODEL-RSDW-AUDIT.md").read_text(encoding="utf-8")
missing = [name for name in names if f"`/{name}`" not in report]
assert not missing, missing
print("PASS: 21 loaders, scanner syntax, inventory, path drift, and report coverage verified")
