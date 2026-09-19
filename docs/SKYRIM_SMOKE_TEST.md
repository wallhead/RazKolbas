# Skyrim native-host test installation

Installed 2026-09-19 at the user's request, into `D:/TESV_EX`. The originally typed `D:/TESV/_EX` is absent. Skyrim executable version is `1.6.1170.0`, SHA-256 `c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`. This records the test executable identity; it does not establish a verified rendering-hook profile.

MO2 is `D:/TESV_EX/MO2/ModOrganizer.exe`, selected profile `TRUE AE V5.32 EXTENDED + OSTIM`. Its existing executable **Skyrim TrueAE V5.3 EXTENDED** points to this installation's `skse64_loader.exe`.

Installed from source commit `4376113`, Release:

| File | SHA-256 |
|---|---|
| `D:/TESV_EX/Data/SKSE/Plugins/RazKolbas.dll` | `e2b4e2184f7119133adfbd106f4ffcc476fcb75025a1afff060b05b6ae870d2c` |
| `D:/TESV_EX/Data/SKSE/Plugins/RazKolbas.ini` | `692eaa6b8fbbd62cc3ed2595fb5e02d0cf1060425d3e7ff0c40086b2418337fc` |

These are loose game Data files, visible to every MO2 profile using this game root. No existing files were overwritten; no MO2 profile or other mod was changed. No existing RazKolbas override was found in game Data, MO2 mods/overwrite, or the installation's Mods folder. Both copied hashes match their source files. All 10 Release CTest groups passed immediately before installation. The local machine-readable record is `artifacts/local/skyrim-install.json`.

## User test

1. Launch **Skyrim TrueAE V5.3 EXTENDED** through this MO2 instance.
2. Reach the main menu, then load a save if desired, and exit normally.
3. Inspect `Documents/My Games/Skyrim Special Edition/SKSE/RazKolbas.log` and `skse64.log`.

Expected RazKolbas log messages include `RazKolbas native host bootstrap ready` and `Native mode: no verified rendering hook profile attached; SR/FG/NR inactive`. This test establishes plugin loading/configuration/lifecycle coexistence only. No in-game upscaling, frame generation, NR image effect or ImGui menu is implemented yet. The standalone NR feature-creation probe is separate and was not installed into Skyrim.

Game launch/load results: **NOT RUN by the agent**; the user will test. Current settings retain Auto/Quality with FG and NR disabled.

The shared Documents log already existed before this installation: its last write was 18:55:50 on 2026-09-19, and it identifies a different build dated September 13. Those old FG/NR messages are not evidence for this build. A copy is preserved at `artifacts/local/RazKolbas-before-install.log`. This logger appends, so inspect only fresh entries after installation at 21:07 on September 19 and look for the native-host messages above.

## Remove this test build

Remove only the two installed files above. Preserve the INI first if it has been edited. Leave the surrounding Data/SKSE directories and other mods intact.
