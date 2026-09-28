# D3D11-facing FG swap facade: offline WARP result

**Review 27 correction (2026-09-28):** The facade's earlier colour assertions
read FLIP_DISCARD buffers after Present and were not a valid pixel oracle.
Those assertions were removed. The separate bridge WARP regression now reads
the lower destination before Present while a single cached D3D11 buffer zero
is rendered across two- and three-buffer rotations. A follow-up test-only
lower-swap observer now reads four distinct facade frames before the real
lower Present, including Present1. Facade COM, Present and resize results
remain valid; 100 resize/present cycles passed. See `FG_REVIEW_27_TRIAGE.md`.

The live V5.4 trace found that the ReShade nested and ENB outer swaps expose
`IDXGISwapChain1/3/4`, return the original D3D11 device, and reject D3D12
`GetDevice`. `FgD3D11SwapFacade` now implements those COM interfaces in a
source-only, hidden-window WARP test. It owns the D3D11 colour buffers from
`FgD3D11PresentBridge` and a D3D12 lower swap. `GetDevice` queries only the
original D3D11 device; `GetBuffer` queries only the corresponding D3D11
texture. The lower D3D12 device and backbuffers are not returned to callers.

The test checked stable COM identity across SwapChain1/3/4, D3D11 device and
buffer identity, `E_NOINTERFACE` for D3D12 queries, TEST Present, real Present
and Present1 colour readback, held-buffer resize rejection, invalid-count
rejection without changing the lower size, ResizeBuffers and ResizeBuffers1,
and twelve more resize/present cycles. Both Debug and Release builds passed
**52/52** CTest groups. This proves the isolated COM/pixel transaction on
WARP, not compatibility with the live ENB/ReShade wrapper chain.

The facade is not connected to Skyrim. A subsequent standalone probe also
passed a real colour pixel through a Streamline FG-Off lower swap; see
`docs/re/FG_STREAMLINE_FACADE_OFF_2026-09-28.md`. It still uses CPU waits for
the offline colour transfer. Zero width or height now resolves from the HWND
client extent, with WARP and Streamline FG-Off tests; non-HWND swaps and
recovery after device removal are not implemented.
Private-data and parent calls currently forward to the lower swap and need
wrapper-chain validation, especially when a caller stores the facade itself
as private data. No provider inputs are tagged, no generated frame is
requested, and no FG-On output has been observed. The installed 0.1.115 DLL
remains unchanged. The next source task is a bounded wrapper-chain harness
with the real ENB/ReShade interface order and safe zero-size resize and
private-data lifetime behavior before considering a game-facing replacement.
