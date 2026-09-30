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
