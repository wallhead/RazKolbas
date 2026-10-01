# 0.1.148 world-camera packet candidate

The 0.1.147 user-started run confirmed continuous writes from the already
disassembled Skyrim 1.6.1170 camera producer: world samples were fresh,
unchanged between the post-menu-preparation and post-world-hook snapshots,
decoded, and consecutive. The user reported normal image and good FPS. The
source of numeric game FPS variation remains unproven.

Source 0.1.148 adds a frame pairer. It waits for two previously observed fresh
adjacent world frames, then requires the current pre-UI and intended pre-Present writes
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

## User-started 0.1.148 runtime result

The user started Skyrim through MO2 with FG Off, loaded the save, and reported
normal world image and FPS. In the actual call order the outer world-hook
`observeGameCameraWrites(..., false)` runs **after** the DXGI pre-Present
callback. At real boundary/world frame 12,000, the raw world guides and
matching SR jitter were present, but `cameraProducer=false` and
`cameraCandidate=false`; the candidate attach rejected the older camera
source. The later post-world-hook observer reported one writer, 161,116
completed writes, zero rejected writes, a fresh and menu-stable decoded
camera with consecutive history. The outer Present returned `S_OK` with zero
failures. These observations establish the phase-order bug; they do not
establish a working candidate or FG submission.

The ignored live log snapshot is
`artifacts/local/fg-camera-candidate-0148-live-snapshot.log`, 24,146,831 bytes,
SHA-256 `916c41534d8a2b6e2a5c246ed7626184c3cc7b6da6a966b99f81423426dc3d52`.
The appended log's 0.1.148 session begins at the 11:17:59 marker. Skyrim
was running when the snapshot was copied and closed afterward.

Source 0.1.149 moves the pairer's second sample into
`sampleFgFrameBoundary`, after the real boundary identifies its world source
and before the raw guide packet is inspected. The later outer-hook observer
still logs diagnostics but cannot publish a camera candidate. The pairer
continues to reject a missing or different pre-UI sample, so this change does
not relabel an earlier frame as current. Release build/CTest passed **69/69**;
Debug passed **64/64**. The full 14-file stage is
`artifacts/local/stage-v54-fg-camera-present-0149`. After Skyrim exited,
all 14 old and staged files matched their manifests, and the staged INI
matched the installed user INI. The previous DLL/INI/manifest were backed
up in `artifacts/local/backup-v54-before-0149`; only the DLL and manifest
were replaced. The installed 14/14 payloads verified. Installed DLL SHA-256:
`b1c3c88b9c3decfc3385c6d35a11f540ee923c58b690b29536fab28f48191d5b`;
unchanged INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`b5b9f9ab928b363486f99440daada73c78a24e919a884752cee6f36cda3e7940`.

## User-started 0.1.149 runtime result

The user started Skyrim through MO2, loaded a save, and reported normal
image/UI and steady FPS compared with 0.1.148. The 0.1.149 session begins
at the 11:32:14 bootstrap marker. The first logged loaded-world candidate is
frame 11,757; frame 13,200 still reports `cameraProducer=true`, matching
`srJitter=true` and `cameraCandidate=true`. At frame 12,000, the outer camera
diagnostic saw generation 1/revision 127,506, one writer, zero rejected
writes, fresh/menu-stable/decoded/consecutive true, and the outer Present
returned `S_OK` with failed count zero. The captured 0.1.149 segment has 19
logged candidate-true packets and no sampled Present line with nonzero
failure count. This validates candidate binding at the real game boundary;
it does not validate FG admission or generated output.

Ignored log snapshot:
`artifacts/local/fg-camera-present-0149-live-snapshot.log`, 24,269,863 bytes,
SHA-256 `637e696871d06840d86aa1dcfc8a042490b3650189a9fdc42a7ebf8fe471499a`.
The snapshot includes earlier appended sessions and was copied while Skyrim
was running. **0.1.149 Skyrim camera candidate: RUN/PASS. FG-On: NOT RUN.**
The camera record remains a candidate until its temporal/jitter semantics
and the final colour, converted depth/motion, transparent UI and provider
retirement path are jointly validated in a real frame. No installed files
were replaced while Skyrim was running.
