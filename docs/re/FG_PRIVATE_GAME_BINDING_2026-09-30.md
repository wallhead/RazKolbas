# Private FG-Off swap binding, ReShade parent factory

The V5.4 game path now has an opt-in, restart-only
`Diagnostics.ProbeFgPrivateSwapOff` switch. It leaves
`FrameGeneration.Enabled=false` and sets `slDLSSGSetOptions(Off)`. During the
verified Skyrim 1.6.1170 D3D11 creation callback it prepares a private,
hash-pinned Streamline 2.14.1 D3D12 lower swap on the selected NVIDIA adapter.
Preparation takes the **adapter's exact ReShade 6.8 parent factory**, upgrades
that interface through Streamline, verifies the proxy and lower Present owners,
and tests one real FG-Off lower Present before installing the ReShade factory
trace. The existing verified ReShade and System32 DXGI slot-10 hooks then
substitute a D3D11 facade using the native D3D11 device passed by ReShade.
The selected adapter LUID, window, extent, callback thread, native method
owner and one-shot admission must match. A failed preparation or callback
releases the candidate lower and calls the saved native method. No new binary
patch site was added; the existing pinned factory sites and CAS pointer-slot
ownership apply.

The first standalone probe used a new System32 factory instead of the
ReShade adapter parent. With ReShade loaded first, the Streamline lower
returned `E_ABORT` (`0x80004004`) on its first real Present. A prior private
facade probe intercepted lower Present, so it did not prove this step. Loading
ReShade after lower creation made the System32-factory probe pass. Preloading
Streamline before ReShade did **not** fix the ReShade-first/System32-factory
case. Inspection of the current
[DynamicShaderFrameGen source](https://github.com/jatelop8/DynamicShaderFrameGen/tree/879ab2c13404e02f352fa10515abd0fc25996d4f/src)
showed its D3D12 lower creation uses `adapter->GetParent`, then upgrades that
factory through Streamline. That evidence motivated the parent-factory change;
it is not a claim that its entire presentation design was copied or validated.

The corrected standalone `RazKolbasFgPrivateGameRouteProbe` loads the exact
installed ReShade DLL **before** the private runtime, enumerates its NVIDIA
adapter, prepares the lower, then patches the ReShade and native factory
slots in the same order as the game route. It observed one substitution, a
distinct ReShade upper swap, successful `Present(TEST)`, a real upper Present,
resize from 160×96 to 192×108, and ordered release/shutdown. Exit code was
zero. The route preflight also returned success. Release CTest passed 61/61;
Debug CTest passed 57/57. The admission test first failed to compile because
the new gate was absent, then passed after implementation.
A second regression test found that the trial switch parsed but was initially
classified as a live menu change. It is now restart-scoped, and both CTest
presets passed again after that correction.

The V5.4 MO2 package was staged with the existing SR/NR choices preserved,
the six pinned NVIDIA FG runtime files, and
`Diagnostics.ProbeFgPrivateSwapOff=true`. The 14 manifest files were
hash-checked after installation. The installed FG runtime passed the same
ReShade-first offline route probe. The previous DLL, INI and manifest are
retained in ignored `artifacts/local/backup-v54-before-0130` for rollback.

This is **offline FG-Off evidence**. Skyrim, ENB, the actual V5.4 window,
render thread, SR/NR interaction and FG-On generated frames remain NOT RUN for
this build. The one real preflight Present may briefly publish an empty frame
at startup. The route is guarded by a diagnostic switch and must not be
described as working DLSS-G until a real game submission and generated display
are observed. The next required run is one user-started Skyrim launch with
this package, then inspect `RazKolbas.log` for startup factory selection,
facade substitution, first real Presents and native fallback; visually check
the world, ENB/ReShade effects, UI and resize.
