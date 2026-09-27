# Native UI live checkpoint, 2026-09-27

The owner reports that inventory is now good in installed 0.1.102, but
MagicMenu is blurry. The user-started process was PID 14768, launched at
10:58:16 from the V5.4 Stock Game, exact Skyrim 1.6.1170 SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The installed 0.1.102 DLL remained SHA-256
`faec1fb7d0ccad2f81ebc02f623094806804fef95218bbabb34760e04b379b80`.
The current user token was medium integrity and Windows denied `OpenProcess`
for this game. A separately elevated, bounded helper ran the existing
`tools/re/inspect_live_detours.py` read-only; it sent no input, installed no
hook and wrote no game memory. The ignored report is
`artifacts/local/ui-live-2026-09-27-pid-14768/read-only-detours.json`.

At 11:03 the owner says the **magic menu** was open. All 12 one-second
samples reported both graphics-state dimension pairs as **1707x960**, with
current/previous ratio `(1,1)` and the Skyrim dynamic-resolution switch off.
Ten sampled viewports were 1707x960; two were transient 512x512 viewports.
The live RazKolbas log separately reports reduced source 1707x960 and native
output 2560x1440. These are state samples, not the consumed Scaleform/movie
size at a particular glyph draw. The dimension-writer CALL at game RVA
`0x14b2e14` points to a private relay whose absolute destination lies in
`UnderwaterNG.dll`. The resize hook is a private relay associated with SSE
Display Tweaks; candidate mouse metadata, screen-size and native-transition
CALLs still point into SkyrimSE.exe. Relay ownership does not establish who
wrote the sampled dimensions. The ignored elevated module listing and probe
report preserve exact addresses and bytes.

The 0.1.102 log at 11:01:24 records inventory composition preservation and
Skyrim `Cursor Menu` replay. The bounded native-colour capture for PID 14768,
frame 8077, changed 689 pixels in box x=1204..1231, y=910..950 between
before and after replay; the post-EndFrame image is byte-identical to the
post-replay image. This supports that the cursor replay changes/preserves a
small native pixel region. The owner's inventory report is the visual/runtime
outcome; the capture alone cannot prove item selection or general UI quality.
Raw capture files remain ignored and are not staged.

Source inspection finds that `beforeDeferredUiFlush` skips the late reduced
scene copy only when InventoryMenu is on the stack. MagicMenu is a separate
CommonLib `kCustomRendering` item menu with cursor updates. The live magic
blur and reduced dimensions make the late copy a concrete candidate, but the
pre-copy MagicMenu pixels and exact draw state were not captured. We therefore
added a bounded **0.1.103 candidate** that applies the inventory preservation
and native cursor replay to MagicMenu only. Inventory-specific capture remains
inventory-only; other menus, provider/NR/sharpening and INI settings are
unchanged. If the magic menu becomes clear, that supports the copy hypothesis;
if not, draw-target/Scaleform-size tracing remains necessary.

0.1.103 Release build and all 43 CTest groups passed. The staged five-file
MO2 package passed independent manifest and ZIP payload hashes. ZIP:
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.103-v54-magic-preserve.zip`,
SHA-256 `987133ddaadbf963ee39d718f44400524d1c549e2e9d616f6e1be8bc63961bed`.
Release DLL SHA-256
`3ff918af007eda099db838c6b6052eacee0451c29b346bf835f47de8f127b843`.
At this checkpoint **0.1.103 is packaged but not installed or run in Skyrim**;
the game must exit before its loaded DLL is replaced. A result for magic menu,
cursor interaction and inventory regression is still **NOT RUN**.
