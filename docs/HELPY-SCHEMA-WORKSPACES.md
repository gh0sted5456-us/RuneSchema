# RSMK schema and workspace improvements

The desktop title is RSMK (RuneSchema Modding Kit). Backend plugin IDs, DLL
filenames, settings and AppData paths remain `RuneSchema.Helpy`; users do not
need to move existing state. The sidebar logo's R follows the chosen accent
without changing its brackets or generating new icon captures.

## Home

Home is the initial workspace. Its non-blocking Dragonwilds-to-RuneSchema logo
crossfade follows Windows' client-area animation preference; the R keeps the
chosen app accent and the brackets retain their artwork. Shortcuts open Item
Service, Map, Recipes and the icon-cache tools without adding a startup wizard.

Quick switches use the existing checked local settings service. Each writes
only its own setting, checks the expected file contents and creates the usual
backup. An unsaved full Settings draft blocks quick saves. Corrupt Save Bypass
requires explicit risk confirmation; all quick switches apply after restart,
not to the current session or a connected remote server.

## Optional workspaces and tool windows

Settings > Workspaces enables **Authoring Tools**, **Tools**, **Live Log**, and
**Character Editor** independently. They are hidden by default; existing profiles
without these choices also start hidden. Settings remains reachable. Hiding a
workspace is a presentation preference, not an authority or server permission.

Map and Recipes can detach into themed desktop windows, sharing their existing
selection, zoom and draft state. **Dock back** or closing a detached window
returns it to Helpy. Item Bay remains one shared catalogue. Minimized tool
windows appear on Helpy's internal taskbar; a Restore button shows the same
window/state without rebuilding its catalogue. F2 and shutdown include all
owned tool windows. Detachment/minimization are session-only.

## Character Editor: verified character copies

The first pass browses native character JSON, stages supported scalar edits,
reviews a patch, and exports a new verified copy. Unknown mod fields are
preserved. Identity, ownership and persistence/version fields,
quest/journal progress and generic structural replacements are not editable here.
Source-file changes invalidate the draft/preview rather than overwriting it.
Cleaning uses the existing inspect/preview/copy flow, not an in-place save edit.

The inventory view shows saved slot, PersistenceID, instance identity, native
stack count, equipped links, and names/icons/source when current loaded ItemData
can resolve them unambiguously. Source and name filters are explicitly local to
the displayed 100-row page. Unresolved identities are not evidence of missing
mods; unknown fields remain untouched.

Select up to 128 records across pages for reviewed, draft-only removal. Sparse
slot numbers and MaxSlotIndex are retained, and known native Loadout references
to removed Inventory slots are unlinked. Unsupported link layouts reject the
whole batch. Existing native Count fields support 1..10000; increases also
require the loaded native MaxStackSize and never exceed it. Single-item records
do not acquire invented count fields. Preview and explicit export confirmation
remain mandatory; originals are never replaced.

The mod-roots view uses current accepted source ownership. Imported character
JSON remains editable across runtime lanes, but attaching/converting Steam
saves to Game Pass requires a verified native save-provider adapter and is not
implemented by renaming JSON. Opaque WGS containers and world/SPUD editing are
not supported in this pass. Originals are never replaced by this editor.

## Item Bay

Completed alphabetical catalogues are scoped to actual mounted item content,
schema/clone paths and language. Icon-source ownership refinements no longer
discard an otherwise valid catalogue. Exact compatible legacy catalogues are
migrated on reuse; changed content still requires a fresh catalogue. Automatic
mod-cache checks stay silent when they import nothing and report no warnings.

The progress strip distinguishes mounted-source/cache checks, saved catalogue
reads, uncached discovery, and visible icon uploads. Visible-icon progress uses
actual demanded texture counts; unknown-duration work uses an indeterminate
indicator, not a timer-based percentage. Missing server-side captures and failed
icons are shown as unavailable rather than an endless loading state.

Complete catalogues also write a bounded, sorted `.json.pages` sidecar. Restoring
the first page reads only its records and a compact offset table, not the entire
JSON catalogue. Search/filter slices read at most 2048 records on a filesystem
worker and keep alphabetical order. Legacy catalogues migrate once in the
background; corrupt/mismatched sidecars fall back to the normal verified index.
Icon-only dependency revisions do not block a still-valid item catalogue; real
content revisions still require verification. Normal source checks retain their
60-second cadence and never sweep/load all Unreal assets for icons.

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

## Game-update reflection mappings

`[RuneSchema] autoUpdateMappings = true` enables a native UE4SS `DumpUSMAP`
request when the running executable differs from the last verified build.
The first request is delayed 15 seconds; subsequent launches reuse the validated
mapping without another dump. Changing this setting requires a game restart.
`RUNESCHEMA_DISABLE_AUTO_MAPPING` is also available as a process-level opt-out.

RuneSchema validates the generated file, required native types, unchanged bytes
and the copied fingerprint before activating an immutable per-build snapshot.
These files live under `RuneSchema/runtime/live/saved/cache/mappings/automatic`;
the previous receipt is backed up before replacement. Missing dumper support,
ambiguous output, invalid data or timeouts retain the shipped fallback and log
`[MAPPINGS][DEFERRED]`. UE4SS itself is not modified.

The dump covers native and currently loaded Blueprint reflection, not every
unloaded asset. It does not refresh native code signatures, waive supported-build
guards, or make incompatible hooks safe. Steam's private menu-only test verified
generation and next-launch reuse. Game Pass reflection was also dumped manually;
that does not establish save-import/conversion safety.
