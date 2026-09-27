# AIO native UI RE23: local cross-check

Reviewed 2026-09-27 against the immutable
`C:/Users/user/Downloads/AIO_Native_UI_RE_23.zip` packet. The packet is
reference evidence, not product code or a Skyrim patch. Its 78 payload hashes
passed the included verifier. The original AIO host DLL still matches SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
Local read-only PE mapping verified all 19 bounded code-span hashes and three
embedded shader blob hashes. The packet's strict-token scalar shader probe
passed 11 finite cases locally. Its 173 original-CPU assertions per compiler
are packet-reported; they were not rerun here and are not GPU/game tests.

## Material resource distinction

Capstone 5.0.7 independently confirmed the host's two four-global writer
bodies. AIO DLL RVA `0x1566f0` normally writes display width/height from host
`+0x24/+0x28` to four pointers at `+0x728/+0x730/+0x738/+0x740`. Its Modex
suppression is conditional on frame count at least 600 and unsigned age less
than two. DLL RVA `0x1567b0` writes render width/height from `+0x2c/+0x30`
under its distinct CS/Modex conditions, then forwards its saved target.
Both installers can address the same game call and the later installer reads
the then-current call target. The **live installed relay order and final
consumer values are unmeasured**; the individual writes alone do not define
the final game-visible dimension policy.

The snapshot at DLL RVA `0x1f5ff0` targets host `+0x490`, the HUD-less/base
role, via transfer helper `0x1f6c30`. This is not a precedent for publishing
the reduced scene over already-composed native inventory pixels. The optional
extraction path `0x1f5480` compares current native colour to that base and
draws to separate UI resource `+0x3e0`. The packet's exact shader model emits
transparent black for RGB differences no greater than float32
`0.0010000000474974513`, otherwise the *current full RGB* with alpha one.
It cannot rerasterize low-resolution text at native resolution, and a stale
or differently processed base would make the difference mask misleading.
The packet's synthetic already-processed plain-SR pre-Present scenarios
record no additional image copy/draw. Direct-UI scenarios take separate
native-layer paths. These are synthetic branch outcomes, not observed live
routes in the user's SDR/ENB/ReShade configuration.

## Exact game-side candidate and read-only probe

The V5.4 Stock Game executable, preserved decoded `.text` and installed
1.6.1170 Address Library identities remain as recorded in
`AIO_NATIVE_UI_RE22_AUDIT.md`. Hash-verified Address Library ID `106583`
maps to game RVA `0x14b2d90`; `+0x84` is an aligned `E8` CALL at
`0x14b2e14` to `0x14b4fe0`. The alternate `+0x8e` falls inside a later
instruction. ID `99938` is absent from this installed 1.6.1170 library.
The four packet IDs `411483..411486` map to adjacent game RVAs
`0x328cc44/48/4c/50`, two width/height pairs within the graphics state.
Their authoritative game-variable names and actual live writers/consumers
are not established by this static mapping.

The hash-gated `tools/re/inspect_live_detours.py` now reports both pairs and
the direct-call owner/bytes for the dimension-writer, mouse metadata,
screen-size and internal-native-transition sites during a user-started
game. It only reads process memory; it installs no hook and sends no input.
Its pure decoder/pair regression first failed for missing behavior and then
passed two cases. The tool has **not run against a live 0.1.102 process**.

The next run should first settle 0.1.102 inventory list/cursor interaction.
The read-only dimension probe can then distinguish current pair values and
site ownership at title screen and loaded HUD. It cannot alone prove the
final values at a callee, pixel raster extent, or scene/UI copy order. Those
still require a bounded draw/capture trace before a rendering patch is made.
