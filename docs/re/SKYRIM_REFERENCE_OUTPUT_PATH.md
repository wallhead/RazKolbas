# Skyrim AIO Build16-Hotfix1 SR entry and output trace

Status: **STATIC_OBSERVED** on 2026-09-20. This is a trace of the supplied
reference, not a claim that RazKolbas displays SR. The input
`SkyrimUpscalerAIOBuild16-Hotfix1/SKSE/Plugins/SkyrimUpscaler.dll` has SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
The supplied `UpscalerBasePlugin/PDPerfPlugin.dll` has SHA-256
`8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
Both were read only, never loaded or installed for this trace. RVAs below
are relative to `SkyrimUpscaler.dll` unless explicitly called game RVAs.

## Entry and destination chain

1. Installer `0x157b60` resolves AE Address Library ID 82084 at `0x158500`
   and adds `0x17a` at `0x15851c` to find the world-draw CALL. In the
   independently decoded 1.6.1170 library this is game RVA `0xfa507a`.
   The callback is reference RVA `0x156830`, and it forwards the original
   game CALL before processing. This matches the live exact-game CALL bytes
   and the pass-through path already used by RazKolbas.
2. The swap-chain factory hook at `0x202b60` constructs an object via
   `0x1ea890`, stores it at global RVA `0x384180` at `0x203282`, returns the
   object to the caller, and retains the real swap chain at object `+0x10`.
   The constructor installs vtable RVA `0x32e8f0`. This is the reference's
   presentation proxy, not a Skyrim engine renderer pointer.
3. The callback reads proxy vtable slot `+0x120` at `0x156938` and keeps its
   returned buffer index in `r12d` at `0x15693f`. It passes
   `proxy + 0x38 + index*0x58` as the second argument to SR wrapper
   `0x1f4ca0` at `0x156d92..0x156dab`. The proxy method at vtable `+0x120`
   is RVA `0x1e9cc0` and delegates to the underlying swap chain's same
   slot, with a conditional index adjustment.
4. `0x1f4ca0` saves that second argument in its caller home slot on entry.
   After the delayed `PDPerfPlugin!EvaluateUpscaler` call at `0x1f5022`,
   an eligible branch reloads the same argument at `0x1f5165`, obtains an RTV
   through helper `0x14d9c0` (whose error string names
   `ImageWrapper::GetRTV`), and invokes shader helper `0x1f34a0` at
   `0x1f51c8`. That helper binds an RTV through D3D11 context vtable
   `+0x108` (`OMSetRenderTargets`) at `0x1f359a` and issues a draw through
   `+0x68` (`Draw`) at `0x1f378b`. Thus the reference has a **post-evaluation
   shader-draw destination** in its proxy's indexed render-target wrapper.
   It is not the previously assumed fourth `CopyResource` at `0x1f5092`:
   that call copies a caller-supplied texture into a private host texture.
5. The proxy vtable's standard `GetBuffer` slot `+0x48` is RVA `0x1ea110`.
   Depending on proxy flags, it returns a proxy-owned buffer (`+0x140` or
   `+0x38`) or forwards to the underlying swap chain. Its `Present` slot
   `+0x40` is RVA `0x1ea1d0`; the branch at `0x1ea231..0x1ea2a3` uses
   D3D11 `CopyResource` to move a proxy buffer into a selected buffer when
   its conditions require it, then calls the real swap chain's Present.
   The branch at `0x1ea2ad` calls the substantial presentation helper
   `0x201b70` before forwarding. That helper itself invokes SR wrapper
   `0x1f4ca0` at `0x201f8e`. This is a second SR entry in the reference,
   distinct from the post-world callback.

## Render-sized proxy buffer and presentation pass

The factory records the requested swap-chain extent at state `+0x24/+0x28`
(`0x20319f..0x2031a9`). Unless its branch at `0x2031ac` forces scale 1,
it multiplies those dimensions by the ratio at `+0x44` and truncates them
into render extent `+0x2c/+0x30` (`0x2031d8..0x2031fd`). Both constructor
paths pass that render extent to proxy constructor `0x1ea890`.

Constructor `0x1ea890` sets proxy flags `+0x28=1`, `+0x29=0`, `+0x2a=1`
and calls `0x1ea560`. The latter queries underlying swap buffers and creates
proxy texture `+0x140` through `ID3D11Device::CreateTexture2D` at
`0x1ea838..0x1ea84e`, replacing the descriptor's width/height with the
passed render extent. `GetBuffer` at `0x1ea110..0x1ea177` returns `+0x140`
with `AddRef` while flags `+0x2a` and `+0x28` remain set. The post-world
callback supplies the same `+0x140` wrapper to `0x1f4ca0` at
`0x156d92..0x156dab`; that wrapper calls D3D11 `CopyResource` from this
source into its private input at `0x1f4d01..0x1f4d16`. Thus the reference
has a concrete render-sized SDR texture in its own proxy path, rather than
relying solely on Skyrim's display-sized backbuffer.

In the proxy's `Present` method, the initial `+0x29=0` flag takes branch
`0x1ea2ad`, which calls `0x201b70` before the real Present. The helper at
`0x201ca1..0x201ccd` reads the indexed proxy target, and at
`0x201d2a..0x201e1a` draws from proxy-owned resources using render
dimensions. Its `0x201eac..0x201ee0` code computes render/display width
and height ratios; `0x201f7d..0x201f8e` passes proxy `+0x140` to the same
SR wrapper. No byte/word stores to these flags other than constructor/setup
were identified in the proxy's `0x1e8000..0x1eb000` code range; a broader
scan found other stores at the same offsets on unrelated objects, so this
does **not** prove the flags never change at runtime.

These facts support an owned reduced-buffer/proxy architecture as a possible
source path for RazKolbas. They do not prove which pixel stages contain UI,
how the reference composes the final full-size image, or compatibility with
the existing ReShade/ENB swap-chain ownership. The tested RazKolbas path
continues to use native 2560x1440 DLAA; reduced SR is not activated.

The reference's private offsets are not a drop-in contract for RazKolbas.
RazKolbas 0.1.24 has a D3D11 creation/swap observer and user-tested
full-resolution SDR DLAA copyback before UI; it does not own a reduced
presentation proxy. Its separate HDR world target cannot be copied directly
to the RGBA8 display target without the game's colour conversion.

## Reproduction and next implementation question

The disassemblies were generated from hash-verified files with
`python tools/re/trace_sr.py --package SkyrimUpscalerAIOBuild16-Hotfix1
--host skyrim --output artifacts/local/<name> --rva <site>`; raw instruction
outputs remain ignored under `artifacts/local/`. The independent Address
Library decode is in `artifacts/local/skyrim-sr-hook-map.json` and its
reproducible tool is `tools/re/map_skyrim_sr_hooks.py`.

The next engineering question is whether a **RazKolbas-owned** reduced SDR
buffer can be substituted in the exact game/ENB/ReShade swap-chain chain
without capturing UI at reduced resolution, while still delivering the
display-sized SR or spatial fallback at the verified pre-UI boundary.
The static reference proves its own allocation and routing, but not that
compatibility contract. No further unchanged menu-only run answers it.
