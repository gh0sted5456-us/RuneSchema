---
channel: experimental
promote_to: HELPY-NOTIFICATIONS.md
---

# Cleanup notifications and emergency hotfixes

!!! warning "Experimental — local testing candidate"
    These changes require the matching RuneSchema core and Helpy build. They have
    not been promoted to the stable channel or published as a hotfix release.

## Cleanup messages

After the native player-ready check, a successful character or world cleanup can
show a green notification in Helpy, including its native fullscreen overlay:

- `Removed 12 records from uninstalled mod: Siege.`
- `Removed 12 records.` when ownership is unknown or mixed.

Zero removals do not produce a cleanup popup. A named owner must come from the
removed records' ownership metadata, be absent from the active mod set, and have
no installed mod folder. Disabled or failed loaders with a present folder use the
count-only message. Asset
names and filename prefixes are not ownership evidence. World building notices
use the count-only message where the removed data has no verified owner.

World receipts belong to the selected native save slot and the newly loaded
world. A failed load, another slot's blanket cleanup, or reopening Helpy does not
turn an old receipt into a new cleanup. Connected players receive the notice
through their owned RuneSchema networking component. Both ends need this feature.

Technical receipts remain in **RuneSchema.log**. The informational
[`RS-SAVE-006`](../ERROR-CODES.md#rs-save-006) event supplies the short popup;
the existing detailed receipt distinguishes a committed file replacement from
a repaired native load buffer or character payload. A popup does not claim the
game has already persisted a transformed load payload to disk.

## Receiving a hotfix

Helpy's normal update check also checks the latest 20 GitHub releases for a
published, non-draft tag ending in `hf`, such as `0.8.1.0hf`. A prerelease may
carry a hotfix. The numeric base must match the installed RuneSchema version.
Automatic checks keep the existing daily schedule and network/update-check
preferences; **Check for updates** can be used manually.

An applicable platform asset creates an **Emergency hotfix available** entry in
**Notifications → Updates**. Select **Review hotfix**, close that installation's
game or dedicated server, then choose **Download and review hotfix**. Metadata
checks are allowed during play; downloading and applying are refused while the
target game/server is running or its core DLL is locked.

The worker rechecks the fixed RuneSchema GitHub repository, release/asset IDs and
GitHub's asset SHA-256 digest. It validates the manifest and installed file hashes,
then lists the files that need replacing. Choose **Apply and restart Helpy** to
hand off to the bundled worker. It waits for the original Helpy process to exit
before replacing whole files, verifies the result and reopens the installed app.
It does not terminate the game. Cancelling or letting the 15-minute review expire
leaves installed files unchanged.

Previous changed files and the transaction log stay in the AppData tool job.
Failures attempt rollback and identify any file needing manual attention.
An applied receipt suppresses repeat notices for that installation and asset.
An incompatible private build is refused rather than overwritten.

## Preparing a hotfix release

Keep normal releases and tags intact. Publish a separate release with the final
`hf` marker. Upload a separate ZIP for each affected lane:

| Lane | Asset for tag `0.8.1.0hf` |
| --- | --- |
| Steam, Epic, dedicated server | `RuneSchema-0.8.1.0hf-Win64-hotfix.zip` |
| Game Pass | `RuneSchema-0.8.1.0hf-WinGDK-hotfix.zip` |

Each ZIP contains `RuneSchema/hotfix.json` plus the affected files at their normal
paths beneath `RuneSchema/`. The manifest is limited to 64 KiB. For example:

```json
{
  "Format": 1,
  "Version": "0.8.1.0hf",
  "FromVersions": ["0.8.1.0"],
  "Platform": "Win64",
  "Files": [
    {
      "Path": "scripts/mod-data-relay.lua",
      "BeforeSHA256": "<64 hexadecimal digits from the installed original>",
      "SHA256": "<64 hexadecimal digits from the replacement>"
    }
  ]
}
```

Use actual SHA-256 values, not the placeholders. An empty `BeforeSHA256` permits
adding a missing base file; it does not permit overwriting an arbitrary existing
file. Every affected file must be declared exactly once. A target already equal
to its replacement is left unchanged. There are no deletion instructions or
remote installer scripts.

Only the repair worker's existing base-file scope is eligible: core DLL, bundled
scripts, `modmenu.json`, and the bundled Helpy, HUD, RegistryBridge, QuestItems and
RSNetworking plugins. User mods, settings, caches, profiles, saves, logs and
backups are excluded. Changes to a UCAS/UTOC package must include its complete
matching PAK/UCAS/UTOC trio. Updating a plugin manifest must preserve its exact ID.

The GitHub release asset must expose a SHA-256 digest. See the
[official release API](https://docs.github.com/en/rest/releases/releases) for its
release and asset metadata. Test the exact ZIP on a disposable installation,
including a failed replacement and rollback, before publishing it.

## Stable RuneSchema and Helpy releases

Helpy checks the latest published stable GitHub release. A newer release
appears under **Notifications → Updates** with links to GitHub, Nexus Mods,
and CurseForge. **Settings → Updates & runtime** offers a manual check.
Preview builds and emergency hotfixes have separate release paths.
