# Diagnostics integration

## Contribution and merge boundary

Integrated the community `RuneschemaDIAGSRC.zip` diagnostics contribution
(SHA-256 `C77912DB5B87E7656077D7CC95A07E9FAEDA361ED6BCD09EEED324D6FE045A95`).
The existing MIT license and community attribution remain unchanged.

Included structured diagnostic contracts, source-site reports, the bounded writer,
legacy-log references, issue grouping, filters, evidence, session history, mutes,
notifications, and reviewed report exports. Loader context now carries the owning
mod, definition file, target and operation where known. Unknown causes remain
unknown; a warning is not a compatibility verdict.

This is a selective merge, not a replacement with the older contributed tree.
Current FName-first event filtering, lazy observers, startup deferral, table replay
cursors, identity-scoped enrollment, PAK rollback and selective cleanup protections
remain authoritative. Inventory deletion, scrapping and their companion storage
features were retired and are deliberately excluded, not deferred work.

## Files and compatibility

- `RSDiagLog.jsonl` is written beside the active runtime's `RuneSchema.log`.
  The traditional log remains available; committed structured records have stable
  references in it rather than becoming duplicate issues in the viewer.
- Diagnostic history and mutes live in the matching AppData runtime directory:
  `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/diagnostics/`, where platform
  is `steam`, `epic`, `gamepass` or `server`. Unknown runtimes use the existing
  `core/runtime/unknown` lane. This avoids cross-platform history-retention races.
- Existing Live Log workspace visibility/preferences are retained. Its displayed
  destination is now **Diagnostics**, with a single overview/issues/Live Log view
  set rather than a duplicate Live Log/Advanced Diagnostics wrapper.
  All log levels feed bounded activity history; only warnings/errors notify.
- History accepts the contribution's version-2 structured snapshots and older
  version-1 snapshots. No game save format or gameplay rule changes are introduced.
- Report exports require a text-report destination and reject the loader's mod
  lists, configurations and save extensions. Exports are explicit, reviewed local
  actions; there is no automatic upload. Path redaction is best effort, not a
  guarantee of anonymity.

## Performance limits

- No additional global ProcessEvent observer or Unreal reflection scan.
- Producer-side diagnostics enqueue to a bounded 2,048-record queue. Serialization,
  coalescing and file rotation run on the writer thread; the sink exposes dropped
  and failed-write counters. JSONL defaults to 10 MiB plus three rotated backups.
- The issue model retains at most 4,000 events / 2 MiB. History snapshots are
  revision-driven, at most once every eight seconds, with one pending snapshot.
  Defaults retain 14 days, 10 sessions and 20 MiB; these are configurable.
- Visible log readers poll at 250 ms; hidden/minimized/background readers at 1 s.
  Pausing the displayed log does not discard incoming issue notifications.
- Session-directory enumeration occurs once per session and on opening the
  session picker, not on a recurring UI timer. Selecting older evidence is an
  explicit bounded disk read.
- DPI is cached at creation and updated by window/monitor-change events. The
  optional hidden-game notification overlay updates at most 10 Hz while active;
  unchanged cards are not repainted. No overlay window is needed without notices.
- The main window draws its existing background through an icon-only caption,
  retaining an accessible RuneSchema title for task switching. DWM still owns
  caption actions and hit targets; native hit tests retain dragging, resizing and the system menu.
  Helpy paints contrast glyphs over those button targets so opaque artwork cannot
  hide minimize, maximize/restore or close. DWM bounds are cached on window-position
  and activation events, with DPI-scaled bounds for hidden-window rendering.
  Caption metrics change only at creation/DPI events. Detached workspaces keep
  their individually named native captions with cached theme colors. Composition
  failure falls back to the native frame. No second image decode or DPI loop is
  introduced. See [Microsoft's custom-frame contract](https://learn.microsoft.com/en-us/windows/win32/dwm/customframe).
  Splash platform artwork is centered below the tips.
- Settings defaults to the upper navigation; Home, Profile and Report a bug
  default below the divider. All rows and the divider share one persisted drag
  order, including cross-divider moves. Old primary-route ordering remains a
  fallback; merely viewing the layout does not rewrite preferences. Both groups
  retain matching column geometry and scroll when their visible rows exceed the
  available height, with the runtime badge kept below them.
- Home centers its runtime/status action above the social links rather than
  duplicating it in the upper-right header. The badge retains details, connection
  status, keyboard navigation and its existing click action. Footer space is
  reserved before sizing Home tiles, including at the minimum window size.

## Verification boundary

Core tests cover schemas, source sites, partial outcomes, writer rotation,
coalescing, UTF-8 paths, production JSONL/legacy reference delivery and shutdown.
Helpy tests cover ingestion, grouping, mutes, history, report redaction and safe
destinations, navigation, geometry and retained native-inventory retirement guards.
The live-reader regression also tails an actual temporary log through the
production worker: initial info/debug history, appended activity, and warning
delivery while the viewer is paused. A read-only Steam log probe confirmed
ordinary log lines populate the model when the structured error file is empty.
Synthetic rendering covers the advanced overview, inspector, report review and
splash placement at normal, compact and minimum window sizes.

These checks do not establish live gameplay frame-time or loading-time changes.
Measure those with the candidate installed before claiming a runtime improvement.
