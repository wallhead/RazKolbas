# FG-Off colour and loading-picture regression

The user's closer comparison supersedes the earlier broad report that the
image looked normal. They report a warmer/yellower world image with RazKolbas,
and nearly black loading **background pictures**, rather than 3D loading
models. The initial screen before the main menu looks good; loading a save
and fast travel are affected. The user confirms the same save and unchanged
ENB/ReShade settings in the two screenshots. This does not change the prior
black-screen repair's actual Present results, but visual compatibility is
now an open regression. Game FG is still Off.

## Supplied comparison

Both original PNGs are 2560x1440. Originals remain in the user's NVIDIA
capture directory and are not staged in Git.

| Shot | User identification | Visible FPS counters | Original SHA-256 |
|---|---|---|---|
| 2026.09.30 - 18.03.06.15 | RazKolbas On; warmer image | 43 / 41 | `4f9f58ee102fe49d908f304578f3ec1c4dd002df032a95652565e364b2a21cc7` |
| 2026.09.30 - 18.08.26.99 | RazKolbas Off | 64 / 64 | `bb574d85d9bdbcd1cc8db7189b3473b79e90a7a294e049cfb6b31193b034fd40` |

Read-only analysis of the originals confirms differing RGB values in static
scene patches. A sign patch gains red while the road loses green/blue; a sky
patch loses red. The differences are not a single uniform yellow offset.
Independent per-channel affine fits have significant residuals (0.063–0.093
in normalized RGB), so they are not a justified correction curve. Separate
runs contain moving elements and possibly different adaptation history.
No brightness, white-balance, saturation or gamma filter was added.

## Runtime and source evidence

The screenshot-relevant 0.1.134 session starts **17:51:43**. Its saved complete
session has 1,330 lines, SHA-256
`7e4e495a14ee4798226e0d9e043030e0d701725790ff9cef629a287386bbe440`.
The private FG-Off presentation chain was active; camera-write observation
was absent. The final recorded outer Present at frame 32400, **18:03:10**,
is S_OK with failed=0. These results do not establish colour fidelity.

At **18:02:57**, Loading Menu frame 31644 reports native 2560x1440 RTV, DSV
and viewport, mode DLSS SR, and 20,305 prior provider submissions. Its
pre-EndFrame/after-EndFrame scissor is 1485x835, whereas the earlier
before-first-PostDisplay sample is 2560x1440. Source inspection confirms:

- `processOwnedWorldFrame` can retain menu-boundary world admission and submit
  DLSS without an explicit current Loading Menu exclusion.
- `beforeDeferredUiFlush` treats cold loading specially only before successful
  SR/world admission; later loading follows another composite path.
- The scissor handler deliberately forwards the incoming rectangle. Existing
  tests explain that the previous general scissor-rescaling hypothesis failed
  in Skyrim. This observation alone does not justify repeating that change.
- The spatial and sharpening passes use `D3D11StateScope`, which switches to
  a fresh context state. They do not simply inherit the game's alpha blend
  state; that proposed explanation was rejected on source inspection.

These are concrete differences to investigate, not proof of the picture's
last writer or the colour shift's cause.

A subsequent user-started session begins **18:12:35**, saved at 511 lines,
SHA-256 `ab5897a54cebe0700251b94c8eed33aab2886d760ed78f464f1657a8c3c5b89c`.
It still uses the private FG-Off route and has no camera-write observer.
The final frame 16800, **18:16:37**, is S_OK with failed=0. Its late throughput
differs markedly from the screenshot run, so no universal FPS rate or recovery
claim is made. Skyrim subsequently closed before configuration changes;
the agent did not launch or terminate it for this comparison.

## Ghidra and Capstone cross-check

The exact 1.6.1170 executable remains SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The preserved decoded text was independently hash-checked against
`75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3`.
Its PE-unwind-bounded framebuffer initialization, RVA **0xe4cc50–0xe4cd83**,
is 307 bytes, SHA-256
`b9f1a01b2504964f861ab55be175a6063c7c77c3b1b4865457ab3453c1a8d772`.
Capstone **5.0.7** decodes every byte. Ghidra **12.1.3** independently
disassembles and successfully decompiles that complete window with the
Windows x64 compiler specification.

Both show GetBuffer(0), then CreateRenderTargetView and
CreateShaderResourceView with **null view descriptors** at RVA 0xe4ccaa and
0xe4ccd1. Thus this vanilla framebuffer initializer does not supply an
explicit sRGB view override. It does not prove every later ENB/ReShade view
uses the same encoding. No original executable, reference DLL or game code
was modified. A separate full 1,183-byte ENB 0.505 creation window was decoded
with complete Capstone coverage and the exact pinned ENB hash.

The requested external read-only frame-hook inspection failed once with
OpenProcess access denied (Win32 error 5). No elevated/denied retry occurred.
An in-process FG phase probe was being designed, but no source or hook was
installed before the new visual regression took priority.

## Prepared isolated presentation comparison

With Skyrim closed, backed up the actual current DLL, INI and manifest at
`artifacts/local/backup-v54-0134-before-native-colour-test`. The user's current
INI had additional diagnostic-Off entries; those were preserved. Changed
**only** `Diagnostics.ProbeFgPrivateSwapOff` from true to false. A reversible
byte comparison and independent effective-field parser both confirm no other
INI change. This isolates the private presentation trial while retaining SR.

- DLL unchanged, 0.1.134, SHA-256
  `e9c7fc54436a9f1c23c0a11ae5650badbbc630941a1d9304b9ca194d1d847e54`.
- Prior actual INI SHA-256:
  `e514ffe7be2e8482a4dfdcd271650da6e0a1fbd1d2ae6920d513a0295267670c`.
- Native-comparison INI SHA-256:
  `902a9e6a297070571062ff8d2df65c442169dace72ab6c90116f2702de2233bd`.
- DLSS Balanced/M, sharpening approximately 0.95 and hotkeys preserved;
  NR Off, FG Off, camera observer Off. All six FG runtime DLLs remain staged.
- Updated the INI manifest hash/status; all **14** installed payload hashes
  match. No DLL replacement or source build was needed for this configuration
  comparison; prior build/test outcomes remain historical evidence.

All raw analysis, captures, decompiler files and install receipts are ignored
under `artifacts/local/visual-regression-2026-09-30/`.

## Native-presentation comparison result

The user started Skyrim through MO2 with FG Off and the isolated native route.
They reported **“fps and 3d world is good, loading screen still black.”** This
is qualitative user observation; no matched numerical FPS or colour capture
was supplied for this trial. The new 18:30:02 session was saved as ignored
`artifacts/local/visual-regression-2026-09-30/native-1830-session.log`:
750 lines, SHA-256
`2fd17f2e53c82400caa94f7d70c02e8dea2ba297ce6dd11a1eb80f739880b6aa`.
It shows the FG-off private-route trial was not armed, native lower-swap
preflight remained read-only, and the outer Present at frame 17400 returned
S_OK. The cold Loading Menu still entered native UI with spatial publication
at frame 13 before any provider submission; later world DLSS submissions
reached 8,342. Thus a DLSS-only loading gate cannot explain the cold failure.

The controlled result associates the world-image/FPS regression with the
private FG-off presentation route in this setup. The loading-background
regression persists independently in the native SR/UI route. It does not
establish the loading picture's last writer or a quantitative FPS recovery.
The safe native presentation setting remains Off in the later diagnostic
install. **Colour/FPS cause within the private route: OPEN.
Loading-picture cause: OPEN. Game FG-On: NOT RUN.**

## TRP reference and next diagnostic

The user confirmed Theo's Render Pipeline is a reference for the current
work, not a new full MFG-adoption scope. The pinned TRP commit
`ecae29e6bda56d2541715886ca2060eea6fb8f4a` separates loading artwork
transition policy from its spatial loading-background upscaler. Its root
license is GPL-3.0 with modding exceptions; no donor code was copied into
RazKolbas. A RazKolbas-specific, default-off one-shot capture was added
to inspect the reduced scene and native output at one save-load frame and one
post-world fast-travel loading frame. The sampler skips the first boot loading
screen, which the user says already looks normal. The installed diagnostic
remains FG Off; no rendering change is inferred from the pixels before game
capture and analysis.

## 0.1.135 one-shot loading-picture diagnostic

The new `Diagnostics.ProbeLoadingPicture` defaults Off and requires restart.
Its bounded scheduler selects at most one stable save-loading frame after
Main Menu and one post-world loading frame, each after 30 distinct consecutive
loading frames. On those frames only, it reads the reduced scene and native
target before the first menu PostDisplay and before Scaleform EndFrame, then
the native target after EndFrame and before Present. Raw files and descriptor
manifest go to the user's `RazKolbasCaptures` directory, outside Git. Each
stage has a 24 MiB readback cap. It does not enable FG, NR, private presentation
or camera-write observation. This is a diagnostic, not a loading repair.

Release build and CTest passed **64/64**; Debug passed **59/59**. The focused
selector ran **438 assertions in three cases**. The previous 0.1.134
installation's 14 payload hashes matched before replacement. Stage and install
verification found exactly two changed payloads: DLL and INI. The INI consists
of the exact previous native-comparison bytes plus
`Diagnostics.ProbeLoadingPicture=true`; user DLSS/FG/NR/sharpening and hotkeys
remain as they were. The prior DLL/INI/manifest are backed up under ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0135-loading-diagnostic`.
Installed 0.1.135 hashes:

- DLL: `790e1d331d63494a6ee90a2af549e9bbac6ab3a4e4a85e791500a05d2cc1a4f3`.
- INI: `f4337edfa204495cf59d4c603af07d4670785381adfe623111ed4fd952a04eb1`.
- Manifest: `db64ad6318c2bab8f4e25f6ec51eae9adc7f7818041f253acbe375f636d9abc9`.

All 14 installed payload hashes match the manifest. **Game loading-picture
capture and visual repair: NOT RUN. Game FG-On: NOT RUN.** The next evidence is
one user-started save load and fast travel with FG Off, followed by raw-stage
inspection and a targeted repair.
