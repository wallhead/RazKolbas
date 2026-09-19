# Acceptance matrix — RazKolbas implementation and runtime validation

**Status on 19 September 2026:** this is the acceptance plan for a new plugin. No row below is a claim of a passed game/GPU test. The research package contains static inspection evidence only. An implementer must record each row as PASS, FAIL, NOT RUN, or NOT APPLICABLE with a justification.

## Test record requirements

Each run records the source commit, build configuration/compiler, exact Skyrim executable/version, SKSE/CommonLib versions, OS/driver, physical GPU and renderer LUID, output mode, render/output extents, vendor runtime hashes, ENB/ReShade versions and enabled options. Include the logs/captures/measurements and the exact reproduction steps. A failed test cannot be replaced by a screenshot from a different configuration.

Keep three independent columns in the implementation status: implemented, build-tested, and runtime-tested. An unavailable test GPU is NOT RUN. A provider capability rejection can pass the negative test while its rendering feature remains NOT RUN.

## 1. Host, configuration and ownership

| ID | Test | Acceptance evidence |
|---|---|---|
| H01 | Clean SKSE install, all effects disabled | Native world/UI unchanged; no external SDK required to load safely. |
| H02 | Unsupported game runtime or changed hook bytes | No blind patch; precise log; original rendering preserved. |
| H03 | Existing hook/second upscaler/Community Shaders temporal owner | Validated chaining or explicit rejection; no double temporal evaluation. |
| H04 | Missing SDK, unsupported DLL version, initialization failure | Requested/effective difference and reason visible; no broken output producer. |
| H05 | Render GPU is not adapter zero | D3D12 companion LUID equals renderer LUID; no silent cross-GPU selection. |
| H06 | Parse/serialize defaults, unknown keys, invalid enum, NaN/Infinity | Deterministic validation/fallback; no crash or absurd allocation. |
| H07 | INI write denied/interrupted; last-good restoration | Previous valid file preserved; persistence failure visible. |
| H08 | ImGui/hotkey change requiring recreation or restart | Transaction applied at safe boundary, or restart clearly required; no fake success. |
| H09 | Settings migration from PureDark | Explicit enum conversion; source INI untouched; no auth/credential migration. |
| H10 | Failed settings transaction | Old pipeline remains usable; replacement resources safely retired. |
| H11 | ImGui/ENB/ReShade menu input transitions | No stuck input, cursor or key interception; contexts remain independent. |
| H12 | Real/generated frame accounting | Simulation, jitter and history advance only on real frames. |

## 2. Capture, interop and lifetime

| ID | Test | Acceptance evidence |
|---|---|---|
| R01 | Static/slow-pan/fast-pan camera and moving object | MV direction/units/jitter documented and visually verified. |
| R02 | Near/far geometry, sky, transparent surfaces | Depth convention, views and expected missing-depth cases documented. |
| R03 | Nontrivial render/output sizes and quality ratios | Colour/depth/MV extents match the submitted contract, no out-of-bounds work. |
| R04 | HDR/SDR/ENB colour formats and exposure | Encoding and exposure verified; no double gamma or clipping from a guessed format. |
| R05 | Synthetic D3D11→D3D12→D3D11 image round trip | Correct output and ordering under API debug validation. |
| R06 | Unsupported sharing tier or texture format | Rejected/converted through a supported path; no shared DSV assumption. |
| R07 | Slot ring wraps while GPU is delayed | Allocators/descriptors/upload memory not reused before retirement. |
| R08 | FG retains frame resources after real-frame submission | Reuse waits for the full provider/presentation lifetime, not just Evaluate. |
| R09 | Resource state transitions and handoff | Debug layer free of relevant state/ownership errors; explicit tested policy. |
| R10 | Repeated resize/quality switch/alt-tab/minimize/restore | Actual swap chain and dependent resources recreated; no stale views or zero-size dispatch. |
| R11 | Provider failure/cancellation after partial recording | No wait on an unsignaled fence; valid output or controlled shutdown. |
| R12 | Device removed/reset/hung | Original error retained; no infinite fake-success loop or claim that TAA repairs removal. |
| R13 | Menu/load/camera-cut reset during skipped evaluation | Reset stays latched until a successful evaluation consumes it. |

## 3. Upscaling, ENB and ReShade

Repeat applicable SR tests separately for DLSS, FSR and XeSS, using the actual available GPU/runtime capabilities.

| ID | Test | Acceptance evidence |
|---|---|---|
| S01 | Native-AA and each supported quality mode | Correct dimensions, jitter, mip bias and stable history. |
| S02 | SR provider switch and unsupported requested provider | Valid recreation or explicit restart; truthful effective fallback. |
| S03 | First/third person, hair/foliage, water, particles, moving NPC | No systematic smearing attributable to wrong inputs; limitations documented. |
| S04 | ENB off/on × ReShade off/on | Four separate records; world/UI extents and effects order verified. |
| S05 | ReShade before-SR versus after-SR mode | Exactly one intended effect execution; comparison capture proves placement. |
| S06 | ReShade depth-dependent effect with resize | Same-device, current-generation depth; correct dimensions; no stale publication. |
| S07 | Map/inventory/dialogue/3D previews/subtitles/fades/loading | Native readable UI; no improper scene scaling or duplicated effects. |
| S08 | Render-state restoration after SR/ReShade/ImGui | Following game passes retain expected targets, viewport, shaders and bindings. |
| S09 | ReShade absent/recreated/disabled | Core plugin remains functional; bridge lifetime handled safely. |
| S10 | SR evaluation fails | Existing valid image retained; log bounded; history reset pending. |

## 4. Frame generation

Run independently for DLSS FG, FSR FG and XeSS FG. Mixed SR/FG provider combinations are separate test configurations, not automatic support.

| ID | Test | Acceptance evidence |
|---|---|---|
| F01 | Capability-rejected GPU/runtime | FG unavailable with precise reason; SR/native still works. |
| F02 | One extra generated frame | Measured real/displayed frame identities and counts consistent with 2× target, subject to actual drops. |
| F03 | Higher/dynamic generated counts where supported | Provider limits honored; total multiplier not confused with extra-frame count. |
| F04 | HUD/translucency/crosshair/subtitles on generated frames | UI composition correct on intermediate frames, not only screenshots of real frames. |
| F05 | Camera cut/loading/menus/resize while FG active | No interpolation across invalid history; resources retired safely. |
| F06 | Low/variable real FPS, VSync, limiter and refresh changes | Pacing and latency measured; one coordinated policy; no stacked sleep controllers. |
| F07 | Required Reflex/XeLL path | Correct markers/lifecycle, no unsupported latency combination. |
| F08 | Provider failure/FG disable and subsequent SR frame | Presentation ownership remains coherent; no orphaned proxy or stale frame. |

## 5. Neural rendering without RenoDX

| ID | Test | Acceptance evidence |
|---|---|---|
| N01 | Exact-runtime native D3D12 harness | Genuine NR feature creation/evaluation/release with recorded contract and result semantics. |
| N02 | Verified native initialization and compatibility profile | Documented or reconstructed ABI works on the exact runtime. Required shim/byte patch is allowed, version-checked, reversible and backed by real create/evaluate/teardown evidence. Runtime provenance and raw versus modified probe results are recorded. |
| N03 | Skyrim NR with ReShade and RenoDX both absent | Actual NR output changes controlled by NR settings; no hidden add-on dependency. |
| N04 | NR before SR | HUD-free same-frame inputs; output feeds chosen SR; motion/history verified. |
| N05 | NR after SR | Correct high-resolution guide contract; not merely stretched low-resolution metadata. |
| N06 | NR disabled, zero transfer and failure cases | Defined identity/fallback behavior; original or SR image remains valid. |
| N07 | Tone/structure/style/mask/preset control | Setting affects the intended model/resolve parameter; unsupported controls disabled honestly. |
| N08 | Reduced input scale and host-side resolve | Colour space, reference image, ratio/residual behavior and dimensions verified. |
| N09 | Multipass where implemented | Independent/ping-pong resources, explicit history/reset ownership; timing/VRAM recorded. |
| N10 | NR + ENB + ReShade + SR + FG supported pairing | Complete combined pipeline recorded; isolated-feature passes do not substitute for this test. |

## 6. Performance and release

| ID | Test | Acceptance evidence |
|---|---|---|
| P01 | Repeatable baseline versus SR/FG/NR configurations | Same scene/camera/settings/extents/warm-up; raw results and methodology retained. |
| P02 | Frame-time decomposition | Capture, conversions, interop, SR, NR, UI and presentation/FG measured separately. |
| P03 | Extended play and repeated transitions | Resource/memory counts stabilize; no monotonic leak, deadlock or unbounded log. |
| P04 | Latency and pacing alongside displayed FPS | Report real FPS and generated/displayed FPS separately; no unsupported performance claim. |
| P05 | Clean MO2 installation and uninstall | No silent replacement of unrelated ENB/ReShade root DLLs; deliberate file patches have manifests/backups/restore. No Fallout/PureDark reference-host dependency. |
| P06 | Reproducible build and runtime manifest | Pinned dependencies, commands, hashes and notices; per-file runtime provenance and packaging permissions recorded. |
| P07 | Supported game/GPU/driver matrix | Only actually validated combinations advertised; remaining cells explicitly NOT RUN. |

## Suggested minimum run set

Begin with one physical GPU and game runtime for the safe host/capture/interop gates. Expand to representative NVIDIA, AMD and Intel hardware, distinguishing older supported analytical/cross-vendor paths from newer ML/FG capabilities. Include at least one deliberately unsupported configuration to validate rejection. The exact model list must follow the chosen SDK release and available test machines rather than a hard-coded generic vendor table.

For visual temporal validation, use deterministic camera paths where practical and inspect frame sequences, not only still images. Keep debug-validation runs separate from performance measurements because instrumentation changes timing.


## 7. Reverse engineering, patching and bootstrap

RE, private ABI reconstruction and byte-patching are authorized methods; this section verifies their engineering implementation rather than forbidding them. Apply the active `docs/PATCHING_POLICY.md`, not the older prompt's method restrictions.

| ID | Test | Acceptance evidence |
|---|---|---|
| K01 | Exact-hash/byte fixture patch and restore | Owned function returns 1 -> 2 -> 1; original and modified bytes plus protections recorded. |
| K02 | Wrong hash, changed original bytes, ambiguous signature | No write; precise failure and unaffected surrounding state. |
| K03 | Existing compatible and incompatible hooks | Correct proven chain or explicit rejection; original callee executes the intended count. |
| K04 | Partial patch-set preparation/application failure | No partial execution resumes; safe rollback, or controlled termination if rollback cannot restore a safe state. |
| K05 | Concurrent execution at target | Proven quiescent/transaction protocol; no torn instruction stream or reachable freed trampoline. |
| K06 | Another owner modifies our patched site | Rollback does not overwrite that owner or free code still reachable by the chain. |
| K07 | Disk dry-run/apply/restore | Input untouched on dry-run; backup/journal/result hash verified; correct original restored. |
| K08 | Already-applied patch, double restore and updated target file | Idempotent exact behavior; no blind writes into a different version. |
| K09 | Experimental compatibility profile on/off | Raw failure/probe, modified path and actual GPU outputs recorded separately. No invented hardware-support claim. |
| K10 | Safe mode and patch-specific disable | Startup avoids rendering patches in SafeMode; named disable affects only its documented dependency scope. |
| B01 | Provider bootstrap ordering | Trace proves selected initialization precedes the API/device stage it requires, outside loader lock. |
| B02 | Too-late provider load | Earlier validated interception or restart-required status; no falsely initialized feature. |
| B03 | Reconstructed private NR contract | Arguments, parameter factory/ABI, resources and result semantics match actual create/evaluate/teardown behavior. |
| B04 | Required native NR compatibility correction | Shim or byte patch fixes the isolated cause on the matching binary and passes rendering regression. |

## 8. Additional coordinator/retirement regressions

| ID | Test | Acceptance evidence |
|---|---|---|
| C01 | Reset arrives while older evaluation is in flight | Older success consumes its captured epoch only; newer reset remains pending. |
| C02 | SR fails at reduced render resolution | Valid output at display resolution through the fallback path; no unwritten or undersized output published. |
| C03 | D3D12 complete but D3D11 copyback consumer incomplete | Shared texture/slot is not reused until the later consumer completes. |
| C04 | API fences complete but FG provider retains resources | Slot remains retained for the provider's complete contract. |
| C05 | Submission aborts before signal | No queued wait is introduced for the unsubmitted signal; coherent failure state. |
| C06 | Frame count, resource generation and depth runtime mismatch | Invalid input is rejected before work is dispatched or a stale view is published. |

## Ownership of test status

Each run links a task T01–T24 from the implementation plan. A harness JSON report contains: case ID, PASS/FAIL/NOT RUN/NOT APPLICABLE, source commit, compiler/build, game version/hash where relevant, physical GPU/LUID, driver/OS, vendor module hashes, ENB/ReShade identity, active patch IDs and log/capture paths. `Run-Validation.ps1` may accept `-Provider DLSS|FSR|XeSS` for provider-specific cases. Its default is the configured provider; it must state which one actually ran.

A test that cannot execute produces NOT RUN with the missing prerequisite. Negative capability tests, successful builds and static disassembly do not substitute for positive graphics execution. The planning package itself claims no rows as runtime PASS.
