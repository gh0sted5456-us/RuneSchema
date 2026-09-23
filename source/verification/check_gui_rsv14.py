#!/usr/bin/env python3
"""Focused source checks for the RSv14 Helpy/settings refinement."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
quick = (root / "raw/include/Generator/QuickMenuUI.h").read_text(encoding="utf-8")
schemas = (root / "raw/include/Generator/LoaderSchemas.h").read_text(encoding="utf-8")
main = (root / "raw/src/dllmain.cpp").read_text(encoding="utf-8")
inspection_h = (root / "raw/include/Generator/InspectionTools.h").read_text(encoding="utf-8")
inspection_cpp = (root / "raw/src/Generator/InspectionTools.cpp").read_text(encoding="utf-8")


def require(name: str, condition: bool) -> None:
    if not condition:
        raise AssertionError(name)
    print("PASS", name)


require("Helpy Item Lab tab", '"ITEM LAB"' in quick)
require("RuneSchema numeric PowerLevel badges", "/Game/RuneSchema/UI/Icons/Badges/Powerlevel/" in (root / "raw/include/Generator/QuickMenuDecorations.h").read_text(encoding="utf-8") and "T_Power_" in (root / "raw/include/Generator/QuickMenuDecorations.h").read_text(encoding="utf-8"))
require("right-click item details route", "RightClick" in quick and '"ITEM DETAILS"' in quick)
require("Item Lab quick test fields", all(value in quick for value in ('"Defense"','"Granted effects"','"Pack / drop arrays"')))
require("semantic placard corners", "Placard chrome has fixed semantic corners" in quick)
require("source badge upper-left", "{r.x+5,r.y+5,24,24}" in quick)
require("favorite hit remains independent", 'f.hits.push_back({hit,"favorite",e.path})' in quick)
require("separate journal route", 'action=="clone-journal"' in quick and '"JOURNAL ENTRY FOR THIS ITEM"' in quick)
require("separate recipe route", 'action=="clone-recipe"' in quick and '"RECIPE FOR THIS ITEM"' in quick)
require("recipe unlock remains opt-in", "auto-unlock is opt-in" in quick)
require("server settings tab", 'BeginRuneSchemaTab("Server & Loaders"' in main)
require("authoring settings tab", 'BeginRuneSchemaTab("Authoring & Tools"' in main)
require("all loader authoring page", '"All Loaders","Players","Nameplates"' in main)
require("direct item recipe journal pages", '"Items","Recipes","Journal","Quests"' in main)
require("focused editor declaration", "RenderPlayerNameplateBuilder" in inspection_h)
require("focused editor implementation", "void RenderPlayerNameplateBuilder" in inspection_cpp)
require("focused player editor wired", "RenderPlayerNameplateBuilder(false)" in main)
require("focused nameplate editor wired", "RenderPlayerNameplateBuilder(true)" in main)
for field in ("PlayerGuid", "PlayerGuids", "AttributeMultipliers", "Attributes", "Appearance"):
    require(f"player schema exposes {field}", f'player["properties"]["{field}"]' in schemas)
for field in ("Definition", "Distance", "ShowSelf", "Client", "Server"):
    require(f"nameplate schema exposes {field}", f'{{"{field}"' in schemas)

print("PASS RSv14 focused GUI/source checks")
