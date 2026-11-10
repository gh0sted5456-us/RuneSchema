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

Abyss (RSDW Modding Community) is credited for the initial corrupt-character
bypass work on which RuneSchema's corrupt-save bypass was based. This credit
records the implementation's origin; it does not replace applicable licenses.

RuneSchema's community member credits, including the original work by
Snorkles, are consolidated in `AUTHORS.md`. That roster is separate from
this dependency inventory. See `LICENSING.md` for the scope of the project's
grant and the treatment of existing contributions.

## Maxxfilth Mod Menu

Mod Menu 2 adapts the schema, configuration and shared-variable backend from
Maxxfilth's Mod Menu 2.0.2. Copyright (c) 2026 Maxxfilth. The complete MIT
license is retained in `licenses/ModMenu-MIT.txt` and the packaged notices.
RuneSchema supplies its own Helpy presentation and input handling.
Upstream: https://www.curseforge.com/runescape-dragonwilds/ue4ss-mods/mod-menu

## Other build and runtime components

Helpy uses SDL **3.4.16** for mapped Xbox, PlayStation and Nintendo gamepad input.
The Windows x64 SDK is pinned to the official
[release archive](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16),
SHA-256 `1a784cb2a5c64d56fe7a62090fe9d242d9865f235e4ea9678f1a6ba4e693e7de`.
Its unmodified `SDL3.dll` accompanies Helpy. Copyright (C) 1997–2026 Sam Lantinga.
The complete zlib license is preserved in `licenses/SDL3-zlib.txt` and the
packaged consolidated notices. SDL is not installed into Dragonwilds' renderer
or the RuneSchema bridge DLL.

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

The RuneScape: Dragonwilds wordmark used for game identification on RSMK's Home
transition is third-party game artwork, not RuneSchema MIT-licensed artwork.
RuneScape and Dragonwilds names and marks remain with their respective owners;
this presentation does not imply affiliation or endorsement.

The Helpy map atlas includes an adapted Scorned Wilderness inset from the
Dragonwilds map. Artwork © Jagex. [Atlas attribution and provenance](resources/map-atlas/PROVENANCE.md).

Created using intellectual property belonging to Jagex Limited under the terms of Jagex's [Fan Content Policy](https://legal.jagex.com/docs/policies/fan-content-policy).

## Khronos graphics declarations

Helpy's optional native adapters use the Vulkan C headers from
KhronosGroup/Vulkan-Headers `v1.3.290` and Khronos OpenGL/EGL declaration headers.
Only declarations are vendored; no GPU driver or Vulkan loader is included.
Their original notices, license texts, source locations and hashes are retained
under `source/helpy/vendor/vulkan/` and `source/helpy/vendor/khronos/`.

## Dear ImGui

- Bundled version: 1.92.1.
- Upstream: https://github.com/ocornut/imgui
- Copyright (c) 2014-2025 Omar Cornut; MIT notice retained in the bundled `source/helpy/vendor/imgui/LICENSE.txt`.
- Helpy's build adapts navigation focus drawing to stay inside controls and exclude selectable click-spacing from the highlight. Other builds retain the upstream renderer.

## Lua 5.4.9

The Helpy Mod Menu 2 plugin links Lua for offline configuration parsing.
Source: https://github.com/lua/lua/tree/v5.4.9

Copyright (C) 1994-2026 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Helpy support reader

- Microsoft WebView2 Loader, SDK 1.0.3405.78. The loader uses the machine's
  WebView2 Runtime. See licenses/WebView2-Loader.txt and licenses/WebView2-NOTICE.txt.
- MD4C 0.5.2, MIT. Parses the bundled support guide. See licenses/MD4C-MIT.txt.
