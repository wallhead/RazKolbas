# FG camera binding and Streamline constants, 2026-09-29

The two-frame 0.1.125 Skyrim capture showed byte-identical game camera
buffers at MenuDisplay and the game-facing pre-Present observer. Paired
prepared depth/motion guides repeated the forward-Z, normalized
previous-UV-minus-current-UV static-scene convention. See
`FG_CAMERA_PHASE_GAME_CAPTURE_2026-09-29.md` for actual hashes, frames and
limits.

Source now derives a candidate `FgCameraData` from the guarded 1.6.1170
camera calibration, a real-frame identity, pixel jitter and an independently
assigned camera revision. It rejects invalid world state, cut without reset,
invalid stamps/jitter and unsupported projection geometry. It sets
`mvecScale={1,1}`, `depthInverted=false`, `cameraMotionIncluded=true`, and
uses the calibrated vertical FOV. The pinned Streamline 2.14.1 adapter copies
the four row-major matrices and remaining camera fields into `sl::Constants`
and rejects mismatched tokens and nonfinite values. The synthetic FG-On
harness now uses this adapter instead of a separate constants initializer.
Another pinned-SDK adapter constructs one-extra-frame `sl::DLSSGOptions`
from a prepared submission's actual render/display extents, five resource
formats and lower swap count, rejecting later size/format drift.
A retained tag bundle now maps prepared depth/motion at render size and
HUD-less/UI at display size, with a null full-display Backbuffer tag and
`eValidUntilPresent` lifetimes. It requires a matching completed-copy ticket
and explicit shader-read state for each of five resources. WARP fixtures
create textures in that declared state. A numeric state/fence ticket is not
proof that the live queues have completed their copy and transition.
The three adapters now compose into a single source-stamped
`FgStreamlineFrameInputs` packet. It rejects a different real frame or a
non-world/loading/paused frame before returning constants, options and
retained tags. This packet still does not call Streamline or own an SL token.
A separate SDK-call boundary requires an explicitly matching SL token
binding, then calls `slSetConstants` before `slSetTagForFrame` through
injectable functions. Mocked calls verify order, arguments, stale-token
rejection and failure propagation. The game has not supplied real token
ownership or invoked this boundary.

The standalone synthetic FG-On harness now builds the same stamped packet
from five D3D12 textures after a real producer-fence wait, calls the SDK
submission boundary with its official SL token, and checks any returned
provider-input completion fence before reusing those textures. Its producer
is a single D3D12 queue; it does not exercise the game's D3D11 copy path.
On the local RTX 4080 SUPER, the probe acquired foreground, reported
`slSetD3DDevice=0` and DLSS-G support, then reported `status=0x0` and
`numFramesActuallyPresented=2` for each of eight real Presents. It turned
DLSS-G Off, issued the drain Present, shut down and exited 0. This directly
verifies synthetic extra-frame output through the new packet and call path,
not Skyrim image quality or ENB/ReShade compatibility.

Debug built with **57/57** CTest groups passed. Release with the hash-pinned
Streamline 2.14.1 SDK built with **60/60**, including constants, options,
tag-bundle, source-packet and mocked SDK-call tests.

The game-side mapping remains a candidate, **not live Skyrim FG submission**.
A caller could falsely stamp old camera bytes with a current token; the mapping API
cannot establish that the producer emitted those bytes for that real frame.
Game wiring must use an independently sampled producer/frame stamp or a
continuous phase check. Moving-object vectors, cuts, actual Streamline lower
Present, native UI ownership and provider-input retirement remain unverified.
The installed 0.1.125 plugin is unchanged and FG remains Off.
