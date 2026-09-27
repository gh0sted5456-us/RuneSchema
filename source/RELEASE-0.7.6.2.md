# RuneSchema 0.7.6.2

## Quest registry lifecycle correction

- Quest registry preparation is idempotent for a successfully prepared game instance and subsystem.
- Repeated pre-hook, post-hook, and readiness requests no longer republish the same quest identities into native maps.
- A new game instance clears the prior session manifest before preparing its own registry.
- Genuine native identity conflicts now report the owning quest and exact identity instead of the generic `identity is already occupied` message.

The authored quest audit found no duplicate quest keys or persistence IDs. This update corrects the runtime registration lifecycle; it does not require quest JSON changes.
