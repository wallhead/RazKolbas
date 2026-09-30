# FG Render Phase Trace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Verify an early renderer call and the exact world-call interval on the same source frame already paired with Present.

**Architecture:** Extend the existing read-only boundary tracker with renderer-entry and world-entry events, preserving its real Present ownership and reset policy. The verified Renderer Begin `GetClientRect` relay records the early event; the world forwarder calls a new observer immediately before the original world function and keeps its existing after observer. A source frame is phase-ready only when all four events occur once, in order, on one thread.

**Tech Stack:** C++20, existing exact-hash Skyrim 1.6.1170 hooks, Catch2, CMake.

**Spec:** `RazKolbas_Codex_Handoff/docs/SPEC.md` sections 3–5; `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.

## Global Constraints

- No Streamline marker, guide tag, generated frame or private swap activation in this trace.
- Preserve original game calls and the stable FG-Off visual route.
- Preserve requested settings and never stage binaries or runtime captures.
- Mark offline tests and user-started runtime tests separately.

## Review Focus

- TEST and foreign swap events leave every phase pending; Task 1 test.
- Missing or duplicate renderer entries reject phase readiness; Task 1 test.
- Renderer entry after world entry rejects phase readiness; Task 1 test.
- A world-entry callback runs before the original and the existing observer remains after; Task 2 test.
- Thread migration is allowed only between completed real frames; Task 1 test.

---

### Task 1: Four-event phase ledger

**Files:** `include/rk/FgRealFrameBoundaries.hpp`, `src/core/FgRealFrameBoundaries.cpp`, `tests/unit/FgRealFrameBoundariesTests.cpp`.

**Interfaces:** Add `rendererBegin(uint64_t)` and `worldBegin(uint64_t)`; extend `FgBoundarySample` with phase counts, thread identity and `phaseReady`.

- [x] Write tests for ordered, missing, duplicate, late and cross-thread entries.
- [x] Run focused test and observe the missing interface fail.
- [x] Implement minimum state and consume it only on the matching real Present or reset.
- [x] Run focused test green and include it in the phase-trace change.

### Task 2: Exact game callback wiring

**Files:** `include/rk/WorldDraw.hpp`, `src/skyrim/WorldDraw.cpp`, `tests/unit/WorldDrawTests.cpp`, `include/rk/WorldDrawHook.hpp`, `src/skyrim/WorldDrawHook.cpp`, `src/skyrim/RendererBootstrap.cpp`, `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`, `docs/IMPLEMENTATION_STATUS.md`.

**Interfaces:** Extend `WorldDrawForwarder::configure` with optional pre-original observer; expose `recordFgRendererBegin()` and pass all events to Task 1 under the existing diagnostic setting.

- [x] Write a failing original-order test.
- [x] Add the observer and game wiring without changing the original call's arguments or result.
- [x] Build Release and Debug; run focused and full CTest.
- [x] Stage and install the next FG-Off trace only after Skyrim is closed; verify all payload hashes.
- [x] Record runtime as NOT RUN and prepare the change for commit/push.
