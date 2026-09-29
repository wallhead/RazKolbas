# Paired FG camera, guides and UI capture, 2026-09-29

The user started the installed 0.1.124 diagnostic build, loaded a save and
reported completion. Process 20860 captured the exact 720-byte D3D11 camera
buffer at world frames 103469, 103470 and 103589; prepared 1485x835 RGBA8
colour, RG16F motion and R32F depth at the consecutive frames; and the
2560x1440 direct UI plane at frame 103469. The game log records the two guide
captures and `consecutive guide bundles complete=true`. It records the UI
route as `candidate=complete route=complete nativeBeforeComposite=unchanged
targetSame=true generationSame=true finalRtvFormat=28`. The captures are
preserved outside Git under
`C:/Users/user/Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/`
in the PID-20860 directories named by those frame numbers. Independent
SHA-256 checks verified all 13 raw files against their complete manifests.

Frame 103470's previous view-projection and previous position are
byte-identical to frame 103469's current values. The current-position delta
is `(0,-0.0009765625,+0.00341796875)`. Both frames use the same unjittered
projection. Its forward-depth form gives near approximately 15, far
approximately 353468, vertical FOV approximately 1.10110 radians and aspect
approximately 1.77844. The view basis determinant is approximately -1.0000001;
the source-only camera calibration retains its rows as right, up and forward
instead of changing handedness.

For each pixel, the analysis used the captured depth as current clip Z,
current top-left UV as `(x+0.5)/1485,(y+0.5)/835`, and the measured matrices
to project current clip into previous clip:

```
previousVP * Translate(currentPosition - previousPosition) * inverseCurrentVP
```

The captured motion vector is **previous top-left UV minus current top-left
UV**. An independent full-frame calculation for frame 103470 found Pearson
correlation `(0.99999508,0.99999752)` for X/Y and UV RMSE
`(6.13e-8,7.55e-8)`, less than 0.0001 render pixel RMS. Frame 103469 also
matched, with R-squared `(0.999906,0.999888)` and UV RMSE
`(3.83e-8,6.92e-8)`. Omitting the camera-position delta worsened the
frame-103470 RMSE to `(2.38e-6,1.15e-5)`, about 39 and 152 times larger.
This is strong evidence that the captured RG16F guide uses normalized UV
motion, includes camera motion, and pairs with the forward depth. For this
measured guide, the pinned Streamline 2.14.1 guide's normalized-vector
example supports `mvecScale={1,1}`, `depthInverted=false` and
`cameraMotionIncluded=true` as candidates. The sampled view was a static
door scene; moving-object vectors and other routes remain unverified.

The native UI-plane manifest and pixels support the separate UI contract at
frame 103469. The scene at the UI boundary and the native image immediately
before composition are byte-identical. Of 3,686,400 display pixels, 149,359
have nonzero UI alpha, including 118,322 partial-alpha pixels. No RGB appears
where alpha is zero, and no RGB component exceeds alpha. The final image
matches byte-rounded premultiplied blending over the HUD-free scene at every
pixel; 140,908 RGB pixels change, all under nonzero UI alpha. This is one
valid native UI plane, not continuous UI tagging or generated-frame UI proof.

After capture, the game was closed and the normal INI restored to SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`.
All eight installed MO2 payloads match the updated manifest. The one-shot
probes are off for subsequent launches. There was no FG-On submission or
generated Skyrim frame in this run.

Source-only `deriveFgCameraCalibration` now extracts the measured world axes,
near/far, both projection FOV axes and aspect ratio with strict projection,
view-basis and finite-value guards. Its synthetic tests first failed while
the API was absent; Debug and Release then passed 57/57 CTest groups each.
It is not wired to live FG. The 0.1.124 DLL used for this run predates the
helper; the later 0.1.125 diagnostic DLL includes it without invoking it.
The SDK does not
specify which FOV axis its generic `cameraFOV` field expects, so the adapter
keeps vertical and horizontal values separate. Same-frame jitter and reset,
pre-Present camera freshness, dynamic-object vectors, D3D11-to-D3D12
resource lifetime, Streamline token/Reflex alignment and generated-frame
appearance still require validation before a game FG-On claim.
