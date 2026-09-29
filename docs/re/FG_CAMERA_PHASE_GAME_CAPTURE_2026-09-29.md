# FG camera phase comparison in Skyrim, 2026-09-29

The user started the installed 0.1.125 diagnostic build and loaded a save.
Process 27624 sampled the exact 720-byte game D3D11 camera buffer at
MenuDisplay and at RazKolbas's game-facing pre-Present observer on world
frames 9545 and 9546. It also saved a third world sample at frame 9665 and
prepared 1485x835 colour, RG16F motion and R32F depth on the first two
frames. The log reports `menuDisplayEqual=true` for both same-frame pairs
and `paired pre-Present samples=2/2`. The complete five-camera-file bundle
and two complete three-guide-file bundles are preserved outside Git under
`C:/Users/user/Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/`
in the PID-27624 directories named by those frames. Independent SHA-256 and
size checks matched all 11 raw files to their manifests.

The 720 bytes at MenuDisplay and pre-Present are **byte-identical** on each
of frames 9545 and 9546. Frame 9546's previous view-projection and previous
position are byte-identical to frame 9545's current values. The view basis
determinant is approximately -1.0000001 on both; the projection-derived near
plane is approximately 15 and far plane approximately 353468. This supports
the same-camera phase relation for these two real world frames. It does not
show a CPU producer interception, establish every frame's freshness, or
cover Streamline's eventual lower D3D12 Present under ENB/ReShade.

The prepared guides are finite. Position-aware reprojection using each
captured depth image predicts the RG16F motion as previous UV minus current
UV on a stride-eight grid. Correlation by axis is approximately
`(0.999957,0.999962)` on frame 9545 and `(0.999992,0.999993)` on frame
9546; UV RMSE is below `6.3e-8` per axis. This independently repeats the
static-scene guide convention measured in the earlier 0.1.124 run. No
moving-object or camera-cut sequence was sampled.

The assistant closed the game after capture, restored the normal INI with
SHA-256 `e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`,
and verified all eight installed MO2 payload hashes against the updated
manifest. The one-shot probe is off for subsequent launches. FG-On, generated
frames, Streamline token/Reflex alignment and real input-resource retirement
are still **NOT RUN in Skyrim**.
