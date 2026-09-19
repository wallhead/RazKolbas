# Skyrim renderer observer — first T05 milestone

This is an experimental creation observer, not frame processing. SR, FG and NR remain inactive. The game executable and vendor DLL files are never byte-patched by this observer.

The exact executable and owner identities, IAT RVA, ABI, activation gates and recovery contract are recorded in `../../patches/skyrim/device-create.observe-v1.json` and compiled into `src/skyrim/RuntimeProfile.cpp`. Unknown files or conflicting pointer owners leave native rendering untouched. Updating Windows or ENB may intentionally invalidate the profile.

## Evidence and limits

- Static PE inspection of the selected Skyrim 1.6.1170 file found the named D3D11 creation import at RVA `0x17502a0`. The opt-in `.local_game_profile` test verifies the complete file hash, maps an owned byte copy of its headers/sections without execution, and validates the compiled import parser (43 assertions passed).
- A scan for direct RIP-relative import references did not identify a disk call site. No game instruction address or early-startup timing guarantee is inferred from this absence. Fresh runtime interception is still required.
- SKSE upstream `PluginManager.cpp` loads the plugin and subsequently invokes `SKSEPlugin_Load`; installation runs there, not in our DllMain. Source: https://github.com/ianpatt/skse64/blob/master/skse64/PluginManager.cpp . This does not establish Streamline initialization ordering; Streamline is not loaded.
- The callback invokes the prior verified owner exactly once, then obtains the adapter through the returned actual device. It checks COM device identity against the swap chain and supplied immediate context. It logs adapter LUID, vendor/device IDs, feature level, dimensions, format, buffer count and sample count. COM queries retain no references beyond observation.
- Synthetic tests cover exact-slot installation/restoration, malformed or ambiguous PE rejection without writes, full call forwarding, observer exceptions and original last-error preservation.
- A real D3D11 WARP integration test creates a hidden 64×64 swap chain and independently verifies actual adapter identity. GPU readback before and after observation is identical. This is a local API test, not Skyrim/ENB compatibility evidence.
- Debug and Release each have 12 passing CTest groups. Selective patch-disable parsing and suppression were regression-tested after review identified the missing configuration gate.

## Controlled game test

The normal sample INI keeps `ExperimentalPatches=false`. The installed controlled test opts in. Launch the selected SKSE executable through MO2, reach the menu, then exit. Inspect only fresh timestamped entries in `Documents/My Games/Skyrim Special Edition/SKSE/RazKolbas.log` for installation followed by `Renderer observation`, `Actual render adapter` and `Actual swap chain`. Reaching the menu without those entries does not prove capture. Any rejection should preserve native fallback and provide its reason.

The callback and original owner are pinned for process lifetime; live removal is deliberately not exposed. To suppress installation on the next launch, set `DisabledPatchIds=skyrim1170.device-create.observe-v1`, set `ExperimentalPatches=false`, or use SafeMode. No Present, resize, depth, motion-vector, frame-resource or temporal integration exists yet.
