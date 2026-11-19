---
channel: experimental
promote_to: HELPY-DELIVERY-FLAGS.md
---

!!! info "Feature request"
    This is a requested Helpy feature, not installed runtime functionality. Helpy doesn't read
    delivery flags yet. [Experimental overview](index.md).

# Helpy delivery flags

Player deliveries are **on by default** for every character and world. A mod can switch them
off for a particular character or world by writing a **flag** into a folder that Helpy maintains.
Helpy reads every mod's flags before it makes a player-driven delivery. Any mod can use this; the
first user is the Grand Exchange, a trading app for Dragonwilds. While a character trades there,
Helpy deliveries to it would let players spawn items and sell them.

**Deliveries a server makes through Helpy are never blocked by flags.**

## Requested behavior

1. Before each **player-driven** delivery, Helpy gathers the valid flags (below) for the target
   character's GUID and the target world's GUID.
2. If any mod's newest flag for either target is `noPlayerDeliveries` with `on: true`, Helpy refuses
   the delivery. It shows the mod's `message`, or a default like *"{mod name} has paused Helpy
   deliveries to this character."*
3. Helpy's Settings lists the flags currently in force: which mod set them, on what, and why. The
   player can see what is blocking deliveries but can't turn the flags off there; only the mod that
   set a flag can lift it.
4. A setting that defaults to on, *Let mods pause player deliveries*, is honored per mod. Turning it
   off for a mod is recorded, and that mod can find out through `helpy-state.json` (below). The
   Grand Exchange treats an opted-out character as not enrolled.

## The folder

`%LOCALAPPDATA%\RSDragonwilds\RuneSchema\Helpy\flags\` (under Proton, the same path inside the
game's prefix). Helpy creates the folder. Each mod gets its own subfolder:

```text
flags\
  helpy-state.json          Helpy writes this: which mods are registered and any opt-outs
  grandexchange\
    mod.json                the mod's registration
    flags.json              its flags
```

**`mod.json`:**

```json
{"id": "grandexchange", "name": "Grand Exchange", "publicKey": "<hex SPKI DER, ECDSA P-256>",
 "url": "https://www.nexusmods.com/runescapedragonwilds/mods/..."}
```

**`flags.json`:**

```json
{"flags": [{"statement": "helpy-flag/1\n{...}", "sig": "<hex DER ECDSA>"}]}
```

Each statement is the text `helpy-flag/1\n` followed by one line of JSON:

```json
{"mod": "grandexchange", "target": "character", "id": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071",
 "flag": "noPlayerDeliveries", "on": true, "seq": 3, "at": 1800000000,
 "message": "This character trades on the Grand Exchange. Un-enroll it in the Grand Exchange app to use Helpy again."}
```

- `target` is `character` or `world`; `id` is that GUID.
- For each combination of mod, target, id and flag, **the highest `seq` wins**. A statement with
  `on: false` lifts the flag.
- `sig` is ECDSA P-256 / SHA-256 over the whole statement text, prefix included. Helpy checks it
  against the key registered for that mod. A statement whose `mod` doesn't match its folder is
  ignored.

## Why signatures and not shared encryption

Any key shipped inside a mod or inside Helpy can be extracted. A key that's shared for encryption
would therefore let anyone forge or edit flags. With signatures, a mod keeps its **private** key
(the Grand Exchange's stays on its server), and Helpy only needs the **public** key, which is safe
to publish. A mod is free to encrypt its own files for privacy, but signatures are what Helpy relies
on.

**Pinning keys.** A player who wanted deliveries back could replace a mod's `mod.json` with a key of
their own, so that the real flags fail the check and are ignored. To prevent this:

- Helpy ships a small registry of known mods and their key fingerprints (`sha256:` followed by the
  hex SHA-256 of the SPKI DER). For the Grand Exchange, that's the `main` fingerprint in its
  `assets/network.json`.
- For a mod that isn't in the registry, Helpy trusts the first key it sees and records it in
  `helpy-state.json`. After that, a changed key is refused until the player resets it in Settings,
  and the reset is recorded.

## What mods do alongside this

The folder is the shared mechanism. A mod that needs stronger guarantees adds its own backstop. The
Grand Exchange, for example:

- writes the same signed flag into the character's J4SSave section, so deleting the folder doesn't
  lift it;
- sends its `flags.json` to its server with every deposit. The server refuses deposits from a
  character whose local flag is missing or older than its own record, and the app then writes the
  flag again.

Helpy isn't expected to enforce any of that; it only needs to honor the flags it can verify.

## Limits

- The rule depends on Helpy telling player-driven deliveries apart from server deliveries.
- Flags are per PC. A mod that cares about other PCs writes the flag wherever its app runs, as the
  Grand Exchange does on sign-in.
