---
channel: experimental
promote_to: GRAND-EXCHANGE.md
---

!!! warning "Experimental integration"
    Requires matching rebuilt RuneSchema and Helpy. No published installation is changed by this work.

# Grand Exchange integration

Grand Exchange remains an independent mod and companion app. The optional native
Helpy plugin belongs at `Helpy/plugins/GrandExchange`. Its manifest uses `Host: "helpy"`,
`ApiVersion: 1`, and `EntryPoint: "GrandExchange.Helpy.dll"`; the DLL belongs in the
child plugin's **`dll/`** folder. `settings/plugins.txt` accepts its `GrandExchange` ID.
Staging and repair preserve this layout and the plugin's own notices. Installing a
plugin is separate from enabling game mutations or enrolling a character.

## Public data

`RuneSchemaData::SharedDirectoryLocation(root,"grandexchange")` resolves the public
folder, normally `core/grandexchange`, reusing an established legacy folder when
appropriate. The app publishes `items.index.json` with schema
`grandexchange.items.index/1` and an `items` array. Each row carries public item
metadata, including `path`, `icon`, `mod`, stack limit and suggested prices.

Helpy's icon build reads the index on a worker with a 16 MiB / 32,768-row bound.
It accepts canonical object paths only and captures references for registered
native item sources through the existing icon cache. Imported metadata never
grants items or makes an unregistered item a native item.

Account keys, sign-ins, enrollment files, transaction journals and tester passes
remain private to the companion app. Never put them in shared Core folders.

## Navigation API

API 1 has an optional append-only `RuneSchemaHostApi::RegisterNavTab` field.
Check `StructSize` through that field and its pointer before calling it. Registration
is accepted only from the plugin's initialization thread. The plugin declares the
`helpy.nav-tab` capability and supplies a `RuneSchemaNavTabV1`:

- `Route`: stable `<pluginId>.<route>` token, for example `GrandExchange.exchange`;
- `Title` and a short text/glyph `Icon` for the collapsed sidebar;
- `Draw` and `Context`, valid until shutdown.

The UI thread calls `Draw(context,ui)`. `RuneSchemaNavUiV1` supplies host text,
button and separator functions; no private ImGui context or ABI crosses the DLL.
Callbacks must return promptly, perform no blocking I/O, and never register or
unload plugins from a draw callback. The host removes and drains callbacks before
shutdown/unload, including failed initialization. Built-in numeric page IDs stay
unchanged. A removed route returns to Home.

The Grand Exchange tab and Home entry open the full companion workspace inside
Helpy. `RuneSchemaNavUiV1` appends optional `Companion` and `ReturnHome` callbacks;
check `StructSize` and pointers before using them. The companion uses the same
pages and account process as its standalone window. Its own navigation includes
**Return to Helpy**. Older hosts retain an explicit standalone fallback.

`RuneSchemaCompanionSurface.h` defines the local UI transport. The host creates a
same-user, nonce-bound session and binds it to the companion window. Shared D3D11
textures and fences carry pixels; bounded input records carry mouse, keyboard and
controller state. The host copies each ready frame before returning its texture
slot, retains the last picture between publications, and reopens surfaces on
resize. The two processes must use the same graphics adapter. The transport never
carries account keys or game commands. Trading and journal ownership remain in
the companion; the plugin does not become a second inventory writer.

## Verification scope

Core folder, bounded index parser, navigation registration/removal, desktop-host,
header parity and staging/repair contracts have targeted checks. Both the Helpy
desktop and Grand Exchange plugin are compiled. The GPU fixture checks shared
pixels, three viewport sizes, retained frames, mouse forwarding and Return to Helpy
on an NVIDIA GeForce RTX 5080. It uses hidden synthetic windows and two D3D11
devices in one process; it is not an in-game or cross-process acceptance test.
Native icon capture, delivery
backend selection and multiplayer Vault persistence still require isolated game
verification before a release candidate can be accepted.
