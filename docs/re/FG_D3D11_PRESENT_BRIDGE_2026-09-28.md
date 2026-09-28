# D3D11 colour to D3D12 lower Present: offline proof

The V5.4 live trace establishes a D3D11-facing ENB/ReShade/Skyrim swap
contract. The new `FgD3D11PresentBridge` isolates one physical part of the
future facade without changing the installed 0.1.115 game path.

For a hidden WARP flip-discard chain, it creates display-sized D3D11 render
textures on the original D3D11 device. It copies the current texture to an NT
shared D3D11/D3D12 surface, waits for the separate producer and consumer
fences, then copies that colour into the current D3D12 lower backbuffer. It
transitions both resources into and out of copy states, waits for a D3D12
completion fence, and forwards exactly one real Present. A TEST Present does
not consume the prepared frame. A second real Present without a fresh copy is
rejected. On uncertain GPU progress, the bridge becomes unusable and retains
its device, swap, surfaces and submitted command objects for process lifetime.

The WARP integration test read back the lower D3D12 backbuffer before Present
and matched the D3D11 source colour on two successive swap indices. It also
checked that each virtual render texture belongs to the original D3D11 device.
Debug and Release builds passed **52/52** CTest groups each. The Debug run
also exposed an older coordinator test comparing string-literal pointers;
that test now compares the call text through `std::string_view`.
This is a same-adapter pixel-transfer and ordering result, not a DLSS-G or FSR
generated-frame result. No Skyrim process was started for this test.

The component is intentionally **not installed**. The bridge itself is not an
`IDXGISwapChain` COM facade and currently blocks the CPU for completion. Its
resize transaction now prepares replacement D3D11 buffers before forwarding
an explicit-size lower ResizeBuffers/1 and rejects held virtual buffers.
The subsequent source-only COM facade and its WARP result are recorded in
`docs/re/FG_D3D11_COM_FACADE_2026-09-28.md`. Wrapper-chain testing and device
recovery remain before any installed replacement. FG On and generated output
in Skyrim remain **NOT RUN**.
