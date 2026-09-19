# Reverse engineering and patching — permitted implementation methods

## Explicit permission

For RazKolbas, Codex is permitted to reverse engineer the supplied reference upscalers and relevant game/rendering interfaces and to implement byte patches where necessary. This includes disassembly, decompilation, matching-PDB analysis, targeted dynamic debugging, shader inspection, ABI reconstruction, API interception, compatibility shims, inline/trampoline/vtable/IAT/delay-IAT/EAT hooks, and runtime or controlled file patches. There is **no blanket ban on RE or byte-patching** and no requirement to use only publicly documented interfaces.

If an NR loader, API-version mismatch, calling convention, presentation path or resource contract fails, locate the concrete cause. Reproduce it in a native probe. Implement the justified shim or patch, then verify real feature creation, GPU output and teardown. A returned success code alone is not evidence of a working feature; retain both original probe results and the behavior of the modified path.

Research may execute and patch working copies of the references. Keep pristine copies and hashes for comparison. Do not mistake temporary reference-host experiments for the independent shipping implementation.

## Patch record

Every production or experimental patch has a stable ID and a machine-readable descriptor in `patches/`. Record:

| Field | Required content |
|---|---|
| Purpose | Concrete defect or integration contract and intended observable change. |
| Target | Module name, original file SHA-256, PE architecture/size/version; PDB GUID/age when useful. |
| Location | Verified module-relative RVA or unique section-bounded signature with validated surrounding code; Address Library record when appropriate. |
| Original | Exact expected bytes at the write site; relocatable fields normalized/validated deliberately, never a wildcard replacing the whole check. |
| Replacement | Replacement bytes, or a named code-generated hook builder and its expected installed representation. |
| ABI | Instruction boundaries, overwritten instructions, calling convention, registers/stack, return behavior and trampoline relocation. |
| Lifetime | Startup/runtime activation boundary, thread quiescence protocol, callback ownership, removal conditions. |
| Recovery | Reverse operation, original bytes, dependency rollback, failure scope and next-launch safe mode. |
| Evidence | Trace/capture/test IDs, observed version set, and why this change solves the problem. |

A debug address, RVA or signature from one reference DLL is not automatically a Skyrim engine address. Prefer source changes when the source is owned/available and they are simpler; use binary changes when they are the appropriate integration method.

## In-memory application

Prepare and validate a patch set before modifying code. Resolve the actual ASLR-loaded base, module ownership, section bounds, current bytes, and any existing hook chain. Existing compatible hooks can be chained after validating their target and lifetime; unrelated patches must not be overwritten blindly.

Use a tested detour implementation where applicable. A raw multi-byte code write is not atomic. Apply at a proven safe startup point or establish a transaction/quiescence protocol that covers all threads that could execute the site. A lock taken only by RazKolbas does not stop another game thread. Preserve whole instructions, RIP-relative operands and branch reach. Preserve unwind/control-flow requirements where the generated code needs them.

Change page protections only for the bounded write; restore each affected region's previous protections, flush the instruction cache, read back the result and record the installed patch. On preparation failure, write nothing. On application failure, roll back only while execution is quiescent; an unrecoverable rollback must not resume execution into half-patched code. Log the failure and use the controlled termination path when necessary.

Rollback is ownership-aware: restore only if the current installed state is still ours. Do not overwrite a later mod's patch. Do not free a trampoline or unload its DLL while callbacks/other hooks can still reach it. Disabling a feature can route through a dormant hook until a safe teardown/restart rather than hot-unpatching active code.

These are engineering requirements, not a ban on patching. Microsoft documents matching detour signatures, transaction/thread handling and instruction-cache coherency; see `SOURCES.md` [W1–W3].

## On-disk patching

A file patch is allowed when needed. Implement `tools/Patch-Binary.ps1` with `-Manifest`, `-Input`, `-Output`, `-WhatIf`, and `-Restore` modes; use exact hash/byte verification and refuse ambiguous targets. Default output is a separate working copy. A deployment command may replace a configured target only with its exact original hash, a verified backup and a reversible journal. Never apply silently to arbitrary files sharing a name.

Verify the patched output hash/size/PE structure and record original plus resulting identity. Restore only the matching patched version, not an unrelated subsequently updated file. The plan does not pre-authorize publishing third-party binaries; packaging/provenance decisions are separate.

## Research versus shipping configuration

`General.SafeMode=true` avoids rendering hooks/vendor feature initialization at next startup and preserves native rendering. `Patching.EnableVersionedPatches=true` permits implemented matching patches in normal operation; `Patching.ExperimentalPatches=false` excludes unvalidated experimental profiles by default. The latter can be explicitly enabled for controlled testing. These switches do not determine whether Codex may conduct RE.

A modified compatibility check must not be presented as a new physical GPU capability. Log the unmodified capability result, the chosen compatibility profile, and actual create/evaluate/retirement test results. Officially supported, experimentally validated, rejected and not tested are different statuses.

## Required patch tests

Exact match; changed original byte; wrong hash; zero/two signature matches; out-of-bounds region; insufficient instruction span; unexpected existing detour; already-applied patch; partially prepared patch set; write failure; restoration failure; concurrent execution; stale target after reload; rollback after another owner changed bytes; disk dry run; backup/restore; double restore. Every negative case has a precise result and no blind write.
