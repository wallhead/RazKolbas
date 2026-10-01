# FG guides from published SR inputs, 2026-10-01

The 0.1.150 game run confirmed sampled raw world-guide snapshots and a
same-frame camera candidate with normal image/UI/FPS. Raw Skyrim depth is
typeless R24, while the working DLSS SR path already converts its own depth
to R32_FLOAT and prepares RG16F motion before evaluation. A later FG bridge
must receive those converted textures rather than treating the raw depth
copy as an FG-ready D3D12 resource.

Source 0.1.151 exposes the converted depth/motion pair only for the exact
successfully published, current-generation MenuDisplay SR token. It rejects
pre-publication, stale, wrong-phase and wrong-format pairs. The game caller
immediately copies both prepared guides into the existing independent
world-guide latch on the same D3D11 immediate context, alongside the
HUD-free pre-UI display. It records the token only after SR publication
succeeds. No FG-ready stamp, Streamline token, D3D12 lease or provider
submission is created. The capture remains deliberately sparse: the first
16 eligible world frames and every 600th frame thereafter.

The new WARP test first failed to compile against the absent published-guide
API. It now verifies no pair before publication, exact token/format/extent
after publication, wrong-generation refusal, and that the world latch copies
the converted textures to different COM objects. Full Release build/CTest
passed **69/69** and Debug passed **64/64**. A final Release rebuild after
removing a shadowed-local warning passed **69/69**.

The Release package was staged at
`artifacts/local/stage-v54-fg-converted-guides-0151` while the 0.1.150 game
was still running; **no installed file was changed**. The stage manifest
verified **14/14** payloads. Staged DLL SHA-256:
`e8231e791dd27890519b55e72319d5dd6102b13954ddc2410c927e6d67a5ffb4`;
INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`00ac1c5b4988368247a3e0353376d753a8d93b40dfc688685bad7025acda613e`.
The 0.1.150 package verified **14/14** and its INI matched the stage.
After the user quit Skyrim through its menu and the process exited, the old
DLL/INI/manifest were backed up under
`artifacts/local/backup-v54-before-0151`. Only the DLL and manifest were
replaced. The installed 0.1.151 package verified **14/14** and the INI stayed
byte-identical. **0.1.151 Skyrim runtime and FG-On/generated frames: NOT RUN.**
The next user-started FG-Off save load must check converted-guide capture at
the real Present and image/UI/FPS. Continuous input pooling, transparent
native UI, D3D11-to-D3D12 lease and provider retirement remain required
before enabling game FG.
