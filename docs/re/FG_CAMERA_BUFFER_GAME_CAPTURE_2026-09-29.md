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

The game was closed after capture. The installed normal INI was restored
from the ignored 0.1.123 backup; its SHA-256 is
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`.
All eight installed MO2 payload hashes then matched the updated install
manifest. The probe is off for subsequent starts. This run did not enable
FG, check generated-frame quality, or obtain a user visual assessment.

Next, use these measured offsets in an exact-version camera decoder with
numeric and frame-stamp guards, then verify continuously at the intended
pre-Present phase. The remaining FG integration must still bind the real
scene/depth/motion and separate native UI inputs to the Streamline lease,
check GPU completion and token lifetime, and finally run a user-started
FG-On game test with motion, UI, ENB/ReShade and recovery paths.
