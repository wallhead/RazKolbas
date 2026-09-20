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
