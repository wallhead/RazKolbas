# Checkpoint review and decisions

The independent read-only review covered the implementation through `e949ba5`. It did not claim that the full T01–T24 product was complete. No minor findings were deferred.

Important findings addressed with failing regressions followed by passing tests:

1. Partial INI replacement: preserve and flush a last-good backup before any rename; recover a missing destination after injected Windows errors 1176/1177 and retain recoverable data on failure. The Windows [ReplaceFileW contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew) explicitly permits partial filename changes on failure.
2. Pointer rollback: retain the lease after transient mapping/protection errors so a subsequent restore can succeed; detect confirmed transfer to a later owner separately. Unresolved destructor cleanup terminates instead of silently losing an active patch.
3. NR retirement: a failed shutdown or pointer restoration cannot unload the runtime or produce successful process exit. A non-vendor callback-boundary test verifies this ordering. The native probe uses controlled process termination if retirement cannot be established.
4. Independently found during review: reject a matching byte pattern embedded inside a different instruction. The regression uses a valid movabs with the pattern inside its immediate bytes; the planner now validates the boundary from a declared known instruction start.

## Follow-up: community NR creation probe

The same reviewer checked the new direct feature-creation experiment, recovered callback contracts and isolated Python parameter probe. Two important cleanup findings were addressed:

- Exceptions after successful NR initialization now terminate the child at the feature-call boundary before main device/shim ownership unwinds. An inner guard also covers command-list/GPU ownership. The `throw_after_init` runtime regression returned 10 with the injected exception marker and no retirement PASS or shim restoration.
- Successful ReleaseFeature alone no longer proves retirement: callback allocation/release counts must balance. The `omit_one_release` regression deliberately retained one of four resources and returned 10 with `RESOURCE_CALLBACK_RETIREMENT_UNBALANCED`; it did not report PASS. The subsequent fault-free run returned 0 with four releases, parameter destruction, shutdown and shim restoration.

All four local GPU regression cases passed, and all 10 CTest groups passed in both Debug and Release. The runtime cases are separate from CTest and require the exact local NR/core DLLs and NVIDIA adapter. Counting callbacks is sufficient for this fixed creation experiment's negative check; future concurrent renderer ownership still requires identity-based resource tracking. Evaluation/output and Skyrim integration remain unverified.

## Rulings retained for the next session

- Work at the repository root and preserve the original handoff directory. Cost if that mapping is unwanted: relocating the original implementation paths; no source handoff was overwritten.
- Bootstrap the unborn branch directly. There was no base commit/worktree to branch from; existing supplied files were preserved.
- Use one exact-commit FetchContent lock rather than parallel vcpkg and source locks. Cost: builds require Git/network on first configure; the pinned commits remain inspectable.
- Use CommonLib 3.6.0 headers for the native SKSE interface without eager relocation database initialization. Cost: the real game integration still needs explicit runtime-profile and lifecycle work.
- Keep the two original archive containers marked MISSING_REFERENCE while accepting all 57 matching extracted files for static analysis. Cost: container-level reproduction remains unverified.
- The unfinished SR/FG/capture/UI/ENB/ReShade/game-detour scope remains open. The review's exclusion of future features is not a product-completion decision.
- Actual SKSE load and NR create/evaluate/resource retirement remain unvalidated. Source review and Init_Ext success are not substitutes for these tests.
- The NR import parser is only accepted for its exact-hash experimental input. It must be hardened or replaced before supporting arbitrary/new runtime versions.
- Final build and reference preservation results are recorded separately by the implementer; the reviewer did not execute vendor/game code.

No merge or push was requested. Preserve this local checkpoint branch and continue the dependency graph; the full implementation remains unfinished.
