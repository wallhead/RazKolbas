# Skyrim 1.6.1170 per-frame buffer static trace, 2026-09-29

The current `DynamicShaderFrameGen` reference names Address Library ID 411384
for a per-frame D3D11 buffer. The exact Skyrim 1.6.1170 Address Library maps
that ID to game RVA `0x3288788`. This was cross-checked against other known
entries, including the already runtime-verified jitter object at `0x328cc20`.
The candidate is a game global pointer, not a verified camera field layout.

Capstone 5.0.7 decoded the hash-matched, previously extracted Skyrim `.text`
image (`text_1000.bin`, 24,435,000 bytes, SHA-256 beginning `75105f3a`).
At game RVA `0xe45c26`, a RIP-relative load resolves to `0x3288788` and
places the loaded pointer in RDX. The nearby call through D3D11 context
vtable offset `+0x70` is `Map` (slot 14). The routine copies five 0x80-byte
blocks and one 0x50-byte tail from its stack into the mapped destination,
0x2d0 bytes total. At RVA `0xe45d03` it loads the same global for a call
through vtable offset `+0x78`, `Unmap` (slot 15). Other xrefs include resource
release sites at `0xe42ff9` and `0xe4d59b`. The locally ignored scripts
`artifacts/local/find-fg-camera-xrefs.py` and
`artifacts/local/disasm-skyrim-camera.py` reproduce this bounded static trace.

This identifies an exact game producer for a per-frame mapped buffer. It does
not establish matrix offsets, row/column convention, current/previous frame
semantics, or that all writes come from this single routine. The reference
project itself observes multiple Map/Unmap writes in one frame and warns that
an old post-Unmap copy can miss or mix camera and impostor updates. Do not
copy its field offsets or fabricate a Streamline token from this static trace.
The next game-facing diagnostic needs a bounded, frame-stamped observation of
actual writes at or before Unmap, plus phase and callsite identity, before any
camera record can pass FG admission. No game camera probe was run here.

Installed version 0.1.123 adds a separate, default-off read-only staging probe of the
exact buffer after the producer has unmapped it. It records three real-frame
byte images, N, N+1 and N+120 or later, for bounded differential analysis.
This cannot reveal every writer or prove the last Map/Unmap callsite; a
per-write trace may still be needed. Debug and Release passed 55/55 CTest
groups; the game probe is not yet run. Its one-shot INI is enabled in the
isolated V5.4 MO2 mod, with the normal INI preserved in the local backup.
