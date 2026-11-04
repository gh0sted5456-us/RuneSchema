---
channel: experimental
promote_to: PRESENTATION-TRANSPORT.md
---

!!! info "Pre-release · 0.8.1.0"
    Included in 0.8.1.0. Existing releases retain their original behavior.
    [Candidate download and limits](PRE-RELEASE.md).

# Lua scheduling and relay teardown

The optional mod-data relay now uses `LoopInGameThreadWithDelay` directly. It
creates no timer when its file loads. The first successful `Open` creates one
250 ms game-thread timer, shared by that Lua state's open channels. It no longer
chains async `ExecuteWithDelay` into `ExecuteInGameThread`.

Call `RuneSchemaRelay.Close(channel)` when the consumer is disabled or torn down.
It removes that channel's callback and local queued/reassembly/replay state.
Closing the last channel cancels the timer. Reopening starts a new generation;
an old queued callback cannot restart a second loop. Close is local teardown,
not a new server-unsubscribe protocol. Multiple open channels retain their
existing subscription and rate budgets.

Mapping generation remains opt-in through the native nonce handoff. A valid
request schedules one `ExecuteInGameThreadWithDelay(15000, ...)`; absent or
malformed requests schedule nothing. Missing APIs and scheduling failures write
the existing unavailable result. There is no unsafe async fallback or idle
250 ms mapping loop. The timer uses UE4SS's elapsed delay, not Lua CPU time.

The opt-in Mod Menu 2 adapter also rearms directly on the game thread: 100 ms
while a page is open and one second while hidden. Missing or failed scheduling
stops its bridge and releases its shared menu state. The separate Mod Menu
guard remains in place; this adapter never hooks the original game menu.

The pinned UE4SS commit provides these APIs. Its maintainers recommend the
[game-thread delayed actions](https://docs.ue4ss.com/dev/lua-api/global-functions/delayedactions.html)
and identify the old async functions as unsafe in
[#1456](https://github.com/UE4SS-RE/RE-UE4SS/issues/1456).
The separate upstream restart/concurrency report
[#1445](https://github.com/UE4SS-RE/RE-UE4SS/issues/1445) remains outside this mod's
fix. Passing scheduler fixtures does not establish that arbitrary Lua hot
restarts or third-party async callbacks are safe.
