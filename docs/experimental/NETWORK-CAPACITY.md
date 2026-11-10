---
channel: experimental
promote_to: PRESENTATION-TRANSPORT.md
---

!!! info "Experimental"
    These limits apply to the network-capacity candidate. Use matching
    RuneSchema runtimes when opting into larger payloads.

# Network bridge capacity

The [presentation transport guide](../PRESENTATION-TRANSPORT.md) describes the
bridge API. This candidate uses the same cooked RPC signatures and chunk format.

## Inline messages

Generic bridge actions, presentation cues and appearance routes can carry up
to 16 KiB of UTF-8 JSON per message. Existing appearance declarations still
default to 4 KiB. Set `Appearance.MaxBytes` to `16384` to opt in. World-state
snapshots keep their existing 16 KiB aggregate limit. For larger opaque data,
declare a bulk relay channel.

## Bulk relay

Set `Relay.MaxBytes` to up to `4194304` (4 MiB). The default remains 64 KiB.
The server supports 512 mod channels and 128 peers. Each 1 KiB raw chunk fits
in a JSON envelope of at most 4 KiB.

Updated subscribers advertise their receive capacity with `maxBytes` in the
version-1 subscription. The authority returns `relay-capacity-v2:<bytes>` with
the registered channel's upload limit. Subscribers that omit `maxBytes`
retain the 1 MiB receive limit. Older authorities still accept legacy-sized
transfers and return their original receipt.

The server stores one copy of a queued message and sends its chunks in order
to each subscriber. It rotates across recipients, sending up to 16 chunks
every 250 ms and up to four per recipient in that interval. The Lua helper
sends up to three chunks every 250 ms after a capacity-aware receipt. Large
payloads take time to deliver, especially with many recipients.

Server budgets are 32 MiB each for incoming assemblies and retained snapshots,
and 64 MiB of unique queued message data across at most 8,192 deliveries. The
Lua helper reserves up to 16 MiB each for incoming and outgoing messages.
The server continues to enforce channel ownership, publish permissions,
ordering, checksums and per-peer rate limits.

Receivers update the checksum as each chunk arrives. The bridge retains packet
receipts and rejection diagnostics while omitting verbose trace rows for
successful bulk chunks.
