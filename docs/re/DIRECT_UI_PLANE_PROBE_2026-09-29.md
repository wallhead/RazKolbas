# Guarded direct UI plane probe, 0.1.120, 2026-09-29

## Evidence and question

The 0.1.119 game capture attributed visible native-target writes to eight
menu-call intervals before `GRenderer::EndFrame`; neither EndFrame nor Present
changed the sampled native pixels. The AIO reference's static direct route
also binds and clears a separate UI target. Neither observation establishes
that Skyrim's HUD preserves useful alpha when rendered into a transparent
target. This probe asks whether a separate HUD plane can reproduce the normal
native image before any FG input tagging is attempted.

## Implemented contract

`Diagnostics.ProbeDirectUiPlane` is false by default and read at startup.
Once per process, only on an eligible MenuDisplay frame with actual DLSS SR
provider output, the plugin creates a display-sized RGBA8 texture and RTV.
It excludes title, loading, Inventory and Magic menu stacks. The owned UI
redirector validates the device, size, format, generation, frame and render
thread; it clears the new target to transparent and maps recognized late
scene RTV binds to it. Existing native-depth companions and viewport remaps
remain in force. The probe composites premultiplied UI onto the published
native scene at pre-Present, preserving the scene alpha, then disarms the
target. Failed submission and stale-frame paths also disarm it. Normal frames
continue through the established native route.

The one-shot capture writes `hudless-native.raw`,
`direct-ui-premultiplied.raw` and `composited-final.raw` with dimensions,
format, byte counts and hashes in a manifest. The name describes the blend
model being tested; the actual UI pixels are not yet proven premultiplied.
`CaptureFirstDlssFrame` can collect the per-menu sequence in the same run.
Captures remain in the user's Skyrim documents directory, outside Git.

WARP tests check route arming, clear, late binding, disarming, non-aliasing,
premultiplied blend output, preserved alpha and restored D3D11 state. Debug
and Release builds each passed 55/55 CTest groups. No Skyrim frame has yet
run this candidate; runtime result is **NOT RUN**. FG-On is also **NOT RUN**.

## Runtime decision

In a user-started save, verify the manifest and per-file hashes, then compare
the three captures. Measure UI-plane nonzero colour and alpha distributions,
bounding rectangles and the composite difference from a normal native HUD
frame. Check whether the image remains correct with ENB and ReShade active.
If alpha is unusable, trace the actual D3D11 blend/write state for the
attributed HUD draws before changing composition. Only a verified separate
UI producer can be offered to FG; camera constants and the Streamline FG
contract remain independent open work.
