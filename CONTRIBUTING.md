# Contributing to RuneSchema

RuneSchema is developed and maintained by the RSDW Modding Community.
The team coordinates updates, testing, and releases. See `AUTHORS.md` for
community credits. Official repository and distribution work are published
under `gh0sted5456-us`.

## License for new contributions

By intentionally submitting a new original contribution for inclusion, you
agree to license it under the repository's MIT License, unless a different
arrangement is explicitly agreed in writing before acceptance. You must have
the rights needed to make that grant. This applies prospectively; it does not
retroactively license earlier contributions.

Contributors retain copyright in their original contributions unless a
separate valid assignment says otherwise. A pull request, project membership,
or MIT license grant alone does not transfer copyright to the organization.
Preserve applicable author and license notices.

## Third-party material

Identify copied/adapted code and its source revision, retain its complete
license, and update `THIRD_PARTY_NOTICES.md`. Do not change an upstream
copyright holder to RSDW Modding Community. Flag uncertain provenance before
submission. Do not submit game or engine content without the required rights.

## Release checks

Release versions use a channel suffix. Main releases end in lowercase `m`
(for example, `0.7.7.4m`), while experimental releases end in lowercase `e`
(for example, `0.7.7.4e`). The builder and archive finalizer reject a main or
experimental branch carrying the wrong suffix.

Use the supported build launcher, or finalize a direct internal build with
`build/package-licenses.ps1 -FinalizeBuild`. Check the licenses inside the
actual ZIPs before uploading. Run the packaging smoke test:

```powershell
powershell.exe -NoProfile -File build/test-package-licenses.ps1
```

See `LICENSING.md` for scope and `docs/NEXUS-LICENSING.txt` for matching mod-page
wording. Existing public archives must be checked independently.
