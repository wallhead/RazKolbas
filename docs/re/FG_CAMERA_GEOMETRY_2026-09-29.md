# FG camera geometry and paired-guide probe, 2026-09-29

The pinned local Streamline 2.14.1 `sl_consts.h` states that common-constant
matrices are row-major and unjittered. It defines `clipToPrevClip` as the
current-clip to previous-clip transform. The sampled Skyrim 1.6.1170 buffer
contains rotation-only view/view-projection matrices and separate current and
previous world positions. A temporal transform built from only the two VP
matrices would omit camera translation.

`deriveFgCameraTransforms` therefore computes the following *candidate*
column-vector transform from the verified sampled fields, then transposes it
for Streamline's row-vector matrix storage:

```
previousVP * Translate(currentPosition - previousPosition) * inverseCurrentVP
```

It also transposes the observed projection and inverse projection into
`cameraViewToClip` and `clipToCameraView`, and inverts the temporal transform
for `prevClipToClip`. Synthetic tests check that a camera moving from world X=2
to X=5 maps a static current-relative X=1 point to previous-relative X=4,
including the reverse transform. A non-symmetric projection test checks the
row-vector layout. Numeric guards reject nonfinite inputs and singular
temporal transforms. Debug and Release each passed 57/57 CTest groups.

An independent read-only check used captured world frame 34112. The current
and previous camera-position delta was `(0,-0.00048828125,+0.002197265625)`.
For a front-facing current view point `(20,5,100,1)`, direct previous
projection and the derived transform differed by at most `1.9e-5` clip-space
units, consistent with the stored float inverses. This proves the algebra
against one sampled game frame; it does not prove image-space motion-vector
sign, depth interpretation or Streamline visual quality.

The view basis determinant is approximately -1 in all three saved frames.
The local Streamline helper reconstructs up with `cross(forward,right)`, but
that helper is illustrative and its handedness must not be imposed on Skyrim
by flipping forward: the captured projection has `clip.w=view.z`, so positive
view Z is in front. Camera basis, near/far, depth inversion, motion-vector
normalization, camera-motion inclusion, reset/cut state and same-frame jitter
are still admission requirements before a real FG submission. The game's
existing same-frame NGX jitter conversion is an algebraic candidate for
Streamline pixel-space jitter, not yet a stamped FG constant.

Version 0.1.124 extends the default-off camera diagnostic to save prepared
colour, motion and depth bundles on the two consecutive camera-sample frames.
With the separate default-off direct UI-plane probe enabled for the same run,
the logs can correlate world frame, camera bytes, prepared guides and native
pre-Present UI. This diagnostic may stall those frames for readback. It does
not enable FG or alter normal rendering after the bounded capture. Its game
outcome is **NOT RUN** until the owner starts Skyrim.

The eight-payload MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.124-fg-paired.zip`
with SHA-256
`33aa2cec22d1133a0f86c2205e5f61130f44c17222db5ae851d049eb7ee503b4`.
All eight staged payload hashes matched the manifest, and independent ZIP
stream hashing verified exactly those eight entries plus the manifest. The
isolated V5.4 MO2 mod contains Release DLL SHA-256
`903b06f3161bc25348a2ae3c4758e9e1f57bb08affc2348afcd9fe32230bcadf`
and one-shot diagnostic INI SHA-256
`f3f210cce739b382eb81acb1d3333a9fd53c7b408218a53e4224e0c9c4ff2ee2`.
All eight installed payloads match its manifest; the previous DLL, normal INI
and manifest are backed up under ignored
`artifacts/local/mo2-install-backup-0.1.124-2026-09-29`. Skyrim was stopped
during installation and was not started by the assistant.
