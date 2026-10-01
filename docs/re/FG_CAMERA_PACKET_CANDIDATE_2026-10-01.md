# 0.1.148 world-camera packet candidate

The 0.1.147 user-started run confirmed continuous writes from the already
disassembled Skyrim 1.6.1170 camera producer: world samples were fresh,
unchanged between the post-menu-preparation and pre-Present snapshots,
decoded, and consecutive. The user reported normal image and good FPS. The
source of numeric game FPS variation remains unproven.

Source 0.1.148 adds a frame pairer. It waits for two previously observed fresh
adjacent world frames, then requires the current pre-UI and pre-Present writes
to be byte-identical, to have a strictly newer producer revision from the same
buffer generation, and to decode to consecutive current/previous matrices and
positions. A missing phase, stale revision, buffer replacement, frame gap,
invalid buffer or discontinuous camera history yields no sample. It does not
infer a camera from a mere Present number.

The owned SR evaluation now retains the exact pixel jitter it passed to NGX,
stamped with that evaluation's world source and owned-scene generation. After
the raw guide latch returns a matching real-Present packet, the code checks
the camera sample source and the jitter source/generation, derives a candidate
camera record, and attaches it to that packet. It does not set
`FgSourceFrame::cameraValid`, any guide readiness bit, an SL token, or a
provider lease. The log reports whether the producer, SR jitter and resulting
candidate were present, with bounded rejection reasons. The observation runs
only when both camera-write and frame-boundary diagnostics are enabled.

Tests were written first. The pairer test initially failed to compile because
the new class was absent; the packet test initially failed because the
candidate field and attach API were absent. Focused tests now cover a valid
adjacent sequence, phase mismatch, stale revision, buffer generation change,
world-frame gap, discontinuous decoded history, source/jitter mismatches and
the continued false camera-ready stamp. Full Release CTest passed **69/69**;
Debug CTest passed **64/64** after the 0.1.148 version bump.

The Release package was staged from the just-built DLL and exact installed
vendor runtimes at `artifacts/local/stage-v54-fg-camera-packet-0148`. Its
14/14 payload hashes verified. The installed INI was preserved except for
`ProbeFgFrameBoundaries=false` becoming true; camera observation stays true,
private FG-Off presentation stays false, and FG itself stays Off. After
Skyrim had closed, the prior DLL/INI/manifest were backed up at
`artifacts/local/backup-v54-before-0148`; only those three installed files
were replaced, then 14/14 installed hashes verified. The first replacement
preflight rejected a mistyped expected manifest hash before creating a backup
or changing any installed file; a fresh actual-hash read corrected the
comparison and the second attempt completed.

Installed DLL SHA-256:
`29b45b9cf9fdeb985237958a04709be22773e15ab17e8c94cc615c2c000a9fcb`;
INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`1cb68ea79a94a93c17b7d15bd5fa4ca8111734562a59646c70e2df4ddd5c78ab`.

**0.1.148 Skyrim candidate binding: NOT RUN. FG-On: NOT RUN.** The camera
record remains a candidate until its temporal/jitter semantics and the final
colour, converted depth/motion, transparent UI and provider retirement path
are jointly validated in a real frame. The prior 0.1.147 game build was not
replaced while Skyrim was running.
