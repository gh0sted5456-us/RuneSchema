Optional USMAP
==============

Place the current game mapping here. The conventional name is:

    Mappings.usmap

Modern UE4SS generated names ending in .usmap are also accepted. RuneSchema
fingerprints the chosen file at startup but does not parse its type table until
a plugin or diagnostic calls the mapping query service. Parsed descriptions use
a small bounded cache keyed by the fingerprint.

Live Unreal reflection is checked before object writes. The file is optional,
and a server/client fingerprint mismatch is a warning rather than a load gate.

This dlls/mappings location is authoritative. Legacy UE4SS-root, ue4ss/mappings,
RuneSchema/mappings, and RuneSchema/shared files remain readable for upgrades.
Regenerate the map after game updates with UE4SS DumpUSMAP, including
Blueprint-generated types. Use the uncompressed UE4SS output.
