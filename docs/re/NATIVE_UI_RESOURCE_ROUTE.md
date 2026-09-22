# Native UI resource route

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
