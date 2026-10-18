# Optional base-game icon caches

Packs contain small preview pixels, Unreal icon paths, native source content
identities and coverage, not items, gameplay packages, DLLs, saves or player data.
RuneScape: Dragonwilds and its artwork belong to Jagex; pixels are not MIT-licensed.

**Download base-game cache** selects an index candidate by reported runtime/game
version, verifies the pack's SHA-256, then uses normal content-verified import.
Actual native content hashes, not machine-local timestamps, decide compatibility.
Missing or mismatched packs leave local **Build / update** available. Downloading
does not mount or register content. Mods should provide their own `/cache` packs.

Entries must state complete/partial coverage accurately; record missing icons
and warnings rather than claiming an incomplete capture is full. Filenames are
the whole-file lowercase SHA-256 plus `.rsmk-icons`. New builds require new files
and index entries; never overwrite old pixels under another build's identity.
