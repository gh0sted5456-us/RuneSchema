# RSMK schema and workspace improvements

The desktop title is RSMK (RuneSchema Modding Kit). Backend plugin IDs, DLL
filenames, settings and AppData paths remain `RuneSchema.Helpy`; users do not
need to move existing state. The sidebar logo's R follows the chosen accent
without changing its brackets or generating new icon captures.

## Item Bay

Use **Detach Item Bay** to open a separate movable/resizable desktop window.
Drag from that window into the main Transmog, Creation or Recipes workspace.
**Dock back**, or closing only the Bay, returns it to the main window without
losing its selection/filter state or closing RSMK. F2 hides both windows and
returns to the game. This is one shared catalogue/context, not a second scan.
Detached state is session-only.

Docked and detached views keep the same hover descriptions/stats, right-click
actions and selected-item details. **Show item details** pins a row in the
collapsible selected-item pane; its native facts, description, resolved icon,
reload and copy-path controls use the shared inspection cache. Docking does not
discard selection or completed inspection data. Paging/filtering can leave the
selected item's detail record visible without adding it to the new result set.
World changes clear stale selections/details. Hover tooltips remain transient
and are constrained to the native surface where the item was hovered, including
when the detached Bay sits on the opposite side of the main window.

## Clone cards

The result card uses the chosen appearance item's resolved native inventory
icon, falling back to the source only when the appearance has no icon. An
explicit custom icon wins. Successful preparation/creation receipts return the
actual resolved icon and created path; this also covers nested native icon
fields rather than only a literal `Icon` property. It does not generate a new
3D render of arbitrary material changes. Existing compatible appearance assets
must still exist and satisfy the native clone checks.

## Recipes

The browser merges loaded RecipeData, mounted native metadata (including a
recursive subclass query), cooked declarations and RuneSchema definitions.
Results deduplicate by object path, identify owner/source, and search before
paging. Lists above 32,768 no longer reject the entire request; the bounded
metadata catalogue supports up to 1,048,576 entries, 128 per native page.

Unavailable or failed schema entries stay visibly unavailable, not editable
pretend recipes. Selection still resolves and checks a real RecipeData before
inspection/override. If the live engine's reflected subclass query is not
available, the browser explicitly reports incomplete metadata rather than
claiming it found every unloaded Blueprint-derived recipe. There is no bulk
asset-load sweep at startup.

## Schema and native registry parity

See [asset patches](reference/loaders/assets.md) for `$WearableBridge`, and
[registry declarations](reference/loaders/registry.md) for the eleven supported
native lanes and explicit `Source: Schema` references. Schema-created wearables
join the bridge after successful native identity registration. A bridge refresh
does not allocate IDs or bypass reverse-map validation. Server-deferred items
remain in the normal settled registrar lifecycle.

JSON can clone/patch existing compatible content and the established schema
loaders construct their supported asset types. It cannot conjure missing mesh,
texture, skeleton, material or Blueprint content. Live application to already
equipped items still needs a re-equip/cache-refresh test.

These are source/candidate changes. Standalone tests and previews do not prove
live-game rendering, wearable refresh, multiplayer enrollment or every mod's
recipe coverage. Install and perform a world acceptance test separately.
