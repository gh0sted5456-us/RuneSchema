# RuneSchema third-party notices

RuneSchema's own license and ownership statement do not replace upstream
copyrights or licenses. This inventory distinguishes code ancestry from
runtime/build dependencies. Preserve notices supplied with the actual
revisions used, including historical notices not reproduced in this list.

## PalSchema

- Upstream: https://github.com/Okaetsu/PalSchema
- Role: foundation for the schema framework.
- Verified upstream notice: Copyright (c) 2026 Okaetsu.
- License: MIT; complete text in `licenses/PalSchema-MIT.txt`.
- Verified license blob: `8786e078b8aff85ca35b6b573354ab98f1dd82b0`.

This is a copy of the verified upstream license, not a claim that all
RuneSchema-specific historical additions were licensed by Okaetsu.

## UE4SS / RE-UE4SS

- Upstream: https://github.com/UE4SS-RE/RE-UE4SS
- Role: runtime host and C++ build dependency.
- Current CMake default pin: `f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44`.
- Notice at that pin: Copyright (c) 2022 Narknon.
- Root license: MIT; complete text in `licenses/UE4SS-MIT.txt`.
- Verified license blob: `eee4c931619e9ae3ccf4cd4514732653939dffbc`.

The root MIT license is not a blanket license for all submodules. In
particular, UEPseudo is identified by UE4SS's contribution documentation as
subject to Epic Games' licensing terms. Preserve and review those terms
separately. A copied UE4SS license does not authorize redistribution of
otherwise restricted engine content.

Reference: https://docs.ue4ss.com/dev/contributing.html

## JSON for Modern C++ / nlohmann/json

- Upstream: https://github.com/nlohmann/json
- CMake version: `v3.11.3`.
- Notice in that version: Copyright (c) 2013-2022 Niels Lohmann.
- License: MIT; complete text in `licenses/nlohmann-json-MIT.txt`.
- Verified license blob: `1c1f7a690d815db3a79ea3d4c9138a497e2e7702`.

Preserve embedded notices in the vendored header as well. This copied notice
records the fetched version; it does not replace any different notice in a
vendored or future version.

## Community credits

RuneSchema's community member credits, including the original work by
Snorkles, are consolidated in `AUTHORS.md`. That roster is separate from
this dependency inventory. See `LICENSING.md` for the scope of the project's
grant and the treatment of existing contributions.

## Other build and runtime components

The current CMake file also references SafetyHook, Glaze, efsw, Zydis, and
Zycore (including transitive dependencies). Their own notices and terms must
accompany distributions where applicable; they are not covered by this
project's copyright statement. The packaging helper collects license and
notice files available in local build/dependency caches into
`licenses/dependencies/` and records their original relative paths and hashes
in `licenses/dependencies/manifest.json`.

UPX's existing `source/tools/upx/LICENSE`, `COPYING`, and `README` must be
retained with that tool. The UPX compression exception must be distinguished
from the obligations for redistributing the UPX executable itself. Do not
label UPX as MIT or assume that copying its GPL text alone satisfies all
binary-distribution obligations.

Game mappings, cooked containers, art, and other third-party game or engine
material retain their separate rights and terms. This document is an
inventory and notice-preservation policy, not a certification that every
historical archive or transitive component has been audited.
