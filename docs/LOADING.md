# Loading efficiency

LoadTight1 retains the PakLayout1 lifecycle and load order. It changes two bounded operations within existing synchronous batches.

## Asset queue

The prior loop erased each completed document from a vector immediately, shifting the remaining strings/JSON on each erase. For N successful documents this moves N*(N-1)/2 entries. ConsumeQueue processes in order and moves only retained entries toward the front, then erases the unused suffix. A fully consumed queue performs no entry moves; a partial queue moves at most N entries. Existing vector capacity is retained and draining adds no queue allocations.

The consumer must not resize or re-enter the queue. Asset loading already holds its existing mutex for the batch. The helper requires nonthrowing move assignment. If processing throws, completed entries remain removed while unresolved entries, the failing document and the untouched suffix remain in original order. Native property writes before a failure are not rolled back, matching the previous behavior. Asset resolution, clone identity checks, deferred patch handling and error reporting are unchanged.

## Early tables

The existing live-object walk now counts unique, valid snapshots and stops when all queued addresses have been found. Duplicate visits do not decrease that count twice. Missing/invalid/non-table addresses still cause a full walk. The replay continues in original arrival order and revalidates array slot, address, serial and live flags immediately before each table registration. Serial zero remains valid. No early pointer is dereferenced to bypass discovery.

## Review findings and limits

The available pre-change UE4SS.log identifies Compact2, not PakLayout1. Its timestamps show about 1.47 seconds between config load and RuneSchema's loaded message, covering signature/offset/early-hook work. Without an enabled startup trace this interval cannot be attributed to the signature scan alone. First loader initialization to final registration spans about 0.68 seconds and includes synchronous game work. These are historical observations, not comparative benchmarks.

The cached UE4SS SinglePassSigScanner source already serializes match callbacks with m_scanner_mutex. RuneSchema's nine bootstrap signatures still have callers. No signatures, scanner settings or locks were removed. Recipe placement passes were reviewed but retained because later table changes can require placement again. No extra polling, background Unreal access, configurable boot stages or address cache was introduced.

## Validation

The actual compaction helper matches the old erase loop across 9,217 retained/consumed/exception cases. A 10,000-entry all-consumed fixture reduces entry moves from 49,995,000 to zero; this is a synthetic operation count, not a game-load time claim. Tests extract the production asset-batch and replay bodies and cover order, unresolved documents, clone errors, mid-batch exceptions/recovery, missing/invalid/replaced identities, serial changes, duplicate visits and early termination. Existing readiness concurrency/overflow checks also run. Shipping optimization, imports, source boundaries and archive hashes are checked separately.

For the next launch, verify the LoadTight1 version, early-table replay counts, armor/recipe entries, world entry and normal exit. Advanced verbose logging can produce startup-current.log for a timed comparison; it remains off by default and writes/flushes add overhead. No diagnostic export is required for an ordinary gameplay test. A successful build does not establish a heap-crash fix or dedicated-server compatibility.
