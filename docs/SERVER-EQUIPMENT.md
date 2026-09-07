# Equipment profiles in Server1

This candidate adds native Surge and Shadowveil profiles for the supplied Windows dedicated-server executable while retaining the existing client profiles. It does not change mod JSON, boot order, callback logic, or loot patch behavior.

| Executable | PE timestamp | SizeOfImage | SHA256 |
|---|---:|---:|---|
| RSDragonwilds-Win64-Shipping.exe | 3306174033 | 230744064 | 1d9d140943604c22b9b50065a00e87b20e448fced8f53e222addbe2177924f86 |
| RSDragonwildsServer-Win64-Shipping.exe | 1580929729 | 207376384 | 1c83a69084cecbd3ce8412b8efbc43c505c9b88eac303ddda0289b22285b29bc |

SHA256 identifies the analyzed files; startup selection uses the existing PE timestamp/image-size contract plus exact 32-byte checks at every hook and nonzero resume site. It does not hash the entire executable at startup. All sites are checked before hook creation. Unknown images, changed bytes and hook errors leave the affected feature disabled. There is no runtime pattern scan or fallback to client offsets on a server.

Offline analysis located unique candidate sites for eight Surge and five Shadowveil hooks. Their containing function fragments have equal sizes and normalized instructions after external absolute addresses are abstracted; local branch offsets and register/member operands remain equal. Every nonzero resume window was compared separately, including the backward Surge resume outside its unwind fragment. This supports reuse of existing callbacks, but abstracting external addresses does not prove all callees/globals equivalent and does not replace live multiplayer testing.

The shared NativeHookContract helper selects profiles and checks hook/resume bytes with overflow-safe bounds. Tests cover both builds, mismatched fingerprints, altered hook/resume bytes and invalid ranges. Active client support is retained. The prior ambiguous error is replaced with the actual failed stage.

## Server test

1. Stop the server and back up RuneSchema/dlls/main.dll.
2. Extract the runtime ZIP into the existing RuneSchema mod folder, replacing only dlls/main.dll. Preserve config and content; do not install the standalone probe or dispatch bridge packages.
3. Restart and check for `Equipment Surge (server): enabled` and `Equipment Shadowveil (server): enabled`. A disabled message now states unsupported build, changed bytes, or hook creation/activation failure.
4. With a compatible client, check Surge legs equipped/unequipped and Shadowveil equipment across melee, bow, magic, utility and evade actions according to the configured rules. Check reconnect, equipment swaps and normal server shutdown.

Native effects may require compatible client behavior as well as server authority. The same Server1 DLL retains the client profile, but client regression and live server results remain pending. A successful build or enabled log is not proof of gameplay stability. Keep Core1 available for rollback.

Build using tools/build-cached.ps1: fresh retained static libraries, five standalone suites, full runtime rebuild, explicit /MD /O1 /Os, map and PDB. Source/build outputs default to the separate server-equipment workspace. The existing upstream SafetyHook nodiscard warning may appear when rebuilding that dependency; do not hide it or describe it as a RuneSchema compiler warning.
