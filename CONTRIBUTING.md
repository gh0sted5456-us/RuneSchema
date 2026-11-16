# Contributing

RuneSchema is maintained by the RSDW Modding Community. Repository and release
work is published under `gh0sted5456-us`.

## Make a focused change

1. Record the problem, expected behavior, platform/version and reproduction.
2. Trace the affected code, callers and platform variants. Fix the cause without
   unrelated rewrites or dependency upgrades.
3. Build and test the changed behavior. Inspect UI changes at small and large
   sizes. State what was tested and what still needs live verification.
4. Update the matching guide, diagnostic references and release notes.
5. Submit a concise explanation of the change, validation and remaining limits.

Human and AI-assisted contributions follow the same standard. Read
[If Using AI](IF-USING-AI.md) for the verification checklist.

## Branches and releases

- `main`: stable source and normal documentation.
- `experimental`: integration work and its dedicated website area.
- `pre-release`: an assembled candidate and matching guides awaiting promotion.

Follow [documentation channels](docs/DOCUMENTATION-CHANNELS.md). Maintainers
publish verified runtime ZIPs; source archives and small binary patches are not
complete installations. Keep existing release tags and downloads intact.

For the 1.0.0 series, use a unique serial such as `1.0.0.J00001` for the
runtime display version, package and tag. `J0` identifies Jonesy builds. Assign
the four uppercase base-36 characters in ascending order, starting at `0001`,
and keep the numeric CMake and Windows versions at `1.0.0.0`.

Before publishing, check versions, required files, hashes and license contents
in the actual ZIP. Use the supported build launcher or
`build/package-licenses.ps1 -FinalizeBuild`, then run
`build/test-package-licenses.ps1` for packaging changes.

## License and privacy

New original contributions are licensed under this repository's MIT License
unless agreed otherwise before acceptance. Contributors retain copyright;
submitting code does not transfer ownership or relicense earlier work.

Include only material you have rights to contribute. Keep third-party attribution
and complete licenses. Identify adapted material
and update `THIRD_PARTY_NOTICES.md`. See [license scope](LICENSING.md).

Keep personal instructions in ignored `AGENTS.local.md`. Do not commit private
settings, credentials, saves, logs, caches, attachments or generated build data.
