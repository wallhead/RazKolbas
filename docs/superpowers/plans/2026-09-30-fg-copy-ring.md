# FG lower-swap copy queue plan

**Goal:** Remove per-frame CPU waits and command-object creation from the private FG presentation copy while preserving exact colour, ordered Present and safe resize/retirement.

**Scope:** The private route proves that it creates the lower swap on the same D3D12 direct queue used by the D3D11 interop bridge. Other bridge callers keep the synchronous path until they provide that proof. FG remains Off. A byte-reversible private-route On trial is installed after offline checks so the new path can be tested in Skyrim; the original private-route Off INI is backed up.

**Evidence:** `FgSharedInputs::copy` already enqueues a D3D12 wait on the D3D11 producer fence before its completion signal. Submitting the backbuffer copy on that same queue orders it after the producer without a CPU wait. The lower swap created by `FgPrivateSwapRoute` uses this queue. TRP uses retained copy command slots and waits when reusing them; its implementation is a reference, not donor code.

## Task 1 — queue-owned async contract

- [x] Write a WARP test that blocks the known lower queue, calls `copyToCurrent`, and expects a prepared result without waiting for that queue; current code failed compilation before the new option existed.
- [x] Add an explicit queue-ownership option to facade/bridge creation and pass it only from the verified private route and controlled WARP harness.
- [x] Keep the existing synchronous fallback for callers without queue-ownership proof.

## Task 2 — bounded command slots

- [x] Retain one reusable allocator/list per physical lower buffer after first use and a monotonic D3D12 completion fence. Lazy first-use allocation replaces the planned eager allocation; it avoids command-object allocation on subsequent frames without creating unused objects.
- [x] Before a slot is reused, wait for its prior fence value. Submit D3D11 shared copy, D3D12 copy and completion signal in queue order; return without a per-frame CPU wait.
- [x] Preserve resources on uncertain completion, drain slots before resize/teardown, and propagate real failure HRESULTs. Never pretend a failed resize succeeded.
- [x] Add rotating-colour, blocked-queue, resize, failed/uncertain retirement tests, then run targeted and full Release/Debug suites.

## Task 3 — evidence and delivery

- [x] Run the pinned standalone private-route/ENB/ReShade/Steam FG-Off and facade-on probes. Record actual output and limits.
- [x] Stage the unchanged-settings private-route Off candidate and verify all 14 payloads. Then stage and install a one-key, FG-Off private-route On trial after offline checks, with a backup and 14/14 installed verification. The change from the original Off-only plan is necessary to exercise the new path in Skyrim; it may reproduce the earlier colour/FPS issue, so runtime claims remain withheld.
- [x] Record the user-started FG-Off trial separately from offline probes. Yellow colour and 50-versus-60 FPS regressed; the private route was restored Off after Skyrim closed. Game FG-On remains NOT RUN. Source and documentation are committed/pushed without binaries or captures; this result requires the follow-up evidence commit.
