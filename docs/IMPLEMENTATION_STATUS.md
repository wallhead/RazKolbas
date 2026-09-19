# Implementation checkpoint

## Scope

The complete T01–T24 plan in `../RazKolbas_Codex_Handoff/docs/plans/2026-09-19-razkolbas-implementation.md` remains the target. This checkpoint is not a product completion claim.

Current local validation: all 13 CTest groups pass in Debug and Release. Counts below for earlier stages describe their original milestone runs. The user selected `D:/TESV_EX` for testing (their typed `D:/TESV/_EX` did not exist). Release DLL/INI are installed in MO2 mod `RazKolbas`, enabled in `TRUE AE V5.32 EXTENDED + OSTIM`; prior loose copies were backed up and removed from game Data. The user's menu/exit test confirmed native-host loading at 21:15 on September 19. The 0.1.1 device-creation observer subsequently captured the actual adapter and swap chain with ENB/ReShade active; frame processing remains unimplemented. See `SKYRIM_SMOKE_TEST.md`.

| Task | Implementation | Build/test | Game/GPU |
|---|---|---|---|
| T01 | Inventory and environment tools implemented; host dependency sources pinned; production vendor SDK selection open | 4 inventory regressions PASS; 57 extracted files VERIFIED; original archives MISSING_REFERENCE | NOT RUN |
| T02 | Native-only SKSE DLL, guarded lifecycle, logging, build presets, CI and explicit staging implemented | Debug DLL built; 3 CTest groups PASS, including actual DLL load/export checks | Skyrim 1.6.1170 native-host load PASS: fresh SKSE and plugin logs; user reports menu/exit |
| T03 | Shared variant-typed INI schema, requested SR policy, transactions and temporal invariants implemented; full FG pairing/resource contracts remain open | 7 Debug CTest groups PASS; invalid numbers/enums, locked file, failed replacement, reset epoch and generated frames covered | NOT RUN |
| T04 | Partial: exact-hash planner, owned executable patch/restore, atomic pointer leases and reversible working-copy file tooling | 10 Debug CTest groups PASS, including executable/concurrent fixtures and disk regressions | Game patches NOT RUN |
| T08 | Static RE, exact-hash community NR loader, caller-name shim, verified driver parameter factory and direct feature-creation probe | Debug/Release probe built; parameter ABI round trips verified | RTX 4080 SUPER: original init 0xBAD00002; patched-route init/create/release/shutdown 0x1; GPU fence completed, 4 callback allocations/releases balanced; evaluate/output NOT RUN |
| T05 | Partial: exact-hash creation observer and ReShade6.7.3 Present/resize/release observers; actual device identity capture | 13 Debug/Release groups PASS; WARP capture, real resize success/failure and release; offline game43 and ReShade38 assertions PASS | Creation interception PASS with ENB/ReShade; RTX4080 SUPER, 2560×1440; 0.1.2 table rejected: no Present/resize/release patches; 0.1.3 provenance probe pending; processing/resource ownership open |
| T06–T07, T09–T24 | Not implemented | NOT RUN | NOT RUN |

## Commands run

- `pwsh -File tests/integration/Inventory.Tests.ps1`: 4 tests failed before implementation, then 4 passed.
- `pwsh -File tools/Inventory-References.ps1 -ReferenceRoot . -Verify`: exit 1, 57 verified entries, 2 missing original archives, no hash mismatches.
- `pwsh -File tools/Inspect-Environment.ps1`: exit 0; local JSON under `artifacts/local/`.
- `pwsh -File tools/Build.ps1 -Preset win-dev`: exit 0; `unit.host`, `integration.inventory`, `integration.plugin` PASS. Three lifecycle tests failed before implementation; the DLL integration test failed before the DLL existed.
- `pwsh -File tools/Build.ps1 -Preset win-release`: T02 Release DLL and 3 CTest groups PASS.
- T03: `unit.config`, `unit.policy`, `unit.history`, `unit.frame_identity` failed before implementation and passed afterward; the supplied INI round-trips through the schema. All 7 Debug groups passed together.
- Latest: Debug and Release builds passed all 10 CTest groups. `tools/re/run_nr_probe.py` passed four separate local GPU cases: original initialization rejection, controlled post-init exception, controlled missing-release rejection, and successful direct feature creation/retirement. Fresh reference verification still reports 57 VERIFIED and 2 MISSING_REFERENCE archives.

## Decisions

- Preserve the supplied handoff directory; build implementation at the repository root. The original prompt path was absent; the available `CODEX_PROMPT.md` provides the implementation instructions.
- Initialize the unborn repository on `codex/razkolbas-bootstrap`; no worktree can be based on a nonexistent HEAD.
- Use exact source commits through CMake FetchContent and `runtime/dependencies.lock.json`, avoiding two conflicting dependency manifests. No vcpkg manifest is claimed to exist.
- Pin CommonLib 3.6.0 for C++20. The native-only bootstrap will use its SKSE interface declarations without eagerly initializing its relocation database. Engine integration remains T05.
- Supplied extracted files are sufficient for static RE; missing original archive containers do not block it.

## Next action

T04: finish production patch descriptors and engine detour/quiescence integration. T05 now has the selected Skyrim 1.6.1170 installation, recorded executable hash and successful native-host load logs; next recover/verify rendering integration for that executable. Independent T08 next probe: recover evaluation/resource/temporal contracts and evaluate a known image with fenced GPU readback. Parameter factory and feature 0x12 creation now execute successfully. See `re/NR_BOOTSTRAP.md`, including the community runtime's invalid embedded signature and the measured direct-loading compatibility route. T07 same-adapter interop remains an independent open task. No SR, FG, ENB, ReShade, ImGui or NR image output is implemented or advertised.

T05 observer checkpoint: see re/SKYRIM_HOOK_MAP.md. The controlled game run confirmed interception and actual-device capture at 21:45:38 with ENB and ReShade active; next establish frame/resize boundaries and resource lifetimes. Normal configuration defaults remain opt-out. No frame processing or vendor runtimes were added.

0.1.2: verified ReShade swap-table profile and pass-through Present/Present1/ResizeBuffers/ResizeBuffers1/Release callbacks implemented. See re/SWAPCHAIN_TRACE.md. No backbuffer or COM resources retained; no world/pre-UI/source-frame identity inferred from Present. Configuration now accepts comma-separated known disable IDs. The 0.1.2 game run rejected the returned table; no presentation hooks were applied. 0.1.3 logs actual table/method ownership for the next short menu probe.
