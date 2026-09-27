# Cold Main Menu native UI candidate, 2026-09-27

The owner wants the title screen before loading a save to be clear as well as
inventory and MagicMenu. In the 0.1.102 user-started run, the log shows
reduced-source 1707x960 to native 2560x1440 pre-Present spatial publication
from the first frame, with no world-like depth. Native UI routing did not
activate until frame 7832 at 11:01:20, after a save was loaded and the scene
admission gates passed. Thus the title screen had no pre-UI native handoff in
that run. This is log evidence about publication order, not a capture of the
title glyph target.

0.1.104 adds a guarded cold-title candidate at the existing verified
`IMenu::PostDisplay` boundary. It recognizes CommonLib's exact `Main Menu`
name on the UI stack, excludes `Loading Menu`, requires the known renderer
lock/thread, device/context/swap, motion/depth pointers, owned scene texture
and prepared native UI companions, then publishes the current scene through
the existing spatial fallback before native Scaleform drawing. NGX remains
gated by the existing world/menu admission checks. This cold route does not
set the sticky world-UI activation flag, so loading screens retain their old
pre-Present behavior. If a reduced menu pass follows, Main Menu now keeps the
native composite instead of copying that reduced scene over it; its Cursor
Menu can be replayed on native colour. The separate MagicMenu preservation
candidate from 0.1.103 is included. Inventory-specific diagnostic capture
remains restricted to InventoryMenu.

The design is bounded but **not yet a proven visual fix**. The title may fail
the preflight in the real menu, may use different movie dimensions despite a
native RTV/viewport, or may have another later overwrite. The first
user-started run should observe whether the title is clear, then load a save
and check MagicMenu clarity/cursor and the previously working inventory. If
the title remains blurry, the next evidence is the first title glyph draw's
effective RTV/DSV/viewport, logical movie size, and before/after publication
pixels; no blind global screen-size patch is justified by current samples.

The 0.1.104 Release build passed 43 CTest groups. The five-file MO2 ZIP
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.104-v54-title-magic.zip`
passed independent per-entry payload hashes, ZIP SHA-256
`ad1a82d56c6e194bc872569004735fbb4f04988497debe58ff7b93e09ac6c8d8`.
While Skyrim was absent, the installed 0.1.102 DLL and manifest were saved to
ignored `artifacts/local/v54-0.1.104-install-backup`; only the DLL and
manifest were replaced. All five installed payload hashes passed an
independent check, including unchanged INI, signed SR and patched NR files.
Installed DLL SHA-256:
`a83a9cff9b86f29cc3f2ce8f0cdfc866b83cfd783ad10405f6e878056652b1c0`.
**Skyrim visual and runtime verification for 0.1.104: NOT RUN.**
