# RuneSchema 0.9.0.8

November 26, 2026. Pre-release.

This update includes [0.9.0.7HF](RELEASE-0.9.0.7HF.md), including saved full
Helpy permissions from one Settings Menu PIN unlock.

## Changes

- Invalid registry asset paths, sections and catalogue pages are isolated while
  valid declarations continue to register. Skipped declarations report
  `RS-RGST-016`; automatic character cleanup stays off for that session.
- A refused native player-state load remains refused unless Save Bypass is
  explicitly enabled. Quick Launch and dedicated-server mode alone no longer
  accept rejected player state with default progress.
- Save Bypass controls explain that entering with default progress can overwrite
  the character on its next save. Native character-validation fallback is unchanged.
- Helpy's diagnostic registry and its exact-count regression test include the new
  registry warning.

Existing settings, activation keys, item identity checks and receipt checks are
preserved. This update does not restore previously reset characters.

## Verification and limits

- Windows Release runtime and all bundled native plugins built successfully.
- Helpy: **194/194** tests passed. Core: **283/289** tests passed, including the
  changed registry-isolation and player-state guard checks. Negative controls
  confirmed that removing the explicit guard fails its regression check.
- Six core source-contract tests still fail: `structured-diagnostics-b3d5`,
  `dedicated-helpy-contract`, `placement-cache-regression`, `map-travel-contract`,
  `helpy-instant-open`, and `native-binding-resolution`. The same test executables
  reproduce those failures against source text from both the previous hotfix and
  Claude's incoming changes; these are not separately rebuilt baseline DLLs.
  Their assertions remain intact.
- Installed payloads on desktop Steam and Game Pass were read back and matched
  SHA-256 hashes. Game Pass loaded an existing character; the owner confirmed
  expected gear/progress and a new character skipped the opening cinematic.
  Steam gameplay and dedicated-server acceptance remain unverified.
- Public ZIP contains the complete RuneSchema runtime, bundled plugins and
  cooked content, with third-party notices and **no Mod Menu activation key**.

The separate J4SSaves cinematic script update is not included in this ZIP. Its
installed receipt-fix DLL is retained in the separate J4SSaves 1.3.1 script update.
