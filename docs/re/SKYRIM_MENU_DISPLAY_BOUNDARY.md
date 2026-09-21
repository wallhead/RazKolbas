# Skyrim 1.6.1170 menu-display boundary

The 0.1.53 runtime result disproved a color-only render-target bind as the
start of native UI. Skyrim rebound the same 1707x960 owned scene with depth
later in that frame, after RazKolbas had already published an incomplete
image. This caused the reported black screen even though Present continued
successfully.

Static analysis of the exact user executable identifies a later semantic
boundary inside Address Library AE ID 82084 (RVA `0xfa4f00`). At RVA
`0xfa51d6`, the loop calls virtual slot `+0x30` on each object from the UI menu
stack. CommonLibSSE defines this slot as `IMenu::PostDisplay`. Immediately
before that virtual call, RVA `0xfa51cb` directly calls RVA `0xe441c0` after
materializing four Win64 arguments. Its original instruction is
`e8 f0 ef e9 ff`; it is ID 82084 plus `0x2cb`. The decoded `e441c0` prefix
consumes EDX, R8D and R9B, so the forwarding proxy preserves all four argument
registers and calls the original exactly once.

The owned world reconstruction now runs on the first such call while the
frame is still in `World`. A successful publication enters `NativeUi` and
stays there across `e441c0` and all following menu `PostDisplay` calls. The
existing ENB context redirector maps any cached reduced single-color UI bind
and matching viewport to the native target. The phase closes at pre-Present,
after menu drawing. A frame with no menu-display call retains the proven
0.1.52 pre-Present fallback. A new frame cannot begin while the prior frame is
still in `NativeUi`.

Installation verifies the exact executable SHA-256, image size, 38-byte
caller/continuation window, 66-byte original-target prefix and five-byte CALL.
Both the menu and world relays are allocated and sealed before either game
instruction is changed. The menu CALL is written first; if the world CALL
cannot be installed, the menu CALL is restored under the same startup
execution boundary.

Debug and Release each pass all 35 CTest groups. The Release NVIDIA harness
completed 600 pooled DLSS SR frames at 1707x960 to 2560x1440 with zero
fallbacks. Skyrim runtime behavior, native UI separation and ENB/ReShade
appearance remain **NOT RUN** for this boundary until the candidate is
installed and the user starts the game.
