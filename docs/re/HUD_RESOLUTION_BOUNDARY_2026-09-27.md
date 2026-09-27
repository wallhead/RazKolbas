# DLSS Quality HUD resolution boundary: evidence before a shared fix

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
