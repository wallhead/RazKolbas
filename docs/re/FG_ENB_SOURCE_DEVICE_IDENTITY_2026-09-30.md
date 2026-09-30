# ENB source ownership and FG-Off presentation repair

## Observed failure

The user-started 0.1.132 Skyrim run on 2026-09-30 accepted the exact Steam
factory hook and returned the private D3D11 facade through ReShade to ENB.
The user reported a black screen. The first and subsequent outer
`Present(0, 0x200)` calls returned `0x887a0001` (INVALID_CALL). FG was Off;
this was a failure of real-frame presentation, not generated frames.
The saved startup/run evidence is ignored local data under
`artifacts/local/live-0132/`. Skyrim was closed by the user before replacement.

## Controls and actual RE

The standalone route probe now loads the exact modlist ENB 0.505
(`35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3`),
ReShade 6.8 and, optionally, the verified Steam overlay DLL. ENB's exported
`D3D11CreateDeviceAndSwapChain` creates the outer device/swap; the native
factory callback performs the same private lower/facade substitution.
This reproduced the failing path without starting Skyrim.

Capstone inspection of ENB Present at RVA `0x6c540` confirmed that TEST
forwards to its nested swap without the real-frame work. The real path runs
ENB processing, optionally overrides interval when ForceVSync is enabled,
clamps interval to four and forwards the flags unchanged. The modlist's
ForceVSync is false. No ENB byte patch or speculative interval change was made.

Three-buffer, shader-input/render-target usage and tearing controls without
ENB passed real Present. Preparing the private lower before the D3D11 device
also passed. Adding tearing to the request only after private preparation
passed: the actual lower flags were `0x842` and request flags `0x802`.
These controls did not reproduce the failure until ENB was included.

The first error with ENB was inside shared-copy admission, before any D3D12
back-buffer copy or lower real Present:

```text
copy phase: shared-copy-submit
HRESULT: 0x80004005
detail: FG copy source device identity differs
```

The auxiliary texture belonged to the verified native D3D11 swap when
created. After ENB installed its wrapper, that same retained texture's
GetDevice reported a different COM identity. The native immediate context
still passed its identity check. The per-copy source identity query rejected
the valid owned source and poisoned the bridge. Subsequent calls reported
INVALID_CALL or the bridge's poisoned-state DEVICE_REMOVED result, obscuring
the original admission failure. The latter is not evidence that the GPU was
physically removed.

## Implemented ownership correction

`FgSourceLease` retains an immutable exact source resource and the creating
interop's unique ID. Raw source capture still requires matching canonical
D3D11 device identity. The auxiliary overload accepts only the final,
class-owned source, whose private construction verifies native swap GetDevice,
GetBuffer and render views; it checks that object's verified native device
against the interop. It does not accept a caller-supplied ownership boolean,
arbitrary device alias or same-adapter substitute.

The leased copy still verifies context identity, target ownership,
descriptors and fence/device health. Raw per-copy admission remains strict.
Resize prepares and captures the replacement generation before touching the
lower swap, then commits its source lease only after a successful, verified
lower resize. Normal ordinary-texture reference checks include the new retained
lease; caller-held and bound resources still block resize.

The probe also established a second source contract violation: the auxiliary
discard swap discarded the requested SHADER_INPUT usage. Its buffer reported
bind `0x20` and SRV creation returned `0x80070057`. Auxiliary creation now
preserves requested shader-input usage and verifies an actual SRV before
admission. The same probe reports bind `0x28`, SRV `S_OK`; resize preserves it.

## Retirement and diagnostics

The bridge records the first copy failure's phase/result separately from
later failed calls. A shared provider lifetime now remains with the facade,
bridge and any poisoned resource quarantine. Private-route retirement releases
its proxy owners first and calls slShutdown only when no bridge retains the
runtime. An initialized runtime whose safe retirement is uncertain remains
pinned for process lifetime, including its DLL search directory.

The research harness uses ordered cleanup on early failures as well as normal
exit. Its early SRV-failure path initially crashed during automatic teardown;
an intentional failure after successful SRV creation now exits with the
expected code 42, without an access violation. Hooks are restored before
callback/route ownership is retired. The former emergency ExitProcess bypass
is no longer used as a substitute for cleanup verification.

## Verification

| Case | Actual result |
|---|---|
| Original exact ENB chain, before source lease | FAIL: source identity rejection; later invalid Presents |
| Leased source with later GetDevice alias | PASS: real D3D11→D3D12 copy and pixel readback |
| Raw aliased source, empty lease, foreign interop/context, wrong extent | PASS: rejected |
| Delayed consumer queue and destroyed bridge | PASS: timeout, original diagnostic retained, provider retained with quarantine |
| Completed bridge destruction | PASS: provider lease released |
| Exact ENB/ReShade, pristine native factory, no Steam preload | PASS: shader view, 240 real Presents, resize and ordered teardown; exit 0 |
| Exact ENB/ReShade with verified Steam factory chain | PASS: shader view, 240 real Presents, resize and ordered teardown; exit 0 |
| Intentional post-SRV early failure | PASS: expected exit 42, no teardown crash |
| Full Release CTest | PASS: 61/61 |
| Full Debug CTest | PASS: 57/57 |

The independent reviewer found no correctness blockers and independently
passed 63 interop assertions and 918 bridge/facade assertions on the fresh
Release binary. Source lease and provider-lifetime tests were added before
their implementation; the missing APIs initially failed the build. The
actual original ENB reproduction and shader-view failure provide the runtime
red cases, followed by the successful probes above.

**0.1.133 Skyrim image/UI/effect regression: NOT RUN. Steam overlay appearance
and input: NOT RUN. FG-On generated frames: NOT RUN.** These probes exercise
real GPU copy/presentation and exact proxy creation, but do not recreate a
Skyrim scene or prove its ENB effect output or generated-frame operation.

Next gate: one user-started Skyrim launch/save load with the installed 0.1.133
FG-Off trial. Inspect actual Presents and verify the image, native UI and ENB
appearance. Steam overlay is optional for this gate; both offline factory
paths passed. No supplied reference or vendor DLL was modified or added to Git.

## Subsequent 0.1.133 Skyrim result

The user-started run beginning at 11:56:21 on 2026-09-30 (PID 25160)
accepted the pinned Steam/native chain and bound the private facade through
ReShade at 11:56:52, with FG disabled. The saved latest-session log reaches
45,000 real outer Presents with zero failed calls and `S_OK` observations.
DLSS SR evaluated at frame 41892; the native HUD route activated at 41923.
Frame 42600 reports 709 DLSS submissions and no in-flight fallbacks; native
HUD viewport adjustment/restoration records zero conflicts.

The user confirmed normal world image, native UI and ENB appearance, no
black screen, and visible Steam overlay. This passes the FG-Off presentation
regression for this run. Steam overlay input and FG-On generated Skyrim
frames remain NOT RUN. Raw evidence is ignored
`artifacts/local/live-0133/RazKolbas-0133-run.log`, SHA-256
`17fdc60cfaabd5334f8c2a567c5acc9785e74da475d8a6c740f17c3d140936a0`.
No binary or configuration was changed for this observation.
