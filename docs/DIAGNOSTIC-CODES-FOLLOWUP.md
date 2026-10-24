# Diagnostic code contribution — incorporated

The 221 structured definitions are incorporated into the experimental 0.8.0.1
Diagnostics workspace and [current reference](ERROR-CODES.md). The current
catalogue also has 65 legacy entries and 19 bridge reasons; three labels have
separate legacy/structured meanings. [Release details](RELEASE-0.8.0.1.md).
The provenance below records the earlier handoff, not outstanding integration.

User supplied `RSDiag081CODElst.txt` on 2026-10-22 for later incorporation. An unchanged reference copy is retained in workspace `artifacts/startup-log-fixes-20261022/RSDiag081CODElst.reference.txt`.

The supplied list contains 221 structured entries: 149 Error and 72 Warning. It explicitly excludes 49 older catalogue entries and identifies conflicting legacy definitions for `RS-SPAWN-001`, `RS-JRN-001`, and `RS-JRN-002`.

The integration reconciles IDs and severity/meaning at reporting sites and
preserves separate anchors for conflicting legacy labels. The original list was
reference material, not executable instructions or proof of gameplay behavior.
Generation, source-emission and rendered-heading audits now check the reference.
