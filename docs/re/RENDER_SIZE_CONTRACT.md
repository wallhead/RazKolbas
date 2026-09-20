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

Skyrim still renders at 2560x1440, TAA remains enabled, and installed 0.1.24
retains the working full-resolution DLAA path. The DRS CALL, state field
accesses and scissor entry ABI are now decoded for this executable. Reduced
game sizing still requires guarded DRS/scissor ownership, native UI placement,
output ownership and connection to the display-sized failure fallback as one
reversible transition. No new plugin package or MO2 install resulted from this
read-only inspection. In-game reduced-render SR and FG remain NOT RUN.
