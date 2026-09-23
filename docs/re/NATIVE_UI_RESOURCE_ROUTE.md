# Native UI resource route

## 0.1.57 sampled-depth trace and baseline correction (2026-09-23)

The user started 0.1.57, loaded a world and reported that the image still did
not look like the original ENB/ReShade output. The process remained responsive
and produced no RazKolbas warning, error or Present failure before it was
closed normally. The preserved local log is
`artifacts/local/runtime-0.1.57-2026-09-23-1106/RazKolbas.log`, SHA-256
`ca183c1dac05293b0d9cb791da031492d92bf8d06ae1ad541fcade10130a8ab6`.

Every one of the first twelve bounded menu traces contained exactly one
singleton read of the observed original depth at pixel-shader slot 3. Each
trace also contained the established eight render-target/viewport events and
seven unrelated singleton reads. This is live evidence for the reference
route's separate sampled late-depth resource and is sufficient to permit the
narrow identity-based substitution. The route also activated at frame 20,882
with zero DLSS submissions, proving that the same menu boundary can publish a
spatial baseline independently of provider success.

The run exposed a separate admission defect. The menu callback can execute
multiple times in one world frame, but `needsSample` treated an equal frame as
a timeline rewind. The log consequently contained 392 admission entries for
only 112 unique frames, repeatedly reset readiness and alternated between the
menu and pre-Present routes. Equal-frame calls are now ignored while a true
frame rewind or generation change still resets the gate.

The exact original-depth singleton is now replaced during NativeUi by a
distinct display-sized SRV backed by its own depth texture. It is separate
from the display-sized writable DSV, uses the source view formats, and is
created and cleared outside the intercepted context callback. All unrelated
slots and resources continue unchanged. WARP verifies the resource identity,
display extent and separation from both the original reduced depth and the
writable native depth.

`Diagnostics.SpatialBaselineOnly` was added as a default-off restart setting.
When enabled for the next controlled package, the owned reduced route and
native UI translation remain active while NGX submission is suppressed and
the current frame is spatially published. This provides a stable ordinary
image comparison before DLSS is re-enabled. Debug and Release each pass all
35 CTest groups. The Release RTX 4080 SUPER replay completed 600 pooled
1707x960-to-2560x1440 evaluations with zero fallbacks; output SHA-256 was
`9e24bc310a96dbeb827811488e7712457da160d242bd12370dbbdd05cdb65d37`.

This source is commit `be22ce8`. It is installed as the controlled 0.1.58
spatial-baseline package; its deployment receipt and hashes are recorded in
`../IMPLEMENTATION_STATUS.md`. Actual-game visual parity remains **NOT RUN**.

## 0.1.55 user-run evidence (2026-09-22)

The user started the installed 0.1.55 diagnostic, loaded a save and reported a
visible image that did not look like Skyrim Upscaler AIO. RazKolbas was the
enabled upscaler and the reference AIO mod was disabled. The assistant closed
the responsive game normally after collecting the bounded trace. No new crash
log was created by this run.

The first twelve frames all produced the same eight-event sequence between the
verified menu-display call and pre-ENB Present:

1. bind the owned 1707x960 scene as RTV0, one alternating 1707x960 auxiliary
   resource as RTV1, and one stable 1707x960 depth resource;
2. set a 1707x960 viewport;
3. bind the owned scene alone with the same depth and viewport three times.

The scene identity stayed `0x1b3bf5b8970`, depth stayed
`0x1b5222f1840`, and RTV1 alternated between `0x1b5222f9690` and
`0x1b5222f9980`. This is a deterministic resource-role contract. There is no
color-only/no-depth transition from which UI placement can be inferred.

The run also proves that DLSS SR itself was active. The populated scene/depth
gate became ready at frame 6301, the NGX feature was created there, and the
first pooled evaluation returned at frame 6421. Provider submissions then
advanced continuously to 5,580 by frame 12,000 with zero logged Present
failures. The effective mode was DLSS SR, with 1707x960 input and 2560x1440
output.

ENB remained active. At the two sampled frames, the reduced-scene hash was
unchanged across ENB Present while the native-buffer hash changed from
`7bc7d2ed...` to `55eb0814...` and from `ce45487f...` to `fb5ca423...`.
The visual difference from AIO therefore comes from placement: 0.1.55 lets the
four late passes render into the reduced scene, upscales that combined image
at pre-Present, and then lets ENB process it. AIO's live and static evidence
shows earlier publication followed by identity-based replacement of the scene,
auxiliary target, depth target and viewport with display-sized counterparts.

## Implemented guarded route

The next route learns and retains the two auxiliary RTV identities and the
shared DSV during the read-only startup trace, then allocates compatible
display-sized views outside the context callbacks. Stable pre-Present DLSS and
spatial fallback remain in use while the scene gate, NGX provider and
companions warm up. Only after at least one successful DLSS submission does the
menu boundary publish the current scene and enter native-UI routing.

During that phase, the verified context hook replaces the owned scene with the
current native flip RTV, either learned auxiliary with its corresponding
display-sized RTV, the learned depth with a display-sized DSV, and the reduced
viewport with the display viewport. Unknown reduced MRT or depth identities
trip the compatibility fault, permanently disable late-pass translation, and
return subsequent frames to the stable pre-Present publication path. The frame
closes at pre-Present, after the four traced passes.

WARP covers the learned scene + alternating auxiliary + depth mapping and
checks that both output attachments and depth have the display extent. Actual
Skyrim visual parity and ENB/ReShade behavior for this route remain **NOT RUN**
until the next user-started game test.

Debug and Release CTest each pass all 35 groups. The Release RTX 4080 SUPER
harness also completed 600 pooled 1707x960-to-2560x1440 DLSS frames with zero
fallbacks; output SHA-256 was
`eff793f9f4695c298308e3e55310f5c6cf63f802761419c954adddebe698c255`.

## Pass-10 history and read-side follow-up

The supplied PureDark Consolidated Verified 10 evidence identified two concrete
candidate defects. Temporal state advanced after an attempted evaluation even
when that evaluation failed, and menu publication reused a readiness gate from
the later pre-Present source phase. The presenter now tracks attempted,
successfully evaluated and successfully published frames separately. A failed
evaluation, fallback, frame gap or source-phase change forces history reset.

Menu routing now requires two independent colour/depth samples at the menu
boundary and does not require an earlier DLSS submission. This holds the
publication boundary constant when the path changes from spatial fallback to
DLSS. Probe failures are contained inside the `noexcept` callback, and the
generic menu forwarder independently guarantees original-call forwarding after
an instrumentation exception.

The verified ENB `PSSetShaderResources` slot is installed as a pass-through
observer for the first bounded menu-to-Present traces. It reports singleton SRV
reads whose underlying resource exactly matches the observed original depth and
preserves the caller's slot, count and view. It does not yet substitute a
display-sized sampled-depth SRV. The reference uses separate writable and
sampled late-depth resources, so a live matching call is required before that
resource and replacement are enabled.

Debug and Release builds pass all 35 CTest groups. The Release RTX 4080 SUPER
harness completed 600 pooled 1707x960-to-2560x1440 DLSS frames with zero
fallbacks and output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`.
