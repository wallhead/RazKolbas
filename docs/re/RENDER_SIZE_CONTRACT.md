# Render-size and DLSS SR contract — 2026-09-20

## Implemented, source-only checkpoint

`PreparedSrInputs` now owns separate render and display extents. Its three
colour/motion/depth copies retain the source render dimensions; its output is
allocated at the requested display dimensions. Both HDR and SDR paths reject
zero, undersized or greater-than-8192 display extents before copying source
resources. The existing one-to-one DLAA API is unchanged. `OffscreenDlssProbe`
now creates an NGX Quality SR feature when extents differ, submits the render
subrect dimensions, reads back the full display-sized output and retires the
feature. This code is **not active in the installed MO2 game plugin**.

The Release build and all 22 CTest groups passed. The isolated
`RazKolbasSrLivePathReplay --sdr-sr` used the 0.1.22 user capture's
2560x1440 post-world SDR frame, sampled it to synthetic 1280x720 colour, and
created same-sized zero motion and varying typeless depth. It exercised the
production owned-copy/NGX probe code on the selected NVIDIA adapter. NGX
submitted, readback passed the probe's finite/nonuniform check, clean
retirement completed, and the 2560x1440 RGBA8 output SHA-256 was
`fecd28b1b422cb55ea05a500f12ec1fc7f7f2736d45727e9b6f4f3d5bea36101`.
The readback contained 14,745,600 bytes; 480 of 880 sampled RGB positions
differed from simple nearest-neighbor enlargement of the source frame.
The existing full-size SDR DLAA replay also passed, output SHA-256
`4269af78e1136d6a2c04590e5249a5064f636b733d1326ee4dc44518cfba89e3`.
These are compatibility results on synthetic guides, not image-quality or
actual reduced-workload Skyrim results.

The source-only SDR spatial fallback now accepts a preserved RGBA8 scene
texture and produces a display-sized RGBA8 output, alongside the existing HDR
RGBA16F fallback. A WARP integration test submitted a 2x2 four-colour source
to a 4x4 target, waited for GPU completion, and checked format, extent,
corners and interpolation by readback. Release `tools/Build.ps1 -Preset
win-release` passed all 22 CTest groups. This producer is not yet connected
to Skyrim's reduced-render path; its test does not establish an in-game
fallback or render-workload reduction.

The continuous SDR presenter now has a separate Quality SR entry that takes
render-sized scene colour/motion/depth and publishes a display-sized output
to the active backbuffer. It validates the destination device, format,
extent and active RTV before NGX submission, and keeps the existing DLAA mode
unchanged. A standalone 30-frame replay on the selected NVIDIA adapter used
synthetic 1280x720 colour/guides derived from the user capture's 2560x1440
post-world SDR frame. It completed all 30 Quality SR submissions, copied
2560x1440 output and retired cleanly; output SHA-256 was
`867cce8ef853ab8329e0eb605d4f3c1960b6bab5874e7284ece2a9b051137bb2`.
The same continuous path's 30-frame full-resolution DLAA regression passed
with output SHA-256
`8de9edf807257eb7efce05a646a05cdc6f09eeb3594c300b7fbd1cdc00fcb788`.
Release `tools/Build.ps1 -Preset win-release` passed all 22 CTest groups.
These results validate repeated NGX calls and destination copy in isolation;
Skyrim still supplies no genuinely reduced scene to this path.

The source-only SDR SR presentation stage now treats a busy or recoverably
failed provider as a real failure and draws a display-sized RGBA8 fallback
directly from preserved scene colour to the bound backbuffer. It keeps the
source and display resources through a GPU completion query, preserves the
caller graphics state, and propagates device removal without pretending a
fallback repaired the device. Its WARP regression read back a 2x2-to-4x4
failed-SR image and verified the bound RTV was unchanged. Release
`tools/Build.ps1 -Preset win-release` passed all 23 CTest groups. The
standalone NVIDIA 30-frame replay with one injected SR failure produced 29
Quality SR frames and one 2560x1440 spatial fallback (fallback SHA-256
`3106a5bfc949e2e522cbdc15a983ecd06b4a94da42b6f64de674453ad2299791`),
then resumed SR with a history reset and retired cleanly. Final output SHA-256
was `0d65cd3d050a3a433ad4a3c612dd4f22f893012204bc246fe704a7503ca2c773`.
The no-failure SR and DLAA regressions still produced the hashes above.
This does **not** validate fallback under Skyrim's real reduced-render source
or ENB/ReShade/UI composition; the MO2 plugin has not been updated.

## Exact Skyrim AE 1.6.1170 address map

The installed Address Library file SHA-256 is
`c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452`;
the decoder verified format 2, version 1.6.1170.0 and 428461 records. The
user-supplied DynamicShaderFrameGen source at pinned commit
`daaba8aadb2dbc8c5e52b028f12475c3450b6866` names these candidate IDs:

| Candidate | Exact installed-library RVA | Source comment RVA | State |
| --- | --- | --- | --- |
| Renderer Begin, ID 77245 | `0xe44590`; `+0xe2`=`0xe44672` | `0xe43450` | `+0xe2` was previously verified as an aligned CALL in decoded live code. |
| DRS control, ID 36555 | `0x643c00`; `+0x2d`=`0x643c2d` | `0x643300` | Decoded CALL to `0xe587f0` verified; no RazKolbas write. |
| Scissor function, ID 77365 | `0xe4adf0` | `0xe49cb0` | Decoded x/y/width/height ABI and original bytes verified; no detour. |
| BSGraphics::State, ID 411479 | `0x328cc20` | — | Ratio, previous-ratio and lock field accesses decoded; sampled menu values below. |

The source comment RVAs for DRS and scissor differ from this installed
Address Library, so they are not patch locations for RazKolbas. The source
calls the original jitter update, sets previous/current dynamic-resolution
ratios and lock, adjusts scissor coordinates, and disables vanilla DRS. The
exact game code and state field accesses are now decoded below; the reference's
entire hook sequence and a safe activation transaction remain unverified for
RazKolbas.
The supplied executable SHA-256 is
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The executable's on-disk text is encoded, so `tools/re/inspect_live_renderer.py`
now captures only bounded decoded bytes at the mapped DRS/scissor candidates
during a user-started game session. It performs no writes or remote calls.

## User-started decoded-code inspection, 14:36 on 2026-09-20

The user started `D:/TESV_EX/SkyrimSE.exe` (PID 20924); the inspector verified
the executable SHA-256 above and read decoded regions from image base
`0x7ff6f59a0000` without writing to or controlling the process. Raw captures
and the manifest remain ignored under
`artifacts/local/drs-live-2026-09-20-1436-extended/`. The 256-byte DRS
candidate SHA-256 is
`381d157c08454d3e6631b7ecf4b64a3ae021a9d3a70ed774bdb410668e0ed4b6`;
the 384-byte scissor candidate SHA-256 is
`9f5ebf128617965c575acdc46592cc846bacef4d3cb97ade6f1eb3e64a6c1d64`.

The exact DRS site at `0x643c2d` is the aligned five-byte
`e8 be 4b 81 00`, a `CALL` to RVA `0xe587f0`. The preceding instruction
loads `RCX` with the static graphics state at RVA `0x328cc20`. The callee
first checks the dword at state `+0x118` and skips updates when it is
nonzero. Otherwise it copies current float ratios `+0x104/+0x108` to
previous `+0x10c/+0x110`, then conditionally adjusts/quantizes the current
ratios using the dimensions at `+0x24/+0x28` and flags at `+0x11c` through
`+0x11e`. The sampled menu state held dimensions 2560x1440, all four ratios
at 1.0, counter `+0x118` at zero, and bytes `+0x11c/+0x11d/+0x11e` at
1/0/0. This confirms the reference's DRS concept for this process but also
shows its NOP would suppress an entire ratio-update CALL. Any RazKolbas
replacement must forward the original when reduced-render SR is inactive.

The scissor candidate at RVA `0xe4adf0` starts with `48 83 ec 38`, is not
already detoured in this capture, and returns at `0xe4ae2e`. Its Win64
arguments are a renderer pointer followed by **x, y, width, height**: the
function constructs a `RECT` of `(x, y, x+width, y+height)` before calling
the renderer's vtable slot `+0x168`. This resolves the reference's ambiguous
`right/bottom` parameter names. A future size adapter must scale the four
extent arguments consistently and validate which rendering domains call it;
blindly scaling every UI scissor would damage native-resolution UI.

## Activation boundary

The existing ignored `artifacts/local/live-stage-2026-09-20-1234/` bundle
already contains 12,288 decoded bytes beginning at the original world-draw
target `0xe44850`. Its first function returns at `0xe44bc9`; it selects and
clears render targets through renderer virtual calls. Capturing it again would
not reveal where the engine creates a smaller world target.

The decoded Renderer Begin function from the 14:36 user run gives a narrower
target. Its branch at `0xe445c3..0xe44625` invokes two renderer-state
callees, `0xe43bc0` and `0xe44050`. Later, at `0xe44725..0xe4475c`, it
constructs a stack descriptor with width and height from renderer state or
fallback globals, loads static object `0x328be80` into RCX, and calls
`0xe4fb90`. These callees were absent from the prior decoded captures, so
their effects could not be inferred from Renderer Begin alone. The targeted
user-started capture below resolves their immediate behavior, while the DRS
world-source question remains open. No DRS hook or reduced-render mode was
enabled on the earlier inference.

Skyrim still renders at 2560x1440, TAA remains enabled, and installed 0.1.24
retains the working full-resolution DLAA path. The DRS CALL, state field
accesses and scissor entry ABI are now decoded for this executable. Reduced
game sizing still requires guarded DRS/scissor ownership, native UI placement,
output ownership and connection to the display-sized failure fallback as one
reversible transition. No new plugin package or MO2 install resulted from this
read-only inspection. In-game reduced-render SR and FG remain NOT RUN.

## User-started decoded resize trace, 15:07 on 2026-09-20

The user-started SkyrimSE.exe PID 24652 had the same exact executable hash
and image base `0x7ff6f59a0000`. The inspector captured the missing decoded
callees without writing memory or sending game input. The complete ignored
bundle is `artifacts/local/render-size-live-2026-09-20-1512-loaded/`.
The loaded-world graphics-state snapshot still reported 2560x1440, current
and previous DRS ratios 1.0, and lock counter zero. The installed 0.1.24
log reached world/Present count 22,800 with no Present failure and reported
16,200 continuous full-resolution DLAA submissions with skipped=0 before
the process exited. No new crash log was found; the exit mechanism was not
observed. These observations do not exercise reduced rendering.

The caller at `0xe4475c` invokes `0xe4fb90`, which only copies 28 bytes from
the stack descriptor into static object `0x328be80` and returns at
`0xe4fba6`. It neither allocates nor resizes a target. This corrects the
earlier candidate interpretation above. The actual renderer reset path is
`0xe43bc0`: after checking the current swap dimensions, a changed-extent
branch releases entries across the renderer's colour and depth target tables
and reaches the swap-chain vtable `+0x68` (`ResizeBuffers`) call at
`0xe43e9f`. The separate `0xe44050` compares current and requested
dimensions and can call vtable `+0x70` (`ResizeTarget`) at `0xe44151`.
These are display/swap reset paths; this trace does not show how DRS sets a
smaller world viewport or which scene texture should feed DLSS before the
game's SDR conversion.

There is already an indirect jump at game RVA `0xe43e84`, immediately before
the `ResizeBuffers` call. A bounded read of its pointer led to an executable
stub at `0x7ffc691506c0`; that stub reads a separate 12-byte dimension/
format payload, sets Win64 arguments, and jumps back to game RVA `0xe43e9c`.
The hook owner was not identified. RazKolbas must not overwrite or bypass this
existing resize owner. The captured first 1536 bytes from `0xe43bc0` have
SHA-256 `def9e8e1ec4117ed3deb6b3f27620b31b09f63eb92527801bcf9db56d5132867`;
the 768 bytes from `0xe44050` have SHA-256
`6c926f2a19a7253168113b888a144ea50fa37e784cf875cadbbf7a04084ed25d`.
Raw code and disassembly remain ignored, not staged. A DRS transaction still
requires a verified reduced scene source and post-world display/UI ownership
before it can be activated.

## Source-only render-size decision policy

`RenderSizePolicy` now returns a native plan unless the requested extent is
valid, no larger than the display, genuinely smaller, and both the owned
world target and display-sized output/fallback path are declared ready.
Its optional scissor conversion scales only the world domain using the
verified x/y/width/height ABI; UI scissors remain native. Fractional world
edges round outward for coverage and coordinates clamp before arithmetic.
This is a pure decision function used by offline tests, not an installed
DRS/scissor hook. The readiness booleans do not themselves establish target
ownership. The installed 0.1.24 mod remains unchanged.

## Existing RazKolbas swap-chain integration boundary

`RendererBootstrap::createProxy` currently forwards
`D3D11CreateDeviceAndSwapChain` and observes its returned device/context/swap
pointer; despite its local name, it does not allocate an `IDXGISwapChain`
proxy. The same returned pointer is stored by `bindWorldDrawRenderer` and
used by `WorldDrawHook` to call `GetBuffer(0)` at the post-world boundary.
`installSwapObserver` patches only exact-hash known ENB/ReShade swap-chain
vtables, with pass-through Present/Resize behavior. The installed source path
therefore gets a display-sized backbuffer, and `SdrDlssPresenter` requires
that backbuffer's format/extent for continuous DLAA.

Substituting a render-sized `GetBuffer` here would change the identity and
lifetime observed by the renderer, the world hook and the ENB/ReShade vtable
chain. It would also deprive the current post-world copyback of its
display-sized target. The reference's reduced proxy cannot be inserted by
only changing the DRS ratio or scissor: an owned reduced scene surface,
display-sized destination and native UI transition must be prepared and
switched together. The unknown existing `ResizeBuffers` owner at game RVA
`0xe43e84` remains untouched.

## Owned reduced SDR surface checkpoint

`ReducedSdrSurface` allocates a default-usage, single-sample RGBA8 texture
with RTV and SRV bindings at a validated smaller render extent. It records
the separate display extent and owns both the texture and its RTV. It does
not implement `IDXGISwapChain`, substitute `GetBuffer`, or alter a Skyrim
target. A WARP integration test clears the owned 4x4 target, passes it through
the existing SDR SR input copy, forces provider failure, and reads back the
expected red 8x8 display fallback after retiring the original surface and
copied input. A separate test rejects invalid extents and proves an old
surface stays distinct when a new extent is allocated. Debug and Release
builds passed 25/25 CTest groups. This proves the offscreen resource and
fallback handoff; genuine in-game reduced world rendering remains NOT RUN.

## Resolved D3D11 context wrapper chain, user-started run 15:43

The saved decoded world-draw function calls virtual slot `+0x108` at game
RVA `0xe449f8` on the object loaded from game RVA `0x32887b0`. The new
bounded, read-only capture of PID 22120 resolves this as the D3D11 immediate
context's `OMSetRenderTargets`, **not** a Skyrim renderer class method.
The same function's `+0x190` call is `ClearRenderTargetView`; its `+0x1a8`
call is `ClearDepthStencilView`. The instance handed to Skyrim is ENB's
context wrapper: vtable RVA `0x1a49f8`, binding method RVA `0x68f40` in
the exact ENB `d3d11.dll` hash
`47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58`.
Its method forwards through object offset `+0x6c68` at ENB RVA `0x69227`
to a ReShade context wrapper. The latter's exact `dxgi.dll` hash is
`059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc`,
vtable RVA `0x3d2750`, binding method RVA `0xf40d0`; it forwards through
object offset `+0x18` to a heap vtable. The `+0x108` and `+0x190` entries
of that table resolve into Windows system `d3d11.dll` RVAs `0x100640` and
`0x106eb0`, respectively. The system file hash matches the existing exact
runtime profile
`722871e4ac32972617483197709fe0d924ced5ed894b18fd13e0813d0b25950f`.
This chain establishes API ownership and rules out patching a supposed
game-owned target-binding method. The ignored raw manifest and bounded
code/table snapshots are at
`artifacts/local/d3d11-context-chain-2026-09-20-1555/`. No game memory was
written, no remote call was made, and no game input was sent.

The same live capture read the eight game target-index dwords at RVA
`0x202ab88`: first index 8, remaining seven `-1` at the instant sampled.
The renderer records at game RVA `0x32887c0+0xa58` have a null texture in
slot 0 while slot 1 holds the HDR scene colour; this agrees with the
separate post-world COM resource mapping and the DynamicShaderFrameGen
source's warning that its `kFRAMEBUFFER.texture` route can be null. These
unsynchronized pointer values are a snapshot, not proof that index 8 is
always the UI or swap target. The existing 0.1.24 process has continued to
submit full-resolution DLAA with no reported Present failures in the
sampled log; reduced rendering is still NOT RUN.

## DynamicShaderFrameGen adaptation boundary

The current upstream `HEAD` is the locally pinned
`daaba8aadb2dbc8c5e52b028f12475c3450b6866`. Its source provides a
concrete render-time sequence: forward the original `Main_UpdateJitter`
CALL, then set dynamic-resolution previous/current ratios and lock before
the world render. Our exact-game mapping and decoded call at `0xe44672`
support adapting that sequence with the existing exact-byte CALL patcher.
Its scissor comments use left/top/right/bottom names, but the decoded game
ABI is x/y/width/height; any hook here must use the verified ABI. Its
`kFRAMEBUFFER` pre-UI path is disabled after the source observed a null
texture, and its active Present path takes colour from the backbuffer, whose
actual descriptor controls the submitted input size. That code cannot by
itself establish a smaller DLSS input or native-resolution UI in this
modlist. RazKolbas will use the DRS timing and ratios as a reference, keep
its own post-world/UI placement, and activate a reduced ratio only with a
verified smaller source plus display-sized fallback. No DynamicShaderFrameGen
code or binaries have been copied into the product.

## Experimental engine DRS probe, source 0.1.25

RazKolbas now has its own exact-hash startup CALL hook for the Renderer Begin
jitter site `0xe44672`. It forwards the original `0xe58a10` first and may
then update state current/previous ratios and its lock under the verified
`BSGraphics::State` layout. An independent pure policy rejects foreign locks,
extent mismatch and changed ratio ownership; it can release only the exact
ratio/lock it previously set. The callback requires an explicit
`Diagnostics.ProbeReducedWorld=true`, an explicit manual scale in `[0.5,1)`,
the verified display-sized creation output, a live game `kMAIN` texture
pointer, and exact decoded caller/target bytes. The setting is **off by
default**. On an eligible sampled world frame the hook logs the actual
colour, motion, depth and display texture descriptors. Once the probe has
run, the current full-resolution SDR DLAA copyback is suppressed for the
rest of that process so a changed guide size is not submitted as DLAA.

This tests whether Skyrim's own DRS really makes the scene inputs smaller
while retaining the native display/UI path in this ENB/ReShade modlist. It
does not call DLSS SR and is not the owned reduced SDR surface from the
earlier WARP test. A separate exact patch descriptor is at
`patches/skyrim/jitter-drs.probe-v1.json`. In-game probe and scissor/UI
compatibility remain NOT RUN until the user starts Skyrim with a staged
opt-in build. The existing 0.1.24 game process was not modified.

## 0.1.25 user-run result and guarded retry

The user started installed 0.1.25 at 16:16:16 as exact-hash SkyrimSE.exe
PID 15004. The startup CALL hook installed and ENB/ReShade device creation
completed. At the first world callback the ratio was (1,1), the display was
2560x1440, and state lock was 1, so the 0.1.25 ownership guard rejected the
probe permanently and performed no DRS write. The world stayed at native
2560x1440; full-resolution DLAA continued through at least 28,200 world and
Present observations, with zero reported Present failures. The End diagnostics
menu was opened; no reduced-render result was obtained.

A later bounded, read-only `BSGraphics::State` sample of the same process
reported display 2560x1440, previous/current ratios all 1.0, lock 0, and
flags `01 00 00 00`. The loaded module list contained RazKolbas, ENB,
ReShade and SSE Display Tweaks, but no separate SkyrimUpscaler or
DynamicShaderFrameGen module. These observations support treating the first
native-sized lock as potentially transient; they do not establish its owner
or prove that it clears at the exact Renderer Begin callback. The decoded
vanilla DRS callee at `0xe587f0` returns immediately when lock is nonzero.
Raw process bytes are ignored under
`artifacts/local/drs-live-2026-09-20-1616-probe/`.

The next source revision permits a strictly read-only retry when the state
still has exact display dimensions, native (1,1) ratios, and lock 1. It logs
the first wait and every 600th wait. It never overwrites that lock. If a
later callback sees lock 0, the existing guarded transition can activate;
changed ratios, extents or other lock values still reject. The 0.1.25 DLL
was not modified during the running game. This retry is build-tested only;
in-game DRS activation, guide extents, scissor/UI behavior and DLSS SR remain
NOT RUN for the revised build.

## 0.1.26 user-run result: ratio write did not reduce scene targets

The user started exact-hash SkyrimSE.exe PID 6196 with the guarded retry.
Renderer Begin saw a native-sized lock on 5,078 callbacks, then one callback
accepted lock 0 and set a 1706x960 ratio at 16:30:43. The next callback saw
lock 3 with ratio (0.6664063,0.6666667), so the ownership guard rejected
further writes. Later read-only game state reported the reduced ratio in the
**previous** fields, current ratios (1,1), and lock 0. Measured kMAIN colour,
motion, depth and display textures remained 2560x1440 at repeated sampled
world frames. No reduced scene or DLSS SR submission occurred. The process
continued past 19,200 world/Present calls without reported Present failure;
there was no new crash log, and the user exited the game.

The earlier `drsProbeHasRun` gate intentionally withheld continuous DLAA
after the tentative ratio write. That proved too conservative once the game
state and all scene inputs returned to native dimensions. Source now has an
exact native-recovery policy: after the rejected probe, read current game
ratios/lock and actual colour, motion, depth and backbuffer descriptors under
the verified renderer lock; resume DLAA only when current ratios are (1,1),
lock 0 and all three scene inputs match the display. This path is
build-tested only. The installed opt-in DRS probe should be disabled while
the genuine reduced-buffer integration is developed.

Static `SkyrimSE.exe` bytes at `0xe587f0` are encrypted, while the live
process exposed decoded DRS code. `tools/re/capture_decoded_text.py` is a
hash-gated, read-only capture of the full executable `.text` sections for
offline xrefs to render-target allocation and DRS lock writers. It requires
a user-started game; output stays ignored under `artifacts/local/`. The
reference SkyrimUpscaler proxy creates and returns a render-sized SDR buffer
at swap-chain `GetBuffer`, which explains why adapting only the DRS ratio was
insufficient here. That proxy behavior is an RE reference, not a RazKolbas
implementation or a safe instruction to replace the existing ENB/ReShade
swap-chain owner without an owned presentation transaction.

## Full decoded-code capture and allocation xrefs, user run 16:57

The user started installed 0.1.27 as exact-hash SkyrimSE.exe PID 8012 at
16:57:14. `capture_decoded_text.py` read its 24,435,000-byte executable
`.text` section without calling, pausing or writing to the process. SHA-256
is `75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3`;
raw bytes and offline candidate reports remain ignored at
`artifacts/local/skyrim-decoded-text-2026-09-20-1657/`. Disk bytes at DRS
RVA `0xe587f0` are encrypted; the captured live bytes decode to the verified
`cmp [rcx+0x118],0`. The probe was off in the installed INI. The observed
menu-like run forwarded more than 28,200 world/Present calls with zero
reported Present failures. Its sampled depth was initially uniform, so this
run does not establish a continuous DLAA display submission or exercise the
new rejected-probe recovery path. The assistant did not start, control or
close the game.

The decoded function at `0xe58980` treats state `+0x118` as an **atomic
reference counter**: `DL=0` performs `lock inc`, while nonzero `DL` performs
a guarded `lock dec`. Exactly eight direct CALL sites were found at
`0x643db3`, `0x643df8`, `0x643eb0`, `0x643ed1`, `0x643f57`, `0x643f82`,
`0x6d2201` and `0x6d22dd`. The `0x6d2201` call increments the counter just
before Renderer Begin `0xe44590`; `0x6d22dd` decrements it after Renderer End.
This explains the frequent lock 1 at our jitter callback and why a later
callback could see 3 after our raw assignment. The ratio-only probe's
exclusive-lock policy was an invalid ownership model. Any future DRS
transaction must preserve this shared count through the game's counted
interface; writing a literal 1 or 0 would risk corrupting another phase's
lock. The opt-in probe is disabled in the installed package.

The renderer target allocator at `0xe44d90` receives a descriptor in `R9`.
At `0xe44dc3..0xe44e00` it copies descriptor width, height, format and flags
to a D3D11_TEXTURE2D_DESC; `0xe44e40` calls device vtable `+0x28`
(`CreateTexture2D`) and stores the colour texture at renderer record
`+0xa58`, followed by view creation. The allocation initializer at
`0x14cf210` loads display width and height from state RVAs `0x328cc44` and
`0x328cc48`, then builds target descriptors and calls wrapper `0xe4fbb0`
many times (including early target indices 2–7). It does not use the DRS
ratio for those descriptor dimensions. This is why a ratio write alone did
not change kMAIN/motion/depth texture extents. It does **not** by itself
settle whether Skyrim can reduce viewport pixel work inside full-size
textures; that still requires a measured viewport/region trace.

The current decoded entry at `0xe4fbb0` begins with an indirect JMP
`ff 25 f1 40 19 ff`, indicating an existing hook on the texture-creation
wrapper in this modlist. The original wrapper body tail-jumps to `0xe44d90`
at `0xe4fbe7`. The hook target/owner was not captured before the user exited,
so RazKolbas must not patch or bypass that entry. The next exact run should
read the indirect target and loaded-module owner, and trace viewport and
buffer transitions without changing game memory. Offline helper scripts
`find_drs_accesses.py` and `find_static_lea_xrefs.py` list **candidate** xrefs;
overlapping x86 decoding means each patch-relevant site still needs aligned
function verification.

The world viewport is a separate D3D11_VIEWPORT at static RVA `0x202abe0`.
`e44b5f` and `e4b8c3` pass it to `RSSetViewports`. Skyrim's setup functions
write its width/height (`+8`/`+c`) at `e43fe1/e43fd9` and
`e442f4/e442ec`; the former path reads current DRS ratios from state
`+0x104/+0x108` at `e43f10/e43f18` and multiplies them into viewport
dimensions. This supports a *possible* reduced viewport inside full-size
textures, but the 0.1.26 runtime probe did not measure the viewport or a
valid cropped colour/depth/motion rectangle. Texture allocation remains
native sized. Its shared counter model makes the old ratio-writing probe
unsafe, so it is retired in source; `ProbeReducedWorld=true` now returns
Unsupported without installing the call-site hook.
