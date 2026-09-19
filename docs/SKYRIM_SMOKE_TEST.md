# Skyrim native-host test installation

## Current installed test: 0.1.1 renderer observer

On September 19 at 21:44, source commit `81fabde` Release replaced the managed MO2 plugin after confirming Skyrim was stopped. The existing INI was preserved with only `ExperimentalPatches=true` opted in. The active profile still enables `+RazKolbas`. Both installed files match the staged package; no loose game Data plugin exists.

- Package: `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.1-renderer-observer-81fabde.zip`, SHA-256 `b8915e7eca4f3d189dfd05cce95d43799c74d59b018c36896942975140e660ad`.
- Installed DLL SHA-256: `7197b63a6773af18a4ef1c27795a47ba3575a5f5fc09e93e9bab4d8c92e9210d`.
- Installed INI SHA-256: `bce1346afa14199ba36b3040ccff5992d34c08f5f6223b4670eecd0e77de8365`.
- Previous DLL/INI/MO2 metadata backup: `artifacts/local/renderer-observer-install-2026-09-19-214408/`.
- Complete deployment record: `artifacts/local/renderer-observer-install.json`.
- Debug and Release: all 12 CTest groups PASS; exact-game offline profile: 43 assertions PASS; reviewer follow-up: no actionable findings.

**Game observation NOT RUN.** Next launch through MO2, reach the main menu and exit. Fresh log entries must show observer installation, a successful original creation result, actual render adapter and actual swap-chain details. A rejection message is evidence to diagnose, not a capture success. SR/FG/NR remain inactive. See `re/SKYRIM_HOOK_MAP.md` for scope and next-launch disable controls.

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
