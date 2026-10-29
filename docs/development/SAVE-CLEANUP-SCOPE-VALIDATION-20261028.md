# Selected-save cleanup validation — October 28, 2026

The Steam candidate was tested with isolated copies of existing modded character
and world saves. Original files were read-only inputs and hash-checked throughout.
The private copy retained the other installed RuneSchema mods while the Dye
family was disabled there. Installed games and their settings were unchanged.

## Verified behavior

- A character preview left both seeded character files and both world files
  byte-for-byte unchanged. This checkpoint included a manual preview confirmation.
- Native entry with a copied modded character removed **271 records from one
  character file**: three loadout entries, three picked-up markers, ten new-recipe
  flags and 255 unlocked-recipe entries.
- The pre-cleanup backup matched the original file's SHA-256 hash.
- The other 32 copied files, including the other characters, worlds and native
  backups, remained unchanged at that checkpoint. All 33 original files remained
  unchanged.
- Native character possession followed cleanup. Re-entering with the cleaned
  file caused no further removal or file changes.
- Enabling Blanket with next-entry activation, without restarting the process,
  cleaned two additional copied characters. The originals remained unchanged.
- Saving a disabled Blanket choice with next-launch activation retained the
  current session's already-applied Blanket policy through another load.
- After restarting with the final compressed DLLs, that disabled Blanket choice
  took effect. Restoring all 33 private files before the test again produced
  exactly one changed character, 271 removed records, a verified original
  backup and 32 unchanged files. All originals remained unchanged.

## Why local-file preflight is needed

Steam's local load in this test did not reach the filtered reflected JSON
callbacks. Without file preflight, missing armor disappeared in game, and an
explicit native save subsequently dropped it from the copied loadout. That
visible result did not establish a RuneSchema backup or pruning receipt.

The candidate therefore reads the selected name and GUID through the native
local controller's reflected getters at the verified entry boundary. It checks
the exact file's GUID before reusing the existing backup and atomic replacement
path. Selection ambiguity or identity mismatch leaves the file unchanged and
does not authorize a blanket sweep.

The incoming-payload filter also recognizes the exact native
`DominionPlayerControllerBase:Server_ReceiveLoadFromDisk` function, using a cached
function pointer after its FName check. It does not broaden global event scans.

## Limits

This test covers Steam's local JSON provider. Xbox WGS containers remain
provider-owned; they are not edited as JSON files. Dedicated-server character
cleanup uses incoming payloads, not a client's local character directory.
Selected-save cleanup is now the default by user request after the Steam test.
Existing explicit Blanket choices remain intact. Authenticated Game Pass and
dedicated-server joins still need separate live validation.

The file preflight validates the certified item/recipe sections. World-dependent
quest and journal adapters run later; this test does not claim coverage of every
quest type. No world-building records were removed in this fixture because the
selected world's building IDs resolved.

Quick Settings and the optional Maxxfilth Mod Menu descriptor share
`RuneSchema/settings/settings.ini`. Their typed configuration, activation rules,
preview guards, panel geometry and compressed plugin boundary have separate
automated checks.
