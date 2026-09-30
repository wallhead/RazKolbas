# Private FG-Off swap binding, ReShade parent factory

The V5.4 game path now has an opt-in, restart-only
`Diagnostics.ProbeFgPrivateSwapOff` switch. It leaves
`FrameGeneration.Enabled=false` and sets `slDLSSGSetOptions(Off)`. During the
verified Skyrim 1.6.1170 D3D11 creation callback it prepares a private,
hash-pinned Streamline 2.14.1 D3D12 lower swap on the selected NVIDIA adapter.
Preparation takes the **adapter's exact ReShade 6.8 parent factory**, upgrades
that interface through Streamline, verifies the proxy and lower Present owners,
and tests one real FG-Off lower Present before installing the ReShade factory
trace. The existing verified ReShade and System32 DXGI slot-10 hooks then
substitute a D3D11 facade using the native D3D11 device passed by ReShade.
The selected adapter LUID, window, extent, callback thread, native method
owner and one-shot admission must match. A failed preparation or callback
releases the candidate lower and calls the saved native method. No new binary
patch site was added; the existing pinned factory sites and CAS pointer-slot
ownership apply.

The first standalone probe used a new System32 factory instead of the
ReShade adapter parent. With ReShade loaded first, the Streamline lower
returned `E_ABORT` (`0x80004004`) on its first real Present. A prior private
facade probe intercepted lower Present, so it did not prove this step. Loading
ReShade after lower creation made the System32-factory probe pass. Preloading
Streamline before ReShade did **not** fix the ReShade-first/System32-factory
case. Inspection of the current
[DynamicShaderFrameGen source](https://github.com/jatelop8/DynamicShaderFrameGen/tree/879ab2c13404e02f352fa10515abd0fc25996d4f/src)
showed its D3D12 lower creation uses `adapter->GetParent`, then upgrades that
factory through Streamline. That evidence motivated the parent-factory change;
it is not a claim that its entire presentation design was copied or validated.

The corrected standalone `RazKolbasFgPrivateGameRouteProbe` loads the exact
installed ReShade DLL **before** the private runtime, enumerates its NVIDIA
adapter, prepares the lower, then patches the ReShade and native factory
slots in the same order as the game route. It observed one substitution, a
distinct ReShade upper swap, successful `Present(TEST)`, a real upper Present,
resize from 160×96 to 192×108, and ordered release/shutdown. Exit code was
zero. The route preflight also returned success. Release CTest passed 61/61;
Debug CTest passed 57/57. The admission test first failed to compile because
the new gate was absent, then passed after implementation.
A second regression test found that the trial switch parsed but was initially
classified as a live menu change. It is now restart-scoped, and both CTest
presets passed again after that correction.

The V5.4 MO2 package was staged with the existing SR/NR choices preserved,
the six pinned NVIDIA FG runtime files, and
`Diagnostics.ProbeFgPrivateSwapOff=true`. The 14 manifest files were
hash-checked after installation. The installed FG runtime passed the same
ReShade-first offline route probe. The previous DLL, INI and manifest are
retained in ignored `artifacts/local/backup-v54-before-0130` for rollback.

This is **offline FG-Off evidence**. Skyrim, ENB, the actual V5.4 window,
render thread, SR/NR interaction and FG-On generated frames remain NOT RUN for
this build. The one real preflight Present may briefly publish an empty frame
at startup. The route is guarded by a diagnostic switch and must not be
described as working DLSS-G until a real game submission and generated display
are observed. The next required run is one user-started Skyrim launch with
this package, then inspect `RazKolbas.log` for startup factory selection,
facade substitution, first real Presents and native fallback; visually check
the world, ENB/ReShade effects, UI and resize.

## First game run: private binding rejected, native fallback preserved

The user started the installed 0.1.130 V5.4 package on 2026-09-30 at
07:53 local time (PID 6508). At 07:53:52 the log reported the prepared
2560×1440 private lower and the pending game callback. At 07:53:53,
`installNativeFactoryTrace` rejected the native site with
`Owned route method pointer or prologue differs`; the route was disabled
before replacement and the original ReShade creation succeeded. The
subsequent structural observation recorded the expected System32 DXGI
table and method addresses, with `nativeTraceActive=false`. There was no
facade-substitution message. The code bytes were not included in 0.1.130's
failure log, so neither a particular detour nor its owner is established.

The saved run excerpt reached source frame 35061. At least 34,800 outer
Presents had returned `S_OK` with zero observed failures, and same-frame
DLSS SR/native UI publication was active after world admission. The owner
answered that the game image and UI looked normal. These are native-fallback
results, not FG binding or generated-frame evidence. The run log is retained
under ignored `artifacts/local/live-0130`. A hash-gated, read-only
`OpenProcess` attempt and the later `Stop-Process` attempt both returned
Windows access denied; no remote memory modification was attempted.

## 0.1.131: collect the missing bytes without guessing a hook

The standalone probe had verified the native module, table and method
addresses but omitted the production prologue validation. It now validates
that prologue before its pointer-slot writes. With the installed private
runtime and ReShade loaded first it still exited zero: native code protection
was `PAGE_EXECUTE_READ` (`0x20`), the expected 16-byte prologue matched,
one facade substitution occurred, real Present succeeded and resize reached
192×108. Thus this stricter offline case did **not** reproduce the game
rejection.

The next build logs 64 entry bytes before and after private preparation,
and on rejection also logs the snapshot bytes versus the expected prologue.
It recognizes only entry `E9 rel32`, `FF 25 [RIP+disp32]` and
`MOV RAX, imm64; JMP RAX`, reads at most one target's 64 bytes, and records
the loaded-module owner or allocation metadata. No diagnostic code is
executed, no jump is approved, and existing owner/profile/CAS checks and
native fallback remain unchanged. Bounded reads refuse guard/noaccess pages
and crossing a region boundary. Tests first failed because the reader was
absent, then passed (68 assertions in seven factory-trace cases).

One distinct possibility is a page-protection artifact: the production
whole-image snapshot currently skips `PAGE_EXECUTE` pages, whereas the
bounded `ReadProcessMemory` test successfully read an execute-only fixture
on this system. This is **an unconfirmed hypothesis for the game**, not
grounds to accept arbitrary changed code. Logging both snapshot and direct
bytes will distinguish that case from an actual inline detour. Attribution
or a compatibility patch must wait for the next user-started game evidence.

Final 0.1.131 Release CTest passed 61/61 and Debug 57/57. An independent
review found no significant code issues and ran the two new reader cases
in each build (23 assertions each). The review's initial documentation gap
was addressed by recording the actual 0.1.130 fallback above, separately
from the next build's NOT RUN status. The final stricter route probe also
exited zero. The complete package is staged with existing settings preserved;
the running Skyrim process must exit before installing it. No 0.1.131
Skyrim result or FG-On result is claimed.

The user subsequently closed Skyrim. On 2026-09-30, the 0.1.131 DLL and
manifest were installed into the existing V5.4 MO2 mod; all 14 installed
manifest hashes matched. DLL SHA-256:
`97e4f1180028866113b924b577a1f8084da2d83f0280300bedcc511563c0600d`.
The INI was unchanged from the staging snapshot and was preserved. The old
DLL/INI/manifest are backed up under ignored
`artifacts/local/backup-v54-before-0131`. Skyrim was not running during
installation and was not started by the agent. Runtime evidence for this
build is still NOT RUN.
