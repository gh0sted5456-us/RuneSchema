from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

cmake = (ROOT / "raw" / "CMakeLists.txt").read_text(encoding="utf-8")
main = (ROOT / "raw" / "src" / "dllmain.cpp").read_text(encoding="utf-8")
host_h = (ROOT / "raw" / "include" / "Runtime" / "PluginHost.h").read_text(encoding="utf-8")
host_cpp = (ROOT / "raw" / "src" / "Runtime" / "PluginHost.cpp").read_text(encoding="utf-8")
helpy = (ROOT / "raw" / "plugins" / "helpy" / "HelpyPlugin.cpp").read_text(encoding="utf-8")
network_manifest = (ROOT / "plugins" / "RuneSchema.Networking" / "plugin.json").read_text(encoding="utf-8")

checks = {
    "main delegates UI init": "m_pluginHost.OnUiInit()" in main,
    "main delegates Unreal init": "m_pluginHost.OnUnrealInit()" in main,
    "main does not construct quick menu": "InGameQuickMenu" not in main,
    "plugin host supports UI lifecycle": "OnUiInit" in host_h and "RuneSchemaPlugin_OnUiInit" in host_cpp,
    "plugin host supports Unreal lifecycle": "OnUnrealInit" in host_h and "RuneSchemaPlugin_OnUnrealInit" in host_cpp,
    "Helpy owns quick menu": "PS::InGameQuickMenu" in helpy,
    "Helpy exports UI lifecycle": "RuneSchemaPlugin_OnUiInit" in helpy,
    "Helpy exports Unreal lifecycle": "RuneSchemaPlugin_OnUnrealInit" in helpy,
    "main excludes quick menu source": "list(FILTER SRC_FILES EXCLUDE REGEX" in cmake and "InGameQuickMenu" in cmake,
    "Helpy target compiles quick menu": "add_library(RuneSchemaHelpyPlugin" in cmake and "src/Generator/InGameQuickMenu.cpp" in cmake,
    "networking is not a DLL target": "RuneSchemaNetworkingPlugin" not in cmake,
    "networking manifest is content-only": '"EntryPoint"' not in network_manifest,
    "networking uses fixed paks layout": '"PackagedRoot"' not in network_manifest,
    "main retains registry bridge dependency": 'bridge.registry' in main,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failed:
    raise SystemExit("Plugin ownership checks failed: " + ", ".join(failed))
