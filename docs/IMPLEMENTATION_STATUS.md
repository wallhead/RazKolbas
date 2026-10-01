# Implementation checkpoint

## FG real-frame owner thread handoff (2026-09-30)

The 0.1.141 main-menu log shows one swap object presenting across two threads
in the same run: thread 7816 at observed Present 6000, 15544 at 6600, then
7816 at 9000 and 15544 at 9600. Sampled `worldForwarded` counts equal the
observed Present counts. The source-only `FgStreamlineFrameSession` now allows
thread handoff after a completed real Present while rejecting mid-frame
changes. The regression was RED before the change and GREEN afterward.
Release and Debug builds passed; full CTest passed **64/64** and **59/59**.
Installed 0.1.141 is unchanged: FG Off, bright reduced-loading route On,
read-only phase-site snapshot On. Skyrim FG-On remains **NOT RUN**.
See `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.

## FG main-update candidate snapshot, 0.1.141 (2026-09-30)

The exact Skyrim 1.6.1170 disk `.text` is encrypted and the earlier external
read of live bytes was denied. Source 0.1.141 adds a default-Off, read-only
startup snapshot for the Address Library Main Update CALL candidate at
RVA `0x63ead0`, gated by the existing exact game hash. It logs the live site,
one CALL/JMP target, memory ownership and one relay jump without installing a
game hook, submitting a marker or enabling FG. A failing decoder/config test
preceded implementation. Release CTest passed **64/64** and Debug **59/59**.
The 0.1.141 DLL and user INI with only this probe appended are installed in
the V5.4 MO2 mod; all **14/14** payload hashes verified. FG remains Off and
the bright reduced loading workaround remains On. A user-started main-menu
launch ran the read-only probe. The live CALL goes from game RVA `0x63ead0`
to a game relay at `0x645ea0`, then CBP's `Render` callback at RVA `0x45f00`.
Capstone and Ghidra show that CBP updates actors and tail-jumps its original
trampoline. The user requested the one-snapshot-per-launch probe stay On.
Per-frame cadence, exact phase placement and Skyrim FG-On remain **NOT RUN**. See
`docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md` for exact hashes and test scope.

## 0.1.136 loading-route trial after pixel capture (2026-09-30)

The user reports a warmer/yellower image with RazKolbas and nearly black
loading background pictures when loading a save or fast travelling; the
initial screen before the main menu is good. Their supplied same-save,
unchanged-ENB/ReShade screenshots show approximately 41 versus 64 FPS.
This reopens visual compatibility despite earlier successful Presents and
broad image confirmation. Camera observation and actual FG remain Off.

Ghidra 12.1.3 and complete Capstone 5.0.7 decoding independently confirm that
Skyrim's framebuffer initializer uses null RTV/SRV descriptors, not an
explicit sRGB override. Runtime loading samples show a separate native
target/composite path after world admission; DLSS remains active and a
reduced scissor is observed. None of these observations yet proves a fix.
No colour filter or speculative rendering patch was added.

After Skyrim closed, prepared one isolated test by changing **only**
`ProbeFgPrivateSwapOff=true` to **false**. The installed 0.1.134 DLL,
DLSS Balanced/M, sharpening and other user settings are preserved. Independent
byte/effective-field checks passed; all **14** installed file hashes match.
The actual pre-test INI/DLL/manifest are backed up. Native-comparison INI:
`902a9e6a297070571062ff8d2df65c442169dace72ab6c90116f2702de2233bd`.

The user-started native-route comparison is now **RUN**. They report good FPS
and 3D world image but a still-black loading background. The saved 18:30:02
session has no private FG-off route, shows cold loading spatial publication
before provider submission, and reaches 8,342 DLSS submissions after world
admission. This isolates the private route as the world-image/FPS regression
suspect and leaves the loading picture as a separate native SR/UI regression.
The qualitative report has no matched numerical FPS measurement. The native
trial setting remains in the installed configuration. A default-off two-context
loading-picture capture now selects one post-menu save-load and one post-world
loading frame. Release build/CTest passed **64/64** and Debug **59/59**. The
0.1.135 diagnostic is installed, with only DLL and INI changed; all 14 payload
hashes match its manifest and the user's other settings are preserved. Game
runtime evidence for that diagnostic is **RUN**: both save-load and fast-travel
bundles were captured and the user confirms the darkness persists throughout.
The native target drops from mean RGB 157/132/92 to 5/5/5 on save load and
47/48/40 to 9/8/6 on fast travel during menu drawing. Reduced pixels stay
unchanged; later EndFrame and pre-Present pixels match the dark target. The
artwork is present, so a missing-artwork hook is not the immediate fix. Source
0.1.136 adds an opt-in reduced-loading spatial-route A/B trial, default Off.
Release CTest passed **64/64**, Debug **59/59**. Its game test is **NOT RUN**.
TRP is a reference for
current work only, per the user's clarification; no donor code was copied.

**Next: install the bounded 0.1.136 trial with FG Off, run one user-started
save-load and fast travel, and compare the loading art and raw reduced/native
pixels. Colour/loading repair unverified; game FG-On NOT RUN.**
The external read-only process inspection was denied once, error 5; no retry
or new game hook. The in-process FG phase diagnostic is deferred while these
visual regressions are isolated. Details and source/runtime evidence:
`docs/re/FG_OFF_VISUAL_REGRESSION_2026-09-30.md`.

## FG real-frame token/phase owner, source-only (2026-09-30)

The user resumed FG implementation and explicitly chose to keep doing FG while
recording the supplied FPS Review 35. The new Streamline session owns one SDK
token, ordered simulation/render/Present markers and matching constants/tags
per real frame, with numeric-token, thread, identity and no-retry guards. The
pinned private runtime now supplies the dynamic core/feature calls. It does not
install a guessed game simulation hook or enable Skyrim FG.

The production session passed real synthetic DLSS-G probes directly and through
the D3D11 facade/ReShade: **8 real frames each, actualPresented=2 on every frame,
status 0, provider input fences retired**. Exact ENB/ReShade/Steam FG-Off passed
**240 Presents**, resize and teardown. Release build/CTest passed **63/63**;
configured Debug **58/58**, plus SDK-enabled Debug session/tag tests **642
assertions in 19 cases**. Independent review's exception-cleanup token finding
was reproduced RED and fixed; final Release session tests passed 501 assertions
in 13 cases. Details: `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.

Ghidra and Capstone phase research identified multiple renderer paths and a
Main Update candidate already patched in the historical decoded capture.
A bounded read-only inspector is prepared to identify its current call/relay
owner in a user-started FG-Off run. Simulation/render timing, continuous input
ownership and **generated Skyrim frames remain NOT RUN**. No new installation
or settings change: installed 0.1.134, FG Off, camera diagnostic Off, all 14
payloads verified. FPS Review 35's verified source findings and measurement
limits are recorded in `docs/re/FPS_REVIEW35_2026-09-30.md`; FPS recovery remains
unresolved.

## 0.1.134 live camera evidence and sustained FPS regression (2026-09-30)

The user loaded a save and confirmed a good image, but reported a continuous
drop from 60 to **22 FPS**. The latest run admitted the exact ENB observer and
the statically identified Skyrim writer. At frame 74400, its camera was fresh,
stable between the menu boundary and Present, decoded and consecutive; one
writer pair, zero rejected writes and zero failed outer Presents. This is
live camera diagnostic evidence, not a game FG-On result or a performance pass.

The final logged 2400 outer Presents took 110 seconds, approximately **21.8/s**,
consistent with the report. The earlier 0.1.133 run's final 2400 took 32 seconds;
the logs do not establish an identical save/view comparison. The exact-ENB CPU
microbenchmark measures about **1.1 microseconds added per Map/write/Unmap pair**,
including unrelated buffers. It does not establish the full game's slowdown.

After preserving logs and closing Skyrim normally under prior authorization,
changed only `ProbeFgCameraWrites` to **false** for an A/B run. All **14** installed
payload hashes match; the **0.1.134 DLL is unchanged**, SHA-256
`e9c7fc54436a9f1c23c0a11ae5650badbbc630941a1d9304b9ca194d1d847e54`.
Current INI SHA-256:
`e1de3933bfb8a71b75676e53684314c0826388a3214d35a5c4a3ac58795e68c3`.
The original user INI remains its unchanged byte prefix. SR/NR/FG, hotkeys and
the private FG-Off presentation trial are preserved. The prior camera-On
INI/manifest are backed up at `artifacts/local/backup-v54-0134-before-camera-off`.

The user has now started the camera-Off run (PID 25504; session marker 13:54:06).
Its saved latest-session log contains no camera observer/writer/sample lines,
and reports zero failed outer Presents through frame 30000. Frames 27000–30000
took 59 seconds, approximately **50.8 real Presents/s**. This is improved
throughput in the current sample; identical view and steady user FPS have not
yet been confirmed. The performance cause and full recovery remain unresolved.

Later in the same camera-Off run, frames 43200–46800 (14:05:58–14:07:24)
took 86 seconds: **41.9 real Presents/s**, with individual 600-Present intervals
at 40–42.9/s. No camera observer lines or failed Presents appeared; the INI
still matches the camera-Off hash above. Thus the earlier 50.8/s sample is not
a claim of sustained recovery to 60 FPS. User same-view confirmation is pending.

**Camera-Off run: observed; steady FPS/view confirmation pending. Game FG-On:
NOT RUN.** The user requested limiting this work to the camera-Off comparison
and pushing the current source/evidence for further ChatGPT review. No further
production change is included. Runtime evidence, measurement
limits and install receipt: `docs/re/FG_CAMERA_GAME_FRESHNESS_AND_FPS_2026-09-30.md`.
The older prepared/NOT RUN entries below describe their original checkpoints.

## Source 0.1.134 camera-write diagnostic (2026-09-30)

After the user-confirmed 0.1.133 presentation pass, the next bounded FG input
prerequisite observes fresh camera writes before ENB Unmap. Default-off,
restart-scoped `Diagnostics.ProbeFgCameraWrites` validates both exact ENB slots,
captures the selected 720-byte camera buffer while mapped, and tracks writer
revisions plus menu-to-Present stability and decoded camera history. It does
not enable FG, assign Streamline tokens or change native UI rendering.

Ghidra and complete Capstone windows independently confirm the game producer
and ENB Map/Unmap forwarding. The production exact-ENB hardware probe passed
240 fresh identical writes, generation replacement, foreign-buffer forwarding,
GPU readback and altered-owner/selective-disable negatives. Release CTest
passed **62/62**, Debug **58/58**. Details and actual evidence limits:
`docs/re/FG_CAMERA_WRITE_OBSERVATION_2026-09-30.md`.

**0.1.134 game writer/freshness/phase evidence and image/UI regression: NOT RUN.
FG-On generated Skyrim frames: NOT RUN.** Next required game step remains
user-started, FG Off. The user authorized closing Skyrim; normal window closure
succeeded, and the agent confirmed no Skyrim process remained.

Independent review's descriptor/generation findings were reproduced in failing
tests and repaired; final review found no remaining blockers. Camera tests
passed 83 assertions in 10 cases in both configurations. The two hook IDs have
machine-readable records under `patches/skyrim/`.

Installed **0.1.134** while Skyrim was closed; all **14** manifest payloads
matched. DLL SHA-256:
`e9c7fc54436a9f1c23c0a11ae5650badbbc630941a1d9304b9ca194d1d847e54`.
The preserved INI bytes are the new file's unchanged prefix, with only
ProbeFgCameraWrites=true appended; SR/NR/FG and hotkeys are unchanged.
INI SHA-256: `96171bfd3e316e469745c23518525dad781d2b6ef52c8bc614747051f32075be`.
Complete staging is `artifacts/local/stage-v54-fg-camera-writes-0134`; verified
old DLL/INI/manifest backup is `artifacts/local/backup-v54-before-0134`.
No 0.1.134 Skyrim run or game FG-On is claimed.

## 0.1.133 Skyrim FG-Off presentation regression passed (2026-09-30)

The user started Skyrim (PID 25160), loaded a save and confirmed that the
world image, native UI and ENB effects look normal without a black screen.
The user also confirmed that the Steam overlay is visible. Overlay input
behavior has not been separately tested.

The current run starts at **11:56:21** with the 0.1.133 build marker.
At 11:56:52 it accepted the exact pinned Steam/native factory chain and
returned the private Streamline lower/D3D11 facade through ReShade, explicitly
keeping frame generation disabled. The actual renderer reports NVIDIA RTX
4080 SUPER (`10de:2702`), LUID `00000000:000106a6`, output 2560x1440.
The preserved log reaches **45,000 real outer Presents, zero failures**, all
observed results `S_OK`. DLSS SR first evaluated at frame 41892; native HUD
routing activated at 41923, and frame 42600 reports 709 DLSS submissions,
zero fallbacks in flight. Native HUD viewport adjustments/restores have
zero conflicts.

The final pre-close snapshot reached **94,200 outer Presents, zero failures**.
It is preserved as `artifacts/local/live-0133/RazKolbas-0133-final-before-close.log`,
SHA-256 `8b867de9ae753c103d85945d49350855f13d05fb0a96de7ea636930554346430`.

Runtime evidence: `artifacts/local/live-0133/RazKolbas-0133-run.log` (ignored),
SHA-256 `17fdc60cfaabd5334f8c2a567c5acc9785e74da475d8a6c740f17c3d140936a0`.
All 14 installed payloads and the preserved INI were freshly verified before
this launch. At this observation, source and installed DLL were `aefb7d1` /
0.1.133; no rebuild, settings change or DLL replacement preceded the successful
run. The agent did not start Skyrim.

**FG-Off game presentation and user-confirmed image/UI/ENB: PASS for this
run. Steam overlay visibility: PASS (user report). Overlay input: NOT RUN.
FG-On generated Skyrim frames: NOT RUN.** Continue the existing FG camera,
guide/UI, token/marker and input-retirement integration before enabling game
FG; the standalone synthetic FG result is not a substitute for that work.

## Source 0.1.133 ENB source ownership and presentation repair (2026-09-30)

The user-started 0.1.132 run bound the private facade successfully but showed
a black screen. Real outer Presents returned INVALID_CALL. The exact ENB,
ReShade and Steam chain reproduced this offline: after ENB wraps the device,
the retained auxiliary buffer's GetDevice reports its wrapper identity,
causing the raw copy check to reject and poison the bridge.

0.1.133 retains verified source ownership in an immutable interop-bound
lease; raw resources and unrelated contexts still reject. Resize captures
and commits the new source generation transactionally. It also preserves
and verifies the requested shader-input access on the auxiliary buffer.
Provider lifetime survives poisoned resource quarantine; first copy errors
remain available even after subsequent generic failures.

Exact ENB/ReShade probes passed **240 real Presents, shader-view creation,
resize and ordered teardown**, both with the verified Steam hook and with
the pristine native factory and no Steam overlay preload. Steam is not a
required dependency. An intentional early probe failure exited with its
expected code 42 without a teardown crash. Release CTest passed **61/61**,
Debug **57/57**; independent review found no correctness blockers.
Evidence and test limits:
`docs/re/FG_ENB_SOURCE_DEVICE_IDENTITY_2026-09-30.md`.

**0.1.133 Skyrim image/UI/ENB regression: NOT RUN. Steam overlay appearance/input:
NOT RUN. FG-On generated frames: NOT RUN.** The next necessary game step is
one user-started launch/save load with FG Off. No game launch was performed
by the agent; installation preserves the user's existing settings.

Installed in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas` while Skyrim was
closed. All **14** package manifest hashes matched. DLL SHA-256:
`ee75d0005192c88780d14a1906e3cbf7868bc7134847f2c849dca91118235209`.
The INI is unchanged (Balanced/M, sharpening 0.95, NR Off, FG Off, private
FG-Off trial enabled). Complete MO2 staging is under ignored
`artifacts/local/stage-v54-fg-enb-0133`; the preceding DLL/INI/manifest are
preserved under ignored `artifacts/local/backup-v54-before-0133`.

## Source 0.1.132 Steam overlay factory compatibility (2026-09-30)

The 0.1.131 game capture identified an existing native E9/FF25 chain before
private Streamline preparation; it was unchanged afterward. PAGE_EXECUTE_READ
and matching direct/snapshot bytes ruled out the execute-only snapshot
hypothesis. Exact Steam overlay disassembly and an isolated unmodified-DLL
reproduction identified its callback RVA 0x9c250 and original-pointer slot
0x180340. The external game module snapshot was not obtained before exit;
the new in-process validator must prove the loaded Steam owner before use.

0.1.132 validates the full pinned Steam/native identity and callback/relay/
trampoline chain, pins the Steam owner, then re-reads mutable links. Only
the local validation snapshot is normalized. The live native entry and
Steam hook are preserved; the existing factory callback saves that native
entry rather than a borrowed Steam trampoline address. Steam overlay is
not disabled or packaged. Unknown/changed owners still reject with native
fallback. Descriptor: `steam11057416.factory.inline-native-chain-v1`.

Both standalone game-route cases, with exact Steam preloaded and without
Steam, exited zero: one substitution, real FG-Off Present, resize to
192x108 and ordered teardown. Release CTest passed **61/61**, Debug **57/57**.
Independent review found no significant issues and separately passed factory
tests (84 assertions/eight cases) and owned-route tests (102 assertions/ten
cases). Details: `docs/re/FG_STEAM_OVERLAY_NATIVE_FACTORY_2026-09-30.md`.

**Subsequent 0.1.132 Skyrim result: FAIL, black screen after successful private
facade binding; real Presents returned INVALID_CALL.** Steam overlay
rendering/input and FG-On generated frames remain NOT RUN. The subsequently
reproduced ENB source-ownership cause and repair are recorded in the 0.1.133
entry above.
The complete MO2 package is staged under ignored
`artifacts/local/stage-v54-fg-steam-0132`. After confirming Skyrim was closed,
the final 0.1.132 DLL and manifest were installed in
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`; all **14** manifest hashes
matched. Installed DLL SHA-256:
`c483578f52648aea8ad9ee564fcfd49d7573e2220bddf462279b613de50f50b8`.
The previous 0.1.131 DLL/INI/manifest are preserved under ignored
`artifacts/local/backup-v54-before-0132`. The installed INI is unchanged:
SR Balanced/M, sharpening 0.95, NR Off and FG Off, with the private FG-Off
trial enabled. The staged runtime also passed the Steam-preloaded route probe.

Final recovery-control verification found that the parser's existing allowlist
omitted the native/Streamline factory IDs and the new Steam ID. Their distinct
disable bits are now accepted; duplicates and unknown IDs remain rejected.
The new regression failed at parsing before the fix, then passed six assertions.
Both full CTest presets passed again after this change. Independent review
also passed all configuration cases (128 assertions/19 cases) without findings.
The final DLL includes this correction; Skyrim was not launched by the agent.

## Source 0.1.131 native factory code capture (2026-09-30)

The user-started 0.1.130 V5.4 run prepared the private Streamline lower,
but rejected the native factory profile at the code-byte check and retired
the candidate before the game swap replacement. Native fallback, DLSS SR
and native UI continued; the user confirmed the image/UI looked normal.
This run **did not bind the FG facade or generate frames**. The code
mismatch's cause was not established at that checkpoint. Read-only external process inspection
and process termination both returned Windows access denied.

0.1.131 records bounded entry/target bytes, page protection, jump addresses,
module path/hash/RVA before and after private preparation and on profile
rejection. These observations neither authorize an unknown hook nor change
the strict validation/fallback. The standalone game-route probe now checks
the native prologue as well as its owner/table/method addresses before
patching; that stricter probe passed with real Presents and resize. Reader
tests cover forward/backward and indirect jumps, inaccessible/guard pages,
address overflow and execute-only pages. The next required game run must
identify the actual mismatch before any compatibility patch is selected.
Details: `docs/re/FG_PRIVATE_GAME_BINDING_2026-09-30.md`.

**0.1.131 Skyrim result: capture collected; private binding rejected;
native fallback preserved. FG-On result: NOT RUN.** See the 0.1.132 entry
above for the subsequently identified existing Steam factory hook.
Release CTest passed **61/61** and Debug **57/57**. Independent review found
no significant code issues and independently passed the two new reader
cases in both builds. The stricter private route probe again passed with
the installed runtime. A complete MO2 package preserving the installed
SR/NR/FG settings is staged under ignored
`artifacts/local/stage-v54-fg-factory-0131` and is now installed as recorded
below.

After the user closed Skyrim, 0.1.131 was installed in
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas` on 2026-09-30. All 14
manifest files passed hash verification. The installed DLL SHA-256 is
`97e4f1180028866113b924b577a1f8084da2d83f0280300bedcc511563c0600d`.
The previous DLL/INI/manifest are preserved under ignored
`artifacts/local/backup-v54-before-0131`. Existing settings were preserved:
FG Off, private FG-Off capture enabled. The next action is one user-started
launch to collect the before/after/rejection code evidence. That run is now
recorded above; it did not bind the FG facade or generate frames.

## Source 0.1.130 private FG-Off game route (2026-09-30)

The opt-in V5.4 creation path now links the private Streamline runtime into
`RazKolbas.dll`, prepares a managed D3D12 lower on the selected ReShade
adapter parent, and can return a D3D11 facade at the already verified native
factory callback. FG generation remains Off. The exact ReShade-first offline
probe completed real lower and upper Presents, resize, and ordered teardown;
Release CTest passed 61/61, Debug 57/57. The initial System32-factory
`E_ABORT` failure and corrected factory selection are recorded in
`docs/re/FG_PRIVATE_GAME_BINDING_2026-09-30.md`.

**Skyrim runtime result: native fallback observed; private binding rejected.
FG-On result: NOT RUN.** The first user-started run is recorded above; no
live FG claim is made.
The 0.1.130 FG-Off trial package is now installed at
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas` with all 14 manifest files
hash-verified and the previous DLL/INI/manifest preserved under ignored
`artifacts/local/backup-v54-before-0130`. The installed private runtime passed
the exact ReShade-first offline route probe. The user subsequently started
Skyrim and confirmed normal visuals while the private route fell back.

## Private ReShade/Streamline FG-Off resize (2026-09-29)

The offline D3D11 auxiliary swap now prepares a new back buffer before lower
resize and commits it only after the Streamline-owned lower swap reports the
expected extent. The exact ReShade outer swap resized from 160×96 to 192×108,
rebuilt its runtime and successfully presented again. An injected lower
resize rejection retained the old extent and following Present. A reserved
flag was rejected before the Streamline proxy after a concrete probe showed
that forwarding it poisoned the proxy's next Present. Details:
`docs/re/FG_PRIVATE_FACADE_RESIZE_2026-09-29.md`.

Release CTest passed **60/60**, Debug **57/57**, and the final private probe
exited zero. This remains offline FG-Off evidence. The private path is not
installed into Skyrim; V5.4 MO2 remains 0.1.128 with FG Off. Next is guarded
game binding and one bundled, user-started FG-Off startup/Present test before
any FG-On claim.

## Private ReShade/facade FG-Off boundary (2026-09-29)

The isolated private-loader probe now creates a Streamline-owned D3D12 lower
swap, inserts a D3D11 facade at the exact ReShade native delegate, and receives
a **distinct ReShade outer swap**. A normal D3D11 texture blocked ReShade's
sRGB render-target view; an auxiliary D3D11 swap-chain back buffer supports
both view formats. ReShade initialized its effect runtime and three upper
Presents reached the lower swap with FG Off. The probe and private shutdown
returned success. See `docs/re/FG_PRIVATE_FACADE_RESHADE_OFF_2026-09-29.md`.

Release CTest passed **60/60**, Debug **57/57**. This is an offline proof only:
external-buffer resize is explicitly unsupported, no game interception uses
the private loader, and no FG-On frame has been observed. V5.4 MO2 remains
0.1.128 with FG Off; Skyrim was not started. Next is transactional auxiliary
buffer resize/retirement, then guarded game binding and a bundled user-started
test.

## Branch review 34 corrections (2026-09-29)

The supplied branch review's new probe and loader findings were checked
against source and corrected. The private loader now holds and revalidates
the pinned runtime files during initialization, refuses a pre-existing
Streamline module, and verifies loaded module/export ownership. The
native-first probe now gates every required lower/FG-Off milestone, uses
the selected NVIDIA adapter for both APIs, and checks LUID equality.
Seven injected failures returned their expected nonzero codes; the normal
real-vendor run, foreign-module rejection and changed-byte rejection were
observed in separate processes. The two probe executables import
`CreateDXGIFactory1` from different DLLs (`sl.interposer.dll` versus
System32 `dxgi.dll`), resolving the review's factory-entry hypothesis.
Details and limits: `docs/re/FG_BRANCH_REVIEW34_2026-09-29.md`.

Release CTest passed **60/60** and Debug **57/57**. No Skyrim run or mod
installation occurred.
Private FG remains outside `RazKolbas.dll`, and the V5.4 MO2 installation
remains 0.1.128 with FG Off. The combined private-loader, ReShade-returned
swap and managed Present test remains the next FG integration gate.

## Private Streamline loader and factory pointer contract (2026-09-29)

Source now has a hash-pinned private Streamline 2.14.1 runtime loader and an
optional MO2 packaging path for the six exact NVIDIA-signed runtime DLLs.
An isolated-process probe loaded the staged runtime, bound a D3D12 device,
then shut down successfully. A mutated staged DLL was refused before load.
With exact ReShade 6.8, `slUpgradeInterface` returned a distinct interposer
factory proxy whether the ReShade factory existed before or after `slInit`;
it left ReShade's native delegate unchanged. See
`docs/re/FG_PRIVATE_STREAMLINE_RUNTIME_2026-09-29.md`.

A further private-loader probe upgraded native factory/device interfaces,
created a real D3D12 lower swap, resolved the DLSS-G option function, set FG
Off and created D3D11. ReShade's stored delegate remained native in that
process, unlike the older statically imported Streamline harness. The exact
cause of that global-hook difference is **not established**. The returned
proxy is the reliable contract under both probes.

Release CTest passed **60/60**. The private loader is not linked into the
game plugin yet. Installed V5.4 MO2 remains 0.1.128, and game FG is Off.
No Skyrim runtime verification occurred in this step. Next: route an owned
lower factory/swap through the returned Streamline proxy while retaining
ReShade and ENB's outer D3D11 contracts, then integrate the validated startup
and frame service into the game plugin before one bundled user-started test.

## Source 0.1.129 chains a pinned Streamline factory owner (2026-09-29)

The separate-process ReShade/Streamline result was converted into a guarded
factory-owner profile. If the System32 DXGI factory table is still native,
the pass-through hook uses its original profile. If its method is owned by
the **exact** Streamline 2.14.1 `sl.interposer.dll` at RVA `0x26510`, it
validates the interposer file hash, size, mapped image, RVA and 16-byte
prologue, then atomically chains that method. Any other owner is left alone.
The callback and both prior-owner modules are pinned for process lifetime.
The descriptor is
`patches/skyrim/streamline2141.factory.create-native-lower-chain-v1.json`.
The standalone ReShade probe now uses that same profile.

This 0.1.129 source does **not** initialize Streamline in Skyrim or
substitute a D3D12 lower swap, so FG remains Off. It has not been packaged,
installed or run in Skyrim; V5.4 MO2 still contains 0.1.128. Debug and
Release suites passed **57/57** and **60/60** CTest groups. The standalone
ReShade FG-Off and focused FG-On probes were rerun against the shared
owner profile: both exited 0; the FG-On run again reported two actual
presentations on each of eight submissions and completion values 1–8. A bounded
Capstone trace of exact ENB 0.505 also confirmed that after downstream D3D11
creation succeeds, ENB immediately calls the returned swap's `GetDesc` and
`GetBuffer`; see `docs/re/FG_ENB505_CREATION_BOUNDARY_2026-09-29.md`.

## Installed 0.1.128 native factory trace and offline wrapper proof (2026-09-29)

The user-started 0.1.127 run identified ReShade's native delegate with
System32 DXGI's exact vtable RVA `0xa1428` and method RVA `0x67c90`; the
existing scene path kept presenting. 0.1.128 adds a hash- and RVA-guarded
atomic pass-through hook to that native slot, observing the raw lower swap
before ReShade wraps it. The pinned WARP factory integration test forwarded
a real swap call and restored the slot. Debug and Release suites passed
**57/57** and **60/60** groups. The full installed V5.4 MO2 package and
download archive contain eight payloads matching the stage manifest. Archive
SHA-256: `2be25cb3f08e7e030a18ec527ebf63557cdd78767b71a5c908b682eafb6c92b1`;
DLL SHA-256: `428f5a4b0c48cf72861d0572b67330da89edc5b38e961c9f167edc7286cf78d9`.
The previous mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.128-2026-09-29`.
**0.1.128 has not run in Skyrim; FG remains Off in the game.**

Separate-process reverse engineering and probes then loaded the exact
installed ReShade DLL, found its native factory delegate, and showed that
Streamline 2.14.1 takes that same slot with pinned `sl.interposer.dll`
RVA `0x26510`. The probe chained it while returning the existing facade to
ReShade. ReShade's wrapper passed FG-Off colour/resize checks and a focused
FG-On run reported two actual presentations on each of eight submissions,
with completion fence values 1–8. See
`docs/re/FG_RESHADE_STREAMLINE_WRAP_OFFLINE_2026-09-29.md` for provenance
and limits. Live game guide submission, ENB behavior, and production FG-On
still require implementation and runtime verification.

## Installed 0.1.127 factory-site guard correction (2026-09-29)

The user-started 0.1.126 V5.4 run confirmed the pinned ReShade 6.8 factory
trace, returned swap and D3D11 preflight in `RazKolbas.log` at 19:41:58–59.
Later log entries show the existing owned-scene path reaching frame 40200 and
a successful observed Present. The new factory delegate line was absent.
Source inspection identified a guard error: it compared the returned swap
vtable RVA `0x3ee960` to the factory vtable RVA `0x3ee350`, so the read-only
delegate probe was skipped. This run provides **no** native delegate identity
and **no** game FG result. An external `OpenProcess` read returned Windows
error 5 (access denied); `Stop-Process` was likewise denied, so the user must
close the game before installation.

0.1.127 checks the factory object's own vtable and the exact ReShade method
and hash. A new regression fixture distinguishes factory and swap vtables.
Debug and Release builds passed **57/57** and **60/60** CTest groups. The
full nine-entry archive at
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.127-fg-factory-delegate.zip`
has SHA-256
`4b47f189943f4fdbc77e933b2345b4a8f9fb0bce0deea2d28b87eaa2eb6ac302`;
all ZIP contents match the eight-payload stage manifest. The new DLL SHA-256
is `2d314a7e403d0198beed523e0af5ccd8f52b554b5dac6fe97d110f211ec2300f`.
The complete prior isolated mod was copied to ignored
`artifacts/local/mo2-install-backup-0.1.127-2026-09-29` after Skyrim exited.
The V5.4 `mods/RazKolbas` DLL and manifest were replaced; all eight installed
payload hashes match the stage manifest. **0.1.127 has not run in Skyrim.**
The next step is a user-started game run to observe the actual delegate.

## Installed 0.1.126 guarded ReShade factory delegate probe (2026-09-29)

The installed V5.4 MO2 build adds one read-only structural observation of
the native factory pointer loaded at ReShade 6.8's verified `[this+8]`
site. It logs the downstream slot-10 method address, executable state and
module owner after the normal nested swap is created. It does not hook that
delegate, substitute a swap, load the FG SDK into Skyrim or change FG Off.
The current game process was closed before replacement. Debug and Release
built and passed **57/57** and **60/60** CTest groups respectively. The
eight-payload archive
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.126-fg-factory-delegate.zip`
has SHA-256
`c4208115f697383aadcf4ac825c3253d8897a9311c52b18f6811a2467a303dc3`;
all eight ZIP and installed MO2 payload hashes match the stage manifest.
The installed DLL SHA-256 is
`e34123286c896d0c1f47515c113d8470a32a131e0c5182ef38ccd2ff50627441`.
The previous complete isolated mod is backed up outside Git under
`artifacts/local/mo2-install-backup-0.1.126-2026-09-29`. The later
user-started game run exposed the guard error recorded above.

## Standalone D3D11 facade plus DLSS-G On (2026-09-29)

The combined `--facade-on` probe now sends real D3D11-cleared colour through
the D3D11-facing facade into the Streamline-managed D3D12 lower swap while
submitting the existing synthetic guide packet. On RTX 4080 SUPER, eight
lower Presents returned `S_OK` and each reported two actual presentations;
Off/drain and shutdown succeeded. Release built and passed **60/60** CTest
groups. The D3D12-only On probe and FG-Off facade colour/resize probe still
exit 0. See `docs/re/FG_D3D11_FACADE_ON_PROBE_2026-09-29.md` for the exact
boundary and limits. The installed 0.1.125 game build is unchanged; Skyrim
FG-On remains **NOT RUN**.

The pinned Streamline state on this adapter reported an actual non-null
input-processing fence and monotonically increasing values. A new retained
lease keeps each standalone packet alive until that fence reaches its value;
missing completion is quarantined rather than treated as success. WARP
fence/quarantine tests and the repeated `--facade-on` hardware probe passed.
Provider telemetry can vary with foreground/window state; one precondition
attempt exited without a foreground window and a later D3D12-only probe
reported one actual presentation on some frames despite requesting two.
Capstone inspection of the hash-matched V5.4 ReShade factory found the exact
underlying `CreateSwapChain` vtable dispatch under its wrapper constructor.
This narrows the startup-owner insertion point but does not verify a hook or
ENB/ReShade compatibility; see
`docs/re/FG_RESHADE680_FACTORY_CALL_2026-09-29.md`.

## Source-side FG camera-to-Streamline mapping (2026-09-29)

The 0.1.125 camera-phase result below now feeds a guarded source-only
`FgCameraData` candidate and pinned-2.14.1 `sl::Constants`,
one-extra-frame `sl::DLSSGOptions`, and retained input-tag adapters composed
into one source-stamped input packet. An SDK-call boundary checks an SL token
binding and invokes constants then tags; its call order/failure path has
mock tests. The synthetic FG-On harness now uses the whole packet and call
boundary, waits its real producer fence and checks any reported provider-input
completion fence. On RTX 4080 SUPER it reported two actual presentations per
real frame for eight consecutive frames, then drained and shut down cleanly.
Debug passed **57/57** CTest groups; Release with the pinned SDK passed
**60/60**, including
matrix/token/guide, exact format/dimension and WARP tag-lifetime tests. The
installed 0.1.125 DLL has not changed and FG stays Off.
The mapping cannot itself prove that caller-stamped camera bytes came from
the current frame. Continuous producer/phase proof, live SL tag submission,
provider fence retirement and the game-facing D3D12 presentation owner are
still open. See `docs/re/FG_STREAMLINE_CAMERA_CONSTANTS_2026-09-29.md`.

## User-started 0.1.125 camera phase result (2026-09-29)

Both 720-byte camera samples at the game-facing pre-Present observer were
byte-identical to their MenuDisplay samples on world frames 9545 and 9546.
All five camera files and six prepared-guide files passed size/SHA checks
against complete manifests. Adjacent camera history again matched exactly;
the finite depth/motion pair repeated the measured previous-UV-minus-current-
UV relationship with correlation above 0.99995 on each axis. This supports
same-frame camera freshness at the sampled observer, not at a future
Streamline lower Present or on all game routes. See
`docs/re/FG_CAMERA_PHASE_GAME_CAPTURE_2026-09-29.md`.
The game was closed and the normal INI restored; all eight installed MO2
payload hashes match the updated manifest. FG remains Off and FG-On in Skyrim
is **NOT RUN**.

## Installed 0.1.125 camera phase comparison (2026-09-29)

The next default-off diagnostic saves the same game camera buffer at
MenuDisplay and at RazKolbas's game-facing pre-Present observer on each of
two consecutive real world frames, with frame numbers and byte comparisons
in the log. The source
also extracts the observed 1.6.1170 camera basis, near/far and separate
vertical/horizontal FOV candidates under strict geometry guards. It does not
submit FG constants or enable a provider. Debug and Release each built and
passed **57/57** CTest groups; a missing-calibration API was observed failing
before implementation.

The eight-payload MO2 archive
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.125-fg-present-phase.zip`
has SHA-256 `acde9ad1c9627298f0da752b033ab523657b840c84ccba1fc5bd15f8c6767032`.
Its nine ZIP entries and eight payload hashes were verified. The isolated V5.4
MO2 mod contains DLL SHA-256
`047ad7d895943335dc0c07d22879f2e4986c37548684b7f916f91edbe7bb7e3f`
and one-shot INI SHA-256
`dcc54915e2ba392c3e38ebd3ef941a451b6f3d8457ddb6f465016e1949eacf21`;
all eight installed payloads match the manifest. The preceding DLL, normal
INI and manifest are backed up under ignored
`artifacts/local/mo2-install-backup-0.1.125-2026-09-29`.
At packaging time Skyrim 0.1.125 was **NOT RUN**. The later user-started
result is recorded above. FG remains Off.

## User-started 0.1.124 paired FG guide result (2026-09-29)

The read-only run captured three camera buffers, prepared colour/motion/depth
guides on two consecutive frames and a native UI plane on their first frame.
All 13 raw-file hashes match complete manifests. Frame 103470's previous
camera fields equal frame 103469's current fields. Position-aware camera
reprojection predicts the RG16F motion guide as previous UV minus current UV
with correlation above 0.999995 on each axis over the full second frame;
omitting camera translation makes the error 39–152 times larger. The R32F
depth follows the captured forward-depth projection. The 2560x1440 UI plane
has valid premultiplied alpha and blends exactly over the unchanged native
HUD-free scene on the sampled frame. See
`docs/re/FG_PAIRED_GUIDES_GAME_CAPTURE_2026-09-29.md` for measurements and
limits. This does **not** verify moving-object vectors, continuous pre-Present
freshness or FG-On output.

The game was closed; the normal INI was restored and all eight installed MO2
payloads match the updated manifest. FG remains Off. Source-only camera
calibration now derives world axes and projection near/far/FOV/aspect with
strict guards from the observed 1.6.1170 route. Debug and Release each pass
**57/57** CTest groups. The 0.1.124 DLL used for that run predates this
source-only calibration; 0.1.125 installed above contains it without invoking
it in the game.

## Installed 0.1.124 paired FG input diagnostic (2026-09-29)

The source now derives candidate Streamline row-major projection and temporal
clip transforms from the observed Skyrim camera fields, including translation
between the separately stored camera positions. This is source-only geometry;
no Streamline FG constants are submitted. Synthetic geometry regressions and
the full Debug and Release suites passed **57/57** CTest groups in each build.
The default-off camera probe now also saves prepared colour, motion and depth
guides on its two consecutive camera-sample frames. The one-shot installed INI
arms that probe and the native UI-plane probe together while FG remains Off.

The eight-payload MO2 archive
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.124-fg-paired.zip`
has SHA-256 `33aa2cec22d1133a0f86c2205e5f61130f44c17222db5ae851d049eb7ee503b4`.
Its nine ZIP entries and all eight payload hashes were verified. The isolated
V5.4 MO2 mod contains DLL SHA-256
`903b06f3161bc25348a2ae3c4758e9e1f57bb08affc2348afcd9fe32230bcadf`
and one-shot INI SHA-256
`f3f210cce739b382eb81acb1d3333a9fd53c7b408218a53e4224e0c9c4ff2ee2`;
all eight installed payloads match the manifest. The prior DLL, normal INI and
manifest are backed up under ignored
`artifacts/local/mo2-install-backup-0.1.124-2026-09-29`.

At packaging time Skyrim 0.1.124 and FG-On were **NOT RUN**. The later
user-started paired result is recorded above; FG-On remains **NOT RUN**.
The paired readback can briefly stall diagnostic frames.
See `docs/re/FG_CAMERA_GEOMETRY_2026-09-29.md` for the numerical derivation,
SDK contract and remaining guide/camera admission questions.

## Source-only observed game camera decoder (2026-09-29)

The 720-byte sampled layout now has a separate 1.6.1170 world-route decoder for
view/projection, current/previous view-projection and camera position. It
rejects inconsistent matrix products/inverses and nonfinite history, and
requires exact prior/current agreement when the caller asserts consecutive
real frames. Synthetic tests were written before implementation and observed
failing for missing decoding and corrupted fields. An independent review
caught an adjacent-frame mixing case; its regression failed before the
numeric thresholds were tightened. Debug and Release each passed **56/56**
CTest groups after that repair. This source-only decoder is not connected
to live FG, is not in the installed 0.1.123 DLL and does not establish
Streamline-ready constants or continuous camera freshness. The installed
MO2 mod remains at 0.1.123 with FG Off.

## User-started 0.1.123 camera buffer result (2026-09-29)

The three-frame 720-byte D3D11 constant-buffer capture completed at world
frames 34111, 34112 and 34231. Independent file hashes match the complete
manifest. Numerical products/inverses identify current view/projection and
view-projection blocks. Frame 34112's previous view-projection and position
blocks exactly match frame 34111's current blocks. This establishes the
candidate temporal offsets on the sampled world route, not continuous camera
freshness or FG submission. See
`docs/re/FG_CAMERA_BUFFER_GAME_CAPTURE_2026-09-29.md` for offsets and limits.
The game was closed, the normal INI was restored, and all eight installed MO2
payload hashes match the updated manifest. FG remains Off.

## User-started 0.1.122 UI-plane provenance result (2026-09-29)

The user started Skyrim; process 3604 captured the first eligible guarded
DLSS MenuDisplay frame 47328 at 2560x1440. The four-file
`direct-ui-plane-3604-47328-254597031` bundle is complete and every raw
file's 14,745,600-byte length and SHA-256 match its manifest. The log reports
`candidate=complete`, `route=complete`, `nativeBeforeComposite=unchanged`,
`targetSame=true`, `generationSame=true`, and typed final RTV format 28
(RGBA8 UNORM). B0 and B1 are byte-identical. The UI plane has 149,508
nonzero-alpha pixels (118,453 partial and 31,055 opaque), zero RGB under
zero alpha, and zero RGB components above alpha plus one byte. Final
composition changed 141,044 RGB pixels, all under nonzero UI alpha; alpha
remained 255. Blend residual was below 0.5 byte in every channel. The
preview shows ordinary HUD, quest and subtitle text over the scene. The
game ran for hundreds more frames without a logged route/composite failure,
then was closed. The normal INI was restored and all eight installed MO2
payloads match their updated manifest. This verifies the guard on one
ordinary world frame, not continuous FG input, menu coverage, camera data or
FG-On. See `docs/re/DIRECT_UI_PLANE_PROBE_2026-09-29.md`.

## Installed 0.1.123 read-only game camera buffer probe (2026-09-29)

Source now has default-off `Diagnostics.ProbeFgCameraBuffer`. It resolves the
statically traced exact 1.6.1170 per-frame D3D11 buffer cell, validates its
COM object and bounded constant-buffer descriptor, and takes read-only
staging copies on three real DLSS world frames: consecutive N/N+1 and N+120
or later. The raw byte bundle is explicitly a linear-byte representation;
no matrix offsets or FG constants are inferred by the capture itself. WARP
tests cover exact bytes, budget and foreign-device rejection. Debug and
Release each passed **55/55** CTest groups. The eight-payload MO2 archive
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.123-fg-camera-buffer.zip`
is SHA-256 `7fd8c3c261c4cefd3c0a263408c150712ff6c46d3a28617c3795bce7fc7f0cf7`;
all eight payload hashes and the archive's nine entries were checked. The
installed DLL SHA-256 is
`7d3615563739eff49f2835ecc35c19849a21cea2a5543639f587764afcdd120f`;
the one-shot INI SHA-256 is
`1174b6b5a0367f49f4e3b43ad0d5c1e55670acf6bd15f2723d36ec78015bd7b3`.
All eight installed payloads match the manifest. The prior DLL, normal INI
and manifest are backed up under ignored
`artifacts/local/mo2-install-backup-0.1.123-2026-09-29`. At packaging time
Skyrim runtime had **NOT RUN** for this probe; the later user-started capture
is recorded above. It does not enable FG.

## Installed 0.1.122 review safeguards and FG camera producer (2026-09-29)

The supplied branch review 31 was checked against source and its manifest;
details and limitations are in `docs/re/BRANCH_REVIEW_31_TRIAGE_2026-09-29.md`.
The numeric FG camera gate now rejects mismatched/singular projection pairs
and degenerate basis vectors. The one-shot UI-plane probe has a pre-UI scene
snapshot, a route-coverage flag, actual-size capture budgets and a typed RTV
check. It also compares native target identity and scene generation across
the UI interval. Debug and Release each built and passed **55/55** CTest groups. The
eight-file MO2 package
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.122-ui-provenance.zip`
has SHA-256 `01b98b077362bacd664ff49efe6e8e25257dfa4493025441b5f67f4fd7dcb38e`;
all nine archive entries were checked against the eight manifest hashes, with
no missing or unexpected payload. The new Release DLL SHA-256 is
`2a8d1abe854c454e5aaec008f3ba7829fd0391b534018550a2b4ef0cbae3ea5d`.
It is installed in the isolated V5.4 MO2 mod with the one-shot
`ProbeDirectUiPlane=true` INI SHA-256
`c0e8d2b571cf81d99d4f36c7a1498d5f6bc594de89046f0e02f70266581244c4`.
The other six package payloads are unchanged; all eight installed payload
hashes match. The prior DLL/INI/manifest are preserved under ignored
`artifacts/local/mo2-install-backup-0.1.122-2026-09-29`. The 0.1.121 package
and prior-version backup are preserved; it was superseded before a Skyrim run
by the native-target and generation checks.
Skyrim 0.1.122 is **NOT RUN**; the requested new evidence is a B0/B1/UI/final
bundle and logged route-completeness status. No FG input promotion or game
FG-On has occurred.

An exact 1.6.1170 static trace identifies a candidate game per-frame D3D11
buffer at RVA `0x3288788`, written by a `Map`/`Unmap` routine around RVAs
`0xe45c26`–`0xe45d0d`. Matrix offsets and temporal meaning remain unverified;
see `docs/re/FG_CAMERA_PERFRAME_STATIC_2026-09-29.md`.

## User-started 0.1.120 direct UI plane capture (2026-09-29)

Source now has a restart-scoped, default-off `Diagnostics.ProbeDirectUiPlane`
switch. On the first eligible native-boundary DLSS frame, it routes native-size
HUD output to a separate transparent RGBA8 target, composites that target over
the published scene before Present, then retires the route. Inventory, Magic,
title and loading screens are excluded from this first probe. It saves the
pre-composite scene, UI plane and final composite for alpha and image analysis.
FG remains off. Debug and Release each passed **55/55** CTest groups, including
WARP route and premultiplied-composition tests. The user-started Skyrim run
captured the first eligible native-boundary DLSS frame 26506 at 2560x1440.
The separate UI target had 149,447 pixels with nonzero alpha (118,404 partial,
31,043 opaque), no nonzero RGB under zero alpha, and no RGB component above
alpha by more than one byte. Native colour was unchanged throughout the menu
loop and EndFrame; the 140,927 pixels changed at final composition all lay
under nonzero UI alpha. The final image satisfies the premultiplied blend
equation to below 0.5 byte in every channel, with scene alpha unchanged.
All three new capture bundles passed file-size/SHA checks. This establishes a
usable separate HUD plane for the sampled frame; continuous operation,
Inventory/Magic/title/loading routing, ENB/ReShade effect placement and
FG-On are still **NOT RUN**. Details are in
`docs/re/DIRECT_UI_PLANE_PROBE_2026-09-29.md`.
The eight-payload MO2 archive at
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.120-direct-ui-plane.zip`
is SHA-256 `98ff59010adeda88f623fc1d5d344da821ad6345a5306e0a7d272b1ea034683a`;
all archived payloads match its manifest. The isolated V5.4 MO2 mod contains
DLL SHA-256 `c2b9e597417cc3fee397a0b2ffeb69b8e808f55a722bc5ad4c30f353e8938a75`
and one-shot INI SHA-256
`389d201369ee2a07669fcf7b78c9190370e2bf83ec107967037f1a68ef95dc59`.
All eight installed payloads match; the prior DLL/INI/manifest are backed up
under ignored `artifacts/local/mo2-install-backup-0.1.120-2026-09-29`.
Skyrim was closed after the capture. The normal INI was restored to SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`;
all eight installed payloads match the updated manifest. The next step is
to retain a native scene/UI pair through FG input submission without changing
the visible native composition, then validate it before FG-On.

## User-started 0.1.119 per-menu UI capture (2026-09-29)

The one-shot 0.1.119 game run completed its full-frame UI, SR, Inventory
cursor and Magic movie bundles; all manifest-listed files passed size/SHA
verification. In frame 9985, eight menu-call intervals changed the native
frame: TrueHUD, resistWidget, goldWidget, weightWidget, lvlWidget,
gametimeWidget, equipWidget_STB, and HUD Menu. The HUD Menu interval changed
78,250 RGB pixels; `EndFrame` and Present changed zero. Alpha did not change.
This identifies the writers to target for a guarded transparent UI route but
does not prove that route or FG. Exact rectangles, attribution limits, and
runtime hashes are in `docs/re/UI_PIPELINE_PER_MENU_CAPTURE_2026-09-29.md`.
Skyrim was closed after capture and the normal INI was restored; all eight
installed MO2 payloads match the updated local manifest. FG-On remains
**NOT RUN in Skyrim**.

## User-started 0.1.118 HUD write timing and 0.1.119 per-menu probe (2026-09-29)

The 0.1.118 one-shot game run completed two manifest-verified bundles at world
frames 17413 and 17444. In the 2560x1440 UI bundle, the native target gained
140,755 visible HUD/quest/subtitle/overlay pixels between the first menu call
and the pre-`EndFrame` capture, then stayed byte-identical through `EndFrame`
and Present. Every alpha byte remained 255. Thus the earlier sole-deferred-
writer hypothesis is false for this frame, while individual menu attribution
and a separate alpha UI producer remain open. The game was closed after the
capture and the original INI was restored. Exact evidence is in
`docs/re/UI_PIPELINE_GAME_CAPTURE_2026-09-29.md`.

The current 0.1.119 source adds an armed, same-frame per-menu native-target
probe with unchanged frames omitted, retaining the pre/post-`EndFrame` and
pre-Present stages. It does not change normal render routing or enable FG.
Debug and Release builds each passed **54/54** CTest groups. The eight-payload
MO2 archive `D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.119-ui-per-menu.zip`
is SHA-256 `be5a8b87646e5abf8152f49161193111f7a2d7cba45ea4d95c2a7b8904564271`;
all eight archive payload hashes matched its manifest. The isolated V5.4 MO2
mod now contains the installed DLL SHA-256
`e79da11b59e19edb02752679495bf49532ebf374a10616f349ae7e528e623b91`
and one-shot INI SHA-256
`0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
All eight installed payloads match the manifest; the previous DLL, INI, and
manifest are backed up under ignored
`artifacts/local/mo2-install-backup-0.1.119-2026-09-29`.
Skyrim 0.1.119 is **NOT RUN**; FG-On remains
**NOT RUN in Skyrim**. The current DynamicShaderFrameGen upstream HEAD was
rechecked at `879ab2c`: its menu-start callback remains diagnostic only and
its FG path consumes a completed backbuffer, so it does not supply this missing
native UI plane.

## Ghidra/Capstone UI call-chain correction (2026-09-29)

Ghidra 12.1.3 analyzed the exact AIO reference DLL and bounded decoded code
from the hash-matched Skyrim 1.6.1170 executable; Capstone 5.0.7 independently
decoded the critical sites. Skyrim calls each menu's virtual `PostDisplay`
before the common `GRenderer::EndFrame` wrapper. AIO's analyzed direct
branch binds and clears a separate UI RTV, redirects recognized late OM binds,
and conditionally copies that UI texture to its FG export before composition.
RazKolbas currently renders UI into its final native target and has no
verified separate alpha producer. These are static call/resource findings,
not proof of the active game's per-draw blend/alpha state. See
`docs/re/UI_PIPELINE_GHIDRA_CAPSTONE_2026-09-29.md`.

The 0.1.117 per-menu full-frame diagnostic was superseded **before any game
run** because pre-`PostDisplay` reads cannot attribute work submitted at
`EndFrame`. Version 0.1.118 instead attempts four full-frame stages around the
verified loop and flush, without changing the UI route or enabling FG. Debug
and Release each passed **54/54** tests. The verified eight-payload package
`RazKolbas-0.1.118-ui-endframe.zip` is installed in the V5.4 MO2 mod with a
one-shot capture armed. Installed DLL SHA-256 is
`0fbcb39165eaa588703c399f65caab866a3ce9bcb73bcb0ddf63e61ac3a8129e`.
Skyrim 0.1.118 and FG-On are **NOT RUN**. The next runtime step is a
user-started save load to locate the changed interval; only then trace its
actual D3D11 draw state and build the separate UI producer.

## Full-frame FG UI diagnostic installed (2026-09-29)

The user-started 0.1.116 run produced 25 manifest-verified raw files. Its
first native-boundary DLSS frame captured all 15 menu entries, but the 16
centre crops were identical and contained a door rather than the edge HUD
seen in a nearby full-frame capture. Inventory and Magic replays independently
showed native menu writes: a late cursor after Inventory preparation and a
large Magic movie write. Final replay alpha was fully opaque; no transparent FG UI
source has been established. No new crash log appeared. Skyrim was closed
after capture, and the prior INI was restored. See
`docs/re/FG_UI_GAME_CAPTURE_2026-09-29.md` for exact hashes and pixel counts.

Version 0.1.117 now hashes full native frames at each menu entry and retains
the baseline, changed entries and post-EndFrame image. Debug and Release
builds each passed **54/54** tests. A verified eight-file MO2 package was
installed in V5.4 with one-shot capture armed; the installed DLL hash is
`7165040ef6eb9d0ab87dd552ce6f9581abce0cb994cb1040bb350fd6c270b764`.
The 0.1.117 Skyrim run is **NOT RUN**. FG remains effectively Off in-game;
the separate UI producer and actual camera/Streamline constants are still
open. The next runtime step is a user-started save load to collect the
full-frame native UI boundary, then restore the diagnostic INI.

## AIO DLSS-G camera/token RE30 audit (2026-09-29)

The supplied RE30 archive passed an independent 121-file manifest check;
its exact AIO host/PDPerf identities and 60 original instruction-byte
anchors match the pristine local binaries. Its named `SetCameraData` path
belongs to a separate Latewarp owner, while the RTTI-identified Streamline
method's live game-camera producer remains unknown. Host source number and
SL frame counter are distinct; synthetic CPU schedules demonstrate stale
constants and log-and-continue token failures, not actual Skyrim failures.
Source-only `FgCameraData` now requires a matching real-frame stamp and
nonzero sample revision. Submission also checks the row-major temporal
matrix/inverse pair. Debug and Release each passed 54/54 CTest groups. The
installed 0.1.116 DLL and armed UI capture are
unchanged; Skyrim FG-On and game camera validation are **NOT RUN**. See
`docs/re/AIO_DLSSG_CAMERA_TOKENS_RE30_AUDIT.md`.

## V5.4 UI capture and diagnostic trigger correction (2026-09-29)

The user-started 0.1.115 run produced verified SR, empty first-frame UI,
Inventory cursor and Magic movie replay bundles. The first DLSS frame was 31
frames before native UI route activation, explaining the empty UI sequence.
Same-frame Inventory replay changed only 702 cursor pixels; Magic replay
changed 1,820,499 pixels in a large left-side region. Every replay image had
opaque final alpha, so these composites cannot serve as the FG UI plane.
FG On was requested in the menu but remained effective Off in the game.
Source 0.1.116 now arms the UI sequence on the first native-boundary DLSS
provider frame, independently of the first SR-stage capture. This corrected
capture is **NOT RUN** in Skyrim. Debug and Release each passed 54/54 tests.
The eight-file MO2 package was hash-verified and installed in the V5.4 mod
folder with a reversible four-file backup. Installed DLL SHA-256 is
`ed62abde2e4484113b51f0c45ac0310619064d5b69384d543dc66416737f8ce2`;
the one-shot INI SHA-256 is
`0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
Vendor runtimes are unchanged. See
`docs/re/FG_UI_GAME_CAPTURE_2026-09-29.md`.

## AIO FG native UI RE29 audit (2026-09-29)

The supplied RE29 archive passed an independent 144-file manifest check.
Its exact AIO host and PDPerf DLL hashes match the local pristine references,
and 97 reported instruction-byte anchors matched the original PE files.
The direct reference route uses a separate transparent UI target, while its
detection route exports composite RGB with binary alpha. PDPerf explicitly
copies UI and HUD-less inputs into owned companions, so copy order and content
freshness matter. These findings refine the input contract but do not prove
RazKolbas's live UI alpha, ENB/ReShade ordering, Inventory/Magic draws, or
camera feed. No code or installed DLL was changed; the one-shot V5.4 capture
remains armed and Skyrim capture/FG-On are **NOT RUN**. See
`docs/re/AIO_FG_NATIVE_UI_RE29_AUDIT.md`.

## One-shot V5.4 UI capture armed (2026-09-29)

With Skyrim closed, the installed V5.4 `RazKolbas.ini` now has only
`Diagnostics.CaptureFirstDlssFrame = true` changed for the next launch.
Its pre-change SHA-256 is
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`,
and the byte-for-byte backup is in ignored
`artifacts/local/fg-ui-probe-2026-09-29/RazKolbas.ini.before-capture`.
The active INI SHA-256 is
`0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
The installed plugin DLL SHA-256 remains
`bd775e7ae9f6dd76492fc5fd1e8958f26fb780fa12dc2d9d0653196796f77db4`.
The existing diagnostic collects prepared/raw/final DLSS images and a
same-frame native UI menu sequence on its first evaluated world frame.
Skyrim collection is **NOT RUN**; this setting must be restored after the
evidence is collected. This capture cannot itself prove a transparent UI
source or a complete camera record.

## Synthetic DLSS-G On and source-only submission contract (2026-09-29)

The hash-pinned Streamline 2.14.1 standalone probe now presented two frames
per one real Present for eight consecutive synthetic frames on the local RTX
4080 SUPER, with `DLSSGState::status=0` and `numFramesToGenerateMax=1`.
The earlier hidden-window run presented only one and its SDK log explicitly
said the window was unfocused; the corrected foreground window was required.
`FgPreparedSubmission` now binds copied input and UI source identities,
typed camera values, frame token and physical lower output index before any
provider call. Debug and Release passed **54/54** tests; the FG-Off facade
and swap probes still pass. See
`docs/re/FG_SYNTHETIC_ON_AND_SUBMISSION_2026-09-29.md` for exact limits.
This is synthetic FG output only. The game path remains FG Off, and the
installed V5.4 0.1.115 build is unchanged. Skyrim FG-On is **NOT RUN**.

## FG native UI plane snapshot, source only (2026-09-28)

`FgUiPlanes` now preserves a display-sized HUD-less snapshot before UI draw,
then separately snapshots an alpha-capable UI source and final colour for the
same source/generation/presentation token/reset epoch. It rejects absent,
reduced, stale or aliased UI sources and an invalid UI region without
modifying the final frame. A WARP test measured distinct pre-UI, UI-alpha and
final pixels with premultiplied UI content, including the case where the
final source is overwritten after the pre-UI copy. It also rejects a
two-bit-alpha UI format. Debug and Release passed **53/53** CTest groups. This is a
source-only capture boundary: the current Skyrim route still draws UI into
the final backbuffer and has no verified separate transparent UI producer.
Consequently no FG UI tags or FG-On game integration were enabled. The
installed V5.4 0.1.115 DLL is unchanged; Skyrim verification is **NOT RUN**
for this change. See `docs/re/FG_UI_PLANES_OFFLINE_2026-09-28.md` and the
read-only RE23/RE24 plus archived-capture assessment in
`docs/re/FG_UI_SOURCE_ROUTE_RE23_RE24_2026-09-28.md`.

## AIO FG deep dive RE28 audit (2026-09-28)

The supplied RE28 archive passed a read-only 84-file manifest hash check.
Its reference CPU probes distinguish cached D3D11 colour, guide lease and
rotating D3D12 destination, consistent with our corrected source routing.
They do not establish live guide freshness, typed SL camera constants or
provider-input completion. The offline lease ring now rejects a missing
presentation token and binds each copied set to source, generation, token,
reset epoch, exact D3D12 resources and producer/copy ticket. A WARP test
rejects altered lease identities while accepting the original. Debug and
Release passed 52/52 CTest groups. This is a preparation/submission integrity
check, not a bound Streamline camera/viewport/previous-input completion
record. See `docs/re/AIO_DLSS_FG_RE28_AUDIT.md`. No Skyrim runtime test was
performed for this audit.

## FG review 27 correction (2026-09-28)

The source-only FG bridge now copies one cached D3D11 logical buffer zero to
the rotating lower D3D12 backbuffer. A new WARP regression failed before the
fix and passed afterwards for two- and three-buffer flip chains. Debug and
Release passed **52/52** CTest groups. The Streamline 2.14.1 FG-Off facade
probe exited 0 after the change. Earlier entries below claiming colour
validation from *post-Present* FLIP_DISCARD readback are superseded: that
buffer content is not guaranteed. Current pixel validation reads the lower
WARP buffer **before** Present in both the bridge test and a test-only facade
lower-swap observer. The latter checks four distinct cached-buffer frames and
Present1. The facade also passed 100 resize/present cycles. A test-only
observer then read three distinct real-frame colours at physical buffers
0, 1, 0 through the Streamline 2.14.1 FG-Off proxy on RTX 4080 SUPER, before
each lower Present. The probe exited 0. This still does not verify FG-On or
generated output.
The installed V5.4 0.1.115 remains unchanged. Skyrim and FG-On/generated
output remain **NOT RUN** for this change. See
`docs/re/FG_REVIEW_27_TRIAGE.md`.

## Streamline lower-swap D3D11 facade, FG Off (2026-09-28)

The hash-pinned Streamline 2.14.1 standalone probe now binds the D3D11
facade above its proxied D3D12 lower swap. It discovered that the queue
reports the proxy D3D12 identity while the lower swap reports native; the
verified `slGetNativeInterface` proxy/native pair resolves the binding without
weakening either exact identity check. On the local RTX 4080 SUPER, the
facade returned the original D3D11 device, hid D3D12, presented once with
DLSS-G Off, and produced a fenced lower-backbuffer pixel matching the D3D11
source. Resize succeeded. Three additional fresh-process readback runs
matched and exited 0. The final Streamline state reported one actual
presented real frame with FG Off. Release **52/52** CTest groups passed. See
`docs/re/FG_STREAMLINE_FACADE_OFF_2026-09-28.md`. This is offline FG-Off
evidence; installed V5.4 0.1.115 is unchanged, and Skyrim FG On/generated
output remain **NOT RUN**.
An additional hidden-window run passed zero-size ResizeBuffers and reported
the actual 144x88 client extent.

## FG D3D11 swap COM facade (2026-09-28)

The source-only `FgD3D11SwapFacade` now exposes SwapChain1/3/4, virtual
D3D11 backbuffers and original D3D11 device identity above a D3D12 lower
swap. WARP tested pixel readback after Present/Present1, D3D12 query hiding,
held-buffer and invalid resize rejection, ResizeBuffers/1, and twelve repeated
resize/present cycles. Debug and Release each passed **52/52** CTest groups.
See `docs/re/FG_D3D11_COM_FACADE_2026-09-28.md`. Wrapper-chain integration,
private-data lifetime, nonblocking pacing, provider tags,
FG On and generated output remain open. No game installation changed; Skyrim
runtime verification for this facade is **NOT RUN**.

## FG D3D11-to-D3D12 lower-colour bridge (2026-09-28)

The source-only `FgD3D11PresentBridge` now moves a D3D11-rendered colour frame
through a same-adapter shared surface into the current D3D12 lower swap
buffer, then forwards one real Present. A hidden-window WARP test read back
the correct colours on two successive swap indices, checked D3D11 texture
device ownership, TEST Present preservation and duplicate-Present rejection.
Debug and Release **52/52** CTest groups passed. See
`docs/re/FG_D3D11_PRESENT_BRIDGE_2026-09-28.md` for exact scope. This is
offline pixel-transfer evidence; the subsequent facade, provider integration,
generated frames and Skyrim runtime verification are separate work.
The installed V5.4 0.1.115 DLL is unchanged, and no game was started.

## V5.4 FG swap facade live trace (2026-09-28)

The user-started 0.1.115 read-only preflight captured both the ReShade nested
swap and the ENB outer swap. Each supports `IDXGISwapChain1/3/4`, returns the
same D3D11 creation-device COM identity, and rejects D3D12 `GetDevice` with
`E_NOINTERFACE`. The full bounded evidence and exact owner hashes are in
`docs/re/FG_V54_SWAP_FACADE_LIVE_2026-09-28.md`. This closes the interface
discovery gate: the game-facing swap must remain D3D11-facing while a future
D3D12 lower owner is hidden beneath it. The current game path presented
successfully; **FG On / generated output: NOT RUN**. Skyrim was closed after
capture. No installed files changed.
The source-only FG lease ring now has an explicit normal stop with actual copy
completion checks and process-lifetime quarantine for unfinished or failed
work; a partial-copy failure WARP test passed. Provider/Present progress still
needs a real owner, so this does not validate generation or live teardown.

## FG review 26 corrections (2026-09-28)

The supplied FG-vs-AIO review was checked against source and public SDK/API
contracts. The standalone Streamline probe now balances native COM query
references; five fresh-process FG-Off swap/Present/resize runs exited 0. The
FG copy bridge now uses separate D3D11-producer and D3D12-consumer fences,
rejects the device-removal completion sentinel, and prevents removed-device
progress from retiring leases. A deliberately blocked WARP D3D12 queue
confirmed that two completed producer submissions do not certify an earlier
consumer ticket. DXGI TEST Present no longer cycles persistent FG mode.
Release build and **51/51** CTest groups pass. These are offline results.
The installed V5.4 0.1.115 diagnostic DLL is unchanged; Skyrim runtime result
after this review remains **NOT RUN**, and FG remains effectively Off.
Unresolved provider, prior-present input-fence, resource quarantine and
ENB/ReShade ownership gates are recorded in
`docs/re/FG_REVIEW_26_TRIAGE.md`.

## Streamline 2.14.1 lower-swap Off probe (2026-09-28)

The optional standalone `RazKolbasFgStreamlineProbe --swap` now exercises a
real NVIDIA-owned D3D12 swap proxy on the local RTX 4080 SUPER without starting
Skyrim. The import-library-created D3D12 device was already a Streamline proxy:
`slGetNativeInterface(device)=0 distinct=1`. Attempting to upgrade it again
returned result `19` (`eErrorInvalidIntegration`), so the probe detects that
case and only upgrades the native DXGI factory. The corrected run reported
`slInit=0`, `slIsFeatureSupported(DLSS-G)=0`,
`slUpgradeInterface(factory)=0`, `CreateCommandQueue=0x0`,
`CreateSwapChainForHwnd=0x0`, a distinct native interface beneath the swap,
`slDLSSGSetOptions(Off)=0`, `Present(TEST)=0x0`, `Present=0x0`,
`slDLSSGGetState=0 status=0x0 actualPresented=1 maxExtra=1`,
`ResizeBuffers=0x0`, `slShutdown=0`, and process exit 0. The single presented
frame was the real frame with FG Off; no generated frame was requested or
observed. This proves the matched public SDK can own a lower D3D12 swap and
forward an Off Present/resize locally. Skyrim/ENB/ReShade ownership and FG On
remain **NOT RUN**. The installed V5.4 0.1.115 DLL is unchanged by this
harness-only work; its creation-time COM trace is still the next live gate.

## 0.1.115 read-only FG swap-owner preflight installed (2026-09-28)

Existing V5.4 owner-started logs show a ReShade 6.8 nested
`CreateSwapChain` within ENB 0.505's D3D11 creation, followed by the ENB
outer swap returned to Skyrim. They do not establish the COM device/interface
contract a D3D12 lower owner must preserve. A bounded read-only probe now
queries the nested and outer swaps once at creation for `IDXGISwapChain1/3/4`,
D3D11/D3D12 `GetDevice`, `GetDesc`, and the identity of the creation device.
The probe retains no COM references after returning and does not route FG,
replace a swap, or change user settings. Its WARP test passed. The Release
build and **51/51** CTest groups pass; the standalone Streamline 2.14.1
capability probe again exited 0 on the local RTX 4080 SUPER.

With Skyrim closed, the 0.1.115 DLL SHA-256
`bd775e7ae9f6dd76492fc5fd1e8958f26fb780fa12dc2d9d0653196796f77db4`
was installed to the V5.4 MO2 RazKolbas mod. All eight manifest payloads
match. The prior 0.1.114 DLL and manifest are backed up under ignored
`artifacts/local/v54-0.1.115-fg-owner-preflight-backup`; the user's INI and
SR/NR runtimes were preserved. **Skyrim runtime result: NOT RUN.** The next
action requires the owner to start Skyrim once; inspect the fresh `FG
nested-swap preflight` and `FG outer-swap preflight` lines before attempting
any D3D12 facade under ENB/ReShade. FG remains effectively Off.

## FG offline input, presentation and Streamline probe (2026-09-28)

Source-only FG infrastructure now leases five same-frame, correctly sized
D3D11 inputs (display colour, render depth/motion, HUD-less colour and native
UI colour/alpha) through persistent same-adapter D3D12 shared surfaces. The
ring checks source/generation/reset stamps and actual texture descriptors,
keeps each slot unavailable until producer, copy, provider-input, Present and
allocator progress all retire, refuses generation advance while any lease is
held, and quarantines the ring after a partial-copy failure. A prepared lease
may be discarded only after its copy fence completes. The WARP integration
tests exercised all five roles, stale guides, a mismatched guide size,
delayed provider retirement and resize-generation rollover. These inputs are
converted texture roles supplied by a caller; a real HUD-less/UI split and
provider use have **NOT RUN**.

`FgLowerSwap` now retains one actual DXGI lower chain and forwards Present,
Present1, COM identity/GetBuffer/GetDevice/GetDesc and ResizeBuffers/1 with
their real HRESULTs. A native pass-through backend runs through the same
one-source Present coordinator and generates zero frames. `FgResizeTransaction`
checks full input retirement, turns FG Off and drains, calls actual DXGI
resize, then advances the resource generation only after success. A hidden
window WARP test passed 100 real resize cycles, checked held-backbuffer
`DXGI_ERROR_INVALID_CALL`, and proved pending provider retirement prevents
both Off and resize. This is an offline lower-chain component, **not** a
D3D11-facing D3D12 proxy in Skyrim.

Official Streamline SDK `v2.14.1` x64 archive SHA-256
`92c4d954631a1710da86ca3fa8d5034f2b9503838c95fc4ae977ae149319781b`
was downloaded from the NVIDIA GitHub release into ignored `artifacts/local`.
The standalone probe pins its headers, import library and six runtime DLL
hashes and never stages them. On the local NVIDIA RTX 4080 SUPER
(vendor `0x10de`, device `0x2702`), the exact 2.14.1 binaries reported
`slInit=0`, `slIsFeatureSupported(DLSS-G)=0`,
`slGetFeatureRequirements(DLSS-G)=0` (flags `0x16`),
`D3D12CreateDevice=0x0`, `slSetD3DDevice=0`, and `slShutdown=0`.
This is a real local capability/bootstrap probe, **not** proof of a
Streamline-upgraded swap chain or generated frames. The older supplied
2.13.0-beta10 binaries remain separate. Release build and **51/51** CTest
groups pass; FG in Skyrim remains **NOT RUN**. At this checkpoint the installed
V5.4 MO2 DLL remained 0.1.114; the newer preflight candidate above supersedes
that installed-state statement. The next owner-dependent step is a bounded live creation
and wrapper trace before replacing Skyrim/ENB/ReShade's D3D11-facing swap.

## FG presentation audit and SDK ABI boundary (2026-09-28)

An independent review of the source-only FG coordinator found that a failed
Off/drain had consumed its real-frame token before any lower Present, making a
safe retry impossible. The token is now checked before the transition and
consumed only immediately before the lower call. Once that call is attempted,
even an uncertain failure cannot submit the same source again. The lower
backend contract now carries Present versus Present1, sync interval, flags,
borrowed Present1 parameters and the exact HRESULT. A `DXGI_PRESENT_TEST`
probe forces active generation Off, forwards the test call, and does not consume
a source token. Invalid backend generated-frame counts are rejected. Mock
tests cover the failed-drain retry, Present1 argument/HRESULT propagation,
test Present, uncertain lower failure and invalid generated count.

The FG hotkey is now polled on non-test swap observations even when no world
diagnostics snapshot exists, so title/loading frames can receive its live
request. The key latch is scoped to the focused swap window so another swap
chain cannot reset it and toggle twice while the key is held. **Skyrim hotkey
and FG runtime result: NOT RUN.** The installed V5.4 MO2 build is still
0.1.114; this source-only work did not replace its DLL. Release build and
**48/48** CTest groups pass.

The supplied Streamline binaries under
`C:\Users\user\Downloads\DLSS310.8.0-Streamline2.13` identify as
v2.13.0-beta10; locally inspected public `main` headers are 2.14.1. These
versions must not be mixed for a vendor backend. Matching headers/binaries,
complete frame input leases and an offline lower-Present owner are still
required before any DLSS-G or FSR backend can be installed. A D3D11-to-D3D12
copy fence alone does not prove provider or Present retirement.

## FG in-game request control and adapter policy (2026-09-28)

The End menu now has a Frame Generation tab with an enable checkbox, provider
selector, requested/effective display and an explicit unavailable reason.
`Interface.ToggleFrameGenerationKey` (default NumMultiply; F6-F9 also accepted)
toggles the same request once per keypress while Skyrim has focus, even if
the End overlay is hidden. The live request is exposed to the future FG
presentation coordinator. Enable/disable is classified Live; provider choice
remains restart-scoped because the current owner cannot safely replace a
live swap chain. The menu saves provider changes and says a restart is needed.

At the owner's updated request, NVIDIA may choose a validated DLSS-G or FSR
3.1 FG capability, while AMD and Intel may choose only a validated FSR
capability. The adapter policy alone does not claim AMD/Intel SR or NR support,
and XeSS FG remains unimplemented. The synthetic provider tests cover all
three vendor IDs, explicit DLSS rejection on AMD/Intel, unknown-adapter
rejection and an Off/On/Off sequence with the same bound provider. A new
provider-neutral presentation coordinator calls one real Present per source,
applies Off and drains before presenting, and falls back to the real frame
when backend enable fails. Its mock tests also reject duplicate Presents and
abort on a failed Off drain. Release build and **48/48** CTest groups pass.
**In-game menu/hotkey behavior and
actual FG output: NOT RUN.** No new DLL has been installed to MO2. The
effective state stays Off because a lower presentation owner and FG backend
are still missing.

## Swappable FG policy checkpoint (2026-09-28)

Source now has a vendor-neutral FG frame/guide contract, one-provider-per-session
selection (DLSS-G first, FSR 3.1 FG as validated fallback on an NVIDIA render
adapter), truthful requested/effective/Off reason, one-real-Present ledger and
independent producer/copy/provider-input/Present/allocator retirement gates.
The fixed three-slot lease pool refuses reuse before those fences retire and
refuses resize generation rollover while any lease is still held.
At this checkpoint FG enablement and provider changes required a restart;
the newer live enablement policy above supersedes that part. TDD produced
an expected compile failure for the missing contract, then an expected config
test failure before restart classification, followed by a Release build and
46/46 passing CTest groups. See `docs/design/FG_PROVIDER_ARCHITECTURE.md`.

The first GPU transport is now source-only: it validates D3D11/D3D12 adapter
LUID and queue identity, copies a single-sample D3D11 texture to a shared
surface, signals the D3D11 producer fence, waits on D3D12, and returns a
copy-completion ticket. A WARP integration test reads the copied pixels back
through D3D12 at an odd 37x19 extent, and rejects cross-bridge and mismatched
source textures. Release build and **47/47** CTest groups pass. This proves
one shared input and its producer/copy synchronization in an offline harness;
the five-input source frame, provider consumption and Present fences are not
yet connected.

**Actual FG backend creation, generated GPU output, Skyrim runtime result and
FSR-on-NVIDIA SR pairing: NOT RUN.** This work has no presentation owner or
vendor FG submission yet and has not been installed into V5.4 MO2. The next
implementation step is a complete generation-safe input lease and offline
presentation-owner harness; only after that should the owner-started game
verify a backend.

## 0.1.114 cold loading UI and first-world hitch candidate (2026-09-28)

Source 0.1.114 extends the already-working cold-title spatial pre-UI handoff
to `Loading Menu` only while world colour/depth admission has not succeeded.
It preserves the resulting native menu composite against a later reduced menu
copy; loading after DLSS admission retains its prior route. The cold title now
creates the planned NGX feature before save loading, while evaluation still
requires world-like colour/depth and the existing 120-frame post-creation
interval. This should remove that interval from the visible first world when
the title remains open long enough. First feature creation and evaluation
CPU durations are logged to narrow the reported stutter; GPU time is not
measured by these timestamps. The automatic first-DLSS-frame 24 MiB input/output
readback, final-composition readback, UI sequence capture, and first-world
pixel probes are now guarded by `Diagnostics.CaptureFirstDlssFrame`, which
defaults to false and needs a restart to change. The config test failed before
this setting existed, then passed; Release build and 45/45 CTest groups pass.
An earlier 0.1.113 candidate was installed but never game-tested and was
superseded. With Skyrim closed, 0.1.114 DLL SHA-256
`bd1ea4c1a1f5ce1db8a1c0d633a844eb57cd50f47ef3d23682fb2bedd7f56645`
was installed into V5.4 MO2. All eight manifest payload hashes match. The
user's INI and vendor runtimes were preserved; the superseded DLL, INI and
manifest are backed up under ignored
`artifacts/local/v54-0.1.114-first-frame-capture-gate-backup`. The 0.1.112
DLL/INI/manifest backup also remains under ignored
`artifacts/local/v54-0.1.113-cold-loading-prewarm-backup`. **Early loading
visual result, first-world transition and stutter timing: NOT RUN** at this
checkpoint.

## 0.1.112 loading UI and save-load transition trace (2026-09-28)

The owner ran installed 0.1.112 and loaded a save, reporting blurry loading
text/logo/UI and a visible low-resolution world followed by a stutter when
DLSS starts. Loading frames 1–3 show `Loading Menu` and `LoadWaitSpinner`
Scaleform movies with 1485x835 full-screen viewports, drawn into a 1485x835
RTV/DSV and D3D11 viewport before the 2560x1440 spatial pre-Present
publication. This confirms that early loading text is already reduced before
the final stretch; the world-HUD viewport window cannot repair it alone. In a
second user-started run, a later loading transition at frames 8517–8519 had
DLSS active, a native 2560x1440 RTV/DSV and viewport at `PostDisplay`, and
the owner reports that screen looked good. Its Scaleform movie viewports still
read 1485x835, and scissor was reduced by EndFrame, so the candidate fix must
target the cold loading path only.

The first world-like colour/depth admission was at frame 10678 (08:55:24).
The NGX feature was created on that frame, a hard-coded 120-frame startup
interval kept spatial fallback active, and first evaluation/publication was
at frame 10798 (08:55:26). The trace establishes the two-second delayed
switch and reduced interim image. It does not measure which GPU operation
caused the perceived stutter. No loading UI or stutter repair is claimed yet.

## 0.1.112 loading-screen UI boundary diagnostic (2026-09-28)

The owner reports that the 0.1.111 HUD bars and compass are sharper with no
new visual issue, but loading-screen text/logo/UI remains blurry. The loading
screen is excluded from the world-HUD viewport window, and the existing
cold-title native route explicitly excludes `Loading Menu`. Source 0.1.112
adds bounded read-only movie-viewport and D3D11 target snapshots before the
first loading-menu `PostDisplay`, before Scaleform `EndFrame`, and after its
flush. It captures up to three early loading frames and three after DLSS has
submitted, so one owner-started session can distinguish startup/save loading
from a later world transition. The Release build and all 45 CTest groups pass.
With Skyrim closed, the DLL SHA-256
`b9a1547667f2e727765ffeb8c2f0817b9c1a5c94906699a1c1c42b48645ed135`
was installed into V5.4 MO2. All eight installed manifest payload hashes
match. The game-updated INI and SR/NR runtimes were preserved; the prior DLL,
INI and manifest are backed up under ignored
`artifacts/local/v54-0.1.112-loading-ui-probe-backup`. **0.1.112 Skyrim loading
UI trace and visual result were pending at installation; the later actual
game trace is recorded above.** See
`docs/re/HUD_RESOLUTION_BOUNDARY_2026-09-27.md`.

## 0.1.111 HUD viewport game result (2026-09-28)

In the owner-started run, frame 154977 showed 14 HUD/widget movies changing
from 1485x835 to 2560x1440 before the Scaleform flush and all 14 restoring
afterward with zero conflicts. Periodic records through more than 11,400 HUD
frames continued to report 14 restored and zero conflicts, while DLSS
publication reached over 11,000 submitted frames with zero fallbacks in flight.
The owner confirms the bars and compass are sharper and reports no new HUD
shift or flicker. This verifies the world-HUD path in the current V5.4 setup;
it does not cover the loading screen or F1 cursor interaction.

## 0.1.111 native world-HUD movie viewport candidate (2026-09-28)

The owner-started 0.1.110 game trace found all 14 active HUD/widget movie
viewports at 1707x960 while the Scaleform D3D11 target, depth, viewport and
scissor were 2560x1440. Its live startup log requested Balanced with a manual
0.666667 scale. This isolates the remaining HUD blur boundary from
the already-fixed inventory path. Source 0.1.111 now changes only full reduced
movie viewports in DLSS world-HUD frames to native dimensions before
`PostDisplay`, then restores their recorded values after Scaleform `EndFrame`
when still owned. It leaves title, inventory, magic and partial/scissored
movies alone. The viewport policy failed its focused test before implementation;
Release build and 45/45 CTest groups pass afterward. The DLL SHA-256
`9d6106cede8cf49c42c37e7da04604c76e845f965767b0eabab4b9f92818dc43`
was installed into V5.4 MO2 after Skyrim closed; all eight manifest payloads
match. The game-updated INI was preserved and its new hash recorded. The
previous DLL, INI and manifest are backed up under ignored
`artifacts/local/v54-0.1.111-hud-viewport-backup`. That was the install
checkpoint; the subsequent game result is recorded above. See
`docs/re/HUD_RESOLUTION_BOUNDARY_2026-09-27.md`.

## 0.1.110 HUD movie viewport probe installed (2026-09-28)

The owner confirmed that 0.1.109 did not sharpen the DLSS Quality HUD bars or
compass. Its log proves that both graphics dimension pairs were native during
the deferred Scaleform flush and were restored afterward; that candidate is
reverted and its patch record retired. The 0.1.108 trace also showed a native
2560x1440 colour target, depth target, viewport and scissor at the flush, while
both graphics dimension pairs were 1707x960. Neither observation establishes
the HUD movie's own viewport or cached raster size.

Version 0.1.110 restores the 0.1.108 draw path and adds a one-time, read-only
snapshot of each active Scaleform movie's viewport on the first post-DLSS
world HUD frame. Release build and 44/44 CTest groups passed. The DLL SHA-256
`b0896479f486092d0cbfdd2a38bd818c1e5aa38eb7f61cff5739322d1ce9840b`
was installed into V5.4 MO2 `RazKolbas` after Skyrim closed. The previous
plugin, INI and manifest are backed up under ignored
`artifacts/local/v54-0.1.110-hud-movie-probe-backup`; all eight installed
manifest payload hashes match. The user's INI and vendor runtimes were
preserved. The owner-started frame-8315 trace confirmed 14 reduced movie
viewports despite the native D3D11 target. **0.1.110 was diagnostic; HUD
visual repair: NOT RUN.** See
`docs/re/HUD_RESOLUTION_BOUNDARY_2026-09-27.md`.

## 0.1.108 DLSS Quality HUD diagnostic installed (2026-09-27)

The owner reports that DLAA has no HUD blur, while DLSS Quality makes health
bars and the compass look lower-resolution. In the user-started 0.1.107
Quality run, the manifest-verified frame-14640 capture had no HUD in its
1707x960 DLSS input; the final 2560x1440 image had bars, compass, icons and
text. The currently affected frame therefore places the HUD after DLSS, but
the actual UI raster dimensions remain unmeasured. The RTX 40
`ada-fastfp16` NR runtime and DLSS SR both submitted successfully. The
read-only live process probe was denied by Windows access control.

Version 0.1.108 adds only a three-frame trace around the common Scaleform
`EndFrame`, recording colour/depth target, viewport, scissor, and both game
dimension pairs. Release build and 44/44 CTest groups passed. The DLL SHA-256
`503de1aa5a8b2d071d3be67f53c514439f3194efec4757e269c832a6693a9c22`
was installed into the existing V5.4 MO2 mod after Skyrim closed, with all
eight manifest hashes verified. The owner's game-updated Quality/NR INI was
preserved. **0.1.108 runtime trace, full UI-category check, and universal
rendering fix: NOT RUN.** See
`docs/re/HUD_RESOLUTION_BOUNDARY_2026-09-27.md`.

## 0.1.107 Skyrim RTX 40 NR + DLAA verification (2026-09-27)

The owner started Skyrim with DLAA and the explicitly selected
`ada-fastfp16` runtime on the RTX 4080 SUPER. The live log recorded the
expected exact DLL hash
`e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a`
and successful NR pre-SR submissions through 5,400 frames while DLAA
continued submitting. No NR error appeared. The owner reported that it
seemed to work. This is a DLAA result on PCI `10de:2702`, not validation for
every RTX 40 card or for DLSS SR.

After closing Skyrim, the installed V5.4 INI was changed to DLSS Quality
with NR enabled and `ada-fastfp16` still selected for the next owner-started
run. The prior INI and manifest are backed up under ignored
`artifacts/local/v54-0.1.107-ada-dlss-test-backup`; all eight installed
manifest payload hashes match. **RTX 40 candidate with DLSS SR: NOT RUN.**

## 0.1.107 Skyrim DLAA + Auto verification (2026-09-27)

The owner started Skyrim with DLAA and NR runtime profile Auto, then enabled
NR in the End menu. The live `RazKolbas.log` selected `legacy-fastfp16` on
renderer PCI `10de:2702`,
recorded successful NR pre-SR submissions through 4,800 frames, and DLAA
submissions through 6,000 frames. No NR error appeared in this session. The
only warning in the inspected session said an owned SR scene was unavailable
because SR was not requested; this is expected for DLAA. The owner confirmed
that the image and UI looked normal and that the End menu showed the legacy
runtime active. This verifies the 0.1.107 Auto/legacy DLAA route in Skyrim,
not the newly supplied RTX 40 DLL.

Skyrim was closed after the inspection. The installed V5.4 INI now requests
`ada-fastfp16` with experimental opt-in for the next owner-started run; the
prior INI and manifest are backed up under ignored
`artifacts/local/v54-0.1.107-ada-test-backup`. The updated eight-file
installed manifest has zero hash mismatches. **RTX 40 candidate in Skyrim:
NOT RUN at this earlier checkpoint; see the RTX 40 DLAA run above.**

## 0.1.107 GPU-aware NR runtime selection source candidate (2026-09-27)

The owner reports that the installed 0.1.106 UI is visually fixed. This is
the owner's game observation; the assistant did not launch Skyrim or inspect
a corresponding new log. The 0.1.106 installed files remain unchanged.

Source now selects NR artifacts from an exact-hash profile catalog using the
actual renderer adapter's NVIDIA GeForce RTX PCI ID and a same-LUID D3D12
check. AMD, Intel, software and unreviewed IDs are unsupported for NR. Auto
keeps the legacy runtime on the reviewed RTX 4080 SUPER profile; three newly
supplied DLLs are explicit experimental candidates. Runtime choice is
restart-bound, and the End menu distinguishes requested/effective profile and
load/evaluation stage. A local side-by-side MO2 package was staged in
ignored `artifacts/local` and installed into the V5.4 MO2 mod while Skyrim
was closed. The existing INI and previously working SR and legacy NR DLLs
were preserved; all eight installed manifest payload hashes match. The
previous plugin and manifest are backed up in ignored
`artifacts/local/v54-0.1.107-install-backup`. Installed plugin SHA-256:
`bccbeda2ae2fd2fff064e8b45b83bf1790f19fa535c6cd1ed05e7bc18726b05a`.
The second review's sticky-teardown and pending-profile/live-control fixes
are included. See
`docs/NR_RUNTIME_SELECTION.md` for artifact identities and hardware scope.

The Release plugin/harness build succeeded and all 44 CTest groups passed.
The existing legacy and supplied 40-series NR DLLs each completed 30 frames
in separate standalone RTX 4080 SUPER processes with nonconstant changed
output; the 40-series DLL required its own caller-identity shim. The 20/30
and 50 DLLs have only static identity/export checks: compatible hardware
tests **NOT RUN**. The new source build's Skyrim/ENB/ReShade runtime and
visual tests **NOT RUN**. No FG implementation or FG runtime test was done
in this NR change.

## DLSS Frame Generation RE25 architecture checkpoint (2026-09-27)

The owner's `AIO_DLSS_FrameGen_RE_25.zip` was audited as reference evidence.
All 121 payload hashes, the exact ZIP entry set, four original-binary
identities, and 42 bounded PE code-span hashes matched; four key instruction
anchors were independently spot-checked with Capstone. The packet's reported
CPU probe assertions were not rerun. See
`docs/re/AIO_DLSS_FG_RE25_AUDIT.md`.

The resulting architecture and implementation sequence are in
`docs/design/DLSS_FG_ARCHITECTURE.md` and
`docs/superpowers/plans/2026-09-27-dlss-fg-integration.md`. AIO's host
export prepares FG input; generated frames require a Streamline-managed
lower D3D12 Present. RazKolbas currently has FG settings and a D3D11 Present
observer, but no matching presentation owner, FG guide/UI lifetime service or
DLSS-G provider. **FG implementation: NOT STARTED. Skyrim FG runtime test:
NOT RUN.** No DLL, installed MO2 mod or INI was changed for this audit.
Next offline action: implement the frame/retirement policy and its focused
tests, then build the same-adapter presentation/interop harness before any
Skyrim FG candidate is installed.

## 0.1.106 native MagicMenu movie replay candidate (2026-09-27)

The owner ran 0.1.105 and reports MagicMenu is visible but blurry. Its bounded
same-frame capture proves the native 2560x1440 image before deferred copy has
the world and hero but no magic list, while the 1707x960 reduced scene already
has the list. After the late copy, the native image contains the enlarged
list and remains byte-identical through EndFrame and pre-Present. This is why
the earlier native-composite-only candidate made MagicMenu invisible and the
visibility restore left it blurry. See
`docs/re/MAGIC_MENU_NATIVE_REPLAY_2026-09-27.md`.

The 0.1.106 candidate keeps that native world/hero, skips the blurry late
copy only when a live MagicMenu movie exists, and queues the movie's
`Display()` on the rebound native target before EndFrame. It keeps the
inventory and title paths. A one-frame native before/after/EndFrame capture
will test whether the movie draws. Release build and all 43 CTest groups
pass; the five-file MO2 ZIP passed independent entry/hash verification.
After the owner exited Skyrim, only the DLL and manifest were installed in
the V5.4 MO2 mod; all five installed payload hashes match the manifest.
The prior DLL/manifest are saved in ignored
`artifacts/local/v54-0.1.106-install-backup`. Installed DLL SHA-256:
`b5eeac5c4d1c5b9a2cf1f61c79420a3a115f1f0754bf8defd8130ac053001ccf`.
**Skyrim runtime and visual verification of 0.1.106: NOT RUN.** The next
user-started game should check MagicMenu clarity and whether the list,
hero and cursor remain visible; the one-shot native replay capture will
then be inspected.

## 0.1.105 MagicMenu visibility restore installed (2026-09-27)

In the user-started 0.1.104 run, the owner reported the title and other
screens looked good, but MagicMenu was invisible. The log confirms MagicMenu
used the new native-composite preservation and Cursor Menu replay at frame
215772. Inventory's native-composite preservation remains user-verified; the
same assumption failed for MagicMenu. Version 0.1.105 restores MagicMenu's
prior reduced-scene publication, keeps the inventory and cold-title routes,
and adds a single sustained-MagicMenu boundary capture to locate the menu's
actual pixels before a native-resolution fix is attempted. See
`docs/re/MAGIC_MENU_VISIBILITY_2026-09-27.md`.

Release build and all 43 CTest groups pass. The five-file MO2 ZIP and its
payload hashes passed independent verification. After Skyrim exited, only the
DLL and manifest were replaced in the V5.4 mod; the prior DLL/manifest were
backed up, and INI/vendor libraries were preserved. Installed DLL SHA-256 is
`283919b4db47de5ddf1be7de4d282dfd1fb5254f138523c526fff5401feb65e0`.
In the user-started 0.1.105 run, the owner reports MagicMenu is visible but
blurry. The capture completed at frame 11251; see the 0.1.106 checkpoint
above. Title and inventory were not separately retested in this report.

## 0.1.104 cold Main Menu + MagicMenu candidate installed (2026-09-27)

The owner also wants the title screen before loading a save to be clear. In
the 0.1.102 live log, native UI routing first activated at frame 7832 after a
save loaded; the cold title used reduced 1707x960 pre-Present spatial output
to 2560x1440. Source 0.1.104 adds a Main Menu-only pre-UI spatial publication
and native UI handoff when exact renderer/resource guards pass. Loading Menu
is excluded and cold-title activation does not latch the world route. The
0.1.103 MagicMenu-specific composite preservation is included. See
`docs/re/COLD_MAIN_MENU_ROUTE_2026-09-27.md` for the evidence and limits.

The 0.1.104 Release build passed all 43 CTest groups. Its five-file MO2 ZIP
and installed payload hashes passed independent verification. Skyrim was
absent when only the DLL/manifest were replaced in the V5.4 RazKolbas mod;
the prior 0.1.102 DLL/manifest were backed up, and the user's INI and both
vendor libraries remained unchanged. Installed DLL SHA-256 is
`a83a9cff9b86f29cc3f2ce8f0cdfc866b83cfd783ad10405f6e878056652b1c0`.
In the subsequent user-started Skyrim run, the owner reported the title and
other screens looked good, but MagicMenu was invisible. This is a runtime
regression of the MagicMenu composite-preservation candidate. The report does
not separately quantify title glyph resolution or prove every other menu.

## 0.1.102 inventory result and 0.1.103 MagicMenu candidate (2026-09-27)

The owner reports inventory is good in the user-started 0.1.102 run and
MagicMenu remains blurry. A bounded capture confirms the Skyrim cursor replay
changed a small native pixel region and survived EndFrame. A read-only live
probe during MagicMenu sampled both game dimension pairs at 1707x960 while
RazKolbas output was 2560x1440; the dimension-writer CALL relayed into
UnderwaterNG.dll. These facts do not establish the effective glyph draw size
or prove which owner wrote the dimensions. See
`docs/re/LIVE_NATIVE_UI_2026-09-27.md`.

The MagicMenu-specific native-composite preservation/cursor-replay candidate
is built as 0.1.103. Release build and 43 CTest groups passed; the MO2 ZIP
and all five manifest payloads passed independent hashes. **0.1.103 is not
yet installed or runtime-tested** because 0.1.102 remains loaded in Skyrim.
The next action is to exit the game, install the verified candidate without
replacing the user's INI or runtime libraries, then let the owner test MagicMenu
clarity/cursor and check that inventory remains good.

## AIO native UI RE24 local cross-check (2026-09-27)

The owner supplied `C:/Users/user/Downloads/AIO_Native_UI_RE_24.zip` as
further reference evidence. A normalized local check passed all 67 payload
hashes, and the exact original AIO DLL passed all 17 bounded code-span hashes.
Capstone independently decoded selected transition, graphics-state and late-OM
sites. The packet's 114 synthetic original-CPU checks per compiler were
**not rerun locally**; no game/GPU behavior was verified. The evidence shows
that AIO's native callback can publish twice in a synthetic frame, its
fullscreen helper leaves stencil/scissor disabled and partially restores
state, and effective cached views matter more than wrapper texture fields.
See `docs/re/AIO_NATIVE_UI_RE24_AUDIT.md` for exact limits.

No renderer source or installed 0.1.102 payload changed. **Inventory cursor
interaction, title/HUD draw state and native-resolution pixels remain NOT
RUN** for this build. The next user-started game should settle the cursor
candidate and supply the live read-only dimension probe before any new patch.

## AIO native UI RE23 cross-check and read-only probe (2026-09-27)

The owner supplied `C:/Users/user/Downloads/AIO_Native_UI_RE_23.zip` as
additional reference evidence. Its 78 payload hashes, the original AIO DLL
identity, 19 code-span hashes and three shader blob hashes passed local
read-only checks; its strict-token scalar shader test passed 11 finite cases.
The packet's 173 synthetic original-CPU assertions per compiler were **not
rerun locally** and do not demonstrate Skyrim/GPU pixels. The packet separates
display/reduced dimension writers, HUD-less base snapshot `+0x490`, optional
difference-mask UI plane `+0x3e0`, and already-composed native output. These
roles cannot be conflated to justify RazKolbas's non-inventory late scene
overwrite. See `docs/re/AIO_NATIVE_UI_RE23_AUDIT.md`.

The exact installed 1.6.1170 Address Library maps the shared dimension-writer
candidate to game RVA `0x14b2e14` (aligned CALL), and four dimension globals
to `0x328cc44/48/4c/50`. The hash-gated read-only live-detour probe now
records both pairs and candidate call-site ownership on a user-started
process. Its two new pure tests failed before implementation and passed after.
The Release `tools/Build.ps1 -Preset win-release` run passed all 43 CTest
groups, including the new read-only probe test. This is build/synthetic
verification, not a game observation.
The installed 0.1.102 DLL/INI/runtime remains unchanged. **The probe has not
run live; cursor interaction, final dimension values, title/HUD draw state and
visual correctness are NOT RUN**. The next user-started game test should
first verify the 0.1.102 inventory cursor and item selection, then gather
the read-only dimensions while the game is open.

## AIO native UI RE22 local cross-check (2026-09-27)

The owner supplied `C:/Users/user/Downloads/AIO_Native_UI_RE_22.zip` as
additional reference evidence. Its 74 payloads passed the packet SHA-256
manifest. The exact original AIO host DLL matches the target hash; local
read-only PE mapping verified all 23 bounded code-span hashes, five menu
objects/strings and the cited UI shader. Capstone independently decoded the
key dimension wrappers and menu predicate. This corrects the inherited
Main/Map label: AIO's bootstrap case is **Main/Loading**, with menu-state
bytes `+0x11/+0x12`; Map is `+0x15`. `docs/re/MAIN_MENU_RE20.md` is corrected.

The hash-matched V5.4 Skyrim 1.6.1170 executable, installed Address Library
and preserved decoded game text map AIO's candidate mouse metadata,
screen-size argument and internal native transition to aligned CALL sites at
game RVAs `0x913efb`, `0xe4cd52` and `0x972d34`, respectively. The alternate
paired IDs do not yield aligned calls in this exact decoded snapshot. The
static ABI clues and limits are in `docs/re/AIO_NATIVE_UI_RE22_AUDIT.md`.
No game hook or renderer source changed, and the 0.1.102 MO2 package remains
installed. The packet's synthetic CPU probe is packet-reported; **live hook
ownership, consumer values, actual UI draw extents, Windows/GPU behavior and
visual correctness are NOT RUN**. The next user-started run still needs to
settle the inventory cursor candidate before any metadata patch is installed.

## Native UI resolution audit 21 local review (2026-09-27)

The supplied `C:/Users/user/Downloads/RazKolbas_Native_UI_Resolution_Audit_21.zip`
was reviewed against commit `9590420`. Its 13 manifest-listed files passed the
packet's SHA-256 verifier. Its two copied predicate bodies exactly match the
local checkout. The source confirms that cold-start menu publication is gated
by world-oriented readiness, the colour-only spatial fallback sits behind
motion/depth validation, full late-3D companion observation gates the native
UI depth/stencil binding, the reduced-pass latch is based on motion-MRT shape
rather than menu identity, and non-inventory deferred spatial publication can
still overwrite the native destination. The preserved 0.1.101 log shows
1707x960-to-2560x1440 pre-Present spatial publication before native routing
activates. These findings do **not** establish the actual title-screen text
draw target or prove the packet's proposed repair visually. Details and limits
are in `docs/re/NATIVE_UI_RESOLUTION_AUDIT21_REVIEW.md`.

No rendering source, installed MO2 payload or user INI changed in this review.
The 0.1.102 Skyrim-themed cursor replay remains **NOT RUN in Skyrim**. The next
user-started run should first verify inventory visibility, native cursor and
item highlight/click behavior; its bounded capture will settle that candidate.
The broader title/HUD repair then needs an effective draw-target/viewport,
writable-stencil and before/after-copy trace so a colour-only native UI route
can be implemented without guessing at Scaleform's draw boundary. The audit's
GCC/Clang policy checks are source-behavior witnesses; Windows/GPU raster and
game visual verification for a resolution fix are **NOT RUN**.

## 0.1.102 Skyrim cursor replay candidate (2026-09-27)

The user-started 0.1.101 run showed a Windows-style pointer over inventory,
but clicking under it did nothing and moving it over rows did not highlight
them. The log recorded the inventory-preservation path and cursor overlay
running, but no diagnostics input-capture activation in that run. This rules
out the ImGui cursor as a usable replacement for Skyrim's own pointer. The
log is preserved only under ignored
`artifacts/local/runtime-0.1.101-cursor-overlay-mismatch-20260927/RazKolbas.log`
(SHA-256 `f5c677a23ec59cb0cb6bcbd8ebd096a917e57074d95563b3f77047e301e0e9df`).
The 0.1.99 image set shows Skyrim's themed pointer in the reduced source and
after the copy, while the native-before-copy image contains the inventory
list without that pointer. Its exact active-frame position and input mapping
were not measured, so the overlay's coordinate mismatch is an inference.

Source 0.1.102 removes the 0.1.101 ImGui cursor-only path, restoring 0.1.100
input behavior. When the reduced inventory copy is skipped and native colour
has been rebound, it finds the live `Cursor Menu` on Skyrim's UI stack and
calls its existing `PostDisplay` once more before deferred Scaleform EndFrame.
CommonLib's implementation of the inherited `IMenu::PostDisplay` calls the
menu's `uiMovie->Display()`. This is a **guarded candidate**, not a proven
cursor fix. One sustained inventory frame reads native colour before the
replay, after the replay, and after EndFrame into an ignored local capture,
so a subsequent user-started run can show whether and when the cursor movie
changed pixels. No new binary patch site is added.

The Release build passed all 42 CTest groups; Skyrim runtime verification is
**NOT RUN**. The V5.4 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.102-v54-cursor-replay.zip`
(SHA-256 `12bc9dffabb94ef1455998107abb7d9cf877f057fd4dbd240794e30199dfc7d8`).
All five payload hashes and the manifest entry were independently checked
inside the ZIP. Release DLL SHA-256 is
`faec1fb7d0ccad2f81ebc02f623094806804fef95218bbabb34760e04b379b80`.
The game process remained open after a graceful close request, and Windows
denied a process stop. The user then closed Skyrim. All five installed 0.1.101
payloads matched its manifest; its DLL, INI and manifest were backed up under
ignored `artifacts/local/v54-0.1.102-install-backup`. Only the DLL and manifest
were replaced in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. All five
installed payloads match the 0.1.102 manifest. The INI and SR/NR libraries
did not change.

## 0.1.101 inventory cursor overlay (2026-09-27)

The user-started 0.1.100 run confirmed that the inventory is visible again,
but its cursor is invisible. Inventory items still react to mouse movement,
so input is working. With inventory open, the existing End diagnostics menu
shows a cursor that tracks the mouse. The 0.1.100 log recorded the native
inventory-preservation path repeatedly (first at frame 8683, count 600 at
frame 9282) and a functioning diagnostics mouse overlay. A copy of the log
is preserved only under ignored
`artifacts/local/runtime-0.1.100-inventory-cursor-missing-20260927/RazKolbas.log`
(SHA-256 `b1ba32f731945fbd20bc9f29a60562bf9e247428f44b92529592c0a70db63b90`).

Source 0.1.101 reuses that ImGui-drawn cursor on frames where the inventory
copy is skipped, even while the End panel is closed. It renders at Present
above the preserved native inventory. This cursor-only path does not capture
mouse input from Skyrim; pressing End continues to draw the normal diagnostics
panel and only one ImGui cursor. The overlay is disabled on frames outside
the targeted inventory-preservation path. Cursor appearance and alignment
in 0.1.101 are **NOT RUN in Skyrim**. The Release build passed all 42 CTest
groups.

The V5.4 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.101-v54-inventory-cursor.zip`
(SHA-256 `b4917d8d073a9dc88073e907260a574bb4cfa919d254bdfaaaab3a2e7a6438ec`).
All five payload hashes and the manifest entry were independently checked
inside the ZIP. Release DLL SHA-256 is
`ae9eb2d902d539321e78e4d2ae4070d9ffe2905e909c9b0f640c7666e027e61d`.
With Skyrim closed, all five installed 0.1.100 payloads matched its manifest.
The prior DLL, INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.101-install-backup`; only the DLL and manifest were
replaced in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. All five installed
payloads match the new manifest. The INI and SR/NR libraries did not change.

## 0.1.100 preserve the composed inventory (2026-09-27)

The user-started 0.1.99 run captured frame 61644 after 30 consecutive
`InventoryMenu` stack frames at
`C:/Users/user/Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/inventory-boundary-21680-61644-31925187`.
The **native-before-copy image visibly contains the inventory list and hero**.
The reduced source contains the hero and world but no inventory list. The
deferred reduced-scene copy removes the list from native colour; the
native-after-copy, native-after-EndFrame and native-pre-Present hashes are
identical (`e9e7f7fe01c121255c3f42aa5ed4a0281ceee472a5de1815242aa5c33a1e3512`).
Thus the missing list in this frame is caused by RazKolbas's late copy,
before Scaleform EndFrame or Present. The bind trace has 41 events and zero
drops; it shows a native 2560x1440 target before the menu calls, a reduced
1707x960 producer chain, and a native target again before EndFrame. The log
is preserved only under ignored
`artifacts/local/runtime-0.1.99-inventory-overwrite-20260927/RazKolbas.log`
(SHA-256 `f67dbfddfff0f085318c993f006ad80aef541f00c2dd2e9e326ee1cec8aaf8de`).

Source 0.1.100 skips that destructive copy only while `InventoryMenu` is on
the live UI stack. It still rebinds native colour and display-size depth for
deferred Scaleform, and leaves other menus' publication path unchanged. This
preserves the already-composed inventory pixels seen in 0.1.99; whether the
full inventory and hero look correct in the new build is **NOT RUN in Skyrim**.
A WARP test confirms that rebind without publication leaves native pixels
unchanged. The Release build passed all 42 CTest groups.

The V5.4 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.100-v54-inventory-preserve.zip`
(SHA-256 `624e13ffabb7356881da6e050b060e7075aeb3b3d43b787b56cf75697ec193d9`).
All five payload hashes and the manifest entry were independently checked
inside the ZIP. The Release DLL SHA-256 is
`66406a0df92d7f2b689984e1d7eaa9b97e69181f909a29feaf0532e770e44bd1`.
With Skyrim closed, all five installed 0.1.99 payloads matched its manifest.
The prior DLL, INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.100-install-backup`; only the DLL and manifest were
replaced in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. All five installed
payloads match the new manifest. The INI and SR/NR libraries did not change.

## 0.1.99 sustained-inventory diagnostic (2026-09-26)

The user-started 0.1.98 run still had an invisible inventory, confirmed after
the capture. Its `InventoryMenu`-gated capture at
`C:/Users/user/Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/inventory-boundary-20744-17222-27399781`
contains one frame's reduced scene and native target before/after the deferred
copy and at pre-Present. The reduced and native pre-copy images contain the
world and NPC, but no visible inventory panels. The reduced source is unchanged
at pre-Present, and the native after-copy image matches native pre-Present.
This frame may have been the menu's first transitional frame: stack presence
does not prove `InventoryMenu::PostDisplay` drew on that frame. The capture
therefore does not establish whether the persistent inventory disappeared
before, during, or after its actual draw. The user confirmed it remained
invisible in that run.

Source 0.1.99 adds a bounded, read-only bind trace starting after 30 consecutive
`InventoryMenu` stack frames. It watches up to 180 frames for a reduced menu
pass, then records OM targets, viewport and sampled target changes across the
menu call and deferred Scaleform EndFrame. It adds an optional native readback
immediately after EndFrame to the existing before-copy/after-copy/pre-Present
set. The menu preparatory call returns *before* virtual `PostDisplay`; trace
labels reflect that ordering. This remains the 0.1.96 render strategy and is
**not an inventory fix**. The trace does not hook the exact preview draw or
the AIO-equivalent `0x972D34` transition, so its conclusions must be limited
to observed binds and pixels.

The Release build passed all 42 CTest groups; Skyrim runtime verification of
0.1.99 is **NOT RUN**. The V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.99-v54-inventory-trace-r2.zip`
(SHA-256 `d3dc9acae490c394ef9cc5a2f2b2ab919acca8df356722b2117ddbf283000d47`).
All five payload hashes and the manifest entry were independently checked
inside the ZIP. Release DLL SHA-256 is
`33d3970c2ddd5d3708013e20b13c95369f9b46a21ea5fb712b7aa78847b21c6b`.
With Skyrim closed, all five installed 0.1.98 payloads matched its manifest.
The DLL, INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.99-install-backup`; only the DLL and manifest were
replaced in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. All five installed
payloads match the new manifest. The INI and SR/NR libraries were unchanged.

## 0.1.98 inventory-gated boundary diagnostic and updated FG reference (2026-09-26)

The user-started 0.1.97 run still had an invisible inventory and a blurry
hero. The log copied under ignored
`artifacts/local/runtime-0.1.97-inventory-invisible-20260926/RazKolbas.log`
has SHA-256 `fec48ce9a07eded5ae7ab79c8003da4e417ef6600de05d11b933bdb39fd35540`.
Its one-shot five-image capture at
`Skyrim Special Edition/SKSE/RazKolbasCaptures/inventory-boundary-1144-70194-22957578`
was visually a world/dialogue frame, **not inventory**. The trigger was the
first motion-MRT pattern anywhere, so the capture cannot decide where the
inventory pixels disappear. Its reduced source remained unchanged through
pre-Present, and the native target after the late copy matched its
pre-Present image in that particular frame. This does not validate inventory
composition. The user's report that the title screen looks downscaled and
the loaded image improves after a stutter is consistent with the logged
early spatial path switching to world-admitted DLSS; the timing and visual
cause of that stutter were not measured.

Source 0.1.98 changes only this diagnostic's admission: it waits for an
`InventoryMenu` object in Skyrim's live UI stack before consuming the
one-shot capture. Render routing remains the unsuccessful 0.1.96 strategy;
**inventory visibility is not fixed or claimed fixed**. The independent
static audit of the supplied RE20 archive, including the manager and AIO
transition timing, is in `docs/re/INVENTORY_HERO_RE20_AUDIT.md`. The newly
updated DynamicShaderFrameGen v1.52 source was inspected read-only at
`879ab2c`; its current code removed DLSS SR despite stale README claims, so
it supplies no inventory/SR fix. See `docs/re/DYNAMIC_SHADER_FRAMEGEN.md`.

Debug and Release `tools/Build.ps1` each compiled the diagnostic and passed
all 42 CTest groups. Release was rebuilt after the 0.1.98 version and
manifest-label edits and again passed 42/42. This is build verification,
not a visual fix. The V5.4 ZIP is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.98-v54-inventory-gated.zip`
(SHA-256 `b311219b69d9d27184087a5ccc8a2ee0847c9fa268990ed56d6b91282b64e89c`).
All five payload hashes and the manifest entry were independently checked
inside the ZIP. Release/installed DLL SHA-256 is
`f5c9cc5bb33d2af8f772baf2e5ee7a428eaa2320857f0a9781947558e31b6283`.

With no SkyrimSE process running, the 0.1.97 installation's five payloads
matched its manifest. Its DLL, INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.98-install-backup`; only the DLL and manifest were
replaced in `D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. All five
installed payloads now match the new manifest; INI, signed SR, patched NR and
license files are unchanged. Actual inventory-gated capture is **NOT RUN**
until the user opens inventory in a user-started Skyrim session. The assistant
did not start Skyrim.

## 0.1.97 one-frame menu-boundary diagnostic (2026-09-26)

The user-started 0.1.96 V5.4 run again had an invisible inventory. Its
late reduced-scene publication ran at frame 10228 and at least 600 times
thereafter without a compatibility fault; DLSS continued to submit at the
menu boundary. This refutes the hypothesis that the inventory was missing
merely because the 0.1.95 producer chain was released too early. The
complete log is preserved only under ignored
`artifacts/local/runtime-0.1.96-inventory-invisible-20260926/RazKolbas.log`
(SHA-256 `f43cc67e9bc78238ae8151df1d24a64e9464f63f32d727551ff4844e14998c58`).
Skyrim was closed by the user. All five installed 0.1.96 payloads matched
their manifest; the failed DLL, INI and manifest were backed up under
ignored `artifacts/local/v54-0.1.96-regression-rollback-backup`. The
previous visible-inventory 0.1.94 DLL and manifest were restored, and all
five installed payloads rehashed. The INI and SR/NR libraries did not change.

The user also reports that the title screen before loading a save looks
downscaled. The 0.1.96 startup log shows the owned 1707x960 scene is
published at swap creation while the display is 2560x1440; early frames
use spatial publication before the native UI route is ready. This is a
separate title-screen issue, not evidence that the inventory pass itself
uses the same target. Independent, read-only RE of the exact AIO DLL found
a Main Menu-specific viewport sizing gate and a native UI transition that
clears it; see `docs/re/MAIN_MENU_RE20.md`. Static code does not prove the
reference's live timing or pixels in this modlist.

Source 0.1.97 adds a **diagnostic only** capture of the first observed
inventory motion-MRT frame: reduced colour and native colour before the
late copy, native colour after it, then both colours at pre-Present. The
five raw images share one frame and are saved under the game's existing
`SKSE/RazKolbasCaptures` directory, with a manifest and bounded byte
budget. This tests whether the inventory appears in the reduced source,
whether the copy reaches the native buffer, and whether later composition
removes it. The 0.1.96 render behavior remains unchanged, so inventory
visibility is **not claimed fixed**. Debug and Release builds and all 42
CTest groups passed; Skyrim runtime capture is **NOT RUN**. The isolated
V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.97-v54-inventory-boundary.zip`
(SHA-256 `1caf3d003c837a7d5df2aa82d5e945d90f10cf00fc81d16f907146daf91c5b0f`).
Independent extraction verified all five manifest payloads; the DLL
SHA-256 is
`6e26a60364bf30542d525aa66eab9f8e02d73f6f48e2b5ec28105e0f0d26dc6a`.
With Skyrim closed, 0.1.94 was backed up under ignored
`artifacts/local/v54-0.1.97-install-backup`. Only the 0.1.97 DLL and
manifest were installed; all five installed payloads match the new
manifest. The INI and SR/NR libraries remain unchanged. The assistant
has not started Skyrim.

## 0.1.96 deferred inventory composition candidate (2026-09-26)

The user-started 0.1.95 V5.4 run is an **actual-game regression**: the
inventory was invisible and the hero preview blurry. The live log showed
that the native UI route stayed active, the exact motion-MRT compatibility
path did not fault, and DLSS provider submissions continued. Therefore
0.1.95's early release of the reduced inventory producer chain was not a
visual fix. A copy of the live log is kept only under ignored
`artifacts/local/runtime-0.1.95-inventory-invisible-20260926/RazKolbas.log`
(final-log SHA-256
`f67735c4b4e4fe9950bbf7c0746f9e129b1c46cc08f426ec0fb8aa6d05d626f8`).
The user closed Skyrim after the report; the assistant did not launch it.

Source 0.1.96 retains the observed reduced inventory colour/motion/depth
chain through the deferred menu boundary. Immediately before Scaleform's
deferred draw, it publishes the held reduced scene to the native target
using the existing SDR spatial path, then rebinds native colour and a
display-size depth/stencil view for Scaleform. This affects only frames
where the exact observed motion-MRT pattern set the pending-menu flag.
During those frames the final menu publication is spatial fallback, not
DLSS SR; the hero preview may still be limited by its reduced source.
Unknown target shapes retain the existing compatibility fault path.
This is a **candidate**, not a proven visual fix.

The focused WARP test verifies that the reduced chain remains bound after
the offscreen scene read, that the late spatial publication succeeds, and
that deferred Scaleform resumes on native colour/depth. Debug and Release
builds and all 42 CTest groups passed after the change. **Skyrim runtime
verification is pending.** The V5.4 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.96-v54-deferred-menu.zip`
(SHA-256 `9e31a4a6a90ebad1c4c2815a6fcc0c6922deec15013292760bef94685d14361d`).
Independent extraction matched the exact five manifest payloads; staged
DLL SHA-256 is
`48437d70405164eba11b66697d058b7079c9dc79b274666213c17b303a38ab6d`.
The current INI and pinned SR/NR libraries are preserved in the package.
With Skyrim closed, all five installed 0.1.95 payloads matched their
manifest. The failed DLL, INI, and manifest were backed up under ignored
`artifacts/local/v54-0.1.96-install-backup`. The previous visible-inventory
0.1.94 DLL and manifest were restored and all five payloads rehashed.
Then only the 0.1.96 DLL and manifest were installed. All five installed
payloads match the 0.1.96 manifest, including the unchanged INI and
SR/NR runtimes. **The installed 0.1.96 build has not yet been run in Skyrim.**

## 0.1.95 inventory producer-chain routing candidate (2026-09-26)

The user-started 0.1.94 V5.4 inventory run reported that the UI and image
became blurry and appeared to render at lower resolution. The read-only
trace at frame 15297 captured 35 events with no drops. A matching reduced
1707x960 `R16G16_FLOAT` RTV1 was bound beside the reduced main colour and
depth, then read at PS slot 2. The reduced main colour was later read at PS
slot 0 while a separate offscreen colour target was bound. That offscreen
target was later sampled at PS slot 0 after returning to the main colour.
No native-sized colour target appeared among the traced OM binds. The
native UI route had been suspended at frame 15296 after the motion-format
MRT mismatch; after another retry it faulted again at frame 15940. Later
pre-Present publications used 1707x960 input and 2560x1440 output with
spatial fallback and no further provider submissions. This explains the
observed lower-resolution UI path, though it does not measure subjective
sharpness. The log is preserved only under ignored
`artifacts/local/runtime-0.1.94-inventory-blurry-20260926/RazKolbas.log`
(SHA-256 `708d9ae76fca04ec3974ac1f67357d77a35e9bc09dbd77c56627c178d53268d9`).
The assistant closed the game after the trace; no Skyrim process remains.

Source 0.1.95 recognizes only the observed two-target scene/motion/depth
shape: reduced main colour, one-mip `R16G16_FLOAT` RTV1 with exact `0x28`
RTV/SRV flags, and the known original depth. It preserves that reduced
producer chain, including its depth SRV, until the reduced main colour is
sampled into an offscreen target. Subsequent main-scene binds can then use
the existing native UI route. If that scene read never occurs, the exact
deferred Scaleform boundary restores native colour/depth for text and logs
the incomplete chain. Other incompatible targets retain the previous
conservative fault path. This is a targeted **candidate**, not a proven
visual fix or a claim that the hero preview is native-sized.

The WARP integration case failed against 0.1.94 before the change, then
passed with reduced producer, original depth read, offscreen scene sample,
native return, and incomplete-chain Scaleform fallback assertions. Debug
and Release builds and all 42 CTest groups passed. **Skyrim runtime
verification is pending.** The isolated V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.95-v54-inventory-chain.zip`
(SHA-256 `c8d956d6ee6ba1a7ee0097a706eef66ad541323a02aafcab3788e6091d406dff`).
Independent extraction matched the exact five manifest payloads. With
Skyrim closed, immutable 0.1.94 installed payloads matched their manifest;
the 0.1.95 package also matched the preserved INI and both runtimes. The
prior DLL, INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.95-install-backup`; only the DLL and manifest were
replaced. All five installed payloads match the 0.1.95 manifest. Installed
DLL SHA-256 is
`3d94c8cfecb72186628d4a8a484675aa158552a078f30ec6c35a097006156d45`;
the preserved INI SHA-256 is
`ac4963b15c4d86258919f6f194ba4489322e4622d0fe576ff660496a1bb6a941`.
The signed SR and community NR runtimes are unchanged. The assistant did
not start Skyrim.

## Supplied inventory/hero RE audit 19 (2026-09-26)

The user supplied `PureDark_Inventory_Hero_RE_19.zip`. Its exact-DLL byte
spans match the local reference, and independent Capstone/Ghidra checks
corroborate the narrow depth-only late PS substitution and the viewport
mapper's lack of an RTV identity check. See
`docs/re/INVENTORY_HERO_RE19_AUDIT.md`. This reinforces the 0.1.94 trace
strategy but does not identify the inventory/hero target. No source or
installed payload changed; actual-game trace verification remains pending.

## 0.1.94 bounded inventory bind-trace candidate (2026-09-26)

The user-started 0.1.93 V5.4 run reported an invisible inventory and a
pixelated hero preview. At frame 8347, the unknown 1707x960 RTV1 was
single-mip `R16G16_FLOAT` with bind flags `0x28`; the plugin observed one
slot-2 PS binding of that resource. Its paired native RTV/SRV was prepared at
frame 8348 and the late route resumed at frame 8349, after which no further
compatibility fault was logged. This is an **actual-game regression**; a
successful D3D11 view substitution did not produce a visible inventory.
The game was closed, its log was preserved only under ignored
`artifacts/local/runtime-0.1.93-inventory-invisible-20260926/RazKolbas.log`
(SHA-256 `9ca7bb63bfea704cc588ac3422cfc12785d427cfebce91f9e11ba4951413fa63`),
and the installed 0.1.92 DLL was restored with the user's current INI and
both NVIDIA runtimes preserved. The rollback rehashed all five installed
payloads; DLL SHA-256 is
`79fafe3d4555d14c96aeb7b7558f0875713d17a831d3c9ac5d79af1a8eb29324`.
The failed 0.1.93 install was backed up under ignored
`artifacts/local/v54-0.1.93-regression-rollback-backup`.

Static RE of the exact-hash AIO reference changes the interpretation of this
target. Ghidra identifies its setup routine as `SetupMotionVector`; the
reference creates one display-size motion-vector companion at host `+0x2d8`
and uses that view as RTV1 during the late route. Capstone independently
decoded the companion creation and the RTV1 selection with complete bounded
instruction windows. See `docs/re/INVENTORY_MOTION_BOUNDARY_1170.md`. A
slot-2 read of the `R16G16_FLOAT` resource proves a read dependency, not
that the resource contains inventory pixels. The prior source-image inference
was wrong.

Source 0.1.94 reverts all 0.1.93 target and SRV redirection. After the first
late-route fault involving a motion-format target, it records one complete
subsequent menu-to-Present interval: render-target/depth identities, formats
and extents; viewport extents; and PS reads of targets observed in that
interval. The trace is bounded to 256 events and does not alter D3D state.
This is a diagnostic for the other inventory/hero offscreen binds and their
ordering, not a visual fix. A WARP test first failed to link against the
missing trace API, then passed after checking target, viewport and sampled
target events. **Skyrim runtime verification is pending**.

Debug and Release builds and all 42 CTest groups passed. The isolated V5.4
package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.94-v54-inventory-trace.zip`
(SHA-256 `f9ebaff8450603b2da20d77b15b8d8583ddc3aae2e3d570aa735f840aafe35cd`).
Independent extraction matched its exact five manifest payloads. With Skyrim
closed, all prior installed payloads matched the corrected rollback manifest.
The DLL, mutable user INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.94-install-backup`; only the DLL and manifest were
replaced. All five installed payloads match the 0.1.94 manifest. Installed
DLL SHA-256 is
`b87ca26f31b2887f6ef3e2daae23c4b3d4ad0847379536c920811b087320ae85`,
and preserved INI SHA-256 is
`ac4963b15c4d86258919f6f194ba4489322e4622d0fe576ff660496a1bb6a941`.
The signed SR and modified NR runtimes are unchanged. The assistant did not
start Skyrim.

## 0.1.92 inventory source read diagnostic candidate (2026-09-26)

The user-started 0.1.91 V5.4 run made the inventory disappear while other
menus remained visible. Its log shows a third reduced, single-mip R16G16_FLOAT
render target triggered the late UI route fault at frame 14925; the new
companion was prepared at frame 14926 and the route resumed at frame 14927.
This is an **actual-game regression**, regardless of the later stable DLSS/NR
submissions. The game was closed, and the installed 0.1.90 DLL and manifest
were restored with the user's INI and both NVIDIA runtimes preserved. The
0.1.91 log is preserved only under ignored
`artifacts/local/runtime-0.1.91-inventory-invisible-20260926/RazKolbas.log`
(SHA-256 `44bcac40fb171dd55b855cffa49915f97573b895c2ff3ef10ef441deaf371c79`).

The 0.1.92 source reverts the speculative third-target redirection and keeps
0.1.90's fallback behavior. A read-only probe now records the unknown
target's D3D11 bind flags and counts PS shader-resource binds of that same
resource through the fault frame. This tests whether the inventory path
samples the original reduced target after drawing to it, in which case an
RTV-only native companion would lose the inventory image. It does not
redirect that target or change scene admission. The WARP native UI test
checks that the unknown target's shader-resource bind is counted while the
fault remains observable. Debug and Release builds and all 42 CTest groups
passed. The five-payload V5.4 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.92-v54-ui-source-trace.zip`
(SHA-256 `ab606f20524241d7720df467a76691ae5264a64d9f32980a37e38b3da26dbb68`).
Independent extraction matched every manifest payload. With Skyrim closed,
the prior installation matched its manifest; its DLL, INI and manifest were
backed up under ignored `artifacts/local/v54-0.1.92-install-backup`. Only the
DLL and manifest were replaced. All five installed payloads now match the
new manifest; DLL SHA-256 is
`79fafe3d4555d14c96aeb7b7558f0875713d17a831d3c9ac5d79af1a8eb29324`.
The user INI and both NVIDIA runtimes retain their previous hashes.
**Skyrim runtime verification is pending**; the user must start the game and
open the inventory to reproduce and capture the source-read count.

## 0.1.90 bounded native UI route recovery candidate (2026-09-26)

In the user-started 0.1.89 V5.4 run, the native UI route worked at first,
then detected an incompatible two-target reduced scene/depth bind at frame
24616. It permanently selected pre-Present publication after that frame.
The user reported that UI flicker began after some time. This timing makes
the route transition a concrete suspect, but the specific changed target
and the visual cause are not yet proven. DLSS/NR submission continued and
Present reported zero failures after the transition.

The 0.1.90 candidate uses pre-Present publication after the first late-UI
bind mismatch, waits at least 120 frames and for the owned scene admission
gate, then allows one retry of the native UI route. A second mismatch leaves
the pre-Present route selected. The existing companion resource mapping is
unchanged; this candidate only recovers when the mismatch was transient.
The one-time mismatch warning now records the unknown reduced target's
identity, slot, format and mip count, plus actual and expected depth
identities, so a repeat identifies which resource changed. The WARP native
UI integration test first failed to compile against the missing retry API,
then passed with a transient recovery and a repeated-fault cutoff. Debug
and Release builds and all 42 CTest groups passed. A later user-started 0.1.90
run reproduced inventory-triggered UI flicker. Its unknown reduced auxiliary
target appeared at frame 12783, and the bounded retry hit the same
incompatibility at frame 13193. Thus 0.1.90 did not resolve inventory UI
flicker; 0.1.92 uses this build's rendering behavior only as a diagnostic
baseline for the failed 0.1.91 target redirection.

The isolated V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.90-v54-ui-recovery.zip`
(SHA-256 `615aa74e735a26e70043f2f8e734c588fd35a35d4e08ed2ea17b6a371e7048c4`).
It was independently extracted and its exact five manifest payloads passed
hash verification. With Skyrim closed, the prior immutable installed files
matched their 0.1.89 manifest. The installed DLL, mutable INI and manifest
were backed up under ignored `artifacts/local/v54-0.1.90-install-backup`.
Only the DLL and manifest were replaced, and every new installed payload
matches the 0.1.90 manifest. The installed DLL SHA-256 is
`c967accec6143a34a30d6a918117bdcd047ff0d85d0328476ecc2546ac3541b5`;
the preserved user INI SHA-256 is
`5dad2032781738e15b25be2f213a4426897bda6eba80722c3d321b0c503b2e8c`.
The signed SR and community NR runtimes remain unchanged. The assistant did
not start Skyrim. The next runtime check is a user-started loaded-world
session long enough to see whether native UI routing stays active or resumes
after a transient contract fault, while checking which HUD element flickers.

## 0.1.89 explicit pre-SR depth requirement (2026-09-26)

An arbitrary pre-SR callback no longer selects the R32 depth-conversion
preparer just because it exists. `PreSrRequirements.requireR32Depth` is an
explicit presenter contract, requested by the NR processor at both DLSS
Quality and NativeAA registration sites. `SdrPreparationPolicy` can normalize
a full-frame R24 depth source to R32 independently of crop selection. A
real cropped depth region still requires the existing shader conversion,
because D3D11 cannot partially copy the depth/stencil texture. Persistent
slots now refresh converted depth only when R32 was requested; an unrelated
callback uses ordinary source copies. The NR-configured route therefore
keeps its previous R32 input format and ordering.

The added native DLAA WARP case first failed to compile without the typed
contract, then passed after implementation: an unrelated pre-SR callback saw
R24 depth, while the existing NR-style case explicitly requested and saw
R32. The focused presenter group passed nine cases and 1979 assertions.
Debug and Release builds each passed all 42 CTest groups. The new typed
preparation route has not yet been run in Skyrim or in a standalone NVIDIA
replay; those outcomes are **NOT RUN**.

The independently extracted V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.89-v54-depth-policy.zip`
(SHA-256 `0eb0838e12a84c394defef9610b05864bdf6c8dee275e076844517fa6541f3b0`).
Its five manifest payload hashes and file set passed verification. With
Skyrim closed, the 0.1.88 immutable payloads matched their manifest; the
DLL, mutable INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.89-install-backup`. Only DLL and manifest were
replaced. Installed DLL SHA-256 is
`38bd3fd861d03fefbb63963cfa596f3e1d7705ba5969791b7a8217cd52d0b588`;
all five installed payload hashes match the new manifest. The user's saved
INI remains SHA-256
`c0e655dd5c6da827f5c197e04613d280b928b180dbe29b1fb1280d11522b8220`
with `NeuralRendering.Enabled=false` and `Style=2`; the signed SR and
community NR runtimes remain byte-identical. The assistant did not start
Skyrim. The user then launched V5.4 and enabled NR in the End menu. The
0.1.89 session (game PID 22640, start 11:06:42) logged at least 18,000 NR
pre-SR submissions, Quality publication from 1707x960 to 2560x1440 at
frame 38400, and Present HRESULT 0 with zero failed calls. The user first
reported that the image seemed good, then clarified that the UI began
flickering after some time. At frame 24616 the late native UI route logged
an incompatible two-target scene/depth bind and permanently switched to
pre-Present publication. That switch is a concrete suspect for the delayed
flicker, not yet a proven cause. The only later warnings observed were the
UI bind and route-change pair; early startup also logged an emergency spatial
publication and expected admission-gate failures. The physical MO2 INI was
saved with NR enabled; its hash changed during the live run, so the
pre-launch hash above is a baseline, not the final saved hash. This confirms
ordinary NR and SR submission stability but **does not pass visual UI
continuity**. Injected NGX-failure recovery and reference image equivalence
remain unverified.

## 0.1.88 NR evaluation recovery candidate (2026-09-26)

The 0.1.87 menu-save retry remains **NOT RUN** in Skyrim; the user elected to
continue to the next audit item. In the prior NR path, any NGX evaluation
failure permanently disabled NR. The command list was still submitted, but
the code returned before enqueuing a D3D11 wait for its output fence; the
next input copy could race that D3D12 work. Teardown could also treat an
older successful copyback fence as proof that a later failed evaluation had
retired.

The 0.1.88 failure path checks device removal, enqueues and flushes the
D3D11 wait on the failed evaluation's D3D12 output fence, leaves the
original prepared colour for DLSS SR/DLAA, and requests a temporal reset.
One or two consecutive evaluation failures retry on the next valid frame;
three disable NR until restart. A failed fence handoff or device removal
disables immediately. The pending-fence ledger now retains a newer output
fence after an older copyback retires, so teardown waits for both when
needed. Other initialization, unsupported resource, and rebuild errors are
still terminal; they were not reclassified without fault evidence.

The retry/fence policy tests first failed to compile before implementation
and then passed, including injected one-frame failure, success reset,
three-failure disable, failed handoff, and old-copyback/new-output ordering.
Debug and Release builds each passed all 42 CTest groups. The standalone
RTX 4080 SUPER bridge completed 30 normal NR frames at 16 ms pacing with
output SHA-256 `ae6231bac4ad697363fe4ccb1bfde6fe3ddb08329378b6e253af1a8c5444ae0c`,
zero ring saturations, and one creation CPU wait with no per-frame CPU wait.
This bridge did not inject a real NGX evaluation failure; the recovery policy
and queue-ordering behavior are unit-tested, while real-GPU recovery and
in-game behavior remain **NOT RUN**.

The independently extracted V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.88-v54-nr-recovery.zip`
(SHA-256 `85cd3a56be079a18ea74e6d561cbb9bbfe2a61c28865e8ed82de8e05b16804ad`).
Its five payload hashes and exact file set passed verification. With Skyrim
absent, the installed 0.1.87 immutable payloads matched their prior
manifest; DLL, INI, and manifest were backed up under ignored
`artifacts/local/v54-0.1.88-install-backup`. Only DLL and manifest were
replaced. The installed 0.1.88 DLL SHA-256 is
`e4b09cb83258c5e8582f6fd6f032f9d0a44ba3e5f1c30ecc253f0ce18b72e118`;
all five installed payload hashes match the new manifest. The INI and both
vendor runtimes are byte-identical to the previous install. The assistant
did not start Skyrim. The next required action is a user-started V5.4 run
to check startup, NR off/on, image continuity, and menu-save persistence.

The user-started V5.4 process (PID 21152) began at 10:44:40 with the
installed 0.1.88 DLL hash. By the 10:51:27 checkpoint it was responsive,
the log contained at least 2400 NR-before-SR submissions, and menu-boundary
Quality publication reached frame 27000 with 9390 provider submissions,
zero fallbacks in flight, and Present HRESULT 0/failed=0. The inspected
session had zero warnings/errors. Live NR controls and history resets were
logged, and NR was later switched off. The physical MO2 mod INI changed from
SHA-256 `01ac5740d2b2708c525ff85edb766ab1754169147e91065e406d14238589ef44`
to `c0e655dd5c6da827f5c197e04613d280b928b180dbe29b1fb1280d11522b8220`;
it now contains `NeuralRendering.Enabled=false` and `Style=2`. This verifies
that menu changes reached the physical INI in this session. Persistence
through a subsequent launch and actual recovery from an NGX evaluation
failure are **NOT RUN**. The user reported completion of the requested
menu interaction, but did not provide a separate visual-quality report.
The assistant's normal close request did not exit Skyrim, and Windows denied
a process-stop request; the game was still running at this checkpoint.

## 0.1.87 MO2 INI-save retry candidate (2026-09-26)

The user confirmed that NR responds correctly to off/on toggles in the
user-started 0.1.86 V5.4 session. Its log had 13 diagnostics save warnings
with Win32 1175, and the physical MO2 mod INI remained unchanged. The save
path writes a flushed temporary and last-good backup, then calls
`ReplaceFileW` on a virtualized destination. Inspection found its temporary
and backup files in MO2's `overwrite/SKSE/Plugins`, while the destination INI
is supplied by `mods/RazKolbas`. The inspected upstream USVFS source hooks
`MoveFileExW` but not `ReplaceFileW`; the installed MO2 version has not been
independently matched to that source revision.

For error 1175 only, when both original and temporary names still exist,
`saveIni` now retries via `MoveFileExW` with replace-existing and write-through
flags. The durable backup remains until success; other/partial replacement
failures keep their recovery behavior. A new test first failed against 0.1.86,
then passed with the retry. The locked-destination and 1176/1177 recovery
tests also passed. Debug and Release builds each passed all 41 CTest groups.
The SKSE plugin metadata and CMake version are 0.1.87.

The independently extracted V5.4 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.87-v54-ini-save-final.zip`
(SHA-256 `d1075981bafc363e465c8ec220bfdb1406c35eedc2ce2b8c3e918f966ff95668`).
Its five manifest payload hashes and exact file set passed verification.
After Skyrim exited, the 0.1.86 install matched its prior manifest; DLL,
INI and manifest were backed up under ignored
`artifacts/local/v54-0.1.87-install-backup`. Only the DLL and manifest were
replaced. The installed DLL SHA-256 is
`50b6dd317d52f52aaa3f6fde9eef657f4218daf01926b0eb64999a28a3a53ea7`;
all five installed payloads match the new manifest. The user's physical INI,
signed SR runtime and community NR runtime remain byte-identical. The
assistant did not launch Skyrim.

Actual MO2 `MoveFileExW` behavior, successful menu Save, saved INI contents,
reload persistence, and continued in-game NR/SR behavior are **NOT RUN** for
0.1.87. The next step is a user-started V5.4 session: change the NR Style
slider by one step, release it to trigger automatic save, exit normally,
and check the effective virtual INI,
physical mod INI, MO2 overwrite, and next launch. A successful save may
change the mutable INI hash from the static package manifest; later
installers must preserve that user data.

## 0.1.86 NR audit-fix candidate (2026-09-26)

The 0.1.82 external code review remains relevant to this branch. The NR
evaluation path no longer calls the blocking CPU fence wait after every
submission. Three D3D12 command contexts are reused only after their output
fence retires; a full ring fails open for that frame and requests an NR
history reset. The D3D11 input Signal/Flush, D3D12 queue Wait, D3D12 output
Signal, and D3D11 output Wait/copyback remain in same-frame order. Feature
creation, rebuild, and teardown still drain on the CPU when necessary to
prove resource retirement. A D3D11 signal after output copyback protects
shared output lifetime during rebuild or shutdown. Input formats, guide
scales, depth convention, NR pass count, runtime identity, and SR insertion
point were not changed.

The model-native `DLSSNR.UICorrection` parameter is forced off because this
host does not supply validated model UI/alpha resources. The menu control is
disabled with an explanation; the existing INI key remains accepted but has
no evaluation effect. The exact 165,840,496-byte NR runtime is now hashed in
64 KiB chunks rather than loaded into a whole-file vector. Hashing still
occurs at NR initialization, so first-use latency has not been eliminated.

Debug and Release builds each passed all 41 CTest groups. The standalone RTX 4080 SUPER
NR bridge completed 30 frames twice at 16 ms pacing with identical output
SHA-256 `ae6231bac4ad697363fe4ccb1bfde6fe3ddb08329378b6e253af1a8c5444ae0c`,
zero ring saturations, and no CPU fence wait calls after feature creation.
An unpaced 30-frame run also completed, with nine ring saturations that were
retried by the harness; its output hash differed from the paced run because
skipped NR evaluations reset temporal history. These are standalone GPU
results, not Skyrim/ENB/ReShade runtime verification. Actual-game temporal
quality, frame time, resize, save-load, and teardown behavior remain **NOT
RUN** for 0.1.86.

The MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.86-v54-nr-audit-candidate.zip`
(SHA-256 `65a0b24132911af576e2bc89287f918b2e9a8d683c5a9b462f259d8c04768e4d`).
Its six ZIP entries were expanded and all five manifest payload hashes
verified. With Skyrim stopped, the existing V5.4 mod was checked against
its prior manifest and the DLL/manifest/INI backed up under ignored
`artifacts/local/v54-audit-0.1.86-install-backup`. Only DLL and manifest
were replaced. The installed DLL SHA-256 is
`aee84a660e4ed38d4994a32211fbca51e07e6b547f7286f92a07f60fe71713bb`;
all five installed payload hashes match the new manifest, so the INI and SR/NR
runtimes remain byte-identical. The assistant did not start Skyrim. The next
required game action is a user-started V5.4 loaded-world test with NR enabled
in DLSS Quality and NativeAA.

The user-started V5.4 session at 09:55 on 2026-09-26 loaded the installed
0.1.86 DLL. At the inspected 10:02:58 checkpoint, the process was responsive;
the native UI resource route had activated at frame 11372. NR-before-SR
reached at least 8400 submissions, while menu-boundary DLSS Quality
publication reached 8340 provider submissions from a 1707x960 source to
2560x1440 display, with zero fallbacks in flight. Present observation reached
19800 calls with HRESULT 0 and zero failures. The inspected session had no
NR, DLSS, native UI contract, device-removal, or Present warnings/errors.
The user changed live NR controls, including enabled off/on, and the log
recorded history-reset updates and later continued NR submissions. The only
non-startup warnings were 13 diagnostics INI replacement failures (Win32
1175); the physical MO2 mod INI remained unchanged, so menu changes were
not persisted. The user reported that NR responds correctly to off/on toggles
in the running game; comparative frame time and motion-quality measurements
remain **NOT VERIFIED**. A NativeAA interval was not observed in this session.

## 0.1.85 V5.4 native UI boundary candidate (2026-09-26)

After the 0.1.84 reduced DLSS SR run, the user reported that UI blurs while
moving. The exact prepared input capture confirms HUD bars/text already in
the 1707x960 source. Twelve consecutive menu-to-Present traces in the
user-started run had nine events: four reduced scene render-target/viewport
pairs followed by a reduced scene singleton bind with the same depth. The
existing UI observation contract accepted only eight events and latched a
contract fault, leaving native UI companion routing unavailable. The source
candidate adds a separate, exact ENB 0.505 nine-event contract; the earlier
ENB profile retains its eight-event contract. The new pure contract test
rejects altered depth and target count, and the WARP integration test exercises
both layouts with native colour/depth translation. Targeted `[native_ui]`
tests passed 226 assertions in three cases. In the user-started 0.1.85 game
run on 2026-09-26, the user reported that the moving UI now seems to work.
This is a positive visual result for the reported blur, not a long-term
stability or frame-by-frame image-quality measurement.

The final 0.1.85 Release build and all 40 CTest groups passed. The MO2 package
is `D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.85-v54-native-ui-candidate.zip`
(SHA-256 `1d6c690608cf51e8946ee4047bdf4ecf4d7a77e67472220e8720c5af93f2ca13`).
SkyrimSE.exe had exited before installation. Only the RazKolbas DLL and
manifest were replaced in the V5.4 MO2 mod; the INI and NVIDIA SR/NR runtimes
remained byte-identical. All five installed payload hashes matched the
manifest. The installed DLL SHA-256 is
`fdea4ed2a6c8e9b9be219bf0f48f1a58258d753d1631326630b4b4f004f75760`.
The previous DLL, manifest and INI were copied to ignored
`artifacts/local/v54-ui-0.1.85-install-backup`. The assistant did not start
Skyrim. In the subsequent user-started session, the log recorded native UI
companions ready at frame 2, native UI resource routing activated at frame
9962, and a deferred Scaleform UI flush reasserting native colour and
full-size depth/stencil. NR pre-SR submission and NGX evaluation followed at
frame 10051. Menu-boundary publication continued through at least frame
15000 with 4950 provider submissions, a 1707x960 source, 2560x1440 output,
and zero fallbacks in flight. No native UI contract fault or world SR
suspension was observed in the inspected session. The separate diagnostics
INI save warning (`INI replacement failed: 1175`) persisted; it did not
prevent this UI route or DLSS submissions.

## 0.1.84 V5.4 reduced-scene route runtime observation (2026-09-26)

The user changed the V5.4 mod's INI to `Quality=Quality`. The 0.1.83 runtime
log confirms that value was read at startup, but the world inputs and display
remained 2560x1440. The missing condition was the ReShade 6.8 factory and
nested swap route; 0.1.83 contained only the earlier ReShade 6.7.3 exact
profiles. The End menu did open. In one loaded-world interval native-size
DLAA exceeded 11,000 submitted frames and pre-SR NR submissions were logged;
this does not establish DLSS SR. A later interval used native fallback while
waiting for world-like depth. The INI save path also repeatedly failed with
Win32 error 1175 under the MO2 virtual path; this did not prevent startup
reading of Quality and is a separate follow-up.

Source 0.1.84 adds versioned ReShade 6.8 factory/nested swap and ENB 0.505
context/caller profiles while retaining the earlier profiles and rejecting
unknown hashes. The local mapped-image ENB/ReShade audit passed 81 assertions;
the ReShade swap audit passed 39. The final 0.1.84 Release build and all 40
CTest groups passed. The subsequent V5.4 runtime outcome is recorded below.
See `docs/re/V54_GRAPHICS_PROFILE.md` for the binary identities and sites.

The 0.1.84 MO2 package is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.84-v54-owned-sr-candidate.zip`
(SHA-256 `963df0efb812be6f7b0689ebaca296867238555db7423440a743f0b6481cea87`).
The installed Release DLL is SHA-256
`1057da7b5f5b067ab93eb92af418d0d434a1a83cd5596080925ca2a218c5ee86`.
All five payload hashes in the staged and installed manifests were verified;
the user's INI and both pinned NVIDIA runtimes were preserved. The prior DLL,
manifest and INI were copied to ignored
`artifacts/local/v54-owned-0.1.84-install-backup`. Skyrim was not started by
the assistant. The exact next action is a user-started V5.4 Skyrim launch and
loaded-world observation of early scene size, ENB UI routing, NGX submission,
visual quality and crash status.

The user started SkyrimSE.exe PID 30080 at 08:41:11 and loaded a world. The
installed DLL hash matched the tested Release build. At 08:42:09 the exact
ReShade 6.8 factory and nested swap hooks installed; the log reported an
early owned scene of **1707x960** for a **2560x1440** display before ENB
GetDesc/GetBuffer. At 08:42:18 the ENB 0.505 UI context hooks installed and
early scene integration completed. Initial empty/loading frames correctly
used the spatial fallback. At frame 9421 the colour/depth admission gate
accepted a world-like source. At frame 9541 the first NR pre-SR submission
completed, NGX evaluation returned, and an exact three-stage capture was
written. By 08:46:36 the log reported 9,060 provider submissions and 9,000
NR pre-SR submissions; pre-Present mode was 2 with zero fallbacks in flight.
The process was responding, Present HRESULTs remained zero, and no new
warning/error lines appeared in the checked post-admission interval. This is
an actual-game PASS for reduced input routing and repeated DLSS SR/NR
submission in this interval, not a claim of long-term stability or final
visual quality.

The capture manifest records prepared colour at 1707x960 and raw DLSS plus
final composition at 2560x1440. Inspection shows a nonblack scene and HUD
elements already in the prepared pre-SR colour. Native-resolution HUD
separation is therefore still incomplete for at least these elements. The
same-frame native UI sequence capture reported zero entry snapshots. No
user visual-quality verdict was received for this run. The next engineering
task is to identify which HUD draw path enters the reduced scene before SR
and move or reconstruct that UI at display resolution without disturbing
ENB/ReShade placement. Preserve this runtime capture outside Git.

## 0.1.83 V5.4 End-menu compatibility candidate (2026-09-25 historical checkpoint)

The first user-started V5.4 launch loaded 0.1.82, but the log recorded
`Unverified D3D11 owner; existing hook left untouched` and `renderer observation
disabled`. The selected profile had `+RazKolbas` and the installed INI retained
`Interface.Enabled=true` and `ToggleMenuKey=End`. The V5.4 root ENB DLL is
version 0.505 with SHA-256 `35ff1543...be7bae3`, whereas 0.1.82 accepted only
the earlier exact ENB owner. The different V5.4 ReShade 6.8 DLL is SHA-256
`b2945c29...02d08da`. See `docs/re/V54_GRAPHICS_PROFILE.md`.

Source 0.1.83 adds the exact V5.4 ENB creation export and outer swap table
profile, and admits its verified Present boundary for the diagnostics menu.
It retains the old profile and rejects every other owner. The new ENB DLL
passed the opt-in offline mapped-image swap-table audit (31 assertions),
the targeted Debug tests passed 255 assertions across 15 cases, and the
Release build passed all 40 CTest groups. Actual V5.4 End-menu rendering is
**NOT RUN** until the updated DLL is installed and Skyrim is user-started.
At this checkpoint, the new ReShade owned-scene and ENB UI routes were **NOT
RUN/NOT IMPLEMENTED**. Subsequent runtime findings and the 0.1.84 candidate
are recorded above.

The 0.1.83 package was staged from Release using the latest V5.4 MO2 INI,
without altering either pinned NVIDIA runtime, and independently verified
against all five manifest payload hashes. ZIP
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.83-v54-menu-4671d05.zip`
has SHA-256 `392bbca8257fcb7b7e0dd9f3f362fa8cd2077f2fce74df435756f1de66ffb137`.
The installed DLL SHA-256 is
`950aa6e348b2b2e040c57e43426339aa23e35fbd50df81f52c1f26fe89c6e973`;
the preserved current INI SHA-256 is
`01ac5740d2b2708c525ff85edb766ab1754169147e91065e406d14238589ef44`.
The previous DLL, manifest, metadata and INI were backed up in ignored
`artifacts/local/v54-menu-0.1.83-4671d05-backup`. The staged manifest labels
this build `EXPERIMENTAL_V54_MENU_ONLY`.

MO2 had overwritten the earlier direct profile edit while its UI was open:
one intermediate disk snapshot contained `-RazKolbas` and `+DLSS5`.
After MO2 was closed, its saved state returned to `+RazKolbas` and `-DLSS5`.
MO2 was reopened and the profile remained in that state with SHA-256
`d95bf38b11bca10840639b3e154b5bd22ec46bc43f2529e34a8da1a76fbb6482`.
All installed manifest payloads verify. Skyrim was not started by the assistant;
the V5.4 0.1.83 game test was **NOT RUN at this checkpoint**. Subsequent
user-started runs confirmed ENB creation/Present hooks and End-menu visibility.

## V5.4 MO2 deployment of 0.1.82 (2026-09-25)

The verified 0.1.82 package was copied to
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.82-native-nr-depth-071a4b4.zip`
and installed as the standalone mod
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/RazKolbas`. The user-supplied path included
an extra underscore after `BETA`; the existing portable MO2 root is
`D:/TESV54BETA/BETA_TRUEAE_V54`. The package SHA-256 remains
`3a0c936a55c796b09adbcab02855ab79ce8ae640c6b675427eb7ec083108b781`.
All five manifest payloads and the installed manifest match the hashes recorded
below, including DLL SHA-256 `5acb36c8...92b95`, INI SHA-256
`44953a84...72d18`, DLSS SHA-256 `c85f971c...6b0b7e`, and DLSS-NR SHA-256
`91ea4143...9e40be7`. The mod contains no PDB, nested archive, reference host,
IDA database, capture or game data.

Portable MO2 selected profile `V5.4 NO-LORE`. Its `modlist.txt` was backed up
under ignored
`artifacts/local/v54-install-backup-0.1.82-071a4b4-20260925`, then updated with
exactly one `+RazKolbas` entry while preserving UTF-8 without BOM and CRLF.
The enabled profile file remained stable while MO2 was running and has SHA-256
`78f1c54c7bc6d815ae1ac5279125a1a249f1dbd98a487c567ae1bfccb485f287`.
No loose RazKolbas DLL/INI exists in `Stock Game`, and no RazKolbas DLL exists
in MO2 `overwrite`. The profile's other DLSS/upscaler alternatives remain
disabled. Skyrim and `skse64_loader` were not running during deployment, so
actual game loading and rendering in this new V5.4 modlist are **NOT RUN**.

## 0.1.82 native NR depth-contract correction candidate (2026-09-25)

The installed 0.1.81 NativeAA run proved that the new stage ordering was active,
but NR did not evaluate. After the populated-scene gate admitted the frame, the
log reported `Neural Rendering disabled after frame 1: NR pre-SR resource
contract differs`; DLAA then continued successfully for more than 73,000
submissions. Source inspection isolated the mismatch: the native preparation
path retained Skyrim's `R24G8_TYPELESS` depth (DXGI format 44), while the NR
bridge intentionally accepts normalized `R32_FLOAT` depth (format 41). This was
not a provider-init, unsigned-runtime or DLAA-routing failure.

Source 0.1.82 routes any presenter with a pre-SR processor through the existing
R24-to-R32 depth shader, including full-size NativeAA. Reused frame slots now
refresh that converted depth instead of attempting an invalid `CopyResource`
between R24 and R32. The unchanged path without a pre-SR processor retains its
prior direct copies. Contract failures now log all actual formats and extents.

The NativeAA regression first failed with `44 == 41`, then passed after the
correction. Fresh Debug tests pass 157 cases / 4,640 assertions, and the Release
build passes all 40 CTest groups. Debug and Release 30-frame RTX 4080 SUPER NR
bridges still pass the live Style/Intensity change at frame 10 with input
SHA-256 `a15273e9bf608a48641cd44278e1a137a210ca11bb56200f3bdba7040f8df625`
and output SHA-256
`ae6231bac4ad697363fe4ccb1bfde6fe3ddb08329378b6e253af1a8c5444ae0c`.
Source commit `071a4b4` is pushed. The independently extracted and hash-verified
MO2 package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.82-native-nr-depth-071a4b4.zip`,
SHA-256 `3a0c936a55c796b09adbcab02855ab79ce8ae640c6b675427eb7ec083108b781`.
It contains exactly the manifest and its five
payloads, with no PDB, archive, reference host, IDA database, capture or game
data. The package preserves the effective MO2 INI from the virtual Data view,
SHA-256 `44953a84bd15183632d8336ea19a8d460825aae3b956e654f20ce2f2aa272d18`,
including NativeAA, enabled NR and the user's current Intensity `0.69`. Release
DLL SHA-256 is
`5acb36c82ebd290baffbdc0df81fe9b2cf50a15b6d0108a9bb83aaa335992b95`.
After Skyrim exited, the complete prior mod and effective virtual-Data INI were
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.82-071a4b4-20260925`. The old manifest's
DLL, runtimes and license matched; its physical mod INI was absent because the
menu had persisted the current file through MO2's virtual Data path. The
effective file was present and was used as the package source. The verified
0.1.82 payload is now installed at `D:/TESV_EX/MO2/mods/RazKolbas`: all five
manifest entries match, effective and packaged INIs both match SHA-256
`44953a...72d18`, and `meta.ini` is unchanged. Installed manifest SHA-256 is
`d319251d0f57a2d1c08a90f35ccc501fa42261b61562932a69be804ac764f86a`.
Skyrim remained absent throughout installation and verification. Actual 0.1.82
NativeAA NR then passed in the user-started session beginning at 20:49:52. The
loaded DLL matched SHA-256 `5acb36...92b95`. NR completed submissions 1, 2 and
3 at 20:52:16 and continued through at least submission 13,800 at 20:58:15.
DLAA continued through at least 14,400 submissions with `skipped=0`; Present
reached 21,000 with zero reported failures. Numerous live Style, Intensity,
Local Tone, Local Structure, Auto Mask and native UI-correction changes were
accepted with history resets while NR continued. The session contains no NR
disable, resource-contract, device-removal, DLAA-disable or Present-failure
marker. Its only warning is the expected reduced-owned-scene refusal because
the selected quality is NativeAA rather than reduced DLSS SR. The user reports
that it appears to be working. The latest persisted values observed during this
check were Style 0, Intensity 1.01, Local Tone 0.99, Local Structure 1.0,
Auto Mask off, native UI correction off and sharpening 0.49.

This is an actual-game functional PASS for sustained NR-before-DLAA and live
control updates. It is not an objective image-quality comparison, GPU-time
measurement or DLSS Quality NR result; those remain separate checks.

## 0.1.81 DLAA NR, live controls and temporal handoff candidate (2026-09-25)

The first installed 0.1.80 Skyrim runs separated three real issues. In DLSS
Quality mode, the log recorded NR submissions and the user reported that the
effect appeared active, but with heavy ghosting. In NativeAA mode the user
reported no NR effect; the same session submitted more than 18,600 DLAA frames
without any NR submission. Code inspection confirmed that the NR preprocessor
was attached only to the reduced DLSS SR presenter. The End-menu NR controls
saved the INI but did not publish any live update to the active stage.

Source 0.1.81 attaches the same fail-open NR preprocessor to both the reduced
DLSS SR presenter and the native DLAA presenter. A WARP regression proves the
native path calls NR before DLAA and still evaluates DLAA when NR returns
false. The evaluation controls Enabled, Style, Intensity, Local Tone, Local
Structure, Auto Skin/Skin Structure, Auto Mask and UI Correction now publish a
thread-safe runtime snapshot. Each changed snapshot forces one NR history
reset. Network preset remains feature-creation scoped and is labelled as
restart-required; unsupported multi-pass, HDR-input, reduced-NR-input and
custom-resolve controls are disabled in the menu rather than appearing live.

Static RE of the followed reference interop path exposed a concrete temporal
ordering difference: D3D11 Signal is followed by Flush before the D3D12 queue
Wait. RazKolbas previously omitted that Flush. The bridge now uses the same
Signal -> Flush -> queue Wait order so the D3D12 NR evaluation cannot consume
batched color, motion or depth copies a frame late. This is a root-cause-based
ghosting correction; its visual effect still requires the next Skyrim run.
The recovered reference continues to support positive render-dimension motion
scales and non-inverted depth for this Skyrim route, so those values were not
changed speculatively.

The live-update path rejects unsupported enable combinations, cannot report a
successful update after startup configuration failed, drains pending controls
even if the owned route is suspended, and catches all update failures inside
the noexcept renderer hook. Independent subagent review found the corrected
DLAA ordering and Signal/Flush/Wait sequence sound and found no remaining
blocker in these paths.

Fresh Debug tests pass 156 cases / 4,631 assertions. Fresh Release builds pass
all 40 CTest groups. The Debug and Release 30-frame RTX 4080 SUPER bridges both
pass while applying Style and Intensity at frame 10; input SHA-256 is
`a15273e9bf608a48641cd44278e1a137a210ca11bb56200f3bdba7040f8df625`
and final output SHA-256 is
`ae6231bac4ad697363fe4ccb1bfde6fe3ddb08329378b6e253af1a8c5444ae0c`.
The 0.1.81 game runtime is **NOT RUN**. Source commit `d8274db` is pushed. The
independently extracted and hash-verified MO2 package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.81-dlaa-nr-runtime-d8274db.zip`,
SHA-256
`82f34215f52dffe4d050dd36160f917a5f5df0ef662b79b6f03c8f4d229b3953`.
It contains exactly the five manifest-listed payloads plus the manifest; no
PDB, reference host, signed original NR DLL, capture or archive is tracked in
git.

Before installation, every old 0.1.80 payload except the user-edited INI
matched its manifest. The complete mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.81-d8274db-20260925`. The current INI
was intentionally preserved in the package and installation; its SHA-256 is
`29663ea89c392cbb91d03f5512daa13e9db15bd6a2b265db0813fb48b8de117e`.
It starts 0.1.81 in NativeAA with NR enabled, Auto network preset, Style 0,
Intensity 2, Local Tone 0.17, Local Structure 0.34, Auto skin and one
full-resolution SDR pass. All installed manifest payloads now match. Installed
DLL SHA-256 is
`7b19a916b0704da46aed70c584add5922d3420bb32024edeb889c40d0777baaf`;
manifest SHA-256 is
`59ce7f4a6cc3fe7082283716ac75a72f2d33480d1a02ed55d793ba19f898c7f4`.
The signed SR runtime remains `c85f...b0b7e` and the fast-FP16 NR runtime
remains `91ea...0be7`. Skyrim was absent for installation; the assistant did
not start or close it.

## 0.1.80 fast-FP16 NR-before-SR candidate (2026-09-25)

The supplied openNR report, the signed NVIDIA 310.8.0 original and the new
fast-FP16 runtime were checked against each other. The signed original is
SHA-256 `e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e`
and is the exact input required by openNR commit
`62587ae0f581be8e8bc3bb01619f6e3b8efd1983`. Pinned openNR produces
`e67dee...` for its current compatibility profile. The selected fast-FP16 DLL
is instead the separately supplied SHA-256
`91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7`.
It has the same size, version, imports and exports as the earlier community
runtime, while its changed bytes are confined to embedded device-code data.
Its retained NVIDIA certificate reports `HashMismatch`, so the package gate
uses exact size, version and SHA-256. The signed original is not installed.

The five-case Release probe passed on the RTX 4080 SUPER: expected unshimmed
failure, injected post-init exception, missing-release detection, FP16
evaluation and RGBA8 evaluation. Both positive formats produced nontrivial
pixels and completed feature release, parameter destruction, shutdown and
caller-shim restoration. A production-shaped D3D11-to-D3D12 bridge then
processed 30 consecutive RGBA8 frames through feature `0x12` and copied the
result back to D3D11. Input SHA-256 was
`a15273e9bf608a48641cd44278e1a137a210ca11bb56200f3bdba7040f8df625`;
output SHA-256 was
`ae64138f7bc63cf7c75afbfab92e5937fd82bedcdfcf23646ece3b59c518a8d7`.
The simpler D3D11-direct route recommended by the report was tested first but
`NVSDK_NGX_D3D11_Init_Ext` returned `0xBAD00001` before feature creation.

Source 0.1.80 now owns an exact-build D3D12 NR stage immediately before the
existing DLSS SR submission. It transfers the prepared D3D11 colour, motion
and depth through same-adapter shared resources and a shared fence, evaluates
NR, copies the result back to the SR colour input, and restores resource states
before retirement. Any initialization, evaluation or retirement error disables
NR and sends the unchanged colour to SR. Configuration and the End menu expose
the recovered AIO controls, while the first live candidate deliberately
accepts only one full-resolution SDR RGBA8 pass. NR requires the pinned signed
SR runtime in the same package because it currently runs within the reduced
DLSS SR route.

Fresh Debug and Release builds each pass all 40 CTest groups. The Release
five-case GPU probe and 30-frame live bridge pass. Actual Skyrim startup,
save-load stability, image quality, temporal guide semantics and performance
are **NOT RUN**. Motion-vector sign/scale and depth inversion are carried from
the existing SR contract but remain unverified for live NR. The current bridge
also waits synchronously for D3D12 completion each frame; it is a functional
candidate, not the final asynchronous performance design.

Source commit `7152f54` is pushed. The independently extracted and verified
MO2 package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.80-fastfp16-nr-7152f54.zip`,
SHA-256
`b5e0ea4493fd9623a9a4d140f8ab37c04f9f3f4bec2121baca3f7e45c952a6df`.
It contains exactly the five manifest-listed payloads plus the manifest: the
plugin, NR-enabled test INI, signed SR runtime, exact fast-FP16 NR runtime and
Dear ImGui notice. No PDB, reference host or signed original NR DLL is present.
With Skyrim stopped, all 0.1.79 payloads first matched their manifest and the
complete mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.80-7152f54-20260925`. The 0.1.80
payloads were then installed to `D:/TESV_EX/MO2/mods/RazKolbas` and every
manifest hash passed. Installed plugin SHA-256 is
`9a598203f2545f57649e1cbfb0a559e351eb818ceff6b4cdf7b38d74d97e97e2`;
installed NR SHA-256 is `91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7`;
installed manifest SHA-256 is
`1163698e98470eb7221c119dc2c460bff1da21f8dc60ba8fe2a743e2dbf1a684`.
The test INI keeps DLSS Quality, sharpening 1.0 and manual render scale
0.666667, and enables one-pass full-resolution SDR NR with Shipping preset.
The assistant did not start Skyrim.

## 0.1.79 writable native UI depth views (2026-09-25)

The user-started 0.1.78 run held real DLSS continuously after provider
admission. Its log reached more than 70,000 provider submissions with
`mode=2`, zero fallbacks and continuous successful deferred Scaleform DSV
rebinds. The resource-bar fills no longer alternated: health, stamina and
magicka remained black while their frames and text remained visible. This is
an actual-game **PASS** for the generation-scoped provider latch and an
actual-game **FAIL** for fill rendering.

The preserved full-frame stage capture proves that the prepared reduced input
and raw DLSS output contain only the world image. The final native composition
adds the HUD frames and text but omits the coloured fills. Every pixel in the
prepared, raw DLSS and final images has alpha 255, disproving output-alpha and
post-sharpen hypotheses. The run log is preserved below ignored
`artifacts/local/runtime-0.1.78-black-bars-20260925-113034/RazKolbas.log`,
SHA-256
`ac8d3c9ec798f76652a7bce58a689d3326389deacac75a8009656da2acbfb6cf`.
Skyrim accepted a normal close request after evidence collection.

An independent capture-history review found the decisive transition: the
0.1.72 through 0.1.74 final captures have coloured resource fills, while the
first 0.1.75 capture and every later capture have black fills. The sole
production change at that transition was binding the display-sized DSV.
Inspection then found that both owned DSV creation paths copied the observed
source view descriptor verbatim, including any `READ_ONLY_DEPTH` and
`READ_ONLY_STENCIL` flags, even though one owned view is cleared and written
every frame and the other is used to clear the separate sampled-depth texture.

Source 0.1.79 explicitly creates those owned writable/clear DSVs with
`Flags=0`. A WARP regression supplies a source D24S8 view with both read-only
flags. It first reproduced inherited flags `3`, then verifies source flags stay
observable while the display-sized writable and sampled-clear views both have
flags zero and the bound native UI DSV is writable. The next run also logs the
actual source and owned flags once. Complete Debug and Release builds and all
40 CTest groups pass. Actual-game fill restoration remains **NOT RUN**.

Source commit `c595997` is pushed. An independent subagent review found no
blocking issue in the two writable-view corrections, WARP regression or
one-time descriptor log; it separately reran all 40 Debug and Release CTest
groups successfully. The independently extracted and verified package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.79-writable-ui-depth-c595997.zip`,
SHA-256
`5055370577f5460c58367adbd644738fafa39d75a312befbf801d6b29d608bb4`.
It contains exactly the four manifest-listed payloads plus the manifest, every
payload hash matches and the NVIDIA runtime signature remains valid. With
Skyrim stopped, the verified 0.1.78 installation was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.79-c595997-20260925`. Only the plugin
DLL and manifest were replaced. Installed DLL SHA-256 is
`c1b0c979fca3ce6fd6b5ebce2f6b6c4b24aaff906eafab4d7348a70379cd5a14`;
installed manifest SHA-256 is
`e25ec6f4a18f697e613b0ad44d58b641ad0102c7f65dd99236232985b3be6afa`.
The user INI, signed NVIDIA runtime and `meta.ini` remain byte-identical.

The user started the installed 0.1.79 build, loaded the same scene and reported
that the bars now work correctly. This is an actual-game **PASS** for health,
stamina and magicka fill rendering. The automatic descriptor receipt confirms
the exact causal contract at frame 2: D24S8 format 45, observed source DSV
flags `0x3`, owned writable DSV flags `0x0`, and owned sampled-clear DSV flags
`0x0`. The route then held real DLSS `mode=2` beyond 9,300 provider
submissions with zero fallbacks while the deferred Scaleform rebind continued.
The game remained responsive and accepted a normal close request. The final
log is preserved below ignored
`artifacts/local/runtime-0.1.79-writable-ui-depth-pass-20260925/RazKolbas.log`,
SHA-256
`070111aa615b362e9651531fad0aeb447a714d73f787af1b690f0a1950501977`.

## 0.1.78 generation-scoped provider admission (2026-09-25)

The user-started 0.1.77 run still showed periodic black health, stamina and
magicka fills. This is an actual-game **FAIL** for the deferred-flush DSV
rebind hypothesis. The hook was active and successfully reasserted the native
MRT/DSV set for thousands of Scaleform flushes without a compatibility fault,
while Present continued succeeding. That rules out a missing depth attachment
at the common `GRenderer::EndFrame` wrapper.

The same run exposed a separate synchronized state transition. After native UI
routing and real DLSS were established, the 10x10 diagnostic scene/depth probe
continued to clear admission on a single low-diversity sample. This repeatedly
alternated menu-boundary frames between real DLSS (`mode=2`) and spatial
fallback (`mode=3`), requesting an NGX history reset on every fallback. The
probe is deliberately sparse and is suitable for initial/loading-screen
admission, but it is not a per-frame validity contract after successful NGX
publication.

Source 0.1.78 latches successful menu-boundary provider admission for the exact
owned-scene resource generation. Later weak diagnostic samples remain logged
but cannot demote frames in that generation. A generation change still requires
fresh colour/depth admission, and real provider failures continue through the
existing fallback and reset path. The regression covers initial rejection,
first admission, same-generation retention and generation-change invalidation.
Complete Debug and Release builds and all 40 CTest groups pass. Actual-game fill
stability remains **NOT RUN** for 0.1.78.

Source commit `7e9c6cc` is pushed. The independently extracted and verified
package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.78-provider-latch-7e9c6cc.zip`,
SHA-256
`6a676160bfa11f7cec3c77e16c1570e87ab4400b41d3ff1d3801d832e5447331`.
It contains exactly the four manifest-listed payloads plus the manifest, every
payload hash matches, and the NVIDIA runtime signature is valid. After the
evidence run closed normally, the complete verified 0.1.77 mod was backed up
under ignored
`artifacts/local/mo2-install-backup-0.1.78-7e9c6cc-20260925`. Only the plugin
DLL and manifest were replaced in the existing MO2 mod. Installed DLL SHA-256
is `d9f86fd3139a501420b94d045cc6c8d4aa47432e4e658bd320aa48363af33317`;
installed manifest SHA-256 is
`e4c651a747635528130e16554733e0a177af2878e5d78301af0b67c8767b8dd1`.
The user INI, signed NVIDIA runtime and `meta.ini` remain byte-identical.

## 0.1.77 deferred Scaleform flush rebind (2026-09-25)

The user started installed 0.1.76 and reported that the health, stamina and
magicka fills still flicker black. This is an actual-game **FAIL** for the
boundary-latch/scissor hypothesis. The run log proves the menu publication
boundary remained latched while DLSS submissions and Present continued, so
the earlier composition-order explanation is disproved.

Targeted Capstone re-analysis of the exact PureDark reference confirmed its
two full-size depth roles and the per-frame writable-depth clear with depth
one/stencil zero. RazKolbas already matches those values. The material
difference is that the reference's late OM translation covers actual late
draw submission, while RazKolbas bound its DSV before the menu loop and relied
on that state surviving until the common deferred Scaleform flush.

Source 0.1.77 adds an exact-hash, exact-byte CALL hook at Skyrim 1.6.1170 RVA
`0xfa51ea` (Address Library caller ID 82084) to the verified wrapper at RVA
`0xfc3300` (ID 82733), which invokes `GRenderer::EndFrame` virtual slot
`+0x28`. Immediately before forwarding the original wrapper once, the hook
reasserts the currently bound native MRT set with the existing display-sized
D24S8 attachment. It preserves every current render target and performs no
second clear. A machine-readable patch record and exact ABI, exception-safe
forwarding, and WARP MRT/DSV regressions cover the route. Complete Debug and
Release tests each pass all 40 CTest groups. An independent subagent review
matched the captured live bytes, Address Library IDs, proxy ABI, three-patch
rollback, COM ownership and MRT preservation and found no blocking defect.

Source commit `0f7a037` is pushed. The independently extracted and verified
package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.77-ui-flush-0f7a037.zip`, SHA-256
`0ae0a98450dab9adf99694b65412e4dd38d64ee6c726cc4fbc29b639afb7b41a`.
Every payload matches its manifest and the NVIDIA runtime signature is valid.
With Skyrim stopped, the complete 0.1.76 mod was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.77-0f7a037-20260925`, then the verified
payload was installed while preserving `meta.ini`. Installed DLL SHA-256 is
`577d46e1a95b002752f5e9a5f9d010f669664e5f637169a48afbcf49e6ec016c`;
all payloads match installed manifest SHA-256
`79879cb290e7cf11457d452314521b98d2033eb50ba73466b43beec88d19dffd`.
The subsequent user-started run reported the same fill flicker, so visual
stability **FAILED**. The log nevertheless confirms continuous successful
deferred-flush rebinds and no Present or route compatibility failure.

## 0.1.76 stable native UI publication boundary (2026-09-25)

The user started installed 0.1.75 and confirmed that the two isolated centre
symbols are gone. This is an actual-game **PASS** for the deferred Scaleform
stencil repair. The user then reported that the health, stamina and magicka
fills periodically become black while the bar frames remain visible.

The same run's log identifies a synchronized boundary oscillation. After
native UI routing activated at frame 13322, isolated 10x10 depth samples fell
just below the 16-value admission threshold at frames 13532, 13833, 14374,
15065 and later. Each single rejection immediately moved publication from the
menu boundary (`mode=2`) back to the late pre-Present fallback (`mode=3`) for
several seconds, then two good samples moved it forward again. This repeatedly
changed UI composition order and matches the reported periodic global fill
flash; Present itself continued succeeding.

Source 0.1.76 latches the verified menu publication boundary after its first
valid admission. A transiently weak source sample can still select spatial
scene fallback, but publication remains before native UI, so it cannot move
the UI between two composition orders. The 0.1.74 scissor-coordinate mutation
is also retired: its actual-game run proved hundreds of remaps did not fix the
symbols, and application-owned scissor rectangles now pass through unchanged.
The native D24S8 binding that removed the symbols remains intact. Regression
tests cover both the boundary latch and scissor pass-through. Complete Debug
and Release builds each pass all 40 CTest groups.

Source commit `76bdef4` is pushed. The independently extracted and verified
package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.76-ui-boundary-76bdef4.zip`, SHA-256
`70e3ac9c7be4853c8a116ee5c9d270667f8286b6dafccab192eb9736ed54e4d8`.
Every payload matches its manifest and the NVIDIA runtime signature is valid.
After evidence collection, Skyrim accepted a normal window-close request under
the user's standing authorization. The previous mod was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.76-76bdef4-20260925` and the verified
stage was installed while preserving `meta.ini`. Installed DLL SHA-256 is
`baf52150cb8681683982c6b0f17ad41a74841b266290c176f2ca36b66620d5ac`;
all payloads match manifest SHA-256
`b8be9d2757df520238240f611799364705c7765b725004645ae305000d5d0b58`.
The subsequent user run reported the same fill flicker. This visual check
therefore **FAILED**, although the log confirmed the boundary latch itself
worked and Present remained healthy.

## 0.1.75 deferred Scaleform stencil repair (2026-09-25)

The user-started 0.1.74 run activated the owned 1707x960-to-2560x1440 route,
began real DLSS submissions and logged native UI scissor remaps through at
least 512 calls. The user still saw both centre symbols. This is a runtime
**FAIL** for the scissor-only hypothesis.

Ghidra 12.1.3 and Capstone 5.0.6 then located the common deferred Scaleform
flush. After the 17-entry `IMenu::PostDisplay` loop, Skyrim RVA `0xfa51ea`
calls Address Library ID 82733 at RVA `0xfc3300`; that wrapper invokes virtual
slot `+0x28`, `GRenderer::EndFrame`. Exact-hash TrueHUD analysis found its
actor-info-bar projection and display-info update at RVA `0x4fe80`. The active
Oathvein ActionScript masks the bar geometry while its text is unmasked. Full
addresses, bytes, binary identities and confidence limits are recorded in
[`SCALEFORM_END_FRAME_1170.md`](re/SCALEFORM_END_FRAME_1170.md).

That evidence exposed a local state bug: RazKolbas created and cleared a
display-sized D24S8 companion, but `commitPublishedUi()` rebound the native
color target with a null DSV immediately before deferred Scaleform submission.
Source 0.1.75 binds that prepared native DSV for the `NativeUi` phase while
keeping the earlier processing bind color-only. The new WARP assertion failed
against 0.1.74 because the bound DSV was null and passes after the correction.
Complete Debug and Release builds each pass all 40 CTest groups.

Source commit `c3fad61` is pushed. The independently extracted and verified
package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.75-ui-stencil-c3fad61.zip`, SHA-256
`429a876e024f8725503ba540924695653f15a97007c7bf4dacb2e466cf360a5e`.
Every payload matches its manifest and the NVIDIA runtime signature is valid.
With Skyrim stopped, the previous mod was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.75-c3fad61-20260925`, then the complete
stage was copied into the existing MO2 mod while preserving `meta.ini`.
Installed DLL SHA-256 is
`8a84995909ba3d5f367c297bc23a6a43c05c9576abdc47578d2b7172d4ca16ff`;
the restored INI and signed NVIDIA runtime retain their expected hashes
`2053bdc20ddf21d7c4a6bf351dfcd1cd4705d3557d34cdfb69db296122cd4ff1`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All effective installed payloads match manifest SHA-256
`e32ec5243b2b0c50e4ae3031e460640e465c1571bcc0ba7a50ee5c15e3adcbac`.
Actual-game removal of the two symbols is **NOT RUN** pending one user start.

## 0.1.74 native UI scissor-coordinate repair (2026-09-25)

The user started installed 0.1.73 and confirmed the two malformed centre
widgets were visible. The automatic menu sequence completed on frame 6661 at
`owned-ui-sequence-21536-6661-51193187`; its manifest SHA-256 is
`4fca162add3f2723c83550723787b6c2839b765172948a821de46a51df2e7ae1`.
It resolved 17 live entries, including `TrueHUD`, two Better Third Person
Selection menus and `HUD Menu`. Every snapshot taken before those entries is
byte-identical. The after-all snapshot differs in only 15,116 centre-crop
pixels inside local bounding box `(398,258)-(747,360)`, exactly covering the
two reported fragments. This proves the movie calls queue their GPU output for
a later common UI flush; it does not justify attributing the pixels to the
first menu in stack order. The active TrueHUD configuration enables projected
actor info bars, which makes TrueHUD the strongest content-owner candidate,
but the capture alone does not prove it. Skyrim closed normally at the user's
previously authorized post-investigation boundary.

The state evidence exposes a shared routing error independent of the widget
owner. `NativeUiRedirector` translated a reduced full viewport to the native
target but did not intercept `RSSetScissorRects`. A Scaleform widget could
therefore rasterize through the enlarged viewport while retaining reduced
pixel-coordinate clipping rectangles. Source 0.1.74 adds the exact ENB
2026-05-08 context slot 45 contract at RVA `0x5d100`, verified by the existing
module hash, size, image, table and method-prologue gates. It scales scissors
only while the native target is bound and the immediately effective viewport
was translated from the reduced extent. An explicitly native viewport clears
that state and leaves subsequent scissors byte-for-byte unchanged. The first
remap and powers of two are logged for runtime proof. A WARP regression first
failed to compile because the scissor callback was absent; it now verifies a
32x16-to-64x32 viewport/scissor translation, native-scissor pass-through and
the remap counter. Complete Debug and Release builds each pass all 40 CTest
groups. Actual-game removal of the fragments remains pending.

Source commit `5207858` is pushed. The independently extracted package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.74-ui-scissor-5207858.zip`, SHA-256
`024b61404cd33902d727338b7084ead49346a42510d8df1e0ae40f32689759ee`.
It contains exactly the four manifest payloads plus the manifest; all payload
hashes match and the NVIDIA runtime signature is valid. With Skyrim stopped,
the complete 0.1.73 mod was copied to ignored
`artifacts/local/mo2-install-backup-0.1.74-5207858-20260925`. Only the plugin
DLL and manifest were replaced. Installed DLL SHA-256 is
`77bb9dc805494a4be023418f202fa45e16aa123e7c778d2f48bb78dc9abfad42`.
The latest loose INI written by the user's prior session was preserved at
`2053bdc20ddf21d7c4a6bf351dfcd1cd4705d3557d34cdfb69db296122cd4ff1`;
the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All effective installed payloads match the new manifest. The actual-game run
logged scissor remaps through at least 512 calls, but the user still saw both
symbols. The visual test therefore **FAILED** and led to the deferred
Scaleform/stencil investigation above.

## 0.1.73 same-frame per-menu UI owner trace (2026-09-25)

The user started installed 0.1.72 in the affected scene and confirmed that the
two centre symbols were visible. Frame 17461 produced the automatic three-stage
bundle `owned-sr-stages-26912-17461-31821953`. Its manifest SHA-256 is
`3f70cd06f0a50d1363ff5612cbff1b43b375a1af71f3ff190cf94079cff9a05e`.
The 1707x960 prepared DLSS colour input and the 2560x1440 raw DLSS output before
post-sharpening are both clean. The symbols first appear in the same frame's
2560x1440 final pre-Present composition. This runtime result rules out source
contamination, DLSS reconstruction, post-sharpening and temporal ghosting as
their origin. It places the first draw in Skyrim's later native UI composition.
Skyrim then closed normally at the user's request.

Source 0.1.73 adds a single-process, single-frame owner trace. When the first
valid DLSS evaluation arms the existing three-stage capture, each verified
pre-`IMenu::PostDisplay` callback now reads only the centre half-width by
half-height region of the native target. It names that snapshot with the
corresponding live menu-stack ordinal and registered menu name, then saves one
final after-all-menus snapshot. Comparing adjacent files identifies the exact
menu interval in which the symbols first appear. The UI singleton pointer cell
comes from Address Library AE 1.6.1170 ID 400327 (RVA `0x20f6a00`) and remains
behind the existing exact executable hash and ABI gates; no CommonLib runtime
link was added. A new WARP regression verifies exact rectangular GPU readback,
pixel packing and bounds rejection. Complete Debug and Release builds each pass
all 40 CTest groups. Actual-game validation remains pending at this source
checkpoint.

Source commit `d8997ae` is pushed. The independently verified package is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.73-ui-owner-d8997ae.zip`, SHA-256
`597c04ae8ffb46152fe1dcb7670ad883861360d9f0640dfa968ec4cd0f8ffca0`.
It contains exactly the four manifest payloads plus the manifest, and every
extracted payload hash matches. With Skyrim stopped, the complete prior mod was
copied to ignored
`artifacts/local/mo2-install-backup-0.1.73-d8997ae-20260925`. Only the plugin
DLL and manifest were replaced. Installed DLL SHA-256 is
`c8f09384c9448646dd23b9de94d1e28c90e5fa652c36e9eeba31e13d2c189d12`.
The loose user INI remains
`b95d0ce49ce0ac9c9e03375cf4e1fc2d2eacbdf5234cbd6a2a5354c6c2db611c`
and the signed NVIDIA SR runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The actual-game menu sequence is **NOT RUN** and requires one user-started
save load in the affected scene.

## 0.1.72 same-frame DLSS stage capture (2026-09-25)

The user started installed 0.1.71, loaded the affected scene and reported that
the two centre symbols remained. The process stayed responsive, Present calls
continued succeeding, the owned 1707x960-to-2560x1440 route activated at frame
11042 and real DLSS submissions began at frame 11131. This **FAILS** the
0.1.71 visual symptom test and rules out the repaired target/viewport ordering
defect as the cause of those symbols. Skyrim closed normally at the user's
request after the log evidence was collected.

Source 0.1.72 adds one bounded diagnostic for the next run. On the first valid
evaluation token it reads back the exact prepared reduced colour input and the
raw display-sized DLSS output before post-sharpening. At the same frame's
pre-Present boundary it reads the final display composition and atomically
saves all three raw images plus extents, formats and hashes below
`Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures`. The
diagnostic then disables itself for that process. A new WARP test first failed
because the evaluated-stage API was absent, then verified the selected input
and output extents and output pixels. Complete Debug and Release builds each
passed all 40 CTest groups. Source commit `c162112` is pushed.

The verified archive is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.72-three-stage-capture-c162112.zip`,
SHA-256
`0f320868e3ede731981a4ca6a26a3f1d725508d3ae28ad9eb2dff10035a10808`.
Independent extraction verified the exact manifest file set and every payload
hash. With Skyrim stopped, 0.1.71 was backed up below ignored
`artifacts/local/mo2-install-backup-0.1.72-c162112-20260925`; only the DLL and
manifest were replaced. Installed DLL SHA-256 is
`c7fb2ad8109176d7d13db469948c9abda6c620b9f2244045443e499fb57803b4`.
The NVIDIA runtime and loose user INI remained byte-identical. The actual-game
three-stage capture **PASS** is recorded in the 0.1.73 section above; its final
composition contains the reported symbols while both DLSS stages are clean.

## 0.1.71 native UI viewport-order repair (2026-09-25)

The user's 0.1.70 Quality screenshot showed two red/white symbols near the
centre while the reduced 1707x960-to-2560x1440 DLSS route and native UI route
were active. The screenshot does not identify the widget owner or prove that
the symbols entered the DLSS input. The supplied UI-boundary report, SHA-256
`687382c2f295fca0016af9d156ef0ecc28df817d6fdfbc478a30b9dd8e94ff45`,
corrected an earlier interpretation: Skyrim's base `IMenu::PostDisplay()`
calls `uiMovie->Display()`, so the current callback precedes the menu movie
draw. Moving that callback earlier was therefore not justified.

The preserved `RazKolbas_UI_State_DeepDive_14.zip`, SHA-256
`ae8470da530613cd26e7508fc171fc3b08e68117c690ceec2102aeb9fad8d8d2`,
passed all hashes in its own `SHA256SUMS.txt`. Its strongest source-level
finding reproduced in the current tree: a custom menu can bind an offscreen
reduced target, set a reduced viewport, then restore the cached scene RTV
without another viewport call. RazKolbas translated that scene RTV to the
native target but left the effective viewport reduced. The new WARP test first
failed with a 32x32 effective viewport where the translated target required
64x64. The redirector now repairs that still-reduced full-scene viewport at
the successful target transition while preserving unrelated targets, origin
and depth range. The focused test then passed, and complete Debug and Release
builds each passed all 40 CTest groups. Source commit `65b3a76` is pushed.

The verified MO2 archive is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.71-ui-viewport-65b3a76.zip`,
SHA-256
`d679ddeb272576e43a47a5d5ba8a3d7c476de0011b179c92ed936e5fe437bf6b`.
Independent extraction found exactly the four manifest payloads plus the
manifest and verified every payload hash. With Skyrim stopped, the old mod was
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.71-65b3a76-20260925`; only the DLL and
manifest were replaced. Installed DLL SHA-256 is
`f54693d720c3b9692542da6975d5ffea1597bbbf596199b69fcd395626cc7d4b`.
The signed NVIDIA runtime and loose user INI remained byte-identical at
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`
and `b95d0ce49ce0ac9c9e03375cf4e1fc2d2eacbdf5234cbd6a2a5354c6c2db611c`.
Actual-game removal of the two symbols **FAILED** in the user-started run
recorded above. The repair remains because its isolated state regression is
real, but it is not attributed to the visible symbols.

## 0.1.70 startup depth retry and sharpening verification (2026-09-24)

The user reported that the sharpening slider had no visible effect in the
first 0.1.69 run. The slider did update and persist its requested value, but
the renderer had produced no NGX frame to sharpen. RazKolbas exhausted its 24
world-depth probes at `22:18:50`, while the save finished loading at
`22:19:58`; every sampled startup frame contained only cleared far-plane
depth. The one-shot readiness limit therefore disabled DLAA before gameplay.

The user's next 0.1.69 launch loaded the save within the initial probe window.
At `22:31:38` the twelfth sample passed with 93 distinct and 92 non-far depth
samples. NGX evaluation completed, output was finite and nonuniform, and the
continuous display path reached at least 7,800 submitted frames with zero
reported skips. The user then reported that sharpening worked. The preserved
requested configuration for that result is Native (DLAA), model preset Auto,
sharpening enabled and sharpness `0.699999988079071`. This is direct runtime
evidence for DLAA plus the post-sharpen path; it is not reduced-resolution
DLSS SR evidence.

Source 0.1.70 removes the terminal startup-scene failure. It continues depth
probing after the initial 24 attempts at a lower readback rate, allowing a
later save load to activate DLAA. It also logs successful or rejected live
sharpening updates in the non-owned presenter path. Test-first validation
covered the extended retry schedule. Debug and Release each pass all 40 CTest
groups. Source commit `ee00286` is pushed.

The verified MO2 archive is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.70-depth-retry-ee00286.zip`, SHA-256
`23af8ec622cd20cbda41e164e2933a9c565eb997787a1da9dd9af6f40672d705`.
Installed DLL SHA-256 is
`e77f455fc7f1c0339ca58bc868b36e341bdeb0debef4f3c7031e131a68c20871`;
the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The user's latest INI was preserved at SHA-256
`138ed26a1e8f42d855cd764d50667bf4110333e62f3b3ce84930f0fb0b5f929d`.
The prior mod and loose INI are backed up in ignored
`artifacts/local/mo2-install-backup-0.1.70-ee00286-20260924-223454`.
Skyrim was closed before installation.

The user then started 0.1.70 with Quality mode. Runtime installation and the
reduced route **PASS**: the plugin selected `requested=true`, created a
1707x960 owned scene for the 2560x1440 display, admitted real world colour and
depth at frame 32221, created the NGX feature and activated the native UI
resource route. The first evaluation used game jitter `(-0.125,0.27777776)`,
MV scale 1707x960, neutral NGX sharpening and post-sharpness 0.7. The route
reached at least 5,460 provider submissions with zero fallbacks in flight and
no post-activation warning or error. Live menu interaction toggled sharpening
and swept its value; each change reached the presenter, and the saved setting
settled at enabled/0.75. The menu closed and released mouse capture. Visual
quality, camera suppression, focus-loss restoration and a post-load scene
transition still require the user's explicit observation; they are not
inferred from the log.

## 0.1.69 input-dispatch menu capture candidate (2026-09-24)

The user-started 0.1.68 test **FAILED** camera suppression. Live inspection
proved that RazKolbas selected `ControlMap +0x129`, wrote the byte while the
menu was open, and restored it on close. The camera still moved, so the
`ignoreKeyboardMouse` field is insufficient for this gameplay input path.

The working DynamicShaderFrameGen reference suppresses input one level lower:
at the `BSInputDeviceManager` dispatch boundary it forwards a static null
event list while its menu is visible. Live 1.6.1170 disassembly mapped Address
Library ID 68617 to function RVA `0xcd8f40` and confirmed the relevant direct
CALL at `+0x7b` / RVA `0xcd8fbb`. Its exact argument setup passes the
dispatcher in RCX and `InputEvent* const*` in RDX. The active call was already
redirected through an SKSE relay to `SmartTalk.dll`; replacing it blindly
would break that mod.

Source 0.1.69 validates the executable hash, exact eight-byte argument setup,
CALL opcode and exact nineteen-byte continuation. It decodes and validates the
current executable target, installs its own register-preserving near relay at
the SKSE startup boundary, and keeps the previous target as the next owner in
the chain. While the End menu is visible and Skyrim is focused, the proxy
passes a one-element null event list to that chain; otherwise it forwards the
original pointer unchanged. The callback DLL and relay remain valid for the
process lifetime. Patch identity and recovery are recorded in
`patches/skyrim1170-menu-input-dispatch-v1.json`.

Test-first validation observed both new behavioral cases fail against the
initial stubs. The completed implementation passes 16 focused assertions.
Debug and Release each pass all 40 CTest groups. Runtime hook installation,
SmartTalk coexistence and camera suppression remain **NOT RUN** until the user
starts Skyrim.

Source commit `78474f4` is installed and ready for that test. The independently
verified MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.69-menu-input-dispatch-78474f4.zip`
has SHA-256
`1147270d7ca50a7fd94bfd6b8eab281a94df0c196b41093bb3ff616ce6ffdaa5`.
It contains exactly the four manifest payloads plus the manifest; all hashes
pass and the NVIDIA runtime signature is valid. Skyrim was closed for the
installation. The prior physical mod state is preserved in ignored
`artifacts/local/mo2-install-backup-0.1.69-78474f4`. Its sole old-manifest
mismatch was a missing `RazKolbas.ini` after the 0.1.68 run. Installation
restored that INI from the independently verified 0.1.68 extraction at its
previously recorded SHA-256
`38c8cc8051065d35cafd18ae7c0179c0dcde89a74cb8623e032d90e93671cec8`.
Installed DLL SHA-256 is
`553ca500015f795bf441dba620281b173a13833d49c4de90f31e1c6acb485e0b`;
the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match manifest status
`EXPERIMENTAL_MENU_INPUT_DISPATCH_PENDING_GAME_TEST`.

## 0.1.68 Anniversary Edition menu input layout fix (2026-09-24)

The user tested installed 0.1.67 and reported that the camera still moved
while using the mouse over the End menu. The current log proved that the
capture path resolved its singleton and believed the target byte was already
`true` before capture. That disproved the assumed shared `+0x121` field
layout rather than the state machine.

Skyrim 1.6.1170 has an AE-only Marketplace input-context pointer. It shifts
the `ControlMap` runtime data from `+0xE8` to `+0xF0`; therefore
`ignoreKeyboardMouse` is at `+0x129`, eight bytes after the 1.5.97 member at
`+0x121`. This is independently documented by the Community Shaders
CommonLibSSE-NG versioned runtime-data accessor and by a live 1.6.1170 byte
diff of the neighboring `enabledControls` member in CommonLibSSE-NG issue
111. RazKolbas 0.1.68 now selects the singleton RVA and field offset together:
1.5.97 uses `0x2ec5bd0/+0x121`; 1.6.1170 uses
`0x30fda10/+0x129`. Startup logs the selected pair.

Test-first validation reproduced the defect: the new AE profile assertion
failed with `0x121` before the correction and passed with `0x129` afterward.
Debug and Release each pass all 39 CTest groups. The subsequent user-started
test **FAILED** camera suppression even though live memory and logs confirmed
the correct byte changed. This result motivated the dispatch hook in 0.1.69.

Source commit `3bf5b55` is installed and ready for that test. The verified MO2
archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.68-ae-menu-input-fix-3bf5b55.zip`
has SHA-256
`04755170df6b1d795248646c9d65ace3f6758b2c3f593394bd4ee866e000ebbb`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest, all payload hashes passed, and the NVIDIA runtime signature is
valid. Skyrim was closed for installation. Only the DLL and manifest were
replaced; the user-edited INI remains
`38c8cc8051065d35cafd18ae7c0179c0dcde89a74cb8623e032d90e93671cec8`
and the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The installed DLL is
`e0e34e4c27bdfd96d4521bd4eedbb379f90a3810f4ac5aa4b3cb11ffc5623d95`.
All installed payloads match the final manifest status
`EXPERIMENTAL_AE_MENU_INPUT_CAPTURE_PENDING_GAME_TEST`. The prior installation
is preserved in ignored
`artifacts/local/mo2-install-backup-0.1.68-3bf5b55`; after an initial wildcard
copy syntax error, the rollback snapshot was reconstructed from the verified
0.1.67 stage plus the untouched INI/runtime and validated against the old
manifest (whose sole expected mismatch is the user-edited INI).

## 0.1.67 menu input capture candidate (2026-09-24)

The user verified that the 0.1.66 End menu renders and its widgets receive
mouse input, but Skyrim also receives the same mouse movement and rotates the
camera. The existing menu polls Win32 cursor/button state directly for ImGui
and does not suppress the engine input path, so the duplicated input is the
measured cause.

Source 0.1.67 adds a small ownership-aware capture state machine. While the
RazKolbas menu is visible and Skyrim owns focus, the Present callback sets the
engine `ControlMap::ignoreKeyboardMouse` byte. ImGui continues using its
independent Win32 polling. Closing the menu, losing focus or disabling the
menu restores the exact value observed when capture began. A conflicting
owner that clears the byte while capture is active is overridden on the next
menu frame; a block already owned by Skyrim or another mod is preserved on
release. The singleton slot is version-gated to supported runtimes: canonical
Address Library ID 514705 maps to RVA `0x2ec5bd0` for 1.5.97, and ID 400863
maps to RVA `0x30fda10` for the installed 1.6.1170 database. The field offset
is the CommonLibSSE-NG `ControlMap::ignoreKeyboardMouse` layout at `+0x121`.
Both the singleton slot and heap byte must be committed/readable, and the byte
must also be writable, before RazKolbas changes it.

Test-first validation observed the three new cases fail against the initial
no-op state machine, then pass after implementation. Debug and Release each
pass all 39 CTest groups. The 1.6.1170 Address Library file is unchanged at
SHA-256 `c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452`.
The canonical 1.5.97 CSV at source commit
`99070858ff1b9cedf94ed45762405ff16cb61ca0` was read only and had SHA-256
`abc579c9edb3a339c4c98406a11fa5c86257631e90bddc29fe404870f8401eaa`.
No game or reference file was modified. The subsequent user-started Skyrim
test **FAILED** camera suppression: ImGui remained interactive, but moving the
mouse also rotated the camera. The log showed capture enable/release events
and a pre-existing `true` value at the incorrectly assumed `+0x121` field.
The failure is addressed by the versioned layout in 0.1.68 above.

Source commit `746884c` is pushed to `codex/razkolbas-bootstrap`. The verified
MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.67-menu-input-capture-746884c.zip`
has SHA-256
`219c92f9cb68342c2300df81ad23daa39731b5462ee2c8097292127cc043ae2c`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest, all payload hashes passed, and the NVIDIA runtime signature is
valid. Skyrim was already closed. The complete prior mod was backed up to
ignored `artifacts/local/mo2-install-backup-0.1.67-746884c`; its only old
manifest mismatch was the INI intentionally changed through the 0.1.66 menu.
Only the installed DLL and manifest were replaced. Installed DLL SHA-256 is
`437c7c6c7befba41f20c97a043b3d5ec1be4ec1e1ad1af3f01fb4302656be67d`.
The preserved user INI and signed NVIDIA runtime retain SHA-256
`45027f3506e40ec4dbfee157a704f25c98b4cc1491582a42a082858afd2a4887`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the final manifest status
`EXPERIMENTAL_MENU_INPUT_CAPTURE_PENDING_GAME_TEST`.

## 0.1.66 reference-aligned menu layout (2026-09-23)

The supplied `SkyrimUpscaler.ini` and recovered `SettingGUI.cpp` strings were
inspected as reference evidence. The reference menu uses an Upscaling tab,
labels its primary controls `Quality Level`, `DLSS Preset`, `Enable
Sharpening` and `Sharpness`, explains that presets do not alter render
resolution, and separates user controls from performance/debug information.
Its current INI also confirms that A-E were removed after DLSS 3.10.4 and
describes J/K/L/M consistently with the pinned NVIDIA headers.

RazKolbas now follows that information architecture without exposing reference
features it has not implemented. The End window is titled `RazKolbas
Upscaler` and has `Upscaling` and `Diagnostics` tabs. The primary tab shows
current DLSS mode, display resolution, render resolution and scale before the
quality, preset and live sharpening controls. Friendly labels such as `Native
(DLAA)`, `Ultra Performance` and `Preset K` are mapped to the validated INI
tokens. Tooltips explain resolution and preset behavior. Detailed admission,
fallback and frame counters remain on the Diagnostics tab. Runtime visual and
mouse-interaction verification is pending.

Source commit `35b5b2e` is pushed. Debug and Release each pass all 38 CTest
groups. The verified MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.66-menu-layout-35b5b2e.zip`
has SHA-256
`949cfbf28a5560cb06ba2417904e321c0d7d4aae5d1827e50f2b04568f30465f`;
its Release DLL is
`a96c2482eadab13d508435a404e6a5c6dc9f81f1934e3d99ed9eb91eddab2351`.
Independent extraction found exactly four manifest payloads plus the manifest,
all payload hashes passed, and the NVIDIA runtime signature is valid. The
first installation attempt stopped without changing files because Skyrim was
running. After the user exited, all installed 0.1.65 payloads matched their
manifest and the complete mod was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.66-35b5b2e`. Only the DLL and manifest
were replaced. The installed 0.1.66 DLL matches the hash above; the user INI
remains
`2b3afaa42d207c07eb28d5de0f8fc53e5a7fdeb4179022d0b8134ec9f0d19d6c`
and the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All four installed payloads match the 0.1.66 manifest. Runtime visual and
mouse-interaction verification remains pending.

## 0.1.64 game result and 0.1.65 in-game DLSS controls (2026-09-23)

The user-started 0.1.64 loaded-save run passed. The user reported that the
image looked good and that DLSS appeared to be working. The session reached
20,400 world frames and 13,470 successful owned DLSS publications from
`1707x960` to `2560x1440`, with no fallback in flight, no error/critical log
records and no failed Present. The four warnings were the expected early-route
and first-three-frame admission messages. The startup evaluation recorded
neutral NGX sharpening and the separate native-resolution pass at `0.3`.
The ignored session log is
`artifacts/local/runtime-0.1.64-post-sharpen-2026-09-23-2016/current-session.log`,
SHA-256
`d663bafdbadeae509fa7365f9b98ce50979a8eb07554fc65d17202c693f99948`.

The next candidate adds controls to the existing End menu. Post-DLSS
sharpening has an enable checkbox and a `[0,1]` slider. Changes are queued by
the Present-side menu and consumed at the verified world-render boundary, so
the active presenter changes without recreating the NVIDIA feature. The INI
is saved when the checkbox changes or the slider is released. Quality offers
NativeAA, Quality, Balanced, Performance and UltraPerformance. Model preset
offers Auto, J, K, L and M, exactly matching the usable choices in the pinned
NGX headers. Quality and model changes are persisted and labelled for the next
game launch because they alter feature creation and owned render dimensions.
The selected non-Auto model preset is written to the quality-specific NVIDIA
hint before feature creation.

Source commit `5f26e11` is pushed to `codex/razkolbas-bootstrap`. Debug and
Release each pass all 38 CTest groups after the embedded version was advanced
to 0.1.65. Release DLL SHA-256 is
`67d4456423440a297e9f740c85d63a2ca30b6a333fd9e3c2834aa581e56f7db0`.
The independently extracted MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.65-menu-controls-5f26e11.zip`
has SHA-256
`13309e7f670de5b75a1c8a81736effa2a3517a3760bb8f393b31fd320a3b3fc4`.
It contains exactly the four manifest payloads plus the manifest; every hash
and the NVIDIA runtime signature passed verification.

Skyrim was absent during installation. All 0.1.64 payloads first matched the
old manifest and the complete mod was copied to ignored backup
`artifacts/local/mo2-install-backup-0.1.65-5f26e11`. Only the DLL and manifest
were replaced in `D:/TESV_EX/MO2/mods/RazKolbas`. The user INI remains
byte-identical at SHA-256
`2b3afaa42d207c07eb28d5de0f8fc53e5a7fdeb4179022d0b8134ec9f0d19d6c`;
the signed NVIDIA runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed manifest payloads match. Runtime interaction with the new menu
widgets is pending a user-started loaded-save test.

The supplied `PureDark_Sharpening_12.zip` passed its 155-payload manifest
check. Its ordinary sharpening reconstruction confirms the current stage
placement, five-tap RGB formula, gain mapping and direct code-value filtering.
It also identifies deliberate reference details that the RazKolbas safety
implementation does not claim to reproduce bit-for-bit: unclamped
out-of-bounds loads, the `0x7EF19FFF` reciprocal approximation, alpha forced
to one, and singular arithmetic without epsilon guards. Exact evidence and
the implementation comparison are recorded in
`docs/re/SHARPENING_REFERENCE.md`.

## 0.1.63 sampler result and 0.1.64 post-DLSS detail pass (2026-09-23)

The user-started 0.1.63 loaded-save run resolved the sampler question. All six
hooks remained installed (`0x3f/0x3f`) and received live traffic. The pixel
stage exceeded 16 million calls. Every observed anisotropic sampler already
had an authored negative bias, normally `-1.0`; later pixel-stage traffic also
used `-0.5`. Observed zero-bias samplers had `MaxAnisotropy == 0`. Consequently
no descriptor satisfied the reference predicate of zero bias plus anisotropy
greater than one, and RazKolbas correctly created no replacement. Over the
same run, native `2560x1440` mode-2 publication reached 8,760 DLSS submissions
with zero errors, zero Present failures and zero fallback in flight. The four
warnings were expected startup/admission messages. The responsive game was
closed after evidence collection. The ignored filtered log is
`artifacts/local/runtime-0.1.63-sampler-trace-2026-09-23-1937/current-session.log`,
SHA-256
`c0f8eac598c49475948e82a5f02f367af8dc1c070982eb229c30de3b0b1a1ec3`.

This rules out hook overwrite, wrong vtable and missing sampler calls for this
load order. Overriding ENB's existing `-1.0`/`-0.5` values would violate the
recovered reference contract and could add shimmer. The temporary per-call
descriptor telemetry is removed. The retained sampler path now caches
permanently ineligible sampler identities, matching the reference behavior and
avoiding repeated descriptor reads.

The next candidate implements the missing independent image-detail control as
a native-resolution contrast-limited five-tap D3D11 pass on the completed DLSS output. It runs
immediately before publication into the active display target, so the existing
native UI route composes afterward and is not sharpened. NGX receives neutral
integrated sharpness because the pinned SDK marks that field unsupported. The
pass preserves alpha, maps configured `Upscaling.Sharpness` in `[0,1]` to the
recovered `exp2(2*s-2)` gain, bounds its negative lobe to `[-0.1875,0]`,
caches shaders/constants and the three persistent output SRVs, and restores
the caller's D3D11 state. A WARP test first failed with the renderer absent,
then verified the expected adaptive five-tap output and argument validation.
A presenter-level nonuniform fixture proves enabled sharpening changes the
published pixel, disabled sharpening performs an exact copy, NGX receives
neutral sharpness, and the active RTV is restored. All 38
CTest groups pass in both Debug and Release. Actual Skyrim appearance and
stability remain NOT RUN for this candidate.

Source commit `4825fc0` is pushed to `codex/razkolbas-bootstrap`. Release DLL
SHA-256 is
`5c7b2171aa9c5661a5dcc78d5abda63b300af8da594687da3c58c16dc1eb1fb2`.
The MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.64-post-sharpen-4825fc0.zip`
has SHA-256
`cb9f19e7654b9c11cd5f066ccb7a4f25304bae88de35f0039e1c247301bc435a`.
Independent extraction verified exactly the four manifest payloads plus the
manifest, every payload hash, and the NVIDIA runtime signature.

Skyrim was absent during installation. All four prior 0.1.63 payloads matched
their manifest before the complete mod was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.64-4825fc0`. Only the DLL and manifest
were replaced. The installed user INI and signed NVIDIA runtime remain
unchanged at SHA-256
`2b3afaa42d207c07eb28d5de0f8fc53e5a7fdeb4179022d0b8134ec9f0d19d6c`
and
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the 0.1.64 manifest, SHA-256
`71af55c9e3363e6206591001ba38c6f9cd297b6a6d690aca484dfca397379167`,
whose status is `EXPERIMENTAL_DLSS_POST_SHARPEN_PENDING_GAME_TEST`.
The next user-started loaded-save run should verify continuous DLSS, an
`ngxSharpness=0; postSharpness=0.3` startup line, no errors or Present
failures, retained ENB/ReShade appearance and improved perceived scene detail.

## 0.1.62 mip-detail correction installed; game test pending (2026-09-23)

The 0.1.61 run proved continuous owned DLSS evaluation and correct ENB/UI
routing, but the user reported that the otherwise correct image still looked
like a lower-resolution image. Source inspection found that the declared
automatic mip-bias policy had no sampler implementation. The supplied PureDark
RE independently identifies six shader-stage sampler wrappers and a restrictive
replacement rule: change only samplers with zero existing `MipLODBias` and
`MaxAnisotropy > 1`. The reference configuration uses `-0.584962`, equal to
`log2(2/3)` for Quality mode.

The in-progress correction computes automatic bias from the smaller
render/display axis, validates the
exact installed ENB PS/VS/GS/HS/DS/CS sampler slots before patching any of
them, and caches cloned sampler states without modifying originals. Debug
and Release pass all 37 CTest groups, including new WARP replacement/cache,
profile-admission, mip-policy and parameter-delivery tests. Independent review
confirmed the exact installed ENB sites and identified the NGX SDK's explicit
statement that integrated DLSS sharpening is unsupported. The configured
value is now propagated truthfully for API completeness, but it is not claimed
as an effective correction; the supplied reference uses a separate post-upscale
filter. The safe manual-bias range is consistently `[-3,3]`. A partial
nine-slot hook-install conflict leaves a conservative unarmed lease and permits
no retry until the next launch.

Source commit `df60c0a` was pushed to `codex/razkolbas-bootstrap`. Debug and
Release each pass all 37 CTest groups after the review correction. Release DLL
SHA-256 is
`4ea6bb3114d0a95dfa456dfb5cf060bed8e3bad15de7e992431ce61429782203`.
The MO2 archive
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.62-mip-bias-df60c0a.zip` has SHA-256
`1671bfd52dd9a0d2276264f2c13c330759ae640fbdaa2fe8e9356c9c180c91e9`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest, verified every payload hash, and verified the NVIDIA runtime's
Authenticode signature.

Skyrim was absent during installation. Every 0.1.61 payload first matched its
manifest, and the complete prior mod was copied to ignored
`artifacts/local/mo2-install-backup-0.1.62-df60c0a`. Only the plugin DLL and
manifest were replaced; the user INI and signed NVIDIA runtime retain SHA-256
`2b3afaa42d207c07eb28d5de0f8fc53e5a7fdeb4179022d0b8134ec9f0d19d6c`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the 0.1.62 manifest, whose status is
`EXPERIMENTAL_DLSS_MIP_BIAS_PENDING_GAME_TEST`. The assistant did not start
Skyrim. The next user-started loaded-save run must verify six sampler hooks,
automatic bias near `-0.5849625`, at least one cached replacement, continuous
DLSS evaluation, ENB/ReShade appearance, stability and perceived texture
detail.

### 0.1.62 runtime result and 0.1.63 sampler trace

The user started 0.1.62 at 19:09 and loaded a save. All six exact sampler
slots installed with automatic bias `-0.5849625`. The early ENB contract
remained correct at `1707x960`, DLSS created and first evaluated successfully,
and native `2560x1440` mode-2 publication reached 2,820 provider submissions
with zero fallback in flight. The session had zero errors and zero Present
failures; its four warnings were the expected startup/admission warnings.
However, no sampler replacement was created. The mip-bias correction therefore
did not affect this run. The assistant closed the responsive game after
collecting evidence. The filtered ignored session log is
`artifacts/local/runtime-0.1.62-mip-bias-2026-09-23-1909/current-session.log`,
SHA-256
`674d8030181cf7004e8a98575bef43bb9a8de867d580bead1bccf84beb7d44a5`.

Fresh independent disassembly of supplied `SkyrimUpscaler.dll`, SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`,
confirms that its D3D creation wrapper at RVA `0x155D40` loads the returned
immediate context and patches slots 10/26/32/61/65/70 through helper RVA
`0x172060`; wrappers and saved downstream globals match the supplied excerpts.
This corroborates the six-slot target but does not yet distinguish a later
owner overwrite from a live descriptor that fails the reference predicate.

Source now adds bounded diagnostic evidence. It reports sampler-hook ownership
as a six-bit mask and, at power-of-two calls for each stage, reports non-null,
zero-bias, anisotropy and eligible counts plus the first descriptor values.
Debug and Release each pass all 37 CTest groups. The next diagnostic package
must be installed and run before changing the sampler predicate or hook layer.

Source commit `ecd3274` is pushed and packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.63-sampler-trace-ecd3274.zip`,
SHA-256
`5180b5843db40039670ab00b3be0bb2d841893a27e5240a0eb33da4454e95852`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest, verified all hashes, and verified the NVIDIA runtime signature.
With Skyrim stopped, all installed 0.1.62 payloads matched their manifest and
the complete mod was copied to ignored
`artifacts/local/mo2-install-backup-0.1.63-ecd3274`. Only the DLL and manifest
were replaced. Installed DLL SHA-256 is
`e9026aa0827cfe3dd3305c7b51393586c17238ecefe52164a5adc73516357bd5`;
the user INI and signed NVIDIA runtime remain unchanged. All installed payloads
match the 0.1.63 manifest, whose status is
`DIAGNOSTIC_DLSS_SAMPLER_OWNERSHIP_TRACE_PENDING_GAME_TEST`. The assistant did
not start Skyrim.

## 0.1.61 live DLSS runs continuously; quality correction required (2026-09-23, 16:54 launch)

After the 0.1.60 spatial route passed both runtime checks and the user's
visual ENB check, Skyrim was closed and that exact install was backed up under
ignored
`artifacts/local/mo2-install-backup-0.1.60-early-enb-contract-e186b1c-visual-good/`.
The 0.1.61 payload reuses the reviewed `e186b1c` binary and official signed
NVIDIA runtime. Its only functional configuration change is
`SpatialBaselineOnly=false`; `Provider=Auto`, `Quality=Quality`, and
`ManualRenderScale=0.666667` are unchanged. This lets the already validated
early ENB/UI resource contract proceed to live NGX evaluation.

The payload is installed at `D:/TESV_EX/MO2/mods/RazKolbas`, and all four
manifest payload hashes match after installation. Installed DLL SHA-256 is
`8b256903a729a5447bf393172fc68d2f87968fe0740b93a40b1581f52fd5efd7`;
INI SHA-256 is
`2b3afaa42d207c07eb28d5de0f8fc53e5a7fdeb4179022d0b8134ec9f0d19d6c`;
manifest SHA-256 is
`8ef3cba44e3cd46854c46ddce9669abe08988f127bc34e90b4e47630309ed07b`.
MO2 package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.61-live-dlss-e186b1c.zip` has
SHA-256
`81501d0adee368e0a3e08305a969704ae5ca4bc8a0827d34d310388aeaa72833`.
The user started Skyrim at 16:54:05 and loaded the game. The early ENB
contract remained valid in all 35 bounded probes through world-forwarded frame
20,400: actual, metadata and ENB reference dimensions were `1707x960`,
`dimensionMatch=true`, bound mask was `0x67`, and slots 5/6 were present. The
owned scene reached its colour/depth admission gate at frame 12,601. NGX
feature creation succeeded, the first evaluation returned, and continuous
mode-2 publication reached 7,680 provider submissions with zero fallbacks in
flight. Present reported zero failures; the log contained zero errors and only
the four expected startup warnings.

The user reported that the image and ENB appearance were good, but that it
visually looked like a lower resolution. This is a route/stability pass and an
image-quality failure. The final filtered session snapshot is retained under
ignored `artifacts/local/runtime-0.1.61-live-dlss-2026-09-23-1654/`, SHA-256
`4e70d5b9fbd62f2c08122ee72fea0eccec830c0563e5da6261fee33fa814b6b2`.

## 0.1.60 early ENB contract passes runtime and visual test (2026-09-23, 13:08 launch)

Commit `e186b1c` implements creation-time reduced scene publication for the
exact installed ENB/ReShade chain and was pushed to
`codex/razkolbas-bootstrap`. Final Debug and Release builds each passed all
36 CTest groups; the focused route/fallback/resize selection passed 352
assertions in 23 cases. Final independent re-review found no blocker.

Skyrim was closed and the exact 0.1.59 installed payload matched its manifest.
It was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.59-enb-target-probe-922c6d9-before-e186b1c/`.
The 0.1.60 payload was then installed at `D:/TESV_EX/MO2/mods/RazKolbas`.
Installed DLL SHA-256:
`8b256903a729a5447bf393172fc68d2f87968fe0740b93a40b1581f52fd5efd7`;
INI SHA-256:
`d84768616750ca01c66ad68a9984b8070887867c20c8a4056b88f318d9f858e5`;
manifest SHA-256:
`a5318524bb6ff3c125074a2cb4b80f67448c243229650b4ddb3314455a80a730`.
All four manifest payload hashes were verified after installation.

MO2 package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.60-early-enb-contract-e186b1c.zip`
has SHA-256
`9f12b07ecab71166473c092cf62c2f06f07a52db482f4360e404bf22d47ab109`.
The configuration remained `Quality=Quality`,
`ManualRenderScale=0.666667`, `SpatialBaselineOnly=true`. The user started
Skyrim at 13:08:09 and loaded the game with ENB and ReShade active. The
session produced 50 bounded ENB probes through world-forwarded frame 29,400;
every probe reported actual, metadata and ENB reference dimensions of
`1707x960`, `dimensionMatch=true`, bound mask `0x67`, and slots 5/6 present.
This is the exact runtime reversal of the 0.1.59 late-alias failure. Native
publication remained `2560x1440`, reduced and native samples were non-black
and diverse, the native UI route activated, and Present reported zero failed
or occluded calls through at least world-forwarded frame 31,200. There were
zero error records. The four warnings were the expected one-time emergency
startup publication and three pre-admission provider attempts.

The user reported that the image and ENB appearance looked good. This closes
the spatial ENB/resource-contract validation. The installed payload was
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.60-early-enb-contract-e186b1c-visual-good/`.
A filtered runtime snapshot is retained under ignored
`artifacts/local/runtime-0.1.60-early-enb-contract-2026-09-23-1308/` with
SHA-256
`35de79d84c34b8a7a2b58a70bcb35da343bcc764122f4b587328ec019bb8dc53`.
The next controlled run removes only the `SpatialBaselineOnly` diagnostic
gate so the same early resource contract reaches live NGX evaluation.

## 0.1.59 proves the reduced-scene ENB dimension failure (2026-09-23, 12:09 launch)

The user started Skyrim and loaded a save with ReShade unchanged. PID 4040
started at 12:09:12 and exited normally after a window-close request once
collection was complete. The exact 0.1.59 manifest and payload hashes matched
the installed mod. This session produced 64 bounded ENB target samples from
world-forwarded frame 0 through 37,800. Every sample reported:

- actual and private-metadata HDR target `1707x960`, format 10;
- readable ENB reference dimensions `2560x1440`;
- `dimensionMatch=false`;
- requested target count 3, resulting bound mask `0x07`, with slots 5/6 absent.

There were zero error records. The three warning records were the existing
first-frame owned-admission messages. Spatial publication continued and no
NGX evaluation was used. This proves the dimension condition recovered at
ENB RVA `0x691CB/0x691D3` is false on the reduced route and that its two
auxiliary attachments are skipped in the observed calls. It does not identify
the artistic names or contents of those attachments, nor does it exclude a
separate ReShade issue after this condition is repaired.

The full cumulative log is retained under ignored
`artifacts/local/runtime-0.1.59-enb-target-probe-2026-09-23-1209/RazKolbas.log`,
SHA-256 `9bdf8307b7656fc8bd7db6abc4f2ab8a34acf13b726f3f0d0c9c174948eb9678`.
Current-session filtering produced 822 lines; all 64 probe lines matched the
same failed dimension/attachment result.

The next implementation moves owned scene allocation to the nested factory
callback before ENB's first description/buffer query. A verified ReShade
GetDesc slot 12 hook exposes reduced dimensions only to exact ENB callers
`0x5E53E` and `0x4872D`; exact ENB buffer callers `0x5E580/0x5E795` and
Skyrim `0xE4CC87` share the stable scene. RazKolbas and ReShade presentation
callers continue to receive the native buffer. Provider/UI/presentation
attachment remains in the outer callback and must match the early extent.
Early publication is admitted only for the captured factory, the verified SDR
flip descriptor, the exact ENB creation owner and unchanged ENB UI vtable
sites. The reduced surface must share canonical D3D11 device identity with
the nested swap, outer device, context and swap. NGX preflight failure or an
optimal-size mismatch now commits the same route with display-sized spatial
publication instead of abandoning the reduced scene. Resize atomically stops
new owned queries before native resize, retains the old scene for outstanding
references and defers cross-thread UI cleanup to the render thread. A foreign
thread cannot forward ResizeBuffers while that cleanup remains pending; an
owner-thread retry drains it synchronously before forwarding.

The first test run failed on the missing APIs; the implemented path passes
the focused 352 assertions in 23 cases and all 36 Debug and Release CTest
groups. Independent review found the initial admission, provider-failure,
device-lineage and resize gaps; each was corrected before packaging. If any
late outer integration step fails, the nested ReShade Present hook now draws
the same reduced scene to the native inner flip target with the existing
spatial presenter. This preserves a complete display path after ENB has
cached the early resource while leaving the normal path untouched once late
integration succeeds.

The supplied `PureDark_ENB_Integration_11.zip` was read only as reference
evidence (SHA-256
`46d41c628e3ea1ee4707f4eff94a2dc60f382cd7588f26b4c5b6a77252237db7`).
Its independently decoded reference path confirms that AIO constructs the
owned proxy during nested factory creation and has a host pre-Present
missing-work path before forwarding Present. It identifies no ENB SDK
resolution callback and supports the selected description-plus-resource
contract. The archive remains outside the repository.

Game runtime verification of this implementation is NOT RUN.

## 0.1.59 ENB target-dimension diagnostic installed (2026-09-23)

Read-only RE of the exact installed ENB found that its HDR MRT attachment
path compares RTV private-data dimensions against globals populated from
swap GetDesc. A mismatch skips slots 5 and 6. ENB refreshes those globals
through another GetDesc path as well. This is a concrete integration
condition, not yet the measured cause of the user's missing-effects image.
Evidence, exact RVAs, metadata layout and limits are in
`docs/re/ENB_TARGET_DIMENSION_CONTRACT.md`.

The new read-only diagnostic uses the existing owned-world OM hook. After
the unchanged downstream bind it samples actual texture size, ENB metadata,
current reference dimensions and bound-target mask. The module hash and
comparison bytes must match. Sampling includes failed candidates in its
budget (eight attempts per 600-call window, at most 64 windows). It does
not change rendering, metadata, shaders or effect settings.

Validation: the test-first build failed because the new probe API did not
yet exist; the budget regression build likewise failed before that API was
added. Final `tools/Build.ps1 -Preset win-dev` and `-Preset win-release`
both passed all 36 CTest groups. WARP tests distinguish actual dimensions
from deliberately different private metadata, reject short metadata, check
binding preservation and target-mask reporting, and cover real inaccessible
memory plus sample-budget exhaustion. Static exact-byte validation against
the installed ENB also passed. Independent review identified the initial
unbounded non-HDR inspection path; it was corrected and re-reviewed.

The new probe's Skyrim runtime test is NOT RUN. ReShade remains a possible
contributor to reduced-path ordering or resource issues; no ReShade-off
comparison has been performed. The established native DLAA visual result
remains the baseline. Source commit `922c6d9` was packaged and installed into
`D:/TESV_EX/MO2/mods/RazKolbas` while Skyrim was closed. The full known-good
DLAA mod, including its INI and manifest, was copied to ignored
`artifacts/local/mo2-install-backup-0.1.59-native-dlaa-922c6d9/` first.

The next-run INI uses `Quality=Quality`, `ManualRenderScale=0.666667` and
`SpatialBaselineOnly=true`, so the probe exercises the failed reduced route
without NGX evaluation. ENB and ReShade settings were not changed. All four
installed payload hashes match the new manifest. DLL SHA-256:
`45c0cf2b08857653f8e1038b3698025209cbc0d44e8a97f330baea5769dea695`.
INI SHA-256:
`d84768616750ca01c66ad68a9984b8070887867c20c8a4056b88f318d9f858e5`.
MO2 package `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.59-enb-target-probe-922c6d9.zip`,
SHA-256 `457aee3ddde1e28b4e1b49454d8310eab4592ce3cd9e8dc5b7a3db58b5da4f90`.
This is a diagnostic package, not a visual fix or a DLSS SR completion claim.

Next: the user starts Skyrim, loads a save and leaves the scene running for
roughly 30 seconds; collect current-session `ENB target probe` records and
correlate reference/metadata dimensions with slots 5/6. No game was launched
by the assistant. If metadata is valid and the dimensions differ, implement
consistent early ENB description/buffer publication with matching allocation
and a safe failure path. If they match, continue tracing the producer and
late effects order, including ReShade, instead of applying a sizing patch.

## Native DLAA visual baseline confirmed (2026-09-23, 11:39 launch)

The user started Skyrim with the NativeAA configuration and reported the image
looks good. Process 22156 remained responsive. The current-session log confirms
native 2560x1440 buffers and camera extent, `ownedRouteArmed=false`, zero owned
route activations, and 6,000 continuous DLAA submissions by world frame 17,421
with `skipped=0`. Sampled Presents report zero failures. No warning/error entry
appears in the 283-line session snapshot. This validates the current binary's
native DLAA appearance by user observation, not just a historical build.

The filtered log is retained under ignored
`artifacts/local/runtime-0.1.58-native-dlaa-2026-09-23-1139/current-session.log`,
SHA-256 `2a02987ef5fb3de01a8021b3d3fc2e63ac527b60a62dbd42d22307618ac7496c`.
The existing stage-pair capture mechanism saved frames 11,421 and 11,422 under
the SKSE `RazKolbasCaptures` directory (`stage-pair-22156-11421-46129609` and
`stage-pair-22156-11422-46130437`). They provide native-path comparison material;
their contents have not yet been independently analyzed in this checkpoint.
The installed manifest's four payload hashes were reverified. No configuration
or binary changes were made during this run; the game was left running.

Together with the spatial-only reduced-route failure, this narrows the visual
defect to differences introduced by reduced rendering and its integration
with the effects/presentation path. It does not identify the precise ENB
resource or missing setup step. Native DLAA is the working baseline; reduced
SR visual correctness remains unresolved. No repeated baseline run is needed.

### Latest steering: reuse the known-good DLAA baseline

The user clarified that the earlier native-resolution DLAA image looked
correct. This removes the need to request a separate pure-native baseline
run. Before any further launch, the temporary safe-mode setting was replaced
with `SafeMode=false`, `Upscaling.Quality=NativeAA`, `ManualRenderScale=0.0`
and `SpatialBaselineOnly=false`. The existing code excludes NativeAA from
`srRequested`; owned-scene preparation therefore returns before reduced
allocation, UI substitution or renderer-rectangle activation. The previous
spatial INI remains backed up as recorded below. No DLL changed; the installed
manifest was refreshed and all four payloads verified.

This configures the existing native DLAA path; it is not a claim that the
current binary has reproduced the earlier visual result. The next technical
investigation is the difference between that previously good native path and
the owned reduced path: renderer/resource sizing, ENB input identities and
publication order. The safe-mode launch request below is superseded.

## 0.1.58 visual mismatch persists; native comparison prepared (2026-09-23)

The user ran the spatial-only build and reported that ENB appeared missing.
The current session (process 10660, started 11:32:21) confirms zero provider
submissions, successful sampled Presents and menu-route activation at frame
8792. The equal-frame admission fix worked: 49 admission records represent
49 unique world frames. However, the menu gate still revoked admission when
depth diversity fell, switching publication back to pre-Present. Thus the
previous claim of a stable same-boundary spatial comparison was incorrect.
This run does not establish the cause of the missing-effects appearance.

The log is cumulative across launches. Current-session analysis must filter by
startup timestamp; totals across the full file are not this run's results.
The 441-line filtered snapshot contains three expected startup admission
warnings, two sampled menu publications and 31 pre-Present publications.
Full log SHA-256: `ccdfe4ecea6bec065025a8ef705c6c84968c0c9c60c3256b0c762ab2f416ab2d`.
Filtered snapshot SHA-256:
`2e376d5bbf236ae3f4c2f8f49905475ce602d7e740523ae7cabf1e01dd0e642a`.
Both remain ignored under `artifacts/local/runtime-0.1.58-2026-09-23-1132/`.
ENB's on-disk `UseEffect=true` and changed native-buffer hashes across ENB
show activity, not full effect correctness or matching guide inputs.

After collecting evidence, Skyrim closed normally. The installed INI now has
`General.SafeMode=true`, which the verified startup predicate checks before
installing any RazKolbas rendering hook. This is a true native comparison:
no owned reduced-scene route, DLSS processing or RazKolbas overlay. No source
or binary change was made, and ENB/ReShade settings were left untouched.
The previous INI and manifest are backed up under ignored
`artifacts/local/native-baseline-2026-09-23/`. All installed payloads match the
updated manifest; INI SHA-256 is
`6683d20b08b285ce7794b70324950ec264543067c08d154fb23fa75daede17bb`.
The existing downloadable 0.1.58 spatial-baseline ZIP remains unchanged and
does not contain this native-comparison setting.

Next action: user starts Skyrim and loads the same save to establish whether
ordinary ENB appearance returns with all RazKolbas render hooks absent.
Do not re-enable DLSS or call visual parity fixed. Before another reduced-route
candidate, revisit publication ownership and ENB input/resource ordering;
passing resource-identity tests does not prove correct effects placement.

## 0.1.57 live depth evidence and spatial-baseline candidate (2026-09-23)

The user-run 0.1.57 session loaded a world without a RazKolbas warning, error
or Present failure, but the user reported that the image did not match the
ordinary ENB/ReShade appearance. All first twelve bounded traces observed one
exact original-depth singleton PS read at slot 3. The menu route activated at
frame 20,882 with zero DLSS submissions, demonstrating a same-boundary spatial
publication. The log also showed 392 admission entries across only 112 unique
frames: repeated menu callbacks in the same world frame were incorrectly
treated as frame rewinds, which reset readiness and toggled publication
between menu and pre-Present. The process was closed normally after evidence
collection. The ignored log snapshot and its hash are recorded in
`re/NATIVE_UI_RESOURCE_ROUTE.md`.

Source now ignores duplicate same-frame admission requests while preserving
generation and real-rewind resets. During NativeUi, only a singleton SRV whose
underlying resource exactly matches the learned original depth is replaced by
a separate display-sized sampled-depth resource; the writable native DSV and
all unrelated bindings remain distinct. A default-off
`Diagnostics.SpatialBaselineOnly` setting supports the next controlled visual
comparison with the same reduced scene and UI route but continuous spatial
publication and zero NGX submissions. Debug and Release each pass all 35 CTest
groups. The RTX 4080 SUPER replay completed 600 pooled frames with zero
fallbacks and output SHA-256
`9e24bc310a96dbeb827811488e7712457da160d242bd12370dbbdd05cdb65d37`.
Actual-game visual parity for this candidate is **NOT RUN** until the user
starts Skyrim.

Source commit `be22ce8` was packaged and installed as 0.1.58. The verified
MO2 archive is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.58-spatial-baseline-be22ce8.zip`,
SHA-256
`dbc14c24fbeae50ebe6a51b0a5721d6e76e6f892276ceb0ce91eb68926c95f3d`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest and verified every payload hash. The complete prior mod was
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.58-be22ce8` after its 0.1.57 payloads
matched the old manifest. SkyrimSE.exe was absent throughout installation.
The installed DLL SHA-256 is
`cb757f67a7da6adf790f54d8ed05c4c86cf101544dd037295fc0a57fe72f4fef`.
The installed INI intentionally enables `SpatialBaselineOnly=true` and has
SHA-256
`d84768616750ca01c66ad68a9984b8070887867c20c8a4056b88f318d9f858e5`.
The signed NVIDIA SR runtime remains byte-identical at SHA-256
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The next user-started run must compare this stable spatial image with the
ordinary ENB/ReShade appearance; this package intentionally submits no NGX.

## PureDark Consolidated Verified 10 follow-up (2026-09-23)

The user-supplied pass-10 RE bundle was independently hash-checked: all 2,034
manifest payloads are present and match, with no unlisted files. Its bundled
checker has a Windows path-separator comparison bug; this does not indicate
payload corruption. The evidence was reviewed without copying or staging DLLs,
archives, compiled probes, captures or analysis databases. Full provenance,
confirmed findings and limits are recorded in
`re/PUREDARK_CONSOLIDATED_10.md`.

Source now separates attempted, successfully evaluated and successfully
published SR frames. Evaluation failure, skipped fallback, forward frame gap and
pre-Present/menu source-phase change force temporal reset. The native-UI route
requires its own two-sample menu-boundary colour/depth admission and no longer
requires an earlier DLSS submission. The exact verified ENB
PSSetShaderResources slot is installed as a pass-through observer; it reports
singleton reads whose underlying resource is the observed original depth while
leaving bindings unchanged. A separate sampled late-depth replacement is not
enabled until that consumer is seen live. Debug and Release builds pass all 35
CTest groups. The Release RTX 4080 SUPER harness completed 600 pooled
1707x960-to-2560x1440 DLSS frames with zero fallbacks and output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`.
Independent review found one exception-containment issue in the menu probe;
the callback and generic forwarder now both preserve original menu forwarding,
with an injected-throw regression. The game trace remains pending in this
checkpoint.

Source commit `1f3799f` was packaged and installed as 0.1.57. MO2 package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.57-phase-receipt-depth-trace-1f3799f.zip`
has SHA-256
`bcb99bf295cb0abe2cb72a991f6e8026e9f1dbb8f812faff9bef73c71f87d5d0`.
Independent extraction found exactly the four manifest-listed payloads plus the
manifest and verified every payload hash. The prior installed mod matched its
0.1.56 manifest and was copied to ignored backup
`artifacts/local/mo2-install-backup-0.1.57-1f3799f`. SkyrimSE.exe was absent;
only the RazKolbas DLL and manifest were replaced. Installed DLL SHA-256 is
`4806eff823686c8f0cfaaf43cf4746831a76da07fc61e2196c9594f385324b42`.
The user INI and signed NVIDIA SR runtime remain byte-identical at SHA-256
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
Runtime source-phase admission and PS-depth trace are **NOT RUN** until the user
starts Skyrim and loads a save. The assistant did not start the game.

## Scope

The complete T01–T24 plan in `../RazKolbas_Codex_Handoff/docs/plans/2026-09-19-razkolbas-implementation.md` remains the target. This checkpoint is not a product completion claim.

Current local validation: all 15 CTest groups pass in Debug and Release. Counts below for earlier stages describe their original milestone runs. The user selected `D:/TESV_EX` for testing (their typed `D:/TESV/_EX` did not exist). Release0.1.7 DLL/INI are installed in MO2 mod `RazKolbas`, enabled in `TRUE AE V5.32 EXTENDED + OSTIM`; prior loose copies were backed up and removed from game Data. The native host, actual device observer and ENB Present observer work with ENB/ReShade active. A bounded GPU readback of three candidate menu textures succeeded in0.1.6 at23:24:39 on September19. Version0.1.7 replaces automatic startup capture with an explicit hotkey; the physical-key capture passed at00:11:48 on September20 with nonzero motion and nonuniform depth. The three requested gameplay captures are complete and verified; raw motion responds strongly in the fast-pan capture, but units/jitter/live stage remain open. The supplied NVIDIA SR DLL now successfully evaluates a captured frame in the standalone D3D11 DLAA replay (see `re/SR_REPLAY.md`). Production frame processing remains unimplemented. See `SKYRIM_SMOKE_TEST.md` and `re/FRAME_INPUTS.md`.

| Task | Implementation | Build/test | Game/GPU |
|---|---|---|---|
| T01 | Inventory and environment tools implemented; host dependency sources pinned; production vendor SDK selection open | 4 inventory regressions PASS; 57 extracted files VERIFIED; original archives MISSING_REFERENCE | NOT RUN |
| T02 | Native-only SKSE DLL, guarded lifecycle, logging, build presets, CI and explicit staging implemented | Debug DLL built; 3 CTest groups PASS, including actual DLL load/export checks | Skyrim 1.6.1170 native-host load PASS: fresh SKSE and plugin logs; user reports menu/exit |
| T03 | Shared variant-typed INI schema, requested SR policy, transactions and temporal invariants implemented; full FG pairing/resource contracts remain open | 7 Debug CTest groups PASS; invalid numbers/enums, locked file, failed replacement, reset epoch and generated frames covered | NOT RUN |
| T04 | Partial: exact-hash planner, owned executable patch/restore, atomic pointer leases and reversible working-copy file tooling | 10 Debug CTest groups PASS, including executable/concurrent fixtures and disk regressions | Game patches NOT RUN |
| T08 | Static RE, exact-hash community NR loader, caller-name shim, verified driver parameter factory and direct feature-creation probe | Debug/Release probe built; parameter ABI round trips verified | RTX 4080 SUPER: original init 0xBAD00002; patched-route init/create/release/shutdown 0x1; GPU fence completed, 4 callback allocations/releases balanced; evaluate/output NOT RUN |
| T05 | Partial: exact-hash creation observer and ENB/ReShade Present/resize/release observers; actual device identity capture | 13 Debug/Release groups PASS; WARP capture, real resize success/failure and release; offline game43, ENB30 and ReShade38 assertions PASS | Creation interception PASS with ENB/ReShade; RTX4080 SUPER, 2560×1440; 0.1.4 ENB Present PASS: 15000 calls, zero failures; menu/Alt+F4 exit observed; resize/Release-zero NOT OBSERVED; processing/resource ownership open |
| T06 | Partial: exact-layout, owned-lock, creation-anchor candidate readback; bounded synchronous CPU bundle | 14 Debug/Release groups PASS; real WARP pixel/binding/raw format tests; mismatched device/context rejected | 0.1.6 captured 2560x1440 colour/motion/depth candidates with ENB/ReShade; hashes verified; menu motion0/depth uniform, world semantics NOT RUN |
| T07, T09–T24 | Not implemented | NOT RUN | NOT RUN |

## Commands run

- `pwsh -File tests/integration/Inventory.Tests.ps1`: 4 tests failed before implementation, then 4 passed.
- `pwsh -File tools/Inventory-References.ps1 -ReferenceRoot . -Verify`: exit 1, 57 verified entries, 2 missing original archives, no hash mismatches.
- `pwsh -File tools/Inspect-Environment.ps1`: exit 0; local JSON under `artifacts/local/`.
- `pwsh -File tools/Build.ps1 -Preset win-dev`: exit 0; `unit.host`, `integration.inventory`, `integration.plugin` PASS. Three lifecycle tests failed before implementation; the DLL integration test failed before the DLL existed.
- `pwsh -File tools/Build.ps1 -Preset win-release`: T02 Release DLL and 3 CTest groups PASS.
- T03: `unit.config`, `unit.policy`, `unit.history`, `unit.frame_identity` failed before implementation and passed afterward; the supplied INI round-trips through the schema. All 7 Debug groups passed together.
- Latest: Debug and Release builds passed all 10 CTest groups. `tools/re/run_nr_probe.py` passed four separate local GPU cases: original initialization rejection, controlled post-init exception, controlled missing-release rejection, and successful direct feature creation/retirement. Fresh reference verification still reports 57 VERIFIED and 2 MISSING_REFERENCE archives.

## Decisions

- Preserve the supplied handoff directory; build implementation at the repository root. The original prompt path was absent; the available `CODEX_PROMPT.md` provides the implementation instructions.
- Initialize the unborn repository on `codex/razkolbas-bootstrap`; no worktree can be based on a nonexistent HEAD.
- Use exact source commits through CMake FetchContent and `runtime/dependencies.lock.json`, avoiding two conflicting dependency manifests. No vcpkg manifest is claimed to exist.
- Pin CommonLib 3.6.0 for C++20. The native-only bootstrap will use its SKSE interface declarations without eagerly initializing its relocation database. Engine integration remains T05.
- Supplied extracted files are sufficient for static RE; missing original archive containers do not block it.

## Next action

T04: finish production patch descriptors and engine detour/quiescence integration. T05 now has the selected Skyrim 1.6.1170 installation, recorded executable hash and successful native-host load logs; next recover/verify rendering integration for that executable. Independent T08 next probe: recover evaluation/resource/temporal contracts and evaluate a known image with fenced GPU readback. Parameter factory and feature 0x12 creation now execute successfully. See `re/NR_BOOTSTRAP.md`, including the community runtime's invalid embedded signature and the measured direct-loading compatibility route. T07 same-adapter interop remains an independent open task. No SR, FG, ENB, ReShade, ImGui or NR image output is implemented or advertised.

T05 observer checkpoint: see re/SKYRIM_HOOK_MAP.md. The controlled game run confirmed interception and actual-device capture at 21:45:38 with ENB and ReShade active; next establish frame/resize boundaries and resource lifetimes. Normal configuration defaults remain opt-out. No frame processing or vendor runtimes were added.

0.1.2: verified ReShade swap-table profile and pass-through Present/Present1/ResizeBuffers/ResizeBuffers1/Release callbacks implemented. See re/SWAPCHAIN_TRACE.md. No backbuffer or COM resources retained; no world/pre-UI/source-frame identity inferred from Present. Configuration now accepts comma-separated known disable IDs. The 0.1.2 game run rejected the returned table; no presentation hooks were applied. 0.1.3 identified the actual ENB outer table; 0.1.4 uses that exact three-method profile and passed automated menu presentation testing.

Next executable work: establish world/pre-UI colour/depth/motion-vector and resource-generation contracts for this game; the verified Present boundary alone is not suitable evidence for SR input placement. Actual game resize and retirement tests remain open. Further menu launch/close probes are authorized and can run through MO2 without asking the user to repeat them.

0.1.6 checkpoint: renderer layout correlated with exact Address Library and live Lock/Unlock code; End already patched by another mod and left alone. 0.1.5 rejected accessor/raw interface inequality; instrumented evidence proved renderer fields matched original creation outputs. 0.1.6 anchors those outputs and revalidates canonical COM identity under the already-owned renderer lock before capture. One complete56.25MiB candidate bundle passed size/hash inspection. Continued Present>=18600, failures0; menu screenshot and Alt+F4 exit observed. Next implement a bounded capture trigger for loaded-save stationary/pan tests, then establish world/pre-UI timing and guide semantics. No additional menu-only user test is needed. SR/FG/NR image evaluation remains inactive.

0.1.7 checkpoint: Ctrl+Shift+F10 capture policy and restart-scoped setting implemented; no automatic timer remains. Six requests/session, five-second cooldown, bounded expiry/retries and release/focus/presentation-gap rearming tested RED/GREEN; all15 Debug/Release groups PASS. Deployed8ad4771, loaded through MO2; menu and idle/no-automatic-capture PASS. Two automated key chords produced no request/capture, so hotkey runtime success is not claimed. Game left open for a physical-key test; then loaded-save stationary/slow-pan/fast-pan captures are needed (docs/CAPTURE_TEST.md). No SR/FG/NR output yet.

0.1.7 physical-key follow-up: user press produced request1 at00:11:48 on September20. Complete bundle18080-136142390 verified independently: all three2560x1440 files have matching hashes/row extents, finite colour and motion, nonzero motion range and1,658,290 distinct low24 depth values. Hotkey-to-readback runtime PASS; prior synthetic-input failure is not reproduced by physical input. Scene/camera conditions were not specified, so no guide direction/scale/depth convention is inferred. Next: three controlled captures in the same outdoor scene (stationary, slow pan, fast pan). Game remains open; five request slots remain as of this evidence.

## Current checkpoint — supplied DLSS runtime replay

User completed all three requested stationary/slow/fast-pan captures and exited Skyrim. All nine raw files passed hash/extent validation; comparison preview inspected. No repeated capture request is pending. See `re/FRAME_INPUTS.md`.

Responding to the user's direction to reuse working libraries: recovered PDPerf's ordinary SR parameter conversion and implemented `RazKolbasSrReplay` against pinned official NGX SDK and the supplied signed SR DLL. The runtime's real D3D11 DLAA evaluation of capture18080-136199593 passed on RTX4080 SUPER, producing finite/nonuniform output with clean retirement. Final normal replay reproduced output SHA256 facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c. Review fixes cover exception termination before GPU owners unwind, Debug CRT library selection and deferred write failures. Post-evaluation injected exception exits10 without a success claim. Both replay configurations build.

Next concrete integration step: recover the reference pre-SR input preparation and jitter/hook location, then connect the verified standard NGX path to the owned Skyrim backend. Existing Present captures cannot establish that timing. No new algorithm is needed. Installed MO2 0.1.7 and its DLL/INI are unchanged; SR/FG/NR processing remains inactive in-game. This checkpoint is offline native-resolution DLAA only, not reduced-resolution SR, temporal quality, FPS improvement or full T11 completion. See `re/SR_REPLAY.md`.

## 0.1.9 installed world-call contract (2026-09-20)

Commit `5d67588` adds an exact Skyrim 1.6.1170 world-draw `CALL` plan at RVA `0xfa507a` with independently verified executable SHA256, exact image size, five live instruction bytes, and decoded original target RVA `0xe44850`. The plan rejects changed bytes/identity/target and performs no instruction write. Debug and Release builds passed all 15 CTest groups. The managed MO2 package `RazKolbas-0.1.9-sr-callsite-contract-5d67588.zip` was installed at `D:/TESV_EX/MO2/mods/RazKolbas`, preserving the existing INI. Installed DLL SHA256 is `a8c25a89fc1b68aee145937400d64b10a6e3541bf5b081eb598a9880380fc6dc`.

Live launch through MO2 on September 20 at 08:50:03 passed the read-only call contract at 08:50:14. Renderer creation on the RTX 4080 SUPER, the exact ENB observer and more than 4,200 successful Present calls followed by 08:51:52 with zero logged Present failures. This verifies startup and observation only. A pass-through detour, frame-resource transfer, in-game NGX SR output, FG and NR are not implemented. The next implementation slice is a reversible call-site transaction with safe lifetime and verified forwarding ABI, then owned frame-resource preparation at the world boundary. Reference PDPerf and SkyrimUpscaler DLLs remain RE-only and are not shipped or loaded by this plugin.

## Offline CALL forwarding fixture (after 0.1.9)

The prepared direct-CALL plan now encodes a five-byte replacement only when its recorded original target remains consistent and the detour fits signed rel32 reach. An owned executable fixture forwards through a counting detour to the original function and verifies one visit per call, unchanged return, restoration, and both displacement limits. TDD red was a missing encoder; Debug and Release then passed all 15 CTest groups. This is offline preparation: no Skyrim instruction write, game launch, MO2 update, or runtime claim occurred in this step. The installed MO2 DLL remains 0.1.9 from commit `5d67588`. Next: establish a proven Skyrim thread-quiescence and callback/near-thunk lifetime protocol, then verify the actual forwarding ABI before any game hook activation.
A separate 14-byte RIP-indirect absolute jump relay also ran offline through two Win64 integer arguments with exact return values and visit count; it is code generation only, not a Skyrim allocation or installed hook.

## Offline near relay and world-call ABI preparation

A move-only near relay allocator now reserves a free page within signed rel32 reach of the prepared world CALL, writes the register-preserving RIP-indirect jump under read/write protection, seals execute/read, flushes and verifies it. An owned executable fixture exercised the full CALL -> near relay -> C++ target route, restored the original five bytes and retired the relay. Its initial crash was isolated to missing Win64 stack alignment/shadow space in the synthetic caller; the corrected ABI fixture passes. Exact-hash reference callback disassembly at `0x156830/0x15691d/0x156922` supports RCX pointer plus EDX 32-bit forwarding. The previously logged live continuation immediately overwrites RAX, so the return is unused at this site. A typed `WorldDrawForwarder` models original-first, exactly-once forwarding; argument semantics and real-game operation remain unverified. The game executable is encoded on disk at these RVAs, so static file disassembly cannot verify its callee. Descriptor: `patches/skyrim/world-draw.call-prep-v1.json`. Debug and Release build/test: all 15 CTest groups PASS. Game launch, game patch, and MO2 update: NOT RUN. Next: implement a startup-only ownership-aware instruction transaction and activation gate, then the user-run pass-through test. No automatic game start.

## 0.1.10 read-only live code snapshot (pending runtime)

After the exact 1.6.1170 world CALL contract is accepted, the plugin now logs bounded decoded live bytes from caller base RVA `0xfa4f00` (0x200 bytes) and original target RVA `0xe44850` (0x100 bytes). This is needed because the hash-verified on-disk executable text is encoded and cannot establish the callee ABI. It changes no game instruction and activates no vendor provider. Debug/Release builds passed all 15 CTest groups. Runtime verification is NOT RUN; 0.1.9 remains installed until managed 0.1.10 deployment. The user specifically requested no assistant-driven game launch.

## 0.1.10 managed MO2 deployment

At 09:15:05 on 2026-09-20, managed install replaced only the RazKolbas DLL in `D:/TESV_EX/MO2/mods/RazKolbas` after verifying the prior installed hash and absence of SkyrimSE.exe. Existing INI SHA256 `7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1` was preserved. New DLL SHA256 `2c76caaa56e19168a5fb0299d7a8e98bedc25f2012dced9b83b29b3a80c0f127`; MO2 download `RazKolbas-0.1.10-sr-live-code-7489ed5.zip` SHA256 `5a5ca103b958896e66f4c024ba7940665169f8089e845ea34ef784a2a7e88290`. Prior DLL/INI/meta were backed up under `artifacts/local/sr-live-code-install-2026-09-20-091505`. No game launch was performed; runtime probe remains NOT RUN until the user starts Skyrim through MO2. The new code only reads/logs bounded decoded game bytes after the exact callsite contract passes.

## 0.1.10 user-run decoded world-call evidence

User launched Skyrim through MO2 on 2026-09-20. At 09:16:38 the plugin verified the exact world CALL and logged a 512-byte decoded caller and 256-byte decoded original target. Caller SHA256 d0fd5a13877dbf1b79ca1822c6bf5119b87e1127c72d97eb146e52172c08a3f2; target SHA256 24082619f4ccf5d4a7ddf280ce35718e891c73a57a61e20210db9cc9e22ec54d. Live disassembly proves EDX=0, RCX=&game object at RVA 0x32887c0, target reads DL and retains RCX, and caller overwrites RAX. ENB-wrapped D3D11 renderer creation succeeded at 09:16:54 on RTX 4080 SUPER; menu Present observation continued without failure. This is a PASS for read-only decoded-code capture and renderer observation only. In-game relay, world-draw forwarding and SR processing remain NOT RUN. Assistant did not launch or control the game. Raw bytes/disassembly stay ignored under artifacts/local/sr-live-code-2026-09-20-091638; interpretation is in docs/re/SR_REPLAY.md.

## 0.1.11 experimental world-call pass-through, pending game run

Exact 0.1.10 decoded caller/target bytes are now a fail-closed activation gate. A new bounded CALL write requires the exact executable identity, original instruction, caller/target ABI, sealed near relay, executable page and SKSEPlugin_Load startup boundary. It rechecks live bytes under temporary read/write protection, restores prior protection, flushes and reads back. A changed owner causes zero writes. The 0.1.11 experimental opt-in callback forwards to the original game target exactly once, then counts calls for the normal Present log (`worldForwarded`); it performs no DLSS work. Callback DLL, relay and forwarding state are retained for process lifetime. The owned executable fixture covers install, execution, idempotence, changed-owner refusal, restore and relay lifetime. Debug and Release: all 15 CTest groups PASS. In-game forwarding: NOT RUN. No game start was performed by the assistant.

## 0.1.11 managed MO2 pass-through deployment

At 09:35:14 on 2026-09-20, after confirming SkyrimSE.exe was absent, exact prior DLL/INI hashes were checked and backed up under artifacts/local/world-pass-through-install-2026-09-20-093514. Installed only RazKolbas 0.1.11 DLL plus matching install manifest in D:/TESV_EX/MO2/mods/RazKolbas; the INI remained SHA256 7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1. New DLL SHA256 4f1951e8d530bff1cf1a767e7e096b407d384db35db2e4af7df77546cf37495b. MO2 package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.11-world-pass-through-1ba6ceb.zip SHA256 dcb19cfd167257ed30c60eeb44e21bdcf57c27cf5dd2df3c111b76dcee9b6309. The user must launch Skyrim; in-game pass-through and forwarded-call counter remain NOT RUN. The assistant did not start the game.

## 0.1.11 user-run world forwarding PASS

The user launched Skyrim through MO2 at 09:36:08 on 2026-09-20. At 09:36:16 the decoded caller/target ABI gate passed and the exact world CALL relay installed. ENB-wrapped renderer creation succeeded at 09:36:33. Present observations 1, 2, 3, 600 and 1200 each reported the same worldForwarded count; more than 31800 Presents completed with failed=0 while the process remained responsive. This validates original-first game pass-through at the main menu, not DLSS, gameplay resource semantics or visual equivalence. A bounded external read-only snapshot confirmed the renderer object passed as RCX maps to AE ID411393 at RVA 0x32887c0 and its candidate fields remained populated; raw snapshot stays ignored at artifacts/local/world-pass-through-live-2026-09-20-0938. The assistant did not launch or control Skyrim. Next: establish resource state exactly before/after this world boundary, then connect owned NGX input/output preparation.

## 0.1.12 sparse world-boundary numeric telemetry

The next diagnostic build samples only the verified renderer object's numeric fields immediately before and after the original world target on the first three calls and every 600th call. It logs argument flags, callback thread, renderer pointer match, lock owner/recursion, creation device/context/swap and candidate colour/motion/depth pointers. It does not dereference candidates, acquire the lock, call D3D, copy GPU data or alter rendering. Debug and Release: all 15 CTest groups PASS. In-game 0.1.12 telemetry: NOT RUN. The existing 0.1.11 user run remains the validated pass-through baseline.

## 0.1.12 managed MO2 world-stage deployment

At 09:45:39 on 2026-09-20, after confirming SkyrimSE.exe was absent, exact prior DLL/INI hashes were checked and backed up under artifacts/local/world-stage-install-2026-09-20-094539. Installed only RazKolbas 0.1.12 DLL plus matching install manifest in D:/TESV_EX/MO2/mods/RazKolbas; INI remained SHA256 7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1. New DLL SHA256 e32c178c2a43e007052edcbe2b0eb39b8fbcec7f9d654aa351b245a8ebb6f2d6. MO2 package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.12-world-stage-9f1d3b1.zip SHA256 663063d81be94788d3f153ddc9f04ff2fbf235b7a5715ffac702a2549b2f21dd. The user must launch Skyrim and load a save for world-stage telemetry; game outcome remains NOT RUN. The assistant did not start the game.

## 0.1.12 user-run world-stage snapshot PASS

The user launched 0.1.12 through MO2 at 09:46:25 on 2026-09-20 and reported loading a save. The log does not independently identify the scene. Across 29 sampled world-stage calls through #15600, every before/after renderer read succeeded, RCX matched AE ID411393's renderer object, the callback thread owned its lock with recursion 1 before and after the original target, and the nonzero candidate colour/motion/depth pointers stayed stable around that call. Thread IDs changed from 6588 to 4764 on sampled frames, with lock ownership following the current thread. All 29 corresponding Present/worldForwarded counts matched; failed=0 and the process was responsive. This is a PASS for guarded numeric access at the post-world boundary only. Candidate image semantics, depth conversion, jitter, frame history and in-game NGX evaluation remain NOT RUN. The assistant did not launch or control Skyrim.

## 0.1.13 owned SR input copy, pending game run

The supplied signed NVIDIA SR runtime accepted the captured Skyrim R24G8_TYPELESS depth texture directly in an isolated reset-frame DLAA replay. Init/create/evaluate/release/shutdown returned success; independent validation found finite nonuniform output SHA256 facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c, identical to the prior normalized-R32 experiment. That is format acceptance, not proof of correct depth semantics or temporal quality. Reproducible mode RAZKOLBAS_SR_REPLAY_DEPTH_MODE=typeless; ignored results under artifacts/local/sr-depth-typeless-validated-2026-09-20.

A new move-only PreparedSrInputs owns same-device colour/motion/native-typeless-depth copies and a distinct float output texture. Exact texture/device checks precede allocation and all GPU copies. WARP verified byte-identical colour/motion/depth copies and rejected missing, swapped and foreign-device sources. The 0.1.13 game callback, after original world draw under the observed renderer lock, queues the owned copies once, places an event query and retains them until GPU completion or process exit on uncertain failure. No game source texture or presentation output is written; NGX remains inactive in Skyrim. Debug and Release: all 16 CTest groups PASS. In-game copy result: NOT RUN. The assistant did not start Skyrim.

## 0.1.13 managed MO2 owned-input deployment

At 10:00:35 on 2026-09-20, after confirming SkyrimSE.exe was absent, exact prior DLL/INI hashes were checked and backed up under artifacts/local/owned-sr-inputs-install-2026-09-20-100035. Installed RazKolbas 0.1.13 DLL and matching manifest in D:/TESV_EX/MO2/mods/RazKolbas; INI remained SHA256 7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1. New DLL SHA256 9b67fa1de2772bb308e7c7d9076085d6def271f8319b12713e6f7c7a3d55d0e8. MO2 package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.13-owned-sr-inputs-86850e3.zip SHA256 ac8771c759452704238fb16e7cf19994821fee44259152ee92ce6f738737e9df. Game copy result remains NOT RUN until the user launches Skyrim; assistant did not start it.

## 0.1.13 user-run owned-input GPU copy PASS

The user launched Skyrim through MO2 at 10:01:54 on 2026-09-20. The installed 0.1.13 DLL hash matched the managed package. At 10:02:24 the guarded callback queued owned 2560x1440 colour, motion and native typeless-depth copies on the game RTX 4080 SUPER D3D11 device. The subsequent D3D11 event query completed and the device remained healthy. Through Present #18000, worldForwarded matched Present, failed=0, and the process was responsive; the 98 log lines for this launch contained no warning or error. This is runtime PASS for in-game GPU copy completion only. In-game copied-pixel equality, NGX evaluation, temporal quality and displayed SR remain NOT RUN. The assistant did not launch or control Skyrim.

## 0.1.14 guarded offscreen game-device DLAA probe, pending user run

The next experimental slice keeps original-first world forwarding and the 0.1.13 owned source copies. On a completed copy fence, it checks the supplied signed NVIDIA SR runtime exact size/SHA256, isolates the D3D11 context state, initializes NGX on the game device, creates DLAA, evaluates once into an owned output, fences staging readback, requires finite/nonuniform RGB, records output SHA256, and then retires the feature/parameters/runtime on confirmed completion. Uncertain failures retain resources until process exit. No game/ENB/ReShade display target is written. The runtime comes from SkyrimUpscalerAIOBuild16-Hotfix1/UpscalerBasePlugin/nvngx_dlss.dll, not PDPerfPlugin.dll, SkyrimUpscaler.dll, or the unsigned community NR runtime. The reset/jitter/MV scale/exposure inputs remain explicit unverified diagnostic assumptions. WARP checked graphics-state restoration after injected work. Debug and Release builds passed all 17 CTest groups with the pinned NGX SDK. In-game NGX evaluation, output quality and displayed SR are NOT RUN until the user launches the new managed package. The assistant did not start Skyrim.

## 0.1.14 managed MO2 offscreen-DLAA deployment

At about 10:20 on 2026-09-20, after confirming SkyrimSE.exe was absent and the installed 0.1.13 DLL/INI hashes matched their prior receipts, the existing DLL/INI/manifest/meta were backed up under artifacts/local/offscreen-dlss-install-b5bf152. Installed 0.1.14 DLL SHA256 a4137ebd1a16cd357ce669642c59d4ed04ca5a5b8b3d9ce61f95d94f84967ff4 and signed NVIDIA SR runtime SHA256 c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e under D:/TESV_EX/MO2/mods/RazKolbas/SKSE/Plugins/RazKolbasRuntime/. Existing INI remained SHA256 7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1 with versioned/experimental patches enabled. MO2 package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.14-offscreen-dlss-b5bf152.zip SHA256 83482df8c105666756fe1aa1666527fead8effa218873974ddd3a9c7d965e2ea; every ZIP entry matched the manifest. The plugin import table has no vendor/reference host import. In-game offscreen NGX probe remains NOT RUN until the user launches Skyrim. The assistant did not start the game.

## 0.1.14 user-run offscreen NGX output failure

The user launched Skyrim through MO2 at 10:22:29 on 2026-09-20. At 10:23:00 the plugin queued three owned inputs on the RTX 4080 SUPER game device; at 10:23:01 the signed NGX runtime accepted a reset-frame DLAA offscreen evaluation. The completion/readback path rejected its output as nonfinite or uniform; the combined check cannot yet tell whether the NaN sentinel was unwritten or whether an image was valid but uniform. No game display write occurred. The game remained responsive with Present/worldForwarded equal through #10800 and failed=0; one warning was logged. Live NGX output validation FAILED; displayed SR and temporal quality remain NOT RUN. An isolated replay of an older main-menu capture using the same runtime/native typeless depth produced all 11059200 finite RGB components and 7194 distinct half-float values, so a menu frame alone is not an adequate explanation. Next: isolate context-state/copy behavior and separate diagnostic counts before asking for another game launch. The assistant did not start or control Skyrim.

## 0.1.15 exact-path NGX isolation and scene-readiness diagnostic

A new isolated RazKolbasSrLivePathReplay exercises the same owned copy and offscreen NGX module as the plugin, using a pinned NVIDIA runtime under a sibling RazKolbasRuntime directory. On the archived menu capture it passed with output SHA256 9e0929f3835a3b9c890e880089d4e817a7a0de1adc1b03eb23fd38aef4a13ce8, identical to the independent menu replay. On the stationary world capture it passed with output SHA256 facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c, identical to the independent world replay. Both retired NGX. This isolates the 0.1.14 live failure to game frame contents, hook timing or other in-process state, not a known standalone owned-copy/state-switch failure. The exact cause remains unknown.

The next game diagnostic samples owned typeless depth at a 10x10 CPU readback after its copy fence, retries no faster than every 600 world calls (24 attempts maximum) until at least 16 distinct and 16 non-far samples appear, then hashes all three owned inputs before one NGX evaluation. Archived menu/world captures measured 1 versus 98/95/89 distinct samples. An invalid output now logs finite/varying/zero/NaN-sentinel counts and SHA256. The depth/readback operations may stall a diagnostic frame. In-game 0.1.15 result: NOT RUN; no user game launch was requested yet. The assistant has not started or controlled Skyrim.

0.1.15 build validation: pinned-SDK Debug and Release builds and all 17 CTest groups passed. The new standalone live-path replay returned the exact prior independent output hashes on both archived menu and stationary world frames. The 0.1.15 game run remains NOT RUN.

## 0.1.15 managed MO2 world-gated diagnostic deployment

At about 10:35 on 2026-09-20, after confirming SkyrimSE.exe was absent and exact 0.1.14 DLL/INI/runtime hashes matched, the prior DLL/INI/manifest were backed up under artifacts/local/offscreen-dlss-worldgate-install-e49f021. Installed 0.1.15 DLL SHA256 8e4a5d50f5593c9f669b7cec8fc17019ac285b0a7fc4f810aaf3d60b7ecfd782 and its manifest in D:/TESV_EX/MO2/mods/RazKolbas. The signed NVIDIA runtime stayed SHA256 c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e; the user INI stayed SHA256 7de35e2d1e60a0bdc1e06517cec25e9b50a34af7ee47f3583035a7a88535fea1. MO2 ZIP D:/TESV_EX/MO2/downloads/RazKolbas-0.1.15-world-gated-dlaa-e49f021.zip SHA256 e927eace4ec4ad96a53f5c6a32109201da7d46129773bf29a3ffef2c6212e3e6; all three packaged files matched its manifest. In-game output validation remains NOT RUN pending the user launch. The assistant did not start Skyrim.

## 0.1.15 user-run offscreen DLAA PASS

The user launched 0.1.15 through MO2 at 10:36:57 on 2026-09-20. The guard observed menu-like depth through attempt 18, then accepted attempt 23 at 10:39:57 with 99 distinct and 98 non-far low24 depth samples. Owned input SHA256 values: colour 452f5fec83499de7d018914faacd10de71349039d8ddcbb94ab6663096fe6a50; motion b8898cc54d0f3854cac67373608176e36cc3f2ba58588227ada4adb8e12b38b2; native typeless depth aa72ead44e11dcfb6b40578e02cc5acf551ed6b445ef8c99b5c266a2e6b0946c. NGX accepted a reset-frame DLAA evaluation on the Skyrim RTX 4080 SUPER device. The output fence completed, readback had finite nonuniform RGB, output SHA256 1efc5d77fc9eadd9e805e6203fe3e00a2ceaba6f6b7e1a093b0a64c88298fd72, and release/destroy/shutdown succeeded. Present/worldForwarded matched through at least #14400 with failed=0; no warning/error in this launch slice, and the process was responsive. PASS for one offscreen NGX DLAA frame on actual game device. Displayed SR, temporal stability, guide semantics, image quality and performance remain NOT RUN. The assistant did not start or control Skyrim.

## 0.1.16 read-only world/presentation target map

Static RE of exact-hash SkyrimUpscaler.dll corrected the interpretation of the fourth CopyResource at RVA 0x1f5092: destination is private host texture +0x4e8 and source is caller +0x140, the same direction as the first copy at 0x1f4d16. This does not identify a game display copyback. See docs/re/SR_REPLAY.md. The 0.1.16 source logs canonical D3D11 OM RTV/DSV and swap-backbuffer identities at one accepted post-world frame and one following pre-ENB-Present boundary, without writing game/display resources. Debug and Release builds and all 17 CTest groups PASS. In-game target-map result NOT RUN pending user launch; the assistant will not launch Skyrim.

## 0.1.16 MO2 target-map diagnostic deployment

Commit aaaa89b, release RazKolbas.dll SHA256 b50a48b27c886c19bb50aaf72da4a69774cf896f755f734fefc49077234c360f. After verifying SkyrimSE.exe was absent and installed 0.1.15 DLL/INI/NVIDIA runtime hashes matched the recorded values, the prior mod was copied to ignored artifacts/local/target-map-install-backup-aaaa89b. The 0.1.16 DLL and manifest were installed in D:/TESV_EX/MO2/mods/RazKolbas; user INI, signed NVIDIA SR runtime, and MO2 meta.ini hashes remained unchanged. ZIP D:/TESV_EX/MO2/downloads/RazKolbas-0.1.16-target-map-aaaa89b.zip SHA256 8267091fad4a13aff01b66194ed38ab75f69e8d5a4b00714a804bdae995bca0d. All three payload entry hashes and installed hashes matched the manifest; ZIP contains only manifest plus the three payload files. Runtime target identity trace NOT RUN until the user launches Skyrim through MO2 and reaches a loaded world. No game launch was performed by the assistant.

## 0.1.16 user-run target-map gate miss and 0.1.17 source checkpoint

The user-run 0.1.16 Skyrim process started 10:51:27 and the plugin bootstrapped 10:51:36 on 2026-09-20. All 24 owned depth attempts sampled one far-plane value; the gate exhausted at 10:54:17. The target-map callback was therefore never armed and NGX was not evaluated. Present/worldForwarded matched past 28200 with failed=0; process responsive at last read. The log does not determine whether gameplay was visible. Runtime target map NOT RUN. 0.1.17 decouples one read-only world/Present target map from depth readiness at >=600 forwarded calls, while retaining the guarded DLAA attempt and optional second world-like map. Debug and Release builds and all 17 CTest groups PASS. In-game 0.1.17 target map NOT RUN until user launch. No assistant game control.

## 0.1.17 MO2 independent target-map diagnostic deployment

Commit d998418, Release RazKolbas.dll SHA256 e714ed8ac4bb185350627020f1c495cdf49667681aeff0d11d2b64f1ca0ad22a. SkyrimSE.exe was absent before and after installation. Installed 0.1.16 DLL, user INI and signed NVIDIA SR runtime hashes matched their recorded identities. Prior mod backed up under ignored artifacts/local/target-map-independent-install-backup-d998418. Installed 0.1.17 DLL and manifest to D:/TESV_EX/MO2/mods/RazKolbas; INI, runtime and MO2 meta.ini remained hash-identical. ZIP D:/TESV_EX/MO2/downloads/RazKolbas-0.1.17-target-map-d998418.zip SHA256 40d3fd58b911cf79aa8e931415f3ab4841e80333f7e44b079dc189493fda8758. All three installed and ZIP payload hashes matched the manifest; ZIP contains only those three files and manifest. Runtime independent target map NOT RUN until user launches Skyrim through MO2. The assistant did not launch or control the game.

## User-supplied DynamicShaderFrameGen source reference

On 2026-09-20, inspected https://github.com/jatelop8/DynamicShaderFrameGen at commit daaba8aadb2dbc8c5e52b028f12475c3450b6866, cloned only under ignored artifacts/local. Static source findings and provenance are in docs/re/DYNAMIC_SHADER_FRAMEGEN.md. Its D3D11 shared-backbuffer/D3D12 presentation proxy, render-time DRS/jitter hooks, and reported failed pre-UI kFRAMEBUFFER path are candidate design evidence, not RazKolbas runtime validation. Current NR relies on external ReShade add-ons and does not meet our addon-free NR requirement. No reference source, binary or capture was copied into the product or staged.

## 0.1.17 user-run resource map and next exact-code trace

The user-run Skyrim process PID 2220 bootstrapped RazKolbas 0.1.17 at 11:02:56 on 2026-09-20. At 11:03:28, the read-only post-world map identified RTV0 as the 2560x1440 RGBA16F world-colour candidate, distinct from the 2560x1440 RGBA8 swap backbuffer. At the following pre-ENB-Present snapshot RTV0 was that backbuffer. The DSV identity was unchanged. This is a runtime PASS for the one-shot resource-identity map, not for displayed SR or UI/ENB order. All 24 depth attempts sampled uniform far-plane depth; NGX evaluation was NOT RUN in this launch. Present/world-forwarded counters matched through 33,000 with failed=0 at the last log. No display write or game control by the assistant. Exact values and limitations are in docs/re/SR_REPLAY.md.

A bounded, read-only, exact-executable-hash process snapshot captured decoded renderer code and data under ignored artifacts/local/target-map-live-2026-09-20-1108. The first renderer target slot was null; the next and index-7 texture pointers matched the logged colour and motion raw pointers. The user-supplied DynamicShaderFrameGen source reports an unusable null kFRAMEBUFFER route, but its UI-entry RVA comment conflicts with our decoded Address Library mapping. Updated tools/re/inspect_live_renderer.py to collect the disputed entry, larger world caller, and next call target in a subsequent user-started process. This script change is syntax-checked only; new code regions and a loaded-world 0.1.17 map are NOT RUN until the user starts Skyrim. Current MO2 0.1.17 payload remains installed and unchanged. Next: capture and disassemble the live code, then establish the pre-UI world-to-backbuffer transition before any display write.

## Exact Skyrim reference output-path RE (static, 2026-09-20)

Read-only, hash-verified disassembly of the supplied SkyrimUpscaler.dll found the previously missing post-EvaluateUpscaler shader-draw destination. The installer resolves AE Address Library ID 82084 +0x17a to the world-draw CALL; the callback obtains its own swap-chain proxy's indexed target wrapper and passes it to SR evaluation. After the vendor call, the wrapper obtains that target's RTV and draws through D3D11 OMSetRenderTargets/Draw. The proxy's GetBuffer/Present methods conditionally expose/copy proxy buffers and forward Present to the real chain. Exact RVAs, evidence and branch limits are in docs/re/SKYRIM_REFERENCE_OUTPUT_PATH.md. This corrects the earlier inference that the fourth CopyResource might be the output copy; it is an input copy. The reference's output target belongs to its proxy, which RazKolbas does not use or ship. This is STATIC_OBSERVED, not a RazKolbas runtime/display test. No game launch, package change or installed DLL change occurred. Next implementation task remains an owned display-sized target/conversion and verified pre-UI placement with native fallback; no further identical menu-only probe is justified.

## Owned HDR fallback and retained DLAA output (offline checkpoint, 2026-09-20)

Added a D3D11 RGBA16F spatial fallback producer. It validates the immediate context, same-device source, single-sample HDR geometry and display extent; allocates an owned display-sized HDR target; copies equal extents or draws a bilinear fullscreen triangle for reduced-resolution input. An event query and retained source/output references keep asynchronous GPU ownership explicit. D3D11.1 state isolation restores the caller's bindings. This is a linear-HDR intermediate, not a backbuffer colour transform or a presentation write. The successful one-shot NGX probe now transfers its validated output out of PreparedSrInputs before retiring colour/motion/depth copies, preserving the actual produced texture for a later presentation stage.

The fallback test first failed with the explicit unimplemented result; the output-retention test first failed with a null transferred texture. Debug and Release `powershell -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset win-dev` / `-Preset win-release` then built and passed all 18 CTest groups. WARP readback confirmed a 2x2 HDR source became a 4x4 target with expected corner and bilinear interior half-float values; the test also checked caller RTV/viewport restoration and foreign-device rejection. In-game fallback, retained NGX output in the changed build, output colour conversion and visible presentation are NOT RUN. The MO2 install remains 0.1.17 and unchanged. No game was started or controlled by the assistant.

The exact SkyrimSE.exe on disk is encoded at the relevant RVAs, so remaining world-to-backbuffer/UI ordering needs the bounded read-only live-code capture already prepared in tools/re/inspect_live_renderer.py during a user-started, loaded-world session. Do not repeat an unchanged main-menu-only target-map run. After that capture, select and test the owned presentation target and conversion before a display write.

## 0.1.17 loaded-world live trace and 0.1.18 same-frame stage-pair source checkpoint

The user-started 0.1.17 Skyrim process PID 25160 had exact executable hash c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9. A bounded read-only live snapshot recovered decoded code at AE ID 82084 and the immediately following indirect dispatch; details and limits are in re/LIVE_STAGE_TRACE.md. The post-world callback after loading the save had RTV0 on the 2560x1440 format-28 backbuffer, distinct from format-10 HDR scene colour; RTV1 was format34. World-like depth appeared on attempt13 and the signed NVIDIA SR runtime produced a fenced finite/nonuniform offscreen DLAA result SHA256 864429cab53aa48d7bad66b79b538cbd0996da7ea718cd2aff1ec333cc715bd5. The installed 0.1.17 version discarded this output and did not write any display pixels. Present/world-forwarded counts matched through15000 with failed0. The process exited without assistant control. This is a second actual-device offscreen DLAA success, not a visible upscaler result.

Exact-hash static extraction found eight embedded DXBC shaders in the supplied SkyrimUpscaler.dll. Their host draw shaders sample/copy inputs, with no tone-map pass observed; their dynamic mode selection is unresolved. Captured game HDR colour contains values above1, so direct RGBA16F-to-RGBA8 sampling at post-world would clip highlights. No display write was added on that inference.

0.1.18 source adds one-shot same-frame diagnostic readback: current HDR scene plus post-world RGBA8 backbuffer, then the same backbuffer before ENB Present if the forwarded world-frame count and COM identity still match. It validates formats/extents and a64MiB total CPU budget, saves three raw files with hashes and an atomic completed manifest in the user's existing RazKolbasCaptures directory, and leaves display pixels untouched. The new WARP integration test first failed on the unimplemented producer, then passed readback, same-frame change and manifest verification. Debug and Release Build.ps1 both built and passed all19 CTest groups. The changed plugin's in-game stage pair is NOT RUN. Next: deploy the exact Release build in the isolated MO2 mod, then a user-started loaded-world run to obtain this single paired capture; use its pixels and the decoded callback chain to choose a correct owned presentation point.

## 0.1.18 MO2 stage-pair deployment

Source commits ccdd13a and 0c2ab5d. The first Stage-MO2.ps1 invocation under Windows PowerShell 5.1 found that .NET Framework lacks IO.Path.GetRelativePath; the script now computes a checked destination-relative path and staging succeeded. The failed partial stage is ignored and was not installed. Release DLL SHA256 41dd85b2480d4aeee11baeac42eb3e7c250cfe8e46fa4d0e413124c5f56a060d. Package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.18-stage-pair-0c2ab5d.zip SHA256 8ba68f540f5fe943f76335b75f44877f45f146016d940adc987e0305c996b593; all ZIP entries matched the staged manifest and contained only the plugin DLL, existing user INI, signed NVIDIA SR runtime and manifest. SkyrimSE.exe was absent before and during installation. Prior MO2 mod was backed up under ignored artifacts/local/mo2-install-backup-0.1.18-0c2ab5d with original manifest hashes verified. The installed plugin DLL and manifest were replaced in D:/TESV_EX/MO2/mods/RazKolbas; installed DLL/INI/runtime hashes match the package, while INI, NVIDIA runtime and meta.ini remained byte-identical to the prior mod. No reference host DLL, PDB or capture was packaged. In-game 0.1.18 stage-pair result: NOT RUN until the user next starts Skyrim and loads a save. The assistant did not start or control the game.

## 0.1.18 actual-device same-frame capture

The user-launched SkyrimSE.exe PID 23872 was already running at 12:18:09 when asked to start Skyrim, so no second game process was launched. The installed 0.1.18 plugin saved a complete three-image stage pair at world frame 6613 at 12:20:30. Independent SHA256 rehash matched the manifest; 173413/3686400 pixels (4.704%) changed between post-world and pre-ENB-Present RGBA8 backbuffers, concentrated at upper edges and lower centre, with unchanged alpha. The corresponding HDR RGBA16F scene was finite and RGB exceeded 1 in 3.266% of pixels. One-shot signed-NVIDIA DLAA output was fenced, finite and nonuniform (SHA256 10791db599dae89c0a31a5e8c3ac0d8c156fd3172021c4cff964f70135b93a87), still offscreen. Stage pair actual-device PASS; visible SR/FG/NR NOT RUN. Detailed measured limits are in docs/re/LIVE_STAGE_TRACE.md. The assistant neither launched a duplicate nor exited the running game.

## 0.1.19 post-world pipeline boundary diagnostic

Visual inspection of the user-run 0.1.18 pair showed a UI-free post-world scene and later HUD/dialogue/debug overlays in the same-frame pre-ENB-Present image. A 120000-pixel sample had HDR/SDR channel correlations about 0.73/0.81/0.82, so direct HDR-to-RGBA8 copy or an assumed global tone curve is not an established display path. The source now records bound PS/VS identity, topology, viewport count and up to 16 PS texture slots at the one-shot accepted post-world frame, including whether any slot aliases the HDR scene. It is read-only; visible SR remains NOT RUN. Details in docs/re/LIVE_STAGE_TRACE.md.

The pipeline boundary WARP integration test first failed because the implementation header was absent, then passed 19 assertions after implementation; a test-only shader compilation issue was rooted in pointer-size truncation and fixed. Debug and Release Build.ps1 builds both passed all 20 CTest groups. Source commit dec628b. Release DLL SHA256 428bdd3f7e6f4064530218823f92400a4a22eba77904c0e9d343ab22a3a5fb9c. Package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.19-pipeline-boundary-dec628b.zip SHA256 22f8a497aca96e2bfb393e3e4ae452250d7c2edc1288ced7b98e61f8c2b6475d; its four entries and payload hashes were verified. With SkyrimSE.exe absent, the prior MO2 DLL and manifest were backed up under ignored artifacts/local/mo2-install-backup-0.1.19-dec628b after original manifest verification. Installed the new DLL and manifest in D:/TESV_EX/MO2/mods/RazKolbas; installed DLL/INI/runtime hashes match the new manifest, and user INI, signed NVIDIA runtime and MO2 meta.ini remained unchanged. Actual-game pipeline snapshot NOT RUN until a user-started game session reaches a loaded world. The assistant did not start or control the game this turn.

## 0.1.20 continuous SDR DLAA deployment

User requested continuous evaluation instead of a one-frame result. Source commit 7b46b1b adds a persistent signed-NVIDIA SDR DLAA session after the verified world callback, with three owned fenced input/output slots, RGBA8 copyback to RTV0 before UI, native-frame fallback for busy slots or errors, and a first-frame post-world/pre-Present capture. This is full-resolution DLAA; reduced-resolution SR, FG, NR and in-game visual quality remain unverified. The current live NGX session persists for process lifetime; standalone harness drains fences and shuts it down. The source also corrects the stale bootstrap version text. The approach and limitations are detailed in re/LIVE_STAGE_TRACE.md.

WARP SDR input and display-copy tests passed. Debug and Release Build.ps1 passed all 21 CTest groups; Release was rebuilt after the final log wording edit and again passed 21/21. The standalone real-NVIDIA 30-frame replay using the prior UI-free captured SDR scene and synthetic guides produced a changed output, SHA256 841ce5d5d2138dd76acf6ede6a64f9d2b2f032e0fb0c43413839644cea274949, and exited cleanly. This is not a real-game continuous result.

Release DLL SHA256 f1bf879506ed6fede2774adad74e873c2baab60da34183fc378026316145c484. The MO2 ZIP is D:/TESV_EX/MO2/downloads/RazKolbas-0.1.20-continuous-sdr-7b46b1b.zip, SHA256 8df4c50c51bb178bfe8cdcc02b3bb88465e901277845c5d54bf4eecba6073bb3. All four ZIP entries matched the stage files and manifest hashes. After verifying SkyrimSE.exe was absent and the previous install matched its manifest, the prior DLL and manifest were backed up under ignored artifacts/local/mo2-install-backup-0.1.20-7b46b1b. Only the plugin DLL and manifest were replaced in D:/TESV_EX/MO2/mods/RazKolbas. Installed DLL, user INI and signed NVIDIA runtime match the new manifest; INI, runtime and meta.ini remained byte-identical to the prior installation. The patched unsigned DLSS-NR reference, PureDark host DLL, PDBs and captures were not packaged. Actual-game 0.1.20 startup, continuous DLAA submissions, final displayed pixels, ENB/ReShade interaction and stability: NOT RUN pending user launch. The assistant did not start or close the game.

## 0.1.20 user-run continuous SDR DLAA result

The user-launched SkyrimSE.exe PID 26724 started at 12:59:23 on 2026-09-20 with the installed 0.1.20 DLL. At 13:01:39 world-like depth passed attempt12; the offscreen HDR probe completed and released NGX. Continuous SDR DLAA began at world frame6614 at 13:01:41. The log reached13,800 submitted frames with skipped=0; world-forwarded and Present counts matched through20,400 with Present failed=0. No continuous-path disable or error was logged in this interval, and the process remained running during inspection. This is an actual-game PASS for sustained submission/copyback at the selected pre-UI boundary, not a proof of final ReShade output or temporal quality.

Complete native diagnostic frame6613 and post-copyback DLAA frame6614 stage pairs were independently SHA256 rehashed against their manifests. The DLAA post-world capture is a coherent UI-free scene; its same-frame pre-ENB-Present capture includes HUD/dialogue. Between these stages143,936 pixels changed, with100,739 changing by more than10 RGB levels in at least one channel. Comparable native counts are143,592 and100,733; the >10 masks have IoU0.893. The captures support UI composition after copyback. The generic capture manifest still says no display write, which is inaccurate for frame6614; callback log/capture ordering establish the actual mode. The user reported that TAA flickering seemed to be disappearing. That report is qualitative; Skyrim TAA was not disabled, and motion/jitter correctness and moving-scene quality remain unverified. Details in re/LIVE_STAGE_TRACE.md. The assistant did not start or close Skyrim.
Final process log extended this run to 19,800 SDR DLAA submissions, skipped=0, and equal world/Present counts through26,400 with failed=0. The process later exited without a new crash log; exit mechanism not observed. A source-only manifest wording correction removes the unconditional no-display-write label; Release rebuild passed all21 CTest groups. This corrected DLL is not installed, so the currently installed 0.1.20 binary remains the one validated above.

## Reference jitter contract and next live-code capture preparation

Hash-verified read-only static RE of the supplied SkyrimUpscaler.dll and PDPerfPlugin.dll recovered the complete callback-to-NGX jitter path. The reference advances a provider-phased base-2/base-3 radical-inverse sample, applies (-2*jx/width,+2*jy/height) to its game projection fields, stores (-jx,-jy) for the NGX payload, and forwards positive dimension-derived motion scales. It also resolves AE IDs 77245, 77518 and 77520 for version-specific camera/jitter hooks. Exact RVAs, sign constants, payload offsets, source hashes and limits are in re/JITTER_CONTRACT.md. The current installed RazKolbas still sends zero NGX jitter and leaves Skyrim TAA active; no unverified hook was added.

The existing bounded read-only exact-game-hash inspector now includes the two missing decoded jitter/camera function regions containing the reference candidate sites. Python syntax checks passed. The next live-code read requires a user-started Skyrim process because this executable's on-disk code is encoded. No new DLL was built or installed for this RE step; the already tested MO2 0.1.20 installation remains unchanged. Decoded jitter-site ABI/ownership verification: NOT RUN until the game is started. No reference DLL or binary capture was staged.

## 2026-09-20 user-started live jitter-site inspection

The user started D:/TESV_EX/SkyrimSE.exe (PID 28592, SHA256 c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9). Bounded read-only memory inspection captured decoded code while the game was at the main menu; the assistant neither started, patched nor closed the game. Capture files remain ignored under artifacts/local/jitter-live-2026-09-20-1316 and artifacts/local/jitter-live-steam-2026-09-20-1316. Hash-verified Address Library ID mapping, installer disassembly and live bytes are reconciled in re/JITTER_CONTRACT.md.

The reference installer uses version-selected relocation pairs 75709/75711 and 77518/77520. In the user's executable, Renderer Begin +0xe2 is a valid CALL into the second pair's jitter function, and second-pair update +0x11 is aligned game TAA/jitter code. However, the reference's second-pair camera patch target 77518+0x1d5 resolves to game RVA 0xe58be5, inside an instruction. The adjacent 77520+0x1d5 location is an aligned source of copied camera bytes, not that target. The prior static note had incorrectly assigned +0x7a1/+0x7a4 and helper 0x1592c0 to jitter; that mapping is corrected. Direct reuse of those reference patch offsets is blocked. No new RazKolbas DLL was built or installed; installed 0.1.20 and its user INI remain unchanged. A compatible camera hook ABI and same-frame jitter ownership are still NOT VERIFIED; no visual/runtime jitter test was performed.
The source-only FrameProbeRuntime read-only site labels were corrected to log the aligned Renderer Begin CALL and to distinguish the candidate jitter target, camera target context and copied-byte source. This change does not add or activate a hook. The updated Release DLL was built locally, with all 21 CTest groups passing; it was NOT installed into MO2 because the game was running. The installed 0.1.20 DLL remains the validated runtime.

## 0.1.21 bounded same-frame game-jitter observation (source)

The decoded Renderer Begin caller loads the game camera object at RVA 0x328cc20 and calls the game jitter update at RVA 0xe58a10. Source 0.1.21 adds a bounded read from that object in the existing post-world callback after the original world draw. Startup checks the exact decoded caller LEA, CALL and target prologue before arming the observation; on the first 12 and each 600th forwarded world call it logs dimensions and projection offsets only when finite and plausible. It does not patch the camera, alter Skyrim TAA, or change NGX jitter, so it cannot yet claim temporal alignment. The SKSE version metadata was updated to 0.1.21. Release Build.ps1 passed all 21 CTest groups. Actual game jitter log: NOT RUN until this build is installed and a user-started game loads a world.
0.1.21 MO2 deployment: release DLL SHA256 be778c1127f4359f41fd23e256fc35cb03527774bca1cfd11cd59a3652b35764. Stage artifacts/local/mo2-stage-0.1.21-cac2e8c and package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.21-jitter-observe-cac2e8c.zip (SHA256 bac7cdcf037b1e7946de75852c85e082aa0f345cabd2a70f36da3acd0dde90d3) were independently verified against all three manifest payload hashes; ZIP contains only plugin DLL, preserved user INI, signed NVIDIA SR runtime and manifest. SkyrimSE.exe was absent at install. Existing installed payload matched its prior manifest before replacement, and prior DLL/manifest were backed up under ignored artifacts/local/mo2-install-backup-0.1.21-cac2e8c. Only DLL and manifest were replaced in D:/TESV_EX/MO2/mods/RazKolbas; installed payloads match the new manifest, with INI, NVIDIA runtime and meta.ini untouched. Actual game jitter observations and temporal alignment: NOT RUN pending user-started loaded-world session. The assistant did not start or close the game.

## 0.1.21 user-run camera jitter result and 0.1.22 source

User-started SkyrimSE.exe PID 1488 (started 13:34:59) loaded the installed RazKolbas DLL and signed nvngx_dlss.dll. The 0.1.21 log armed exact-byte-gated camera observation at 13:35:07. At 13:35:29, first 12 post-world samples were finite at 2560x1440 and frames 9-12 repeated frames 1-4. Converting projection X*width/2 and -projection Y*height/2 yields eight pixel-space pairs: (-.25,-.166667),(.25,.388889),(-.375,.055556),(.125,-.277778),(-.125,.277778),(.375,-.055556),(-.4375,-.388889),(0,.166667). At inspection, the log had reached world frame 25200, with no jitter-read rejection, SDR DLAA disable or busy-slot message in this session. This validates the read-only sample path and eight-phase game values, not NGX temporal quality. The assistant did not launch or control the game.

Source 0.1.22 converts a same-world-callback camera sample to NGX pixel offsets on each prospective DLAA frame. It validates dimensions and finite half-pixel bounds, skips to native rendering on missing/mismatched samples, and resets NGX history after a skipped frame. Skyrim TAA remains enabled. In-game 0.1.22 result: NOT RUN; the user is still running the prior installed 0.1.21 DLL. Motion-vector convention and visual temporal quality remain unverified.
Release Build.ps1 passed all 22 CTest groups after the final 0.1.22 source edits, including the new observed-projection conversion and invalid-sample tests. The standalone NVIDIA replay and actual-game 0.1.22 submission were NOT RUN while the user-started 0.1.21 game remained active. No installed MO2 files were changed during that run.
The standalone signed-NVIDIA D3D11 replay ran after Skyrim exited. Using the user-run 0.1.21 post-world SDR capture, synthetic motion/depth and the measured eight-sample camera jitter cycle, it submitted 30 frames, produced changed output SHA256 863ef90ba5686d8d464baed17a2a5985c50f90653c9b174f644c6f035cadf322 and released the feature cleanly. This establishes the new NGX parameter path accepts nonzero jitter; real game motion quality remains NOT RUN for 0.1.22.
0.1.22 MO2 deployment: Release DLL SHA256 ee337006f7ea10988f6a26222299046737eac46ad659bf8f1042ae7efa05975b. Package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.22-game-jitter-f428926.zip SHA256 2d3f7f33a92f8e4631ab49583cf93b0c10b025990edc84d215fa875e3dcf1f63; all three ZIP payload hashes matched the staged manifest. SkyrimSE.exe was absent when installing. The installed 0.1.21 payload matched its manifest before replacement; prior DLL/manifest were backed up under ignored artifacts/local/mo2-install-backup-0.1.22-f428926. Only DLL and manifest were replaced in D:/TESV_EX/MO2/mods/RazKolbas. The installed DLL, user INI and signed NVIDIA SR runtime match the new manifest, with INI, runtime and meta.ini unchanged. Reference host DLLs, unsigned patched NR DLL, PDBs and captures were not packaged. Real-game 0.1.22 same-frame NGX jitter submission and visual quality: NOT RUN pending user launch. The assistant did not launch or close Skyrim.

## 0.1.22 user-run same-frame jitter result

The user-started SkyrimSE.exe PID 2068 began at 13:50:23 with the installed RazKolbas.dll SHA256 ee337006f7ea10988f6a26222299046737eac46ad659bf8f1042ae7efa05975b and signed NVIDIA SR runtime loaded from the isolated MO2 mod. Continuous SDR DLAA first submitted source world frame 8417 at 13:53:23 with measured NGX jitter (-0.25,-0.16666669), rather than the prior zero offsets. At inspection, 5,400 frames had been submitted with skipped=0; world/Present counts matched through at least 13,800 and Present failures were zero. The current-session log showed no camera-read rejection, jitter-unavailable native fallback, busy slot, DLAA disable, offscreen probe failure or Present failure. The user reported moving-scene visuals were good. This is actual-game PASS for nonzero same-frame jitter submission and observed stability in this interval, with positive qualitative visual feedback. It is not an objective final ReShade pixel comparison or proof that Skyrim TAA is replaced. The assistant did not launch or close the game.

## Post-0.1.22 reduced-input SR backend checkpoint (source only)

Implemented separate render and display extents in owned SR inputs and the offscreen NGX provider; a smaller input now creates Quality SR with display-sized output while existing DLAA remains unchanged. Release Build.ps1 passed all 22 CTest groups. An isolated NVIDIA replay using a 1280x720 downsampled user capture and synthetic guides produced a validated 2560x1440 output SHA256 fecd28b1b422cb55ea05a500f12ec1fc7f7f2736d45727e9b6f4f3d5bea36101 and clean retirement; 480/880 sampled RGB positions differed from nearest-neighbor enlargement. The full-resolution DLAA replay still passed. This proves the resource/NGX SR contract in isolation, not Skyrim reduced-render workload or quality. The exact Address Library maps DRS ID36555 to RVA 0x643c00 and scissor ID77365 to 0xe4adf0, differing from comments in the supplied community source. See re/RENDER_SIZE_CONTRACT.md. A bounded read-only decoded-code capture is prepared for the next user-started game session. Installed MO2 0.1.22 and its user INI/runtime were not changed; in-game reduced SR is NOT RUN. The assistant did not start or control Skyrim.

## 0.1.23 End-key diagnostics menu (source)

Added a read-only Dear ImGui panel on the exact verified ENB Present path. End toggles it while Skyrim is focused. It reads atomic effective-frame state and reports Native/DLAA/DLSS SR, render and native display extents/scale, source and DLSS submission counters, fallback count, session-disable state, and the still-active Skyrim TAA. The current game path can publish only Native or full-resolution DLAA; it does not claim reduced-render DLSS SR. Backbuffer RTV is temporary and D3D11 state is restored before Present. Pinned upstream ImGui 1.91.9b MIT license notice is included in staging. Release Build.ps1 passed all 22 CTest groups. On-screen End toggle and ENB/ReShade composition are NOT RUN until the user starts Skyrim with the installed build. See re/DIAGNOSTICS_MENU.md.

0.1.23 MO2 deployment: package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.23-menu-ef9570b.zip SHA256 b6772ee4d4176441d1285711d3b2a260a90d5b76d88f9a3c373819ede746647e. Independent ZIP inspection found exactly the manifest and four payloads, all matching their SHA256 entries. SkyrimSE.exe was absent. The installed 0.1.22 payload first matched its manifest; prior DLL and manifest were copied to ignored artifacts/local/mo2-install-backup-0.1.23-ef9570b and checked. Installed DLL SHA256 325c4f74d3ac8d725044bbdfc595fa295941b341bab3cf1395e64bf0fc502a32. The new installed manifest verifies all four payloads; user INI and signed NVIDIA SR runtime hashes are unchanged. Added the Dear ImGui MIT notice. Real-game End menu and reduced-render SR remain NOT RUN pending a user-started launch. The assistant did not start or control Skyrim.

## 0.1.23 user-run menu and 0.1.24 cursor checkpoint

The user-started 0.1.23 process opened the End menu at 14:19:06; Present/world counts matched through at least 37,200 and Present failures were zero. The user confirmed the panel renders, but Skyrim's visible cursor passes beneath it and cannot move the panel. The sampled depth stayed menu-like in this session, so in-game DLAA for 0.1.23 is NOT VERIFIED. Root cause in the new panel: mouse coordinates/buttons were forwarded to ImGui, but ImGui's own cursor drawing remained disabled while the game cursor had already been composited under the panel. Source 0.1.24 enables the ImGui cursor only when open/focused and records bounded mouse position/button/capture diagnostics. Release Build.ps1 passed all 22 CTest groups. Actual drag and gameplay-input isolation are NOT RUN; see re/DIAGNOSTICS_MENU.md. The assistant did not start or close Skyrim.

0.1.24 cursor deployment: package D:/TESV_EX/MO2/downloads/RazKolbas-0.1.24-cursor-6b3cc05.zip SHA256 7573897e9e2e8c8c204d6701388dc78d055c538fba76f4cded2690cba67c353c; independent ZIP inspection verified exactly four payloads plus manifest and all hashes. Skyrim was closed. The previous installed payload matched its manifest; prior 0.1.23 DLL/manifest were verified in ignored artifacts/local/mo2-install-backup-0.1.24-6b3cc05. Installed DLL SHA256 7494c13238712bfe5595ba8c7b1cb60d0656d1381d92c1ad713b80f22be772ee. All installed manifest hashes match; INI, signed NVIDIA runtime and ImGui notice are unchanged. Live panel dragging and whether Skyrim input needs separate suppression remain NOT RUN pending a user-started launch. The assistant did not start or close Skyrim.

## Post-0.1.24 SDR spatial fallback (source only)

The existing HDR spatial fallback now shares its owned D3D11 implementation with a new RGBA8 SDR fallback. A WARP integration test submitted a reduced 2x2 four-colour scene, waited for completion, read back the display-sized 4x4 output, checked corners/interpolation and rejected a zero output extent. The test failed to compile before the new API was implemented. Release `tools/Build.ps1 -Preset win-release` passed all 22 CTest groups afterward. This producer is not yet connected to Skyrim's reduced-render path; in-game DLSS SR, reduced workload and fallback remain NOT RUN. The installed MO2 0.1.24 binary is unchanged. The user deferred a separate game run for the cursor, so the next game session should be a combined SR/UI validation after the render-size and presentation transaction is ready. FG remains unimplemented and requires a verified display/UI owner after SR. See `re/RENDER_SIZE_CONTRACT.md`.

## 2026-09-20 decoded DRS/scissor contract (user-started game)

The user started SkyrimSE.exe PID 20924 at 14:36:35. A hash-gated, bounded read-only capture from its decoded 1.6.1170 image established the exact DRS CALL at RVA 0x643c2d to 0xe587f0, the DRS state ratio/lock offsets, and the scissor function at 0xe4adf0. At the main menu, the sampled state reported 2560x1440, four ratio floats of 1.0 and lock counter zero. The scissor function takes x,y,width,height and constructs a RECT. The reference source's NOP5 at 36555+0x2d would suppress the whole vanilla DRS update call; RazKolbas must preserve that call outside an active reduced-render transaction. Full disassembly evidence is in re/RENDER_SIZE_CONTRACT.md; raw captures remain ignored under artifacts/local/drs-live-2026-09-20-1436-extended. No game memory was written, no game controls were exercised, and the assistant did not start or close Skyrim.

Source now provides an exact-code ABI gate for those decoded DRS/scissor sites and a matching CALL planner contract. The new test first failed to compile because the gate was missing; Release tools/Build.ps1 then passed all 22 CTest groups, including changed-byte and truncated-context rejection. The code is not wired to install either hook and is not in the MO2 0.1.24 installation. Reduced-render in-game DLSS SR, fallback, native UI placement and FG remain NOT RUN. Next implementation: guarded render-size transaction that forwards vanilla DRS when inactive, scales scissor only in the world render domain, and publishes a display-sized DLSS or spatial fallback before native-resolution UI composition.

## Continuous reduced-input SDR Quality SR replay (source only)

SdrDlssPresenter now accepts separate render-sized scene/guides and a display-sized active backbuffer for Quality SR. It validates the destination before NGX submission, retains the three owned input/output slots through GPU completion, and preserves the existing full-size DLAA entry. The harness initially crashed because its new 1280x720 depth data was paired with a 2560x1440 texture descriptor; this fixture mismatch was corrected before any success claim. The final 30-frame NVIDIA replay submitted synthetic 1280x720 inputs derived from a user 2560x1440 post-world capture, published 2560x1440 output SHA256 867cce8ef853ab8329e0eb605d4f3c1960b6bab5874e7284ece2a9b051137bb2, and shut down cleanly. The 30-frame DLAA regression also passed, output SHA256 8de9edf807257eb7efce05a646a05cdc6f09eeb3594c300b7fbd1cdc00fcb788. Release tools/Build.ps1 passed all 22 CTest groups after the final edit. This is an isolated GPU replay on synthetic guides, not reduced-workload Skyrim DLSS. The active MO2 0.1.24 plugin was not replaced; game SR failure/fallback, UI placement and FG remain NOT RUN. See re/RENDER_SIZE_CONTRACT.md.

## Source-only SDR SR failure presentation and replay

The SDR SR presentation stage now invokes one provider per real frame and, on a busy slot or recoverable evaluation failure, draws the preserved reduced RGBA8 scene directly into the active display-sized backbuffer. It retains scene/display resources through a GPU query and propagates device removal without a fake success. A new WARP integration test first failed because the API was missing, then read back a 2x2-to-4x4 forced-failure output, checked the restored RTV binding and retained-source lifetime. Release tools/Build.ps1 passed all 23 CTest groups. The standalone NVIDIA replay with one injected failure completed 30 displayed frames: 29 Quality SR submissions and one 2560x1440 spatial fallback, fallback SHA256 3106a5bfc949e2e522cbdc15a983ecd06b4a94da42b6f64de674453ad2299791. It reset provider history after the fallback, resumed SR and shut down cleanly; final output SHA256 0d65cd3d050a3a433ad4a3c612dd4f22f893012204bc246fe704a7503ca2c773. No-failure SR and DLAA 30-frame regressions passed. This is an isolated replay with synthetic reduced inputs, not an in-game reduced-render or UI/ENB/ReShade result. Installed MO2 0.1.24 remains unchanged; in-game DLSS SR and FG remain NOT RUN. Next implementation boundary: supply a genuine reduced world scene and native UI/display ownership to this stage before enabling any DRS transaction.

## 2026-09-20 render-target recreation trace prepared

The earlier ignored live-stage bundle already held 12 KiB of decoded original world-draw code, so no repeat capture is planned. Renderer Begin's decoded caller builds a width/height descriptor at RVA 0xe44725..0xe4475c and calls 0xe4fb90; reset branches call 0xe43bc0 and 0xe44050. The exact callee behavior and whether a genuinely reduced HUD-free source survives to the post-world boundary remain unresolved. The exact-game-hash, read-only inspector now includes those missing callees plus setup function 0xe4f180 for a future user-started process. Installed MO2 0.1.24 and game runtime are unchanged. In-game reduced SR/FG remain NOT RUN; this source change is a diagnostic capture plan, not a validated game contract. See re/RENDER_SIZE_CONTRACT.md.

## 2026-09-20 user-started decoded renderer reset result

The user-started exact-hash SkyrimSE.exe PID 24652 was read without any remote calls, writes, game input or assistant-controlled exit. The decoded Renderer Begin callee at RVA 0xe4fb90 is only a 28-byte descriptor copy, correcting the previous allocation candidate. A full callee capture shows 0xe43bc0 releases renderer colour/depth entries and reaches swap-chain ResizeBuffers (+0x68) on a changed-extent path; 0xe44050 can call ResizeTarget (+0x70). An existing third-party/unknown-owner indirect jump at 0xe43e84 redirects immediately before ResizeBuffers through a stub that supplies its own dimension/format payload. Its owner remains unidentified, so no RazKolbas patch targets that site. Loaded-world state remained 2560x1440 with DRS ratios 1.0 and lock zero. The log reached 22,800 world/Present calls with failed=0 and 16,200 full-resolution DLAA submissions with skipped=0; no new crash log was found, and process exit cause was not observed. In-game reduced DLSS SR and FG remain NOT RUN. Ignored raw evidence and SHA-256 values are recorded in re/RENDER_SIZE_CONTRACT.md. The installed MO2 0.1.24 package was not changed.

## Reference reduced-buffer path and source-only sizing guard

Further exact-hash, read-only RE of the supplied SkyrimUpscaler.dll found that its swap-chain factory computes a render extent from display dimensions and a ratio, and its proxy allocates a render-sized SDR texture at object +0x140. With the constructor initial flags, GetBuffer returns that texture; the post-world SR wrapper copies from it. The proxy Present helper also invokes the SR wrapper, so the prior callback-only description was incomplete. Branches, RVAs, uncertainty about UI resolution, and ignored disassembly locations are in re/SKYRIM_REFERENCE_OUTPUT_PATH.md. This is reference behavior, not a RazKolbas runtime result or permission to take an occupied swap-chain/resize hook.

The source-only RenderSizePolicy selects reduced dimensions only when an owned world target and display-sized output/fallback are both ready; otherwise it returns native dimensions. It scales x/y/width/height only for world scissors and preserves native UI scissors. A new empty-scissor test exposed an accidental one-pixel expansion and was fixed. Debug and Release tools/Build.ps1 passed all 24 CTest groups after the fix. No DRS/scissor hook was installed, no game was started or controlled, and MO2 remains 0.1.24. Genuine in-game reduced DLSS SR, UI placement and FG are NOT RUN.

## Owned reduced SDR surface and WARP handoff

Source now creates an owned render-sized RGBA8 texture with RTV/SRV bindings and a separate display extent. WARP rendered into a 4x4 instance, copied it through the existing SDR SR input path, forced provider failure, then read back an 8x8 spatial fallback after the original surface and copied input were retired. Invalid geometry and a distinct replacement generation were verified. Debug and Release tools/Build.ps1 passed all 25 CTest groups. This is an offline producer/retention result only: Skyrim has not been directed to render into this surface, no swap-chain or DRS hook was installed, and installed MO2 0.1.24 remains unchanged. In-game reduced DLSS SR and FG remain NOT RUN. Details: re/RENDER_SIZE_CONTRACT.md.

## Pending exact renderer target-binding capture

The saved decoded world-draw code loads the renderer interface from game RVA 0x32887b0 and calls virtual slot +0x108 at RVA 0xe449f8; the prior live bundle began its renderer snapshot 16 bytes later and omitted the interface/vtable. The exact-game-hash, read-only inspector now captures that pointer, its vtable and bounded method code from a user-started loaded-world process, rejecting null or non-game targets. Python syntax and CLI checks passed. The new live capture is NOT RUN; no game was launched or controlled, no DLL was changed or installed, and MO2 remains 0.1.24. This specific decoded method is needed to decide how to hand a full-size target back to native UI after genuine reduced SR. See re/RENDER_SIZE_CONTRACT.md.

## User-started D3D11 context chain and DynamicShaderFrameGen source cross-check

The user-started exact-hash PID 22120 remained running while a bounded read-only capture followed the game pointer at RVA 0x32887b0. It is the D3D11 immediate context, not a game renderer interface: `OMSetRenderTargets` at vtable +0x108 goes through exact-hash ENB `d3d11.dll` (RVA 0x68f40), then exact-hash ReShade `dxgi.dll` (RVA 0xf40d0), then system `d3d11.dll` (RVA 0x100640). The same chain maps +0x190 to `ClearRenderTargetView`. No game memory write, remote call, input, launch or exit occurred. The current DynamicShaderFrameGen HEAD still matches the pinned source. Its DRS timing is useful, but its own active DLSS Present path reads the full-size backbuffer after composition because its pre-UI kFRAMEBUFFER texture was null; direct copying would not prove genuine reduced-input DLSS or native UI here. The independent adaptation contract and raw ignored capture location are in re/RENDER_SIZE_CONTRACT.md. Installed MO2 remains 0.1.24, and in-game reduced DLSS SR/FG remain NOT RUN.

## 0.1.25 experimental engine DRS candidate from DynamicShaderFrameGen timing

The current upstream DynamicShaderFrameGen HEAD remains daaba8aadb2dbc8c5e52b028f12475c3450b6866. RazKolbas independently implemented a guarded Renderer Begin jitter CALL hook at verified RVA 0xe44672, forwarding Skyrim's original before touching state. Exact caller/target ABI tests and a pure ratio/lock ownership policy passed; Debug targeted tests passed 136 assertions in 16 cases and Release tools/Build.ps1 passed all 26 CTest groups. The probe is disabled by default and needs Diagnostics.ProbeReducedWorld=true plus ManualRenderScale in [0.5,1). It measures actual kMAIN/motion/depth/display extents and suppresses the current full-resolution DLAA copyback after activation. It does not submit DLSS SR, replace Skyrim's target, or implement FG. A staged 0.1.25 opt-in MO2 candidate exists under D:/TESV_EX/MO2/downloads/RazKolbas-0.1.25-drsprobe-stage; its four payloads matched the manifest. The prior 0.1.24 game process had exited before staging, and the assistant did not start or close Skyrim. In-game DRS result, scissor/UI behavior, genuine reduced-input DLSS SR and FG remain NOT RUN.

0.1.25 DRS-probe MO2 deployment: ZIP D:/TESV_EX/MO2/downloads/RazKolbas-0.1.25-drsprobe-a5c057f.zip SHA256 444a04b2a0e5e8106359fca0f6c4f8dab3028b6131be09dfab6bb0474beafec2. Independent ZIP inspection found exactly the manifest and four payloads, all matching their SHA256 entries. SkyrimSE.exe was absent. The installed 0.1.24 files first matched their manifest; each prior payload and manifest was copied to ignored artifacts/local/mo2-install-backup-0.1.25-a5c057f. All installed 0.1.25 manifest hashes match; DLL SHA256 b07d21f501d98d5db05603bd0774cec2e0111f8b96c734aff5adcdc77a2efd79. The prior INI was preserved in the backup; the installed opt-in INI sets ManualRenderScale=0.666667 and ProbeReducedWorld=true. Signed NVIDIA SR runtime and ImGui notice hashes are unchanged. Runtime DRS, UI/scissor, reduced SR and FG remain NOT RUN until the user starts the installed MO2 profile. The assistant did not start or close Skyrim.

0.1.25 user-started runtime result: exact-hash SkyrimSE.exe PID 15004 installed the DRS CALL and created the ENB/ReShade device, but its first world callback saw native (1,1) ratios with lock 1. The ownership policy permanently rejected that run without changing the game state. A later read-only live sample saw lock 0 with the same native ratios and 2560x1440 display; no separate upscaler/FrameGen module was loaded. Full-resolution DLAA continued past 28,200 world/Present observations with zero reported Present failures. The user exited; the assistant did not start, control or close Skyrim. Revised source now waits read-only through exact native-sized lock 1 and retries only after it clears, while continuing to reject foreign ratios/extents/locks. The new policy test passed 30 assertions in four cases; Release tools/Build.ps1 passed all 26 CTest groups. This revision has **not** run in Skyrim. Genuine reduced DLSS SR, UI/scissor compatibility and FG remain NOT RUN. See re/RENDER_SIZE_CONTRACT.md.

0.1.26 guarded-retry MO2 deployment: source commit c95b4d0. The ZIP at D:/TESV_EX/MO2/downloads/RazKolbas-0.1.26-drsretry-c95b4d0.zip has SHA256 a11d9aefba0a7238a265330230525be38c9c21d35239ba59ffd2aaac7b59e095. Independent ZIP inspection verified exactly four payloads plus manifest and every payload hash. SkyrimSE.exe was absent before installation. The previously installed 0.1.25 payloads first matched their manifest, then were backed up with that manifest under ignored artifacts/local/mo2-install-backup-0.1.26-c95b4d0. All installed 0.1.26 payload hashes match; DLL SHA256 62aebd2152b4593074148b2c82b753668de08740d35effe842a558dc35f788d4. The opt-in INI still sets ManualRenderScale=0.666667 and ProbeReducedWorld=true; the signed NVIDIA SR runtime is unchanged. The assistant did not launch or close Skyrim. Runtime behavior of the retry remains NOT RUN until the user starts the installed MO2 profile.

0.1.26 user-started runtime result: after 5,078 native-lock waits, one Renderer Begin callback set the 1706x960 DRS ratio; the next saw lock 3 and rejected further writes. The live state later held previous ratios near 0.666, current ratios 1.0 and lock 0. Actual kMAIN/motion/depth/display descriptors all stayed 2560x1440 through multiple world samples; no reduced DLSS SR was submitted. World/Present observations passed 19,200 with zero reported Present failures and no new crash log. The unsuccessful ratio-only probe withheld continuous DLAA for the remainder of this run; the user then exited. Source now confirms native ratios/lock and all three full-size scene guides before permitting DLAA to resume after probe rejection. The new pure recovery test passed, and Release tools/Build.ps1 passed all 26 CTest groups. This recovery has NOT RUN in game. A hash-gated read-only full decoded-code capture tool is prepared because disk Skyrim code is encrypted at the DRS site. The next implementation task is an owned reduced scene/display transaction compatible with ENB/ReShade, using decoded allocation/lock xrefs and the reference's render-sized proxy path as evidence; genuine reduced SR, UI placement and FG remain NOT RUN.

0.1.27 safe MO2 deployment: source commit 7f35897. ZIP D:/TESV_EX/MO2/downloads/RazKolbas-0.1.27-native-recovery-7f35897.zip SHA256 1f2442411266ce8d66fba67ba73b08490a528bdbbe99ad465ad21128c2910a89; independent ZIP validation checked exactly four manifest-listed payloads and their hashes. The game was absent before installation. Installed 0.1.26 files matched the prior manifest and were backed up under ignored artifacts/local/mo2-install-backup-0.1.27-7f35897. New installed DLL SHA256 e76b95d6f7f228f571364aabb4145c4c6eea9553c7d28df5307bbae8ed354dc1 and all installed payloads match the new manifest. The INI explicitly sets ProbeReducedWorld=false, leaving the tested native-resolution DLAA path enabled; the signed NVIDIA runtime is unchanged. The native-recovery code is build-tested but not in-game tested. Next user-started run will be used for a bounded read-only decoded-code capture, not another ratio-write experiment; then static xrefs can identify an owned reduced-buffer and presentation integration. The assistant did not start or close Skyrim.

0.1.27 user-started decoded-code result: exact-hash PID 8012 yielded a 24,435,000-byte decoded executable `.text` snapshot for offline analysis, SHA256 75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3; no game memory was written or game input sent. The decoded state lock helper at RVA 0xe58980 is an atomic increment/decrement counter, not an exclusive boolean. The main render phase increments it at 0x6d2201 before Renderer Begin and decrements at 0x6d22dd after Renderer End. The target allocator at 0xe44d90 creates textures through D3D11 CreateTexture2D, and its initializer 0x14cf210 constructs target descriptors from native state display dimensions rather than the DRS ratio. The texture-creation wrapper at 0xe4fbb0 is already indirectly detoured; its owner was not captured before process exit. These findings explain the failed ratio-only probe and rule out blind writes to the shared lock or wrapper. The installed opt-in probe remains off; reduced DLSS SR, UI placement and FG remain NOT RUN. See re/RENDER_SIZE_CONTRACT.md. Next executable RE is a bounded read-only hook-owner/viewport trace on a user-started game, followed by an owned reduced-buffer transaction design.

Offline viewport xrefs show that Skyrim writes D3D11_VIEWPORT width/height at static RVA 0x202abe8/0x202abec, and the setup path at 0xe43f10/0xe43f18 reads the DRS ratios before those writes. RSSetViewports consumes the static viewport at 0xe44b5f/0xe4b8c3. This indicates a potential reduced viewport inside full-size targets, not proven reduced source pixels. The ratio-only call-site probe is now retired in source: an opt-in setting returns Unsupported, with no callback installed and no ratio/counter writes. The 0.1.27 installed package remains unchanged with the probe off. Release tools/Build.ps1 compiled RazKolbas.dll and all 26 CTest groups passed; no new game test has run. A new MO2 package/install is pending.

0.1.28 safer MO2 deployment: source commit 21bb5e9. ZIP `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.28-retired-drs-21bb5e9.zip` SHA256 `3616627c01d136cc5178088383d0e18f12fbfd950ab4bdfe5a6ef4b320207c7e`; independent ZIP validation found exactly four manifest-listed payloads with matching hashes. SkyrimSE.exe was absent. The previous install's four payloads matched its manifest, and the complete mod folder was copied to ignored `artifacts/local/mo2-install-backup-0.1.28-21bb5e9` before replacing only manifest-listed files. Installed four payload hashes match the new manifest; DLL SHA256 `e0ba31392cf222d6e68c2403e10c6fa245a9e242b2f55bb34373dc27a94c6468`. The user's INI was preserved with `ProbeReducedWorld=false`; the signed NVIDIA SR runtime is unchanged. Source and package are build/manifest-tested; 0.1.28 has NOT RUN in game. The next user-started world run should collect a bounded read-only sample of detour ownership, viewport, DRS counter and ratio with `inspect_live_detours.py`, then determine whether a reduced valid scene rectangle exists before changing presentation ownership. The assistant did not start or close Skyrim.

0.1.28 user-started world run: PID 14920 loaded the installed DLL hash above. Read-only samples identified the texture-creation detour target inside SKSE and the resize detour's private trampoline data inside SSE Display Tweaks. The native dynamic-resolution enable byte was 0; ratio remained 1/1 and the shared counter naturally changed 0/1. Viewports at the shared static address included 2560x1440 world and smaller offscreen passes, so a viewport sample alone is insufficient to prove a reduced world region. The game's `DynamicResolution width/height/toggle` console handler and native update call chain were located in decoded code; no console command was sent. The 0.1.28 log shows a finite nonuniform offscreen DLAA result followed by continuous SDR DLAA display submissions through at least 8,400 frames with `skipped=0`; Present observations through frame 15,000 reported failed=0. This run confirms the native DLAA path, not reduced DLSS SR. An independent top-left region crop for display-sized SDR colour/motion/depth is now implemented; WARP readback verified colour/motion bytes and normalized R32_FLOAT depth conversion. Release tools/Build.ps1 passed all 26 CTest groups. That crop path is not wired into the installed game plugin, and NGX acceptance of R32_FLOAT depth plus a same-frame reduced world rectangle remain NOT RUN. Next executable work is a safe native DRS activation/measurement transaction and owned SR output/presentation path that preserves SKSE and SSE Display Tweaks hooks. The assistant did not start, control or close Skyrim.

0.1.29 diagnostic source: removed the obsolete exclusive-lock DRS policy and its tests, replacing it with a native-ratio readiness gate. The world hook now reads both current and previous engine ratios after forwarding the game draw; a nonnative or invalid ratio suspends new DLAA submissions, preserves pending copy retirement, requests a temporal reset and logs one guarded world-boundary descriptor/viewport sample. Returning to native ratios resets the presenter before DLAA can resume. It does not write DRS state or change render targets. A new pure readiness test and the WARP region/depth conversion test passed; Release tools/Build.ps1 passed all 26 CTest groups. The 0.1.29 behavior is NOT RUN in game. The next game run needs the diagnostic package installed and Skyrim's own `DynamicResolution width/height` command used to create a native-owned reduced viewport; same-frame colour/motion/depth validity and NGX acceptance still need verification. The assistant did not start or close Skyrim.

0.1.29 diagnostic MO2 deployment: source commit 9f93664. ZIP `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.29-native-drs-guard-9f93664.zip` SHA256 `e993f520492d722db2a31d6b040ad77b242d791acd512f8d419e18bcd028113c`; independent ZIP inspection verified exactly the manifest and four listed payloads with matching hashes. SkyrimSE.exe had exited before staging and installation. The previous four installed payloads matched their manifest; the complete previous mod folder is backed up under ignored `artifacts/local/mo2-install-backup-0.1.29-9f93664`. The new four installed payloads match their manifest; DLL SHA256 `1dac44bdbb6d54ed698f22243fe1315ab2c6f9b16e8ba9463e67ff05ce664ddb`. The user's INI remains unchanged with `ProbeReducedWorld=false`, and the signed NVIDIA SR runtime is unchanged. No game runtime claim is made for 0.1.29. The next user-started run should first enter a world scene, then use Skyrim's own `DynamicResolution width 0.666667` and `DynamicResolution height 0.666667` console commands and leave the game running. Capture the new ratio/viewport and RazKolbas one-time guide log, then restore with `width 1`/`height 1` if needed. Do not start or close Skyrim from the assistant.

0.1.29 user-started native DRS result: PID 21428 accepted the two console commands. A read-only 12-second live sample showed stable current/previous ratios 0.665625/0.666667, display 2560x1440 and several 1704x960 viewport samples; the automatic DRS enable byte stayed zero. Other viewport samples belonged to full-size and offscreen passes. RazKolbas suspended DLAA on the first width change. Its one-time descriptor log preceded the height command and showed full-size 2560x1440 colour/motion/depth allocations and then-current viewport. The read-only sample and log prove native ratio/viewport changes and safe DLAA suppression; they do not prove a valid same-frame reduced colour/motion/depth region for DLSS. No reduced SR or FG submission occurred. The user exited the game; the assistant did not start, control or close it. See `re/RENDER_SIZE_CONTRACT.md`.

0.1.30 source update: the End menu now labels the ratio-derived Skyrim DRS target separately from the native display and shows DLAA suspension while reduced DRS is active. The target calculation uses the measured, game-quantized ratios (1704x960 from 0.665625/0.666667 on 2560x1440), rejects invalid values, and does not claim this is a verified DLSS input. The pure target test first failed for the missing function; Release `tools/Build.ps1` then compiled the DLL and passed all 26 CTest groups. This menu change has NOT RUN in Skyrim. NVIDIA's Streamline SR D3D11 and manual-hooking guides are now tracked as reference contracts; Streamline integration, reduced SR and FG remain NOT RUN. Next work is a same-frame reduced world-pixel/guide trace at the viewport-owning render phase, then an owned SR submission path that preserves existing swap-chain owners.

0.1.30 MO2 deployment: source commit `9045933`. ZIP `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.30-drs-menu-9045933.zip` SHA256 `d98be81b0c65aac72fb8585335fb42e83b92b5e986169f32edb0f656755062ed`; independent ZIP inspection verified exactly four manifest-listed payloads and their hashes. SkyrimSE.exe was absent before installation. All previously installed 0.1.29 payloads matched the old manifest; the complete mod folder was backed up under ignored `artifacts/local/mo2-install-backup-0.1.30-9045933`. The four new installed payloads match the new manifest. Installed DLL SHA256 `29dbf10745c83887dcf8c32782e53fa6a9692bd291ed83c9e4c6c3b105856ee3`; the user's INI and signed NVIDIA SR runtime hashes are unchanged. No game runtime claim is made for 0.1.30; the assistant did not start or close Skyrim.

## 2026-09-20 prepared SR fix handoff, 0.1.31 source candidate

The prepared SDR input now carries an explicit source rectangle with nonzero origin and independent render/display extents. Colour and motion are cropped with `CopySubresourceRegion`; R24 depth is converted to owned R32_FLOAT by a cached D3D11 compute shader. The presenter has a separate `evaluatePrepared` entry that consumes these resources directly, accepts R32 depth, identifies each frame/generation and fences all submitted work, including recoverable failure. A token permits controlled publication only for the most recently evaluated same-generation frame and only when the display destination is active RTV0. Existing native DLAA `render` and legacy `renderSr` remain unchanged. A synthetic fallback preserves current scene colour after injected GPU work, and slot/SDK teardown waits for completion. This is an implemented module contract, not a game SR connection.

WARP tests cover nonzero crop origin, enlarged display output, exact prepared presenter entry, offscreen evaluation, stale token rejection, partial-recording failure, fallback and retirement. Release `tools/Build.ps1 -Preset win-release` built the DLL/harnesses and passed all 27 CTest groups. On the RTX 4080 SUPER, `RazKolbasSdrLivePresentation --prepared-r32` replayed 30 synthetic 1280x720-to-2560x1440 frames through the pinned signed `nvngx_dlss.dll` with 30 successful NGX submissions, zero fallback and clean retirement; output SHA256 was `5a63eeda8176d94daba4da79118a57419e4f163e47aff13438ffc5646a2f5cf3`. A post-evaluation injected failure yielded one current-colour spatial fallback and recovery for the remaining frames, with output SHA256 `2d1a957de8972d66655de8f4d4a4bba62ee4bfacbeeeb99f43e9d086de8b9844`. Existing 30-frame legacy SR, DLAA and pre-evaluation fallback replays also passed. The input was a downsampled user capture with synthetic guides, so these tests do not establish Skyrim render reduction, guide units or visual quality.

The first game-facing diagnostic for the scene/display decision now waits for two consecutive valid four-ratio tuples, then records a generation-tagged same-frame post-world viewport and all three guide descriptors. Only after current and previous ratios agree does it read back colour/motion/depth together and log their hashes and depth statistics. This replaces the old suppression-transition-only descriptor sample. It neither writes engine DRS state nor submits reduced SR. The preferred game route remains an owned reduced scene plus distinct display output/native UI handoff, preserving ENB/ReShade/SKSE/SSE Display Tweaks ownership; a native-DRS valid-region route is still conditional on coherent producer-phase evidence. The 0.1.31 game diagnostic, native UI placement, reduced Skyrim SR, NR evaluation and FG are NOT RUN. The assistant did not start or close Skyrim.

0.1.31 MO2 deployment: source commits `1979761` and `345ba44`. Debug and Release builds each passed all 27 CTest groups. The ZIP `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.31-prepared-sr-345ba44.zip` has SHA256 `68ec0f97db58776942462753f701f408bbca51b09766563b6eaad75859ce8f05`; its entry list is exactly the four manifest-listed payloads plus `install-manifest.json`. The previously installed four payloads matched their manifest and SkyrimSE.exe was absent. The prior mod folder was backed up to ignored `artifacts/local/mo2-install-backup-0.1.31-345ba44`; only the plugin DLL and manifest were replaced in `D:/TESV_EX/MO2/mods/RazKolbas`. Every installed payload matches the new manifest. Installed DLL SHA256 is `0f138f494f02d74ed3eb8355f647932a20d0c84e95658ccbf08a0ce3124dd4cf`; the user INI remains `fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`, and the signed NVIDIA runtime remains `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`. The assistant did not start or close Skyrim. This installed build still displays native DLAA/fallback, not reduced game SR.

## 2026-09-20 guarded in-game reduced SR candidate, 0.1.32 source

The existing exact Skyrim world callback now has a reduced SR route for `Upscaling.Provider=Auto` or `DLSS` when Skyrim's own current and previous DRS ratios agree below native. At the stable post-world boundary it reads the SDR backbuffer, motion and depth from the same frame. Before enabling display, it requires matching display-sized guide allocations, nonuniform colour inside the ratio-derived render rectangle, approximately uniform colour outside it, and world-like depth inside. This rejects a detailed full-size image that would make a top-left crop a false SR source. It is a conservative runtime source gate, not proof that every ENB/ReShade configuration renders a usable rectangle. Failed verification leaves Skyrim's native reduced frame untouched. The branch never writes the engine's shared DRS counter or replaces its existing creation/resize detours.

After source acceptance, each frame creates owned cropped RGBA8/RG16/R32 input, derives render-pixel jitter from the same-frame camera projection, evaluates NVIDIA SR, then publishes the display-sized result into the active pre-UI backbuffer. A busy slot or recoverable evaluation failure spatially expands the preserved current colour; a fixed three-slot retirement array retains fallback sources until their GPU events complete without holding swap buffers across resize. The native DLAA presenter is fenced and shut down before SR starts, and the SR presenter is fenced and shut down before native DLAA resumes. The End menu now reports whether the reduced source was accepted and whether DLSS SR was actually submitted. The game route compiles, but its source gate, motion-vector units, quality, native UI handoff and fallback have NOT RUN in Skyrim. The selected candidate is native DRS valid-region SR; if the gate rejects the post-world SDR image, the owned reduced-scene proxy described in the fix handoff remains the next required implementation.

Release `tools/Build.ps1 -Preset win-release` passed all 27 CTest groups after this integration. The signed NVIDIA prepared-R32 30-frame replay and post-evaluation fallback replay still pass with the same output hashes recorded above. These are offline and GPU harness results, not an in-game DLSS success claim.

The owner supplied `C:/Users/user/Downloads/RazKolbas_Code_Examples.zip` as a reference after this source candidate was built. Its README labels the C++ examples illustrative and uncompiled; the archive is not an applied patch or a new runtime result. `PreparedSrExample.hpp` independently emphasizes query retirement and persistent input owners; `UiHandoffExample.hpp` binds a native RTV/viewport/scissor but explicitly leaves Skyrim's renderer binding cache unresolved. RazKolbas's prepared presenter already fences evaluation and publication and checks that the active RTV0 is the native destination. The current Skyrim branch does not blindly bind a different UI target or write unverified renderer-cache fields. Producer lifetime and UI/cache handoff remain in-game validation concerns. No files were copied from the archive into the product.

0.1.32 MO2 deployment: source commits `70c174d` and `6deff35`. ZIP `D:/TESV_EX/MO2/downloads/RazKolbas-0.1.32-guarded-sr-6deff35.zip` SHA256 `392339bf34ecf177ddbaf6c668ba44b0fa0d9bd1b969b56866bc9c8c5a890f6f` contains exactly the four manifest-listed payloads plus `install-manifest.json`; payload hashes were checked within the ZIP. SkyrimSE.exe was absent. The previously installed 0.1.31 payloads matched their manifest before backup to ignored `artifacts/local/mo2-install-backup-0.1.32-6deff35`. Only the DLL and manifest were replaced in `D:/TESV_EX/MO2/mods/RazKolbas`; every installed payload matches the new manifest. Installed DLL SHA256 is `8cb53b71b7ac90e2b7d41a22df89757d0a1013fa9355443f48b64e1a09323577`; the user INI and signed NVIDIA SR runtime retain the hashes recorded above. The assistant did not start or close Skyrim. The installed reduced SR route is **NOT RUN in game**.

## 2026-09-20 configured DLSS quality, 0.1.33 source candidate

The selected `Upscaling.Quality` value now reaches the NVIDIA SR feature creation: Quality, Balanced, Performance and UltraPerformance map to the corresponding pinned NGX enum, while NativeAA requests only native DLAA. An active feature refuses quality changes until it is retired. Exact parsing, invalid choice and active-feature tests passed. Release `tools/Build.ps1 -Preset win-release` passed all 27 CTest groups. This fixes a configuration-to-NGX gap but does **not** create the reduced world scene or native UI handoff. The MO2 installation remains 0.1.32; 0.1.33 has NOT RUN in Skyrim and is not described as DLSS SR working.

## 2026-09-20 owned scene routing components, source only

The `RazKolbas_Owned_Scene_Fix` handoff was hash-verified and inspected as a
reference, not applied as a pre-tested patch. Its calculated Skyrim rectangle
hook address `0xE44722` was rejected: exact local decoded code places the
six-byte `GetClientRect` call at `0xE4471B`, followed by an instruction starting
at `0xE44721`. The import cell is `0x174F928`. See
`re/OWNED_SCENE_RECT_1170.md` for the caller and identity evidence.

Source now includes a version-gated six-byte indirect CALL decoder/encoder,
nearby read-only pointer cell and ownership-checked startup-boundary write and
restore. An executable fixture proved the original HWND/RECT callable is
forwarded exactly once and the callsite is restored. `OwnedSceneDomain` tracks
world, processing and native UI phases by real frame and generation;
`RendererLogicalSize` narrows rectangle replacement to the matching window and
world phase. `NativeUiRedirector` recognizes the underlying reduced texture
identity and maps a cached single RTV plus full viewport to a native target
after publication; its WARP test validates native target pixels and next-frame
world return without ReShade. `ReducedSdrSurface::queryBuffer` now exposes the
stable reduced resource through COM `QueryInterface` for a future controlled
GetBuffer route. Release `tools/Build.ps1 -Preset win-release`
built and passed all 30 CTest groups.

These are production source modules and offline tests, but the **owned scene
producer is not yet connected to Skyrim**: factory interception before nested
swap creation, a correct swap-chain alias/proxy, actual game guide identities,
NGX sizing, context-hook installation, resize retirement and game UI behavior
remain open. The new rectangle patch and UI adapter are not installed in the
game. Before the deployment below, the user's MO2 mod was 0.1.32. DLSS SR is
**not on** in Skyrim.
The 0.1.34 candidate adds a read-only pre-creation adapter-parent factory
provenance log to the existing verified device-creation chain. It records the
actual live factory vtable and CreateSwapChain method owner without changing
that factory. This is the last missing identity needed before a versioned
early factory interception can be prepared. No game was started or controlled
by the assistant. The next runtime action is one user-started normal Skyrim
session to collect this log; the next implementation action is the controlled
factory/GetBuffer proxy route with a WARP fixture where an inner consumer
caches GetBuffer before the outer creation returns.

0.1.34 MO2 diagnostic deployment: source commit `b9df212`. Release build and
all 30 CTest groups passed. Package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.34-owned-scene-prep-b9df212.zip`
SHA-256 `f449b48636b0823f67226e57acc5f84a3fbc05947297eac8cd960530a2359567`
contains exactly four manifest-listed payloads plus its manifest, independently
ZIP hash-verified. SkyrimSE.exe was absent. The previous installed four files
matched the old manifest before the full mod folder was copied to ignored
`artifacts/local/mo2-install-backup-0.1.34-b9df212`. Only the new DLL and
manifest were replaced. All four installed files match the new manifest;
installed DLL SHA-256 is
`1c531a0a4f470552715933fd537e5ae19cc486f2c20940a6026078fa413d69ab`.
The user's INI and signed NVIDIA SR runtime retain their prior hashes. No game
run of 0.1.34 is claimed. It logs the factory identity for the owned route but
does not activate that route or change the renderer client rectangle.

0.1.34 user-started result: the adapter-parent factory is the installed
ReShade `dxgi.dll` (SHA-256
`059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc`),
table RVA `0x3D79D0`, `CreateSwapChain` slot 10 at RVA `0x13A5B0`.
Read-only process inspection found the ENB outer swap delegating through a
ReShade swap with table RVA `0x3D7F90`, `GetBuffer` slot 9 at RVA `0x13B460`.
The ENB immediate context's wrapper table and PS resources/OM targets/viewport
slots were also identified. The log reached at least 25,800 world/Present calls
without a reported Present failure in the captured slice. It did not submit
NGX in that slice; reduced Skyrim SR remains NOT RUN. See
`re/OWNED_SCENE_LIVE_CHAIN_1170.md`. The assistant did not start, control or
close Skyrim.

0.1.35 source candidate: exact ReShade factory and swap/GetBuffer method
profiles validate SHA-256, file size, mapped image size, table RVA, slot pointer
and method prologue. A process-lifetime, owner-checked factory slot-10 callback
is installed before forwarding the existing ENB device-creation call. It
preserves native creation and records the returned nested swap. If that swap
matches the verified ReShade slot-9 profile, a second pass-through callback
records GetBuffer caller module/RVA and returned texture extent. These hooks
are diagnostic; the stable reduced surface route and renderer rectangle/UI
adapters remain dormant. A WARP test validated the stable reduced texture
alias versus native forwarding; a separate WARP test exercised the actual
IDXGIFactory CreateSwapChain ABI and preserved result. The refactored NGX
session returned a 1707x960 Quality plan for 2560x1440 and evaluated 30
synthetic-guide frames on the RTX 4080 SUPER with zero fallback. This is a
GPU replay, not Skyrim's scene. Release build and all 33 CTest groups passed.
Skyrim 0.1.35 runtime is NOT RUN; DLSS SR is still not on in Skyrim.

0.1.35 MO2 diagnostic deployment: source commit `dcc7d26`; package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.35-factory-trace-dcc7d26.zip`
SHA-256 `cd1a90dcb102e93b173ba1a9bce5138555546e19f27732f1c2eef6966fbe1482`.
Independent ZIP inspection found exactly the four manifest-listed payloads and
manifest; every ZIP/staged file matched its SHA-256. SkyrimSE.exe was absent.
The previous installed four files matched their manifest and the complete mod
folder was copied to ignored
`artifacts/local/mo2-install-backup-0.1.35-dcc7d26`. Only the DLL and
manifest were replaced. The installed four files match the new manifest; DLL
SHA-256 is `320d3493f21e352587d29d0e053baf822aba2642e478d2a6849953c78e9e1274`.
The user INI and signed NVIDIA SR runtime hashes are unchanged. The assistant
did not start or close Skyrim. The exact next runtime action is one normal
user-started game session; inspect `Nested factory CreateSwapChain` and
`Nested GetBuffer` log entries to establish the early consumer path. Reduced
Skyrim scene rendering and DLSS SR remain NOT RUN.

0.1.35 user-started runtime result: the verified ReShade factory slot-10 hook
observed its native 2560x1440 swap returned to ENB and installed a pass-through
slot-9 trace before ENB continued. The first two ReShade `GetBuffer(0)` calls
came from ENB `d3d11.dll` and received native buffers. The third was Skyrim's
exact game return RVA `0xE4CC87`, confirming that its early view-cache setup
traverses the same hook after ENB initialization. Read-only live memory matched
the game's indexed renderer record-zero swap with the ENB outer swap and found
non-null cached view fields. A world-like depth frame and over 2,400 native
DLAA display submissions followed without reported skips. The user authorized
closure after collection; the assistant sent a normal window-close request
and verified process exit. No reduced alias was enabled and **DLSS SR is not
on**. The game render lock moved between two threads, so the current fixed
thread guard in the dormant UI/rectangle adapters must become frame-scoped
before activation. See `re/OWNED_SCENE_LIVE_CHAIN_1170.md`.

Follow-up source after the 0.1.35 run: the dormant buffer route now selects
only the exact verified Skyrim return address `0xE4CC87`, selected ReShade
swap instance, buffer zero, `ID3D11Texture2D` IID and game hash. All other
callers, including ENB and RazKolbas, forward to the native buffer. The
world-domain policy and rectangle/UI adapters now accept a new render-thread
owner at each real-frame begin instead of assuming the first render thread is
permanent. WARP buffer-route and worker-thread rectangle tests passed;
Release build and all 33 CTest groups passed. These components are **not
activated or installed in the game**. Native UI context hooking, per-buffer
destination selection, coherent scene/guide production and same-frame NGX
publication remain required before enabling the alias.

The dormant native-UI adapter can now replace its verified display-sized RTV
for each flip-buffer frame during Processing. A WARP two-frame test published
UI to a different native target on frame two while the world returned to the
reduced scene before that publication. This is source-only preparation for
the observed three-buffer flip chain; it has not been hooked into ENB's live
context or run in Skyrim.

After the 0.1.35 trace, the owned SDR input preparer now accepts a truly
render-sized colour texture with motion and depth guides that are both either
render-sized or display-sized. It copies only the valid top-left render region
and rejects inconsistent guide extents. A WARP readback test verified colour,
motion and normalized depth pixels for reduced colour plus native-sized
guides; the Release build and all 33 CTest groups passed. This fixes an input
contract that would otherwise reject the planned reduced alias. It is
source-only: the installed MO2 build remains 0.1.35, the alias is not armed,
and Skyrim DLSS SR remains **NOT RUN**.

The source now has a strict current-native-flip-target resolver. It queries
`IDXGISwapChain3::GetCurrentBackBufferIndex` on the outer swap, obtains that
buffer through the same wrapper, validates the native SDR extent and device,
and creates its RTV. A three-buffer WARP flip fixture tested acquisition
through three successful Present calls and rejection of an incorrect display
extent. The fixture reported index zero throughout those calls, so it did
not prove index rotation; the real ENB/ReShade wrapper and rotating native
target remain **NOT RUN**. The resolver is not yet wired to the UI adapter.

An owned UI context installer now validates the exact local ENB immediate
context table, module SHA-256/file size, both downstream method pointers and
prologues before applying slot-33 `OMSetRenderTargets` and slot-44
`RSSetViewports` CAS hooks. It pins the callback and owner modules, retains
the downstream chain, and leaves hooks pass-through until fully installed;
the redirector itself remains dormant outside a published native-UI phase.
The Release DLL builds. No caller activates this installer yet, and neither
slot was modified in the installed 0.1.35 game build.

The exact Skyrim `Renderer::Begin` `GetClientRect` call at RVA `0xE4471B`
now has a source-level pass-through installer for the next candidate build.
It checks the executable hash, 73-byte decoded caller, six original CALL
bytes and User32 import pointer; `applyRipCall6` then targets a dedicated
nearby read-only callback cell at the SKSE startup boundary. The callback
continues to return the native rectangle until the complete owned route is
activated. Its patch contract and the ENB UI slot contract are recorded in
`patches/skyrim/`. The Release DLL builds; neither hook has run in Skyrim.

0.1.36 source candidate: the device-creation callback now stages the owned
route before Skyrim's verified view-cache `GetBuffer` return. It requires a
current native flip target, a reduced plan from the same NGX presenter that
will evaluate, an owned reduced SDR texture, the exact ENB UI context hooks
and the exact renderer rectangle hook before atomically arming the selected
ReShade buffer alias. Only the one verified Skyrim caller receives that
alias; ENB and RazKolbas requests remain native. At the post-world callback,
the candidate copies reduced colour and coherent guides, binds the current
native RTV for DLSS evaluation or same-frame spatial fallback, and moves
cached UI bindings to native resolution only after a display image is queued.
Before a same-thread ResizeBuffers call it retires its native RTV and reverts
to pass-through until restart. A combined WARP fixture caught an invalid
output-binding order and now passes reduced input, spatial publication,
native UI remapping and GPU retirement. The Release build and 35 CTest groups
pass. **No 0.1.36 Skyrim run has occurred; actual alias acceptance, reduced
scene pixels, NGX submissions, ENB/ReShade placement, UI behavior and resize
remain NOT RUN.** The installed MO2 build remains 0.1.35 until package
deployment below.

0.1.36 MO2 candidate deployment: source commit `6ee6b53`; ZIP
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.36-owned-sr-6ee6b53.zip` SHA-256
`ac1ba7a2878968e89ed77f69469a484d6ee9ef33ab2edce83cfc98e390755f17`.
Independent ZIP inspection found exactly the four manifest-listed payloads
plus manifest, and all ZIP payload hashes match. SkyrimSE.exe was absent.
The prior 0.1.35 payloads matched their manifest and the full mod folder
was copied to ignored
`artifacts/local/mo2-install-backup-0.1.36-6ee6b53` with hashes verified.
Only `RazKolbas.dll` and `install-manifest.json` were replaced in
`D:/TESV_EX/MO2/mods/RazKolbas`. All installed payloads match the 0.1.36
manifest. DLL SHA-256 is
`d42c3240bc478bc075975906f2cd9164970b585844a47c173a627e0af06e653c`;
the user's INI remains
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and the signed NVIDIA SR runtime remains
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The assistant did not start Skyrim. The installed candidate's actual alias,
DLSS SR, fallback, UI and resize results are **NOT RUN**; the next action is
one normal user-started Skyrim session through MO2, then inspect
`RazKolbas.log` for `Owned scene route armed` and `Owned world frame` evidence.

## 0.1.36 live failure and rollback (September 20)

The user started Skyrim with the 0.1.36 MO2 candidate and reported that the
End diagnostics menu opened over a black screen. The menu's 0.6 scale figure
was a reported value, **not** a visible reduced scene. `RazKolbas.log` proves
the exact Skyrim view-cache `GetBuffer` received the 1707x960 owned scene
instead of the 2560x1440 native buffer. The first post-world frame published
the spatial fallback (`mode=3`) with zero DLSS provider submissions. On frame
two the native-UI redirector recorded an unsupported render-target/depth
layout and the owned route suspended. Present continued successfully through
at least 24,000 calls; this is a black-image failure, not a verified crash.
The log did not record why the DLSS submission failed, the exact incompatible
UI bind, or whether the owned scene and native output contained non-black
pixels. DLSS SR visible operation remains **FAILED/NOT PROVEN**.

After collecting the log, the assistant sent WM_CLOSE to the user-started
Skyrim process and verified it exited. The MO2 `RazKolbas.dll` and manifest
were restored from the pre-0.1.36 backup. All four installed payloads match
the restored 0.1.35 manifest; DLL SHA-256 is
`320d3493f21e352587d29d0e053baf822aba2642e478d2a6849953c78e9e1274`.
The user's INI and signed NVIDIA SR runtime were not changed. The assistant
did not start Skyrim.

The next source candidate records the first provider rejection, the first
incompatible UI bind (phase, frame count, thread, target count/slot and depth
extent), and a bounded first-frame 16x16 pixel sample of both the reduced
scene and native output. No raw capture is saved by this probe. Release build
and 35 CTest groups pass. These diagnostics are **NOT RUN** in
Skyrim; no UI translation or image fix is claimed until the bind and pixels
are observed.

The 0.1.37 diagnostic candidate is source commit `6d702b1`. Package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.37-black-screen-diag-6d702b1.zip`
has SHA-256
`c22c350903926decba5b69b844c400ec345830e9bc97f5b4376e45eeadbc3be5`
and exactly the four manifest-listed payloads plus manifest. After confirming
Skyrim was stopped and all old files matched the restored 0.1.35 manifest,
the full mod folder was backed up to ignored
`artifacts/local/mo2-install-backup-0.1.37-6d702b1` with hashes verified.
Only the MO2 DLL and manifest were replaced. All installed payloads match
the 0.1.37 manifest; DLL SHA-256 is
`44820e26186c682c6541a66347dc77e95796f39e2dd6e81af62a67e2f2e8bdf8`.
The INI and signed NVIDIA runtime remain byte-identical. The assistant did
not start Skyrim. The diagnostic build is **NOT RUN** in Skyrim; next action
is one user-started menu run, read the new bounded log records, close
the game and restore 0.1.35 if the screen remains black. No save load is
needed for that diagnostic.

## 0.1.37 live diagnosis and rollback (September 20)

The user started Skyrim with the 0.1.37 diagnostic build. The first owned
post-world frame again selected spatial fallback, with zero DLSS submissions.
The precise provider failure was `Game camera and DLAA target extents differ`:
the owned path validated camera jitter against the **display** extent even
though the renderer rectangle had been reduced. A 16x16 stratified sample
of both the 1707x960 owned scene and 2560x1440 native output had zero
non-black RGB pixels and one distinct RGBA value at this callback. This
measures the sampled post-world surfaces only, not all pixels or the later
pre-Present image. The next bind after this callback was the owned scene in
RTV slot zero with **two render targets and a 1707x960 depth target**;
it occurred while the route was still in NativeUi phase, after one world
callback and before Present. The UI redirector correctly treated this
layout as unknown and suspended the route on frame two. This is evidence
that the candidate's publication boundary is too early or its phase
assumption is wrong; it does not yet identify the final colour-production
boundary. Present remained successful. After collecting the evidence, the
assistant closed the user-started game and verified exit, then restored the
0.1.35 MO2 DLL and manifest from the verified backup. All four installed
payload hashes again match the 0.1.35 manifest. The assistant did not start
Skyrim.

The next source candidate validates owned camera jitter against the render
extent and uses the resulting render-pixel offsets directly. It also records
the first camera extent and samples the owned scene/native buffer before
the first two Presents, after the callback, to locate when image pixels
appear. The pre-Present probe requires the renderer lock and is bounded to
two frames. It does not write captures or change pixels. Release build and
35 CTest groups pass; actual DLSS evaluation and visible output are
**NOT RUN** for this source candidate. No timing or UI remapping fix is
claimed until the additional boundary evidence is observed.

The 0.1.38 present-probe candidate is source commit `e3424d2` and package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.38-present-probe-e3424d2.zip`
(SHA-256 `3fd4e3387192e17a579ca256ce24df298390a42d4b981dc993edb594134910be`).
The ZIP has only the four manifest-listed payloads plus manifest. Skyrim was
stopped, the restored 0.1.35 mod was verified against all manifest hashes
and backed up to ignored `artifacts/local/mo2-install-backup-0.1.38-e3424d2`.
Only the MO2 DLL and manifest were replaced; the installed 0.1.38 DLL
SHA-256 is `90043344aea45dfe5e3541318875b9685bab41d4dab7f9924ab67c950ce495ed`.
All installed payloads match the new manifest; the user's INI and signed
NVIDIA runtime are unchanged. The assistant did not start Skyrim.
Runtime result: **NOT RUN**. Next is one user-started menu session; inspect
the first two pre-Present samples and provider log, close the game, then
restore 0.1.35 if the image remains black. No save load is needed.

## 0.1.38 live result and pre-Present processing candidate

The user started 0.1.38 and again saw a black screen. The camera extent was
exactly 1707x960, confirming the render-space jitter correction. NGX
submitted one DLSS frame, but the 16x16 post-world sample of its input and
the native output were both all black. The first pre-ENB-Present sample,
after the two-RTV/reduced-depth bind, found 87/256 non-black reduced-scene
pixels and 49 distinct values; frame two found 255/256 non-black and 250
distinct. The native buffer remained black in both pre-Present samples.
Thus the reduced scene really is populated **after** the original world
callback, and the one submitted DLSS frame evaluated an unfinished black
image. These are sampled pixels, not a full-frame semantic/UI analysis.
The assistant closed the user-started game, verified exit, and restored the
0.1.35 MO2 DLL and manifest with all four hashes checked. No game was
started by the assistant.

The next source candidate defers owned SR/fallback evaluation from the
post-original world callback to the observed pre-ENB-Present callback. It
keeps the domain in World phase while the reduced scene is populated, so
the intervening two-RTV/reduced-depth bind is forwarded normally; after
publication it closes the frame to Dormant before the next Renderer Begin.
This candidate processes the **whole reduced frame including game UI**.
It aims to restore a visible image and test DLSS on non-black input; it does
**not** meet the final native-resolution UI requirement. A separate, verified
scene/UI boundary remains necessary. Release build and all 35 CTest groups
pass; visible output, colour quality, UI behavior and continuous submissions
are **NOT RUN** for this source candidate.

The 0.1.39 pre-Present SR candidate is source commit `eb463cd`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.39-present-sr-eb463cd.zip`
(SHA-256 `d25e9599cabeda41cd3f966c771de2fcba52e786b7ff78c5d03b54e218c6239d`).
The package has only the four manifest-listed payloads plus manifest.
Skyrim was stopped. The restored 0.1.35 mod passed manifest verification
and was copied to ignored `artifacts/local/mo2-install-backup-0.1.39-eb463cd`
with matching hashes. Only the MO2 DLL and manifest were replaced; all
installed payload hashes match the 0.1.39 manifest. DLL SHA-256 is
`53b580cdd96159da9888339f2fdc77c01a2aefa97bfadf4cb2f96d947dc95e6f`.
The user's INI and signed NVIDIA runtime remain unchanged. The assistant
did not start Skyrim. Actual-game visible SR and stability are **NOT RUN**;
next action is a user-started menu run. If the image is still black, inspect
the first pre-Present source and destination samples, close the game, and
restore 0.1.35. If it is visible, inspect continuous submissions and UI
appearance before treating this as more than an interim whole-frame route.

## 0.1.39 crash and rollback (September 20)

The user reported a crash with 0.1.39. Three CrashLogger logs at 22:30:12,
22:31:04 and 22:32:42 show the same null-read access violation in
`nvwgf2umx.dll+0x1B61A4` on an NVIDIA worker thread. This does not by
itself prove which RazKolbas submission/resource caused it. In the latest
run, the pre-Present route submitted DLSS for frames one through three;
both the reduced scene and native buffer had non-black 16x16 samples, and
the first two Present calls returned success. The log ends after the frame
three submission, within the crash second. Thus pre-Present publication
produced non-black native pixels, but **continuous game SR is unstable**;
visible appearance was not confirmed by the user. The assistant did not
start any of these sessions. SkyrimSE.exe had exited when checked. The
assistant restored the MO2 0.1.35 DLL and manifest from the verified
pre-0.1.39 backup and checked all four installed payload hashes. The user
INI and signed NVIDIA runtime remain unchanged. Do not reinstall 0.1.39.

The next investigation must separate a menu frame with unverified depth/
motion guides from a valid loaded-world SR frame, and distinguish NGX
in-flight resource handling from pre-Present display writes. The current
source is known crash-prone and is not a release candidate. A bounded
fallback-only or depth-gated pre-Present experiment can isolate those
causes, but must be labelled as a diagnostic, not working DLSS.

The next source candidate keeps the verified pre-Present placement but
forces the spatial fallback for owned frames, so it does **not** submit NGX.
It samples the renderer's depth in the 1707x960 owned render rectangle on
frame one and every 600 frames, logging the established 10x10 world-depth
readiness statistics. This isolates the NVIDIA crash from pre-Present
source-to-native publication and lets a visible menu reach a loaded world
if the fallback is stable. A WARP two-frame owned-route regression and the
Release build pass; all 35 CTest groups pass. This is a diagnostic, not a
claim of DLSS SR. Runtime result is **NOT RUN** until a user-started game.

The 0.1.40 spatial-isolation package is source commit `100d5b7` at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.40-spatial-isolation-100d5b7.zip`
(SHA-256 `9a5781b73c1a1e4a0a435d3f7cf9e80c6e1a000e7a9773cd021d527b87ec0035`).
It contains only the four manifest-listed payloads plus manifest. Skyrim
was absent; the restored 0.1.35 mod passed hash verification and was backed
up under ignored `artifacts/local/mo2-install-backup-0.1.40-100d5b7` with
all payload hashes checked. Only the MO2 DLL and manifest were replaced.
The installed diagnostic DLL SHA-256 is
`baa011caa8e9458463a87d4534572aa6cbd3f9382a2d7b9f18699cb74fb33843`;
all four installed payloads match its manifest. User INI and signed NVIDIA
runtime remain unchanged. The assistant did not start Skyrim. Runtime
stability/visibility and depth readiness: **NOT RUN**. If the user runs it,
look for non-black native output, `providerSubmissions=0`, depth statistics
and any crash before drawing further conclusions.

## 0.1.40 loaded-save result and guarded NGX candidate

The user started Skyrim and loaded a save with the 0.1.40 spatial-only build.
The log reached frame 13800 with successful Presents, `providerSubmissions=0`,
one spatial fallback in flight, and no new CrashLogger log. Sampled depth was
clear/menu-like through frame 6000 (`distinct=1`, `nonFar=0`); from frame
6600 through frame 13800 it was consistently world-like (`distinct=95–97`,
`nonFar=94–96`). This isolates the previous crash from the pre-Present
fallback publication in this run, but does not prove that invalid menu guides
alone caused the NVIDIA worker crash. The user did not confirm visible quality.
The assistant closed the user-started game via its main window and verified
exit, then restored the 0.1.35 MO2 DLL and manifest from the verified backup;
all four installed payload hashes match the 0.1.35 manifest. The user INI and
signed NVIDIA runtime remain unchanged. The assistant did not start Skyrim.

The next bounded candidate replaces the forced spatial diagnostic with an
owned-world depth admission gate. It samples the actual reduced render
rectangle every 30 frames, requires two consecutive world-like samples before
submitting NGX, and falls back to spatial output when a sample is menu-like
or unavailable. A new resource generation resets readiness. This is a
hypothesis test for the 0.1.39 crash, not a demonstrated fix; motion guide
semantics and NGX in-flight lifetime remain open suspects. The focused gate
test passed, the Release build passed, and all 35 CTest groups passed. Actual
Skyrim DLSS stability, appearance and native UI separation are **NOT RUN** for
this candidate.

The guarded candidate is source commit `1b2abe5` and package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.41-depth-gate-1b2abe5.zip`
(SHA-256 `5d1d4971a511e21ab4300afb3294afefa83771850719f4d3940da382c5f0f85b`).
The ZIP contains only the four manifest-listed payloads plus manifest.
Skyrim was stopped. The restored 0.1.35 mod passed hash verification and
was backed up under ignored `artifacts/local/mo2-install-backup-0.1.41-1b2abe5`
with all payload hashes checked. Only the MO2 DLL and manifest were replaced;
all installed payloads match the new manifest. The installed DLL SHA-256 is
`c1caa2ad32fb159415f905c96b9b748f6991f61ddf39d490731eda6937b6d3f0`.
The user INI and signed NVIDIA runtime remain unchanged. The assistant did
not start Skyrim. Actual-game NGX stability/appearance: **NOT RUN**. The next
necessary action is a user-started menu and save-load test; inspect depth
admission, NGX submissions, native output and any new crash. The assistant
may close the game after collecting the result.

## 0.1.41 loaded-world crash and isolated follow-up

The user started Skyrim, loaded a save, and reported another crash. The
0.1.41 log showed spatial output with zero NGX submissions through frame
7200. At frame 7741 it recorded `distinct=97`, `nonFar=96`, and the depth
gate's first `NGX-ready=true`; the log ends there. CrashLogger recorded a
null-read access violation at the same `nvwgf2umx.dll+0x1B61A4` instruction
as the three 0.1.39 crashes, on an NVIDIA worker thread. The log cannot yet
distinguish feature creation, input preparation, evaluation, or asynchronous
driver work on that first admitted frame. This run rejects the hypothesis
that menu-like depth alone explains the crash; it does not establish that
the converted world depth or motion guide semantics are correct. Skyrim had
exited when checked. The assistant restored the 0.1.35 MO2 DLL and manifest
from the verified pre-0.1.41 backup, checking all four payload hashes. The
assistant did not start Skyrim.

A WARP test bound the native-sized writable depth target during owned input
preparation; the R32 crop preserved the expected depth pixels and restored
the DSV. This rules out a persistent DSV/SRV conflict in that isolated path,
not every game-context hazard. A standalone NVIDIA replay on the RTX 4080
SUPER passed 30 frames at the exact 1707x960-to-2560x1440 NGX plan with
native typeless depth. A second replay using the owned-scene helper, full-size
guides, cropped R32 depth and the same extents also passed 30 frames with
zero fallback. These synthetic-guide replays do not reproduce the live
Skyrim/ENB/ReShade device and hook chain.

The next source diagnostic separates live feature creation from evaluation:
after valid depth it creates the NGX feature, displays spatial output for
120 frames, then logs input preparation and the first evaluation boundary.
It is instrumentation to locate the fault, not a demonstrated stability fix.
The first Release build and all 35 CTest groups passed. After explicitly
pre-creating the feature, the exact-size owned-R32 standalone replay passed
30 frames with zero fallback. MO2 deployment and game runtime for this
diagnostic are **NOT RUN** at this checkpoint.

The staged diagnostic is source commit `348f16e` and package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.42-ngx-stages-348f16e.zip`
(SHA-256 `54d8318438e84a905c416d44d43334517d03aeb80784e7350f631a514c5e1995`).
The ZIP contains the four manifest-listed payloads plus manifest. With
Skyrim stopped, the restored 0.1.35 mod passed hash verification and was
backed up under ignored `artifacts/local/mo2-install-backup-0.1.42-348f16e`.
Only the MO2 DLL and manifest were replaced; all four installed hashes match
the new manifest. The diagnostic DLL SHA-256 is
`32b7b4773b596d95e79a91e61674e035f728aab2ad3c462744899f41a48f4c50`.
The user INI and signed NVIDIA runtime remain unchanged. The assistant did
not start Skyrim. Game behavior is **NOT RUN**. The next required step is one
user-started menu/save-load run; inspect whether the last stage log is before
creation, after creation during the 120-frame delay, before evaluation, or
after evaluation. This build can still crash and must be rolled back after
data collection if it does.

## 0.1.42 evaluation crash and prepared-input capture candidate

The user started Skyrim with 0.1.42 and loaded a save. The feature was
created at frame 6451 after world-like depth (`distinct=97`, `nonFar=96`),
then spatial output ran through the 120-frame observation interval. At frame
6571, input preparation and the first NGX evaluation both returned. Before
the frame could be logged as published, CrashLogger recorded the same
`nvwgf2umx.dll+0x1B61A4` null-read on an NVIDIA worker thread. This rules
out feature creation by itself as the immediate trigger in this run, and
localizes the live failure to evaluation-triggered GPU work or its immediate
publication path. The log does not prove which asynchronous command failed.
Skyrim had exited when checked. The assistant restored the 0.1.35 MO2 DLL
and manifest from the verified backup; all four payload hashes match. The
assistant did not start Skyrim.

The next candidate retains the previously stable spatial-only publication
and does **not** submit NGX. On the first world-like frame it prepares the
same reduced colour, motion and R32 depth textures that NGX would receive,
reads them back once and writes a manifest-hashed bundle under the user's
`SKSE/RazKolbasCaptures` directory. This enables a standalone replay of the
actual game inputs before another live NGX attempt. The bundle writer has a
targeted positive/duplicate-destination test. Release build and all 35 CTest
groups passed. Game capture and subsequent replay are **NOT RUN** for this
source candidate. Captures remain outside Git.

The capture-only diagnostic is source commit `0575f6e` and package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.43-input-capture-0575f6e.zip`
(SHA-256 `c9102f0c6aeaeff1123e1dcb772cb202d8d73bba1066235027b0b241b8fb63c8`).
Its manifest explicitly says `DIAGNOSTIC_CAPTURE_ONLY_NO_NGX_EVALUATION`.
The ZIP contains only the four manifest-listed payloads plus manifest.
Skyrim was stopped. The restored 0.1.35 mod passed hash verification and
was backed up under ignored `artifacts/local/mo2-install-backup-0.1.43-0575f6e`.
Only the MO2 DLL and manifest were replaced, and all four installed hashes
match the new manifest. The installed DLL SHA-256 is
`4545d3da99987503e9414a365fded5c722f5e948a5e60292817552008ec37911`.
The user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim. Actual-game capture result is **NOT RUN** until the user starts
the game and loads a save; inspect the one-shot capture path and manifest,
then close the game and replay the exact input bundle outside Skyrim.

## 0.1.43 capture-only crash, rollback and exact-input replay

The user started the 0.1.43 capture-only build, loaded a save and reported a
crash. The one-shot bundle at
`SKSE/RazKolbasCaptures/owned-sr-inputs-7484-6271-219164875` completed at
frame 6271, after two world-like depth samples (`distinct=97`, `nonFar=96`).
The last RazKolbas log entry says the bundle was saved with **no NGX
evaluation**. All three 1707x960, four-byte-per-pixel raw files have the
manifest's byte count and SHA-256. CrashLogger then recorded a null read in
`nvwgf2umx.dll+0x1B59E0` on a driver worker thread, a different driver RVA
from the 0.1.39/0.1.41/0.1.42 `+0x1B61A4` crashes. The stack has no
RazKolbas frame and does not identify the D3D11 command that faulted. This
run demonstrates that NGX evaluation is **not necessary** for a live-driver
crash in the current owned path; it does not prove the new input preparation
or readback caused it. Skyrim had exited when checked. The assistant did not
start it. The verified 0.1.35 DLL and manifest were restored from the
pre-0.1.43 backup; all four installed payloads match its manifest, including
DLL SHA-256 `320d3493f21e352587d29d0e053baf822aba2642e478d2a6849953c78e9e1274`.
The user's INI and signed NVIDIA runtime were unchanged.

The captured R32 depth has 1,638,720 finite values in `[0.07157,1]`, and
the two-channel half-float motion texture has 3,277,440 finite values in
approximately `[-0.000587,0.003514]`. Only 318 of 1,638,720 captured colour
pixels are nonblack. Since this was the first admitted world-like frame after
a loading transition, a black fade is possible; this single frame cannot
establish whether later scene colour would be visible. Earlier first-frame
probes found the reduced scene filled after the world callback, and the
0.1.39 pre-Present path did produce nonblack native output.

`RazKolbasSdrLivePresentation` now accepts `--captured-eval-only` and
`--captured-eval-publish` for this exact 1707x960-to-2560x1440 bundle. It
checks the complete manifest, dimensions, formats, sizes and hashes, uploads
the captured prepared RGBA8/RG16F/R32F textures on a separate NVIDIA D3D11
device, pre-creates the planned feature, evaluates one frame, forces GPU
completion by readback, optionally publishes to an active native-size RTV,
and retires the feature. Both modes passed with one NGX submission; the
evaluation and published output hashes matched
`3c26a7f8ad6f3c35e270700e7ef53dd4eb3b9690529977e2a08e88f71c354e42`.
This shows that the captured bytes are accepted in an isolated session, not
that the live Skyrim/ENB/ReShade context or frame lifetime is safe. The
Release build and all 35 CTest groups passed. Captured bytes, game logs,
binaries and crash logs remain outside Git. The next engineering step is to
isolate the capture-only live operations (preparation, readback and ensuing
spatial publication) and examine command/resource lifetime at the driver
boundary before deploying another game diagnostic. No new game launch is
needed to review the present evidence.

## Depth snapshot isolation candidate after the capture-only crash

The common operation in the 0.1.42 first evaluation frame and 0.1.43
capture-only frame is reduced input preparation. Before this change the
R32 crop shader sampled Skyrim's original typeless depth texture directly;
that resource may still be a writable DSV in the saved engine context state.
The preparer now enters its isolated context state, copies the full-size
depth into an owned SRV-only typeless texture, and samples that copy for the
reduced R32 output. It does not change Skyrim's depth pixels or DSV binding.
The existing WARP regression with the source DSV bound verifies the reduced
colour/motion/depth bytes and restored binding. A 30-frame NVIDIA owned-R32
replay with the full-size depth DSV bound submitted 30 DLSS frames at
1707x960-to-2560x1440 with zero fallback and output SHA-256
`2a2c8d5e458e090787ce03074589db10d9f35f2123a83a5e55e11f6a5646a550`.
Release build and all 35 CTest groups passed. This is a live-driver hazard
hypothesis, not a demonstrated Skyrim crash fix. The game-facing
capture-only mode still submits **no NGX**; a user-started save-load run is
**NOT RUN** for this candidate.

The diagnostic is source commit `5e454e9`, packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.44-depth-snapshot-5e454e9.zip`
(SHA-256 `31e15c55a3e1cb497004e7a4009489a628ab4576009589892923fddd8a17e06f`).
The archive contains only the four manifest-listed payloads plus manifest;
its status is `DIAGNOSTIC_CAPTURE_ONLY_DEPTH_SNAPSHOT_NO_NGX_EVALUATION`.
With Skyrim stopped, the complete 0.1.35 MO2 mod was hash-verified and backed
up under ignored `artifacts/local/mo2-install-backup-0.1.44-5e454e9`.
Only the installed DLL and manifest were replaced; all four installed files
match the 0.1.44 manifest. The installed DLL SHA-256 is
`e05dd53a0fa1c0aed7054d4b6f6352d86178aa2d641e9531982837ae61ce0baf`.
The user's INI and signed NVIDIA runtime are unchanged. The assistant did
not start Skyrim. This game diagnostic is **NOT RUN** until a user-started
save-load session; inspect whether the owned input capture completes and
whether any NVIDIA worker crash follows it. Restore the verified 0.1.35
backup after a crash. No DLSS SR success should be inferred from a stable
capture-only result.

## Reconstruction archive review and scene-colour admission

The user supplied `RazKolbas_DLSS_SR_Reconstruction.zip` (SHA-256
`9e8ec4e60cabeaf193fcf28e9a223be70b4b4cb978a5af6b2cec185158b73f20`).
It was treated as reference source, not as repository instructions, and was
extracted only under ignored `artifacts/local/`. Every file listed in its
`SHA256SUMS.txt` matched. Its MSVC x64 build against RazKolbas's pinned public
NGX headers succeeded, and both its portable contract test and Windows WARP
test passed. The archive explicitly does not supply the missing Skyrim/ENB
phase ownership or a ready SKSE plugin. Its four-global dimension switch is
an alternative engine-resolution mechanism; it was not combined with the
current owned-swap-buffer route because the archive itself warns against
activating two sizing mechanisms. Its negative motion-scale fixture is also
not imported as a live convention: current reference evidence observed
positive dimension-sized scale fields, while Skyrim motion direction remains
unverified.

The archive's useful integration invariant is that valid depth alone cannot
attest a current reduced scene. Reanalysis of the 0.1.43 captured colour found
only 318 nonblack pixels out of 1,638,720; its 16x16 stratified sample was
`nonBlack=0`, `distinct=1`, even though the same frame's depth was world-like.
The live admission path now reads the reduced RGBA8 scene and native depth in
one bounded readback every 30 frames. It requires two consecutive samples
with at least 16 nonblack and 16 distinct colour points as well as the existing
world-depth thresholds before any capture or NGX preparation. A failure or
resource-generation change clears readiness. This is a transition guard, not
proof that the scene is HUD-free or placed after the required ENB effects.

The new colour sampler and combined gate have positive, black-transition,
missing-depth and generation-reset tests. Release build and all 35 CTest
groups passed. Exact captured-input evaluation and publication still pass
with output SHA-256
`3c26a7f8ad6f3c35e270700e7ef53dd4eb3b9690529977e2a08e88f71c354e42`;
the 30-frame owned-R32 NVIDIA replay still passes with output SHA-256
`2a2c8d5e458e090787ce03074589db10d9f35f2123a83a5e55e11f6a5646a550`.
Actual-game stability and capture of a populated admitted scene are **NOT
RUN** for this source revision.

The capture-only diagnostic is source commit `81cc331`, packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.45-color-depth-gate-81cc331.zip`
(SHA-256 `d1c8ac7c390279aef505a7f3e72b1a18228f21ed5ffce6e6e84554f4a188ffd8`).
The ZIP contains exactly the four manifest-listed payloads plus manifest; its
status is `DIAGNOSTIC_CAPTURE_ONLY_COLOR_DEPTH_GATE_NO_NGX_EVALUATION`.
With Skyrim stopped, the installed 0.1.44 mod passed manifest verification
and was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.45-81cc331`. Only the DLL and manifest
were replaced. All four installed payloads match the 0.1.45 manifest; DLL
SHA-256 is
`2ac5ce6490459d808ac294029ee968ecdeb890da5370645f9fed2be8bdfac477`.
The user INI and signed NVIDIA runtime are unchanged, and the assistant did
not start Skyrim. The next required evidence is one user-started save-load
run: confirm that black transition samples remain gated, that a later
populated scene produces one capture, and whether the NVIDIA worker crash
recurs even without NGX evaluation.

## 0.1.45 populated capture and bounded live-evaluation candidate

The user started Skyrim with 0.1.45 and loaded a save. The combined gate
initially rejected menu/loading depth while colour was already nonuniform.
At frame 12601 it observed `colorNonBlack=219`, `colorDistinct=211`,
`depthDistinct=98` and `depthNonFar=97`, admitted the scene, and saved exactly
one input bundle without NGX evaluation. The process continued through more
than 15,000 world/Present frames with zero Present failures and no new crash
log. The assistant then closed the user-started game normally as previously
authorized. This is an actual-game PASS for the capture-only colour/depth
gate and depth-snapshot preparation in this run; DLSS was not submitted.

The captured colour is materially populated: 1,480,329 of 1,638,720 pixels
are nonblack. All colour, motion and R32 depth files match their manifest
sizes and hashes; all motion/depth values are finite. Both exact-input
standalone modes passed one NVIDIA evaluation, output readback, clean
retirement and optional native-size publication. Evaluation and publication
produced identical SHA-256
`ec7bbf14b71e507de649dec13844d0368b6180de8acaa2787c8e46298a4a4329`.
This validates the captured bytes on a separate D3D11 device, not the live
Skyrim/ENB/ReShade context.

The next candidate disables capture-only mode only after the combined gate.
It creates the planned feature, retains the existing 120-frame startup
interval, evaluates and publishes at most one live DLSS frame, then forces
all subsequent frames through the current spatial fallback. This bounded
diagnostic tests whether the populated colour gate plus owned depth snapshot
removes the first-evaluation driver crash before continuous SR is attempted.
It remains a diagnostic and can still crash; actual-game result is **NOT RUN**.

The bounded diagnostic is source commit `6c1a1f5`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.46-single-live-eval-6c1a1f5.zip`
(SHA-256 `df7c6365b24c57ca95d67281b791f1f9920929542f4704c0dd528235b43b6ac6`).
The ZIP contains exactly the four manifest-listed payloads plus manifest;
status is `DIAGNOSTIC_SINGLE_LIVE_DLSS_EVALUATION_AFTER_COLOR_DEPTH_GATE`.
With Skyrim stopped, 0.1.45 passed manifest verification and the complete mod
was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.46-6c1a1f5`. Only the MO2 DLL and
manifest were replaced. All installed payloads match the new manifest; DLL
SHA-256 is
`1093eea7f7bc77bc170823a159030e5b8af0355b82812e920cda799d6f8b7e3b`.
The user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim. One user-started save-load run is required; inspect the feature
creation boundary, one evaluation/publication record, later fallback frames,
and any new CrashLogger entry. Restore 0.1.45 or 0.1.35 after a crash.

## 300-frame live-evaluation candidate

The user requested a longer live test before starting the installed 0.1.46
single-frame candidate, so 0.1.46 remains **NOT RUN**. The diagnostic limit is
now 300 successful live DLSS evaluations after the same combined populated
colour/world-depth gate and existing 120-frame feature-start interval. Once
300 frames have been submitted, later frames return to the spatial fallback.
This run is intended to exercise sustained resource and command lifetime and
to make reconstructed output visible long enough to inspect. It remains a
bounded diagnostic rather than a continuous-SR release and can still crash.
Release build and all 35 CTest groups passed; actual-game testing is **NOT
RUN** until the user starts Skyrim and loads a save.

The 300-frame candidate is source commit `4c9c122`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.47-300-live-frames-4c9c122.zip`
(SHA-256 `8f1974f54d5d32a9abdddce0c2d66c490b3f07d32e7986926e6be1f84edb8815`).
The ZIP contains exactly the four manifest-listed payloads plus manifest;
status is `DIAGNOSTIC_300_LIVE_DLSS_FRAMES_AFTER_COLOR_DEPTH_GATE`. With
Skyrim stopped, the installed 0.1.46 files passed manifest verification and
the complete mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.47-4c9c122`. Only the DLL and manifest
were replaced. All installed payloads match the 0.1.47 manifest; DLL SHA-256
is `9067b95c1609d58346d971cb7ec44981561d666d353a7ca63a2f34568a1d2072`.
The user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim. The next required evidence is a user-started save-load run.

## 0.1.47 first-evaluation crash and serialized follow-up

The user started Skyrim and loaded a save with 0.1.47. The populated-scene
gate admitted frame 6391 (`colorNonBlack=222`, `colorDistinct=206`,
`depthDistinct=98`, `depthNonFar=97`). Reduced feature creation completed and
the 120-frame startup interval passed. At frame 6511 the prepared inputs and
first NGX evaluation call returned, but no publication completed. CrashLogger
then recorded the same null read at `nvwgf2umx.dll+0x1B61A4` on an NVIDIA
worker thread. No second DLSS submission occurred, so the 300-frame limit is
not implicated. The crash remains localized to asynchronous work triggered by
the first live evaluation or the immediately following publication commands.

The next diagnostic ends and flushes the evaluation event while the isolated
NGX context state is still active, waits up to five seconds for confirmed GPU
completion, and only then restores Skyrim's context state and publishes the
native-size result. It serializes the bounded 300-frame test deliberately to
remove overlap with the ENB/ReShade command stream. A timeout or device error
falls back without destroying in-flight resources. This is a diagnostic
workaround and actual-game behavior is **NOT RUN**.

The serialized candidate is source commit `eac9f17`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.48-serialized-300-frames-eac9f17.zip`
(SHA-256 `091b0dcd18dc0cc928a40456f8939e4a070480885584a89bbf9a8b2b2925ed01`).
Release build and all 35 CTest groups passed. Exact captured-input evaluation
and publication passed on the RTX 4080 SUPER with output SHA-256
`ec7bbf14b71e507de649dec13844d0368b6180de8acaa2787c8e46298a4a4329`.
With Skyrim stopped, 0.1.47 passed manifest verification and the complete mod
was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.48-eac9f17`. Only the DLL and manifest
were replaced. All installed payloads match the new manifest; DLL SHA-256 is
`f1a9f1c3744407ac2ddbb57ddeea9859f0ad27d91bb56a0cb2ebe87166de2d85`.
The user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim.

## ENB/ReShade stage verification candidate

The user's visual assessment of the short 0.1.47 interval before its NVIDIA
worker crash is that the modified image looked as though ENB was not working.
This is a material integration failure report, but it is subjective and no
matched screenshot or pixel capture exists. The installed 0.1.48 serialized
candidate has not produced a new game session, so its runtime status remains
**NOT RUN**.

The source history confirms that the owned route currently evaluates at the
outer ENB swap chain's Present entry because the earlier post-world callback
observed an unfinished black reduced image. It publishes the complete reduced
frame, including game UI, to the native buffer before calling ENB's original
Present. Whether ENB then transforms that native image in this exact proxy
chain has not previously been measured.

The next diagnostic also patches slot 8 on the already hash-, size-, table-
and prologue-verified ReShade 6.7.3 nested swap chain. Its pass-through
callback records two bounded pixel snapshots at nested Present entry, after
the outer ENB Present has entered its downstream ReShade call and before
ReShade processes/presents. Matching pre-ENB and nested-stage full-image
hashes would support the reported bypass; differing hashes quantify that ENB
changed the native target. The samples are armed only when the populated
colour/world-depth gate transitions ready. No evaluation, publication, UI,
or Present ordering is changed. Release build and all 35 CTest groups pass;
actual-game stage hashes are **NOT RUN**.

The stage-trace candidate is source commit `044398b`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.49-enb-stage-trace-044398b.zip`
(SHA-256 `1762423af7cb96b5013f09f0a08df801660eb394b68bf4891bf645fb2e17a94d`).
The archive contains exactly the four manifest-listed payloads plus manifest;
all extracted hashes match and its status is
`DIAGNOSTIC_ENB_RESHADE_STAGE_TRACE_SERIALIZED_300_FRAMES`. With Skyrim
stopped, every installed 0.1.48 payload matched its manifest and the complete
mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.49-044398b`. Only the installed DLL
and manifest were replaced. The new installed DLL SHA-256 is
`63b28cc3a7b6f652d0e9c34db4a57e511331cdbc2b3f443a814367167d4d5ff6`;
the user INI and signed NVIDIA runtime retain their prior hashes. The assistant
did not start Skyrim.

## User-started 0.1.49 result

The user started Skyrim and loaded a save with 0.1.49. The exact ReShade
nested Present observer installed alongside the owned GetBuffer route. At
world frame 10171 the populated-scene gate accepted colour and depth, and the
paired snapshots then measured two complete fallback frames across the outer
ENB Present implementation. In both pairs the reduced source stayed byte
identical while the native output changed:

- frame 1 native SHA-256 changed from `effd8f495b3a7cccc1afd6e0d4e0095c6b98e7e49ed551ca361e01201d967820`
  to `71ae393a5d01ad8f2ed6f1b2cd7e18b53997f9ab2fa07001d239e96508776b91`;
- frame 2 native SHA-256 changed from `64f38b030c85d1d2beb81d594362efe81e9a8ffcb88c22dd0e9a20143e303cc7`
  to `b4dd201016201dfa26c85fb305e0837ca75c655d810771c0bf1c49835beb090d`.

This proves ENB modifies the native target in the selected fallback path; the
reported visual difference is not explained by ENB Present being skipped.
It does not yet prove that the first DLSS-published image receives the same
transformation or that ENB receives every auxiliary resource it expects.

The serialized live evaluation also removed the prior driver crash in this
run. The first evaluation returned at world frame 10291, all 300 bounded DLSS
frames completed by frame 10590, and Skyrim continued beyond 12,000 world and
Present calls with zero Present failures. No new CrashLogger file appeared.
The assistant closed the user-started game through a normal window-close
request after collecting the authorized evidence.

The next source candidate rearms the same two pre/post-ENB snapshots when the
first provider frame is published. It changes no render ordering or DLSS
parameters and will distinguish ENB processing of actual DLSS output from the
already-proven fallback behavior. Actual-game DLSS-stage hashes are **NOT
RUN**.

The first-DLSS-frame trace is source commit `0aa85ba`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.50-first-dlss-enb-trace-0aa85ba.zip`
(SHA-256 `478d8cfc21940533be4287b0d1ddce943db7686d9b5f85203bf67151b8880257`).
The archive contains exactly the four manifest-listed payloads plus manifest,
and all extracted hashes match. With Skyrim stopped, every 0.1.49 payload
matched its manifest before the complete mod was copied to ignored
`artifacts/local/mo2-install-backup-0.1.50-0aa85ba`. Only the DLL and manifest
were replaced. The installed DLL SHA-256 is
`c2935750db83cfdb76a336c0aad1b9191792d316bc57431ea9fbe909fb012817`;
the user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim.

## User-started 0.1.50 result and continuous candidate

The user started Skyrim and loaded a save with 0.1.50. The populated-scene
gate admitted world frame 6871 and the first serialized evaluation returned
at frame 6991. The newly rearmed provider snapshots completed around that
first DLSS frame. The reduced source remained byte-identical across ENB with
SHA-256 `107bdae4e6c5385961bd0b7473107403cabffacba44097190b76243676525dda`,
while the native DLSS output changed from
`1966e90b3e577620e2960515e3d9e6e8494e6cd3e4d9d760a0ad994b93996ec0`
to `8633a54826fe57512235dd18027133709ead53cc7d9e15441ef3453ab2df0801`.
This proves the outer ENB implementation transforms the first actual
DLSS-published native image in this route.

Immediately after that post-ENB readback, CrashLogger recorded a null read at
`nvwgf2umx.dll+0x1B61A4` on NVIDIA worker thread 22172. This is the same fault
bucket as the earlier un-serialized evaluation crashes. No second DLSS frame
or bounded-completion record occurred. The only render-path difference from
the preceding 0.1.49 run, which completed all 300 serialized evaluations and
continued stably, was rearming the two synchronous full-image snapshot pairs
on the first provider frame. The evidence therefore isolates the added
provider-frame readback as the new trigger; the snapshots must not remain in
the running DLSS path.

The next source candidate removes provider-frame snapshot rearming and keeps
the serialized GPU-completion boundary that passed 300 live evaluations. It
also removes the 300-frame ceiling so successful DLSS SR remains active
continuously instead of returning to spatial fallback. The earlier bounded
gate-transition ENB snapshots remain dormant after their two samples. Actual
continuous in-game stability and performance are **NOT RUN**.

The continuous candidate is source commit `04854c1`, packaged at
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.51-continuous-serialized-dlss-04854c1.zip`
(SHA-256 `2003ccc47d05933dae66de5a816a93a6799f93a49189c36a331dca5d5c6f4baf`).
The archive contains exactly the four manifest-listed payloads plus manifest,
all extracted hashes match, and its status is
`EXPERIMENTAL_CONTINUOUS_SERIALIZED_DLSS_SR_ENB_VERIFIED`. With Skyrim
stopped, the 0.1.50 installation matched its manifest and was fully backed up
under ignored `artifacts/local/mo2-install-backup-0.1.51-04854c1`. Only the
DLL and manifest were replaced. Installed DLL SHA-256 is
`288d0ade48acac5337334b78a3e9878ba119a285aecdc8f2c0222e10644aa9c7`;
the user INI and signed NVIDIA runtime are unchanged. The assistant did not
start Skyrim.

## User-started 0.1.51 crash and pooled-resource candidate

The user started Skyrim and loaded a save with 0.1.51. The combined populated
colour/depth gate admitted world frame 18811 at 21:08:15, reduced feature
creation completed, and the first serialized evaluation returned at frame
18931 at 21:08:17. CrashLogger recorded an access violation five seconds
later at 21:08:22 on NVIDIA worker thread 28056. The fault is the same null
read at `nvwgf2umx.dll+0x1B61A4` seen in earlier failures. The last plugin log
entry is the first-evaluation return, so the exact failing DLSS frame was not
recorded. At the observed roughly 60 Hz cadence, the five-second interval is
consistent with reaching the old 300-frame boundary, but this is an inference
and not an exact submission count.

Code inspection found that the 0.1.51 owned route allocated and retired four
prepared textures, a full-size depth snapshot, depth SRV/UAV, crop constant
buffer and two D3D11 event queries for every successful frame. The 0.1.49
run had previously completed exactly 300 serialized evaluations, while the
unbounded 0.1.51 run crashed only after a similar interval. This supports a
resource/query lifetime or driver-object churn hypothesis; the in-game crash
does not yet prove that hypothesis.

Source commit `2df3b97` replaces that per-frame path with three persistent
prepared slots. Each slot retains its colour, motion, normalized depth,
output, full-size depth snapshot, crop shader/views/constants and one reusable
event query. A completed publication fence gates slot refresh, so input and
output resources are not overwritten while the GPU can still consume them.
Shutdown explicitly drains the three slots before releasing NGX and their
resources.

The new WARP regression observed the expected compile failure before the API
was implemented, then passed 360 evaluate/publish cycles while seeing exactly
three stable prepared resource sets. Debug and Release builds passed all 35
CTest groups. The production pooled route then completed 600 serialized DLSS
SR evaluations on the RTX 4080 SUPER using the captured 1707x960 owned-scene
shape and 2560x1440 output, with zero fallback frames, output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`,
and clean NGX shutdown. This crosses the preceding 300-frame isolated limit;
actual Skyrim/ENB/ReShade stability remains **NOT RUN** for this build.

The installed candidate is
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.52-pooled-continuous-dlss-2df3b97.zip`
(SHA-256 `466447c14bf691b785cf006b1cdbdbd7a894237433f924486bbc9033fe4873bc`).
Its manifest status is
`EXPERIMENTAL_POOLED_CONTINUOUS_DLSS_SR_PENDING_GAME_TEST`; the archive has
exactly the four manifest-listed payloads plus the manifest and every hash
matches. With Skyrim stopped, all installed 0.1.51 payloads first matched
their manifest and the complete mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.52-2df3b97`. Only the installed DLL
and manifest were replaced. Installed DLL SHA-256 is
`124570410a1c8b69d8b23f18754042f578d0f59e0beab756d001176f78b740a0`;
the user INI and signed NVIDIA runtime retain hashes
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
The assistant did not start Skyrim. The next required evidence is a
user-started save-load run held beyond the previous five-second failure
window.

## User-started 0.1.52 pooled-resource result

The user started Skyrim and loaded a save with the installed 0.1.52 build.
The process remained responsive and no new CrashLogger file appeared. The
combined scene gate admitted world frame 9541, the reduced NGX feature was
created, and the first pooled evaluation returned at frame 9661. Continuous
provider publication then reached 4,140 DLSS frames at world frame 13800,
with `fallbacksInFlight=0`. World-forwarded and Present counts matched, every
reported Present HRESULT was zero, and the Present failure count remained
zero.

This actual-game run sustained the pooled route for roughly 69 seconds after
the first evaluation, well beyond the five-second 0.1.51 failure window and
the earlier 300-frame boundary. It is an actual-game **PASS** for continuous
serialized evaluation/publication and the three-slot lifetime correction on
this RTX 4080 SUPER, Skyrim 1.6.1170, ENB and ReShade configuration. It does
not by itself validate temporal image quality or native-resolution UI
separation. After collecting the authorized evidence, the assistant sent a
normal window-close request; Skyrim exited without creating a crash log.

## Guarded pre-UI publication candidate

Exact-game RE ruled out the second apparent Skyrim swap-buffer caller at
`0xe48f0a`: its sole direct caller is the screenshot/export function at
`0x6532a9`, and the returned resource is passed to image serialization at
`0xe4d750`. The apparent reference-proxy flag stores at `0x1f0ebb`,
`0x1a05bf` and `0x1a1857` belong to separate font/UI backend objects, so they
do not provide a transferable native-UI state switch. The evidence and limits
are recorded in `re/SKYRIM_REFERENCE_OUTPUT_PATH.md`.

The source candidate now recognizes the verified ENB context transition where
the owned reduced surface is rebound as exactly one colour target with no
depth after the matching world callback. MRT/depth binds remain ordinary scene
work. A separate admission gate requires two populated colour/depth samples
at this exact transition. Until it admits, or if the transition never appears,
the 0.1.52 pre-Present publication path remains active. After admission, the
same pooled DLSS path publishes to the native flip target before forwarding
the bind; the existing UI redirector then routes that bind and matching full
viewport to the native target. The domain stays in `NativeUi` through Present,
where it closes for the next frame. Any incompatible later reduced MRT/depth
bind suspends the route instead of assuming unknown semantics.

The boundary classifier regression first failed because the API was absent,
then passed with checks for phase, exact reduced identity, single RTV and no
depth. Fresh Debug and Release builds each passed all 35 CTest groups. The
Release NVIDIA harness also completed 600 pooled 1707x960 to 2560x1440 DLSS
frames with zero fallbacks and output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`.

Actual-game boundary admission, native-resolution UI, ENB/ReShade appearance
and stability are **NOT RUN**.

Source commit `4321251` is packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.53-pre-ui-dlss-4321251.zip`
(SHA-256 `0aa5a114ba0ab90d4255e08f925e7cecb432d0bf35daf21b45c2c040bad9554f`).
The archive contains exactly the four manifest-listed payloads plus the
manifest; extraction and all payload hashes passed. With Skyrim stopped, the
installed 0.1.52 payloads first matched their manifest and the full mod was
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.53-4321251`. Only the DLL and manifest
were replaced. Installed DLL SHA-256 is
`3f1f7721d16baacb09c4fbf135839481b5840457123c1e92b36e532cb8554ed8`;
the user INI and signed NVIDIA SR runtime remain unchanged. Manifest status is
`EXPERIMENTAL_GUARDED_PRE_UI_DLSS_SR_PENDING_GAME_TEST`. The assistant did not
start Skyrim. The next required step is a user-started save-load run and
inspection for the `Owned pre-UI boundary admitted` log before assessing UI
sharpness, ENB/ReShade appearance and stability.

## User-started 0.1.53 black-screen result and rollback

The user started 0.1.53 and reported a black screen. The process remained
responsive, Present/world counts continued matching beyond 70,000 calls with
zero reported Present failures, and no new CrashLogger file appeared. At world
frame 4852, the guarded pre-UI gate admitted the first populated single-colour,
no-depth bind. Later in that same frame, the reduced scene was rebound with a
1707x960 depth target. The UI compatibility guard recorded the mismatch and
suspended the route at Present. This proves the admitted bind was an
intermediate scene transition rather than the final UI boundary. The partial
native publication followed by suspension explains the retained black image.

After collecting the log, the assistant sent a normal window-close request;
Skyrim exited cleanly. The complete pre-0.1.53 backup was verified, and the
installed DLL and manifest were restored to stable 0.1.52. Restored DLL
SHA-256 is
`124570410a1c8b69d8b23f18754042f578d0f59e0beab756d001176f78b740a0`;
all four installed payloads match the restored manifest. Source removes the
falsified activation path. A future native-UI implementation must use a
semantically identified later hook or prove that no scene/depth work follows
before publishing.

The source rollback was rebuilt with both `win-dev` and `win-release`; all 35
CTest groups passed in each configuration. The release NVIDIA presentation
harness completed 600 pooled DLSS frames with zero fallbacks and retained
output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`.

## Semantic menu-display publication candidate

Exact Skyrim 1.6.1170 analysis identifies the menu-stack loop at Address
Library AE ID 82084. Its direct CALL at RVA `0xfa51cb` (ID plus `0x2cb`) runs
immediately before the loop invokes `IMenu::PostDisplay` through vtable slot
`+0x30`. Source now installs a coupled exact-byte relay there and preserves
all four Win64 arguments to original target RVA `0xe441c0`. The first call in
an owned World phase publishes DLSS/spatial output to the native flip target;
the `NativeUi` phase remains open while Skyrim draws menus and closes only at
pre-Present. Frames without a menu-display call retain the stable pre-Present
fallback. The domain rejects a new frame until the prior native-UI phase has
closed.

The caller, continuation, original target and CALL are exact-byte gated, and
both relays are prepared before the startup transaction writes either site.
Forwarding/order and phase-lifetime tests were added. Debug and Release each
pass all 35 CTest groups. The Release NVIDIA harness completed 600 pooled
1707x960 to 2560x1440 DLSS frames with zero fallbacks and output SHA-256
`9e24bc310a96dbeb827811488e7712457da160d242bd12370dbbdd05cdb65d37`.
Game runtime, native UI separation and ENB/ReShade appearance are **NOT RUN**
for this candidate.

Source commit `4d96fc0` is packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.54-menu-display-dlss-4d96fc0.zip`
with SHA-256
`b66861a11d0fd917075b499d1464aba32d0b5a2be880fddc9886c2fcf5a9c78f`.
Independent staging and ZIP inspection found exactly the four
manifest-listed payloads plus the manifest, and every payload hash matches.
With Skyrim stopped, all installed stable 0.1.52 payloads first matched their
manifest and the complete mod was backed up under ignored
`artifacts/local/mo2-install-backup-0.1.54-4d96fc0`. Only the DLL and manifest
were replaced. Installed DLL SHA-256 is
`a17ddd13943e62faeedd0b2481323f6e29fb05418765a01fc11e0f5d0de95ac7`;
the user INI and signed NVIDIA runtime retain hashes
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the 0.1.54 manifest. The assistant did not start
Skyrim. The next required evidence is a user-started save-load run.

## Live AIO and PDPerf reference inspection

The user enabled the preserved SkyrimUpscalerAIO Build16-Hotfix1 reference,
disabled RazKolbas, started Skyrim 1.6.1170 and loaded a save. Read-only live
inspection confirmed that the reference patches the same world-draw CALL at
game RVA `0xfa507a` and the renderer jitter CALL at `0xe44672`, then installs
resource-aware OM and viewport hooks into the loaded ENB immediate-context
chain. Its live state stored display 2560x1440 and render 1706x960 while the
sampled Skyrim DRS ratio tuple stayed entirely at 1.0. The reference therefore
owns reduced resources and viewport routing independently of the engine DRS
tuple.

Exact decompilation of the loaded binaries showed that the world callback
forwards Skyrim first, evaluates SR from explicit color, motion and depth
wrappers, publishes to a selected native proxy target, and enables a
resource-identity state used by the OM/viewport hooks. `PDPerfPlugin.dll`
provides a 176-byte evaluation descriptor ABI for both SR and frame
generation, along with swap-chain proxy, camera and Streamline/D3D12 interop
services. It remains a reference only and will not be packaged, loaded or
required by RazKolbas. Full evidence and the fallback implementation boundary
are recorded in `docs/re/AIO_LIVE_ROUTING.md`.

Skyrim was closed normally after capture. No RazKolbas code changed at this
checkpoint because the installed 0.1.54 semantic menu-display candidate has
not yet received its first runtime test. The next step is to disable the AIO
reference, re-enable RazKolbas in MO2, start Skyrim, load a save and observe
0.1.54. If that semantic boundary fails, the next source change will replace
bind-shape inference with explicit resource-role routing derived from this
live evidence.

## 0.1.54 black-screen result and observation-only correction

The user enabled RazKolbas, started Skyrim and reported a black screen with
the installed 0.1.54 menu-display candidate. The process remained responsive,
Present calls continued returning success, and no new crash log appeared.
Frame-one evidence is decisive: at the menu callback the 1707x960 owned scene
and 2560x1440 native buffer were both entirely black. After publication, a
same-frame bind used two color targets with the owned scene in slot zero and a
1707x960 depth target, triggering the compatibility guard. At pre-Present the
owned scene had become populated while the native buffer remained black; the
next frame's reduced scene was fully populated. The menu call therefore occurs
before complete scene/depth work and cannot be used as a publication boundary.
Skyrim was closed normally.

The installed DLL and manifest were restored from the verified pre-0.1.54
backup. Restored DLL SHA-256 is
`124570410a1c8b69d8b23f18754042f578d0f59e0beab756d001176f78b740a0`;
the user INI and signed NVIDIA runtime remain unchanged.

Source now keeps the exact menu CALL only as a bounded read-only marker for
the first twelve owned frames. It records only OM binds containing the owned
reduced scene and their immediately following viewport, with target/depth
extents and canonical identities. It never changes a target or viewport in
this observation phase. Actual SR publication has returned to the proven
pre-Present path and closes the frame immediately, matching the stable 0.1.52
behavior while collecting the resource sequence required for a resource-role
native UI route.

The new WARP regression verifies that observation records color-only,
viewport and color-plus-depth events without changing the bound scene or
depth. Debug and Release builds each pass all 35 CTest groups. The Release
RTX 4080 SUPER harness completed 600 pooled 1707x960-to-2560x1440 DLSS frames
with zero fallbacks and output SHA-256
`3a774c87b2cdc40de4a8fe0ef010cf445af38fc3657951fba25001261a442f70`.
Actual-game visibility and the bounded bind sequence are **NOT RUN** for this
correction.

Source commit `8092889` is packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.55-visible-resource-trace-8092889.zip`
with SHA-256
`513b8f3ae55aa72fb992318d345ee60f2a06f8887f781f49990cee7b2dc49435`.
The archive contains exactly the four manifest-listed payloads plus the
manifest, all extracted hashes match, and its status is
`DIAGNOSTIC_VISIBLE_PRE_PRESENT_DLSS_MENU_RESOURCE_TRACE_PENDING_GAME_TEST`.
The complete stable 0.1.52 installation was verified against its manifest and
backed up under ignored
`artifacts/local/mo2-install-backup-0.1.55-8092889`. Only the installed DLL
and manifest were replaced. Installed DLL SHA-256 is
`b86588a3baed350df4e7cfc24b608eb24f37a7bda09a76964b9d5c8182025cf9`;
the user INI and signed NVIDIA runtime retain hashes
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the 0.1.55 manifest. The assistant did not start
Skyrim. The next evidence is one user-started save-load run: the screen should
remain visible through the pre-Present path while the first twelve frames
record the resource sequence after the menu marker.

## 0.1.55 resource trace result and guarded native UI route

The user started 0.1.55, loaded a save and reported a visible image that did
not look like Skyrim Upscaler AIO. The assistant closed the responsive game
normally after collecting the trace; no new crash log was produced. Frames
1-12 each recorded the same eight events after the menu marker: one reduced
scene + alternating auxiliary MRT bind followed by three reduced scene-only
binds, all with the same reduced depth and each followed by a reduced viewport.
Exact identities and the comparison with AIO are recorded in
`docs/re/NATIVE_UI_RESOURCE_ROUTE.md`.

This run separately verified continuous real DLSS SR. The scene/depth gate
became ready and created the NGX feature at frame 6301, the first pooled
evaluation returned at frame 6421, and submissions reached 5,580 at frame
12,000 with no logged Present failure. ENB also changed the native output hash
at both sampled Present calls while leaving the reduced scene unchanged. The
user's visual difference is therefore consistent with 0.1.55's late
composition order rather than DLSS or ENB being inactive.

Source now allocates display-sized counterparts for the two learned auxiliary
RTVs and shared DSV outside the context callbacks. It preserves the visible
pre-Present route until the input gate is ready, the companions are prepared,
and at least one DLSS evaluation has succeeded. It then publishes at the menu
boundary and maps the observed scene, auxiliary, depth and viewport roles to
native size for the four late passes, closing the frame at pre-Present.
Unknown reduced attachments cause a compatibility fault and permanently
downgrade later frames to the stable pre-Present publication path; the cached
reduced scene stays owned and continues receiving display-sized publication.
The updated WARP regression covers two alternating auxiliary identities and
the depth/viewport mapping. Debug and Release CTest each pass all 35 groups.
The Release RTX 4080 SUPER harness completed 600 pooled
1707x960-to-2560x1440 DLSS frames with zero fallbacks and output SHA-256
`eff793f9f4695c298308e3e55310f5c6cf63f802761419c954adddebe698c255`.
Actual-game visual verification is pending.

Source commit `d64d0e8` was packaged as
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.56-native-ui-route-d64d0e8.zip`
with SHA-256
`6b3ca18b8763392d0ac551a8c837ca7c29085aebe4877d24dbc3bb5c57faa4b6`.
Independent extraction found exactly the four manifest-listed payloads plus
the manifest and verified every payload hash. The previous 0.1.55 mod folder
was verified against its manifest and copied to ignored
`artifacts/local/mo2-install-backup-0.1.56-d64d0e8`. Only the installed DLL
and manifest were replaced. Installed DLL SHA-256 is
`22f761cabb475169f372ac62c3d5abbb7b1626f00928aae85a667c88b9954f4a`;
the user INI and signed NVIDIA runtime retain SHA-256
`fb1e7c233a4581e2e931cc319348ec4d2a41d30c80fde1a18266559194dbefbf`
and `c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`.
All installed payloads match the 0.1.56 manifest. Skyrim was not running and
the assistant did not start it. Status is
`EXPERIMENTAL_GUARDED_MENU_BOUNDARY_DLSS_NATIVE_UI_PENDING_GAME_TEST`.
The next action is a user-started save-load test of visual parity, ENB/ReShade
ordering and native-size UI after the log reports native UI route activation.

## Current 0.1.140 loading-route checkpoint (2026-09-30)

The 0.1.135 game captures established that the native target changes from
bright artwork to nearly black during Loading Menu drawing, before Scaleform
EndFrame; the reduced scene and the later native stages were unchanged. The
0.1.136 FG-off spatial loading trial made both save and fast-travel artwork
visible again, but the user reported it was pixelated at the reduced render
resolution. This is a diagnostic result, not a final-quality repair. TRP's
spatial loading route is a design reference; no TRP code was copied.

The 0.1.138 and 0.1.139 FG-off background-restoration trials both failed the
game visual test: loading pictures remained dark. The immediate 0.1.139
readback proves that the copy briefly restored the previous bright artwork;
the Loading Menu then drew a different picture at very low brightness. The
Ghidra/Capstone call-site check also corrected a one-callback offset in the
earlier per-menu labels: the actual Fader call wipes the old target, and the
Loading Menu draws the new dim artwork. The Loading Menu movie still reports
a 1485x835 viewport while drawing to the 2560x1440 native target. Source
0.1.140 tested a display-size viewport for only that movie. The log confirmed
the changed viewport, but the user's save-load and fast-travel pictures stayed
dark; the native captures remained near black. The installed 0.1.140 INI now
selects the previously user-tested bright reduced loading route and switches
the failed restore/viewport probes Off. This workaround remains lower
resolution. A later user-started 0.1.140 run loaded the same save and
fast-travelled with that route active; the user reported both loading pictures
bright but pixelated. Both one-shot capture manifests are complete. The
reduced 1485x835 scene held legible artwork before menu drawing, the native
target did not change through Scaleform EndFrame, and the spatial pre-Present
pass published the artwork at 2560x1440. The one-shot capture key was switched
back Off after Skyrim closed; FG remains Off and the bright route remains On.
The next full-resolution investigation must locate the upstream artwork
producer or a native-size source, rather than adjusting the later Loading
Menu movie.
Release **64/64** and Debug **59/59** CTest groups passed; all 14 installed
payload hashes verified. **0.1.138, 0.1.139, and 0.1.140 native visual
trials: FAILED; full-resolution loading repair: OPEN; game FG-On: NOT RUN.**
Full capture and installation evidence is in
`docs/re/FG_OFF_VISUAL_REGRESSION_2026-09-30.md`.

0.1.142 FG source-boundary checkpoint: test-first `FgRealFrameBoundaries`
pairs verified post-world callbacks with the next real Present on the bound
swap and rejects menu/no-world, TEST, foreign swap, multi-world, same-frame
thread mismatch and resize-stale candidates. The opt-in, restart-scoped
`Diagnostics.ProbeFgFrameBoundaries` only logs bounded timing evidence; it
does not bind a Streamline frame token or enable FG. Release DLL/test build
and **65/65** CTest groups passed; Debug DLL/test build and **60/60** groups
passed. With Skyrim closed, 0.1.142 was installed in the V5.4 MO2 mod with
FG Off and the previous settings retained; all **14/14** installed payload
hashes match and the prior DLL/INI/manifest are backed up. **Skyrim 0.1.142
runtime boundary trace: NOT RUN; FG-On: NOT RUN.** Exact hashes and the next
user-started trace are recorded in
`docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.

0.1.142 user-started Skyrim trace: the verified world callback and real
Present paired through checkpoint 19,800 on the same swap, with zero
aggregate no-world/multi-world/thread-mismatch counts and zero sampled
Present failures. The owning thread changed between real frames while each
sampled frame remained single-threaded. Loading also produced boundary-ready
events even when the current output was spatial fallback with no DLSS input;
the boundary alone therefore cannot admit FG. Read-only log snapshot SHA-256,
per-phase evidence and limits are in
`docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`. **Game timing trace: RUN;
FG-On: NOT RUN.**

0.1.143 FG phase-candidate checkpoint: read-only callbacks now count the
verified renderer-entry relay and bracket the original world call before the
real Present. Tests reject missing, duplicate, out-of-order, cross-thread and
resize-stale phase candidates while preserving TEST/foreign-swap behavior.
Release and Debug DLL/test builds passed; CTest **65/65** and **60/60**.
With Skyrim closed, the new DLL and manifest were installed, the user's INI
remained byte-for-byte unchanged, and all **14/14** installed payload hashes
verified. This does not invoke Streamline or enable FG. Exact installation
hashes are in `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.
**0.1.143 game phase trace: NOT RUN; FG-On: NOT RUN.**

0.1.143 user-started Skyrim trace: 37 emitted phase samples through real
Present 20,400 all show one renderer entry, one world entry, one completed
world call and one Present in order on the same thread, with zero cumulative
phase rejections. Thread ownership migrated between completed frames. The
same timing occurs during loading, so FG still requires the separate loading
and resource/camera/UI eligibility gates. Read-only snapshot hash and scope
are in `docs/re/FG_REAL_FRAME_SESSION_2026-09-30.md`.
The user reports the same image but roughly six fewer FPS with the 0.1.143
phase trace On. Coarse log timing cannot establish the matched-view cause.
With Skyrim closed, an isolated comparison was installed by changing only
`ProbeFgFrameBoundaries` to false; the DLL and other INI settings are
unchanged, and all **14/14** installed payload hashes match. The one-key
byte comparison, backup and hashes are in the session document.
**Game timing trace: RUN; FG-On: NOT RUN; trace-off FPS comparison: NOT RUN.**

In the trace-off user comparison, FPS was still reported lower. The enabled
ledger is therefore not the sole cause; a matched numerical comparison and
root cause remain open. Trace-off log hash and the still-running disabled
observer stubs are recorded in the session document. Skyrim is closed again.
FG implementation continues with the trace setting Off.

0.1.144 queue-owned FG copy checkpoint: the private FG-Off presentation route
now retains one D3D12 copy allocator/list per physical lower buffer after first
use and uses the exact lower-swap queue's ordering rather than two CPU waits
per frame. Unproved bridge callers retain the prior synchronous path. WARP
blocked-queue, exact rotating-pixel, facade Present-boundary, resize and
uncertain-retirement tests passed. Full Release/Debug builds and CTest passed
**65/65** and **60/60**. The pinned ENB/ReShade game-route reproduction passed
240 Presents and resize both with and without Steam overlay; the synthetic
FG-On probe still observed generated frames. These are offline tests. An
unchanged-settings package and a one-key private-route On trial were staged;
with Skyrim closed, the latter was installed with FG Off and **14/14** payload
hashes verified. The previous DLL/INI/manifest are backed up. Exact hashes,
reproduction output and rollback location are in
`docs/re/FG_ASYNC_COPY_RING_2026-09-30.md`. **0.1.144 Skyrim visuals,
loading and FPS: NOT RUN; actual game FG-On: NOT RUN; six-FPS root cause:
OPEN.** Next is one user-started MO2 save-load test at the same viewpoint, then
restore the private-route Off package if the prior colour or FPS regression
persists.

0.1.144 user-started FG-Off private-route result: 0.1.144 and private facade
binding were confirmed in the live log, with successful outer Presents through
checkpoint 21,600. At the same view, the user reported the yellow colour
shift again and steady **50 FPS versus 60**. Fast-travel artwork was not
reported in this trial. The queued copy therefore did not resolve the
private-route colour/FPS regression. Once Skyrim was closed, the verified
0.1.144 private-route Off INI and manifest were restored; the DLL and all
other settings were retained and **14/14** installed hashes pass. Snapshot,
rollback hashes and limits are in `docs/re/FG_ASYNC_COPY_RING_2026-09-30.md`.
**Game private-route regression: REPRODUCED; game FG-On: NOT RUN; colour/FPS
root cause: OPEN.** Next is an offline ENB/ReShade final-colour boundary
investigation, followed by one targeted game diagnostic rather than another
blind private-route enablement.

Exact ENB 0.505 Ghidra/Capstone follow-up confirms real Present invokes ENB
processing before the nested swap's Present, while TEST bypasses it. Current
TRP source was compared against RazKolbas: TRP owns a stable game-facing
colour buffer and explicit effect/UI handoff before copying into rotating
presentation; RazKolbas still copies a hidden auxiliary swap buffer through
the automatic ReShade/ENB wrapper chain. The installed ReShade previously
rejected an ordinary typed texture's sRGB RTV, so direct buffer substitution
is not an established compatible fix. See
`docs/re/FG_ENB_TRP_COLOUR_BOUNDARY_2026-09-30.md`. **Final colour resource:
UNRESOLVED; next is an exact wrapper-boundary probe, not a tint patch.**

The exact ENB/ReShade/Steam private-route probe now shows that the upper
ReShade and inner facade `GetBuffer(0)` identify the same D3D11 texture; its
synthetic red pixel remains unchanged across the first Present. More
significantly, the exact ReShade log shows D3D12 and D3D11 effect runtimes
both loading the user's three-technique preset. This is a testable duplicate
effect hypothesis for the yellow image and FPS cost, not a measured Skyrim
cause. TRP's explicit source ReShade stage and isolated native output device
are the reference for a single-owner correction. See the RE note for probe
outputs and limits. **Game FG-On: NOT RUN; private route: restored Off.**

Branch review 37 was checked against `bafbc28`: all 12 archive member hashes
match, the two included phase-ledger Git blobs match, and upstream ReShade
6.8 source confirms that both D3D11 and D3D12 swap Presents independently
enter `present_effect_runtime`. This supports a dual-effect hypothesis but
does not prove both techniques ran over one Skyrim frame. The next bounded
offline diagnostic is per-runtime technique and stage-colour attribution with
isolated effects configurations. The archive was not added to Git; no game
change was made. See `docs/re/FG_BRANCH_REVIEW_37_2026-09-30.md`.

The exact-wrapper `effect-trace` probe now counts actual ReShade 6.8 technique
callbacks per runtime and synthetic frame. With the copied three-technique
preset active on both stages, two runtimes executed on 111/96 of the 120
frames before/after resize without Steam and 103/99 with the exact Steam
overlay preloaded. Empty-preset and D3D11-only/D3D12-only controls produced
the expected zero or single-runtime counts. The copied DLL/config/preset
hashes were restored to match the originals. This establishes duplicate
effect execution in the *offline reproduction*, not a measured double-colour
pass or FPS cause in Skyrim. See
`docs/re/FG_RESHADE_EFFECT_EXECUTION_2026-10-01.md`. **Installed private
route Off; FG Off; game FG-On: NOT RUN.** Next is ordered midtone pixel capture
at each effect/copy stage before changing the game route.
