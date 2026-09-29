# Branch review 31 triage, 2026-09-29

The supplied `RazKolbas_Branch_Review_31.zip` is preserved outside Git. Its
SHA-256 is `dca7920ffe293ff9d9dd61cdb5e66199a894835b99ba023e998a55bfe916bed9`.
All ten files named by its manifest matched their stated lengths and SHA-256
values. The report reviews source commit `93dd573` plus the documentation-only
`8919b5e` update. Its report and handoff are external review evidence, not
instructions to execute. The archive contains no live GPU capture or new
Skyrim result.

The reported one-frame 0.1.120 HUD-plane result is retained. The successful
sample at frame 26506 does not prove that every future native UI writer is
covered. Review finding R1 is therefore accepted as a conditional coverage
gap, not as a regression observed in that frame. The one-shot diagnostic now
captures the published native scene immediately before the UI interval as B0,
then the native target B1 and separate transparent UI target before composition,
and finally the composited output. A direct native RTV bind, the preserved
reduced menu chain, or a late reduced-scene publication marks the route partial.
The candidate is reported complete only when that route flag is complete,
B0 equals B1 byte-for-byte, and the native target identity and scene
generation match. Unknown offscreen passes are not redirected. The
displayed final composition still runs after a capture error so a diverted HUD
does not disappear. This is a guard for the probe; no game FG input consumes the
result yet. Unobserved pixel writes and a same-frame normal-render comparison
remain open limitations.

R2 was reproduced in a new failing unit test: the old numeric gate admitted a
wrong projection inverse, a singular projection and degenerate basis vectors.
It now checks the projection/inverse in both directions and rejects zero,
non-unit or nonorthogonal camera axes without assuming handedness. This checks
numeric consistency only; it does not supply or validate the game camera.

R3 was confirmed from `readbackCandidates` byte accounting. The old 32 MiB
pair and 16 MiB final budgets excluded 3440x1440 and 3840x2160 despite
allowing those dimensions. The one-shot arm now preflights the actual RGBA8
dimensions against a 256 MiB six-image-equivalent transient ceiling and uses
exact per-image readback budgets. It skips an unsupported size or failed B0
readback before diverting any UI. Compile-time checks cover ultrawide, 4K and
8K budget decisions. Runtime allocation failure can still occur and remains a
logged probe failure.

R4 is accepted as helper hardening: the premultiplied compositor now requires
an RGBA8 UNORM typed output RTV, and the probe logs that format. No evidence
shows that the 0.1.120 sample used a different view format.

Debug and Release builds each passed all 55 CTest groups after these changes.
Skyrim runtime verification is recorded separately in the implementation
status. In particular, neither this review nor these guards activate FG.
