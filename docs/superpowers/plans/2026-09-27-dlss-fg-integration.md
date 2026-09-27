# DLSS Frame Generation Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce one actual DLSS-generated display frame per real Skyrim frame on supported hardware without breaking SR, NR, ENB, ReShade or native UI.

**Architecture:** Keep the current D3D11 real-frame path and introduce one startup-selected D3D12 lower presentation owner upgraded by Streamline. Transfer same-frame guides into fence-protected D3D12 slots, tag HUD-less/UI inputs and constants to the matching token, and let Streamline generate at one lower Present. All unsupported and failed states present the real frame with FG Off.

**Tech Stack:** C++20, SKSE/CommonLibSSE-NG, D3D11/D3D12/DXGI, pinned NVIDIA Streamline headers/runtime, CMake/CTest, WARP and real-adapter harnesses.

**Spec:** [DLSS FG architecture](../../design/DLSS_FG_ARCHITECTURE.md); [RE25 audit](../../re/AIO_DLSS_FG_RE25_AUDIT.md).

## Global Constraints

- Do not depend on PureDark's `SkyrimUpscaler.dll` or `PDPerfPlugin.dll`; the RE25 packet is evidence only.
- Exact Skyrim 1.6.1170 and ENB/ReShade owner identities remain hash-gated; unknown chains fall back without patching.
- The installed 0.1.106 remains unchanged until a separately verified candidate is ready; never start Skyrim for the owner.
- Use one presentation owner, one SR provider, one FG provider and independent NR. A generated frame does not advance simulation, source ID, camera or jitter.
- Do not stage vendor DLLs, PDBs, archives, game data or captures in git. Pin SDK headers and binary provenance to the selected Streamline version.
- Tag real resource states/extents; do not copy AIO's literal tag states or private 0xB0 packet into our public provider interface.

## Review Focus

1. A second or nested Present passes the real frame twice: test exactly one lower real-frame Present and count generated output from Streamline telemetry.
2. A slot is reused after copy completion while SL still consumes it: test delayed SL completion independently of render/allocator fences.
3. ENB/ReShade or SSE Display Tweaks keeps a stale swap reference through resize: test COM identity, outstanding references and actual resize HRESULT.
4. UI content reaches only a reduced texture or is baked into HUD-less colour: compare generated-frame captures with native HUD/inventory/MagicMenu.
5. A capability/status error leaves the menu saying FG On: test requested/effective state and the Off fallback reason.

---

### Task 1: Frame and retirement policy without a vendor dependency

**Files:** Create `include/rk/FgFrameContract.hpp`, `src/core/FgFrameContract.cpp`, `tests/unit/FgFrameContractTests.cpp`; register `unit.dlss_fg_contract` in `CMakeLists.txt`.

**Interfaces:** `FgSourceFrame` carries source ID, resource generation, presentation token, render/display extents, guide/colour/UI freshness and reset epoch. `FgRetirementSet` records producer, copy/render, SL-input and allocator completion separately. `FgDecision decideFg(const FgSourceFrame&, const FgCapability&, const FgSettings&)` returns Off or one-extra On with a reason; it does not call a vendor API.

- [ ] Write failing cases for stale token/guide, unsupported capability, missing UI role, camera cut/reset, invalid extents, two independent fences and one source producing multiple displayed frames without advancing `FrameIdentity`.
- [ ] Run `ctest --preset win-release -R '^unit.dlss_fg_contract$' --no-tests=error --output-on-failure` and record the expected initial failure.
- [ ] Implement the minimal pure contract, then run that group and the existing frame identity/config tests.
- [ ] Commit the source and exact test result; FG remains effectively Off.

### Task 2: Same-adapter producer slots and complete retirement

**Files:** Create `include/rk/FgSharedInputs.hpp`, `src/interop/FgSharedInputs.cpp`, `tests/integration/FgInteropTests.cpp`, `harness/FgInteropHarness.cpp`; add targets in `CMakeLists.txt`. Reuse the actual-adapter selection already observed by `RendererBootstrap`; leave `NrStage.cpp` behavior intact initially.

**Interfaces:** `FgSharedInputs::prepare(realFrame, d3d11Colour, motion, depth, optionalUi)` returns a slot lease with D3D12 resources, exact descriptors/states, a producer fence and generation. `retire(slot, renderFence, slInputFence)` prevents reuse until all consumers and the command allocator complete.

- [ ] Write failing WARP and real-adapter harness cases for LUID equality, guide copy, odd extents, delayed SL fence, resize-generation rollover, partial copy submission and allocator reuse.
- [ ] Implement D3D11 signal/flush to D3D12 queue wait and owned persistent copy slots; record explicit state transitions, not packet-reported state literals.
- [ ] Run `ctest --preset win-release -R 'fg_interop|nr_command_ring|nr_evaluation_recovery' --no-tests=error --output-on-failure` and the harness with debug layers. A pass proves interop and retirement, not FG output.
- [ ] Commit only sources/tests/docs; do not copy AIO backend objects or binaries.

### Task 3: Early Streamline bootstrap and one lower presentation owner

**Files:** Create `include/rk/StreamlineFgRuntime.hpp`, `src/backends/dlss/StreamlineFgRuntime.cpp`, `include/rk/D3D12PresentationOwner.hpp`, `src/presentation/D3D12PresentationOwner.cpp`, `src/presentation/D3D11SwapFacade.cpp`, `tests/integration/FgPresentationTests.cpp`, `harness/FgPresentationHarness.cpp`; modify the verified creation route in `src/skyrim/RendererBootstrap.cpp` only after the harness proves the wrapper chain.

**Interfaces:** `StreamlineFgRuntime::initialize(adapter, pinnedRuntime)` runs before the companion D3D12 device/lower swap, queries feature support and upgrades the correct lower interface. `D3D12PresentationOwner` exposes a coherent D3D11-facing swap facade to Skyrim/ENB/ReShade and delegates exactly one real Present to an SL-managed D3D12 lower swap. `NativeD3D11` remains a distinct owner/fallback selected at startup.

- [ ] Capture/verify the V5.4 creation and outer/inner swap owner sequence without altering a running game. Write failing COM identity/refcount/GetDevice/GetBuffer/Present/Present1 tests for wrapper lifetime and a failed SL upgrade.
- [ ] Pin official matching Streamline headers and runtime identity; test `slInit`, same-LUID support query, interface upgrade and `presentCommon()` in the standalone harness. Do not infer support from the RTX model or NR status.
- [ ] Implement the pass-through D3D11 facade and D3D12 lower swap without FG On. Test 100 resize/minimize/restore cycles with real errors, outstanding references, alt-mode changes and release ordering.
- [ ] Run focused presentation tests and existing `integration.native_flip`, `integration.owned_swap_buffer`, `integration.factory_create_trace`; commit the offline owner only when no double Present or fake resize success remains.

### Task 4: Native HUD-less and UI inputs

**Files:** Create `include/rk/FgUiPlanes.hpp`, `src/render/FgUiPlanes.cpp`, `tests/integration/FgUiPlanesTests.cpp`; integrate only the established boundaries in `src/render/NativeUiRedirector.cpp` and `src/skyrim/WorldDrawHook.cpp` after current MagicMenu replay is confirmed.

**Interfaces:** `FgUiPlanes::capture(realFrame, displayColourBeforeUi, uiColourAlpha, finalColour)` returns display-sized, separately owned HUD-less/UI inputs with actual valid regions and retirement leases. The tag writer receives only complete, same-frame planes.

- [ ] Write failing pixel tests where reduced UI is present, native UI is absent, alpha is empty, or an old plane survives resize. A missing plane disables FG for that frame while preserving the final real image.
- [ ] Capture the exact ENB/ReShade post-effect and native UI ordering; place HUD-less capture before UI and preserve the final real frame once.
- [ ] Test stencil/scissor, cursor and menu composition in the harness; keep InventoryMenu and MagicMenu runtime checks for the user's later game run.
- [ ] Commit only after independent images show the planned role of each plane.

### Task 5: DLSS-G submission, markers and truthful state

**Files:** Create `include/rk/DlssFg.hpp`, `src/backends/dlss/DlssFg.cpp`, `src/backends/dlss/Reflex.cpp`, `tests/unit/DlssFgTests.cpp`, `tests/integration/DlssFgPresentTests.cpp`; connect at the selected lower Present boundary in `src/presentation/D3D12PresentationOwner.cpp` and extend the End-menu requested/effective fields in `src/ui/DiagnosticsMenu.cpp`.

**Interfaces:** `DlssFg::prepare(FgSourceFrame, FgSharedInputs::Lease, FgUiPlanes::Lease)` validates one token and submits exact constants/tags/options; `beforePresent` selects Off/one-extra On using queried capability/status; `afterPresent` records actual-presented count and SL input-completion fence. `setOffAndDrain()` is mandatory before resize/teardown.

- [ ] Write failing mock-SL tests for one real Present, normalized MV scale, matching token/constants/markers, requested/effective/actual frame counts, nonzero status, no stale prepared slot, Off-before-resize and delayed SL completion.
- [ ] Implement one-extra On first. Use the selected SDK's frame-based tagging API where available; do not mix incompatible tag APIs or reuse AIO's private packet ABI.
- [ ] Put Simulation/Render/Present markers at verified phases; if the simulation phase is unproven, leave FG disabled with that reason rather than fabricating latency markers.
- [ ] Run focused tests, the proxy/interop suites and 43 existing CTest groups. Commit only an offline-validated candidate; actual generation remains NOT RUN until a user-started Skyrim test.

### Task 6: Controlled Skyrim validation and capability rollout

**Files:** Add `docs/testing/DLSS_FG.md`; update `docs/IMPLEMENTATION_STATUS.md`, capability menu and install package only after the previous tasks' harnesses pass.

- [ ] With the owner starting Skyrim, capture supported status, token/real-frame IDs, requested extra count, actual-presented count, present cadence and images from both real and generated frames.
- [ ] Check world pan, HUD, inventory, MagicMenu, loading/cut, ENB/ReShade, Alt-Tab and resize; prove SR/NR continue when FG goes Off.
- [ ] If one extra frame is actually present with correct UI and retirement, mark that exact GPU/runtime/modlist profile validated. Only then consider dynamic/higher counts and display the queried maximum.
- [ ] Record failures as actual outcomes, retain the stable Off path, and stage no third-party DLL without verified provenance/packaging permission.
