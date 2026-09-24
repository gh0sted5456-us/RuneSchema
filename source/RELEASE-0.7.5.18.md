# RuneSchema 0.7.5.18

This release keeps one universal DLL and preserves the 0.7.5.17 folder as a
rollback point.

## Authoring

- Direct DataAsset edits use the cooked `DA_` path as the target identity and
  the containing mod folder as ownership. New definitions do not need a schema
  URL, repeated mod ID, profile, transaction ID, or target envelope.
- Generated `DA_..._C` targets automatically select their class default object.
- Direct field values use `Set` or `Merge`; `$Set`, `$Merge`, `$Append`, and
  `$AppendUnique` remain available for explicit operations.
- `$MergeWhere` applies one validated field merge to a list of existing
  struct-array identities. The full selector set is checked before the group
  writes. The vanilla-beard example replaces roughly forty repeated patch
  objects with one target, one field, one selector list, and one value.
- Older registry and AssetPatch envelopes remain accepted for installed mods.

## Runtime and diagnostics

- A loader-section failure now returns its status to the main loader. The rest
  of that mod, other mods, and unrelated loaders continue where safe.
- Successful loader writes use a common `[LOADER:<name>][OK]` form and stay
  behind advanced logging. Partial definitions use warnings. Save-integrity
  failures remain errors.
- Game Pass never scans the embedded Win64 executable patterns. It uses the
  Game Pass UE4SS/vtable/reflection lane, while Steam/GOG retains its validated
  native lane. Missing early `UDataTable::Serialize` observation no longer
  aborts core startup.
- The old 0.7.5 WinGDK failures for `UDataTable::Serialize`,
  `FFieldClass::GetNameToFieldClassMap`, `GetObjectsOfClass`, and
  `FName::ToString_Wchar` are covered by the native-binding regression test.

## Documentation

- The authoring and character-creation guides use the direct `DA_` form.
- The loader walkthrough now documents common identity, nested discovery,
  failure isolation, multiplayer ownership, SafeSave boundaries, every loader,
  and patterns observed in the active local mod set.
