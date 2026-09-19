# RazKolbas product and architecture specification

**Owner's target:** an independent Skyrim upscaler combining useful behavior of PureDark Skyrim AIO Build16-Hotfix1 and Fallout Upscaling Custom 1.65. Repository: `wallhead/RazKolbas`. This specification includes the owner's explicit permission to reverse engineer and byte-patch. The task is implementation, not another feasibility-only report.

## 1. Required scope

| ID | Requirement | Completion condition |
|---|---|---|
| R1 | Skyrim SE/AE support | SKSE loads RazKolbas; verified runtime profiles provide resource/camera/UI/resize hooks and a clean native fallback. |
| R2 | ENB support | Correct world/display separation, effects ordering, depth/history, native UI and lifecycle with tested ENB versions/presets. |
| R3 | ReShade support | Optional coexistence, selected effect placement, current-device/current-generation depth and no duplicate effect execution. |
| R4 | DLSS + FG + FSR + XeSS + DLSS 5 NR | Real SR providers; real FG providers where viable; genuine independent NR; combination validation; no RenoDX/ReShade dependency for core features. |
| R5 | GPU-aware configuration | Detect the adapter actually rendering Skyrim; query and validate individual capabilities; retain requested settings and show effective settings/reasons. |
| R6 | INI + ImGui | One typed schema, defaults, safe apply/reload/save, private overlay/context, live diagnostics and disabled-option explanations. |
| R7 | RE/byte-patching permitted | Use source inspection, static/dynamic RE, compatibility shims and versioned memory/file patches when technically justified. |

The target is not fulfilled by wrapping a reference host DLL, renaming Fallout's plugin, relabelling sharpening/Ray Reconstruction as NR, or returning fake success from a provider. Genuine unavailable capabilities must remain visible as unavailable while the required implementation work stays open.

## 2. Product identity and environment

Output `Data/SKSE/Plugins/RazKolbas.dll`. Configuration `Data/SKSE/Plugins/RazKolbas.ini`. Additional plugin-owned files under `Data/SKSE/Plugins/RazKolbas/`; vendor runtime roots are explicitly enumerated there. Log `RazKolbas.log` through the discovered SKSE log location. Never assume the user's local game path from another project.

Windows x64, C++20, MSVC, CMake presets/CTest, pinned package dependencies, SKSE/CommonLibSSE-NG, D3D11 capture with a companion same-adapter D3D12 processing device, private Dear ImGui context, and a typed INI layer. Choose concrete package/library revisions during T01 and commit the resulting lock data. There is no existing build system to preserve in the inspected empty repository; preserve one if the repository changes before execution.

Start with the locally available verified SE/AE runtime. Prefer AE 1.6.1170 as the first acceptance reference when available, then map/test SE 1.5.97. Other AE/GOG versions require their own verified profiles; VR and a multi-GPU implementation are expansion tracks, not assumed compatibility. These are proposed targets, not tested support statements.

Core functionality must run with the reference host DLLs, F4SE Menu Framework, ReShade and RenoDX absent. Reference hosts may be used in comparative local experiments. Optional ReShade integration may use its add-on interface from RazKolbas; it does not make ReShade a required host.

## 3. Ownership and frame graph

One `FrameCoordinator` owns selected providers, real frame IDs, render/display domains, resource generations, history resets, settings transactions and presentation mode. Backends do not each install their own competing Present hook. One active SR provider, at most one active FG provider, and a separate optional NR stage.

Initial integration graph, subject to a traced ENB stage:

```text
real game frame and camera/jitter update
  -> correctly sized world render
  -> validated ENB processing boundary, when active
  -> capture HUD-free colour + depth + motion + exposure + metadata
  -> optional ReShade before-SR slot
  -> optional NR before SR
  -> one SR provider, or native/spatial fallback
  -> optional NR after SR instead of before SR (advanced mode)
  -> optional ReShade after-SR slot instead of before-SR
  -> native-resolution UI plane/composition
  -> one presentation/FG owner
  -> real and generated displays
```

This graph is a design target, not an assertion about arbitrary ENB internals. Trace the active pipeline; split/reposition processing if required. A pre-UI hook is not inherently pre-upscale. Offscreen reflections, inventory 3D previews, map and loading/menu targets are separately classified; do not resize all textures uniformly.

D3D12 processing does not require converting Skyrim's full renderer to D3D12. Start SR/NR through validated D3D11 copyback where possible. Add D3D12-backed presentation for the chosen FG integrations. Select ownership at startup; a change that truly needs restart must be reported before Apply, not hidden behind successful-looking no-ops.

## 4. Contracts to define once

| Contract | Minimum data/behavior |
|---|---|
| `AdapterIdentity` | Actual D3D11 render adapter LUID; vendor/device IDs; driver/runtime identity. |
| `FeatureCapability` | Availability and validation are separate fields; raw provider result, effective compatibility path, requirements, limits and reason. |
| `FrameInputs` | Real frame ID, generation, render/display extents, valid rects, formats/sample counts, colour/exposure, depth and MV conventions, current/previous matrices, reset epoch and owned resources. |
| `FrameResult` | Success/failure, valid produced image identity, dimensions/encoding, submission state and retirement token; never imply valid output after a failed producer. |
| `RetirementSet` | Per-queue fences plus provider/presentation retention; a frame slot is reusable only when every required lifetime has ended. |
| `SettingsTransaction` | Requested settings, resolved settings, change category, replacement resources and rollback state. |
| `PatchDescriptor` | Exact version/signature/byte contract, named purpose, target/ABI, quiescence and rollback; see PATCHING_POLICY.md. |

These are RazKolbas-owned interface names, not claimed vendor API types. Define their concrete C++ declarations in the task that owns them. Keep platform-neutral policy code free of game/D3D headers so it can be unit-tested separately. Resources passed to providers must remain owned/retained for the provider's full use interval; raw borrowed globals are insufficient.

Use distinct guide semantics for SR, NR and FG. Verify motion-vector units, direction and jitter inclusion; depth representation/inversion; colour encoding/exposure; actual view/resource sizes; and whether UI is present. Do not silently stretch metadata to match an image.

## 5. Initialization and failure strategy

Split bootstrap into (A) settings/runtime selection and early interception outside loader lock, then (B) attachment to the actual renderer device/adapter and capability/context initialization. Streamline initialization order is an explicit T05/T11 investigation: its standard lifecycle requires early `slInit`, followed by device binding and rechecking available features. A manual-hook path must follow the selected version's documented/proven requirements. Do not casually initialize a global interposer after all devices/swap chains already exist and assume it worked. See SOURCES.md [W5–W6].

Keep native rendering unchanged until the replacement producer, sizing and jitter/TAA transaction are ready. On a normal per-frame SR failure, present a valid **display-sized** fallback produced from preserved scene colour; an undersized render texture is not a valid native-resolution fallback. Restore native sizing/TAA/jitter on a subsequent safe configuration transition when needed. NR-before-SR failure feeds original colour to SR; NR-after-SR failure keeps SR output.

A device-removed error is not repaired by choosing TAA. Stop unsafe submissions, preserve the error/removal reason and follow the viable game recovery/termination path. Never return artificial success forever or wait for a fence value whose signal was not submitted.

History reset is epoch-based and per provider. A reset arriving during an in-flight evaluation must not be consumed by that older evaluation. Generated presentations never advance real-frame or simulation state.

## 6. Resource and synchronization policy

Create the companion D3D12 device on the render adapter's LUID. Verify interface/format/shareability requirements, convert guides as required and keep shared handles under RAII. The reference `0x802` texture flags are evidence for a particular path, not permission to assume every format/DSV is shareable.

Use unambiguous directional synchronization and full retirement:

```text
D3D11 captures/converts -> signals input-ready -> submission/flush
D3D12 waits input-ready -> records/submits processing -> signals output-ready
D3D11 waits output-ready -> copyback/consumption -> records consumer completion
provider/presentation signals or documented retention completion
slot reuse only after every required consumer has finished
```

Track D3D12 states and explicit cross-API handoff. Do not reset command allocators, descriptor ranges, upload memory or shared textures solely because a GPU wait was queued. Fences that protect processing and fences that protect a later D3D11/UI/FG consumer are different responsibilities. Reuse safe slots; do not idle the entire GPU every normal frame. See SOURCES.md [W4].

Resize suspends processing, stops publishing depth, drains/retire dependencies, resizes the real underlying swap chain, rebuilds dependent resources/views, updates domains/UI, then resets history. Minimize/zero size is a suspended state, not a zero-sized dispatch. Existing external backbuffer references and proxy semantics need explicit handling; preserve actual HRESULTs.

## 7. RE and NR requirements

Use both exact archives and the matched Fallout PDB. Older source is guidance, not a complete representation of 1.65. Continue targeted RE to recover missing hooks, runtime initialization, parameter construction, input formats, branch conditions, return semantics and lifetime. Include indirect calls, delay imports, compatibility interception and shader code where relevant.

NR viability is an early independent track, not a last-stage checkbox. Establish a native D3D12 initialize/create/evaluate/release path for the exact runtime. Reconstruct an undocumented interface or introduce a version-specific shim/byte patch when evidence supports it. Lack of a public NR SDK is not by itself a project stop condition. Keep compatibility status explicit and test actual GPU results. Do not infer hardware support merely from a patched check.

Full-game NR starts single-pass, same GPU, before SR, without HUD and without RenoDX/ReShade. Advanced scope includes after-SR guides, model artistic controls, host resolve/transfer, reduced input scale and multipass once each contract has been verified. Missing advanced validation is an open task, not a reason to fake a slider.

## 8. Settings and UI

Use one schema for INI, menu, defaults, validation and persistence. Preserve unknown keys during round-trip where possible. Validate finite values, enums, ranges, dimensions and feature combinations. Requested values persist; effective values and reasons are separate.

Auto policy is a configurable, deterministic preference over usable providers on the actual renderer adapter: prefer the supported native-vendor SR, then validated cross-vendor paths, then native. Initial policy is Auto/Quality with FG and NR off. Vendor identity alone never grants support. Experimental compatibility profiles are explicit. SR and FG selection are independent; a pairing is enabled only after its input/presentation contract is validated.

Menu tabs: Overview, Upscaling, Frame Generation, Neural Rendering, Compatibility, Patches, Diagnostics. Show loaded module identity, GPU LUID, actual provider/model, render/output dimensions, requested/effective settings, patch IDs/status, feature reasons, source/displayed FPS separately, and reset/retirement diagnostics.

Apply/reload/save/reset actions use the same validator. Window/input callbacks enqueue changes; graphics work occurs at the coordinator boundary. Categorize live, recreate and restart-required options. Stage replacement resources before publishing a new generation. Do not destroy the last valid pipeline on a failed Apply. Saving preserves last-good data and reports errors. Own ImGui input/context; do not reuse another mod's ABI accidentally.

## 9. Definition of done

Every requirement maps to explicit tasks and tests. Track implementation, build validation and runtime validation separately. Unit/mock passes are not GPU passes; a capability-negative test can pass while feature rendering is NOT RUN. Validated experimental paths are labelled as such, not as official vendor support.

The final acceptance includes one end-to-end supported configuration with **Skyrim + ENB + ReShade + selected SR + selected FG + genuine NR**, plus the same core NR feature with both ReShade and RenoDX absent, then the advertised runtime/GPU matrix. Release is a reproducible MO2-friendly package, documented runtime installation, patch manifest, diagnostic bundle, dependency/provenance records and clear uninstall/restore instructions. No reference host dependency. No claims of tested FPS gains without measurements.
