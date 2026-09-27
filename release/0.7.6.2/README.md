# RuneSchema 0.7.6.2

Use `RuneSchema-0.7.6.2-Universal.zip` for the complete drag-and-drop runtime. Use `RuneSchema-0.7.6.2-Core.zip` only when the matching UE4SS runtime is already installed.

This patch makes quest registry preparation idempotent per game instance. Repeated Unreal lifecycle and readiness callbacks no longer republish an already prepared quest registry. Genuine conflicts report the exact quest and identity.
