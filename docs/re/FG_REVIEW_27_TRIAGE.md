# FG changes review 27: source audit and disposition

The supplied `RazKolbas_FG_Changes_Review_27.zip` has SHA-256
`cf19017a1be6cf3c412a82372c7650a63d7ce4e9c817aeea0b5bf2740e5779fb`.
It reviewed an older source snapshot (`19d86e9`). The archive is preserved
outside Git. Its synthetic witnesses and source observations are review
evidence, not Windows, GPU, Streamline or Skyrim test results. The current
checkout at review start was `c00e1b8`.

The central finding was confirmed with a new WARP regression: caching the
game-facing D3D11 buffer zero, then presenting twice, read stale colour on
the second physical D3D12 index. The test failed before the fix. The bridge
now owns one stable D3D11 render texture and per-physical-buffer shared
transfer surfaces. Each `copyToCurrent` reads logical buffer zero and writes
the current lower D3D12 backbuffer. Valid game-facing `GetBuffer` indices
alias the same D3D11 texture, so a caller using the rotating index still
gets the current logical source. WARP tests check distinct frame colours
before Present over eight frames for both two- and three-buffer chains.

The review correctly identified the post-Present FLIP_DISCARD test oracle as
invalid. The facade integration test no longer reads a presented lower
buffer. The standalone Streamline FG-Off probe no longer reports a pixel
readback as validation. Its proxy/native identity, Present, state and resize
checks remain. A full facade pixel test using an owned pre-Present snapshot
is still needed; the bridge WARP test proves the copy path in isolation.

Malformed Present/Present1 shapes now fail without consuming a prepared
frame; the WARP test verifies a valid retry. Creation now rejects deferred
D3D11 contexts and non-DIRECT D3D12 queues, with WARP negative tests.
The resize test now checks direct buffer refs and a context-bound RTV after
both the buffer and caller's view pointers are released. The context binding
is detected explicitly. A separate attempted test established that a held
**unbound** RTV can escape the texture AddRef count check; that remains a
production blocker. The AddRef count check is an offline fail-closed
heuristic, not proof of complete production ownership or rollback after
every lower-chain failure.

The review's zero-size HWND resize concern was already addressed in
`c00e1b8`: WARP and a separate Streamline FG-Off probe measured successful
window-derived extents. No further change was needed for that item. COM
private-data and parent lifetime, full ENB/ReShade wrapper ownership,
asynchronous transfer, FG-On tagging/pacing and generated-frame retirement
remain open. This source-only change was not installed into the V5.4 modlist.
Skyrim runtime verification is **NOT RUN** for this review.

Verification: the cached-buffer regression failed before the source fix on
its second frame, then Debug and Release builds passed all 52 CTest groups.
The standalone Streamline 2.14.1 FG-Off facade probe exited 0 on RTX 4080
SUPER after the oracle removal, including Present(TEST), real Present,
state, explicit and zero-size resize. It did not enable FG or inspect a
generated frame.
