# Skyrim native-host test installation

## Current installed test: 0.1.3 swap-table provenance

Installed at22:15 from source defa3a2. All13 Debug/Release CTest groups PASS. This is a read-only diagnostic addition to explain the 0.1.2 mismatch, not a fix or relaxed profile. Next user test: main menu and exit only, keeping ENB/ReShade enabled. Probe result NOT RUN.

Package: D:/TESV_EX/MO2/downloads/RazKolbas-0.1.3-swap-provenance-defa3a2.zip. DLL SHA256 c09516bfe89c20aa61b6db78fa084bf50b99de5941eaaf1a1c4693439477e67d; ZIP SHA256 b6259ee12c5e795895c4b4ccbd3dd661f6828eb36b2851cc1977169b496410be. INI unchanged. Previous DLL/INI/meta backup: artifacts/local/swap-provenance-install-2026-09-19-221546/. Complete record: artifacts/local/swap-provenance-install.json. Installed files, archive layout, enabled mod and absence of loose plugin verified.


## Previous test: 0.1.2 presentation/resize observer

Installed September 19 at 22:07 from source `691b0eb`, after confirming Skyrim was stopped. The enabled MO2 mod was updated; its existing INI was preserved byte for byte, with experimental observation already enabled. ENB/ReShade files and settings were not changed. Archive layout and installed hashes were verified, with no loose game Data plugin.

- Package: `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.2-swap-observer-691b0eb.zip`.
- ZIP SHA-256: `378ea1b05d7c0ee9185fd7f9c609ea48a749c44277c7ea9d13b73ea47598180d`.
- DLL SHA-256: `b427e3f69958448d957ff8583f4a1c6ba9933291333f42c1abea6ef3df046bec`.
- INI SHA-256: `bce1346afa14199ba36b3040ccff5992d34c08f5f6223b4670eecd0e77de8365`.
- Prior DLL/INI/meta backup: `artifacts/local/swap-observer-install-2026-09-19-220753/`.
- Deployment record: `artifacts/local/swap-observer-install.json`.

All 13 CTest groups pass in Debug and Release. Offline exact-ReShade validation passes 38 assertions; no ReShade code is executed by that audit. Game result: **observer rejected the returned table** at 22:09:33. No Present/resize/release patches were applied. Device capture and native fallback remained active.

Next test: launch through MO2 with ENB and ReShade enabled, reach the menu, load a save and play briefly, then exit normally. Fresh 0.1.2 logs should show `Installed reshade673.swapchain-observe-v1` and `Swap Present observation` (or Present1). Any resize or zero-count release is recorded separately; absence means that event was not observed. No video-settings change is required. SR/FG/NR remain inactive; this verifies presentation boundaries rather than image processing.


## Previous test: 0.1.1 renderer observer

On September 19 at 21:44, source commit `81fabde` Release replaced the managed MO2 plugin after confirming Skyrim was stopped. The existing INI was preserved with only `ExperimentalPatches=true` opted in. The active profile still enables `+RazKolbas`. Both installed files match the staged package; no loose game Data plugin exists.

- Package: `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.1-renderer-observer-81fabde.zip`, SHA-256 `b8915e7eca4f3d189dfd05cce95d43799c74d59b018c36896942975140e660ad`.
- Installed DLL SHA-256: `7197b63a6773af18a4ef1c27795a47ba3575a5f5fc09e93e9bab4d8c92e9210d`.
- Installed INI SHA-256: `bce1346afa14199ba36b3040ccff5992d34c08f5f6223b4670eecd0e77de8365`.
- Previous DLL/INI/MO2 metadata backup: `artifacts/local/renderer-observer-install-2026-09-19-214408/`.
- Complete deployment record: `artifacts/local/renderer-observer-install.json`.
- Debug and Release: all 12 CTest groups PASS; exact-game offline profile: 43 assertions PASS; reviewer follow-up: no actionable findings.

**Game observation PASS.** The user completed the requested run. Fresh log entries at 21:45:21 show the observer installed through the verified ENB wrapper; at 21:45:38 the original creation returned HRESULT 0, followed by actual device and swap-chain capture. Skyrim was no longer running when checked after the user finished. SR/FG/NR remain inactive. See `re/SKYRIM_HOOK_MAP.md` for scope and next-launch disable controls.

The sections below retain the initial 0.1.0 bootstrap test as historical evidence.


Installed 2026-09-19 at the user's request, into `D:/TESV_EX`. The originally typed `D:/TESV/_EX` is absent. Skyrim executable version is `1.6.1170.0`, SHA-256 `c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`. This records the test executable identity; it does not establish a verified rendering-hook profile.

MO2 is `D:/TESV_EX/MO2/ModOrganizer.exe`, selected profile `TRUE AE V5.32 EXTENDED + OSTIM`. Its existing executable **Skyrim TrueAE V5.3 EXTENDED** points to this installation's `skse64_loader.exe`.

Initial installation from source commit `4376113`, Release. The initial MO2 package was `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.0-native-host-4376113.zip`, SHA-256 `4d81bd42d4be81f3c2e45c5c3d4eb32cf6271fa4d2f622432ad5f2803e1a26df`. Its archive root is `SKSE/Plugins`, suitable for MO2 installation. The user completed installation after desktop automation was stopped; the mod's files and enabled profile entry were subsequently verified.

| File | SHA-256 |
|---|---|
| `D:/TESV_EX/MO2/mods/RazKolbas/SKSE/Plugins/RazKolbas.dll` | `e2b4e2184f7119133adfbd106f4ffcc476fcb75025a1afff060b05b6ae870d2c` |
| `D:/TESV_EX/MO2/mods/RazKolbas/SKSE/Plugins/RazKolbas.ini` | `692eaa6b8fbbd62cc3ed2595fb5e02d0cf1060425d3e7ff0c40086b2418337fc` |

These files are controlled by MO2's enabled `+RazKolbas` entry. The original loose installation was migrated after verifying both loose-file hashes against the original deployment record; backups are in `artifacts/local/skyrim-smoke-2026-09-19-211554/previous-loose-files/`. No unrelated mod was changed. Both managed-file hashes match their build/configuration sources. All 10 Release CTest groups passed immediately before initial installation. The initial managed-install record is `artifacts/local/skyrim-mo2-install.json`; the historical loose installation record remains `artifacts/local/skyrim-install.json`.

## User test

1. Launch **Skyrim TrueAE V5.3 EXTENDED** through this MO2 instance.
2. Reach the main menu, then load a save if desired, and exit normally.
3. Inspect `Documents/My Games/Skyrim Special Edition/SKSE/RazKolbas.log` and `skse64.log`.

Expected RazKolbas log messages include `RazKolbas native host bootstrap ready` and `Native mode: no verified rendering hook profile attached; SR/FG/NR inactive`. This test establishes plugin loading/configuration/lifecycle coexistence only. No in-game upscaling, frame generation, NR image effect or ImGui menu is implemented yet. The standalone NR feature-creation probe is separate and was not installed into Skyrim.

Game launch/load results: **native-host load PASS**. The user reports completing the menu/exit test. Fresh `skse64.log` reports `RazKolbas.dll ... loaded correctly (handle 195)`; `RazKolbas.log` records bootstrap ready at `2026-09-19 21:15:54` and the expected inactive-renderer message at `21:15:57`. No Skyrim process remained when checked. Logs are saved in `artifacts/local/skyrim-smoke-2026-09-19-211554/`. This verifies loading and message registration, not rendering, complete shutdown-resource behavior or a save-load test. Identical loose and managed copies existed during this launch; afterward the verified loose copies were moved to backup so only the MO2 mod supplies the plugin. Current settings retain Auto/Quality with FG and NR disabled.

The shared Documents log already existed before this installation: its last write was 18:55:50 on 2026-09-19, and it identifies a different build dated September 13. Those old FG/NR messages are not evidence for this build. A copy is preserved at `artifacts/local/RazKolbas-before-install.log`. This logger appends, so inspect only fresh entries after installation at 21:07 on September 19 and look for the native-host messages above.

## Remove this test build

Disable `RazKolbas` in MO2 for the current profile, or remove that mod through MO2. Preserve its INI first if it has been edited. No loose RazKolbas DLL/INI remains in game Data.

## 0.1.1 observed game result

Captured RTX 4080 SUPER (vendor 0x10de, device 0x2702), adapter LUID 00000000:000101a9, feature level 0xb000. Swap chain: 2560×1440, format 28, three buffers, one sample, swap effect 4, windowed, device flags 0x20. SKSE reports the plugin loaded correctly, handle 195. Installed DLL hash still matches the deployed artifact.

ReShade 6.7.3 (dxgi.dll SHA-256 059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc) was active and independently logged the same adapter and swap-chain dimensions. Its log records unsupported ImGui version 18600 errors while registering Rumble and Sky Reflection Fix for Skyrim; these are separate add-on compatibility observations, not failures of RazKolbas device capture. No attribution to RazKolbas or proof that they predate this run is made. User settings were left unchanged.

Full logs preserved in artifacts/local/skyrim-observer-smoke-2026-09-19-214521/. This proves the creation boundary and identity capture in this exact ENB/ReShade setup. It does not establish frame processing, resize handling, resource retirement, image quality, performance, or general compatibility with other versions. Next T05 work: verified frame and resize boundaries and resource lifecycle before provider integration.

## 0.1.2 returned-table rejection

The requested user test completed; Skyrim was absent afterward. Fresh SKSE logs show 0.1.2 loaded correctly. At 22:09:33 the device observer captured the RTX4080 SUPER and 2560×1440 swap chain, then rejected its table with `Unknown swap table identity/location`. ReShade remained active. This is a fallback result, not presentation-hook success. Logs are preserved under `artifacts/local/skyrim-swap-smoke-2026-09-19-220916/`.

The rejection message did not report which owner or table was actually returned. 0.1.3 adds read-only provenance logging: table owner's exact hash/size/RVA and the module hash/size/RVA for base-interface Release, Present and ResizeBuffers slots. It does not relax matching, guess extended-interface slots or patch a new owner. One short menu/exit run is sufficient to collect this missing evidence; loading a save is unnecessary for this probe.
