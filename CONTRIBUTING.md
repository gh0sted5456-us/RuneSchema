# Contributing to RuneSchema

RuneSchema is developed and maintained by the RSDW Modding Community.
The team coordinates updates, testing, and releases. See `AUTHORS.md` for
community credits. Official repository and distribution work are published
under `gh0sted5456-us`.

## Contribution workflow

1. Record the reported behavior, expected result, affected runtime and a small reproduction.
2. Trace the affected code and callers. Keep the change focused on that issue.
3. Build the changed components and run their targeted checks. Test UI changes at usable small and large sizes.
4. Update the relevant guide and diagnostic reference when behavior or codes change.
5. Provide a reviewable candidate with test results and remaining limits. Maintainers review and approve releases before upload.

The same evidence is expected from human and AI-assisted contributions. See
**[If Using AI](IF-USING-AI.md)** for detailed investigation, verification and
handoff instructions. AI is optional; it does not replace contributor review.

Personal instructions, machine paths and private runtime data stay local.
Use the ignored `AGENTS.local.md` for machine-specific instructions; keep
`AGENTS.md` limited to shared project rules.

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
