# FG world-guide pixel ownership, 2026-10-01

The 0.1.149 user-started run validated same-frame camera candidate binding at
the real DXGI Present. That packet also held depth and motion COM references,
but only the HUD-free colour was a snapshot. A reference prevents texture
destruction; it does not prevent Skyrim or ENB from rewriting its pixels.
Passing those references to a later D3D12/FG consumer would risk mixing
source frames.

Source 0.1.150 makes the world-guide latch allocate independent D3D11 textures
for raw depth, raw motion and pre-UI colour before publishing a packet. It
preserves each guide's format and binding contract, removes sharing flags on
the owned copies, and queues all three `CopyResource` operations on the
verified immediate context. If any allocation fails, no frame is latched.
The copies are still raw-format candidates. The latch does not set FG-ready
stamps, perform depth/motion conversion, transfer to D3D12, assign an SL
token or claim provider/presentation retirement.

The WARP regression first failed against the old latch because the published
depth texture was the same COM object as the mutable source. It now verifies
that changing all three source textures after capture leaves the published
pixels intact. A second WARP case verifies the same behavior for
`R32_TYPELESS` depth and `R16G16_FLOAT` motion, with a depth-stencil view and
shader-resource binding on depth. The earlier rejection cases for stale,
cross-thread and wrong-size frames still pass.

Full Release build/CTest passed **69/69**; Debug passed **64/64**. The
14-file Release stage is
`artifacts/local/stage-v54-fg-guide-snapshots-0150`. With Skyrim closed,
the installed 0.1.149 and staged 0.1.150 packages each verified **14/14**
payload hashes. The previous DLL/INI/manifest are backed up under
`artifacts/local/backup-v54-before-0150`; only DLL and manifest were replaced.
All newly installed 14/14 files verified, and the user INI remains unchanged.
Installed DLL SHA-256:
`f646930eacf3720aa6ad80e0908f5f66ed24ac60a482c7cf4c1e83a4c24d7a53`;
INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`a57e41a1d82fe4531629f725088a90827b9af22fd6bdd5e43bd1571723190c04`.

The user started Skyrim through MO2 with FG Off, loaded the same save, and
reported normal image, UI and steady FPS. The 0.1.150 session begins at the
11:48:40 bootstrap marker. From loaded-world frame 9,323, the log shows
`rawWorldGuides=true`, `cameraProducer=true`, `srJitter=true` and
`cameraCandidate=true`; frame 10,200 still has all four. The captured session
has 19 candidate-true packet lines, no raw-guide capture warning, no SR
suspension and no sampled nonzero Present failure. This verifies that the
actual Skyrim/ENB texture descriptors accepted the new snapshot allocation
and copy calls and that the sampled FG-Off visual/FPS path remained healthy.
It does not independently read back the game's copied pixels or validate a
continuous FG input lease.

Ignored live log snapshot:
`artifacts/local/fg-guide-snapshots-0150-live-snapshot.log`, 24,390,307 bytes,
SHA-256 `b2ec8cdf4a2f3e72f3bda8597e670df6fec163b41ae450cffaf0b158a28b5f31`.
The log includes earlier appended sessions and was copied while Skyrim was
running. **0.1.150 sampled world-guide path: RUN/PASS. FG-On/generated
Skyrim frames: NOT RUN.** Continuous resource pooling, converted guide
semantics, transparent UI and full consumer retirement remain separate
required steps.
