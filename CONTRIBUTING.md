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

Keep the build version, plugin manifests, Windows resources and release tag
consistent. Current releases use numeric versions such as `0.8.1.0`; older
`m`/`e` suffixes remain supported for historical versions. The branch and release
status identify the channel; do not invent a suffix for a current build.

Use the supported build launcher, or finalize a direct internal build with
`build/package-licenses.ps1 -FinalizeBuild`. Check the licenses inside the
actual ZIPs before uploading. Run the packaging smoke test:

```powershell
powershell.exe -NoProfile -File build/test-package-licenses.ps1
```

See `LICENSING.md` for scope and `docs/NEXUS-LICENSING.txt` for matching mod-page
wording. Existing public archives must be checked independently.

## Documentation channels

New experimental feature guides belong under `docs/experimental/`, with a visible
experimental notice and a `promote_to` frontmatter path. Register them in
`build/docs-experimental-nav.json`. Keep the normal guides aligned with main.

When promoting a feature, merge its instructions into that normal destination,
update navigation/defaults/walkthroughs and remove the duplicate experimental
entry. Preserve historical changelogs and existing diagnostic anchors. See
[the complete channel and promotion procedure](docs/DOCUMENTATION-CHANNELS.md).
