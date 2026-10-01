# FG Colour Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the actual effect output at each private presentation stage and prepare a single-effect-owner change for a controlled Skyrim trial.

**Architecture:** Extend the exact ENB/ReShade/Steam offline game-route probe with a patterned midtone source, D3D11/copy readbacks, and the final ReShade D3D12 runtime screenshot. Compare four isolated effect configurations, then use the exact ReShade add-on API to keep D3D11 as the sole effect owner on the private route.

**Tech Stack:** Windows x64, C++20, D3D11/D3D12/DXGI, ReShade 6.8 public events, PowerShell, MSVC/CMake/CTest.

**Spec:** `RazKolbas_Codex_Handoff/docs/SPEC.md` (R2, R3, frame ownership); `docs/re/FG_RESHADE_EFFECT_EXECUTION_2026-10-01.md`.

## Global Constraints

- Preserve the user's installed DLL, INI, preset and Steam/ENB settings until a verified trial package is ready.
- Keep FG Off and private presentation Off in the installed mod during offline work.
- Use the exact hash-gated ENB 0.505, ReShade 6.8 and optional Steam overlay in the reproduction.
- Treat callback counts, GPU pixel output, game image and game FPS as separate evidence.
- Never stage DLLs, PDBs, archives, game data or captures.

## Review Focus

- ReShade effects may compile late: sample only after the per-runtime technique callback is observed.
- D3D11 source and physical D3D12 back buffer may not share an index: record the current lower index before Present.
- The Streamline copy surface and final ReShade D3D12 runtime are distinct swaps/devices on the same adapter; use ReShade's own screenshot API for the final image.
- Resize invalidates buffer identities: reacquire resources in the second epoch.
- A single-stage effect may look different without double processing: compare pixel values across all four preset configurations before choosing ownership.

---

### Task 1: Ordered colour-stage probe

**Files:** Modify `harness/FgPrivateGameRouteProbe.cpp`, `include/rk/FgPrivateSwapRoute.hpp`; create `tools/Test-FgReShadeColour.ps1`; update `docs/re/FG_RESHADE_EFFECT_EXECUTION_2026-10-01.md`.

**Interfaces:** `FgPrivateSwapRoute::inspectLowerForProbe` returns retained lower swap and queue references while the route is alive. The optional probe mode emits source, post-D3D11, and post-D3D12 RGBA samples for one warmed frame in each resize epoch.

- [x] Write a runner that rejects a live ReShade path and fails when the probe lacks structured stage samples; observe the red failure.
- [x] Add a four-quadrant midtone pattern and sampled D3D11 readback at a warmed frame in each epoch.
- [x] Add an ordered D3D12 copy-surface readback, then identify its different final swap/device and capture the actual final ReShade screenshot.
- [x] Run the runner with the copied preset and exact wrappers both without and with Steam, then run Off/Off, D3D11-only, and D3D12-only isolated presets.
- [x] Record raw output under ignored `artifacts/local`, summarize values and uncertainty in root docs, verify source file hashes, run targeted and full Release/Debug checks.

### Task 2: Single-effect-owner correction, if stage output supports it

**Files:** `include/rk/FgReShadeEffectOwner.hpp`, `src/backends/fg/FgReShadeEffectOwner.cpp`, `src/skyrim/RendererBootstrap.cpp`, `harness/FgPrivateGameRouteProbe.cpp`, `tools/Test-FgReShadeColour.ps1`, `docs/IMPLEMENTATION_STATUS.md`, and `docs/re/FG_RESHADE_SINGLE_EFFECT_OWNER_2026-10-01.md`.

**Interfaces:** The production route must keep one user-selected ReShade effect execution stage without altering the user's preset, while native FG-Off fallback and ReShade-absent operation remain valid.

- [x] Identify the stage that owns the desired final colour and the runtime that must be suppressed; record the exact observed evidence.
- [x] Write a failing offline test for one execution per real frame; cover resize, exact-version registration and early failure. ReShade-absent uses native fallback by construction; a separate synthetic absent-ReShade test was not added.
- [x] Implement version-gated, reversible runtime selection and make the focused test pass.
- [x] Re-run the 240-frame exact-wrapper probe with and without Steam plus Release/Debug regressions.
- [x] Prepare and install a rollback-ready 0.1.145 FG-Off package with the private route enabled after offline evidence passed. Stop for a user-started Skyrim save-load and fast-travel check before any FG-On game trial.

If Task 1 cannot attribute output safely, record the missing resource/queue contract and continue targeted RE; do not install a speculative suppression.
