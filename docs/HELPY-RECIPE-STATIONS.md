# Helpy recipe stations

The Recipes station picker is populated from native crafting/processing table rows,
not from item outputs or every asset with a recipe array. A compatible row must
expose a typed `StationBuildingPieceData` reference and a supported native recipe
collection. Vendor rows are excluded by their actual `bIsVendor` value, not by a
filename containing the word "vendor". Independent mod tables remain supported
when they expose those native contracts.

Physical station names/icons come from the bound `BuildingPieceData.DisplayName`
and `DisplayIcon`, with native station-row display overrides retained. If the
station asset is not loaded, only that explicit binding is resolved; at most 64
such loads are attempted per catalog request. The normal icon renderer receives
the cooked station texture path. The generic crafting symbol remains a fallback
when content supplies no usable icon; an output armor/weapon icon is not substituted.

Unlinked native recipe menus may still be reported as existing recipe placements,
but are not offered as physical stations in the picker. Loading an existing recipe
prefers an actual station when both menu and station placements are available.
Saving an override without adding a station preserves its original native placements.

Station membership is inspected from native reference arrays, with exact full
object-path comparison. It no longer serializes the complete collection through
the generic item editor's 64-element reader. Limits are 65,536 entries per recipe
array and 262,144 references per inspection. Large crafting categories (including
the 21,982-recipe regression fixture) can therefore resolve the correct station
without lifting unrelated item-editor limits.

These are source/build-tested alpha changes. Native station textures and physical
placement coverage still require an in-game check on Steam and Game Pass.
