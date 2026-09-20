# Direct NVIDIA NGX reuse — September 20, 2026

The supplied NVIDIA SR runtime has executed a captured Skyrim frame through
RazKolbas-owned D3D11 code. This is offline reset-only DLAA, not live Skyrim,
reduced-resolution SR, or a temporal-quality test.

`PDPerfPlugin.dll` and `SkyrimUpscaler.dll` are reverse-engineering references
only. The production RazKolbas plugin must not load, link, copy, or require
either DLL. Its owned SR backend will prepare the game resources and call the
NVIDIA NGX interface directly, following the contract verified in the
standalone replay. The installed MO2 package contains `RazKolbas.dll`, its
INI, and a manifest; the plugin import table has no PDPerf or reference-host
dependency. The signed NVIDIA SR runtime is distinct from the community-
patched unsigned NR runtime and its compatibility workaround.

## Recovered interface

`tools/re/trace_sr.py` accepts only SkyrimUpscaler SHA256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`
and PDPerfPlugin `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
It disassembles exception-directory function regions; leaf functions without
those records and chained regions require separate inspection. Output is static
evidence, not proof that every indirect call is resolved.

Skyrim SR call RVA `0x1f5022` passes a `0xb0`-byte payload to
`PDPerfPlugin!EvaluateUpscaler` (`0x4f670`), which copies it and dispatches via
virtual slot `0x28`. The DLSS routine `0x4a900`, continued at `0x4a988`, converts
payload fields to the ordinary NGX D3D11 structure. Helper `0x48dc0` writes named
NGX parameters and tail-calls `NVSDK_NGX_D3D11_EvaluateFeature_C`.

| Host payload offset | DLSS conversion |
|---|---|
| `0x08` | Color |
| `0x10` | MotionVectors |
| `0x18` | Depth |
| `0x20` | Bias current color mask |
| `0x28`, `0x30` | Output; nonnull `0x30` takes precedence |
| `0x38`, `0x3c` | Float dimensions converted to integer render subrect width/height |
| `0x44`, `0x48` | Jitter.Offset.X/Y |
| `0x4c`, `0x50` | MV.Scale.X/Y |
| `0x54` | Reset byte converted to integer |
| `0x68` | D3D11 context |

Creation routine `0x49dc0` sets dimensions, quality, feature flags and subrect
enablement, then calls ordinary SuperSampling feature `1` at `0x4a0a8`. These
contracts match the official NVIDIA D3D11 SDK helpers. They are separate from
the private NR `0x138`-byte payload and feature `0x12`. Upstream resource
transformations, virtual-dispatch selection and our live hook/jitter source
remain to be established.

## Implementation and result

`harness/SrReplay.cpp` uses official SDK commit
`374959484e79a640feaba44c93ac8cfb0a03f5b5` with its own project ID. It requires no
PureDark/Fallout host. It locks and verifies the supplied `nvngx_dlss.dll` SHA256
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`, then checks the
module actually loaded by NGX has that same path/hash. This ordinary SR runtime
needs no signature workaround; community-patched NR remains a separate path.

Input: stationary capture `18080-136199593`, 2560x1440 RGBA16_FLOAT colour,
RG16_FLOAT motion and R24G8_TYPELESS depth. Only low24 depth bits are normalized
to R32_FLOAT. Experiment flags: HDR, MVLowRes, AutoExposure; reset=true,
jitter=(0,0), MV scale=(2560,1440), exposure=1. These are explicit experiment
settings, not verified live guide conventions. Captures do not contain jitter.

On RTX 4080 SUPER, init/capability/create/evaluate/release/destroy/shutdown all
returned `0x1`. A D3D11 event completed before mapped readback. NaN-initialized
output was overwritten with 11,059,200 finite RGB components; alpha also finite.
RGB range: 0.000307798–11.90625; 78.7469% of components differ from input. That
difference is not an image-quality measurement. Output SHA256:
`facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c`.
The independent validator reproduced this hash. Viewed preview and measurements:
`artifacts/local/sr-replay-validated/`. Both previews clip RGB to 0..1 and use
display gamma 2.2.

## Reproduce

```powershell
python tools/re/fetch_sr_sdk.py --output artifacts/local/ngx-sdk
cmake --preset win-release -DRK_NGX_SDK_ROOT="$PWD/artifacts/local/ngx-sdk"
cmake --build --preset win-release --target RazKolbasSrReplay
python tools/re/run_sr_replay.py --exe build/win-release/bin/Release/RazKolbasSrReplay.exe --runtime-directory SkyrimUpscalerAIOBuild16-Hotfix1/UpscalerBasePlugin --capture '<capture directory>' --output '<new output directory>'
```

Fetcher verifies Git blob hashes at the pinned commit and records SHA256.
CMake verifies Release/Debug library identities and selects the matching CRT.
Both configurations built successfully. The final source replay reproduced the
same output hash; all15 existing Release CTest groups passed. An injected
`RAZKOLBAS_SR_REPLAY_FAULT=throw_after_evaluate` exited10 with
`EXCEPTION_WITH_GPU_OWNERS` and no PASS, confirming the fatal-unwind path.
The runner's stale-output rejection was also exercised before child launch.
Runner checks input hashes, formats, row extents and finite values; refuses stale
outputs; invokes the isolated child with a 90-second timeout. GPU waits are
bounded to 20 seconds. Unexpected exceptions terminate before persistent GPU
owners unwind. SDK binaries, reference DLLs and captures stay out of git/MO2.

Next: use this NGX call path in the owned backend after recovering pre-SR
resource preparation and jitter/hook timing. No repeated stationary/pan test is
needed for that step. Installed 0.1.8 remains diagnostic; live DLSS is inactive.

## Reference hook map, next investigation

Static inspection of the exact supplied `SkyrimUpscaler.dll` install routine
`0x157b60` found two renderer Begin jitter modifications based on AE relocation
ID 77245 at addends `0xe5` and `0x133`. It also identified a call-site hook
labelled `Main_DrawWorld_MainDraw`, based on AE ID 82084 at addend `0x17a`.
`tools/re/map_skyrim_sr_hooks.py` decoded all 428,461 records from the exact
hash-verified 1.6.1170 Address Library and mapped these to game RVAs
`0xe44675`, `0xe446c3`, and `0xfa507a`, respectively. The latter is a stronger
reference for the world-draw stage than ENB Present, but its exact place
relative to UI and the SR input preparation remains unverified. The reference
also uses AE IDs 77518/77520 during jitter/camera hook setup; their final
patched sites require further instruction/data-flow tracing.

RazKolbas 0.1.8 adds one read-only live-byte log for each mapped site after
its exact game profile check. It writes no instructions there. This checks
whether the running modlist has already changed those sites before a new
hook descriptor is considered.

The MO2 run on September 20 loaded 0.1.8 with ENB and ReShade and logged
`e8d1f7e9ff` at game RVA `0xfa507a`. Its signed rel32 resolves to game RVA
`0xe44850`, exactly Address Library AE ID 77247. The two renderer Begin
reference sites logged `0100488b0d32414402488b01ff907003` at `0xe44675` and
`488b01448d42f5ff90800000004b8d04` at `0xe446c3`. This proves the live
CALL has an in-game target in this modlist at startup; it does not yet prove
exclusive ownership or safe patching. Skyrim's on-disk `.text` bytes differ
from these decoded live instructions, so an on-disk byte comparison is not a
valid hook signature.

Continuing through the reference DLL's chained `.pdata` regions clarified the
stage: the callback at `0x156830` forwards the original world-draw CALL at
`0x156922`, resolves HDR/UI resources via `ResolveCSHDRTextures` at `0x156942`
(`0x1f6700`), and, on its normal upscaling branch, invokes `0x1f4ca0` at
`0x156dab`. That routine calls `PDPerfPlugin!EvaluateUpscaler` at `0x1f5022`.
Thus this reference hook is an SR evaluation path after the original engine
call, not just a Present observer. The exact game resource identities,
guide conventions, branch conditions, and coexistence contract for our own
replacement still need verification before altering this call site.
Inside `0x1f4ca0`, three interface calls at `0x1f4d16`, `0x1f4d34` and
`0x1f4d52` copy prepared resources before the NGX payload is built. A fourth
call at `0x1f5092` copies a resource after evaluation. This brackets the
vendor call with explicit resource transfer; merely invoking NGX at Present
would omit part of the reference's image path. These interface methods and
resource roles are still under investigation.

The first MO2 run logged a `BSWin32KeyboardDevice::Process` access violation at
00:45:23. The crash log has the same faulting instruction, call stack and
invalid-pointer pattern as the September 19 18:55:50 crash, which preceded
the 0.1.8 site logger. No RazKolbas frame appears in the faulting stack.
The user launched a second run at 00:47:26; its three site bytes were identical
and it continued past 9,000 observed presentations without a RazKolbas error.
Cause of the first-run crash is unresolved; the read-only site log and crash
are separate evidence.

## 0.1.9 owned call-site preparation

`src/patch/CallSite.cpp` now turns that observed world-draw `CALL` into a
versioned, read-only plan. It requires the independently verified game-file
hash, exact image size and all five live instruction bytes, then decodes the
signed relative operand and requires target RVA `0xe44850`. Any changed
owner/bytes, target, identity or out-of-image location rejects the plan.
The startup observer logs success or the rejection reason; it never rewrites
this engine instruction. Unit regressions use the real 1.6.1170 bytes and
failure cases, and both Debug and Release passed all 15 CTest groups. The
next engineering step is a quiescent, lifetime-safe pass-through call-site
detour with verified forwarding ABI, followed by attaching owned resource
preparation and direct NGX evaluation. Neither pass-through detour nor
in-game SR is claimed by this checkpoint.

## Offline CALL forwarding fixture

The prepared `CALL rel32` replacement now has a checked encoder. It verifies
the recorded original target again and rejects address overflow and a detour
outside the signed 32-bit displacement range. An owned executable fixture
changes only its five-byte call instruction, forwards through a counting
detour to the original callee, observes exactly one visit per call, and
restores the original route. Both Debug and Release passed all 15 CTest groups.
This proves the instruction encoding and controlled fixture behavior; it does
not establish safe quiescence for Skyrim threads, the real callee ABI, a
nearby thunk allocation, or an in-game hook. A separate 14-byte RIP-indirect
absolute jump relay ran in an executable fixture and preserved two Win64
integer arguments and the caller's return path; no relay is installed in
Skyrim. The game was not started for this change at the user's request.

## Near relay and inferred world-call ABI, offline

`prepareNearCallRelay` now reserves a page inside signed rel32 reach of a
validated CALL, emits the 14-byte RIP-indirect absolute jump, changes its page
from read/write to execute/read, flushes and reads it back, and returns the
matching five-byte CALL. The relay is owned until callers are quiescent and
the CALL has been restored; it is not installed in Skyrim. An executable
fixture exercised allocation, two integer arguments, output, restore and
retirement in both build configurations. Its first form crashed because the
synthetic caller omitted Win64 stack alignment and shadow space; correcting
the fixture resolved the crash. The branch displacement had already matched
the allocated relay address.

The exact-hash `SkyrimUpscaler.dll` callback at RVA `0x156830` saves incoming
`RCX` to `RSI` and `EDX` to `EDI`, then at `0x15691d` restores those two values
and calls the stored original game target at `0x156922`. It does not preserve
incoming `R8` or `R9` across intervening calls. The 0.1.8 live game bytes
immediately after the world CALL start `48 8b 05`, overwriting `RAX`; the
reference continues with further calls without preserving that return.
These observations support a two-argument Win64 forwarding shape with an
unused integer return at this call site. The meaning of each argument and
the target's internal behavior remain unverified. `WorldDrawForwarder` now
models that shape as pointer plus 32-bit value, calls the original exactly
once before an optional observer, and fixes the owner before activation.
This is an ABI inference from the reference and live bytes, not a validated
in-game pass-through hook. Static disassembly of the exact Skyrim executable
cannot close the gap: its on-disk `.text` bytes at these RVAs are encoded and
differ from the decoded live bytes. No game process was started for this work.

## 0.1.10 read-only live-code snapshot

The next installed diagnostic build records 0x200 decoded live bytes from
AE ID 82084 base RVA `0xfa4f00` and 0x100 bytes from original target RVA
`0xe44850`, only after the exact 1.6.1170 identity and five-byte CALL have
passed validation. These two bounded log entries permit offline disassembly
of the real caller and target, which the encoded on-disk executable cannot
provide. It does not invoke, modify, or redirect either function. Debug and
Release passed all 15 CTest groups. The user launched Skyrim through MO2 on
2026-09-20; the 0.1.10 log at 09:16:38 verified the exact world CALL and
captured both decoded live-code ranges. Renderer creation through the ENB
wrapper succeeded at 09:16:54 on an RTX 4080 SUPER, and menu Present
observation continued without failures. No game patch or SR feature was
activated. The assistant did not start or control the game.

The 512-byte caller snapshot has SHA-256
`d0fd5a13877dbf1b79ca1822c6bf5119b87e1127c72d97eb146e52172c08a3f2`;
the 256-byte original-target snapshot has SHA-256
`24082619f4ccf5d4a7ddf280ce35718e891c73a57a61e20210db9cc9e22ec54d`.
Raw bytes and offline Capstone disassembly remain ignored under
`artifacts/local/sr-live-code-2026-09-20-091638/`. The caller at RVA
`0xfa5071` zeros EDX, at `0xfa5073` loads RCX with the address at RVA
`0x32887c0`, and at `0xfa507a` invokes the verified original target.
The next instruction at `0xfa507f` overwrites RAX. The original target at
`0xe44866` saves RCX in RBP and at `0xe4486d` copies DL into R15D with
zero extension. Thus this exact call passes a game-owned pointer and a
zero-valued low-byte flag, with no consumed return value. The pointer's
semantic type and the target's complete effects are still unknown. This
live evidence strengthens the two-argument forwarding ABI inferred from
the reference callback; it does not verify the future in-game detour.

## 0.1.11 experimental world-call pass-through

The next build uses that decoded evidence as an activation gate at
`SKSEPlugin_Load`, before observed renderer creation. It requires the exact
Skyrim executable hash, exact CALL, matching caller argument instructions,
matching original-target prologue, explicit experimental patch opt-in, and an
unchanged executable write region. A nearby execute/read relay preserves the
two incoming arguments and calls the pinned RazKolbas callback. The callback
invokes Skyrim's original target exactly once before incrementing a counter;
the normal Present observer reports `worldForwarded`. It does no DLSS work and
does not change render targets. An executable fixture passed install,
forwarding, idempotence, changed-owner refusal, exact restoration, and relay
retirement. Debug and Release passed all 15 CTest groups. The in-game
pass-through outcome remains **NOT RUN** until the user launches the newly
installed build. The callback DLL, relay and forwarding state intentionally
stay alive for the process lifetime; disabling this experimental patch takes
effect on the next launch.

The user-run 0.1.11 launch at 09:36:08 on 2026-09-20 verified the decoded
ABI and installed the exact CALL relay at 09:36:16. Original renderer
creation through ENB succeeded at 09:36:33. Present observations #1, #2,
#3, #600 and #1200 each reported an equal `worldForwarded` count; the log
continued past 31,800 successful Presents with zero reported failures.
This is a **PASS for the game pass-through path at the main menu**. It does
not establish loaded-world resource timing, input guide semantics, or DLSS
output. The assistant did not control the game. A separate bounded read-only
process snapshot confirmed the renderer object is at the same RVA
`0x32887c0` passed as RCX at the CALL; that RVA independently maps to
Address Library ID 411393. The snapshot is ignored under
`artifacts/local/world-pass-through-live-2026-09-20-0938/`.

## 0.1.12 sparse world-boundary resource telemetry

The pass-through now takes bounded numeric snapshots of the verified renderer
object immediately before and after the original target on the first three
calls and every 600th call. It records the incoming flags, callback thread,
renderer pointer match, lock owner/recursion, creation device/context/swap
pointers, and the candidate colour/motion/depth pointers. It never follows
those candidate pointers, acquires the renderer lock, calls a D3D method,
copies GPU data, or changes rendering. This is needed to determine whether
the original target leaves the candidate resources and lock in a state where
owned SR preparation can safely run. Debug and Release passed all 15 CTest
groups; in-game telemetry is **NOT RUN** until the next user launch.

The user-run 0.1.12 launch at 09:46:25 reached the world-stage callback at
09:46:56. Across 29 logged samples through world call #15,600, every
before/after renderer read succeeded, RCX matched the renderer object, the
current callback thread owned its lock with recursion 1 on both sides, and
the colour/motion/depth candidate pointers remained nonzero and unchanged
around the original target. The device/context/swap fields matched the
observed renderer creation path. The corresponding Present counts equalled
`worldForwarded` throughout, with `failed=0`. At sampled frames the callback
thread alternated between IDs 6588 and 4764, and ownership followed the
current thread. This establishes a guarded post-world access boundary in this
run; it does not prove the three buffers' exact image semantics, jitter,
depth conversion, or temporal quality. The user reported loading a save;
the log itself does not identify the scene or save. No hotkey was needed.

## Native depth and owned source-copy preparation

An isolated run of the supplied NVIDIA DLSS runtime used the captured Skyrim
R24G8 typeless depth bytes directly, with the native shader-resource/depth
bind flags. NGX init, DLAA creation, evaluation and teardown all returned
success. The independent validator found finite, nonuniform output and SHA-256
`facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c`,
identical to the earlier normalized-R32 experiment. This proves that this
runtime accepts that typeless texture for this reset-frame replay; the equal
hash does **not** establish correct depth interpretation or temporal quality.
The reproducible mode is `RAZKOLBAS_SR_REPLAY_DEPTH_MODE=typeless` with
`tools/re/run_sr_replay.py`. Its ignored validation is under
`artifacts/local/sr-depth-typeless-validated-2026-09-20/`.

0.1.13 adds an owned D3D11 input path. Under the proven post-world renderer
lock, it checks the actual creation device/context/swap and each candidate's
device identity, exact format, dimensions and resource shape before allocating
separate colour, motion, native typeless depth and output textures. It queues
three `CopyResource` operations on the same immediate context, then an event
query, and retains the copies until the GPU reports completion. Source
textures and game render state are not changed; NGX is not invoked in Skyrim
yet. WARP independently checked byte-identical copies of all three formats,
including typeless depth, and rejected missing, swapped and foreign-device
sources. Debug and Release passed all 16 CTest groups. In-game copy completion
is **NOT RUN** until the next user launch.

The user-run 0.1.13 launch at 10:01:54 on 2026-09-20 loaded the installed
DLL with its expected hash. At 10:02:24 the guarded callback queued the three
owned 2560x1440 copies on the game's RTX 4080 SUPER D3D11 context; the next
callback observed the D3D11 event query complete and a healthy device. The
original-first world forwarding and Present counters matched through #18,000,
with `failed=0`; no warning or error appeared in this launch's 98 log lines.
The process remained responsive. This is a **PASS for in-game GPU copy
completion**. It does not prove copied pixel equivalence in this game run,
in-game NGX evaluation, image quality, or displayed SR. The assistant did not
launch or control Skyrim.

## 0.1.14 guarded live-device DLAA probe

The next build reuses the exact-hash NVIDIA SR runtime and the already completed
owned colour/motion/native-typeless-depth copies. Under the verified renderer
lock, it attempts one reset-only DLAA evaluation on Skyrim's actual D3D11
device. An isolated D3D11.1 context state protects the game's graphics
bindings. Output goes only to a separate texture, then staging readback after
an event-query fence. The readback must have finite, nonuniform RGB and gets a
SHA-256. NGX release, parameter destruction and shutdown follow successful
fence/readback; uncertain failures keep resources until process exit. No
backbuffer, game render target, ENB output, or ReShade input is replaced.

The module requires the supplied signed `nvngx_dlss.dll` to be installed beside
the plugin under `RazKolbasRuntime/` and checks its exact size and SHA-256
before init, then checks the loaded module's hash. It does not use the unsigned
community `_dlssnr.dll` or either reference host. The experiment uses the same
explicit reset/jitter/MV/exposure assumptions as the offline replay; those
values remain unverified for a temporal game pipeline. WARP verified full
render-target and viewport restoration across state switching; Debug and
Release passed all 17 CTest groups. **In-game NGX evaluation is NOT RUN** until
the next user launch. This diagnostic result cannot establish displayed SR or
visual quality.
