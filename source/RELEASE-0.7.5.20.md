# RuneSchema 0.7.5.20

## Native compatibility

- Keeps one universal `main.dll` with isolated Steam/GOG and Game Pass/WinGDK native-binding lanes.
- Adds WinGDK 100.4.0.0 patterns verified against the decrypted UE 5.6.1 executable image mapped by Gaming Services.
- Restores verified native resolution for `UDataTable::Serialize`, `FName::ToString_Wchar`, and `FFieldClass::GetNameToFieldClassMap` on Game Pass.
- Routes Game Pass object enumeration through UE4SS hash tables/object-array fallback. The similarly shaped WinGDK callback iterator is deliberately rejected instead of being called as the Steam `TArray` routine.
- Uses the WinGDK-specific `FMemory::Free` form that produced one match instead of the ambiguous shared form.
- Validates every direct or call-resolved target as executable before exposing it to RuneSchema.
- Keeps UE4SS vtable, hash-table, reflection, and safe degraded fallbacks available when an optional binding cannot be validated.
- Adds a fully fingerprinted Game Pass 100.4.0.0 Shadowveil equipment profile for all five action bindings.
- Keeps Surge native behavior inactive on Game Pass until all eight sites and callback register contracts are verified; ordinary equipment effects remain available.
- Isolates journal/lore registration and unlock delivery from the optional native JSON save-cleanup adapter. The Game Pass adapter is explicitly disabled until its complete helper ABI is verified.

## Storefront isolation

- Steam/GOG never scans WinGDK patterns.
- Game Pass never scans Steam-only patterns.
- Shared loaders and plug-ins continue to consume the same RuneSchema API regardless of storefront.
- Every executable hook requires the exact PE timestamp, image size, hook bytes, and resume bytes for its profile before activation.

See `NATIVE-HOOK-PARITY.md` for the audited hook matrix and fallback behavior.
