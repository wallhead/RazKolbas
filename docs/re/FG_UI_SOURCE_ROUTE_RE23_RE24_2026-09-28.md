# FG UI source route: RE23/RE24 and archived-capture audit, 2026-09-28

The supplied `AIO_Native_UI_RE_23.zip` and `AIO_Native_UI_RE_24.zip` are
reference evidence, not implementation instructions. Their reported original
CPU probes were not rerun in this audit. No private AIO offset or shader blob
is copied into RazKolbas.

RE23 distinguishes a HUD-less/base snapshot from a direct UI surface and an
optional extraction surface. Its reported extraction shader compares the
current native colour with the base. It emits transparent black where RGB
agrees within about 0.001, otherwise the *already composed* current RGB with
alpha one. This is a coarse binary mask, not the original UI colour or
opacity. It also excludes Main/Loading in the reported ordinary branch. A
naive RGB difference cannot reconstruct arbitrary UI alpha: many different
UI colours and opacities produce the same final pixel over a given base.

RE24 reports a separate direct-UI surface cleared transparent with a writable
depth/stencil attachment on a recognized route. That provides a more plausible
source for a true UI plane, but the reference callback is not idempotent: an
extra invocation can clear that plane or repeat publication. Its fullscreen
helper changes more graphics state than OM targets and viewports. RazKolbas
must establish the actual draw boundary and preserve mask/stencil state before
adapting that idea to the current game route.

The existing local, ignored `magic-movie-replay-*` captures were read-only
inspected. Seven same-frame 2560x1440 RGBA8 before/after replay pairs changed
about 48.6–49.2% of pixels at an RGB threshold greater than one byte; their
changed bounds were approximately the left half `(0,0)–(1265,1439)`.
This shows that the MagicMenu replay can change a broad panel-sized region;
it does not reveal the original UI alpha or prove the current 0.1.115 route.
One older `stage-pair-6196-6613-194900234` same-frame 2560x1440 capture
changed 94.54% of RGB pixels between post-world and pre-Present at that same
threshold (mean maximum-channel difference about 7 bytes). That capture
crosses multiple processing stages. It cannot be treated as an isolated UI
draw or used as a binary FG UI mask. These raw captures remain outside Git.

The current source-only `FgUiPlanes` class accepts a genuine separate
alpha-capable UI texture and snapshots the HUD-less/final colours with one
frame identity. The current installed Skyrim path still writes UI into final
colour. A direct UI producer must be placed at the verified post-effects,
pre-UI boundary; if that cannot be established for a menu, FG must present its
real frame with generation Off. Before a game-facing tag path, capture the
same current frame at that boundary and after UI/ENB/ReShade composition for
HUD, InventoryMenu, MagicMenu and loading. Verify scene colour space, target
identity, viewport, scissor and writable stencil along with pixels. Existing
archived captures do not close those current-version checks.
