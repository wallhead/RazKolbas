# FG Real Boundary Trace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pair a verified world callback with its next real Present, without changing the FG-Off image, so the real source-frame session can be attached only where timing is proven.

**Architecture:** A small platform-neutral tracker consumes world and swap events and rejects menu, test, duplicate, multi-world, foreign-swap, resize-stale and cross-thread candidates. The existing exact-hash world hook and swap observer feed it only under a restart-scoped diagnostic setting. This produces timing evidence; it does not enable Streamline markers or FG.

**Tech Stack:** C++20, CMake, Catch2, Skyrim 1.6.1170 exact-hash hook, existing DXGI observer.

**Spec:** `RazKolbas_Codex_Handoff/docs/SPEC.md` sections 3–5 and `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.

## Global Constraints

- Keep one presentation owner and source-frame identity; generated frames never advance Skyrim simulation.
- Preserve the installed native FG-Off image route and requested settings.
- Do not stage DLLs, PDBs, archives, IDA databases, game data or captures.
- Record build results separately from Skyrim runtime results.

## Review Focus

- A Present(TEST) must not consume a world event; Task 1 test.
- A foreign swap must not consume a world event; Task 1 test.
- A resize must invalidate a pending world event; Task 1 test.
- Two world draws before one Present must not look like one valid source frame; Task 1 test.
- A new real frame may move to another thread, but its world and Present must agree; Task 1 test.

---

### Task 1: Source boundary pairing

**Files:**
- Create: `include/rk/FgRealFrameBoundaries.hpp`
- Create: `src/core/FgRealFrameBoundaries.cpp`
- Create: `tests/unit/FgRealFrameBoundariesTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: monotonically observed world completion, swap ownership, Present flags and thread IDs.
- Produces: `FgRealFrameBoundaries::world(uint64_t)`, `present(uint64_t,bool,bool)`, `reset()`, and `FgBoundarySample` with a disposition, source serial, reset epoch and thread IDs.

- [x] **Step 1: Write failing tests** for one valid world/Present pair; absent world; TEST/foreign bypass; multi-world; thread mismatch and next-frame migration; resize invalidation.
- [x] **Step 2: Build `rk_tests` and verify the missing tracker fails compilation.**
- [x] **Step 3: Implement the minimal tracker** using one mutex so concurrent callbacks cannot split serial and thread identity.
- [x] **Step 4: Run focused `unit.fg_real_boundaries` and verify all cases pass.**
- [x] **Step 5: Commit the tracker and tests.**

### Task 2: Opt-in game event wiring

**Files:**
- Modify: `src/skyrim/WorldDrawHook.cpp`, `include/rk/WorldDrawHook.hpp`, `src/skyrim/RendererBootstrap.cpp`
- Modify: `src/config/SettingsSchema.cpp`, `src/config/SettingsTransaction.cpp`, `config/RazKolbas.ini.example`
- Modify: `tests/unit/ConfigTests.cpp`, `docs/IMPLEMENTATION_STATUS.md`, `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`

**Interfaces:**
- Consumes: Task 1 tracker and the already verified world and swap callbacks.
- Produces: restart-scoped `Diagnostics.ProbeFgFrameBoundaries` and bounded, read-only per-Present classification in the log.

- [x] **Step 1: Write a failing config test** for default Off and restart-only enablement.
- [x] **Step 2: Run the focused config test and verify it fails for the absent key.**
- [x] **Step 3: Wire the tracker** after original world forwarding, before real Present, and on matching-swap resize; reject other swap objects and TEST calls.
- [x] **Step 4: Build Release and Debug, run focused and full CTest; record runtime as NOT RUN.**
- [x] **Step 5: Commit source and status.**
