# Per-menu native UI writes, 0.1.119 game capture, 2026-09-29

The user started Skyrim 1.6.1170 with 0.1.119, loaded a save, and left the
game running for a one-shot frame capture. PID 7216 reached first native-
boundary DLSS frame 9985 after 32 SR submissions. Its menu stack/call loop
had 15 entries. The 11-file full-frame UI bundle, three-file SR bundle, and
later Inventory cursor and Magic movie replay bundles all have `complete=true`;
every raw file matches its manifest byte count and SHA-256. Captures remain
outside Git.

At 2560x1440, each `before-menu-call-N` image contains the effect of earlier
calls. Unchanged images were omitted, but their SHA-256 values remain in the
log. The changed intervals are:

| Call just completed | Menu identified from matching stack order | RGB pixels newly changed | Bounding box |
| --- | --- | ---: | --- |
| 1 | TrueHUD | 28,852 | (121,1299)–(535,1373) |
| 6 | resistWidget | 5,691 | (153,1227)–(511,1279) |
| 7 | goldWidget | 653 | (372,1395)–(418,1416) |
| 8 | weightWidget | 1,856 | (185,1392)–(283,1418) |
| 9 | lvlWidget | 9,236 | (33,1273)–(158,1417) |
| 12 | gametimeWidget | 1,521 | (1863,1331)–(1954,1371) |
| 13 | equipWidget_STB | 15,629 | (775,1276)–(1776,1399) |
| 14 | HUD Menu | 78,250 | (820,39)–(2525,1426) |

The last interval ends before the `EndFrame` call and also includes
RazKolbas's pre-flush native RTV/depth rebind. That rebind does not write
native colour pixels by design; the attribution to HUD Menu is therefore
strong but still an inference about the uninstrumented individual draw calls.
The first interval's source is likewise identified by call order, not a
captured GPU command call stack. No pixels changed across `EndFrame` or from
there to pre-Present. No alpha bytes changed in any pair. The previously
sampled opaque final target is not a transparent UI plane.

The changed rectangles match the lower-left bars/widgets, lower-centre
compass and subtitles, and upper-right quest/mod text in the full-frame
capture. The log recorded native RTV and depth at 2560x1440, native viewport
and scissor, and 14 Scaleform movies adjusted from 1485x835 to 2560x1440
for this frame. This identifies concrete UI producers for a separate target;
it does not establish their source alpha, blend write mask, or FG input tag.

The assistant closed PID 7216 after the bundles completed. The normal INI
was restored to SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`;
all eight installed payloads match the updated local manifest. The 0.1.119
game run tested DLSS SR and native UI diagnostics, **not FG-On**. The next
implementation milestone is a guarded transparent native UI target and
composition, with a same-frame alpha/composite capture before FG tags are
enabled. D3D11 blend/depth/stencil behavior and current ENB/ReShade ordering
must be checked against measured pixels in the user-started game.
