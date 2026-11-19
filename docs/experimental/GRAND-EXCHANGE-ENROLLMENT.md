---
channel: experimental
promote_to: GRAND-EXCHANGE-ENROLLMENT.md
---

!!! info "Feature request"
    This is a requested Helpy change, not installed runtime functionality. Helpy does not
    read Grand Exchange enrollment yet. [Experimental overview](index.md).

# Grand Exchange enrollment: blocking player deliveries

The Grand Exchange is a separate trading app for Dragonwilds. A character has to be **enrolled**
before it can trade there. While a character is enrolled, Helpy must **refuse player-driven
deliveries** to it, and to any world enrolled with it. Otherwise items could be spawned with Helpy
and sold on the market. **Deliveries a server makes through Helpy keep working.**

The Grand Exchange never calls Helpy and never depends on it. Helpy reads two things the Grand
Exchange writes: a file on the PC and a mark in the character's save. The reference implementation
is `app/src/model/CharacterEnrollment.cpp` in the Grand Exchange repository, tested by
`EnrollmentFileTests`.

## Requested behavior

1. Load the enrollment file (below). A missing file has no entries. If the file can't be read or
   fails its integrity check, treat it as missing and log it.
2. Read the enrollment mark from the target character's J4SSave section, key
   `GrandExchange.Enrollment` (`{"token": ..., "sig": ...}`), if there is one.
3. Gather every token from both sources that passes the signature check. For each character, keep
   the one with the highest `seq`.
4. Refuse a **player-driven** delivery if either of these is true:
    - the target character's newest token has `kind: enroll` and `helpyPlayerDeliveries: blocked`;
    - the delivery's world GUID appears in the `worlds` of any such token.
5. Never refuse deliveries a server makes through Helpy.
6. Show this message when refusing: *"This character trades on the Grand Exchange, so Helpy can't
   deliver to it. Un-enroll it in the Grand Exchange app first (re-enrolling takes 14 days)."*

Helpy blocks when **either** the file or the save says the character is enrolled, so deleting the
file doesn't lift the block.

## The token

The Bazaar (the Grand Exchange's server) signs every enrollment and un-enrollment. A token is the
text `ge-enroll/1\n` followed by one line of JSON:

```json
{"account": "Aldric", "at": 1800000000, "character": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071",
 "helpyPlayerDeliveries": "blocked", "kind": "enroll", "name": "Aldric", "platform": "Steam",
 "seq": 3, "worldType": "Standard", "worlds": ["9C1E..."]}
```

- `kind` is `enroll` or `unenroll`. `seq` goes up by one each time that character's enrollment
  changes. **The highest valid `seq` wins**, whatever order the tokens arrive in.
- `helpyPlayerDeliveries` is `blocked` or `allowed`. It is always `allowed` on an `unenroll`.
- `worlds` lists the world GUIDs enrolled with the character.
- `sig` is a hex DER ECDSA P-256 / SHA-256 signature over the whole token text, prefix included.
  Check it with the Bazaar's public key (hex SPKI DER).
- **Trusting the key:** the file carries `bazaarKey`. Accept it only if its fingerprint (`sha256:`
  followed by the hex SHA-256 of the SPKI DER) equals the `main` → `fingerprint` in the Grand
  Exchange's `assets/network.json`. Helpy can read that file from the Grand Exchange install, or
  ship the fingerprint itself. Ignore every token that fails the check.

## The enrollment file

The file is `%LOCALAPPDATA%\GrandExchange\enrollment.ge`. On Linux, the Grand Exchange writes it
inside the game's Proton prefix, so Helpy finds it at the same path under Wine.

| Bytes | Content |
| --- | --- |
| 0-3 | `GEEN` |
| 4 | Format, `1` |
| 5-16 | AES-256-GCM nonce (12 bytes) |
| 17 to end-16 | Ciphertext |
| Last 16 | GCM tag |

The key is SHA-256 of `ge-enrollment-file/1\n` followed by the machine id: the text of
`HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid` (under Proton, Wine's own MachineGuid). There
is no associated data. The plaintext is:

```json
{"kind": "GrandExchangeEnrollment", "format": 1, "bazaarKey": "<hex SPKI>",
 "entries": [{"token": "ge-enroll/1\n{...}", "sig": "<hex>"}]}
```

The encryption ties the file to one PC and stops casual reading. **The signatures are the
protection.** Every time the Grand Exchange app signs in, it rewrites the file from the Bazaar, so
a deleted or copied file is restored.

## Limits

- Detecting which deliveries are player-driven and which come from a server is up to Helpy. The
  rule above depends on Helpy telling the two apart.
- A character the bridge hasn't seen in a world yet is enrolled without worlds. Its worlds are added
  later and arrive as a newer `seq`.
