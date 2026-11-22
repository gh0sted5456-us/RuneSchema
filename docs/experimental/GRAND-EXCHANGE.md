---
channel: experimental
promote_to: GRAND-EXCHANGE.md
---

!!! warning "Experimental integration"
    Requires matching rebuilt RuneSchema and Helpy. No published installation is changed by this work.

# Grand Exchange integration

Grand Exchange ships as a dedicated Helpy plugin at `Helpy/plugins/GrandExchange`.
RuneSchema core and Helpy are required. Its manifest uses `Host: "helpy"`,
`ApiVersion: 1`, and `EntryPoint: "GrandExchange.Helpy.dll"`; DLLs belong in the
plugin's **`dll/`** folder. The plugin may also declare a `GameEntryPoint` and
`GameCapabilities` for its in-game half and `"Content": true` for its
`runeschema/` data, as described under "Dedicated Helpy plugins" in the
repository's `docs/development/API-REFERENCE.md`.
PAKs go in `paks/<PackageName>/`. `settings/plugins.txt` accepts its `GrandExchange` ID.
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
- `Draw` and `Context`, valid until shutdown;
- optional (0.9.0.7) Home tile fields: `Description`, one line shown when the tile is
  focused; `IconImage`, a `.png` file name directly in the plugin's own folder
  (at most 256×256 and 1 MiB); and `IconFlags`. Helpy tints the PNG like its own
  icons, so draw it white on transparency, or set `RS_NAV_ICON_FULL_COLOR` to show
  it as is. Without an image, the `Icon` text is drawn.

Every registered tab is a tile on Home, after Helpy's own, and a sidebar row with its
icon. A plugin built before 0.9.0.7 passes the shorter struct; it still registers and
gets a text-icon tile. Grand Exchange should set `IconImage` to its logo, with
`RS_NAV_ICON_FULL_COLOR` if the logo is coloured.

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

0.9.0.7 appends two more optional callbacks; check `StructSize` and pointers:

- `InputText(context,label,buffer,capacity)` draws a themed text box (with the
  controller keyboard) and returns 1 when the player presses Enter. `capacity` is
  2 to 4096 bytes and the buffer must already be NUL-terminated.
- `WebPage(context,page)` fills the rest of the workspace with a host-owned web page
  (`RuneSchemaWebPageV1`): `Origin` (`https://host`, no path), `Home`, and an optional
  `Navigate` URL opened once per new nonzero `NavigateId`. Navigation stays on
  `Origin`; other links open in the player's browser only when clicked. Helpy denies
  permissions, downloads, script dialogs, host objects and web messages, and draws
  Back, Forward, Home, Reload and Open in browser. It returns a
  `RuneSchemaWebPageState`. The page shows in Helpy's window and its game overlay;
  in the native overlay it returns `RS_WEB_UNAVAILABLE` after drawing a fallback.
  [Dragonwilds Wiki in Helpy](HELPY-WIKI.md) is the reference use.

## Plugin notices

A Helpy-side plugin can post into Helpy's notifications with the host service
`helpy.notify`:

```json
{"Id":"sale:9f2c","Title":"Listing sold","Detail":"3 Rune bars for 1,200 coins","Open":"GrandExchange.exchange","Toast":true}
```

- `Id` (letters, digits, `.`, `_`, `-`, `:`; up to 128) identifies the notice. Posting
  the same `Id` again replaces it and marks it unread, so use one per sale.
- `Title` (up to 120 characters) and `Detail` (up to 400) are plain text.
- `Open` is optional and must be one of the caller's own routes. The notice gets an
  **Open** button; it never runs anything else.
- `Toast` (default `true`) also shows the in-game popup and, when Helpy is open, a
  toast. Turning off gamer-points popups does not hide plugin notices.

Notices appear under **Notifications → Plugins** and are kept in the notification
history. The reply is `{"Queued":true}`; nothing is queued unless the reply buffer
fits. Each plugin may post 16 at once, then about one a second (`RS_PLUGIN_FAILED`
when over), and invalid notices return `RS_PLUGIN_INVALID_ARGUMENT`. Coalesce bursts,
for example "5 listings sold". For real-time sale alerts, the Grand Exchange
companion or plugin posts a notice when the Bazaar reports a settled sale.

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
