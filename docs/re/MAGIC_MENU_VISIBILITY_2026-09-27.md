# MagicMenu visibility regression and 0.1.105 restore

In the user-started 0.1.104 run, the owner reported that the screens looked
good except MagicMenu, which was invisible like the earlier InventoryMenu
bug. The 0.1.104 log records its native-composite preservation and Skyrim
Cursor Menu replay at frame 215772 (13:54:27). This directly contradicts
the assumption that MagicMenu's content already resides in native colour at
the deferred UI flush. The report does not identify whether its panels are in
the reduced scene, another render target, or a later Scaleform pass.

The 0.1.105 source routes MagicMenu through the earlier reduced-scene
publication, while retaining the user-verified InventoryMenu route and the
title-screen candidate. It records one capture after 30 consecutive
MagicMenu frames: reduced source and native colour before late publication,
native after publication and EndFrame, then both images before Present.
These are diagnostic raw captures outside git. Comparing them is required
before attempting a MagicMenu native-resolution draw or replaying its
overridden `PostDisplay` method. A visibility restore may return the earlier
blurry MagicMenu; sharp MagicMenu is not yet established.

Release build: 43/43 CTest groups passed. Staged package:
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.105-v54-magic-restore-final.zip`,
SHA-256 `ec24a8ba1dcf65891f3bf05ad7e0c843e44ada2dab91cd1b4fc16f2501d555b8`.
An independent ZIP entry and payload hash check passed. Skyrim was absent
when the V5.4 MO2 mod's DLL and manifest were replaced. The prior files are
backed up under ignored `artifacts/local/v54-0.1.105-install-backup`.
The diagnostic log's moved-from capture label was corrected before any
0.1.105 runtime test; the final replacement was backed up under ignored
`artifacts/local/v54-0.1.105-final-install-backup`.
Installed DLL SHA-256:
`283919b4db47de5ddf1be7de4d282dfd1fb5254f138523c526fff5401feb65e0`.
The INI and pinned NVIDIA SR / modified NR runtimes were unchanged and all
five installed manifest hashes passed. **0.1.105 Skyrim visual verification:
NOT RUN.**
