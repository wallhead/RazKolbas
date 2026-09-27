# AIO native UI RE24: local cross-check

Reviewed 2026-09-27 against the supplied
`C:/Users/user/Downloads/AIO_Native_UI_RE_24.zip`. This packet is reference
evidence, not an instruction to patch the game or reuse AIO as a dependency.
The original `SkyrimUpscaler.dll` matches SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
An independent path-normalized check verified all 67 manifest payloads with
no missing, changed or extra files. The packet's own `verify_bundle.py` reports
false unexpected-file errors on Windows because `Path` produces backslashes
while its manifest uses forward slashes. Read-only PE mapping verified all
17 code-span hashes (11,437 bytes, 2,601 reported decoder rows per tool).
Capstone 5.0.7 independently confirmed selected instruction sites at host RVAs
`0x157729`, `0x1f3609`, `0x1f3773`, `0x1f37a0`, `0x1f37b8`,
`0x201880`, `0x201898`, `0x2018c7`, and `0x201cf4`. The packet's 114
original-CPU assertions per compiler and equal observations are
**packet-reported, not rerun locally**; they use synthetic graphics/backend
objects and are not Skyrim or GPU results.

## Evidence relevant to our current UI path

The AIO native-transition callback at host RVA `0x1572a0` has no internal
`host+0x187` once-per-frame guard. In the packet's synthetic double-call
case it evaluates and publishes twice. The `+0x187` guard at `0x201cf4`
belongs to a pre-Present fallback. This does not prove RazKolbas publishes
twice, but it rules out using that reference byte as a universal duplicate
guard. Our cursor `PostDisplay` replay is a different call and still needs
its own in-game outcome.

AIO's ordinary native callback tail binds native colour/depth without an
intrinsic clear or viewport setter. Its recognized HDR UI path clears a
separate transparent UI surface and depth/stencil on each invocation; its
world-tail clear has a distinct Main/Loading condition. These operations
cannot be copied into a per-menu or end-frame unconditional clear. The
fullscreen helper selects a depth state with stencil disabled and a
rasterizer with scissor disabled, then leaves substantial graphics state
selected. Its zero-count `PSSetShaderResources` call does not clear SRV
slots; the transfer helper restores OM/viewports but not all shader/depth/
raster/compute state. A bound native DSV alone therefore cannot establish
that a subsequent glyph mask used stencil. The actual game setters after
this helper were not traced.

The AIO texture wrapper has distinct cached sRGB RTV, ordinary RTV, SRV and
DSV views. A synthetic pointer-only texture replacement retains an old
cached view, although selected real rebuild paths clear ordinary view caches.
This is a cache contract, not an observed stale-view bug. A diagnostic must
compare the *effective* view's `GetResource`, descriptor and generation at
the draw. Its late OM hook recognizes the reduced main scene as RTV0 but
then substitutes the second attachment and non-null DSV positionally.
RazKolbas's identity/compatibility checks in `NativeUiRedirector` remain
appropriate; this evidence does not justify remapping unknown offscreen
passes or motion SRVs.

Our current `commitPublishedUi` prepares native colour/depth once, and
`beforeDeferredUiFlush` can publish a held non-inventory reduced scene before
rebinding native UI. Inventory skips that late copy and replays `Cursor Menu`
in 0.1.102. RE24 does not establish which title/HUD pixels are native at the
actual draw, whether stencil/raster state was set by Scaleform, or whether a
later copy overwrote them. A new renderer change now would confound the
unverified 0.1.102 cursor test. The existing read-only dimension/owner probe
also cannot report effective D3D11 draw state.

**Next runtime evidence:** with the user-started installed 0.1.102 game,
verify inventory list, Skyrim cursor, row highlighting and selection, then
sample the existing read-only dimension/owner probe at title and loaded HUD.
If native-resolution UI remains wrong, take a bounded same-frame trace at
the first relevant glyph/mask draw: effective RTV/DSV/SRV resources and
typed extents, depth/stencil state and reference, rasterizer/scissor,
viewport, shaders, plus destination/order of later scene publications and
clears. No RE24-induced binary/source patch or runtime verification occurred.
