# Skyrim 1.6.1170 FG camera buffer game capture, 2026-09-29

The user started the installed 0.1.123 build. Its default-off, one-shot
`Diagnostics.ProbeFgCameraBuffer` switch was enabled for this run. Process
33008 captured three real DLSS world frames, 34111, 34112 and 34231, at
14:42:13–14:42:15. The game log reports a 720-byte D3D11 constant buffer
(`BindFlags=0x4`) at each sample and a complete bundle. Each raw file is 720
bytes; independent SHA-256 checks agree with the complete manifest:

| Frame | SHA-256 |
| --- | --- |
| 34111 | `527b58addfdfb796666075f652605dbe14099aacbc9b5474deaff49fe8aeb73f` |
| 34112 | `8227c374059080219d3040d18090cc171ae4f09a7e34e12b2c94752052e79f7a` |
| 34231 | `b8acb2466a3fc793364ea882564e81b4f675da1433470b89fda3570ae2e2ea8e` |

The captures remain outside Git at
`C:/Users/user/Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/fg-camera-buffer-33008-34111-257306453`.
The raw bytes were interpreted as 180 little-endian float32 values. For all
three frames, row-major 4x4 blocks have these numerical relationships:

| Offset | Observation |
| --- | --- |
| `0x000` | View-like orthogonal matrix, determinant approximately -1; no world-position translation. |
| `0x040` | Projection-like matrix; constant across these frames. |
| `0x080` | Product of `0x040 * 0x000` to under `5e-8` Frobenius residual. |
| `0x0c0` | Byte-identical duplicate of `0x080` in each sample. |
| `0x100` | Previous view-projection candidate: the frame-34112 block is **byte-identical** to frame 34111 at `0x080`. |
| `0x140` | Inverse of `0x040`; identity residual below `9e-8`. |
| `0x180` | Byte-identical duplicate of `0x040`. |
| `0x1c0` | Inverse of `0x000`; identity residual below `1.6e-7`. |
| `0x200` | Inverse of `0x080`; identity residual below `2.1e-7`. |
| `0x240` | Byte-identical duplicate of `0x140`. |
| `0x280` | Current position candidate, three floats; changes by about 0.0023 units over one frame and 0.169 units over 120 frames. |
| `0x290` | Previous position candidate; frame 34112 is byte-identical to frame 34111 at `0x280`. |

`0x100` is **not** a jittered current matrix as the same-frame differences
alone might suggest. The consecutive-frame equality establishes a previous
matrix on the sampled pair. The current view matrix changed by about `5.1e-5`
Frobenius norm on the adjacent frames and `0.0032` over 120 frames, while the
projection remained identical. Those changes and the previous-state equality
make a static or dummy buffer implausible. They do not establish every
Map/Unmap writer, buffer freshness at the FG submission phase, or what all
fields after `0x2a0` mean. The sample is from one outdoor world route.

An independent read-only cross-check reproduced the hashes, matrix residuals
and consecutive-frame equality. It also identified a crucial conversion
limit: the view matrices here have no world-position translation. A temporal
clip transform made only from previous VP times inverse current VP would
omit the difference between `0x280` and `0x290`. The camera basis handedness,
Streamline matrix memory convention, jitter/reset, motion-vector sign/scale,
depth convention and pre-Present token freshness are not established by
this capture. Do not submit guessed constants or infer depth parameters
from the projection matrix alone.

The decoder review caught an initially loose matrix threshold: pairing frame
34111's P/V with frame 34112's VP produces a maximum component error of
`3.33e-5`; pairing frame 34111's VP with frame 34112's inverse VP produces
`3.67e-5`. Both passed the initial `1e-3` threshold, even though each real
sample's own product/inverse error is below `1.3e-7`. The source-only
decoder now uses `1e-6` matrix residual limits and has a regression for
adjacent-frame-scale corruption. Numeric gates alone cannot prove freshness
while the camera is stationary; the caller still needs a real-frame stamp
and phase identity.

The game was closed after capture. The installed normal INI was restored
from the ignored 0.1.123 backup; its SHA-256 is
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`.
All eight installed MO2 payload hashes then matched the updated install
manifest. The probe is off for subsequent starts. This run did not enable
FG, check generated-frame quality, or obtain a user visual assessment.

The version-specific decoder now supplies numeric guards for these measured
offsets. Next, connect it with real-frame stamps and verify freshness
continuously at the intended pre-Present phase. The remaining FG integration
must still bind the real scene/depth/motion and separate native UI inputs to
the Streamline lease,
check GPU completion and token lifetime, and finally run a user-started
FG-On game test with motion, UI, ENB/ReShade and recovery paths.
