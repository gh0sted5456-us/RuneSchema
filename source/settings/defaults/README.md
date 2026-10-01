# Optional default character baseline

RuneSchema does not require a file in this folder. Its built-in recovery data
contains appearance-only safety values and baseline recovery is disabled by
default.

To use your own baseline:

1. Copy a healthy native character JSON document to this folder.
2. Name it exactly `default.json`.
3. In RuneSchema settings, enable **Default baseline recovery**, enable
   **Use settings/defaults/default.json**, and select only the categories you
   want to merge.
4. Restart the game.

RuneSchema reads only that exact file, at most once during the startup recovery
pass. It does not scan or watch this directory. Existing live values and later
progress win; selected baseline records are only added when missing, and item,
recipe, and quest persistence identities must resolve in the complete live
registries. A missing or invalid external file falls back only to the DLL's
built-in appearance profile.

The rejected/corrupt-character entry bypass is a separate setting and is never
enabled by baseline recovery.
