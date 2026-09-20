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
   Another branch at `0x1ea2ad` performs further work before forwarding.

This establishes **where this reference routes its SR result**, conditional
on its proxy configuration. The exact pixels returned by each branch,
pre/post-UI placement, ENB/ReShade order, and whether every branch reaches
the real backbuffer are not established by static disassembly alone.
The reference's private offsets are not a drop-in contract for RazKolbas.
RazKolbas currently has a D3D11 creation/swap observer and an offscreen
NGX result, but no owned presentation proxy or display write. Its 0.1.17
runtime map found a distinct HDR world target and RGBA8 backbuffer; writing
the RGBA16F DLAA output directly to that backbuffer would be the wrong
format/colour pipeline.

## Reproduction and next implementation question

The disassemblies were generated from hash-verified files with
`python tools/re/trace_sr.py --package SkyrimUpscalerAIOBuild16-Hotfix1
--host skyrim --output artifacts/local/<name> --rva <site>`; raw instruction
outputs remain ignored under `artifacts/local/`. The independent Address
Library decode is in `artifacts/local/skyrim-sr-hook-map.json` and its
reproducible tool is `tools/re/map_skyrim_sr_hooks.py`.

The next engineering question is which **RazKolbas-owned** target should
receive an appropriately converted, display-sized SR image at the verified
world/pre-UI boundary, with one presentation owner and a valid native
fallback. The reference answers where its own proxy accepts a shader draw;
it does not prove an existing writable native Skyrim backbuffer at that
point. Trace the exact game stage and resource lifetime before adding a
display write. No further unchanged menu-only run answers that question.
