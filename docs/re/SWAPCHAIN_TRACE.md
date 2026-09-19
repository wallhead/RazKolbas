# Presentation and resize observer — T05 continuation

0.1.2 adds pass-through Present, Present1, ResizeBuffers, ResizeBuffers1 and Release observation for the exact local ReShade 6.7.3 wrapper. It remains optional: an absent, updated or differently hooked wrapper is rejected while the earlier device observer and native rendering remain available. This profile does not make ReShade a required dependency for future SR/FG/NR.

## Recovered locations

The pristine local `D:/TESV_EX/dxgi.dll` has SHA-256 `059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc`. It was inspected without loading it into a probe. RIP-relative references to its own ResizeBuffers and ResizeBuffers1 diagnostic strings (RVAs `0x3d7cd0` and `0x3d7e40`) occur at `0x13b7de` and `0x13c0ba`. PE exception-function records contain them in functions starting at `0x13b7a0` and `0x13c060`. Searching initialized data for both function addresses independently identifies the same table `0x3d7f90`, at documented COM slots 13 and 39. The other methods follow the IDXGISwapChain interface layout. Exact addresses, entry bytes and sizes are in the patch descriptor.

ReShade's corresponding source corroborates the wrapper's ownership and method semantics: [DXGISwapChain source, v6.7.3](https://github.com/crosire/reshade/blob/v6.7.3/source/dxgi/dxgi_swapchain.cpp). Present invokes effect processing before forwarding to the underlying swap chain. These observers wrap that entire method; their location is **not** evidence of a Skyrim world/pre-UI/SR boundary. No effects are invoked manually or duplicated by RazKolbas.

## Lifetime and forwarding

Every original is called once with unchanged arguments. Observer exceptions cannot change arguments, last error, return status or output pointers. Present test/occlusion/failure counts are separate; call totals are not source-frame IDs. Resize begins and ends are logged with the real result, including errors and zero dimensions; no fake success or automatic resource recreation is introduced. Release's returned count is logged only when zero, using an opaque integer captured before the call; no object is dereferenced after destruction.

All five sites are prepared and checked before mutation. Each atomic slot patch is independently valid during activation and reverse rollback; immutable original pointers are published first. Code, table owner and forwarding state remain resident even after a failed attempt. No COM object, backbuffer, render target or device context is retained. This milestone therefore does not establish the resource-generation/retirement contract needed for actual processing.

The shared table affects every wrapper instance in this exact module, and logs include object addresses only as opaque correlation tokens. It is never replaced by a shortened shadow table. Future resource ownership must distinguish recreated instances rather than interpreting an address as persistent identity.

## Validation

Unit regressions were observed failing before forwarding/profile/disable-list implementation. They cover ABI arguments and pointer arrays, original failure/status/last error, observer exceptions, unknown hash, wrong extent/table, changed pointer/prologue, executable table section, malformed header and no writes on rejection. Real D3D11 WARP tests exercise live vtable replacement/restoration, unchanged COM identity, failed resize with an externally retained backbuffer, eight successful resizes after release, and a real zero-count Release. The opt-in `.local_swap_profile` test checks the exact local file hash and validates an owned mapped byte copy without executing ReShade.

Game Present/resize/Release callback execution for 0.1.2: **NOT RUN**. The preceding 0.1.1 game run proved only creation capture with ENB and ReShade. Next launch through the same MO2 profile, reach the menu and load a save, play briefly, then exit normally. Resize is separately NOT RUN unless an actual resize callback appears; do not change the modlist or video configuration merely to force it.

## First game result: mismatch, preserved fallback

0.1.2 at22:09:33: actual returned table did not match the expected ReShade identity/location, so no presentation/resize slots were changed. The original log omitted actual ownership details; 0.1.3 adds those read-only details before validation. Do not add a new patch profile until the returned owner's identity and methods are known. The static ReShade profile remains verified only offline.

## Actual outer owner recovered — 0.1.4 profile

The 0.1.3 trace at22:19:30 resolves the rejection: the table returned to Skyrim belongs to ENB, not ReShade. Exact ENB hash47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58, tableRVA1a4848, Release6d2b0, Present6c3e0 and ResizeBuffers6c4a0. These live locations match pristine-file table pointers and disassembly. ENB's file version is0.5.0.4; the profile ID additionally identifies the supplied build date and exact hash.

The new ENB profile touches only base-interface slots2/8/13. No assumption is made that this table exposes Present1 or ResizeBuffers1. ENB Present handles test calls separately and forwards to an underlying interface; ResizeBuffers similarly calls the underlying slot. RazKolbas forwards to ENB once and preserves its existing behavior. ENB Release returns an underlying reference count, so the observer reports only 'Release returned zero', not proof of wrapper destruction.

Profiles are selected by exact owner hash and table location, with per-profile disable IDs. The disabled ReShade ID does not suppress the distinct ENB profile. Lifecycle and atomic rollback remain as previously tested. The user explicitly authorized automated game launch/close; use the selected MO2 SKSE entry, because direct loader execution would omit the managed mod's virtual filesystem. The supplied typed loader path was absent; verified loader is D:/TESV_EX/skse64_loader.exe.

## 0.1.4 runtime result

ENB profile installed22:28:03; at least15000 Present calls completed with failed0 by22:30:31. Menu verified, game closed via targeted Alt+F4 and process absence confirmed. Both ENB/ReShade active. No resize or Release-zero logged: NOT OBSERVED. See ../SKYRIM_SMOKE_TEST.md. All13Debug/Release groups and both offline file audits passed.
