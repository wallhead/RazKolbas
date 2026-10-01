# 0.1.147 camera observer hot path

The 0.1.134 Skyrim trace identified the exact game camera writer and reported
fresh, menu-stable, consecutive decoded world samples. The same run sustained
about 22 real Presents/s, but its location was not controlled against the
earlier run, so that observation did not establish the camera hook as the full
FPS cause. A later user comparison found similarly lower FPS with another DLSS
setup. The 0.1.146 private-route FPS change likewise has no proven RazKolbas
attribution. Neither observation should be promoted to a causal claim.

The old ENB Map/Unmap observer took its mutex and read both Skyrim globals for
every resource on the exact immediate context, even when the resource was not
the selected 720-byte camera buffer. A direct exact-ENB CPU benchmark measured
1.13835 microseconds of additional time per unrelated Map/write720/Unmap pair
relative to the unobserved method (five trials of 20,000 pairs; one run).
This establishes unnecessary callback work, not a game FPS cause.

The observer now caches the selected game buffer identity at installation and
each frame snapshot. Map and Unmap still forward ENB's original methods once.
Unrelated resources return without locking or reading game globals. Selected
resources keep the prior locked validation, pre-Unmap copy, generation and
caller accounting. A newly selected buffer first seen between snapshots is
intentionally skipped until the next snapshot refreshes identity; it is never
accepted from an unvalidated pointer. The existing per-frame pre-UI and
pre-Present snapshots provide refresh opportunities during the diagnostic.

The exact-ENB hardware regression was extended before the implementation.
It failed with exit code **34** on the old hook because the new buffer was
captured before its snapshot. After the change, it exits **0** in Release and
Debug: 240 fresh writes, mid-run replacement, fail-closed late replacement,
foreign forwarding, owner rejection and GPU readback pass. Full Release CTest
passed **67/67** and Debug CTest **62/62** after the source change.

Three corrected Release benchmark runs, each using five 20,000-pair trials:

| Run | Native | Selected | Unrelated | Unrelated minus native |
|---|---:|---:|---:|---:|
| 1 | 0.046975 | 1.13864 | 0.064470 | +0.017495 |
| 2 | 0.049620 | 1.19640 | 0.069485 | +0.019865 |
| 3 | 0.060695 | 1.14341 | 0.056800 | -0.003895 |

Times are CPU microseconds per Map/write720/Unmap pair. The selected camera
path still costs about 1.1 microseconds per pair. The small negative result
in run 3 is benchmark noise; no faster-than-native claim is made. The tests
use an unmodified, exact ENB DLL, synthetic game-global cells and a hardware
D3D11 context. They do not exercise Skyrim rendering, ReShade, Steam overlay
or game FPS. The raw before/after benchmark outputs are ignored local files:
`artifacts/local/fg-private-cpu-2026-10-01/camera-map-benchmark-before.txt`
(SHA-256 `2a152772dd6147fda5631ed6d38f2e748b26a6ccd38efdac07318f9e8e358077`)
and `camera-map-benchmark-after.txt`
(SHA-256 `11af63ba40898fd32a081cd765380cf68e682c23714336887fe366df2655ffc6`).

**0.1.147 Skyrim continuous camera observation and visual/FPS check: NOT RUN.**
The production observer stays opt-in. FG-On remains NOT RUN. The existing
0.1.134 writer trace is evidence for source identity and phase, not permission
to stamp future frames camera-ready without a matching current producer write.
