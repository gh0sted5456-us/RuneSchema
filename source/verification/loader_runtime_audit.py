from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "assets", "blueprints", "buildings", "courses", "dialogue", "effects", "enums",
    "equipment", "events", "journal", "lore", "nameplates", "niagara", "npc", "players",
    "quests", "raw", "recipes", "registry", "spawns", "strings", "vendors",
}

main = (ROOT / "raw/src/Loader/DragonWildsMainLoader.cpp").read_text(encoding="utf-8")
catalog = (ROOT / "raw/include/Runtime/PluginCatalog.h").read_text(encoding="utf-8")
host = (ROOT / "raw/src/Runtime/PluginHost.cpp").read_text(encoding="utf-8")
docs = {p.stem for p in (ROOT / "docs/loaders").glob("*.md") if p.name != "README.md"}
settings_text = (ROOT / "settings/settings.jsonc").read_text(encoding="utf-8-sig")
configured = set(re.findall(r'^\s*"([a-z]+)"\s*:\s*(?:true|false)', settings_text, re.MULTILINE))

checks = {
    "exact documented loader inventory": docs == EXPECTED,
    "settings cover every loader": EXPECTED <= configured,
    "no animations loader": "animations" not in configured and "animations" not in docs,
    "no behaviors loader": "behaviors" not in configured and "behaviors" not in docs,
    "definition-first effects": 'kind!="effects" && kind!="niagara"' in main,
    "players use late aggregation": "RegisterAppearanceSource" in main and "FinalizePlayerRules" in main,
    "nameplates use late aggregation": "LoadNameplateDefinitions" in main and "FinalizeNameplateDefinitions" in main,
    "loader exceptions isolated": "Loader {} initialization failed" in main and "Failed to load {}/{}" in main,
    "plugin order file parsed": 'root/"plugins.txt"' in catalog and "ReadOrder(root)" in catalog,
    "fixed plugin directories": 'plugin.Root/"dll"' in catalog and 'plugin.Root/"paks"' in catalog and 'plugin.Root/"scripts"' in catalog,
    "obsolete packaged root rejected": 'PackagedRoot is obsolete' in catalog,
    "docs ignored by runtime": '/"docs"' not in catalog and '/"docs"' not in main,
    "runtime packaged layer removed": 'runtime/"packaged"' not in catalog and 'runtime/"packaged"' not in main,
    "triplets validated per package": "PakDirectories" in catalog and "value.second!=7" in catalog,
    "dependency order overrides preference": "indegree" in catalog and "dependency.first==result[i].Id" in catalog,
    "required plugin rejection is isolated": "required plugin was disabled" in catalog and "plugin.Enabled=false" in catalog,
    "compatibility is advisory": "BuiltForRuneSchema" in catalog and "attempting best-effort load" in host and "VersionParts" not in catalog,
    "plugin failures preserve core": "plugin catalog rejected; RuneSchema core continues" in host,
    "compatibility notices configurable": 'compatibilityNotices = "quiet"' in (ROOT / "raw/include/Utility/Config.h").read_text(encoding="utf-8") and '"compatibilityNotices": "quiet"' in settings_text,
    "plugin roots precede mod roots": main.index("PluginCatalog::Discover") < main.index("ModLoadOrder::Resolve(modsRoot"),
    "content triplet validated": "PakDirectories(manifest)" in host and "value.second!=7" in catalog,
    "extension connections are opt-in": "manifest.Connections" in host and "HasConnection" in host,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failed:
    raise SystemExit("Loader runtime audit failed: " + ", ".join(failed))
print(f"PASS: {len(EXPECTED)} loaders and {len(checks)} runtime/order invariants audited")
