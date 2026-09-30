# Streamline real-frame token and phase owner, 2026-09-30

Starting source: `f34046c561fd60afc7672404bae36d6aa255d162`, branch
`codex/razkolbas-bootstrap`. The user resumed FG implementation and then
explicitly chose to continue FG while recording the supplied FPS review.
This is a T18 prerequisite, not a completed Skyrim FG provider.

## Implemented contract

`FgStreamlineFrameSession` owns one official SDK token per real source frame.
Its ordered calls are token acquisition, Reflex sleep, SimulationStart,
SimulationEnd, RenderSubmitStart, RenderSubmitEnd, constants, five resource
tags, PresentStart, one lower Present, PresentEnd. The integration must call
these at verified engine phases. Generated frames never begin another session.

Source/generation/reset/present identities, owner thread and SDK numeric frame
index are checked. A token cannot be silently recycled between phases or
between constants and tags. Any partial SDK submission or failed Present
consumes the session; SDK side effects and lower Present are never retried.
The original failing DXGI HRESULT survives a failed/throwing end marker.
The existing input lease still owns resources until the provider completion
fence retires them, including uncertain/partial submissions.

The private runtime now resolves and checks ownership of the pinned core
exports `slGetNewFrameToken`, `slSetConstants`, `slSetTagForFrame` at startup.
The call factory resolves PCL/Reflex functions once and retains the runtime.
Per-frame calls do not enumerate modules or rehash DLLs. The caller remains
responsible for one pacing policy and for shutdown after GPU retirement.

The API contract follows the pinned 2.14.1 headers and NVIDIA's
[general integration](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md),
[Reflex](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
and [DLSS-G](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
guides. A correct call sequence alone does not identify Skyrim's phases.

## Actual verification

| Check | Outcome |
|---|---|
| Initial session regression | RED: missing session API; then 271 assertions/7 cases passed |
| Numeric token guard regressions | RED: changed/recycled indices accepted; then 469 assertions/12 cases passed |
| Independent review | One P2: throwing lower Present could emit PresentEnd after token mutation |
| Review regression and repair | RED reproduced; final Release session tests: 501 assertions/13 cases passed |
| Full Release build/CTest | 63/63 passed |
| Full configured Debug build/CTest | 58/58 passed; this cache has no Streamline SDK |
| Additional SDK-enabled Debug | Session plus tag tests: 642 assertions/19 cases passed |
| Dynamic private-runtime phase probe | Initialized pinned runtime/device; token, sleep and simulation/render phases succeeded; no inputs or Present attempted; shutdown succeeded |
| Synthetic DLSS-G direct D3D12 | 8 real frames: all Present S_OK, status 0, actualPresented=2, one extra supported; input fence values 1–8 retired |
| Synthetic DLSS-G through D3D11 facade + exact ReShade | Same 8-frame result, foreground verified; Off/drain/shutdown succeeded |
| Exact ENB 0.505 + ReShade 6.8 + Steam private FG-Off | 240 Presents passed, shader-input binding passed, resize 192x108 and teardown passed |
| Read-only inspector regression | Existing two Python integration tests passed; CLI help parsed |

The synthetic harness now uses the production session owner. Its guides and
camera are synthetic; these results do not establish generated Skyrim image,
HUD quality, game latency, or recovered FPS. The dynamic phase probe verifies
the separate private loader; it deliberately does not invent a game Present.

Ignored evidence directory: `artifacts/local/fg-session-2026-09-30/`.

| Evidence | SHA-256 |
|---|---|
| native-frame-phase.txt | 81b841638a37e6803a7e9b96238a55138ae45ad676d20dce55cb9e95e2951f34 |
| native-synthetic-on.txt | 459779a033e55521beb70c46b655b5d2d7e20ce2c8cc139b548d08db619220ce |
| native-reshade-facade-on.txt | 11b534707c0f62393b267decc0cd7f7fefb2e9cd7df13225c22cd47a20b4ba6b |
| native-enb-steam-off.txt | a837c1e694eb9968ab7bbcb05df673c42cf7c20b8d6c3d23aa0c4a76cf789f37 |

## Ghidra/Capstone phase research and next boundary

The exact game disk hash remains `c4342088...b73f33be9`. This analysis verifies
the complete SHA-256 of the historical decoded `.text` against its manifest:
`75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3`.
It is a prior process capture, not current loaded bytes or pristine game code.

Capstone 5.0.7 found three direct Begin/End caller ranges, decoding all bytes
within their PE unwind ranges: `0x643c00–0x64425e`,
`0x6d2120–0x6d233c`, `0x1195e1c–0x1195f29`. Ghidra 12.1.3 independently
decompiled the first two correctly based raw imports. The first reports bad
instruction/control-flow warnings and already redirected calls; it is not
accepted as a complete simulation/render control-flow proof. PE unwind ranges
may be function fragments. Multiple renderer Begin calls cannot be treated as
one simulation boundary merely from their names.

Current [Community Shaders hook source](https://github.com/community-shaders/skyrim-community-shaders/blob/5db085e77951b84bd6c191ff5b06d56893f08286/src/Hooks.cpp)
names AE ID 36544 +0x160 for its Main Update profiling hook and ID 68617
+0x7b for input dispatch. This supplies candidates, not patch authority.
The exact Address Library (SHA-256 `c4093c56...2fe0d452`, all 428461 records
consumed) maps them to `0x63ead0` and `0xcd8fbb`. The latter agrees with
RazKolbas's independently verified existing menu input call.

At `0x63ead0`, the old decoded capture contains `e8 2a 51 9a ff`; interpreted
at the saved actual game base, it points outside the captured game image.
It does not recover an original Main Update function or its current owner.
That candidate's PE unwind fragment `0x63e9a6–0x63eb3b` is 405 bytes,
SHA-256 `20321bc86c2ff0714205715d56a4f334bea10a57ff43d036dbad056dedc7ab92`.
The exploration stopped before assigning a false ABI/phase to that target.

`tools/re/inspect_live_detours.py` now includes these candidates and the
renderer Begin caller. It is bounded and read-only: no hook, remote call,
memory write, thread, breakpoint, game launch or setting change. A user-started
FG-Off run is needed to identify the current call/relay owner and obtain its
bytes. Follow with exact ABI/phase validation before any game marker hook.

Frame-phase alignment, continuous same-frame guide/UI/camera ownership,
menu/cut/resize/disable handling and generated Skyrim appearance remain open.
**Skyrim FG-On: NOT RUN. No new game-phase hook is installed.**

## Installed state

The installed 0.1.134 DLL and INI were not replaced. FG and
ProbeFgCameraWrites remain Off; the private FG-Off trial remains enabled.
All 14 installed manifest payloads were freshly checked. The six NVIDIA
runtime DLLs are in `SKSE/Plugins/RazKolbasRuntime/FG/`; the private loader
uses them for its verified presentation route. No FSR/XeSS FG runtime or
completed provider implementation is claimed by this checkpoint.

## 0.1.141 in-process phase-site snapshot

The exact on-disk Skyrim 1.6.1170 image still hashes to
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`,
but the disk bytes at RVA `0x63ead0` are encrypted and cannot disclose the
live CALL target. The prior external `OpenProcess` snapshot was denied.
Source 0.1.141 therefore adds restart-scoped, default-Off
`Diagnostics.ProbeFgGamePhaseSite`. Only after the existing full game-hash
gate, it reads the live 32-byte prefix at the candidate site and one resolved
CALL or JMP target using the already bounded, read-only code inspector. It
reports memory protection, allocation type, module owner, and at most one
additional JMP target. It does not install a hook, run the target, change a
byte, submit a Streamline marker, or enable FG. The candidate remains
unverified as a simulation/render phase until its owner, original ABI and
runtime timing are established.

The CALL decoder and opt-in configuration tests were written first; the
initial Release test build failed because `callTarget` did not exist. After
implementation, focused Release tests passed **91 assertions/9 code-inspector
cases** and **158 assertions/26 config cases**. Final Release and Debug
builds succeeded; full CTest passed **64/64** and **59/59** respectively.
No game phase hook or game FG-On run is claimed.

The 0.1.141 Release DLL and a copy of the user's verified INI with only the
phase-probe key appended were staged with the existing pinned SR, NR and six
FG runtime files. Staged and prior installed manifests each verified
**14/14** payloads; only the DLL and INI payload hashes differ. Skyrim was
closed before installing the DLL/INI/manifest. The prior three files are
backed up under ignored
`artifacts/local/fg-phase-2026-09-30/backup-v54-before-0141-phase-site`.
Installed DLL SHA-256 is
`158c938bc64179d18570361f1a150fe00d40f99ca1e77d39848e18fd602c7593`,
INI SHA-256 is
`ba89f07bca5d8e0ee70e5069fe10f3be6e0d586addc3b2eb71f8d977c1c09da5`,
and manifest SHA-256 is
`dc8d2fbc3b090c9dec7af784b898a44dd39ecc9c946f91b599463c7424fcb86b`.
All **14/14** installed payloads verify, and MO2 `meta.ini` remains
`0bea0fb065f4f86a779cca3862fca96a8994e93c13ccd9016275096b9cc925c5`.
FG stays Off and the bright reduced-loading route stays On. The user started
Skyrim through MO2 to the main menu at 20:42 Moscow time. The 0.1.141 probe
ran once in PID 24840. At game RVA `0x63ead0`, the live bytes begin
`e8cb730000` and the CALL reaches game RVA `0x645ea0`. That target begins
`ff2500000000` and jumps to `cbp.dll` RVA `0x45f00`. The exact installed CBP
file hashes to
`e1976ae08f3158eb9ecdf0df803844a5c4518705a9c7441b87bee0055103b2f1`;
its on-disk callback prefix matches the in-process bytes. The prior decoded
capture had a different redirected CALL, so its target was not used.

Capstone 5.0.7 decoded CBP's callback at RVA `0x45f00`: it calls RVA
`0x466c0`, restores its arguments and tail-jumps through an indirect slot at
RVA `0x1057a0`. Ghidra 12.1.3, using the matching CBP PDB, names these
`Render`, `updateActors` and `orender`. Its decompilation shows that
`updateActors` runs before the original trampoline. Ghidra also found
`orender` assigned in `DetourXS::Create`, called from `SKSEPlugin_Load`.
The pointer's live value and original game bytes remain unknown; the disk
slot is not initialized. Community Shaders' existing `Main_Update` profiling
hook uses the same Address Library ID 36544 + `0x160` and a
`void(RE::Main*, float)` thunk, with `FrameMark` after forwarding. This
supports a per-frame candidate but does not yet prove the Streamline marker
positions or game-frame cadence in this mod stack.

The user closed Skyrim after the capture and requested that the read-only
probe remain enabled. Installed 0.1.141 therefore retains
`ProbeFgGamePhaseSite=true`; each launch logs one startup snapshot. No game
phase hook or SDK marker is installed. **0.1.141 read-only game probe: RUN;
Skyrim FG-On: NOT RUN.** The Main Update call remains a useful independent
cadence check; the world/Present boundaries below supply the direct route
for the next integration step.

## Reference phase comparison after the live probe

The user reconfirmed that Theo's Render Pipeline (TRP) is a reference for
this work. Its source at commit
`423869f06ebef17f7cee51d7f1ce753b2cc8ac4e` starts the next source
frame after the previous real Present, calls Reflex sleep and
`SimulationStart`, submits `SimulationEnd`, `RenderSubmitStart`, constants
and guides in its native world interval, then calls `RenderSubmitEnd` before
the next Present. See its `SourceDLSSGSession::BeginFrame`, `Prepare` and
`BeforePresent`. This provides a practical phase placement model using
boundaries RazKolbas already owns: native world draw and the D3D11 facade's
real Present. It does not license copying its code or claiming its runtime
results for RazKolbas. TRP uses Streamline 2.11.1 public headers and legacy
`slSetTag`; RazKolbas's pinned 2.14.1 path uses frame-based tags, so only the
phase and ownership model transfers.

The DynamicShaderFrameGen reference at
`daaba8aadb2dbc8c5e52b028f12475c3450b6866` acquires a per-frame token
but places its `SimulationEnd` and `RenderSubmitStart` calls in its Present
path. TRP's world-interval placement is the better match for RazKolbas's
existing `FgStreamlineFrameSession` contract. A proposed additional detour
of the Skyrim Main Update call was therefore abandoned before installation;
no game code was changed by that exploration.

The remaining implementation is to connect one real source-frame session to
the owned post-Present/native-world/pre-Present boundaries and hand the
same-frame motion, depth, UI and HUD-less resources to the private D3D12
queue with retirement. First verify one update/world/Present cadence and
thread identity in the exact MO2 stack, then enable one generated frame and
measure actual presented frames plus game image/UI/ENB quality. The current
0.1.141 DLL does not yet make those calls in Skyrim.

### Same-swap Present thread migration found in the 0.1.141 run

The user-started 0.1.141 main-menu log contains successful Presents on the
same swap object `0x16b9620d0b0` with `worldForwarded` equal to the observed
Present count at every sampled checkpoint. The object is presented by thread
7816 at calls 1 and 6000, thread 15544 at 6600, thread 7816 again at 9000,
and thread 15544 at 9600 and later checkpoints. There is no intervening
plugin initialization in the log. These are sampled observations, not proof
that every unsampled frame has a one-to-one world/Present relation. They do
prove that a process-lifetime thread pin would reject this run even if each
individual frame is single-threaded.

`FgStreamlineFrameSession` now accepts a new owner thread only after its
previous real-frame Present completed. It still rejects a mid-frame thread
change and does not issue another SDK call on that rejected phase. A focused
regression failed before the change at `begin(source(2))`, then passed after
the change. Final Release and Debug builds passed; full CTest passed
**64/64** and **59/59** respectively. This is source-only; installed 0.1.141
remains FG Off and has no game session wiring.

### 0.1.142 real world/Present boundary trace

Source 0.1.142 adds `FgRealFrameBoundaries`, a read-only owner that pairs the
completed verified world callback with the next real Present attempt on the
bound game swap. It records source serial, reset epoch, world/Present thread
and cumulative ready/no-world/multiple-world/thread-mismatch counts. TEST and
foreign-swap calls do not consume a world event. A resize invalidates a
pending event; any real Present attempt consumes it even if the lower Present
later fails, so it cannot be replayed. This is an admission trace only: one
world callback does not by itself prove correct FG input guides, UI timing or
Streamline marker placement.

The diagnostic is `Diagnostics.ProbeFgFrameBoundaries`, default Off and
restart-scoped. The installed trial enables it while preserving the user's
other INI bytes and leaving `[FrameGeneration] Enabled=false`. Log kind
values are 0 ready, 1 no new world, 2 multiple worlds, 3 thread mismatch;
TEST/foreign calls are not logged. Output is limited to the first three real
Presents, each 600th real Present and the first three occurrences of each
multi-world or thread-mismatch anomaly. It never invokes a provider or
changes the world/swap image.

The tracker tests were written first and Release compilation failed for its
missing header. The config test was written first and failed for the absent
setting. After implementation, Release and Debug DLL/test builds succeeded;
full CTest passed **65/65** and **60/60** respectively. These are offline
results. **0.1.142 Skyrim boundary trace: NOT RUN; Skyrim FG-On: NOT RUN.**

With Skyrim closed, the 0.1.142 package was staged and all **14/14** payload
hashes verified. The previous installed 14 payloads also matched their
manifest. Only DLL, INI and manifest were replaced in the MO2 mod, after
backing them up under ignored
`artifacts/local/fg-boundary-2026-09-30/backup-before-0142`. Installed DLL
SHA-256 is `81b06397cac9b4bc9a3da18ea0f20adae67ec83fbcda26456cbfa5a29f9bed81`,
INI SHA-256 is `43c6a0b12ecd228b34ffb6baaccbe19221dde0dd0a2f0e9283921e34e4dfa33c`,
and manifest SHA-256 is
`1729187f9eb8c98c64a2d72732e277b331e1219686ab56fd6db13d26c448a463`.
The installed **14/14** payload hashes verify; MO2 `meta.ini` remains
`0bea0fb065f4f86a779cca3862fca96a8994e93c13ccd9016275096b9cc925c5`.
FG Off, the bright reduced-loading route, and the prior read-only phase-site
probe remain configured as before. Next, a user-started main-menu and same-save
world run must establish the actual per-frame boundary counts/thread relation.

### 0.1.142 user-started boundary result

The user started Skyrim through MO2, loaded the same save, stayed in the world
and fast-travelled. PID 19084 began at 21:24:32 local time. The live log was
copied read-only with sharing to ignored
`artifacts/local/fg-boundary-2026-09-30/runtime-0142/RazKolbas.log` (22,975,178
bytes; SHA-256
`2867ac4aa957a829c0a4df89d8f5c021361f9f100f0f598c1d962668d8c0af25`).
The snapshot contains the new trace from real Present 1 through checkpoint
19,800. All 36 emitted boundary checkpoints report `kind=0`, world serial
equal to real Present count, matching world/Present thread IDs, epoch 1, and
zero cumulative no-world, multiple-world or thread-mismatch events. The
matching outer swap reports `S_OK`, zero TEST/occluded/failed counts at the
same checkpoints. The frame thread changed from 22196 to 17512 by checkpoint
6,600, returned to 22196 at 13,800 and 15,600, and matched within each
sampled frame. This validates the completed-world-to-Present pairing over the
observed run, including thread migration between frames. Aggregate counters
cover the intervening frames; individual thread identity is logged only at
the sampled checkpoints.

The game calls this world callback during loading as well. At frame 1 the
log reports Loading Menu rendering and an unadmitted world colour/depth pair,
yet the boundary is `kind=0`. During the later loading interval, frame 6,000,
6,600 and 12,000 publications use spatial fallback (`mode=3`) with zero
provider submissions, while the boundary remains ready. At frame 18,000 the
log again reports DLSS SR (`mode=2`) and 3,439 provider submissions. Thus
boundary readiness is necessary for the Streamline frame phase, but **not**
evidence of eligible world guides or camera/UI input. Production FG admission
must combine it with the existing current-generation resource/camera/UI and
loading gates. No marker, tag, generated frame or private D3D12 swap was
enabled in this run. **0.1.142 boundary timing: RUN; Skyrim FG-On: NOT RUN.**

### 0.1.143 renderer/world phase trace prepared

The next read-only trace brackets the original world call and samples the
existing, hash-verified Renderer Begin `GetClientRect` relay when it targets
the bound game window. The ledger accepts a phase candidate only when one
renderer entry, one world entry, one completed world call and one real Present
arrive in order on one thread. TEST and foreign swap calls do not consume the
candidate; resize invalidates it. The renderer entry is only a candidate for
Streamline's early render phase, not proof of simulation start. Loading can
still satisfy timing while lacking admissible camera/guides/UI, so this trace
cannot turn FG on by itself.

Tests for missing, duplicate, late, cross-thread and resize-stale events were
written before the implementation; Release compilation failed on the absent
interface. The before/original/after world-call test likewise failed to
compile before adding the observer. Focused tests then passed. Release DLL
and test build, Debug DLL and test build, and full CTest **65/65** Release and
**60/60** Debug groups passed. These are offline checks.

Skyrim was confirmed closed before installation. The 0.1.143 package and the
installed mod both verify **14/14** payload hashes. Only the DLL and manifest
were replaced; the installed INI is byte-for-byte unchanged, with FG Off,
`ProbeFgFrameBoundaries=true`, and the brighter reduced-loading route On.
The prior DLL/INI/manifest were copied to ignored
`artifacts/local/fg-phase-2026-09-30/backup-before-0143`. Installed DLL
SHA-256 is `1e943ebca863f23279d10ff0b3ff8fc3e9274b38725eb8151f6143c8f3c6dadd`,
INI is `43c6a0b12ecd228b34ffb6baaccbe19221dde0dd0a2f0e9283921e34e4dfa33c`,
and manifest is
`578db8691e05b5302ddc08117442b0388cbef229a3eb572f58b80089672e91d4`.
**0.1.143 game phase trace: NOT RUN; FG-On: NOT RUN.**

### 0.1.143 user-started phase result

The user started Skyrim through MO2 at 21:45:36 local time (PID 11592). The
new phase fields in the live log confirm that the installed 0.1.143 binary
ran. A read-only log snapshot under ignored
`artifacts/local/fg-phase-2026-09-30/runtime-0143/RazKolbas.log` contains
23,088,092 bytes, SHA-256
`efeb6f3743d596e5e16759714143be1e3d4b280d27af27a86b6e791eccc297c6`.
It records checkpoints from real Present 1 through 20,400. All 37 emitted
phase samples have exactly one renderer entry, one world entry, one completed
world call and one real Present in order on the same thread. The cumulative
ready and phase-ready counts both reach 20,400; no-world, multi-world,
thread-mismatch and phase-rejected counts remain zero. The thread changed
between frames from 23924 to 24620 but matched within each sampled frame.

The snapshot contains Loading Menu movie observations at frames 1–3 and
15,238–15,240. Thus timing also passes while loading artwork is drawn; the
production FG admission must check loading and current world resources in
addition to this phase order. The startup log still prints the stale literal
`0.1.140` in `Plugin.cpp`; the installed DLL hash and new phase fields identify
this run as 0.1.143. No marker, Streamline submission, private lower swap or
generated frame was activated. **0.1.143 game timing trace: RUN; FG-On: NOT
RUN.**

### 0.1.143 trace-on FPS report and isolated trace-off comparison

The user reports that the image looks the same as 0.1.142 but FPS is about
six lower. The 600-frame timestamps in the two saved logs are too coarse and
span different activity to verify or dismiss a six-FPS change at a matched
viewpoint. The added renderer/world callbacks and mutex-protected phase ledger
are a plausible changed cost, not a proven root cause.

After the user closed Skyrim, the installed 0.1.143 INI was copied to an
ignored trial file and **only** `ProbeFgFrameBoundaries = true` was changed
to `false`. Reversing that exact text substitution reconstructs the original
file, including its mixed line endings. The same Release DLL and all pinned
runtime files remain byte-for-byte unchanged. A fresh package has **14/14**
valid staged hashes and differs in exactly one payload, `RazKolbas.ini`.
The prior DLL/INI/manifest were backed up under ignored
`artifacts/local/fg-phase-2026-09-30/backup-before-0143-trace-off` before
replacing only INI and manifest. The installed package again verifies
**14/14** hashes. New INI SHA-256 is
`9246b486cc214a0fc1c288e64c17835ee4d499609b8dbf71b4006a9b41914869`;
new manifest SHA-256 is
`126c013434d860ddc0ea4b6d195f981920f660177fbe90728eda3d65f24af08e`.
The DLL SHA-256 remains
`1e943ebca863f23279d10ff0b3ff8fc3e9274b38725eb8151f6143c8f3c6dadd`.
FG remains Off. **Trace-off same-view FPS comparison: NOT RUN; cause of the
six-FPS report: UNRESOLVED.**

### Trace-off user comparison

The user restarted Skyrim through MO2 and reports FPS is **still lower** with
the phase trace Off; they are willing to proceed with FG work while this
temporary diagnostic result is recorded. The read-only trace-off log snapshot
is ignored at
`artifacts/local/fg-phase-2026-09-30/runtime-0143-trace-off/RazKolbas.log`
(23,193,788 bytes, SHA-256
`ecc20379cc75740c75d6f8ea78379f0622c3d979b1d4af78861e21de813d9933`).
The later run reached DLSS world publication and successful outer Presents;
no new `FG real boundary` lines appeared after this run's startup. Skyrim then
closed. This rejects the enabled phase ledger as the **sole** explanation for
the user's perceived FPS gap. It does not isolate the remaining cause because
the game viewpoint, load progression and overlay readings were not captured
as matched quantitative measurements. The newly installed observer stubs
still execute their cheap disabled checks once per frame. No performance fix
is claimed. **FG-On: NOT RUN; FPS root cause: OPEN.**
