# Implementation checkpoint

## Scope

The complete T01–T24 plan in `../RazKolbas_Codex_Handoff/docs/plans/2026-09-19-razkolbas-implementation.md` remains the target. This checkpoint is not a product completion claim.

Current local validation: all 15 CTest groups pass in Debug and Release. Counts below for earlier stages describe their original milestone runs. The user selected `D:/TESV_EX` for testing (their typed `D:/TESV/_EX` did not exist). Release0.1.7 DLL/INI are installed in MO2 mod `RazKolbas`, enabled in `TRUE AE V5.32 EXTENDED + OSTIM`; prior loose copies were backed up and removed from game Data. The native host, actual device observer and ENB Present observer work with ENB/ReShade active. A bounded GPU readback of three candidate menu textures succeeded in0.1.6 at23:24:39 on September19. Version0.1.7 replaces automatic startup capture with an explicit hotkey; the physical-key capture passed at00:11:48 on September20 with nonzero motion and nonuniform depth. The three requested gameplay captures are complete and verified; raw motion responds strongly in the fast-pan capture, but units/jitter/live stage remain open. The supplied NVIDIA SR DLL now successfully evaluates a captured frame in the standalone D3D11 DLAA replay (see `re/SR_REPLAY.md`). Production frame processing remains unimplemented. See `SKYRIM_SMOKE_TEST.md` and `re/FRAME_INPUTS.md`.

| Task | Implementation | Build/test | Game/GPU |
|---|---|---|---|
| T01 | Inventory and environment tools implemented; host dependency sources pinned; production vendor SDK selection open | 4 inventory regressions PASS; 57 extracted files VERIFIED; original archives MISSING_REFERENCE | NOT RUN |
| T02 | Native-only SKSE DLL, guarded lifecycle, logging, build presets, CI and explicit staging implemented | Debug DLL built; 3 CTest groups PASS, including actual DLL load/export checks | Skyrim 1.6.1170 native-host load PASS: fresh SKSE and plugin logs; user reports menu/exit |
| T03 | Shared variant-typed INI schema, requested SR policy, transactions and temporal invariants implemented; full FG pairing/resource contracts remain open | 7 Debug CTest groups PASS; invalid numbers/enums, locked file, failed replacement, reset epoch and generated frames covered | NOT RUN |
| T04 | Partial: exact-hash planner, owned executable patch/restore, atomic pointer leases and reversible working-copy file tooling | 10 Debug CTest groups PASS, including executable/concurrent fixtures and disk regressions | Game patches NOT RUN |
| T08 | Static RE, exact-hash community NR loader, caller-name shim, verified driver parameter factory and direct feature-creation probe | Debug/Release probe built; parameter ABI round trips verified | RTX 4080 SUPER: original init 0xBAD00002; patched-route init/create/release/shutdown 0x1; GPU fence completed, 4 callback allocations/releases balanced; evaluate/output NOT RUN |
| T05 | Partial: exact-hash creation observer and ENB/ReShade Present/resize/release observers; actual device identity capture | 13 Debug/Release groups PASS; WARP capture, real resize success/failure and release; offline game43, ENB30 and ReShade38 assertions PASS | Creation interception PASS with ENB/ReShade; RTX4080 SUPER, 2560×1440; 0.1.4 ENB Present PASS: 15000 calls, zero failures; menu/Alt+F4 exit observed; resize/Release-zero NOT OBSERVED; processing/resource ownership open |
| T06 | Partial: exact-layout, owned-lock, creation-anchor candidate readback; bounded synchronous CPU bundle | 14 Debug/Release groups PASS; real WARP pixel/binding/raw format tests; mismatched device/context rejected | 0.1.6 captured 2560x1440 colour/motion/depth candidates with ENB/ReShade; hashes verified; menu motion0/depth uniform, world semantics NOT RUN |
| T07, T09–T24 | Not implemented | NOT RUN | NOT RUN |

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

0.1.2: verified ReShade swap-table profile and pass-through Present/Present1/ResizeBuffers/ResizeBuffers1/Release callbacks implemented. See re/SWAPCHAIN_TRACE.md. No backbuffer or COM resources retained; no world/pre-UI/source-frame identity inferred from Present. Configuration now accepts comma-separated known disable IDs. The 0.1.2 game run rejected the returned table; no presentation hooks were applied. 0.1.3 identified the actual ENB outer table; 0.1.4 uses that exact three-method profile and passed automated menu presentation testing.

Next executable work: establish world/pre-UI colour/depth/motion-vector and resource-generation contracts for this game; the verified Present boundary alone is not suitable evidence for SR input placement. Actual game resize and retirement tests remain open. Further menu launch/close probes are authorized and can run through MO2 without asking the user to repeat them.

0.1.6 checkpoint: renderer layout correlated with exact Address Library and live Lock/Unlock code; End already patched by another mod and left alone. 0.1.5 rejected accessor/raw interface inequality; instrumented evidence proved renderer fields matched original creation outputs. 0.1.6 anchors those outputs and revalidates canonical COM identity under the already-owned renderer lock before capture. One complete56.25MiB candidate bundle passed size/hash inspection. Continued Present>=18600, failures0; menu screenshot and Alt+F4 exit observed. Next implement a bounded capture trigger for loaded-save stationary/pan tests, then establish world/pre-UI timing and guide semantics. No additional menu-only user test is needed. SR/FG/NR image evaluation remains inactive.

0.1.7 checkpoint: Ctrl+Shift+F10 capture policy and restart-scoped setting implemented; no automatic timer remains. Six requests/session, five-second cooldown, bounded expiry/retries and release/focus/presentation-gap rearming tested RED/GREEN; all15 Debug/Release groups PASS. Deployed8ad4771, loaded through MO2; menu and idle/no-automatic-capture PASS. Two automated key chords produced no request/capture, so hotkey runtime success is not claimed. Game left open for a physical-key test; then loaded-save stationary/slow-pan/fast-pan captures are needed (docs/CAPTURE_TEST.md). No SR/FG/NR output yet.

0.1.7 physical-key follow-up: user press produced request1 at00:11:48 on September20. Complete bundle18080-136142390 verified independently: all three2560x1440 files have matching hashes/row extents, finite colour and motion, nonzero motion range and1,658,290 distinct low24 depth values. Hotkey-to-readback runtime PASS; prior synthetic-input failure is not reproduced by physical input. Scene/camera conditions were not specified, so no guide direction/scale/depth convention is inferred. Next: three controlled captures in the same outdoor scene (stationary, slow pan, fast pan). Game remains open; five request slots remain as of this evidence.

## Current checkpoint � supplied DLSS runtime replay

User completed all three requested stationary/slow/fast-pan captures and exited Skyrim. All nine raw files passed hash/extent validation; comparison preview inspected. No repeated capture request is pending. See `re/FRAME_INPUTS.md`.

Responding to the user's direction to reuse working libraries: recovered PDPerf's ordinary SR parameter conversion and implemented `RazKolbasSrReplay` against pinned official NGX SDK and the supplied signed SR DLL. The runtime's real D3D11 DLAA evaluation of capture18080-136199593 passed on RTX4080 SUPER, producing finite/nonuniform output with clean retirement. Final normal replay reproduced output SHA256 facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c. Review fixes cover exception termination before GPU owners unwind, Debug CRT library selection and deferred write failures. Post-evaluation injected exception exits10 without a success claim. Both replay configurations build.

Next concrete integration step: recover the reference pre-SR input preparation and jitter/hook location, then connect the verified standard NGX path to the owned Skyrim backend. Existing Present captures cannot establish that timing. No new algorithm is needed. Installed MO2 0.1.7 and its DLL/INI are unchanged; SR/FG/NR processing remains inactive in-game. This checkpoint is offline native-resolution DLAA only, not reduced-resolution SR, temporal quality, FPS improvement or full T11 completion. See `re/SR_REPLAY.md`.
