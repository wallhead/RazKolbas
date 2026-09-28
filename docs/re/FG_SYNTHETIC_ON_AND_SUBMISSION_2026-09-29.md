# Streamline FG-On synthetic result and submission boundary, 2026-09-29

The pinned Streamline 2.14.1 standalone probe now has a separate `--on` path.
It creates a proxied D3D12 flip swap at 1280x720 on the local NVIDIA adapter
(vendor `0x10de`, device `0x2702`, RTX 4080 SUPER), fills synthetic D3D12
depth, motion, HUD-less and transparent UI planes, and tags them together
with the full backbuffer extent for each frame. The probe obtains a new SL
frame token, supplies row-major common constants, enables SL Reflex,
submits PCL simulation/render/Present markers with the same token, requests
one generated frame and calls the lower Present once per real frame.

The first run used the previous hidden `STATIC` window. All SL calls returned
`eOk` and `DLSSGState::status` was `eOk`, but `numFramesActuallyPresented`
was 1 for each of eight real Presents. Verbose SL logging named the reason:
`DLSS-G disabled: window not focused`. That run is a negative FG result.
The corrected probe uses an ordinary foreground-capable top-level window.
It recorded `FG-On foreground=1`, then `status=0x0`,
`numFramesActuallyPresented=2`, `numFramesToGenerateMax=1` for each of
eight real Presents, and exited 0. A second run after explicitly tagging
the full backbuffer extent repeated the same 2-per-1 result eight times.
This is direct SL telemetry for actual synthetic extra-frame presentation.
Subsequent runs while another desktop window held focus again recorded only
one frame; the probe now exits with a distinct focus-unavailable code before
FG submission when it cannot acquire the foreground.

The latter verbose log contains no invalid-token, invalid-constants or
missing-resource warning. It contains a one-time SDK internal
`cloneResource` alignment override warning and a first-frame timer reset
warning after more than 100 ms. The probe uses static synthetic colour,
zero motion and identity camera matrices; its count does not establish
generated-frame image quality. It does not exercise Skyrim, ENB/ReShade,
SR/NR, live camera motion, HUD or menus. The existing FG-Off facade probe
still read red, green and blue real frames at rotating lower indices 0, 1,
0 before Present and passed resize. The `--swap` pass-through probe also
still exited 0. Debug and Release each passed 54/54 CTest groups.

The new source-only `FgPreparedSubmission` validates the real-frame identity,
copied lease, UI snapshots, finite typed camera payload, true texture
extents, single-sample resource descriptors, one D3D12 device and physical
lower backbuffer index. The lease now retains the five original D3D11
source identities; the submission compares copied final, HUD-less and UI
sources to the UI snapshot instead of trusting matching numeric stamps.
WARP tests reject changed tokens, epochs, copy tickets, UI source identities,
missing/wrong-size guides, invalid camera data and an out-of-range physical
index. This is validation before SL submission, not an SL provider or game
integration. Actual resource states, SL token mapping, previous-frame SL
input completion, true camera matrices and the native transparent UI
producer still need to be connected and verified.

The installed V5.4 0.1.115 DLL and INI were not changed. Skyrim and game FG-On
validation are **NOT RUN**. The next runtime evidence must establish the
post-effects/pre-UI source and full camera/guide timing in the user's
Skyrim/ENB/ReShade route before turning on a game FG provider.
