# FG UI planes: offline capture checkpoint, 2026-09-28

The source-only `FgUiPlanes` class takes a pre-UI snapshot on the D3D11
immediate context. After a separate UI colour/alpha texture and final colour
exist, it returns three owned display-sized textures and stamps tied to one
source, generation, presentation token and reset epoch. A changed token,
generation or source cannot finish the pending capture. The separate UI
source must have an alpha-capable format, must not alias either the pre-UI
source or the final source, and must supply a nonempty in-bounds region.
Failure discards the pending FG capture without writing to final colour.

A WARP test cleared pre-UI colour red, changed the final source to green,
cleared a distinct UI texture blue at half alpha, and read back red, green
and blue/half-alpha from the respective snapshots. It also exercised missing,
aliased, reduced-size and stale-frame inputs and generation rollover. Debug
and Release builds each passed 53/53 CTest groups. These are offline D3D11
copy and validation results, not native Skyrim UI capture evidence.

The existing `NativeUiRedirector::commitPublishedUi` boundary changes the
display target before Scaleform's deferred flush. In the current installed
path, native UI still draws into the final backbuffer; that composite cannot
reliably recover an independent UI colour/alpha plane. The class therefore
has no game caller, no Streamline tags, and no FG-On effect. A valid region
and alpha-capable texture descriptor do not prove nonempty or correctly
composited Skyrim UI pixels. Same-context command ordering is required when
passing these snapshots to the later D3D11-to-D3D12 input lease.

Next, establish an actual native-size transparent UI producer at the verified
UI draw boundary, preserve depth/stencil and menu MRT behavior, and compare
HUD, InventoryMenu, MagicMenu and loading images before any FG-On tag path is
enabled. The installed V5.4 0.1.115 DLL was not changed. Skyrim runtime
verification for these planes is **NOT RUN**.
