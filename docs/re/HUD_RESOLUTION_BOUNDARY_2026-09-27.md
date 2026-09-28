# DLSS Quality HUD resolution boundary: evidence before a shared fix

## 0.1.111 game result and remaining loading UI (2026-09-28)

In the owner-started 0.1.111 run, the live scene was 1485x835 and display
2560x1440. At frame 154977, all 14 active world HUD/widget movies reported
native 2560x1440 viewports before Scaleform `EndFrame`; the window restored
all 14 afterward with zero conflicts. Periodic restoration records continued
past 11,400 windows, and DLSS publication passed 11,000 submitted frames. The
owner observed sharper health bars and compass without new HUD issues. The
shared world-HUD repair is therefore game-verified for this setup.

The owner separately reports blurry loading-screen text/logo/UI. The existing
cold-title route excludes `Loading Menu`, and the world-HUD viewport window
does too. Source 0.1.112 adds a bounded read-only loading-menu movie and
D3D11 boundary trace. The Release build and 45/45 CTest groups pass; the
read-only probe is installed in V5.4 MO2 with all eight manifest hashes
verified. A loading-screen fix requires this distinct producer and publication
evidence; **0.1.112 game trace and loading-screen repair are NOT RUN**.

## 0.1.110 game viewport finding and 0.1.111 candidate

The owner started the installed 0.1.110 build and loaded a world with a
1707x960 DLSS render extent. The game-updated INI and live startup log identify
the requested preset as Balanced with a manual scale of 0.666667; this was
not a pure Quality-preset run. At frame 8315 (2026-09-28 00:28:34), the
read-only Scaleform snapshot
recorded 14 active movie viewports. Every recorded movie, including `HUD Menu`,
`TrueHUD`, `SkyParkour`, and the widget movies, reported buffer and rectangle
1707x960. At the same boundary, RTV0, DSV, D3D11 viewport and scissor were all
2560x1440. The movie viewport is the reduced-size producer state missing from
the prior D3D11 and graphics-dimension traces. This is a concrete reason why
replaying already-composed inventory pixels cannot sharpen the HUD by itself.

Source 0.1.111 now presents a native 2560x1440 viewport to each full-screen
reduced movie on the world-HUD stack immediately before its `PostDisplay`,
and restores the exact prior movie viewport after Scaleform `EndFrame` if no
other owner has changed it. It skips title, inventory and magic contexts,
partial/scissored movies, non-DLSS frames and any frame without the verified
native UI phase. It does not change world render dimensions or the DLSS input.
The pure viewport policy was first observed failing, then passed; the Release
build passes all 45 CTest groups. **0.1.111 Skyrim visual and restoration
verification are NOT RUN.**

## 0.1.108–0.1.110 follow-up (2026-09-28)

The owner-started 0.1.108 run logged three post-DLSS world frames (16319–16321)
at the common Scaleform `EndFrame`. RTV0, DSV, viewport and scissor were all
2560x1440, while both graphics-state dimension pairs remained 1707x960. The
owned 1707x960 SR input in the recent same-frame capture held only the world;
the final 2560x1440 image contained the bars and compass. This locates the
affected HUD after SR but does not establish where its pixels were generated.

The 0.1.109 experiment temporarily presented 2560x1440 in both graphics
dimension pairs while the world HUD producer ran, then restored 1707x960.
Its log at frames 8766–8768 confirmed native values during the Scaleform
flush and reduced values afterward, with restoration succeeding through the
run. The owner still reported blurry HUD bars and compass. This falsifies the
simple hypothesis that those two dimension pairs at the late flush are the
sole cause. The experiment is reverted in 0.1.110; its descriptor is marked
retired. Inventory/magic/title/End menu visibility remains distinct from the
HUD blur, so replaying the inventory movie without finding the HUD producer's
source size would merely replay the same soft HUD pixels.

Version 0.1.110 adds a one-time read-only `GFxMovieView::GetViewport` snapshot
for each active menu on the first post-DLSS world HUD frame. It records movie
buffer, rect, scissor, scale and flags, alongside the existing target trace.
It makes no UI state changes. Release build and all 44 CTest groups pass; the
installed DLL and eight manifest entries were verified. The owner-started
game trace above confirmed reduced HUD movie viewport geometry. A universal
HUD fix remained **NOT RUN** in 0.1.110.

The owner reports that health bars and the compass in DLSS Quality look
rendered at a lower resolution, without flicker. They request a fix for all
affected UI elements through the shared UI path. This is distinct from the
RTX 40 NR runtime selection test, which was observed with DLAA.

Two preserved, manifest-verified owned-SR captures give different placement
results. In the 0.1.84 capture
`owned-sr-stages-30080-9541-143917687`, the 1707x960 prepared input visibly
contains HUD bars, compass/direction marker and several overlay elements;
the 2560x1440 final image carries their enlarged result. In the later 0.1.106
capture `owned-sr-stages-28872-11568-108543125`, the 1707x960 prepared
input shows only the world while the final 2560x1440 image contains bars,
direction markers and other HUD. All three raw payload hashes in each
capture match their manifests. The earlier input-contamination mechanism is
real, but one newer frame shows native-stage HUD composition. Neither
capture alone proves the exact cause of the owner's current blur. The PNG
inspection copies are ignored under `artifacts/local/hud-boundary-inspection`;
the original captures remain outside Git.

The relevant shared boundaries in current source are `processOwnedWorldFrame`
(reduced prepared input and DLSS publication), `beforeMenuDisplay` (native UI
transition), `NativeUiRedirector` (target/viewport/depth translation), and
`beforeDeferredUiFlush` (common Scaleform `EndFrame` rebind). Existing RE23/24
evidence also distinguishes Skyrim's display and render dimension writers,
but their live values and the HUD draw's effective state have not been
measured in this 0.1.107 Quality configuration. Applying a HUD-specific
sharpening pass, scaling the final image, or replaying one menu would not
establish a native-resolution source for every UI element.

The installed V5.4 INI is already prepared for an owner-started DLSS Quality
run with NR enabled and explicit `ada-fastfp16`. On that run, inspect the
automatic same-frame prepared/raw/final capture; sample the exact game
display/render dimension pairs and current hook owners with the read-only
`tools/re/inspect_live_detours.py`; then check HUD, compass, text, title,
inventory, magic and the End menu by their shared draw stage. If current
prepared colour contains HUD, trace the producer before SR. If it does not,
trace effective RTV/viewport/scissor/depth and dimension metadata at the
deferred Scaleform flush. A universal repair must keep world rendering at
1707x960 and deliver each UI producer to a 2560x1440 native target with
matching geometry/metadata, without disturbing menu visibility or cursor
interaction. The owner-started 0.1.107 Quality evidence and diagnostic
checkpoint follow; the universal rendering repair is still **NOT RUN**.

## Owner-started 0.1.107 Quality run, PID 27628

The owner reports that DLAA has no HUD blur, whereas DLSS Quality makes the
health bars and compass look lower-resolution. The 22:21:44 user-started
process loaded the exact RTX 40 `ada-fastfp16` NR runtime (SHA-256
`e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a`).
Its log reports a 1707x960 scene and 2560x1440 display, and successful
pre-SR NR submissions and DLSS publication. The 14640 same-frame stage
capture is at `owned-sr-stages-27628-14640-112305640`; all three raw files
match their manifest hashes. Its prepared 1707x960 colour contains only the
world. Its final 2560x1440 composition contains the health/magicka/stamina
bars, compass, icons, HUD text and mod widgets. This rules out the earlier
pre-SR HUD contamination mechanism for this captured Quality frame. It does
not prove the HUD's *rasterized* source size merely because the final target
is native-sized. The companion menu-sequence capture shows an unchanged
centre region across the 15 `PostDisplay` entries; those commands are
deferred to the shared Scaleform `EndFrame`.

The current user token could not open this elevated game process for the
existing read-only dimension probe (`OpenProcess` denied access). A bounded
0.1.108 diagnostic therefore logs target/depth extent, viewport, scissor,
and both graphics-state dimension pairs around the common Scaleform
`EndFrame` for three post-DLSS HUD frames. It does not modify render state.
Release build and 44/44 CTest groups passed. **0.1.108 game run and universal
rendering repair: NOT RUN.**

The 0.1.108 DLL (SHA-256
`503de1aa5a8b2d071d3be67f53c514439f3194efec4757e269c832a6693a9c22`)
was installed into the existing V5.4 MO2 `RazKolbas` mod after Skyrim exited.
All eight manifest payloads were rehashed successfully. The game had
rewritten the INI in 0.1.107; its live Quality, NR, and sharpening settings
were preserved, and its new hash recorded in the manifest. The previous DLL,
INI, and manifest are backed up under ignored
`artifacts/local/v54-0.1.108-hud-trace-backup`. The next step requires an
owner-started Skyrim session; inspect the three `HUD native UI boundary`
triplets and compare the affected HUD and menu categories before altering
UI render behavior.
