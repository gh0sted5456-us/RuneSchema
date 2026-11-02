---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

!!! info "Pre-release · 0.8.1.0 · testing"
    This is the candidate-testing branch. It is not a stable release, and the
    maintainer is assembling its complete runtime ZIP separately. Existing
    release downloads and tags remain unchanged.

# RuneSchema 0.8.1.0 candidate

The `pre-release` branch collects the current RuneSchema and Helpy changes for
testing. Start with the [itemized changelog](CHANGELOG-0.8.1.0.md). Setup,
defaults and remaining limits are documented in this branch's feature guides.

- [Helpy desktop and settings](HELPY-STANDALONE.md)
- [Controller navigation and alternate shortcut](CONTROLLER-NAVIGATION.md)
- [Native overlay](NATIVE-OVERLAY.md)
- [Resource diagnostics and startup alerts](PERFORMANCE-DIAGNOSTICS.md)
- [Item caches and authoring tools](AUTHORING-AND-REPAIR.md)
- [Save recovery](SAVE-RECOVERY.md)
- [Server mod comparison](SERVER-MOD-LIST.md) and [PIN access](SERVER-ACCESS.md)

## Package status

The complete runtime ZIP will be supplied by the maintainer so required
components are not missed. Do not treat a small binary follow-up bundle or
GitHub's automatic **Source code** archive as a complete installation.

Before publishing the ZIP, check its actual file inventory and hashes against
the matching candidate. Include the core DLL, all required plugin manifests and
payloads, matching Helpy application/bridge/helpers, complete PAK triplets,
required Lua companions, packaged assets, default settings and license notices.
Preserve the chosen installer layout. Review local settings, logs and runtime
data so private server PINs, account identifiers and personal saves are not
accidentally distributed.

The core and project-owned plugin DLLs should retain the verified compression
checks. SDL3.dll stays beside Helpy.exe, with its complete bundled license.
Helpy runs from the installed runtime folder; AppData contains user data.

## Testing and promotion

Automated checks cover the supported code paths and rendered menu layouts.
Virtual controller tests do not certify every USB/Bluetooth driver. In
particular, verify the Guide shortcut and Steam/Game Bar interaction on real
controllers: Windows Guide reservation only covers GameInput clients.

Keep this branch separate until candidate testing and the complete ZIP checks
pass. A source push does not replace the stable download or publish a release.
When the candidate is promoted, move its feature documentation to the normal
guides using the [documentation promotion procedure](../DOCUMENTATION-CHANNELS.md).
