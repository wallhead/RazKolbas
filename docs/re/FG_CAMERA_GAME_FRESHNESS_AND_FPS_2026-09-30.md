# 0.1.134 live camera observations and FPS regression

## Actual game outcome

Source and installed DLL for this run were `d85f6f8` / 0.1.134. The user started
Skyrim, loaded a save and reported a good image but a sustained **60 to 22 FPS**
drop. This is an unresolved performance regression. The new run's native UI
and ENB effects were not separately confirmed in the FPS report; the earlier
0.1.133 run has the explicit world/UI/ENB and Steam-overlay confirmation.

The latest 0.1.134 session starts at **13:19:15**; process PID 5252 started at
13:19:05. The exact ENB camera observer armed at 13:19:57. Actual adapter was
NVIDIA GeForce RTX 4080 SUPER (`10de:2702`), LUID `00000000:000106a6`, output
2560x1440. The private Streamline lower/facade route remained FG Off. No game
FG frame token or frame-generation submission was created by this diagnostic.

The observed writer was Map return RVA **0xe45c43** and Unmap return RVA
**0xe45d10**, matching the independent Ghidra/Capstone producer analysis in
`FG_CAMERA_WRITE_OBSERVATION_2026-09-30.md`. The selected camera buffer address
was `0x1b0f57c4df0`, generation 1. Main-menu samples advanced revisions but
decoded=false; those are not valid world-camera evidence.

DLSS SR first evaluated at frame 57924 (13:27:20); the first pooled evaluation
took 31.453 ms. Native UI routing latched at 57955 (13:27:21). At frame 74400
(13:32:37), the log reports:

- Camera revision/completed writes 891814, rejected writes 0, writer pairs 1,
  writer overflow 0.
- Current sample fresh=true, menuStable=true, decoded=true, consecutive=true.
- Totals: observed frames 74400, fresh frames 74394, stable frames 74398,
  missing frames 0, valid cameras 15847, consecutive cameras 15845. These
  totals include menu/loading time, not exclusively world frames.
- Real outer Presents 74400, failed=0, HRESULT S_OK; world forwarding 74400.
- DLSS mode 2, source 1485x835 to native 2560x1440, submissions 16477,
  fallbacksInFlight=0. Native HUD adjusted/restored 14, conflicts 0.

This validates observer admission, the live producer's addresses and the
reported freshness/phase/history checks in this run. It does not prove all
camera semantics, dynamic-object motion, complete guide/UI freshness, SL
token/marker order, input retirement or actual game FG-On display.

## Sustained throughput and the CPU probe

The final four 600-Present intervals were:

| Present range | Time range | Seconds | Real Presents/s |
|---|---|---:|---:|
| 72000–72600 | 13:30:47–13:31:14 | 27 | 22.22 |
| 72600–73200 | 13:31:14–13:31:41 | 27 | 22.22 |
| 73200–73800 | 13:31:41–13:32:09 | 28 | 21.43 |
| 73800–74400 | 13:32:09–13:32:37 | 28 | 21.43 |

Together: 2400 real outer Presents in 110 seconds, approximately **21.8/s**.
Second-resolution wall-clock logs are a coarse throughput measurement, not
per-frame GPU timings. They support the user's continuous 22 FPS report.
The isolated 31.453-ms first evaluation cannot alone explain these later
multi-minute intervals. No per-frame CPU/GPU breakdown was captured.

The saved 0.1.133 run's final 2400 outer Presents (91800–94200) took 32 seconds,
12:17:57–12:18:29, approximately 75/s. Those two runs have no recorded identical
view/location control, so this is context rather than a causal A/B result.

Code inspection shows the diagnostic checks the selected game context and
buffer using two self-ReadProcessMemory calls on each exact-context Map and
Unmap, including foreign resources. The mutex and those checks add avoidable
CPU work. The native hardware probe now accepts an optional `benchmark`
argument: five trials of 20000 Map/720-byte write/Unmap pairs each, reporting
the median CPU microseconds per pair before installing the observer, for the
selected resource after installation, and for a foreign resource afterward.
It uses the same exact ENB context and forwards ENB's complete methods.

Initial successful measurement: native 0.04793 us/pair, observed selected
1.11033, observed foreign 1.14386; deltas **1.0624** and **1.09593 us/pair**.
The same run passed the existing 240-write, replacement, foreign forwarding,
GPU-readback and altered-owner/selective-disable checks; exit 0. Release
camera/config/profile tests passed **327 assertions in 42 cases**.

The final Release harness rebuild also passed. Its rerun exited 0 with the same
240-write/readback checks: native 0.06001 us/pair, observed selected 1.06089,
observed foreign 1.13787; deltas 1.00088 and 1.07786 us/pair. These are repeat
measurements of the same synthetic workload, not a game FPS test.

This measures only a tiny synthetic CPU workload with no scene rendering,
Present or Steam/ReShade chain. Samples run sequentially, with the unobserved
baseline first; it is not a randomized or counterbalanced benchmark. Foreign
Map/Unmap counts in the game are unknown. The observed camera's roughly 37–41
writes per late world frame would contribute only tens of microseconds at
this measured rate. **The probe does not establish the cause of the full
60-to-22 loss.** No production hot-path optimization was installed before
the single-variable comparison.

## Evidence preservation and camera-Off comparison

Logs are ignored local artifacts under `artifacts/local/live-0134-fps/`:

| File | SHA-256 |
|---|---|
| `RazKolbas-0134-snapshot.log` (latest-session extraction) | `f060c6f2187296734ea58b688377e4b5b00abd77d8c4b56b94f88176e5484073` |
| `RazKolbas-0134-final-before-close.log` (complete appended log; select latest session) | `af8ed9ad55ebf20181577cdf504691f43c03da0727311897df89c4c384027bd4` |
| `benchmark-0134.txt` | `9a2e9f914a6a5a9c6678ab1d63b671ec35f0945de08feb9420eaac136e06f158` |
| `focused-tests.txt` | `8b9621ff54523f5d3ebc3fb8e71424d2f65d963d022902931a46fd1764df5f7e` |
| `benchmark-final.txt` | `7fa8eade4bc3dab3e919809db50820d5edb23e78e1db78a10ad3de04008b44d1` |
| `benchmark-build-final.txt` | `8c431416f5e9709b75a1e482a04855e21e800b36a22993152c5b8fae034f23f0` |
| `camera-off-install-receipt.json` | `a82f88ee88f6ac95b95e80c74e80adfbb7abc41790fb1b792301fe8541693af5` |

After preserving the run, the agent requested normal window closure using
the user's earlier authorization. Skyrim exited; no forced termination or
agent-started game launch occurred.

The next comparison keeps the **same 0.1.134 DLL** and changes only the
restart-scoped `Diagnostics.ProbeFgCameraWrites` from true to **false**.
The camera-On INI and manifest are backed up at
`artifacts/local/backup-v54-0134-before-camera-off/`. The target remains
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`.

- Unchanged DLL SHA-256:
  `e9c7fc54436a9f1c23c0a11ae5650badbbc630941a1d9304b9ca194d1d847e54`.
- Camera-Off INI SHA-256:
  `e1de3933bfb8a71b75676e53684314c0826388a3214d35a5c4a3ac58795e68c3`.
- Updated only the corresponding INI hash in the install manifest; all **14**
  listed installed payloads match their hashes.
- Verified the original 0.1.133 user INI remains an unchanged byte prefix.
  DLSS Balanced/M, sharpening, NR Off, FG Off, hotkeys and
  ProbeFgPrivateSwapOff=true are preserved.

**At installation time, camera-Off Skyrim performance: NOT RUN. Sustained
performance regression: unresolved. Game FG-On: NOT RUN.** The planned gate
was for the user to start Skyrim,
loads the same save/location with FG Off, waits for load completion and reports
steady FPS. Inspect the new build/session marker and the observer's disabled
path, then compare late real Present throughput. Resolve the regression before
continuing game FG-On integration.

## User-started camera-Off run

The user subsequently started Skyrim (PID 25504, process start 13:53:56.985).
The latest 0.1.134 session marker is **13:54:06**. The saved latest-session
extraction contains **zero** camera observer-arming, writer or sample log
lines, consistent with the installed restart-scoped setting false. Renderer
creation and the private FG-Off route completed; actual RTX 4080 SUPER and
2560x1440 output remain the same. The game is left running.

At frame **30000** (14:00:57), outer Present was S_OK, failed=0, with world
forwarding 30000. From frame 27000 (13:59:58) to 30000, 3000 real outer Presents
took 59 seconds: approximately **50.8/s**. World DLSS and native UI routing were
active in this interval. This is an improved coarse throughput observation,
but the user has not yet confirmed the same view/location or steady FPS.
It does not establish that all of the original loss was due to the diagnostic,
nor that baseline performance has fully returned.

Ignored snapshot: `artifacts/local/live-0134-fps/RazKolbas-0134-camera-off-snapshot.log`,
SHA-256 `3b1fc1e2788b6509e53a401767cdb83989a0cb58e341c43f942c9329b6120310`.
The camera-Off install receipt was verified before this user-started launch;
no DLL or settings were changed while the game was running.

**Camera-Off run: observed. User steady-FPS/same-view confirmation: pending.
Full performance recovery and root cause: unresolved. Game FG-On: NOT RUN.**
The user requested only this camera-Off comparison and a source/evidence push
for ChatGPT review. No production optimization or additional game experiment
is part of this checkpoint.

## Later check of the same camera-Off run

On the user's request to check the run again, the agent preserved a new
latest-session snapshot. PID 25504 remained active; the session marker remained
13:54:06. The installed INI still has `ProbeFgCameraWrites = false` and hash
`e1de3933bfb8a71b75676e53684314c0826388a3214d35a5c4a3ac58795e68c3`.
The full latest session still contains zero observer-arming, writer or camera
sample lines. No settings, DLLs or game state were changed for this check.

Frames **43200–46800**, 14:05:58–14:07:24, took **86 seconds**: 3600 real outer
Presents, approximately **41.9/s**. The six 600-Present intervals were 14, 14,
14, 15, 15 and 14 seconds, or approximately 40–42.9/s. At frame 46800, Present
was S_OK, failed=0, worldForwarded=46800. These later observations supersede
any interpretation of the earlier 50.8/s sample as steady recovery to 60 FPS.
The user's steady FPS and an identical view/location comparison remain pending;
the underlying performance cause remains unresolved.

Ignored snapshot: `artifacts/local/live-0134-fps/RazKolbas-0134-camera-off-latest.log`,
SHA-256 `0f3a1c271f3405ba6abcc9ee5451fdafa393f97dca4d5c01a09c9932e5147247`.
