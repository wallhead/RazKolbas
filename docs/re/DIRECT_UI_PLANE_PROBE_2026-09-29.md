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
and Release builds each passed 55/55 CTest groups. FG-On is **NOT RUN**.

## User-started Skyrim result

The user started Skyrim with 0.1.120 and loaded a save. The first eligible
native-boundary DLSS frame was 26506 in process 27836, at 2560x1440. The
`direct-ui-plane-27836-26506-249706890` bundle contains three RGBA8 files,
each 14,745,600 bytes; its manifest is complete, and every file's size and
SHA-256 match. The same-frame per-menu and preceding SR bundles also passed
their manifest checks. The log confirms a separate plane was armed and
captured with FG off, without a route or composition error. Skyrim was closed
after capture, and the normal INI and all eight MO2 payloads were verified.

The transparent plane has **149,447** pixels with alpha greater than zero:
31,043 opaque and 118,404 partially transparent, spanning (33,38)–(2525,1427).
It contains 81,613 pixels with nonzero RGB, no nonzero RGB where alpha is zero,
and no RGB channel greater than alpha plus one byte. This is consistent with
premultiplied colour. The native scene buffer did not change through 15 menu
calls or `GRenderer::EndFrame`, whereas the pre-Present composite changed
140,927 RGB pixels, all where plane alpha is nonzero. Native and final alpha
remain 255 everywhere. Per-channel deviation from
`UI.rgb + native.rgb * (1 - UI.a)` is below 0.5 byte before rounding, and no
pixel deviates by more than two bytes. The saved full-frame preview shows HUD,
subtitles and quest text in the expected screen regions over the scene.

This verifies a usable independent HUD colour/alpha producer and correct
one-frame composition in this save. It does not compare against a normal
HUD rendering of the *same* game frame, establish continuous resource
retirement, or validate every menu and ENB/ReShade ordering. FG-On remains
untested.

## Runtime decision

Retain a frame-stamped HUDless scene and transparent UI pair through the FG
input lease while still presenting the correctly composited native frame.
Before enabling FG-On, verify that the backend receives the distinct pair
and that retirement protects both textures. Inventory, Magic, title and
loading UI paths and ENB/ReShade effect placement still need runtime checks.
Camera constants and the Streamline FG contract remain independent open work.

## Review 31 follow-up in installed 0.1.122

The first 0.1.120 frame did not retain an independent scene image from the
start of the UI interval; its pre-composite native image alone cannot rule out
a native UI bypass on another frame. Version 0.1.122 captures that initial B0
image before arming, then B1 and the transparent UI plane before composition,
and the final image afterward. The four filenames are
`scene-at-ui-boundary.raw`, `native-before-composite.raw`,
`direct-ui-premultiplied.raw` and `composited-final.raw`. Direct native binds,
the preserved reduced menu branch and a late reduced-scene publication mark
the route partial. A candidate is logged complete only when no such route
event occurred, B0 equals B1, and the native target and generation match.
This classification is diagnostic and does not feed FG yet. It preserves the
original compositor even if readback fails.

The capture is preflighted against a 256 MiB transient budget before the UI
target is armed, with exact image-size readback budgets. Its output RTV must
be typed RGBA8 UNORM. Debug and Release each passed 55/55 CTest groups. The
prior 0.1.120 result above remains valid for its sampled frame; the guarded
0.1.122 game observation is recorded below.

## User-started 0.1.122 guarded frame

Process 3604 captured world frame 47328 at 2560x1440 after the user loaded a
save. `direct-ui-plane-3604-47328-254597031` has a complete four-file
manifest; all recorded byte counts and SHA-256 values were independently
checked. The log reports a complete UI route, identical target identity and
scene generation, and an RGBA8 UNORM output RTV. The B0 scene at the UI
boundary and B1 native image before composition have the same SHA-256,
`59e8ac2a1920cabab3bc29e8bf7ea2ac8717aac2510798c1555682b2daa07562`,
and zero byte differences. Thus no sampled native colour changed during that
frame's UI interval. The plane SHA-256 is
`c8e28142b34d6e8f57c119fc754016bfc7244918eb87317926ab06a355477e2c`;
the final image SHA-256 is
`e67a4a273ebd0bf9a37bcb0a99f1a8ec38f05390294ef6d23858ca589f3e0d2d`.

The UI plane contains 149,508 pixels with alpha greater than zero, 118,453
partial and 31,055 opaque; its bounding box is (33,38)–(2525,1427). There
are zero nonzero-RGB pixels at alpha zero and zero pixels with any RGB channel
above alpha plus one byte. Composition changed 141,044 RGB pixels, all under
nonzero plane alpha; native and final alpha were 255 everywhere. Maximum
per-channel residual from premultiplied composition was 0.498 byte before
rounding. The saved preview visibly contains normal HUD and text over the
scene. Skyrim was then closed and the normal INI restored. This extends the
single-frame evidence with an independent baseline and route provenance;
continuous route coverage, excluded menus, FG input copies, camera constants
and generated output remain unverified.
