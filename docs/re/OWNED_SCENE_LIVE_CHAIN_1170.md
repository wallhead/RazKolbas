# User-started 0.1.34 factory and swap chain, 20 September 2026

The owner started the installed game through MO2. RazKolbas 0.1.34 logged
`Pre-create factory provenance` before forwarding ENB's
`D3D11CreateDeviceAndSwapChain`. The log gave adapter `0x2552f466ef0`,
factory `0x25529c32130`, vtable owner SHA-256
`059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc`,
file size `5157144`, table RVA `0x3D79D0`, and slot-10 method RVA `0x13A5B0`.
The module is the local `D:/TESV_EX/dxgi.dll` (ReShade). Static PE inspection
confirmed image size `0x51C000`, preferred base `0x180000000`, slot 10's
preferred pointer `0x18013A5B0`, and method prologue
`48 83 EC 38 48 8D 05 15 00 00 00 48 89 44 24 20`.

While the user-started process was running, a bounded read-only
`ReadProcessMemory` inspection verified live table `0x7FFC6B6379D0`, live
method `0x7FFC6B39A5B0`, and identical first 16 method bytes. Slot 15 was
`0x7FFC6B39A680`; no change was made to either slot.

The existing world log identifies ENB swap object `0x2556FF9FF60`. Read-only
inspection found its table at `0x1801A4848` and the pointer at object `+0x28`
to underlying ReShade swap object `0x2552E362C90`. The latter table is
`0x7FFC6B637F90` = ReShade base + `0x3D7F90`; slot 9 `GetBuffer` is
ReShade base + `0x13B460`. Its first 13 bytes are
`48 8B 49 08 48 8B 01 48 FF 60 48 CC CC CC CC CC`: a delegate to its
own underlying object. The observed ENB slot-9 method at RVA `0x5B990`
also delegates to its `+0x28` object. This makes a version-checked factory
slot-10 chain followed by a ReShade swap slot-9 alias route a smaller candidate
than replacing all `IDXGISwapChain4` methods. It is still a candidate: the
exact nested callback behavior and consumer timing need a WARP fixture and
game validation before activation.

The game's immediate context `0x2557B22F370` has ENB vtable at
`0x1801A49F8`. Its slots 8, 33 and 44 point to ENB RVAs `0x5C730`,
`0x68F40` and `0x5D070`, respectively. These are the correct current
downstream methods for the scoped native-UI adapter. Patching system D3D11
methods directly would bypass ENB's wrapper and is not the selected path.

The game reached at least 25,800 world/present observations with Present
HRESULT 0 and no failed Present calls in the captured slice. The 0.1.34 log
still says `no NGX evaluation` while taking full-size input copies at the main
menu. This run measured hook ownership, not DLSS SR, scene reduction, or UI
quality. The assistant did not start, control, or close the game.

Offline inspection of the previously captured decoded game `.text` found two
candidate **game-side** swap `GetBuffer` call sites. At RVA `0xE48F0A`, a
zero-branch call supplies buffer index 0, a texture IID, and a stack output to
vtable offset `0x48`; it then passes the result to `0xE4D750`. At RVA
`0xE4CC84`, a caller obtains a chain from an indexed renderer record at
`global + 0x60 + index*0x50`, supplies index 0, the texture IID, and a stack
output to vtable offset `0x48`. It immediately creates device views for the
returned texture, obtains its descriptor, and updates renderer dimensions.
The corresponding return RVAs are `0xE48F0D` and `0xE4CC87`. This is static
call-shape evidence; the running 0.1.34 build did not record whether either
site passes through the selected ReShade swap instance, nor when ENB caches
views. An alias must not be enabled for these sites solely from the call shape.
