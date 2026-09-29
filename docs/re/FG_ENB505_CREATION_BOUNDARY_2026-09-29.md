# ENB 0.505 creation path above the ReShade facade

Input is the installed V5.4 ENB `d3d11.dll`, SHA-256
`35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3`,
4,553,216 bytes, PE image size `0xa92000`. Capstone 5 decoded the exact
`D3D11CreateDeviceAndSwapChain` export at RVA `0x5e4b0` into ignored
`artifacts/local/enb505-factory-create-5e4b0-20260929.json`. The bounded
window ends mid-function; the instruction ranges below decoded without
skipped bytes. Addresses are RVAs, not runtime addresses.

At `0x5e520–0x5e575`, the ENB export assembles the downstream D3D11
creation call from its caller arguments. It calls a stored function pointer
at `0x5e575` and checks the HRESULT before continuing. After a successful
return, it requires non-null output pointers and a returned swap. At
`0x5e5c4–0x5e5db` it reads that returned swap and calls vtable offset
`0x60`, `IDXGISwapChain::GetDesc`. At `0x5e609–0x5e61d`, it calls vtable
offset `0x48`, `IDXGISwapChain::GetBuffer`, on the same returned swap. A
later branch also uses the returned swap's `GetBuffer` at `0x5e819–0x5e832`
and invokes another interface method at offset `0x50` on its buffer at
`0x5e856`. The existing live trace independently observed ENB's exact
early `GetDesc` and `GetBuffer` callers and the outer ENB swap wrapper.

This narrows the required lower facade contract: ReShade must hand ENB a
D3D11-facing swap whose description and buffer zero are valid immediately
after D3D11 device creation. The standalone ReShade-over-Streamline facade
probe already passed `GetDesc`, `GetBuffer`, colour and resize beneath
ReShade, but it did **not** execute ENB. The ENB static trace supports the
interface order; only a future bundled Skyrim FG run can establish the
complete live chain and visual result.
