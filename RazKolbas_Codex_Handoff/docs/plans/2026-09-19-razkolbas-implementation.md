# RazKolbas Implementation Plan

> **For agentic workers:** Implement this plan task-by-task. Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` when available; otherwise follow the same task/test/checkpoint sequence directly. Skill availability must not prevent implementation. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build RazKolbas, an independent Skyrim SKSE upscaler with ENB, optional ReShade, DLSS/FSR/XeSS SR and FG, native DLSS 5 NR without RenoDX, GPU-aware settings, and INI/ImGui configuration.

**Architecture:** A Skyrim-specific host captures verified real-frame inputs; one coordinator owns processing, history, configuration and presentation. A same-adapter D3D12 service hosts separate vendor/NR adapters. Native D3D11 copyback and D3D12-backed presentation share the same lifetime model. Reverse engineering and version-specific byte patches are explicitly permitted integration tools.

**Tech Stack:** Windows x64; C++20/MSVC; SKSE/CommonLibSSE-NG; CMake/CTest; pinned package/runtime manifests; D3D11/D3D12; Dear ImGui; typed INI configuration; independently selected NVIDIA/AMD/Intel backends; tested detour and instruction-decoding utilities.

**Spec:** `docs/SPEC.md`. Also read `AGENTS.md`, `docs/PATCHING_POLICY.md`, `docs/re/BASELINE_FINDINGS.md`, and `docs/testing/ACCEPTANCE_MATRIX.md`.

**Plan status:** No implementation or game/GPU test is claimed here. `wallhead/RazKolbas` was empty when inspected on 19 September 2026; re-inspect before acting. This plan's paths, targets and commands are to be created by Codex, not assumed to exist already.

## Global constraints

- Product/module `RazKolbas`; main output `Data/SKSE/Plugins/RazKolbas.dll`; INI `Data/SKSE/Plugins/RazKolbas.ini`.
- R1–R7 from the spec remain in scope; an early milestone is not permission to drop later requirements.
- RE, dynamic debugging, private-ABI recovery, compatibility shims and in-memory/on-disk byte patches are allowed. Apply the tested lifecycle in `docs/PATCHING_POLICY.md`, not blanket bans from old prompts.
- Final product is independent of the reference host DLLs; no mandatory RenoDX or ReShade.
- Same render-adapter LUID initially. Explicit Skyrim runtime profiles; no presumed VR/GOG/all-AE coverage.
- One active SR, one active FG, separate NR, one presentation owner. No source-history advancement on generated frames.
- Default Auto/Quality SR, FG off, NR off; user intent survives capability fallback.
- Pin dependencies/runtime identities. Separate archive-reproduction evidence from the implementation's chosen SDK set.
- No fabricated ABI, relocation, SDK method, native return code, GPU capability or test result.
- Preserve existing user changes; keep large references and local game/capture data outside git.
- Keep `implemented`, `build-tested`, `runtime-tested` independent. A skipped test remains NOT RUN for its feature claim.

## Review focus

1. Existing hook owners or updated binaries: no blind write, correct chaining/ownership and reversible matching patches — T04/T05.
2. SDK bootstrap occurs too late or inside loader lock: no apparent-success path with uninitialized features — T02/T05/T08/T11.
3. UI/FG or D3D11 still consumes a texture after D3D12 processing finishes: no premature allocator/descriptor/texture reuse — T07/T16/T17–T20.
4. Failed SR while render size is reduced, or reset arrives mid-evaluation: valid display-sized fallback and no lost reset — T03/T09/T11/T21.
5. Minimized windows, overlapping overlays and failed Apply/save: no zero-size dispatch, stuck input, stale depth or destroyed last-good pipeline — T10/T12/T15/T16/T23.

## Execution conventions

Read `git status --short`, the current branch, existing build files and local tool availability first. An empty repository has no HEAD commit: create the initial branch/commit without requiring a worktree based on a nonexistent revision. In a populated checkout, use an isolated branch/worktree as appropriate and never reset unrelated work.

T02 establishes these exact commands/presets:

```powershell
cmake --preset win-dev
cmake --build --preset win-dev
ctest --preset win-dev --no-tests=error --output-on-failure
cmake --preset win-release
cmake --build --preset win-release
ctest --preset win-release --no-tests=error --output-on-failure
```

`win-dev`: Visual Studio 2022 x64, Debug, output `build/win-dev/bin/Debug/`.
`win-release`: Visual Studio 2022 x64, Release with useful symbol artifacts, output `build/win-release/bin/Release/`.
Use a pinned dependency manager/toolchain, record prerequisite installation commands, and keep developer-specific paths in ignored `CMakeUserPresets.json` or environment variables. No `winget` assumption. Catch2 or an equally small pinned test runner is acceptable; CTest test groups listed below are the stable external contract.

Each task: create concrete regression/probe tests, demonstrate their initial failure or unimplemented condition, implement, execute targeted tests, execute relevant prior tests, review the diff, then make a focused local commit. For hardware tests, record device/version/capture evidence; when unavailable, mark NOT RUN and continue independent work. Do not mark a whole task runtime-tested because its negative capability test passed.

Create `tools/Inspect-Environment.ps1`, `tools/Run-Validation.ps1`, and `tools/Stage-MO2.ps1` as their owning tasks require them. `Run-Validation.ps1 -Case <ID> [-Provider DLSS|FSR|XeSS]` must run an implemented harness or produce a concrete in-game capture checklist and a NOT RUN record; it must not synthesize PASS. Pass an explicit test install directory to `Stage-MO2.ps1`; never auto-select a personal modlist from a guessed path.

## Planned file ownership

```text
CMakeLists.txt / CMakePresets.json / vcpkg.json / cmake/  build and exact dependency pins
include/rk/                                             owned contracts and interfaces
src/plugin/                                            SKSE entry, messages, shutdown
src/config/                                            typed schema, INI, transactions
src/core/                                              capability policy, history, frame identity
src/patch/                                             module identities, patch plans, ownership
src/skyrim/                                            per-runtime hooks, capture, jitter/domains
src/device/                                            renderer identity and capability probing
src/interop/                                           shared textures, queues, slot retirement
src/render/                                            coordinator, fallback and shader state
src/presentation/                                      D3D11 and D3D12 proxy ownership
src/compat/                                            ENB and ReShade integration
src/backends/{dlss,fsr,xess,nr}/                         independent provider adapters
src/ui/                                                private ImGui, input, diagnostics
shaders/                                               conversions, overlays and composition
harness/                                               standalone interop/NR/presentation probes
tests/{unit,integration}/                               repeatable automated tests
patches/                                               exact versioned patch descriptors
runtime/                                               schema/lock data; not blind copied binaries
tools/                                                 inventory, build/test/package/patch scripts
docs/re/                                               observed contracts, binary evidence
docs/testing/                                          acceptance, captures, support matrix
package/Data/SKSE/Plugins/                              generated MO2 staging tree
```

All these implementation paths are **Create** paths in the inspected empty repository. Adapt only where subsequent actual code makes a different placement more coherent; document the mapping. Do not create empty modules merely to make this tree look complete.

## Dependency graph and milestones

```text
T01 -> T02 -> T03 -> T04 -> T05 -> T06
             |       |       |       +-> T10 (menu can proceed early)
             +-------+-------> T07 -> T08 (early NR track)
T06 + T07 + T03 -> T09 -> T11 (first working DLSS SR)
T11 -> T12 (ENB); T09 -> T13/T14 (other SR); T09 + T10 -> T15 (ReShade)
T05 + T07 + T09 -> T16 -> T17 -> T18/T19/T20 (FG providers)
T08 + T09 + T11 -> T21 -> T22 (NR integration/advanced)
completed relevant feature tasks -> T23 -> T24
```

T08 is not a prerequisite for ordinary SR/FG. It is nevertheless an early active workstream, not a task postponed until every menu and provider is finished. T12 ENB investigation starts in T05; its final integration follows the first SR path.

| Milestone | Required working result |
|---|---|
| M0 | Repository/build/tests, safe SKSE host, settings and tested patch infrastructure. |
| M1 | Verified Skyrim capture, standalone/game interop, first real DLSS SR and independent menu. |
| M2 | ENB, ReShade and all three SR adapters on their viable tested combinations. |
| M3 | Proxy presentation, native UI layer and individual FG providers with complete retirement. |
| M4 | Genuine native NR and complete combined pipeline; advanced NR controls validated individually. |
| M5 | Documented runtime/GPU matrix, measured behavior, reproducible MO2 release and restore path. |

---

## T01 — Repository baseline, reference inventory and dependency selection

**Dependencies:** None.

**Files to create/modify:** `README.md`, `.gitignore`, `tools/Inspect-Environment.ps1`, `tools/Inventory-References.ps1`, `runtime/dependencies.lock.json`, `docs/re/REFERENCE_INVENTORY.md`, `tests/integration/Inventory.Tests.ps1`.

**Interface / deliverable:** Consumes the checkout and explicitly supplied reference directory. Produces a JSON inventory of path, size, SHA-256, PE version and PDB identity plus exact source/SDK/compiler pins.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Missing archive yields MISSING_REFERENCE and leaves all existing files untouched.
  - Changed archive byte yields HASH_MISMATCH and does not replace the pristine baseline.
  - A checkout with unrelated modified files survives inventory/initialization unchanged.

- [ ] Inspect branch/status and repository contents; record the actual baseline and preserve newer work. Create a small initial commit when there is no HEAD.
- [ ] Locate the two original archives via `RAZKOLBAS_REFERENCE_ROOT`; verify hashes from BASELINE_FINDINGS. Extract into ignored `reference/pristine/`, clone working copies for experiments, and identify the matched Fallout PDB.
- [ ] Inspect available MSVC/CMake/Windows SDK/debugging tools. Select exact CommonLib, SKSE headers, UI/INI/test/hook packages and vendor header/runtime sets. Record a missing proprietary/private SDK as an explicit probe dependency, not an invented download.
- [ ] Keep the old fo4test source pinned for comparison. Preserve per-file notices for any imported source. Do not mix archived and chosen production SDK DLLs without a tested contract.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
$env:RAZKOLBAS_REFERENCE_ROOT = "D:\RazKolbas-References" # example local path; use the actual directory
pwsh -File tools/Inspect-Environment.ps1
pwsh -File tools/Inventory-References.ps1 -ReferenceRoot $env:RAZKOLBAS_REFERENCE_ROOT -Verify
```

**Expected result:** Repository baseline, reference status and exact dependency decisions are recorded. Missing references do not prevent building original host/policy code; missing evidence remains visible.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T01 repository baseline, reference inventory and dependency selection` (or an equally specific message). Do not stage unrelated changes or large references.

## T02 — Buildable safe SKSE host and build/test conventions

**Dependencies:** T01.

**Files to create/modify:** `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `cmake/Dependencies.cmake`, `include/rk/Result.hpp`, `src/plugin/Plugin.cpp`, `src/plugin/Logging.cpp`, `src/plugin/Bootstrap.cpp`, `tests/unit/HostTests.cpp`, `.github/workflows/build.yml`.

**Interface / deliverable:** Produces RazKolbas.dll, common `Result<T>`/error codes, deferred bootstrap lifecycle and CTest groups. Consumes exact dependency pins, not vendor GPU devices.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Missing vendor runtimes: plugin loads safely in native mode.
  - Repeated lifecycle events do not double-initialize or double-register callbacks.
  - SafeMode startup installs no rendering patches and makes no provider GPU calls.

- [ ] Create minimal SKSE metadata/load and logging using the pinned CommonLib API. Keep GPU work, waits, SDK initialization and menu construction out of DllMain.
- [ ] Define Created -> BootstrapReady -> RendererAttached -> Running -> Suspended -> Stopping lifecycle transitions. Fail early without half-installed hooks on an unsupported runtime.
- [ ] Create the two presets/output layouts and unit test runner. Default core/host build works without vendor runtime files; missing providers are reported rather than failing DLL load.
- [ ] Add host-only Windows CI build/unit tests. Add a small staging command for an explicit test install, with an uninstall file manifest; do not copy into an unknown game directory.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
cmake --preset win-dev
cmake --build --preset win-dev
ctest --preset win-dev -R "^unit.host$" --no-tests=error --output-on-failure
```

**Expected result:** A real DLL builds, unit tests execute, and a supplied Skyrim test install can load it with native rendering unchanged. Record game load as NOT RUN until actually observed.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T02 buildable safe skse host and build/test conventions` (or an equally specific message). Do not stage unrelated changes or large references.

## T03 — Typed settings, capability policy and temporal invariants

**Dependencies:** T02.

**Files to create/modify:** `include/rk/FrameContracts.hpp`, `include/rk/Settings.hpp`, `include/rk/Capabilities.hpp`, `src/config/IniStore.cpp`, `src/config/SettingsTransaction.cpp`, `src/core/CapabilityPolicy.cpp`, `src/core/HistoryEpoch.cpp`, `src/core/FrameIdentity.cpp`, `tests/unit/PolicyTests.cpp`, `tests/unit/ConfigTests.cpp`.

**Interface / deliverable:** Produces the single settings/schema model, requested/effective selection, reset epochs and real/generated frame identity used by every subsequent task.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Requested DLSS + unavailable DLSS keeps requested DLSS while effective selection falls back with a reason.
  - Evaluate epoch 7, request epoch 8, consume successful 7: reset remains pending.
  - One source frame with three generated displays leaves source ID/history/jitter advanced exactly once.
  - NaN, infinity, unknown enum, zero output extent, denied write and interrupted replacement preserve valid state.

- [ ] Implement config/RazKolbas.ini.example as the new schema, with finite/range/enum validation, defaults, retained unknown keys and requested values. Do not spread separate defaults across menu/backend code.
- [ ] Represent availability, experimental validation and raw SDK result separately. Implement deterministic Auto provider ordering, limits and unsupported-pair reasons using injected capability fixtures.
- [ ] Implement per-provider reset epochs. An evaluation captures epoch N; success consumes at most N, so an arriving N+1 reset remains pending. Generated displays never increment the real-frame ID.
- [ ] Implement atomic settings snapshots and staged application categories; rejected or failed transactions retain last-good effective state. Save through temporary file/replacement with explicit persistence errors.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.(config|policy|history|frame_identity)$" --no-tests=error --output-on-failure
```

**Expected result:** Policy tests are real and platform-neutral. The same typed values will drive both INI and ImGui; mock support is not reported as actual GPU support.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T03 typed settings, capability policy and temporal invariants` (or an equally specific message). Do not stage unrelated changes or large references.

## T04 — Versioned patch manager and reversible file-patch tooling

**Dependencies:** T02, T03.

**Files to create/modify:** `include/rk/PatchDescriptor.hpp`, `src/patch/ModuleIdentity.cpp`, `src/patch/PatchPlanner.cpp`, `src/patch/PatchTransaction.cpp`, `src/patch/PatchRegistry.cpp`, `patches/schema.json`, `tools/Patch-Binary.ps1`, `harness/PatchHarness.cpp`, `tests/unit/PatchTests.cpp`, `tests/integration/PatchTests.cpp`, `tests/fixtures/patch-manifest.json`, generated `tests/fixtures/patch-input.bin`.

**Interface / deliverable:** Produces prepare/apply/rollback operations and installed patch leases. Consumes exact module identity and an explicit safe activation boundary; never accepts an unverified bare address as sufficient.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Owned synthetic x64 function B8 01 00 00 00 C3 returns 1; patch immediate to 2 yields 2; rollback yields 1. This is a test fixture, not a game signature.
  - Wrong hash/bytes, duplicate match, truncated instruction or conflicting hook produces zero writes.
  - A later owner changes the installed site: our rollback does not overwrite it or free a still-reachable trampoline.
  - File dry-run leaves input unchanged; apply to working copy and restore reproduce original SHA-256; wrong patched version is rejected.

- [ ] Define descriptor/manifest validation from PATCHING_POLICY.md. Implement section-bounded unique matching, expected-byte validation, known-hook chaining and idempotent ownership tracking.
- [ ] Build memory-application transactions around a tested detour/decoder. Handle whole instructions, RIP-relative relocations, page protections, instruction-cache flush, thread quiescence and readback. Record an unrecoverable rollback as fatal to safe execution, not ordinary fallback.
- [ ] Implement controlled disk patching with Input/Output/Manifest/WhatIf/Restore, exact hash/bytes, working-copy default, backup and reversible journal. Record the resulting file identity.
- [ ] Expose matching patch decisions/status to diagnostics; experimental profiles require the explicit runtime flag. The owner already permits RE/patching: no method-approval pause.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^(unit.patch|integration.patch)$" --no-tests=error --output-on-failure
pwsh -File tools/Patch-Binary.ps1 -Manifest tests/fixtures/patch-manifest.json -Input tests/fixtures/patch-input.bin -Output artifacts/local/patch-output.bin -WhatIf
```

**Expected result:** The implementation really patches and restores controlled fixtures; failure cases prove no blind writes. Game/vendor patch profiles are added only with their recovered evidence, not placeholder bytes.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T04 versioned patch manager and reversible file-patch tooling` (or an equally specific message). Do not stage unrelated changes or large references.

## T05 — Recover Skyrim hooks and the early provider bootstrap boundary

**Dependencies:** T04.

**Files to create/modify:** `src/skyrim/RuntimeProfile.cpp`, `src/skyrim/Hooks.cpp`, `src/skyrim/RendererBootstrap.cpp`, `src/plugin/EarlyBootstrap.cpp`, `patches/skyrim/`, `docs/re/SKYRIM_HOOK_MAP.md`, `docs/re/ENB_STAGE_TRACE.md`, `tests/unit/HookProfileTests.cpp`.

**Interface / deliverable:** Produces verified HookTarget profiles and a renderer attachment notification containing the actual D3D11 device/context/swap chain. Actual game hook signatures are recovered here, not guessed in this plan.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Unknown game hash or mismatched site has no code writes.
  - Known compatible preexisting hook remains called exactly once with the right arguments.
  - Event trace proves provider bootstrap precedes the relevant device/presentation initialization and never runs under loader lock.
  - Pass-through screenshot sequence and target descriptors match the unmodified baseline.

- [ ] Trace the reference hook-installation and device/resource access regions in BASELINE_FINDINGS. Use CommonLib/Address Library types when confirmed; reconstruct missing signatures and structures from callers/callees.
- [ ] Map device/factory/swap-chain creation, world processing, UI start/end, camera/jitter update and resize/load boundaries for the exact local runtime. Record hash, relocation/signature, expected bytes, ABI, original callee and existing owners.
- [ ] Prove an early provider initialization route outside loader lock. Follow the selected Streamline standard/manual lifecycle; if attaching too late, establish an earlier verified hook or report restart-required instead of fabricating initialized support.
- [ ] Install pass-through hooks through the patch manager. Trace the ENB world/display/UI ordering now, even though final ENB SR integration is T12. Chaining verified compatible hooks is allowed.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.hook_profiles$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case H02
pwsh -File tools/Run-Validation.ps1 -Case B01
```

**Expected result:** At least one exact Skyrim runtime has a documented pass-through hook map. Pre-UI is not labelled pre-SR without evidence; no transplanted reference-DLL RVA.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T05 recover skyrim hooks and the early provider bootstrap boundary` (or an equally specific message). Do not stage unrelated changes or large references.

## T06 — Capture real frame inputs and control render-domain metadata

**Dependencies:** T05, T03.

**Files to create/modify:** `src/skyrim/FrameCapture.cpp`, `src/skyrim/RenderDomains.cpp`, `src/skyrim/CameraHistory.cpp`, `src/skyrim/Jitter.cpp`, `src/skyrim/UiBoundary.cpp`, `shaders/InspectDepth.hlsl`, `shaders/InspectMotion.hlsl`, `tests/unit/FrameContractTests.cpp`, `docs/re/FRAME_INPUTS.md`.

**Interface / deliverable:** Produces immutable FrameInputs with owned resources, source-frame/generation identity, verified guides, colour/exposure and camera state. Consumes the verified hook map.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Slow/fast pan and moving NPC show expected guide direction/scale; stationary camera/object motion is distinguishable.
  - Mixing guide frame IDs or resource generations is rejected before dispatch.
  - Odd extents and near/far/sky/transparency cases have documented representation and no out-of-bounds inspection.
  - Native mode has unchanged display/UI dimensions and unchanged jitter/TAA ownership.

- [ ] Acquire colour/depth/motion/exposure from the same source frame at the traced boundary. Log descriptors, view formats, sample counts, viewport/valid rects and colour encoding.
- [ ] Implement depth/MV diagnostic views and raw capture. Establish MV direction, units, jitter inclusion and object/camera motion, plus depth inversion/nonlinearity and alpha meaning.
- [ ] Separate world, display/UI and special/offscreen domains. Add render-size/jitter controls but activate reduced render sizing and TAA replacement only with a ready output producer transaction.
- [ ] Map first-person, world map, inventory previews, loading, camera cuts and cell transitions. Publish correct reset epochs; do not assume every menu scene uses the main world buffers.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.frame_contracts$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case R01
pwsh -File tools/Run-Validation.ps1 -Case R04
```

**Expected result:** Canonical inputs have trace/capture-backed semantics on the initial runtime. Finding a texture pointer alone does not finish this task.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T06 capture real frame inputs and control render-domain metadata` (or an equally specific message). Do not stage unrelated changes or large references.

## T07 — Same-adapter interop and complete slot retirement

**Dependencies:** T02, T03; may proceed alongside T05–T06.

**Files to create/modify:** `src/device/RenderAdapter.cpp`, `src/interop/SharedTexture.cpp`, `src/interop/QueueBridge.cpp`, `src/interop/FrameSlotRing.cpp`, `src/interop/ResourceState.cpp`, `shaders/ConvertGuides.hlsl`, `harness/InteropHarness.cpp`, `tests/unit/RetirementTests.cpp`, `tests/integration/InteropTests.cpp`.

**Interface / deliverable:** Produces InteropSession and per-slot RetirementSet for D3D11 producer, D3D12 work, D3D11 consumer and future presentation/provider consumers. Consumes actual adapter LUID.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - 3x3, 1279x719 and 1919x1079 round-trips are exact for integer copy paths and within declared tolerance for conversion paths.
  - Render adapter not adapter zero: companion LUID still matches.
  - Delayed D3D11 copyback consumer keeps the slot unavailable after D3D12 completion.
  - Partial record/submit failure and unsupported format cause a precise error with no deadlock/leak.

- [ ] Build a standalone D3D11 -> D3D12 -> D3D11 image round-trip on the same adapter; then attach the same service to Skyrim. Query interfaces, sharing tier and exact formats.
- [ ] Convert unsupported/packed guides explicitly; CopyResource is not a type conversion. Use RAII for handles, COM objects, events, allocators and descriptor/upload storage.
- [ ] Implement directional input/output fences and a consumer-retired signal, plus explicit resource-state handoff. Reuse a slot only after all required consumers finish; prove allocator completion independently of GPU-queued waits.
- [ ] Stress resource-generation changes, delayed GPU completion, partial submit failure, minimize/resize and bounded wait diagnostics. Do not schedule a wait for an unsubmitted signal.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^(unit.retirement|integration.interop)$" --no-tests=error --output-on-failure
.\build\win-dev\bin\Debug\RazKolbasInteropHarness.exe --debug-layers --stress-generations 100
```

**Expected result:** Correct images, recorded LUIDs and clean relevant D3D11/D3D12 validation. CPU allocator reset, texture lifetime and final consumers are all covered.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T07 same-adapter interop and complete slot retirement` (or an equally specific message). Do not stage unrelated changes or large references.

## T08 — Early native NR bootstrap experiment and compatibility implementation

**Dependencies:** T04, T07, reference availability from T01.

**Files to create/modify:** `src/backends/nr/NrRuntimeLoader.cpp`, `src/backends/nr/NrContract.cpp`, `src/backends/nr/NrCompatibility.cpp`, `harness/NrHarness.cpp`, `harness/data/nr-sequence/` generated inputs, ignored `runtime/local-nr.json`, `patches/nr/`, `docs/re/NR_BOOTSTRAP.md`, `tests/unit/NrLoaderTests.cpp`.

**Interface / deliverable:** Produces a versioned native NR runtime contract and genuine create/evaluate/release probe. Independent of Skyrim/ENB/ReShade/RenoDX and independent of the private PureDark wrapper interface.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - No ReShade, RenoDX or reference host DLL is loaded in the standalone NR process.
  - Correct runtime creates and evaluates a genuine NR feature; output inspection and temporal sequence support the result, not only a success return.
  - A required compatibility profile is tested on/off; original failure, modified call path and successful output are all recorded.
  - Wrong hash/contract or evaluate failure produces a precise rejected/failed result and never publishes an unwritten texture.

- [ ] Begin this task early. Trace the matched Fallout Prepare/EnsureFeature/Evaluate and parameter writers plus the Skyrim/PD comparison. Include delay imports and indirect calls; recover exact feature ID, parameter object ABI/factory, call signatures, exports and device ownership.
- [ ] Compare a controlled successful reference run against the standalone probe. Record the first divergent call/argument/result instead of treating a generic NGX error as a complete explanation.
- [ ] Implement the recovered initialization path. If a loader, ABI or module-compatibility condition needs correction, implement a version-locked shim or byte patch through the patch infrastructure and verify its exact effect. No requirement to abandon this because the interface is undocumented.
- [ ] Execute known colour/depth/motion sequences through real NR; validate output writes/fences and feature cleanup, then repeat with changed settings/reset/invalid inputs. Document experimental hardware/runtime status separately from official support.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.nr_loader$" --no-tests=error --output-on-failure
.\build\win-dev\bin\Debug\RazKolbasNrHarness.exe --runtime-manifest runtime/local-nr.json --sequence harness/data/nr-sequence --capture-output artifacts/local/nr
```

**Expected result:** A working native NR path or a concrete, evidence-backed unresolved contract with the next probe. The latter leaves T08 open; it is not a permanent unavailable backend masquerading as completion. Continue independent tasks while investigating.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T08 early native nr bootstrap experiment and compatibility implementation` (or an equally specific message). Do not stage unrelated changes or large references.

## T09 — Single frame coordinator, transactional activation and valid fallbacks

**Dependencies:** T06, T07, T03.

**Files to create/modify:** `src/render/FrameCoordinator.cpp`, `src/render/RenderStateGuard.cpp`, `src/render/SpatialFallback.cpp`, `src/render/PipelineTransaction.cpp`, `shaders/SpatialFallback.hlsl`, `tests/unit/CoordinatorTests.cpp`, `tests/integration/FallbackTests.cpp`.

**Interface / deliverable:** Consumes FrameInputs, settings/capabilities and InteropSession; produces a display-sized FrameResult with generation/reset/submission/retirement identity and one selected stage graph.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - SR fails for 1280x720 input and 1920x1080 output: fallback is valid 1920x1080, not a naked 720p texture.
  - NR-before-SR failure uses original colour; NR-after-SR failure keeps SR output.
  - Concurrent queued reset after captured epoch is not lost on older success.
  - Failed replacement, cancelled dispatch and removed device neither fabricate success nor wait on an unsignaled fence.

- [ ] Implement stage ownership with Native, Ready, Active, Suspended and Failed transitions. Use mock providers that can fail before recording, after recording and after submission.
- [ ] Preserve source colour. Produce a spatially scaled, correctly encoded display-sized fallback for reduced-resolution SR failure. Change native TAA/jitter/render dimensions only at a safe transition, not by assuming native input is already full-size.
- [ ] Use two-phase pipeline replacement: prepare/validate replacements, publish a generation, retire prior state; failure retains the prior valid state. Distinguish recoverable provider failure from device removal.
- [ ] Restore only the game graphics state actually modified by injected work, including bindings/viewports/targets. Track every stage exactly once per real frame.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^(unit.coordinator|integration.fallback)$" --no-tests=error --output-on-failure
```

**Expected result:** A coherent pipeline with honest failure semantics and no fake providers; fallback is a genuine output producer, not an unvalidated pointer assignment.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T09 single frame coordinator, transactional activation and valid fallbacks` (or an equally specific message). Do not stage unrelated changes or large references.

## T10 — Independent ImGui menu and safe input/settings integration

**Dependencies:** T03, T05; run early, not after all providers.

**Files to create/modify:** `src/ui/Menu.cpp`, `src/ui/Input.cpp`, `src/ui/Diagnostics.cpp`, `src/ui/PatchPage.cpp`, `src/config/SettingsMigration.cpp`, `tests/unit/UiTransactionTests.cpp`, `docs/CONFIGURATION.md`.

**Interface / deliverable:** Consumes shared settings/capability/patch status and emits SettingsTransaction requests. Owns its ImGui context and backend lifetime, not another mod's context.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Menu and hotkeys share one validator and produce identical effective configuration.
  - Focus loss/alt-tab while dragging a slider does not leave mouse/key input stuck.
  - Failed save retains the last good file and visible error; restart-required Apply is not reported as active.
  - Separate ImGui instances and patched-module status remain isolated and correct.

- [ ] Build the seven spec tabs with actual implemented settings, requested/effective values and disabled reasons. Unimplemented feature controls are explicitly marked; no working-looking placeholder toggles.
- [ ] Queue Apply/Reload/Reset and hotkey commands to the coordinator boundary. Show live/recreate/restart categories before apply; no GPU resource operations from WndProc.
- [ ] Implement input focus and cursor ownership with ENB/ReShade menus, DPI/UI scale and key rebinding. Release capture on focus loss, menu close and teardown.
- [ ] Implement non-destructive settings migration for relevant reference settings through explicit enum translation. Preserve original INI; support denied-write and last-good recovery.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.ui_transactions$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case H11
```

**Expected result:** A real menu works in the host/native path; all listed diagnostics derive from live implementation state.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T10 independent imgui menu and safe input/settings integration` (or an equally specific message). Do not stage unrelated changes or large references.

## T11 — First complete DLSS Super Resolution pipeline

**Dependencies:** T09, T05; T10 for menu validation.

**Files to create/modify:** `src/backends/dlss/StreamlineRuntime.cpp`, `src/backends/dlss/DlssSr.cpp`, `src/device/DlssCapabilities.cpp`, `tests/unit/DlssContractTests.cpp`, `docs/testing/DLSS_SR.md`.

**Interface / deliverable:** Implements the owned SR provider contract. Consumes HUD-free FrameInputs and an already valid initialization/interop lifecycle; returns actual supported render sizes and a produced FrameResult.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Quality output is display-sized with stable static/slow/fast motion and correct camera-cut reset.
  - Feature supported before device binding but rejected afterward stays unavailable with the real reason.
  - Native-AA uses the actual provider contract, not forced zero render scale.
  - Missing runtime or evaluate failure retains valid rendering without leftover competing jitter/TAA ownership.

- [ ] Use the exact selected vendor headers/runtime. Initialize outside loader lock at the verified early boundary; bind the companion device and recheck features after initialization. A manual-hook path must also execute required per-frame runtime bookkeeping.
- [ ] Submit verified colour/depth/MV/exposure, jitter, matrices and reset through the actual API. Query quality/native-AA dimensions; set mip policy and disable competing TAA only when the replacement is ready.
- [ ] Run Quality first with native D3D11 copyback, then other supported modes. Implement resize/quality/provider recreation and lossless requested/effective reporting.
- [ ] Expose input/output/timing diagnostics and compare native, SR-enabled and provider-failure paths in matching scenes. Remove per-frame loads/compiles/expensive feature probes.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.dlss_contract$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S01
pwsh -File tools/Run-Validation.ps1 -Case S10
```

**Expected result:** First genuine playable SR slice on a recorded GPU/runtime; a DLL load alone is not DLSS support.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T11 first complete dlss super resolution pipeline` (or an equally specific message). Do not stage unrelated changes or large references.

## T12 — ENB world/display separation and verified processing order

**Dependencies:** T11, T05, T06.

**Files to create/modify:** `src/compat/EnbBridge.cpp`, `src/compat/EnbRenderDomain.cpp`, `src/compat/EnbHooks.cpp`, `patches/enb/`, `tests/unit/EnbDomainTests.cpp`, `docs/testing/ENB_MATRIX.md`.

**Interface / deliverable:** Consumes the T05 ENB trace and frame/domain contract. Produces an explicit validated ENB stage and resize policy, using API callbacks, hooks or versioned patches as required.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - ENB off/on at native and Quality preserve display/UI size.
  - DOF/bloom/depth and transitions show no duplicate pass or mismatched scene coordinates.
  - Failed new ENB resource allocation retains active resources and quality.
  - Changed ENB binary or competing hook does not receive an old unverified patch.

- [ ] Recover remaining ENB resources/callbacks through the Skyrim reference and active ENB runtime. Trace current boundaries before assigning NR/SR placement; an exported API alone does not establish final order.
- [ ] Implement scene render extent changes without resizing the HWND/native UI. Handle affected ENB buffers, depth-dependent passes and special targets transactionally.
- [ ] Correct actual incompatibilities with targeted source changes, hooks, compatibility shims or byte patches; preserve the user's existing ENB/ReShade installation and hook ownership.
- [ ] Validate bloom/DOF/exposure/colour processing exactly once, UV scaling, temporal resets and quality/resolution changes. Record exact ENB versions/presets; unsupported profiles get a reason.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.enb_domains$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S04
pwsh -File tools/Run-Validation.ps1 -Case S07
```

**Expected result:** ENB works with the real SR pipeline on recorded profiles, including loading/menu/resize; detection alone does not satisfy R2.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T12 enb world/display separation and verified processing order` (or an equally specific message). Do not stage unrelated changes or large references.

## T13 — FSR Super Resolution backend

**Dependencies:** T09, T01; T12 for combined ENB validation.

**Files to create/modify:** `src/backends/fsr/FsrRuntime.cpp`, `src/backends/fsr/FsrSr.cpp`, `src/device/FsrCapabilities.cpp`, `tests/unit/FsrContractTests.cpp`, `docs/testing/FSR_SR.md`.

**Interface / deliverable:** Implements the same owned SR contract without pretending AMD enum/guide requirements are identical to NVIDIA's.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - An implementation fallback is visible in effective settings.
  - Unsupported format or missing runtime is handled without harming DLSS/native path.
  - Jitter/depth/transparency are tested for this provider rather than inherited as assumed passing.

- [ ] Select a coherent AMD SDK/runtime set and query/create the actual available upscaler. Report the implementation/model actually selected, not simply the requested FSR label.
- [ ] Translate canonical guides, exposure, jitter, reset and colour into that API's requirements; add reactive/transparency inputs only with a validated generation path.
- [ ] Implement quality/native modes, context recreation, supported formats and per-frame failure fallback through the coordinator.
- [ ] Exercise native-vendor and any viable cross-vendor path independently; reuse common interop/coordinator rather than duplicating Skyrim hooks.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.fsr_contract$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S01 -Provider FSR
```

**Expected result:** Actual FSR SR is rendered on the recorded compatible path; untested GPU families stay NOT RUN.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T13 fsr super resolution backend` (or an equally specific message). Do not stage unrelated changes or large references.

## T14 — XeSS Super Resolution backend

**Dependencies:** T09, T01; T12 for combined ENB validation.

**Files to create/modify:** `src/backends/xess/XessRuntime.cpp`, `src/backends/xess/XessSr.cpp`, `src/device/XessCapabilities.cpp`, `tests/unit/XessContractTests.cpp`, `docs/testing/XESS_SR.md`.

**Interface / deliverable:** Implements the owned SR contract using the exact selected Intel interface and separately tracked native/cross-vendor capability.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Native and any supported cross-vendor execution are distinguished from mere DLL presence.
  - Provider switch waits for the prior generation to retire and resets history.
  - Wrong runtime/header identity is diagnosed before an incompatible call.

- [ ] Pin matching Intel headers/runtime and initialize the actual D3D12 context. Discover feature/driver requirements and supported quality options rather than hard-coding a GPU name table.
- [ ] Implement the provider's motion scale, exposure, jitter/history and resource state requirements; inspect executed outputs.
- [ ] Implement context preparation/recreation outside normal per-frame hot paths and correctly retire prior contexts/resources.
- [ ] Run combined Skyrim/ENB modes and capability-negative cases without disturbing other provider state.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.xess_contract$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S01 -Provider XeSS
```

**Expected result:** Actual XeSS SR works through the common Skyrim pipeline on validated configurations.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T14 xess super resolution backend` (or an equally specific message). Do not stage unrelated changes or large references.

## T15 — ReShade coexistence, controlled placement and depth publication

**Dependencies:** T09, T10; T12 for full combination.

**Files to create/modify:** `src/compat/ReShadeBridge.cpp`, `src/compat/ReShadeDepth.cpp`, `src/compat/ReShadeRuntimeRegistry.cpp`, `tests/unit/ReShadeLifetimeTests.cpp`, `docs/testing/RESHADE_MATRIX.md`.

**Interface / deliverable:** Consumes same-device scene/depth with frame/generation retirement. Produces exactly-once selected effect execution and valid semantic bindings; optional when ReShade is absent.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Four ENB off/on x ReShade off/on configurations have separate records.
  - Depth-based effect survives resize/reload and never samples stale generation.
  - Effects run once at the selected stage, including later FG-enabled presentation.
  - ReShade completely removed: core SR/NR initialization remains independent.

- [ ] Select the appropriate runtime/window/device through observed lifecycle/API events. Handle absent, late-created, destroyed and reloaded runtimes.
- [ ] Implement before/after-SR whole-chain placement at validated boundaries, preventing automatic Present rendering plus duplicate manual execution. Restore state and resolve linear/sRGB/HDR views deliberately.
- [ ] Publish depth with source frame, API/device, extent, generation and retirement identity. Convert/share between APIs explicitly; never reinterpret a D3D11 view as a D3D12 view.
- [ ] Handle renderer/proxy changes, ENB combinations, overlay input and effects disabled/reloaded. Add versioned compatibility interception only when necessary and tested.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.reshade_lifetime$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S05
pwsh -File tools/Run-Validation.ps1 -Case S06
```

**Expected result:** ReShade integration is functional and optional; coexistence and placement are proven separately.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T15 reshade coexistence, controlled placement and depth publication` (or an equally specific message). Do not stage unrelated changes or large references.

## T16 — D3D12-backed presentation and real resize lifecycle

**Dependencies:** T05, T07, T09.

**Files to create/modify:** `src/presentation/PresentationOwner.cpp`, `src/presentation/NativeD3D11.cpp`, `src/presentation/D3D12Proxy.cpp`, `src/presentation/ResizeTransaction.cpp`, `harness/PresentationHarness.cpp`, `tests/integration/ProxyTests.cpp`, `tests/unit/PresentationTests.cpp`.

**Interface / deliverable:** Produces one startup-selected presentation owner and accurate COM proxy semantics, including completion tokens for presentation resources.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - ResizeBuffers and ResizeBuffers1 change the underlying resources or report a real error, never fake S_OK.
  - IUnknown/interface identity and reference lifetime remain coherent in the proxy contract.
  - 100 resize/minimize/restore cycles have stable tracked resource counts.
  - Late initialization or unsupported ownership change is explicit rather than a second live swap chain.

- [ ] Study the current Fallout PDB-mapped proxy/resize/retirement functions, not the obsolete source no-op. Implement the needed D3D11-facing/D3D12-backed interface contract with coherent QueryInterface/refcounts/GetDevice/GetBuffer/desc behavior.
- [ ] Validate the bootstrap point that can choose native or proxy presentation before conflicting swap-chain creation. Do not toggle ownership from a slider; declare restart-required when necessary.
- [ ] Implement actual resize suspend/drain/release/resize/rebuild/reset, including outstanding references, zero dimensions, format/mode changes, occlusion and original HRESULT propagation.
- [ ] Wire provider/runtime bookkeeping and one pacing policy. Test device loss and disabled FG while keeping a healthy established proxy. A Present fence must not be assumed to cover an SDK's separate queue/retention.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^(unit.presentation|integration.proxy)$" --no-tests=error --output-on-failure
.\build\win-dev\bin\Debug\RazKolbasPresentationHarness.exe --debug-layers --resize-cycles 100
```

**Expected result:** Proxy presentation works before FG is enabled; resize and lifetime failures are handled honestly.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T16 d3d12-backed presentation and real resize lifecycle` (or an equally specific message). Do not stage unrelated changes or large references.

## T17 — Native-resolution HUD/UI plane and generated-frame composition contract

**Dependencies:** T06, T16, T10.

**Files to create/modify:** `src/skyrim/UiCapture.cpp`, `src/render/UiComposite.cpp`, `src/render/UiContract.cpp`, `shaders/UiComposite.hlsl`, `tests/unit/UiPlaneTests.cpp`, `docs/re/UI_CAPTURE.md`.

**Interface / deliverable:** Produces HUD-free scene plus native-size UI colour/alpha or an explicitly selected provider-specific UI contract; lifetime joins the same retirement set.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Crosshair/subtitles/translucent inventory/dialogue/fades recompose correctly on real frames.
  - Inventory 3D preview retains correct domain and is not treated as an arbitrary transparent 2D layer.
  - Retained UI plane prevents slot reuse until the consuming FG/presentation contract finishes.

- [ ] Map UI rendering and redirects from runtime traces. Separate ordinary HUD/menus from world-rendered first-person objects, fades and special 3D previews.
- [ ] Implement a native-resolution UI layer with correct alpha/blending/colour encoding and state restoration. Prove the native frame recomposes correctly before generating frames.
- [ ] Provide each FG adapter the contract it actually needs: HUD-less scene, mask, UI plane or composition callback. Do not force one vendor's assumptions on every backend.
- [ ] A colour-difference mask may be an explicitly experimental fallback with limitations; it is not proof of true UI alpha. Keep menu/ImGui/ENB/ReShade layers ordered once.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.ui_plane$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case S07
```

**Expected result:** UI composition works without FG and exposes a clear tested interface for intermediate frames.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T17 native-resolution hud/ui plane and generated-frame composition contract` (or an equally specific message). Do not stage unrelated changes or large references.

## T18 — DLSS Frame Generation and coordinated latency

**Dependencies:** T11, T16, T17.

**Files to create/modify:** `src/backends/dlss/DlssFg.cpp`, `src/backends/dlss/Reflex.cpp`, `src/presentation/LatencyPolicy.cpp`, `tests/unit/DlssFgTests.cpp`, `docs/testing/DLSS_FG.md`.

**Interface / deliverable:** Implements FG provider lifecycle, source-frame tags, actual generated-frame limits, latency markers and provider-held resource retirement.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Source frame/jitter/simulation advances once even when output displays multiple frames.
  - HUD correctness is inspected on generated frames, not only screenshots of source frames.
  - Unsupported GPU or capability query rejection leaves SR usable and settings truthful.
  - FG failure/disable retains coherent proxy ownership and does not free resources early.

- [ ] Use the exact runtime's capability query and valid device/presentation path. Query frame-count limits instead of deriving them from model strings.
- [ ] Tag HUD-free scene and required guide/UI resources with correct valid-until lifetime. Implement one extra generated frame first; distinguish extra count from total multiplier.
- [ ] Implement required Reflex/latency markers at their actual game phases, not all at Present. Integrate with the coordinator's pacing; avoid stacked sleeps/limiters.
- [ ] Add higher/dynamic counts only if supported and tested. Handle menu/load/cut/resize/disable transitions and SDK-retained resources beyond the real-frame Present return.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.dlss_fg$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case F02 -Provider DLSS
pwsh -File tools/Run-Validation.ps1 -Case F04 -Provider DLSS
```

**Expected result:** Real DLSS FG, measured source/display cadence and latency/pacing evidence on supported configurations.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T18 dlss frame generation and coordinated latency` (or an equally specific message). Do not stage unrelated changes or large references.

## T19 — FSR Frame Generation as an independent presentation provider

**Dependencies:** T13, T16, T17.

**Files to create/modify:** `src/backends/fsr/FsrFg.cpp`, `src/backends/fsr/FsrPresentation.cpp`, `tests/unit/FsrFgTests.cpp`, `docs/testing/FSR_FG.md`.

**Interface / deliverable:** Implements AMD-specific FG/presentation/UI/retention contracts without creating a competing swap-chain owner.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Unsupported combination is rejected while the requested preference persists.
  - Different guide preparations for FG do not overwrite canonical NR/SR input.
  - Generated UI and variable-frame-time scenes retain readable composition and measured pacing.

- [ ] Read the selected AMD FG API's swap-chain/callback/queue requirements and map them into the presentation owner. Keep its resource and callback lifetimes explicit.
- [ ] Translate canonical frame inputs into the actual AMD guide contract, preserving original SR/NR guides if an FG-specific heuristic is added.
- [ ] Enable one extra frame first with correct UI; integrate its pacing/latency needs into the single policy rather than stacking NVIDIA behavior blindly.
- [ ] Validate FSR SR+FSR FG first, then mixed SR pairings independently. Recreate/disable/resize using full retirement and callback quiescence.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.fsr_fg$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case F02 -Provider FSR
pwsh -File tools/Run-Validation.ps1 -Case F08 -Provider FSR
```

**Expected result:** FSR FG genuinely runs through the unified owner; mixed vendor pairings are advertised only after their own tests.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T19 fsr frame generation as an independent presentation provider` (or an equally specific message). Do not stage unrelated changes or large references.

## T20 — XeSS Frame Generation with its required XeLL path

**Dependencies:** T14, T16, T17.

**Files to create/modify:** `src/backends/xess/XessFg.cpp`, `src/backends/xess/XeLL.cpp`, `tests/unit/XessFgTests.cpp`, `docs/testing/XESS_FG.md`.

**Interface / deliverable:** Implements selected Intel FG/XeLL requirements, capability-specific limits and provider-held resource lifetime.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Missing/wrong XeLL version yields an actionable reason, not enabled-looking FG.
  - Only the selected low-latency policy owns frame sleeps/markers.
  - The queried frame-count limit and actual display count agree within documented drop behavior.

- [ ] Pin a compatible FG/XeLL/header set and query exact platform/driver requirements. Do not apply current guide assumptions blindly to older bundled binaries.
- [ ] Implement the actual UI and presentation-proxy integration plus required low-latency markers. Coordinate XeLL with other active latency systems through one selected policy.
- [ ] Test valid generated-frame count(s) and resize/load/cut/disable handling. Verify callback/thread/resource retention separately from synchronous evaluate completion.
- [ ] Run native and viable cross-vendor configurations separately. Label experimental compatibility profiles and mixed-SR pairings accurately.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.xess_fg$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case F07 -Provider XeSS
pwsh -File tools/Run-Validation.ps1 -Case F04 -Provider XeSS
```

**Expected result:** XeSS FG is a real independently tested provider, not a UI label pointing at another vendor implementation.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T20 xess frame generation with its required xell path` (or an equally specific message). Do not stage unrelated changes or large references.

## T21 — Integrate genuine NR into Skyrim before SR

**Dependencies:** T08 working native contract, T09, T11.

**Files to create/modify:** `src/backends/nr/NrBackend.cpp`, `src/render/NrStage.cpp`, `src/render/NrGuides.cpp`, `tests/unit/NrStageTests.cpp`, `docs/testing/NR_SKYRIM.md`.

**Interface / deliverable:** Consumes the native runtime contract and HUD-free FrameInputs; produces actual NR output at the declared extent for the selected SR stage, with independent history and failure handling.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Loaded-module record proves absence of reference hosts, RenoDX and ReShade for the core test.
  - NR on/off and supported intensity/control changes affect the actual NR output through documented parameter mapping.
  - Evaluate failure preserves original colour for SR and keeps reset pending.
  - Camera cut/resize/NR toggle recreates or resets only the required state safely.

- [ ] Integrate the proven native loader/compatibility profile. Use RazKolbas-owned resources and never call into the reference host DLLs as the shipping backend.
- [ ] Start single-pass, same GPU, before SR. Map resource descriptors, subrects, colour/depth/MV/exposure/reset to the recovered contract and assert actual extent/frame/generation agreement.
- [ ] Preserve original scene colour until successful NR production is known. NR failure leaves the SR path valid; error/status retains the exact failing runtime operation.
- [ ] Test actual settings effects and temporal motion in Skyrim with RenoDX/ReShade completely absent, then ENB and optional ReShade. Maintain NR capability and experimental-profile diagnostics.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.nr_stage$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case N03
pwsh -File tools/Run-Validation.ps1 -Case N04
```

**Expected result:** Genuine NR works in Skyrim without the forbidden dependencies in R4; this task is not finished by detecting a DLL or leaving NR unavailable.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T21 integrate genuine nr into skyrim before sr` (or an equally specific message). Do not stage unrelated changes or large references.

## T22 — Advanced NR placement, artistic controls and resolve

**Dependencies:** T21.

**Files to create/modify:** `src/render/NrAfterSrGuides.cpp`, `src/render/NrResolve.cpp`, `src/backends/nr/NrMultipass.cpp`, `shaders/NrResolve.hlsl`, `tests/unit/NrAdvancedTests.cpp`, `docs/re/NR_CONTROLS.md`.

**Interface / deliverable:** Produces validated after-SR placement, model-setting translations and independently defined host resolve/multipass behavior; consumes canonical original guides, not arbitrary FG-modified data.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Zero transfer/disabled state reproduces the intended valid reference image within declared tolerance.
  - After-SR guides have correct physical extents/metadata; a low-resolution descriptor is never relabelled as full resolution.
  - Near-black ratio resolve remains finite; reduced input scale and white point do not introduce unexplained double gamma.
  - Multipass uses independent/ping-pong outputs and retains resets for every pass that still needs one.

- [ ] Recover and validate needed model settings and optional masks. Keep model tone/structure/style distinct from host transfer/colour resolve; unknown reference fields remain unknown until traced.
- [ ] Implement after-SR guide conversion with declared dimensions, motion scaling and colour/exposure semantics. After-SR failure preserves valid SR output.
- [ ] Implement reduced NR working scale and explicit residual/ratio resolve from independently justified equations and colour-space handling. Test identity and finite behavior at zero/black/extreme values.
- [ ] Implement multipass only with verified per-pass feature/history ownership and ping-pong resources. Expose measured VRAM/time costs and only settings that actually work.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev -R "^unit.nr_advanced$" --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case N05
pwsh -File tools/Run-Validation.ps1 -Case N09
```

**Expected result:** Advanced controls have actual parameter/output evidence. A not-yet-validated advanced option is visibly unavailable and remains an open requirement, not a fake slider.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T22 advanced nr placement, artistic controls and resolve` (or an equally specific message). Do not stage unrelated changes or large references.

## T23 — Full combination testing, hardening and additional Skyrim profiles

**Dependencies:** Relevant T10–T22 feature implementations.

**Files to create/modify:** `tools/Run-Validation.ps1`, `tools/Collect-Diagnostics.ps1`, `docs/testing/RESULTS/`, `docs/testing/SUPPORT_MATRIX.md`, `docs/testing/PERFORMANCE.md`, additional `patches/skyrim/` profiles and regression tests.

**Interface / deliverable:** Consumes the complete stage graph and records source/build/runtime/GPU/driver/module/patch identities for every advertised combination.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - N10 proves one full Skyrim+ENB+ReShade+SR+FG+NR combination; N03 independently proves addon-free NR.
  - Real versus generated-frame accounting and delayed-consumer retirement remain correct under stress.
  - No monotonic growth in tracked resources across repeated lifecycle cycles; unexplained growth triggers investigation.
  - Every matrix entry has PASS/FAIL/NOT RUN/NOT APPLICABLE plus evidence, not guessed compatibility.

- [ ] Run the complete acceptance matrix with actual installed reference/production runtime identities. Test ENB off/on x ReShade off/on, each SR/FG separately, selected mixed pairs and NR combinations.
- [ ] Exercise interiors/exteriors, fast travel/load, first/third person, water/foliage/hair/particles, map/inventory/dialogue/subtitles/fades, camera cuts, focus loss and repeated quality/resolution/provider changes.
- [ ] Inject allocation/provider/submit failures; test delayed retirement, writable-path failures, wrong binaries and patch disable/restore. Run an extended session and repeated transitions looking for leak/deadlock/state corruption.
- [ ] Add SE 1.5.97 and additional AE/GOG profiles only by fresh mapping/tests. Measure source and displayed FPS, CPU/GPU stage costs, pacing and latency under controlled comparable scenes; keep debug-validation runs separate from benchmarks.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
ctest --preset win-dev --no-tests=error --output-on-failure
pwsh -File tools/Run-Validation.ps1 -Case N10
pwsh -File tools/Run-Validation.ps1 -Case P03
pwsh -File tools/Collect-Diagnostics.ps1 -Output artifacts/local/diagnostics
```

**Expected result:** Advertised support is backed by reproducible evidence; absent hardware remains NOT RUN and does not become a universal supported claim.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T23 full combination testing, hardening and additional skyrim profiles` (or an equally specific message). Do not stage unrelated changes or large references.

## T24 — Reproducible release, MO2 package and final implementation handoff

**Dependencies:** T23 for advertised release scope.

**Files to create/modify:** `tools/Stage-MO2.ps1`, `tools/Package.ps1`, `runtime/runtime-manifest.json`, `docs/INSTALLATION.md`, `docs/TROUBLESHOOTING.md`, `THIRD_PARTY_NOTICES.md`, `.github/workflows/release.yml`, `docs/IMPLEMENTATION_STATUS.md`.

**Interface / deliverable:** Produces a source-built RazKolbas.dll, versioned configuration, controlled runtime installation layout, hashes/symbol artifacts, patch/restore documentation and honest support records.

- [ ] **Write the following regression/probe cases and record the initial failure or missing implementation.**

  - Clean test-profile install/uninstall leaves unrelated ENB/ReShade/game files untouched.
  - Missing optional provider components give the documented fallback/reason.
  - Package scanner catches reference host DLLs/large private references and verifies expected file layout/hashes.
  - Native safe mode and any declared file-patch restore work on the exact matching versions.

- [ ] Build Release from pinned inputs and record compiler, configuration, dependency revisions and output hashes. Configure deterministic/reproducible settings where feasible; investigate unexplained output differences rather than promising byte identity by default.
- [ ] Stage the MO2 layout with only required allowed files. List required vendor/runtime dependencies and exact versions separately from the original reference hosts. Keep the reference DLLs, PDBs and raw research files out of the install package.
- [ ] Document clean installation, existing upscaler conflicts, restart-required settings, safe mode, experimental patches, disk backup/restore and uninstall. Do not silently overwrite root ENB/ReShade DLLs.
- [ ] Attach source/build tests, runtime support matrix, known limitations and the remaining concrete tasks. A staged partial milestone must say which requirements are still incomplete.

- [ ] **Execute the targeted checks, then prior regression groups affected by this change.**

```powershell
cmake --preset win-release
cmake --build --preset win-release
ctest --preset win-release --no-tests=error --output-on-failure
pwsh -File tools/Package.ps1 -Preset win-release -Output artifacts/release
```

**Expected result:** A real downloadable mod package and complete source/test handoff, with no claim that untested features/GPUs work. Creating this plan alone does not satisfy this gate.

- [ ] Record actual commands/results and any NOT RUN hardware tests in `docs/IMPLEMENTATION_STATUS.md`. Review the targeted diff and commit as `feat: T24 reproducible release, mo2 package and final implementation handoff` (or an equally specific message). Do not stage unrelated changes or large references.


---

## Concrete invariant fixtures

These are implementation tests to add to the relevant test groups, not facts about an existing binary. The test runner must exercise real policy/coordinator/patch code, not merely parse the expected values.

### T03: reset consumption and user intent

```text
Case reset_arrives_during_evaluation:
  requested_epoch = 7, consumed_epoch = 6
  evaluation captures epoch 7
  a camera cut requests epoch 8
  evaluation of epoch 7 succeeds
  expect consumed_epoch == 7
  expect pending_reset == true
  next successful evaluation captures/consumes epoch 8
  expect pending_reset == false

Case preserve_explicit_provider:
  requested.SR = DLSS
  raw DLSS result = RuntimeMissing
  FSR path = Available / validated
  fallback policy allows FSR
  expect requested.SR == DLSS
  expect effective.SR == FSR
  expect reason includes missing DLSS runtime
  reload with DLSS available
  expect user preference is still DLSS and resolves accordingly
```

### T04: exact-byte fixture

```json
{
  "case": "owned_test_function_only",
  "original_hex": "B8 01 00 00 00 C3",
  "replacement_hex": "B8 02 00 00 00 C3",
  "before_return": 1,
  "after_return": 2,
  "restored_return": 1,
  "negative_cases": ["wrong_hash", "changed_byte", "two_matches", "other_owner_after_install"]
}
```

Allocate and execute this fixture only inside the owned patch harness, under a documented safe protection/CFG policy. Compute the fixture hash in the test; there is no fabricated game DLL hash or game offset in this example. The concurrent test must demonstrate actual execution quiescence or a correct detour transaction, not a mutex shared only by the patching thread.

### T07/T16: retirement is not just D3D12 completion

```json
{
  "required": {"processing": 40, "d3d11_consumer": 12, "present": 18, "provider": 9},
  "completed": {"processing": 40, "d3d11_consumer": 11, "present": 18, "provider": 9},
  "expected_reusable": false,
  "after_d3d11_consumer_reaches_12": true
}
```

The numbers are per-timeline fixture values, not values from one shared unordered multi-producer fence. Independently test provider retention lagging after every API fence completed.

### T09/T21: preserve a usable image

```text
Input 1280x720, display 1920x1080, SR output not produced:
  expect preserved input -> valid spatial fallback -> 1920x1080 presentation
  reject publishing the failed SR output or a bare 1280x720 texture

NR-before-SR fails:
  expect SR consumes preserved original colour

NR-after-SR fails:
  expect presentation consumes the successful SR image

Submission fails before completion signal:
  expect no later queue waits on that unsubmitted signal
```

## Checkpoint and continuation format

For each task keep: state, changed paths, tests added, commands/results, exact tested binary/runtime/GPU identity, remaining unknown, and next executable action. Separate IMPLEMENTED, BUILD TESTED and RUNTIME TESTED; use NOT RUN and PARTIAL explicitly.

When a session ends, use this concrete shape rather than a fresh generic roadmap:

```text
Current task: Txx / named substep
Source commit: actual commit or explicitly uncommitted
Changed files: exact paths
Build/tests: exact commands and exit results
Runtime evidence: actual log/capture or NOT RUN with missing prerequisite
Open contract: specific call/ABI/resource/patch condition
Next action: exact test, breakpoint, source change or command
```

Txx above is a reporting format variable, not an unresolved implementation step. Never mark a provider complete because its fallback works. Never postpone every task until the whole feature list is feasible; proceed along the dependency graph, preserve working code and keep unresolved research actionable.

## Requirement traceability

| Requirement | Primary tasks | Proof |
|---|---|---|
| R1 Skyrim | T02, T05–T07, T09, T23 | Hook profiles, capture/interop, actual game records. |
| R2 ENB | T05, T12, T17, T23 | World/display stage and resize/UI records. |
| R3 ReShade | T10, T15, T23 | Optional absence, placement, state/depth lifetime and combination tests. |
| R4 DLSS/FSR/XeSS/FG/NR | T08, T11, T13–T14, T16–T22 | Real backend outputs, source/generated accounting, no-addon NR, mixed-pipeline record. |
| R5 GPU-aware settings | T03, T07, T11, T13–T14, T18–T21 | Actual LUID, raw/effective capabilities and policy fixtures. |
| R6 INI/ImGui | T03, T10, T23–T24 | Shared schema, transactions, input/serialization/error tests. |
| R7 RE/byte-patching | T01, T04–T05, T08, T12, T23–T24 | Versioned contracts, real fixture patches, observed compatibility corrections and restore path. |

## Plan review result

All R1–R7 requirements have implementing tasks and acceptance evidence. The old method restrictions are superseded, NR is an early workstream, game/SDK unknowns are assigned explicit probes rather than fabricated constants, and each advertised capability remains contingent on actual execution evidence. This is a checked implementation handoff, not a completed implementation.
