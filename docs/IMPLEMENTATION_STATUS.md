# Implementation checkpoint

## Scope

The complete T01–T24 plan in `../RazKolbas_Codex_Handoff/docs/plans/2026-09-19-razkolbas-implementation.md` remains the target. This checkpoint is not a product completion claim.

Current local validation: all 10 CTest groups pass in Debug and Release. Counts below for earlier stages describe their original milestone runs. No Skyrim installation or MO2 test profile has been selected, so game validation remains NOT RUN.

| Task | Implementation | Build/test | Game/GPU |
|---|---|---|---|
| T01 | Inventory and environment tools implemented; host dependency sources pinned; production vendor SDK selection open | 4 inventory regressions PASS; 57 extracted files VERIFIED; original archives MISSING_REFERENCE | NOT RUN |
| T02 | Native-only SKSE DLL, guarded lifecycle, logging, build presets, CI and explicit staging implemented | Debug DLL built; 3 CTest groups PASS, including actual DLL load/export checks | Skyrim load NOT RUN |
| T03 | Shared variant-typed INI schema, requested SR policy, transactions and temporal invariants implemented; full FG pairing/resource contracts remain open | 7 Debug CTest groups PASS; invalid numbers/enums, locked file, failed replacement, reset epoch and generated frames covered | NOT RUN |
| T04 | Partial: exact-hash planner, owned executable patch/restore, atomic pointer leases and reversible working-copy file tooling | 10 Debug CTest groups PASS, including executable/concurrent fixtures and disk regressions | Game patches NOT RUN |
| T08 | Static RE, exact-hash community NR loader, caller-name shim, verified driver parameter factory and direct feature-creation probe | Debug/Release probe built; parameter ABI round trips verified | RTX 4080 SUPER: original init 0xBAD00002; patched-route init/create/release/shutdown 0x1; GPU fence completed, 4 callback allocations/releases balanced; evaluate/output NOT RUN |
| T05–T07, T09–T24 | Not implemented | NOT RUN | NOT RUN |

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

T04: finish production patch descriptors and engine detour/quiescence integration; T05 needs the user-selected Skyrim test installation and exact executable hash. Independent T08 next probe: recover evaluation/resource/temporal contracts and evaluate a known image with fenced GPU readback. Parameter factory and feature 0x12 creation now execute successfully. See `re/NR_BOOTSTRAP.md`, including the community runtime's invalid embedded signature and the measured direct-loading compatibility route. T07 same-adapter interop remains an independent open task. No SR, FG, ENB, ReShade, ImGui or NR image output is implemented or advertised.
