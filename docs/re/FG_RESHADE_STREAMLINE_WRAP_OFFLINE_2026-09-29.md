# ReShade over Streamline facade: standalone FG-Off and FG-On

This is an offline native probe on the local RTX 4080 SUPER, not a Skyrim
runtime result. The probe loaded the exact installed V5.4 ReShade 6.8
`dxgi.dll` (SHA-256
`b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`)
in a separate process. Its `CreateDXGIFactory1` returned the known factory
vtable RVA `0x3ee350` and `CreateSwapChain` method RVA `0x14a4f0`. The
factory's `[this+8]` native delegate used the pinned System32 DXGI vtable
RVA `0xa1428` and SHA-256
`25678116473558a56524b5b391f66f5c81febe8b2c463422f35807f77f3207c1`.

After `slInit` and `slUpgradeInterface(factory)`, delegate vtable slot 10
pointed **into Streamline's interposer**, not System32 DXGI. The exact
`sl.interposer.dll` was 652,928 bytes, SHA-256
`8c87c9499461da561edd529aa9bf7831d67d7b94ebb1c1a5ed54ef4934e1ea4c`;
the slot method was RVA `0x26510`. Capstone 5 disassembly of that exact file
(`artifacts/local/sl-interposer-factory-26510-full-20260929.json`, ignored)
shows the method saving the caller's output pointer at `0x26537`, the factory
at `0x2653a`, and device/description, then calling a stored downstream
function with those four arguments at `0x265be–0x265cd` or
`0x2667c–0x26689`, depending on its internal branch. Later code processes
the returned swap. This supports chaining the existing hook owner; it does
not justify overwriting an unknown owner.

The probe temporarily replaced only that pinned slot with a callback that
returned a D3D11-facing `FgD3D11SwapFacade` for the selected factory/device,
and chained the saved Streamline method for other calls. ReShade then built
its own upper wrapper from the facade. In the FG-Off run the wrapper factory
returned `S_OK` with exactly one substitution; `GetDevice(D3D11)` and
`GetBuffer` succeeded; D3D11 red, green, and blue clears reached successive
D3D12 lower backbuffers; explicit and zero-size resize succeeded; and
`slShutdown` succeeded. Raw output is in ignored
`artifacts/local/fg-reshade-facade-probe.stdout.txt`.

In the focused FG-On run, ReShade again returned its wrapper with exactly
one substitution. Eight real-frame submissions each returned `S_OK` from
upper Present. Streamline reported status 0 and
`numFramesActuallyPresented=2` for each frame. Its input-processing fence
was present and advanced through values 1–8; the final Off/drain Present and
`slShutdown` succeeded. Raw output is in ignored
`artifacts/local/fg-reshade-facade-on-focused.txt`. The earlier unfocused
attempt reported only one actual presentation and no completion value; the
focused result establishes the controlled window precondition.

The probe uses synthetic guide textures and a synthetic D3D11 colour source.
It does not include Skyrim's live resources, ENB's upper wrapper, continuous
same-frame camera/UI capture, production Streamline startup, or in-game
resource retirement. Therefore **Skyrim FG-On has not run**. The production
0.1.128 pass-through hook currently expects the native System32 DXGI method
at slot 10 and does not load Streamline; an FG-On path must explicitly
recognize and chain this pinned interposer-owned slot.
