# SkyrimUpscalerAIO Build16-Hotfix1 live routing

Status: **LIVE_OBSERVED** on 2026-09-22. The user enabled the supplied
SkyrimUpscalerAIO reference in MO2, started Skyrim 1.6.1170, loaded a save and
left the process running for read-only inspection. RazKolbas was disabled for
this run. The assistant did not start the game and closed it through the
normal window-close path after capture.

The loaded reference files matched the preserved project copies:

- `SkyrimUpscaler.dll` SHA-256
  `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
- `PDPerfPlugin.dll` SHA-256
  `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
- Skyrim executable SHA-256
  `c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.

These host DLLs are reverse-engineering references only. RazKolbas must not
package, load or require either file.

## Live hook and size state

The process reported a native display of 2560x1440 and a live viewport of
1706x960 while all four sampled engine DRS ratio values remained 1.0. The
reference state object independently stored display `2560x1440`, render
`1706x960`, mode 1 and enabled state 1. This proves that copying Skyrim's DRS
ratio tuple cannot reproduce the reference route: the reference owns the
reduced viewport and resources separately.

The following live patches were present:

- Skyrim RVA `0xfa507a`, the same verified world-draw CALL used by RazKolbas,
  relayed to `SkyrimUpscaler.dll+0x156830`.
- Skyrim RVA `0xe44672`, the renderer jitter CALL, relayed to
  `SkyrimUpscaler.dll+0x157190`.
- The ENB immediate-context `OMSetRenderTargets` slot relayed to
  `SkyrimUpscaler.dll+0x2015f0`.
- The ENB immediate-context `RSSetViewports` slot relayed to
  `SkyrimUpscaler.dll+0x2012c0`.

Skyrim RVA `0xfa51cb`, the RazKolbas 0.1.54 menu-display candidate, and the
menu-loop entry at RVA `0xfa4f00` remained unmodified by the reference.

The live reference state contained distinct non-null resource objects at
offsets `+0x1d0`, `+0x228`, `+0x280`, `+0x2d8`, `+0x330`, `+0x700` and
`+0x710`. The original world and jitter calls resolved back into Skyrim, and
the saved OM and viewport methods resolved back into the loaded ENB D3D11
wrapper. This is a resource-identity route layered into the existing ENB
chain, not a replacement immediate context.

## World, jitter and UI behavior

The world callback at reference RVA `0x156830` forwards Skyrim's original
world call, obtains the active proxy buffer index, and then prepares and
evaluates the upscaler. Its internal processing at RVA `0x1f4ca0` verifies
required color, motion and depth wrappers, builds the fixed evaluation
descriptor, calls `PDPerfPlugin!EvaluateUpscaler`, and blits the result into a
selected native target. It sets an internal publication flag used by the
context hooks.

The jitter callback at RVA `0x157190` forwards the original call, retains the
camera pointer, obtains the phase count and offset, and writes normalized
projection jitter using the current reduced render width and height.

The OM hook at RVA `0x2015f0` compares incoming resources with several exact
stored identities, the active proxy target, and the selected native output.
It conditionally substitutes targets while preserving depth where required.
The viewport hook at RVA `0x2012c0` changes a single full reduced viewport to
the stored display extent only when the publication state and target identity
match. It also has explicit Main Menu and Loading Menu handling.

This explains the RazKolbas 0.1.53 black screen. AIO does not treat the first
single reduced color bind as a universal UI boundary, and it does not reject
every later reduced color-plus-depth bind. It follows known resource
identities throughout the frame. If the semantic menu-display boundary in
0.1.54 is falsified, the safe fallback is a resource map with explicit roles,
not another bind-shape heuristic.

## PDPerf ABI and ownership boundary

`PDPerfPlugin.dll` was loaded at runtime and exposes 102 exports. Relevant
exports include `SetupDirectX`, `InitUpscaler`, `EvaluateUpscaler`,
`InitFrameGen`, `EvaluateFrameGeneration`, `CreateSwapChainProxy`,
`CreateSwapChainProxyForHwnd`, `SetFrameGeneration`, `SetFrameGenParams` and
`SetCameraData`, plus NGX forwarding exports.

The live global backend object dispatched SR through vtable slot `+0x28` and
frame generation through slot `+0x40`. Both exported evaluation functions
copy a 176-byte descriptor before forwarding it. Static and live inspection
show the descriptor carries resource pointers, render dimensions, jitter,
exposure/reset/frame metadata and the selected output. The SR backend checks
feature availability, refreshes the feature when dimensions change and then
submits the descriptor to its concrete D3D11 upscaler. The frame-generation
backend consumes the same frame descriptor, substitutes proxy resources where
needed, attaches the current camera data and submits through the active
Streamline D3D12 implementation.

The reference log corroborated the route:

- NGX D3D11 created DLSS for approximately 1706x960 to 2560x1440.
- Motion and depth were found at 1706x960.
- Streamline D3D12 frame generation initialized successfully.
- The proxy exposed an isolated shared D3D11 game buffer.
- The D3D12 companion path opened shared resources and initialized DLSS-NR.

PDPerf therefore combines several responsibilities that RazKolbas must own
separately: SR descriptor submission, swap-chain proxying, D3D11/D3D12 shared
resource synchronization, Streamline tags, camera constants, latency markers
and generated-frame presentation. Its private ABI is evidence for our own
interfaces; it is not a redistributable dependency.

## Limits and next use

The run proves the reference's live routing and object relationships on this
machine. It does not prove that copying its private offsets is safe across
versions, and no reference memory or code was modified. Raw Ghidra projects,
decompilation, process snapshots and runtime reports remain ignored under
`artifacts/local/`.

RazKolbas 0.1.54 should receive its first game test unchanged. If its verified
menu-display boundary produces a complete scene with native UI, the more
complex reference-style resource map is unnecessary for SR. If it fails, the
recorded resource roles and hook behavior define the next implementation:
publish from the already verified world callback while tracking exact owned
scene, motion, depth, evaluated output, active proxy/native target and UI/HDR
resources through the existing ENB context chain.
