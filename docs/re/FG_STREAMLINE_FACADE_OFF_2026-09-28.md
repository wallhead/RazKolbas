# Streamline 2.14.1 D3D11 facade over D3D12 lower swap: FG-Off probe

The standalone probe ran on the local RTX 4080 SUPER (vendor `0x10de`,
device `0x2702`) with the hash-pinned Streamline 2.14.1 headers and signed
runtime. It did not start Skyrim and did not change the V5.4 MO2 installation.

The first facade attempt failed its exact-device check. Streamline's command
queue reports the **proxy** D3D12 device, while its lower swap reports the
**native** D3D12 device. Both `GetDevice` calls returned `S_OK`, but their
COM identities differ. `slGetNativeInterface(proxy device)` returned a native
interface whose identity exactly matched the lower swap's device. The probe
now verifies this relation and passes the proxy and the verified native
identity separately. The bridge still requires the queue to report the exact
proxy identity and the lower swap to report the exact verified native
identity; same-adapter LUID alone is insufficient.

With DLSS-G explicitly Off, facade creation succeeded. Its game-facing
`GetDevice(ID3D11Device)` returned `S_OK` and the original D3D11 identity;
`GetDevice(ID3D12Device)` returned `E_NOINTERFACE`. A virtual D3D11 buffer
accepted a colour clear. Facade `Present(TEST)`, real `Present`, and
`ResizeBuffers(128x80)` returned `S_OK`. A fenced readback of the **lower
D3D12 backbuffer** after Present was RGBA `(64,127,191,255)`, matching the
cleared `(0.25,0.5,0.75,1)` source within UNORM rounding. Three additional
fresh-process runs repeated the same pixel and resize result, all exit 0.
The earlier five fresh-process runs without readback also exited 0.
The final probe queried `slDLSSGGetState=0` and reported
`actualPresented=1`, `maxExtra=1`; the one presented frame was the real frame
while mode remained Off.

This verifies one real FG-Off pixel path through Streamline's proxied D3D12
queue and lower swap. It does **not** verify FG On, generated frames, resource
tags, Reflex pacing, Skyrim wrapper compatibility, UI separation, or
performance. The bridge currently waits on the CPU for every transfer.
The next gate is a guarded ENB/ReShade wrapper-chain integration and live
D3D11 facade ownership check before enabling a provider. Generated output
in Skyrim remains **NOT RUN**.
