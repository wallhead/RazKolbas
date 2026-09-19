# Candidate frame-resource probe (0.1.5)

This is a bounded diagnostic milestone for T05/T06. It does not establish pre-UI colour, world rendering, motion conventions, depth linearization, source-frame identity or usable SR guides. SR/FG/NR evaluation remains inactive.

## Provenance and recovered layout

Selected game: SkyrimSE.exe 1.6.1170.0. No on-disk game changes.

Game SHA256: `c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9` (37157144 bytes).
Address Library `versionlib-1-6-1170-0.bin`: SHA256 `c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452`, 795129 bytes. Parsed format2 using the pinned CommonLib IDDatabase encoding; all 428461 records consumed exactly.

| AE ID | Meaning | RVA |
|---|---|---|
| 411393 | Renderer object (direct address) | 0x32887c0 |
| 411347 | Runtime-data global pointer | 0x3286a08 |
| 77243 / 77244 | Lock / Unlock | 0xe44550 / 0xe44570 |
| 77245 / 77246 | Begin / End | 0xe44590 / 0xe44770 |
| 82084 | Earlier handoff's pre-UI candidate, still unproven | 0xfa4f00 |

Source reference: [CommonLibVR commit abe9ca7](https://github.com/alandtse/CommonLibVR/tree/abe9ca7b7318dbc04bccfcd59fc3fb670244c2d7). Four downloaded files were re-fetched at that immutable commit and byte-hash matched:

| File | SHA256 |
|---|---|
| include/RE/R/Renderer.h | 125b715fb3f2a47022975c10c3683ceaf505521de1cae0427970e6133771751b |
| src/RE/R/Renderer.cpp | 65829a89a1a9a4bc1fbfe317acc7d8e9087ce0fccbc9ece60aaa0bf87a456498 |
| include/RE/R/RenderTargetData.h | d2217dfb0972a6dae7d8215bd96d4fcf8fd1f61e9b1bb4badb40ba64892268ba |
| include/RE/B/BSShaderRenderTargets.h | d1563adaf080c4d7c3db6b530dd5368020f96e23c14f69fb3ac7c570b555aefb |

The main dependency remains pinned; this reference does not upgrade CommonLib. For this game: device +0x48, context +0x50, window0 swap +0x70, RT table +0xa58 (stride0x30), colour index1, motion index7, depth table +0x2018 (stride0x98), critical section +0x27f0. Snapshot extent0x2840. Header comments are not treated as independent proof of offsets.

Read-only live inspection with `tools/re/inspect_live_renderer.py` correlated the object, 2560x1440 window, nonnull candidate textures and lock with these offsets. Lock bytes `4881c1f027000048ff2572ab9000`; Unlock `4881c1f027000048ff255aab9000`. Runtime verifies both before enabling the probe. Disk .text is encoded; live decoded bytes are the applicable signature. End already contains another owner's call/jump patches, so this milestone does not modify End.

## Runtime contract

Existing exact-game creation validation and exact ENB swap-table profile remain mandatory, with experimental settings enabled. Only the base ENB Present before-callback is eligible; ReShade Present and Present1 are excluded. At least 90 seconds after the first eligible call, at most32 ownership checks spaced5 seconds apart seek one capture. This delay is diagnostic, not a claim that a world frame is loaded.

GetDevice/GetImmediateContext use the live COM swap. Raw renderer memory is copied through bounded ReadProcessMemory. No candidate pointer is dereferenced unless the current thread already owns the verified renderer critical section (positive recursion and matching owner ID) and renderer device/context/swap values exactly match the live COM objects. The callback does not acquire an engine lock. Borrowed textures remain within that same owned-lock call.

All descriptors and the total64MiB CPU budget are validated before GPU copies. Single-sample, one-mip, one-array textures in the supported raw formats are copied to local staging resources, mapped synchronously, packed row-by-row and unmapped. Map completion precedes local staging retirement. No GPU resources or engine pointers survive the callback; output files contain CPU bytes only. One successful ownership check consumes the GPU attempt even if capture/write fails. A diagnostic frame stall is expected; this is not a performance measurement or production capture pipeline.

Files are under Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/process-tick/. Only a manifest ending `complete=true` denotes a fully written bundle. Each raw image records width, height, format, rowBytes, byte count and SHA256. Colour, motion and depth names explicitly include `candidate`.

## Validation

Core pointer-contract/readback tests were observed RED then GREEN. WARP verifies odd-width row packing, source-pixel and bound-RTV preservation, foreign-device/null/budget rejection and raw motion/typeless-depth formats. ENB-only boundary regression was observed RED before its profile/method gate. Runtime capture is pending at this source checkpoint; test results and deployed evidence follow below.

Next: inspect actual bytes/descriptors, then recover world/pre-UI boundary and camera/depth/motion semantics using a loaded save. A main-menu capture cannot establish those contracts.
