# Native UI candidate paired with real Present, 2026-10-01

Source 0.1.152 extends the existing D3D11 UI-plane snapshot helper so a
HUD-free native image can be copied at the MenuDisplay boundary before a real
DXGI Present token exists. The early snapshot has neither token nor reset
epoch. At pre-Present, `finish` requires the same source frame, generation and
display extent, and a nonzero real Present token and epoch. It copies the
separate premultiplied UI and final composed colour, then stamps all three
textures with that epoch. An old or mixed provisional identity is rejected.

The Skyrim hook now attempts this **once** in an ordinary loaded-world DLSS
frame when the existing FG phase diagnostic is enabled. It reuses the native
UI redirector and premultiplied compositor previously validated by the
0.1.124 one-frame capture. At the actual pre-Present callback, it accepts a
candidate only if the UI route reports complete, the native target and
generation still match, the world-guide packet matches, and the real boundary
is phase-ready. It holds the three D3D11 snapshots through this observation
and logs the paired identity. A failed check logs a rejection and does not
label the resources FG-ready. The UI plane is disarmed after the frame.

This is a one-frame **FG-Off** capture, not continuous UI routing, a D3D12
five-input lease, a Streamline token or an FG submission. The extra textures
can be allocated/copied on the sampled frame, so that single frame is not a
performance measurement. The already observed 0.1.124 direct UI image was
full-resolution and premultiplied; the new combined packet still requires
Skyrim runtime verification.

The WARP test for late binding first failed at the pre-UI capture because the
old helper required a token. Release build/CTest passed **69/69** and Debug
passed **64/64** after the change. **0.1.152 game runtime and FG-On/generated
frames: NOT RUN.**

The 0.1.152 Release package was staged at
`artifacts/local/stage-v54-fg-native-ui-0152` with the installed INI as its
source. The stage and previous installed package each verified **14/14**
manifest entries. Skyrim was absent from the process list before replacement.
The old DLL/INI/manifest were copied to ignored
`artifacts/local/backup-v54-before-0152`; only the DLL and manifest were
replaced. The installed package verified **14/14**, and its INI stayed
byte-identical. Staged/installed DLL SHA-256:
`3d49f4001c330738c2954d0d92470f181dcc72fcdf34032a836ce594a3d14614`;
INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`742456247b20796a3dc06c5b4e9d0e43c6997691cd1cd521a1d0287ba06c2667`.
At installation time, game runtime and generated frames were **NOT RUN**.

The user then launched 0.1.152 through MO2 with FG Off, loaded the same save
and reported normal world image, native UI and steady FPS. The session
beginning at the 12:32:49 bootstrap marker armed one loaded-world UI frame
at 12:35:57. Its real pre-Present boundary paired source/present **8850**,
generation **1**, epoch **1**, a complete UI route and a matching converted
guide packet. HUD-free colour, transparent UI and final colour were all
captured at **2560x1440**. The snapshot contains one arm, one accepted pair,
zero UI-candidate rejections, 19 sampled converted-guide/camera-candidate
packets, zero SR suspensions and zero sampled nonzero Present failures.
Ignored live log snapshot:
`artifacts/local/fg-native-ui-0152-live-snapshot.log`, 24,890,903 bytes,
SHA-256 `7852523d78c56ef0423be613ca7cced1e18417b15957972d8b7861a79ab72377`.
This proves the **sampled pairing and visual regression check passed in the
game**. It does not verify continuous UI routing, the eventual D3D12 copy,
provider consumption, generated frames or loading-screen quality.
