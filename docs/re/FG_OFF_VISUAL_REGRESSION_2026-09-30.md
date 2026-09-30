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

All 14 installed payload hashes match the manifest. The one-shot game captures
were subsequently **RUN** as described below. Visual repair and game FG-On
remain **NOT RUN** at this checkpoint.

## 0.1.135 game capture result and bounded 0.1.136 trial

The user loaded the same save and fast-travelled with FG Off, then confirmed
the artwork was nearly black **throughout** each loading screen. Two complete
six-snapshot bundles were saved outside Git:

| Context | Frame | Capture manifest SHA-256 |
|---|---:|---|
| Save load after Main Menu | 18314 | `ddb48b64fe302f7a1ba4220930013fa4fd373fa231dbc4eb8e81ab79e8c67467` |
| After-world fast travel | 20180 | `62cf1a702d20e7ccdf75bc6a1ed4d3d777e79e386b2d55ca3925b4a64eda6327` |

The saved 0.1.135 session has 1,167 lines, SHA-256
`ef169a9eb5033f4f9e279bb179a750707611393aa1043193eb173d918289adfb`.
Both captures have complete manifests and verified per-stage hashes. In the
save-load frame, the native target's mean RGB changes from **157/132/92**
before the first menu PostDisplay to **5/5/5** before Scaleform EndFrame. Fast
travel changes from **47/48/40** to **9/8/6**. The reduced scene is byte-for-byte
unchanged across those two stages in each frame. The native target then stays
byte-for-byte unchanged after EndFrame and through pre-Present. Visual previews
show the loading artwork and text are present in the very dark target. Thus the
art is not simply absent, and the dim result is established during the menu
draw or its native-route handling; later Scaleform flush and pre-Present
publication do not cause the drop. This does not yet prove the exact draw
call, blend state or effect owner.

TRP's current HEAD `cbe7504afda28a84fca0b6c040522b6b692d18ca` still
keeps a separate spatial loading-background evaluation and resets temporal
history on return to world rendering. It also has an exact Skyrim loading
transition policy and optional fade for forced artwork. Those are design
references only; the live RazKolbas capture already contains artwork, so a
missing-artwork hook would not explain these pixels.

The independent FPS lead is concrete but unproven as the whole cause: TRP's
pinned interop allocates a ring of command allocators/lists during setup and
waits when reusing a slot. RazKolbas's private bridge currently allocates an
allocator, command list and fence and waits on a CPU event for each submitted
copy. The controlled native-route A/B shows that path affects world image/FPS;
it does not measure how much time the command-object and CPU-wait sequence
contributes separately. No TRP code was copied.

Source 0.1.136 adds default-off `ProbeLoadingReducedRoute`. After Main Menu,
this isolated FG-off trial leaves Loading Menu drawing on the reduced scene
and uses spatial pre-Present publication, skipping DLSS for those frames and
resetting its temporal history through the existing spatial fallback. Initial
pre-menu loading and ordinary world/native HUD remain on their prior routes.
Release CTest passed **64/64** and Debug **59/59**. **0.1.136 game test: NOT RUN;
visual repair: NOT VERIFIED; game FG-On: NOT RUN.**

### 0.1.136 installed trial and first game captures

The 0.1.136 staged mod was installed after verifying all 14 payloads. The
previous 0.1.135 DLL, INI and manifest were copied to ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0136-reduced-loading-trial`.
Only the DLL and INI changed; the INI preserves the previous bytes and appends
`Diagnostics.ProbeLoadingReducedRoute=true`. FG remains Off. Installed SHA-256:

- DLL: `71e081de66e2959b17d18600459c8a88b4b23466c7157fc39d259521370f1bde`.
- INI: `d9f279fc0058b54f114d3674e5748e7628c26ff9589585db0cee434994219bf8`.
- Manifest: `2721c52029f996d2001fbe6f27ec45ace3de287eb24723c409b12c193660b11a`.

The user started Skyrim through MO2. The log confirms the reduced-loading
trial activated at frame 8099. The one-shot save-loading frame 8128 and
after-world frame 9611 captured four stages each. The reduced scene already
contains loading artwork. With menu-boundary publication deferred, the native
target remains unchanged through Scaleform EndFrame; spatial publication at
pre-Present then copies the reduced artwork to native size. The native
pre-Present mean RGB was **39/36/44** for save loading and **74/70/58** for
after-world loading, versus **5/5/5** and **9/8/6** in the prior 0.1.135
captures. The artwork differs between runs, so these means are supporting
evidence rather than matched-image brightness ratios. Visual previews of both
new pre-Present frames show legible artwork and text. Capture manifests are
complete, with SHA-256 `4c36305a00ac3742146fe5f2c3d5dfc2e77c7622ca1cac1772f48fe6121f7ab2`
and `9302b7e38135aecfe3440590a382da09c941bf2621c27b871582368db42bd888`.

The user judged both loading screens visually bright again, but reported the
artwork was pixelated because it was rendered at the reduced resolution before
being enlarged. The trial therefore isolates the darkening to the native
loading/UI route, but is not an acceptable final-quality repair. In the prior
native-route log, the loading frame enters PostDisplay with a 2560x1440 RTV,
DSV and viewport while its Scaleform movie still reports a 1485x835 buffer;
before EndFrame the scissor is 1485x835. The exact menu call or render state
that darkens the native target has not been identified.

**0.1.136 game capture and user visual test: RUN. Darkness improved;
native-resolution quality: FAILED. Permanent repair: OPEN. Game FG-On:
NOT RUN.**

### 0.1.137 native loading-menu sequence diagnostic

The next bounded diagnostic reuses the existing per-menu native target
readback. When `ProbeLoadingPicture` selects a stable loading frame and the
native UI route is active, it records changed native full frames before up to
16 individual `PostDisplay` calls, plus the EndFrame and pre-Present
boundaries. At most 12 intermediate images are retained. This locates the
menu-call interval that darkens the picture without changing production
rendering. The known Ghidra/Capstone trace establishes that these individual
calls precede the common Scaleform EndFrame; the capture will provide the
missing per-call pixels. The default-off probe remains bounded to one save
load and one after-world load.

Release build and CTest passed **64/64**; Debug build and CTest passed
**59/59**. The 14 staged payload hashes and all 14 old installed payload
hashes matched their manifests. Only the DLL and INI changed. The INI changes
`ProbeLoadingReducedRoute=true` back to `false`, keeps
`ProbeLoadingPicture=true`, and otherwise preserves user settings. The
0.1.136 DLL/INI/manifest backup is under ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0137-native-loading-sequence`.
Installed 0.1.137 SHA-256:

- DLL: `57a19f1070dec90499f9882ceafefaa449cc5e7447ef0bde1f4d1c5fb21c50f0`.
- INI: `ef2bf95d3c675e94cc2df497301e98e3e8f4af58a966f50a820851edc225296c`.
- Manifest: `c5d953b0137762df55a7c3beef1ba4794640bd5fce278fe2145be9749a8ae571`.

All 14 installed payloads match. The existing MO2 `meta.ini` was preserved.
The user started the game and completed both loads. The two per-menu manifests
are complete with SHA-256 `f17362b17cef0136a26a8ab26b9281e7e46ceb0d83551f36613ce4cdc57a0f79`
(save load, frame 21983) and
`3594812dae9085c3bf8c93ef10ab587df3cd71ee0d4cb66481f3ced12fdd69d1`
(fast travel, frame 23654). The companion picture-capture manifests are also
complete; the fast-travel one hashes to
`a1299559389676c52366b0f5592d64ff3ad1532ffbbf2f2f9d6b6664935a202f`.

Both native sequences logged five retained menu-boundary readbacks labelled
SkyParkour, TrueHUD, HUD Menu, Fader Menu, Mist Menu. The readback labelled
before HUD Menu remains
bright (mean RGB **50/48/63** on save load, **56/58/50** on fast travel). The
next readback, before Fader Menu, is essentially black (**0.01/0.01/0.01** and
**0.02/0.02/0.02**). Respectively **3,686,297** and **3,629,862** RGB pixels
change during that one HUD Menu call interval. Fader and Mist Menu then add
small amounts of picture/text onto the black native target; final mean RGB is
**5.8/5.1/4.8** and **3.3/4.0/3.6**. EndFrame and pre-Present are unchanged.
The reduced picture remains byte-for-byte unchanged across this interval.

At this stage the ordinal labels suggested a HUD Menu transition. The later
0.1.139 capture and the Ghidra/Capstone call-site check below correct that
attribution: the first hook callback arms the sequence, so the retained labels
lag the actual menu calls by one. The native target is wiped across the Fader
Menu call interval. A whole-target clear is plausible from the almost
entirely black result, but these captures do not distinguish an explicit
D3D11 clear from a full-screen draw.
The next isolated hypothesis is to preserve the already-published native
background across this interval, then let the later loading-menu calls draw.
That still requires a game visual test for brightness, full-resolution
quality, UI layering, and transition safety. **0.1.137 game capture: RUN;
permanent repair: OPEN; game FG-On: NOT RUN.**

### 0.1.138 native-background restore trial

Source 0.1.138 adds default-off, restart-scoped
`Diagnostics.ProbeLoadingNativeRestore`. With FG configured Off, after the
Main Menu it copies the already published full-resolution native target at
the start of each Loading Menu UI sequence, then copies it back immediately
before the Fader Menu callback if a HUD Menu callback was observed in that
same frame and resource generation. Later menu draws remain native. The
guarded copy checks an immediate context, device, texture identity, format,
size, frame, and generation. It does not affect ordinary world frames or FG-On
configuration. This is a hypothesis test: the prior capture established the
darkening interval, not whether reusing the earlier native target produces
the right image and layering.

A WARP test wipes a captured native RTV, rejects a wrong-frame restore, then
restores the exact sampled colour with the RTV still bound. Release CTest
passed **64/64** and Debug **59/59**. The staged and prior installed packages
each verified **14/14** hashes; only DLL and INI changed. The INI preserves
the existing settings and appends `ProbeLoadingNativeRestore=true` while
leaving FG Off and the reduced-loading trial Off. The 0.1.137 DLL, INI and
manifest are backed up under ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0138-native-background-restore`.
The installed 0.1.138 package verified **14/14** hashes; its DLL is
`4878793cf9925dda8dc53b425cf58af3e9b30878063396ed3594ee5b6cc1cbc7`,
INI is `166d86f7a95eff479ddaf82830656e68b829619e9c9904cc913637bc1a226251`,
and manifest is
`0c3222607c7a39546317b43d37550c0af2f73acb7cf453458087feeb935fd29b`.
The game was closed at install time and MO2 `meta.ini` was preserved. The
user has been asked to load the same save and fast-travel once. **0.1.138
game visual test: NOT RUN; permanent repair: OPEN; game FG-On: NOT RUN.**

The 0.1.138 game test was then run. The log reported a successful restore on
the Loading Menu at frame 6790. The selected save-load frame 6817 still fell
from mean RGB **100/106/117** before the menu calls to **0.03/0.03/0.03**
between the recorded HUD and Fader menu boundaries, and finished at
**4.76/4.52/4.74**. Fast-travel frame 8582 fell from **60/62/55** to
**0.02/0.02/0.02** and finished at **3.89/4.74/5.76**. The user reported the
loading pictures were still dark. The two complete per-menu manifests hash to
`7edf37f5687b9e52a7c600f5dea35e14f9e3dd5a3ff81f2bfab225ffd82277e9`
and `bb9e8c28a0e1355f2cd193c1c55489262799cee532a98cd57f5272038cf55444`.
The paired picture-capture manifests hash to
`40e8dfb4a8d2b55ebb02dcef38632de9714c56c8a7b63b8e8a6ce14f0d41575a`
and `1083cc2dd04b8c2686c3efc4ea0aad2f3d462c5d09e160dd19fa998532878fe4`.
This demonstrates that copying at the ordinal-labelled Fader entry did not preserve the final
image. There was no readback immediately after that copy, so the captures do
not distinguish a bad source/copy from a later menu redraw. **0.1.138 game
test: RUN; brightness repair: FAILED.**

### 0.1.139 later restore timing and immediate readback

Source 0.1.139 moves the guarded copy from the ordinal-labelled Fader entry
to the ordinal-labelled Mist entry. On the two selected
loading frames it also reads back `after-native-restore-native.raw` immediately
after the copy, before the Mist callback. This tests whether the saved
full-resolution image was actually placed on the native target and whether a
later menu draw darkens it again. Release CTest passed **64/64** and Debug
**59/59**. Stage and prior install each verified **14/14** payload hashes.
Skyrim was closed before installation. Only the DLL changed; the FG-off user
INI and MO2 metadata were preserved. The 0.1.138 DLL/INI/manifest backup is
under ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0139-native-background-late-restore`.
The installed 0.1.139 DLL SHA-256 is
`d1ac30e0049f000f9b246da19bf773c26f93f95d9104ad9cfef5e3ecf666f464`
and manifest SHA-256 is
`ce07730575cec174c877b036372370c54a64e8460c701c1f908bc12cbdbb9fb7`.
All **14/14** installed payloads match. The user has been asked to repeat
save load and fast travel. **0.1.139 game visual test: NOT RUN; permanent
repair: OPEN; game FG-On: NOT RUN.**

The 0.1.139 game test was run, and the user again reported dark loading
pictures. In the save-load capture, the native target before menu drawing
held a bright dragon image (mean RGB **88/83/80**). The immediate
`after-native-restore` readback reproduced those exact pixels and alpha 255;
the copy itself worked. By pre-Present the native target instead showed a
different loading picture, a mage, at mean RGB **5.82/6.47/6.08**. The
reduced target remained unchanged. The second selected fast-travel frame
started black and finished with another dark picture. The complete per-menu
manifest hashes are
`3c3eda8efb406843a2918fd3b81bb6a7e8ac8f7da5f813f86f67939ab6b2cc0e`
and `f6d0d51c8a37a8a84b8886b9d85d84881b44f86cb2092c1c12bec8ac42082643`;
the companion picture manifests hash to
`09abd773728af22d780a985c66a74769b12114be4cc74f652653d60ad50d0f12`
and `43feb4fc1343a3a9707cd6e806bb3c72d41b66a482b388b52f88c762c8452c52`.
Restoring the earlier frame is not a valid repair: it shows the previous
artwork instead of the newly selected picture and can erase loading UI.

Ghidra's decoded Skyrim 1.6.1170 function shows the hook at RVA `FA51CB`
calls the menu preparation routine before the virtual `IMenu::PostDisplay` at
`FA51D6`, then advances the stack pointer. Capstone 5.0.7 independently
decoded `call E441C0`, `mov rcx,[rbx]`, `mov rax,[rcx]`,
`call qword ptr [rax+0x30]` at those sites. The first hook callback arms the
one-shot capture, while the ordinal logger starts on the next callback. In
the observed six-menu stack, SkyParkour, TrueHUD, HUD Menu, Fader Menu, Mist
Menu, Loading Menu, the previous labels were therefore one menu behind the
active `PostDisplay` call. The bright-to-black transition is across the
actual **Fader Menu** call; the new dim artwork appears by the end of the
actual **Loading Menu** call. This corrects the earlier HUD attribution.

### 0.1.140 native Loading Menu movie viewport trial

The Loading Menu movie reported a 1485x835 buffer and viewport while the
owned native target, RTV, DSV, and viewport were 2560x1440. Source 0.1.140
adds default-off `ProbeLoadingNativeMovieViewport`, gated by FG Off and the
active Loading Menu. It temporarily gives only that movie a 2560x1440
viewport during native drawing and restores the original at the existing
frame boundary. The old native-background restore is switched Off in the
trial INI. The loading sequence now arms before the per-menu entry snapshot,
so its recorded ordinal labels align with the actual callbacks in this
observed stack. This test asks whether the movie's reduced viewport caused
the dim native artwork; it is not a proven fix. The config test was red for
the absent key before implementation, then passed. Release CTest passed
**64/64** and Debug **59/59**. **0.1.140 game visual test: NOT RUN;
permanent repair: OPEN; game FG-On: NOT RUN.**

The 0.1.140 staged package and prior installed package each verified
**14/14** manifest hashes. Skyrim was closed before installation. Only DLL,
INI, and manifest changed; the previous DLL/INI/manifest are backed up under
ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0140-native-movie-viewport`.
The INI changes `ProbeLoadingNativeRestore` from true to false, adds
`ProbeLoadingNativeMovieViewport=true`, and preserves the other user values.
The installed 14 payloads verify against the manifest. SHA-256: DLL
`478757b526ed27986c2fbcf7014847168c86c96bc31463f18b878cea694545b7`,
INI `419e731bdd6c8420e70af6002697c4b733065847688c9d417f66b1e448707799`,
manifest `ad19232732ac5a4af38032adb9daea16a77a50650e6d74676630b0f49295609b`.
The MO2 `meta.ini` hash remains
`0bea0fb065f4f86a779cca3862fca96a8994e93c13ccd9016275096b9cc925c5`.
The user has been asked for a save-load visual check before fast travel.

The 0.1.140 game run completed a save load and fast travel. The log confirms
the Loading Menu movie viewport changed from **1485x835** to **2560x1440**.
The user reported the artwork was still dark. With the corrected six-call
sequence, the save-load native target stayed bright through the callback
labelled Fader Menu (mean RGB **77.55/67.39/67.64**), became essentially
black before Mist Menu (**0.02/0.01/0.01**), and finished at
**3.82/4.76/6.45**. Fast travel followed the same pattern, finishing at
**3.45/3.68/4.41**. The reduced picture stayed unchanged across native menu
drawing. The two per-menu manifests hash to
`bec2511d14eed71189a4b156a7591eb29214520cf155964af465f00a370f862f`
and `03502f8dc76f1b6cea5894682ddacd65c1006d72a69553867e923e84e3676b95`;
the paired picture manifests hash to
`981d71b171f8a1ef79c9b16f2b4533f584518190c6ac879a87b8738176dba6fa`
and `5054ce12bffb7f637d705dba379308dcbc1619a0128aec34978a5b94b6408981`.
The viewport mismatch is real but does not explain the measured darkness.
**0.1.140 game visual test: RUN; brightness repair: FAILED.**

With Skyrim closed, the installed 0.1.140 DLL was retained and its INI
changed to the previously user-tested bright loading workaround:
`ProbeLoadingReducedRoute=true`, `ProbeLoadingPicture=false`,
`ProbeLoadingNativeRestore=false`, and
`ProbeLoadingNativeMovieViewport=false`. FG remains Off; all other user
values are preserved. Only INI and manifest changed. The previous versions
are backed up under ignored
`artifacts/local/visual-regression-2026-09-30/backup-v54-before-0140-bright-loading-workaround`.
Both staged and prior packages verified **14/14** payload hashes, and all
**14/14** installed payloads verify. Installed INI SHA-256 is
`75f5325db98a7bf73a28222bb71ae7fefbc25427e670e424bed6f2348885d3fc`;
manifest SHA-256 is
`701edfb2ca99bd43178d456a8a8457e1b07b12d00f82dfdca08eb139b69d4ac7`.
This restores legible reduced-resolution loading artwork from the 0.1.136
trial; the exact 0.1.140 INI combination has **not** had a new game run.
Full-resolution quality repair and game FG-On remain OPEN/NOT RUN.
