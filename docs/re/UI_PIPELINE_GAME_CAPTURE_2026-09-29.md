# UI producer timing in the 0.1.118 game capture, 2026-09-29

The user started Skyrim 1.6.1170 with the installed 0.1.118 one-shot probe
and loaded a save. PID 29928 captured world frame 17444 at 2560x1440, after
32 DLSS submissions. The menu stack had 15 entries. Both the four-stage UI
bundle and the three-stage SR bundle have `complete=true`; every raw file
matches its manifest byte count and SHA-256. Raw data remains in the user's
`RazKolbasCaptures` directory and is not tracked by Git.

The native final target hashes were:

| Stage | SHA-256 |
| --- | --- |
| Before first `PostDisplay` | `6edf0eb09db3f0d2548b71a5f20a8451130beba5de47c8ffc01337bc3188e8b7` |
| Before Scaleform `EndFrame` | `b349ce9a8a964bf6d430e10fb23f1513a63274a71426f656e5c94cc711b4a96f` |
| After Scaleform `EndFrame` | `b349ce9a8a964bf6d430e10fb23f1513a63274a71426f656e5c94cc711b4a96f` |
| Pre-Present | `b349ce9a8a964bf6d430e10fb23f1513a63274a71426f656e5c94cc711b4a96f` |

The first interval changed 140,755 RGB pixels, bounded by `(33,39)` and
`(2525,1426)`. The changed pixels visibly form HUD bars, compass, subtitles,
quest text, and mod overlays. No pixels changed across `EndFrame` or between
that point and Present. All 3,686,400 alpha bytes in each capture were 255;
the existing final target is not a transparent FG UI plane.

This is direct evidence that visible HUD pixels reach the native target before
the verified `EndFrame` call in this frame. It corrects the earlier working
hypothesis that `EndFrame` would be the sole pixel-writing boundary. The
first interval also includes RazKolbas's pre-flush operations; the log shows a
native target/depth rebind and no menu replay for this HUD frame. Attribution
to an individual `PostDisplay` call still needs the per-entry probe.

The assistant closed PID 29928 after both bundles completed. The one-shot
INI was restored byte-for-byte to SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`
and its installed manifest entry was updated. This run did not enable FG or
establish its camera constants, tokens, alpha blend state, or a separate UI
producer. FG-On in Skyrim remains **NOT RUN**.

The next diagnostic records the native target before each menu call in the
same frame, retaining changed frames and logging the corresponding stack
ordinal/name. A capture before call N reflects the prior menu call's work;
the last menu's result is measured by the pre-`EndFrame` stage. This will
identify which interval contains the HUD draws before any UI-plane routing
change. The Ghidra/Capstone analysis in
`UI_PIPELINE_GHIDRA_CAPSTONE_2026-09-29.md` supplies the verified call order;
the runtime pixels supply the write timing.
