# Camera write observation before ENB Unmap, 2026-09-30

This report describes the prepared checkpoint. The later game run admitted the
observer and produced fresh/stable world-camera evidence, but the user reported
a sustained 22 FPS regression. Current runtime results and the installed
camera-Off comparison are in `FG_CAMERA_GAME_FRESHNESS_AND_FPS_2026-09-30.md`.
The NOT RUN statements below apply to the original preparation time.

## Purpose and runtime boundary

The user confirmed the 0.1.133 Skyrim world, native UI and ENB effects look
normal with FG Off and the Steam overlay visible. This closes the black-screen
regression gate. That run does not prove continuous camera freshness or game FG.
The agent preserved its log, requested normal window closure with user
authorization, and verified Skyrim exited before preparing a replacement.

0.1.134 adds `Diagnostics.ProbeFgCameraWrites`, default false and restart scoped.
It observes the exact game's selected constant buffer through the exact ENB
immediate context. It copies the successful Map's 720 bytes **before** forwarding
the matching Unmap, while the mapped pointer remains valid. It never reads that
pointer after native Unmap. Each completed write gets a monotonically increasing
revision, including writes whose bytes are unchanged. Revisions are evidence of
observed writes; they are not simulation-frame identities or Streamline tokens.
Every buffer/context invalidation also advances a separate buffer generation;
derived camera continuity requires fresh evidence on both adjacent observations
and the same generation, even if an allocation later reuses its old address.

The camera diagnostic does not invoke FG, change requested FG settings, issue
per-frame GPU staging reads, or alter rendering data. Existing native UI and
private FG-Off presentation stay on their established paths.

The supplied root `docs/re/` audits remain part of the evidence, including
`AIO_DLSSG_CAMERA_TOKENS_RE30_AUDIT.md`: numerically valid matrices, packet
counters and borrowed tokens do not certify a current real-frame submission.
The live upstream check of [DynamicShaderFrameGen](https://github.com/jatelop8/DynamicShaderFrameGen)
on 2026-09-30 still resolves `main` to
`879ab2c13404e02f352fa10515abd0fc25996d4f`, matching the preserved current
reference clone. Its `FrameGen.cpp` Map/Unmap code copies before Unmap and
describes multiple writers to AE Address Library ID 411384. It supplies a
research direction, not a validated writer selector for this modlist.

## Independent Ghidra and Capstone evidence

Installed Skyrim 1.6.1170 disk SHA-256:
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The protected disk text is not usable as decoded instruction evidence. This
analysis uses the previously extracted decoded text of that build, with the
same provenance as the 2026-09-29 camera trace. A bounded raw range
`0xe4583c–0xe45d29` is imported at its actual RVA, SHA-256
`b2335b92027a9f9d9a46404d3c153f794e19996d680924e1508b0b30d82b0ddb`.
Its starting address is a PE unwind-region boundary; this raw import does not
contain all external callees or game data.

Ghidra 12.1.3 successfully disassembled/decompiled that region. It identifies
the context global `0x32887b0`, buffer global `0x3288788`, calls through
context virtual offsets `+0x70` and `+0x78`, and the intervening CPU copy.
Capstone 5.0.7 independently decodes the complete bounded window without
skipped or undecoded bytes:

- `0xe45c26`: load the selected camera buffer global into RDX.
- `0xe45c40`: Map through context slot 14; return address `0xe45c43`.
- Five `0x80`-byte blocks plus a `0x50`-byte tail write 720 bytes.
- `0xe45d03`: reload the same buffer global.
- `0xe45d0d`: Unmap through slot 15; return address `0xe45d10`.

Exact ENB 0.505 input is 4,553,216 bytes, mapped image size `0xa92000`, SHA-256
`35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3`.
Its context table is RVA `0x18fa08`: Map slot 14 is `0x6b370`; Unmap slot 15
is `0x6b500`. Capstone decoded their complete PE unwind ranges
`0x6b370–0x6b4f7` and `0x6b500–0x6b74f`. Map forwards through the downstream
context's `+0x70` at `0x6b3a6`; Unmap performs additional ENB work then
forwards `+0x78` at `0x6b72f`. Ghidra independently decompiled both methods
and confirmed their downstream context member at wrapper `+0x6c68`.
This supports preserving ENB's complete call; it does not justify replacing
Unmap with a direct call to the native D3D11 context.

Ignored evidence under `artifacts/local/live-0133/`:

| Evidence | SHA-256 |
|---|---|
| `camera-producer-capstone-full.json` | `62febe4d5847fd0bcb47b9dc017b113e71ecf8a569f549641546743a85a2f6c1` |
| `enb-map-capstone-full.json` | `724ecd051c6c6f4bd01d8cf4745cb575ce3d8e6867c4d1c3ed23633ebca7a145` |
| `enb-unmap-capstone-full.json` | `26b27bc15bd8fb58b89e6485e59a661898fad9d4380f5ab33a50cb3c21bab92c` |
| `camera-ghidra-correct-base-console.log` | `1b6e6f709fe256136becf7c6325e84fc7e7de30d81b06136ec057554b7ca25df` |
| `enb-camera-ghidra-console.log` | `706684f294165b36f805b0fff4e4f31b10eded92c2c7689ad7bb693b41ce146a` |
| `camera-enb-probe.txt` | `b74b8ceb8e4c8cb19f13aae8bc06a56034aa7f03bf465fd1f555d489ce585862` |

Initial protected-disk, truncated-window and incorrectly based exploratory
reports are excluded from the instruction claims above. Only the correctly
based raw import and complete Capstone windows are used.

## Admission and forwarding

The world hook already verifies the exact executable and settings before its
renderer creation callback. Camera startup additionally requires the selected
immediate context, exact ENB file hash/size, mapped PE size, table RVA, method
ownership, both original slots and both 16-byte prologues. Both contracts pass
before either pointer changes. `PointerPatch` uses compare-and-swap; a second
slot failure restores only the first slot still owned by this observer. Callback
code, ENB code, context and state have one bounded process-lifetime lease.

Recovery IDs are `enb0505.context.camera-map-observe-v1` and
`enb0505.context.camera-unmap-observe-v1`. Disabling either declines the complete
pair. Unknown or altered owners reject; there is no byte patch or adapter-based
ownership exception.

All contexts/resources still forward the original ABI once. Observation
requires exact selected context and buffer globals, successful write Map,
subresource zero, non-null mapped pointer and a dynamic, CPU-write-only,
720-byte constant buffer with zero MiscFlags and StructureByteStride. Thread
mismatch, failed/overlapping Map or resource
generation change invalidates the captured pointer/snapshot. A retained buffer
reference protects its ownership until the next generation is selected.

At the existing after-menu-preparation/before-PostDisplay boundary and the
selected swap's pre-Present boundary, the diagnostic compares revision and
bytes. It logs bounded distinct writer pairs (16 maximum), completed/rejected
writes, freshness, phase stability, decoded matrix validity and consecutive
camera history. Unknown caller addresses remain absolute addresses and are not
silently assigned Skyrim RVAs. Logs say explicitly that no SL token/submission
is created. First samples and periodic totals are retained without frame dumps.

## Actual checks and missing verification

The tests initially failed for the missing capture class, schema field and
profile API. The new helper tests then passed; the implementation's initial
anonymous-namespace profile export caused a linker error and was corrected.
Final Release CTest passed **62/62**, Debug **58/58**. The mapped-lifetime test
uses real D3D11 WARP memory, distinct from the physical-GPU ENB probe.

`RazKolbasFgCameraWriteProbe.exe <exact-ENB-path>` runs the production installer
against the unmodified ENB DLL with synthetic game-global cells and a real
hardware D3D11 context. It exited **0**: 240 identical-byte writes each produced
a fresh revision; replacement at write 121 invalidated the old generation;
foreign buffers forwarded without being captured; actual GPU readback matched
the pre-Unmap copy; both selective disables and independently changed Map/Unmap
owners rejected without changing the other slot. One writer pair, zero rejected
target writes. This is not a Skyrim writer-address or matrix-timing result.

**0.1.134 Skyrim continuous writer/freshness/phase evidence: NOT RUN.
0.1.134 image/UI regression: NOT RUN. Game FG-On: NOT RUN.** The next necessary
gate is one user-started launch and save load with FG Off, then several seconds
of camera movement. Verify observer admission and actual fresh/stable/consecutive
counts before wiring camera samples into real frame-generation submissions.

## Review repairs and installed checkpoint

Independent review found two evidence-admission defects: the initial descriptor
check omitted MiscFlags/StructureByteStride, and derived camera continuity could
cross a buffer replacement. Three regression tests failed on the original
behavior. The complete descriptor predicate, explicit nonzero generations,
address-reuse invalidation and fresh prior-observation requirement repaired
them. Final tests passed **83 assertions in 10 camera-write cases**, independently
rerun by the reviewer in both Release and Debug; final full suites remained
**62/62 Release, 58/58 Debug**. The exact ENB hardware probe was rerun after
these changes and exited zero with the same 240-write/readback result.
`camera-enb-probe-final.txt` has the same hash as the original probe output.
The reviewer verified both repairs and the per-ID machine-readable patch
records, with no remaining blockers in the requested scope. Actual Skyrim
freshness, new-build visual regression and FG-On were excluded from the review
and remain untested.

Installed 0.1.134 in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas` while Skyrim
was closed. All **14** installed manifest payloads matched staging. Only the
DLL, INI and manifest were replaced; vendor payloads were unchanged. DLL hash:
`e9c7fc54436a9f1c23c0a11ae5650badbbc630941a1d9304b9ca194d1d847e54`.
INI hash: `96171bfd3e316e469745c23518525dad781d2b6ef52c8bc614747051f32075be`.
The old INI is retained byte-for-byte as the new file's prefix; only a new
Diagnostics section enabling ProbeFgCameraWrites was appended. SR Balanced/M,
sharpening, NR Off, FG Off, hotkeys and the private FG-Off trial stay as before.
Complete staging: ignored `artifacts/local/stage-v54-fg-camera-writes-0134`.
Verified original DLL/INI/manifest backup:
`artifacts/local/backup-v54-before-0134`. The agent did not start Skyrim.
