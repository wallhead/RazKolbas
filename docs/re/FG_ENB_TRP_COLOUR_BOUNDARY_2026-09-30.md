# FG final-colour ownership: ENB RE and TRP reference

## Decision after the 0.1.144 game trial

The user saw the same yellow image and approximately 50 versus 60 steady FPS
with RazKolbas's private FG-Off D3D12 presentation active. The queued copy
removed two CPU waits, passed exact-wrapper offline probes, and still failed
the game visual/performance comparison. Keep `ProbeFgPrivateSwapOff=false` in
the installed mod. This is a presentation/effect-ownership problem until a
measured trace identifies a narrower cause; do not add a tint correction or
enable game FG through the current private path.

## Exact ENB 0.505 binary contract

The installed ENB `d3d11.dll` SHA-256 is
`35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3`.
Ghidra 12.1.3 decompiled its Present function at RVA `0x6c540`; Capstone
5.0.7 independently decoded the same bytes. With `DXGI_PRESENT_TEST`, the
function jumps directly to its nested swap's vtable slot `+0x40`. On a real
Present it calls `0x18004b060` first, optionally adjusts sync interval,
clamps it to four, then calls that same nested Present. Ghidra shows
`0x18004b060` conditionally calls several ENB processing functions ending
with `0x18004a120` before the nested Present. The output files and Ghidra
project are ignored under `artifacts/local/fg-phase-2026-09-30/`:
`enb-present-capstone.txt`, `enb-present-ghidra.txt`,
`enb-prepresent-ghidra.txt`, and `ghidra-enb-present/`.

This establishes call order, **not** which final D3D11 texture carries the
ENB/ReShade-composed pixels, whether a view uses sRGB conversion, or whether
the processing condition is identical between native and private presentation.
Those unknowns require a bounded resource/colour trace, not an assumed shader
or gamma patch.

## Working TRP architecture versus this route

The locally fetched Theo's Render Pipeline main is
`5efa032f35ec66e8a0360e99d6f59a5c122ced70`. Its relevant
`GameSwapChain`, `NvidiaHostStartup`, `SourceNativeUI`, and
`SourceDLSSGSwapChain` files are unchanged from the local previously pinned
`423869f06ebef17f7cee51d7f1ce753b2cc8ac4e`. The source supplies a
stable game-facing D3D11 colour resource from the outer `GetBuffer`, completes
its ENB/UI/effect work at `BeforeGameSwapChainPresent`, then copies the
finished image into a rotating presentation buffer. It also integrates
ReShade explicitly rather than relying only on an automatic inner wrapper.
This is a useful ownership contract for RazKolbas, not code to transplant
without adapting its runtime and resource boundaries.

RazKolbas currently returns a hidden auxiliary **discard swap-chain** back
buffer through `FgD3D11SwapFacade::GetBuffer`, nested under the installed
ReShade/ENB wrappers, and copies that source when the facade's `Present` is
called. The auxiliary swap was introduced because the installed ReShade 6.8
failed to create an sRGB render-target view on an ordinary typed UNORM
D3D11 texture; a native swap back buffer passed both ordinary and sRGB view
creation. This means a straight replacement with TRP's standalone stable
texture would regress the proven ReShade view contract unless RazKolbas also
owns the corresponding ReShade/effect handoff. The exact game trial now shows
that preserving wrapper creation and successful Presents alone is insufficient.

## Exact-wrapper colour and ReShade-runtime probe

The Release private-route reproduction ran 240 frames and resize with the
installed ENB 0.505 and ReShade 6.8, both without and with Steam overlay;
both runs exited zero. The upper ReShade `GetBuffer(0)` and inner RazKolbas
facade `GetBuffer(0)` have the same canonical `IUnknown` identity, format
`R8G8B8A8_UNORM`, bind flags `0x28`, and misc flags zero. A D3D11 staging
readback of pixel `(0,0)` was `(255,0,0,255)` at both interfaces before and
after the first real Present. The bridge reported `copy-prepared-queued` and
no failure. Outputs are ignored under `artifacts/local/fg-phase-2026-09-30/`
as `colour-handoff-no-steam.txt` and `colour-handoff-steam.txt`. This rejects
a simple upper-versus-inner D3D11 texture mismatch in the synthetic probe;
its solid red image has no Skyrim scene or measured final ENB/ReShade pixels.

The exact-wrapper `ReShade.log` for that probe shows a ReShade D3D12 runtime
created for the private lower swap and a second D3D11 runtime created for the
auxiliary facade swap. They load `ReShade.ini` and `ReShade2.ini` respectively.
Both configs point to `True AE V5.ini`, whose enabled techniques are
AmbientLight, Colourfulness, and the PD80 bonus LUT. The log also shows shader
compilation for both runtimes. Duplicate effect execution on the final image
is therefore a **plausible hypothesis**, not a measured game fact: the probe
did not capture the output pixel after each runtime, and the user's Skyrim
ReShade log was not retained before this probe overwrote it. Do not alter the
user's preset to mask the colour error.

TRP avoids relying on two automatic ReShade wrappers: its `ReShadeIntegration`
captures the native D3D12 output device at host creation and explicitly runs
one source-frame D3D11 effect runtime. Its game-facing colour and native UI
boundaries are separate. RazKolbas should first establish equivalent single
effect ownership and the sRGB RTV resource contract in an offline reproduction,
then capture a bounded game colour stage. An automatic second ReShade runtime
is a specific lead to test, not yet evidence to enable FG. Actual game FG-On
remains NOT RUN.
