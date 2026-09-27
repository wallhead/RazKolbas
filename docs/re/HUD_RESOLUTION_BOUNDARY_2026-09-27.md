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
interaction. **Current 0.1.107 Quality visual reproduction and repair: NOT
RUN.**
