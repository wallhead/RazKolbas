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

## Exact Skyrim AE 1.6.1170 address map

The installed Address Library file SHA-256 is
`c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452`;
the decoder verified format 2, version 1.6.1170.0 and 428461 records. The
user-supplied DynamicShaderFrameGen source at pinned commit
`daaba8aadb2dbc8c5e52b028f12475c3450b6866` names these candidate IDs:

| Candidate | Exact installed-library RVA | Source comment RVA | State |
| --- | --- | --- | --- |
| Renderer Begin, ID 77245 | `0xe44590`; `+0xe2`=`0xe44672` | `0xe43450` | `+0xe2` was previously verified as an aligned CALL in decoded live code. |
| DRS control, ID 36555 | `0x643c00`; `+0x2d`=`0x643c2d` | `0x643300` | **Unverified code bytes and semantics**. |
| Scissor function, ID 77365 | `0xe4adf0` | `0xe49cb0` | **Unverified code bytes and ABI**. |
| BSGraphics::State, ID 411479 | `0x328cc20` | — | Previously sampled as static camera state; DRS field layout unverified. |

The source comment RVAs for DRS and scissor differ from this installed
Address Library, so they are not patch locations for RazKolbas. The source
calls the original jitter update, sets previous/current dynamic-resolution
ratios and lock, adjusts scissor coordinates, and disables vanilla DRS. That
sequence is a hypothesis about the engine contract; neither the exact field
offsets nor a safe patch transaction has been established for this process.
The supplied executable SHA-256 is
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The executable's on-disk text is encoded, so `tools/re/inspect_live_renderer.py`
now captures only bounded decoded bytes at the mapped DRS/scissor candidates
during a user-started game session. It performs no writes or remote calls.

## Activation boundary

Skyrim still renders at 2560x1440, TAA remains enabled, and installed 0.1.22
continues its working full-resolution DLAA path. Reduced game sizing cannot be
activated until the exact DRS/scissor field and instruction contracts, native
UI placement, output ownership, and display-sized failure fallback are ready
as one reversible transition. The next live step is a **read-only capture**
of the mapped code; no new plugin package or MO2 install is needed for it.
