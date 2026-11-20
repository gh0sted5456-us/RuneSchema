---
channel: experimental
promote_to: HELPY-DELIVERY-FLAGS.md
---

!!! info "Experimental · feature branch"
    Built and contract-tested, not yet in a candidate. Live game, server and Grand Exchange
    acceptance are still open. [Experimental overview](index.md).

# Helpy delivery flags

Helpy deliveries you make to your own character are **on by default**. They can be turned off
for a character in two ways:

- **By you.** In **Settings > Deliveries**, or on the **Deliveries** page of the
  [character sheet](CHARACTER-SHEET.md), turn off *Allow Helpy deliveries to* a character.
- **By a mod.** A mod writes a signed **flag** into a folder Helpy maintains, pausing deliveries
  to a character or a world it manages. The first user is the Grand Exchange, a trading app for
  Dragonwilds: while a character trades there, Helpy deliveries to it would let players spawn
  items and sell them.

**Deliveries a host makes to another player through Helpy are never paused.**

## What is paused

RuneSchema checks the flags where items are granted, not only in Helpy's interface:

| Path | Checked |
| --- | --- |
| Item Bay, cart and kit deliveries to your own character (single player or as host) | Before the grant |
| The same requests from a client to a server | On the client before sending, and on the server against its own flags folder |
| Clone Creator with *Give* | Before the clone is granted |
| Character editor: adding an item or raising a stack | Before the draft changes |
| A host delivering to another connected player | Never |

A refused delivery shows the mod's message, or *"{mod name} has paused Helpy deliveries to this
character."* A character you turned off shows *"You turned off Helpy deliveries for this character
in Settings > Deliveries."*

World flags apply where the world save is loaded: single player, a listen host and a dedicated
server. A client doesn't know the world-save GUID, so it checks character flags only; the server
checks world flags.

If a pause exists for any character on this PC and Helpy can't identify the character a delivery
is for (for example while the character is still loading), the delivery is refused rather than
guessed.

## Settings > Deliveries

- **Your characters.** Your current character, and any character you turned off, each with an
  *Allow Helpy deliveries* switch.
- **Pauses set by mods.** Each registered mod, whether its key is verified, the flags currently in
  force (character or world, and the mod's message), and how many flags failed their signature
  check.
- **Temporary lifts.** A lift with `until` shows as *Allowed until {date} by {mod name}* under the
  mod, and under the character when your own switch is on. After it lapses, the pause is listed
  again with *Its temporary lift ended {date}*. Dates are in this PC's local time.
- **Let this mod pause deliveries.** On by default, per mod. Turning it off is recorded in
  `helpy-state.json`, so the mod can see it; the Grand Exchange treats such a character as not
  enrolled. Only the mod can lift its own flags.
- **Trust the new key.** Shown when a mod's key changed after Helpy first saw it. The reset is
  recorded.

## The folder

```text
%LOCALAPPDATA%\RSDragonwilds\RuneSchema\core\HELPY\flags\
  helpy-state.json          Helpy writes this; mods may read it
  grandexchange\
    mod.json                the mod's registration
    flags.json              its signed flags
```

An older installation keeps Helpy's shared folder at `RuneSchema\HELPY`. Use that folder's
`flags` when `RuneSchema\core\HELPY` doesn't exist and `RuneSchema\HELPY` does; never create
`core\HELPY` yourself, because Helpy would then stop using the older folder. Under Proton the
same path is inside the game's prefix. A dedicated server reads the folder on the server PC.

**`mod.json`:**

```json
{"id": "grandexchange", "name": "Grand Exchange", "publicKey": "<hex SPKI DER, ECDSA P-256>",
 "url": "https://www.nexusmods.com/runescapedragonwilds/mods/..."}
```

- `id` is 1 to 40 characters of `a-z`, `0-9`, `-` and `_`, and must match the folder name.
- `publicKey` is the hex SubjectPublicKeyInfo DER of an uncompressed P-256 key (91 bytes).

**`flags.json`** (at most 1 MiB and 4096 statements):

```json
{"flags": [{"statement": "helpy-flag/1\n{...}", "sig": "<hex DER ECDSA>"}]}
```

Each statement is the text `helpy-flag/1\n` followed by one line of JSON (at most 2048 bytes in
all):

```json
{"mod": "grandexchange", "target": "character", "id": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071",
 "flag": "noPlayerDeliveries", "on": true, "seq": 3, "at": 1800000000,
 "message": "This character trades on the Grand Exchange. Un-enroll it in the Grand Exchange app to use Helpy again."}
```

- `target` is `character` or `world`; `id` is that GUID (32 hex digits; dashes and braces are
  accepted).
- For each combination of mod, target, id and flag, **the highest `seq` wins**. A statement with
  `on: false` lifts the flag. On equal `seq`, a pause wins, so a replayed "off" can't lift a newer
  pause with the same number.
- `sig` is a DER ECDSA P-256 / SHA-256 signature over the whole statement text, prefix included,
  checked against the mod's registered key. Non-minimal DER is rejected.
- A statement whose `mod` doesn't match its folder is ignored. Flags other than
  `noPlayerDeliveries` are ignored. `message` is at most 300 characters.
- Up to 64 mod folders are read.
- `until` (optional, unix seconds) on an `on: false` statement makes the lift temporary: it lapses
  by itself. See [Temporary lifts](#temporary-lifts).

**`helpy-state.json`** (version 1):

```json
{"version": 1,
 "characters": {"ABCDEFABCDEFABCDEFABCDEFABCDEF01": {"playerDeliveries": false, "changedAt": 1800000600, "name": "Alt"}},
 "mods": {"grandexchange": {"fingerprint": "sha256:...", "pinnedAt": 1800000100, "allowPause": true}},
 "events": [{"event": "key-pinned", "at": 1800000100, "mod": "grandexchange", "fingerprint": "sha256:..."}]}
```

- `characters` lists the characters the player turned off.
- `mods.<id>.allowPause` is `false` when the player chose not to honour that mod.
- `events` keeps the last 64 changes: `key-pinned`, `key-reset`, `user-deliveries-on/off`,
  `mod-pause-allowed/declined`.

## Temporary lifts

A mod can lift its own pause for a while and have it come back by itself. The first use is the
Grand Exchange's **tester passes**. Its operator gives a trusted tester a pass, which the Bazaar
checks against its own signature, the tester's account, an expiry and a revocation list. While the
pass lasts, that tester's characters may take Helpy deliveries, so the operator can test freely.
The Bazaar signs the lift with the pass's expiry:

```json
{"mod": "grandexchange", "target": "character", "id": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071",
 "flag": "noPlayerDeliveries", "on": false, "until": 1802592000, "seq": 4, "at": 1800000000,
 "message": "Tester pass: Helpy can deliver to this character until 2027-02-15."}
```

How the reader treats it:

1. `until` is optional. When present it must be a positive whole number of unix seconds; any other
   value (0, negative, a string, a fraction) makes the whole statement invalid and it counts as
   rejected, so a malformed lift never stands in for an unlimited one. On an `on: true` statement a
   valid `until` is ignored.
2. The winner is chosen as before (highest `seq`, pause wins ties). If it is an `on: false` with an
   `until` and the PC's clock has reached `until`, it is treated as a pause. Its message is the
   mod's message from its newest `on: true` statement for that target, or the default *"{mod name}
   has paused Helpy deliveries to this {target}."*
3. The gate reads the flags folder on every delivery, so a lift ends at `until` without Helpy
   restarting.
4. The PC's clock is used. Winding it back only stretches a pass the server already chose to give,
   and the Bazaar marks every deposit from that character as a tester's.
5. A player who declined the mod's pauses is still not blocked when a lift lapses.

Revoking a pass early is an ordinary newer `on: true` statement with a higher `seq`, which the
Grand Exchange app writes on its next sync.

The Grand Exchange already signs the same values in its enrollment tokens (`helpyPlayerDeliveries:
"allowed"`, `helpyAllowedUntil`, `tester: true`; its `docs/handoff/HELPY-ENROLLMENT.md`). When its
app writes flags, they go into the statement above.

## Why signatures and not shared encryption

Any key shipped inside a mod or inside Helpy can be extracted, so a shared encryption key would let
anyone forge or edit flags. With signatures, a mod keeps its **private** key (the Grand Exchange's
stays on its server) and Helpy needs only the **public** key, which is safe to publish.

**Pinning keys.** A player who wanted deliveries back could replace a mod's `mod.json` with a key of
their own, so that the real flags fail the check. To prevent this:

- Helpy has a small built-in registry of mods and their key fingerprints (`sha256:` followed by the
  hex SHA-256 of the SPKI DER). A mod in the registry whose `mod.json` key doesn't match is ignored.
  The Grand Exchange's entry is added once its `main` key is published.
- For any other mod, Helpy pins the first key it sees in `helpy-state.json`. A changed key is
  refused until the player chooses *Trust the new key*, and the reset is recorded.

## What mods do alongside this

The folder is the shared mechanism; a mod that needs stronger guarantees adds its own backstop. The
Grand Exchange, for example:

- writes the same signed flag into the character's J4SSave section, so deleting the folder doesn't
  lift it;
- can send its `flags.json` and `helpy-state.json` to its server with every deposit, so the server
  refuses deposits from a character whose local flag is missing, older than its own record, or
  declined in `helpy-state.json`.

## Limits

- Flags are per PC. A mod that cares about other PCs writes the flag wherever its app runs.
- A player can delete the folder or edit `helpy-state.json`; the mod's own backstops are what make a
  pause stick.
- Deliveries made by other mods' own systems (for example cooked item rewards) aren't Helpy
  deliveries and aren't checked.
