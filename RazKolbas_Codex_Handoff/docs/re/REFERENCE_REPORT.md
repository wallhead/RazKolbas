> **Historical technical reference, not the active implementation policy.**
> The report below was produced in the previous analysis. Its binary findings and uncertainty labels remain reference evidence; its proposed project name, roadmap, defaults, and any prescriptive method restrictions are superseded by `../../AGENTS.md`, `../SPEC.md`, `../PATCHING_POLICY.md`, and `../plans/2026-09-19-razkolbas-implementation.md`.
> RazKolbas explicitly permits reverse engineering, dynamic analysis, compatibility shims, runtime byte-patching, and controlled file patches. An undocumented/private interface is a research task, not a reason to abandon a required feature.
> Relative `evidence/` references below resolve to the accompanying original evidence. This copy is not a new reverse-engineering pass or a new GPU test.

# Skyrim Unified Upscaler
## Reverse-engineering findings and implementation specification

**Analysis date:** 19 September 2026  
**Target:** a native Skyrim SKSE plugin with ENB, optional ReShade, DLSS SR, frame generation, FSR, XeSS, and DLSS 5 Neural Rendering, without a RenoDX add-on dependency.  
**Status:** static binary/source investigation and implementation specification. No new game plugin has been built or executed as part of this investigation.

---

## 1. Decision

Build a new Skyrim host, not a renamed Fallout 4 DLL and not a wrapper that depends on PureDark's plugin.

The useful combination is:

- **PureDark's Skyrim integration as the game-specific reference:** Skyrim rendering hooks, render-resolution versus display-resolution handling, ENB/ReShade integration points, explicit UI handling, XeSS coverage, and the configuration surface.
- **Fallout 4 Custom 1.65 as the newer lifecycle reference:** a companion D3D12 renderer, a presentation proxy, fence-based resource retirement, resize/recreation handling, direct and Streamline NR paths, and explicit handling of NR failure.
- **A new implementation layer:** capability-based configuration, one owner of presentation and temporal processing, independent SR/FG/NR backends, supported SDK deployment, and a repeatable Skyrim compatibility test suite.

The shared NR runtime is a particularly useful finding: **both uploaded archives contain byte-identical `nvngx_dlssnr.dll` files**. They are not two different neural-rendering models that need to be merged. Most integration differences are in the hosts, resource preparation, scheduling, and composition.

Do not mistake that result for a finished integration. The exact Skyrim/ENB frame sequence, resource semantics, deployment contract for the private NR runtime, and real GPU synchronization still need execution-time validation.

### What was actually done

Both 7z archives were extracted without executing their contents. Their files were hashed. PE headers, versions, imports, delay imports, exports, exception ranges, strings, and selected x64 disassembly were inspected. The Fallout PDB was parsed and matched to its DLL through its CodeView GUID and age. Its symbols were used to identify current-release functions and trace selected direct calls and string references. The supplied GitHub source and current primary SDK documentation were inspected separately.

This is **not** a claim that every shader, indirect call, or proprietary SDK internals has been completely decompiled. A PDB function name identifies code; it does not by itself prove that code executes in a particular user's configuration. Static branches are not runtime traces.

### Evidence notation

- **B:** verified from the uploaded files, binary metadata, disassembly, or matching PDB.
- **S:** verified in the public source snapshot or primary documentation.
- **I:** interpretation supported by those observations, requiring runtime confirmation.
- **D:** proposed design, not a statement about either existing implementation.
- **U:** unresolved in this investigation.

The `evidence/` folder contains file manifests, hashes, a symbol index, selected call/string references, and short disassembly excerpts. The source references at the end distinguish the old repository from the newer uploaded release.

---

## 2. Exact inputs and provenance

### 2.1 Archives

| Input | Bytes | SHA-256 |
|---|---:|---|
| `SkyrimUpscalerAIOBuild16-Hotfix1(1).7z` | 207,492,573 | `09a0ff6810e87a951fa5a51f1905e030e69f7fe28b15251d46e3afa2f7e53b74` |
| `Upscaling Custom 108772 1.65 2026-09-13T17-12Z DzMlEf9Og(2).7z` | 162,790,599 | `792343d6ca89a06b06421a8180aa5815cb7d67aa0ee761065d65b8eb8afc5eb9` |

The extractor inventoried 25 regular files in the Skyrim archive and 32 in the Fallout archive. [B: `archive_hashes.json`, both manifests]

The Nexus description inspected during this investigation identified version 1.65, updated 13 September 2026. The supplied repository snapshot was pinned to:

`jarari/fo4test @ 0347ce28a17a580ff3ffcd1865aacb86ecc8b072`

**The repository is a reference, not the source of truth for the uploaded 1.65 binary.** [S1, S2]

### 2.2 Main binary identities

| Binary | SHA-256 |
|---|---|
| Skyrim `SkyrimUpscaler.dll` | `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb` |
| Skyrim `PDPerfPlugin.dll` | `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d` |
| Fallout `Upscaling.dll` | `f2633bcf11f618569eac77d1c7e6bf136a498fdb53f5eb1f205bfbb3a74cac7b` |
| NR runtime, identical in both | `8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206` |
| Fallout `sl.dlss_nr.dll` | `9f6672e5e0170dc118a3188d21bda187e1fc1aa3502895b21ab846d23165c11d` |

**Every address in this report is meaningful only for the matching binary hash.** An RVA is relative to its module. A preferred VA in the excerpts uses image base `0x180000000`; ASLR changes the loaded address. These are not Skyrim Address Library IDs.

### 2.3 The unusually valuable Fallout PDB

The 1.65 archive includes `Upscaling.pdb` (27,824,128 bytes). Its identity matches the DLL:

- GUID: `7057e107-1a03-4c53-ab14-7cd63ad16f3e`
- Age: `71`
- Both values match the DLL's CodeView record.

The parser extracted 11,983 symbol records, including library/template records and duplicates—not 11,983 distinct application functions. The delivered project symbol index filters that noise. The match provides a strong basis for associating the listed functions with **1.65**, rather than with the old GitHub source. [B: `fallout_pdb_identity.json`, `fallout_project_symbols.tsv`; methodology S15]

---

## 3. Important corrections and newly established facts

### 3.1 The earlier PureDark evaluation-size claim was wrong

Earlier notes described `EvaluateDLSSNR` as copying `0x140` bytes. Re-inspection of the exact uploaded binary shows:

```text
2 iterations × 0x80 bytes = 0x100
3 final vector copies × 0x10 = 0x30
1 final 8-byte scalar copy = 0x08
--------------------------------
Observed copied extent = 0x138 bytes (312), NOT 0x140 (320)
```

The final scalar copy is at preferred VA `0x18004FE80`; the backend call is at `0x18004FE8C`.

This proves the **copied byte extent of this export**. It does not recover an original C++ typedef, nor prove what future versions will accept. The independent implementation should not use either old number as a supposed public NVIDIA ABI. [B: `pd_nr_abi.asm`]

### 3.2 Resolve mode is at `+0x114`

The current binary establishes a three-part chain:

1. Skyrim reads the `mDLSSNRResolveMethod` INI key and stores it in host object field `+0x7C` (`0x1FB15F`–`0x1FB177`).
2. The NR caller loads host `+0x7C` and stores it at `[rbp+0x34]`. In this function the evaluation payload starts at `[rsp+0x20]`, and `rbp` is payload base `+0xE0`; therefore the destination is payload `+0x114`.
3. The PureDark helper at RVA `0x29E10` reads `[rcx+0x114]` and compares it with `2`, consistent with the configured ratio/colour-transfer mode.

Thus **`+0x114` is the resolve-mode field** in this observed path. `+0x104` was not resolved into that field; do not fill it with a guessed resolve enum. [B: `skyrim_resolve_setting.asm`, `skyrim_nr_caller.asm`, `pd_resolve_selector.asm`]

### 3.3 The old-source resize defect is not the current-release behavior

The pinned GitHub `DX12SwapChain.cpp` returns `S_OK` from `ResizeBuffers` and `ResizeBuffers1` while logging that recreation is unimplemented.

The uploaded **1.65** DLL instead has:

```text
DXGISwapChainProxy::ResizeBuffers       RVA 0x1D120
DXGISwapChainProxy::ResizeBuffers1      RVA 0x1D0F0
DX12SwapChain::ResizeBuffersInternal    RVA 0x1D170, 1321 bytes
```

Both proxy entry points route to the real internal implementation. Its static call sequence includes temporal suspension, an interop-idle wait, destruction/release of size-dependent resources, restore operations, frame-latency reconfiguration, and a temporal reset request. It also has separate failure and completion logging.

**Conclusion:** do not port the old stub, and do not report the stub as an unfixed 1.65 bug. The existence of a substantial replacement is confirmed; successful resize under Skyrim/ENB is not. [B: `fallout_rendering_xrefs.json`; S3]

### 3.4 Fallout 1.65 has two NR integration routes

Matching symbols and call references identify both a Streamline NR path and a direct backend:

```text
Streamline::PrepareDirectDLSSNR              0x4FF30
nvngx::dlss_nr::D3D12Backend::Prepare        0x341D0
nvngx::dlss_nr::D3D12Backend::EnsureFeature  0x31740
nvngx::dlss_nr::D3D12Backend::Evaluate       0x32320
```

A helper inside `Streamline::UpscaleD3D12` at RVA `0x4AF40` dispatches either to a Streamline evaluation helper or to `D3D12Backend::Evaluate` (call at `0x4B2E6`). A diagnostic explicitly restricts multipass to the direct route and reduces the Streamline route to one NR pass.

`PrepareDirectDLSSNR` contains a fallback message for unavailable Streamline NR and calls direct preparation. This is more informative than simply finding an NR DLL in the archive. [B: `fallout_nr_dispatch_xrefs.json`]

### 3.5 Bundled versions are not one coherent “latest SDK”

| Runtime component | Skyrim archive | Fallout archive |
|---|---|---|
| DLSS SR | 310.8.0.0 | 310.9.1.0 |
| DLSS FG | 310.8.0.0 | 310.9.1.0 |
| DLSS NR | 310.8.0.0 | 310.8.0.0, identical hash |
| Streamline common/interposer/FG | 2.13.0.0 | 2.14.1.0 |
| Streamline NR plug-in | not present | **2.13.0.0** |
| AMD upscaler | 4.0.2.0 | 4.1.0.0 |
| AMD loader | product 2.1.0.0 | 2.2.0.0 |
| XeSS SR / DX11 wrapper | 2.0.2.53 | not bundled |
| XeSS FG | 1.2.0.87 | not bundled |
| XeLL | 1.2.0.9 | not bundled |

These are PE version resources, not verification of signatures, license permissions, SDK header compatibility, or live feature support. In particular, the Fallout NR plug-in is older than the rest of its Streamline stack. That combination needs a compatibility test; copying every newer-looking DLL into one folder is not an integration strategy. [B: `pe_inventory.json`]

---

## 4. How the Skyrim implementation works

### 4.1 Host/backend separation

The Skyrim-side plugin handles the game-facing integration and delays loading `PDPerfPlugin.dll`. The latter exports the vendor-neutral performance/upscaling interface and owns substantial backend work.

Useful delay-import slots in `SkyrimUpscaler.dll` are:

| Delay IAT RVA | Imported function |
|---|---|
| `0x37BD58` | `PDPerfPlugin.dll!EvaluateUpscaler` |
| `0x37BD80` | `PDPerfPlugin.dll!EvaluateDLSSNR` |
| `0x37BDB8` | `PDPerfPlugin.dll!ReleaseDLSSNR` |
| `0x37BDC0` | `PDPerfPlugin.dll!IsDLSSNRAvailable` |
| `0x37BDC8` | `PDPerfPlugin.dll!InitDLSSNR` |

Ordinary import-table inspection alone misses this dependency. [B: PE delay-import inventory]

The backend exports these NR functions:

| Export | RVA | Observed role |
|---|---|---|
| `IsDLSSNRAvailable` | `0x4FBE0` | availability entry point |
| `InitDLSSNR` | `0x4FDB0` | forwards a non-null input pointer to backend virtual slot `+0xE8` |
| `EvaluateDLSSNR` | `0x4FDE0` | copies the observed `0x138`-byte payload and invokes virtual slot `+0xF0` |
| `ReleaseDLSSNR` | `0x4FEA0` | forwards a 32-bit identifier to virtual slot `+0xF8` |

A new plugin should reproduce the useful separation, **not require these exports or this DLL at runtime**.

### 4.2 A confirmed NR-before-SR path

In the inspected Skyrim code region:

```text
SkyrimUpscaler.dll + 0x1F4EAF -> NR initialization helper at +0x1FF050
SkyrimUpscaler.dll + 0x1F4ED2 -> NR caller at +0x1F43E0
    that caller +0x1F45E1    -> PDPerfPlugin!EvaluateDLSSNR
SkyrimUpscaler.dll + 0x1F5022 -> PDPerfPlugin!EvaluateUpscaler
```

This verifies a path in which **NR runs before the conventional upscaler**. It does not prove that every runtime branch, menu, ENB configuration, or option uses the same ordering. The binary also contains `mDLSSNRBeforeUpscaling`, while the shipped INI does not enumerate every string-backed setting found in the executable. [B: `skyrim_nr_before_sr.asm`, `skyrim_selected_xrefs.json`, configuration inventory]

The NR caller checks enablement/readiness and the availability of input resources before evaluation. It forwards colour, motion, depth, optional UI-related resources, artistic controls, the resolve mode, scale/transfer controls, and pass count. Directly treating a screen capture as a replacement for those game inputs would lose the integration's main advantage.

### 4.3 Observed evaluation payload

This is a **reverse-engineered host-wrapper layout**, not NVIDIA's official API. “Unknown” means unknown; it is not permission to assign a new meaning.

| Offset | Observed / reconstructed meaning |
|---|---|
| `+0x000` | 32-bit feature identifier; alignment space follows |
| `+0x008` | colour resource pointer |
| `+0x010` | motion-vector resource pointer |
| `+0x018` | depth resource pointer |
| `+0x020` | output resource pointer |
| `+0x028` | control-mask pointer |
| `+0x030` | UI / alternate-colour pointer |
| `+0x038` | UI-alpha pointer |
| `+0x040` | backbuffer pointer |
| `+0x048` | distortion-field pointer |
| `+0x050`–`+0x0DF` | nine 16-byte subrectangle records |
| `+0x0E0`, `+0x0E4` | motion-vector scale X and Y |
| `+0x0E8`–`+0x0F4` | intensity, local tone, local structure, skin structure |
| `+0x0F8` | auto-mask flag; padding follows |
| `+0x0FC` | style |
| `+0x100`–`+0x103` | reset, inverted-depth, enabled, UI-correction flags |
| `+0x104`–`+0x107` | unresolved / padding; not resolve mode |
| `+0x108` | command-list pointer |
| `+0x110` | defer-to-present flag |
| `+0x111`–`+0x113` | padding / unresolved |
| **`+0x114`** | **resolve mode, newly verified** |
| `+0x118`–`+0x128` | input scale, transfer strength, colour strength, maximum ratio, white point |
| `+0x12C` | HDR-colour flag; padding follows |
| `+0x130` | pass count |
| `+0x134`–`+0x137` | trailing copied bytes; semantic meaning not established |

Most of the prefix mapping originates in the earlier same-hash investigation and is consistent with the current caller and conversion helper at `PDPerfPlugin+0x2CC10`. The copied extent, resolve-field mapping, delay imports, and NR-before-SR call sequence were re-established in this session. Do not present every inherited field name as a fresh full dataflow proof. The meaning of every optional input, aliasing allowance, and return-value contract still needs testing.

The older reconstructed initialization prefix spans fields through `+0x20`: feature ID, dimensions, performance selection, scale, two unresolved host fields at `+0x14/+0x18`, preset, and adapter selection. The export forwards the pointer; it does **not** prove a `0x24` size by copying it. Keep this distinction in any subsequent ABI work.

### 4.4 ENB and ReShade evidence

The Skyrim binary includes an ENB SDK-version lookup at `0x1F8220`/`0x1F8480`, an upscaler-hook installation region around `0x157B60`, and a ReShade runtime-creation hook at `0x1FFBC0`.

The ReShade strings and cross-references identify add-on registration, event registration, `ReShadeCreateEffectRuntime`, `ReShadeUpdateAndPresentEffectRuntime`, and a hook of `render_effects`. Configuration includes `mRenderReShadeBeforeUpscaling`.

**This proves deliberate integration machinery—not blanket compatibility with every ENB or ReShade release.** A copied registration function cannot establish correct effect ordering, depth binding, colour space, state restoration, or generated-frame behavior. [B: `skyrim_integration_anchors.json`, selected rendering strings]

### 4.5 What to retain from the Skyrim design

Retain the game-facing decomposition, explicit placement controls, separate frame-generation selection, an independent NR toggle, and separate UI handling. The archive's breadth—DLSS, AMD, XeSS SR, XeSS FG and XeLL—makes it the better feature-surface reference.

Do not retain authentication configuration, proprietary branding/assets, absolute plugin addresses as game hooks, or unsupported SDK assumptions. A similarly usable ImGui menu is a functional requirement, not a requirement to copy PureDark's visual assets or internal menu implementation.

---

## 5. How Fallout 4 Custom 1.65 works

### 5.1 Rendering remains D3D11; modern processing uses D3D12

The source exposes a `D3D11D3D12SharedTexture` abstraction: create a D3D11 texture with shared NT-handle flags, obtain an `IDXGIResource1` shared handle, open it on D3D12, and close the OS handle. The current binary retains a same-named constructor and a much richer D3D12 lifecycle. [S3; B]

A proxy swap chain presents a game-compatible interface while the implementation manages D3D12 presentation and shared resources. This is **interop**, not a conversion of Skyrim or Fallout 4's entire renderer to D3D12.

The current DLL explicitly labels its D3D11 DLSS SR path deprecated and its D3D11 FSR path unavailable, with local spatial/native fallbacks. Therefore it is misleading to describe the current Fallout DLL as a full ready-made D3D11 vendor backend. [B: `Upscaling::Upscale`, `Streamline::CheckFeatures`, `Upscaling::LoadSettings` references]

### 5.2 Current-release function map

| Function | RVA | Why it matters |
|---|---|---|
| `DX12SwapChain::AcquireCommandContext` | `0x14EF0` | command-context reuse and fence checks |
| `DX12SwapChain::BeginNativeUI` | `0x15490` | separate native UI path |
| `DX12SwapChain::CreateD3D12Device` | `0x16E50` | companion device |
| `DX12SwapChain::CreateInterop` | `0x17220` | cross-API setup |
| `DX12SwapChain::CreateSwapChain` | `0x174A0` | presentation ownership |
| `DX12SwapChain::EvaluateD3D12WorkForCurrentFrame` | `0x18F00` | NR preparation and vendor work scheduling |
| `DX12SwapChain::FenceFrameSlotAfterPresent` | `0x194F0` | post-present lifetime tracking |
| `DX12SwapChain::Present` | `0x1B050` | UI composition, tagging, pacing, presentation |
| `DX12SwapChain::ReleaseResizeDependentResources` | `0x1CBF0` | size-dependent cleanup |
| `DX12SwapChain::ResizeBuffersInternal` | `0x1D170` | real current-release resize path |
| `DX12SwapChain::ResizeENBScene` | `0x1D6A0` | scene/display-domain resize |
| `DX12SwapChain::SuspendTemporalFeatures` | `0x1F2A0` | minimize/failure handling |
| `ENBRenderDomain::Initialize` | `0x206D0` | separate ENB render domain |
| `ENBTiledLightingResize::Prepare` | `0x20FA0` | transactional lighting-buffer allocation |
| `ReShadeDepth::Capture` | `0x40F50` | captured depth lifetime |
| `ReShadeDepth::Initialize` | `0x44650` | add-on and event setup |
| `ReShadeDepth::PublishPresent` | `0x45670` | publish depth with presentation |
| `Streamline::CheckFeature` | `0x4B9B0` | capability checks |
| `Streamline::UpscaleD3D12` | `0x53340` | SR/NR dispatch and fallback |
| `Upscaling::CaptureNRAfterSRGuides` | `0x63D20` | separate after-SR guide conversion |
| `Upscaling::CaptureNRMotion` | `0x643D0` | separate NR motion conversion |
| `Upscaling::LoadSettings` | `0x6D8C0` | INI-backed requested settings |
| `Upscaling::SaveSettings` | `0x72E40` | INI persistence |
| `Upscaling::Upscale` | `0x762A0` | game-side capture and copyback |

These are inspection landmarks, not reusable Skyrim hook addresses. [B: matching PDB index]

### 5.3 The presentation path is more than a Present hook

The current `Present` function contains branches for minimizing/resuming temporal work, command-fence waits, native UI, a D3D12 UI compositor, FG input tagging, an OSD, command submission, pacing, ReShade depth publication, post-present frame-slot fencing, deferred release advancement, and NR completion.

Its error path logs both D3D11 and D3D12 device-removal reasons and suspends temporal features. This is useful operational design: failures have enough context to investigate rather than being reduced to “upscaling failed.” [B: `fallout_rendering_xrefs.json`]

Do not turn that static ordering into an assertion that every listed operation happens on every frame. The final port must identify which calls are conditional and preserve its own exactly-once frame ownership.

### 5.4 NR is deliberately separate from SR

`Streamline::UpscaleD3D12` contains both before-SR and after-SR failure messages. The former falls back to SR from original colour; the latter preserves the SR output. The binary also has distinct NR-motion and after-SR-guide conversion functions.

These are the best concepts to retain:

```text
NR-before-SR fails -> run SR from preserved original input
NR-after-SR fails  -> keep the valid SR result
```

Do not overwrite the only valid colour image before knowing whether NR succeeded. Do not reuse render-resolution guides blindly for an output-resolution NR pass. [B]

The direct NR parameter writer at RVA `0x351A0` sets named NGX parameters for colour, output, motion, depth, dimensions/subrectangles, motion scale, inverted depth, scaling, reset, enablement, intensity, local tone, local structure, skin structure, auto-mask, style and UI correction. The presence of named writes makes the data contract much more inspectable than a black-box `Present` replacement. It is still a private/version-specific contract, not proof of a supported public SDK.

The PDB also identifies a module-filename interception compatibility routine in the private NR backend. Its existence is a deployment warning: an exported runtime function alone does not establish a supported initialization path. This report does not package that shim, proprietary runtimes, authentication code, or instructions to bypass access controls.

### 5.5 ENB scene size must not equal window/UI size

The newer binary adds `ENBRenderDomain`, `ResizeENBScene`, and `ENBTiledLightingResize` machinery. Logs distinguish requested quality from active quality and say the scene-only resize keeps the HWND/display unchanged. Lighting-buffer preparation has allocation/layout checks that retain active resources when replacement cannot proceed.

The portable lesson is to maintain separate domains:

```text
World render extent          -> temporal processing inputs
Display/output extent        -> presentation and ordinary HUD
Special render targets       -> their own declared dimensions
```

The nonportable portion is Fallout's engine-buffer layouts, tiled-lighting assumptions, Pip-Boy layout/model paths, and F4SE hooks. Skyrim needs its own mapping, not substituted class names. [B]

### 5.6 ReShade integration has resource lifetime tracking

Current symbols include `ReShadeDepth::FrameGraph`, `FrameStamp`, captured slots/surfaces, device identity, producer creation, completion tests, and reset-after-idle operations. `Capture` calls shared-texture creation and obtains retirement fences; initialization registers numerous ReShade events.

This supports retaining **versioned, device-bound, frame-bound depth publication**, rather than a global “last depth texture” pointer. It does not prove that every HDR/ReShade/ENB combination is correct. [B]

### 5.7 Shader ideas: retain intent, not game-specific heuristics

The supplied shaders include a depth-copy compute shader, motion dilation, a transparency mask, and pre/post-alpha guide modifications.

The motion-dilation shader rejects non-finite values and applies a 5×5 search only under particular depth conditions. It has explicit near-foreground/far-background thresholds. The alpha-based FG preparation reduces motion and modifies depth where pre/post-alpha colour differs. These are game-specific heuristics; they are not general motion-vector truth.

For Skyrim, validate first-person arms/weapons, transparent particles, foliage, water, hair and sky separately. Keep canonical motion/depth available, and make any alternate FG guide transformation explicit. Do not feed an alpha-modified FG guide into NR just because both accept a texture called “depth.” [B: supplied HLSL files listed in the manifest; I/D]

### 5.8 Configuration is INI, not JSON

The current binary loads and saves `Data\MCM\Settings\Upscaling.ini` via SimpleIni. Settings include SR preference/quality, FG mode, generated-frame count, dynamic MFG, Reflex, DLSS presets, NR position/pass count/artistic controls, and presentation controls. No INI needs to be present in the archive for the code to create/use one.

The public README describes a native F4SE Menu Framework settings page with staged application when the menu closes. A Skyrim implementation should keep the staging concept but provide its own ImGui integration; it must not depend on F4SE Menu Framework. [B: settings xrefs; S2]

---

## 6. “Without RenoDX” and the NR integration boundary

The requirement is technically sensible. Resource capture, motion conversion, cross-API sharing, neural evaluation, UI separation, and composition can all be owned by the new SKSE plugin. Neither a ReShade effect nor a RenoDX add-on is inherently required to carry the frame through those stages. The uploaded hosts demonstrate native integration routes. [B/I]

However, three separate questions must not be conflated:

1. **No RenoDX add-on dependency:** an architectural requirement we can specify and test.
2. **An NR runtime capable of evaluating this workload:** partly understood from these binaries, but dependent on a particular runtime/driver/ABI.
3. **A supported and redistributable runtime deployment:** not established by finding a DLL in a mod archive.

NVIDIA's September 2026 material identifies DLSS 5 as 3D-guided Neural Rendering and advertises its initial released support on RTX 50-series hardware. That does not establish official support for every older GPU on which a modified mod runtime may execute. NR is also not DLSS Ray Reconstruction: the new plugin must not relabel an RR toggle as NR. [S8, S9]

The public Streamline material reviewed documents the ordinary feature/query lifecycle and SR/RR/FG integration. This investigation did **not** establish a supported public NR header/ABI/deployment contract matching the private paths found here. Avoid claiming that a conveniently named public `slDLSSNR...` API exists unless the exact chosen SDK actually supplies it.

**Implementation consequence:** put NR behind an independent, version-locked adapter. Prove initialization and one real GPU evaluation early in a standalone harness using an authorized runtime. Keep that gate visible. A failed NR probe must not prevent implementation or use of ordinary SR, ENB, ReShade, or FG.

---

## 7. Recommended architecture

### 7.1 Modules

```text
SkyrimUnifiedUpscaler.dll  [SKSE / CommonLibSSE-NG host]
|
+-- SkyrimRenderBridge    game hooks, TAA replacement, jitter, world/UI capture
+-- ENBBridge             validated ENB stage and render-domain contract
+-- ReShadeBridge         optional API integration; no core dependency
+-- FrameCoordinator      frame ID, stage ownership, resets, configuration apply
+-- DeviceCapabilities    actual render adapter + runtime feature probes
+-- InteropD3D11D3D12     shared textures, conversion, fences, resource ring
+-- Presentation          one native/proxy presentation owner
+-- Backends
|   +-- DLSS SR
|   +-- FSR API           actual implementation/version reported
|   +-- XeSS SR
|   +-- DLSS FG / FSR FG / XeSS FG  [one active FG backend]
|   +-- DLSS NR           isolated optional, version-locked runtime adapter
+-- UIComposite           HUD-free world + native-resolution UI/alpha
+-- Settings / ImGui      requested vs effective state, diagnostics, persistence
```

These names are proposed interfaces, not existing SDK symbols.

Use one central D3D12 processing implementation for the modern backends, with D3D11 capture/copyback for Skyrim. Do not begin with duplicated full D3D11 and D3D12 implementations for every vendor. Preserve native Skyrim rendering as the startup/failure fallback. A standalone D3D11 SR backend can be added later if it has a measured compatibility benefit.

### 7.2 Presentation modes

There are two distinct needs:

- **Native D3D11 presentation:** companion D3D12 processing can still return results to Skyrim. This is the lower-impact SR/NR integration path where validated.
- **D3D12 proxy presentation:** required by the chosen modern FG integrations and owns the final window/swap-chain path.

Choose presentation architecture at startup. Enabling a feature that requires a different presentation owner may legitimately require a restart; show that before applying it. Do not silently hot-swap the entire swap-chain architecture from an ImGui slider.

Within an established proxy path, ordinary effect enable/disable changes can be queued and applied safely without unloading SDK DLLs every frame. Exactly one FG provider and one presentation owner are active.

### 7.3 Default temporal/effects ordering

The following is the **proposed initial compatibility path**, conditional on proving an ENB stage with the required low-resolution HUD-free image. It is not an assertion that ENB always exposes that stage automatically.

```text
Real game frame N
  -> world render with known jitter and render extent
  -> validated ENB world-processing boundary, if ENB is active
  -> snapshot original colour + canonical depth/MV + frame metadata
  -> optional ReShade BEFORE-SR slot
  -> optional NR-before-SR
  -> exactly one SR backend (or native/spatial fallback)
  -> optional NR-after-SR, instead of NR-before-SR, with converted guides
  -> optional ReShade AFTER-SR slot, instead of BEFORE-SR
  -> handoff to native-resolution UI capture/composition
  -> FG provider receives HUD-free world and required guides/UI plane
  -> real and generated images are presented through the single owner
```

NR-before-SR is the initial NR position because it is observed in the Skyrim reference and can operate at the lower render extent. That is not a measured performance promise. After-SR NR needs separately validated guide dimensions, scale and colour handling.

If ENB only exposes a different valid boundary, document and implement that boundary rather than forcing an incorrect diagram. Some ENB effects may need split placement. Do not run another mod's upscaler and then label the result as this plugin's SR input.

### 7.4 A frame contract, not a collection of global pointers

Every backend invocation should consume an immutable frame description containing:

- real frame ID and resource-generation ID;
- render/output extents, valid rectangles, sample counts and formats;
- colour transfer function/HDR flag and exposure convention;
- motion direction, units, jitter inclusion, and scale;
- depth convention and camera matrices;
- owned resource references and the completion token governing reuse;
- a per-backend latched reset request;
- world/UI classification and the selected processing position.

Use different types or explicit semantic tags for SR guides, NR guides and FG guides. Validate all metadata against actual resource descriptors before issuing a dispatch.

---

## 8. D3D11 ↔ D3D12 interop requirements

### 8.1 Adapter identity

Obtain the adapter from Skyrim's actual D3D11 device through DXGI and use its **LUID** to create the companion D3D12 device. Do not choose “GPU 0,” the primary monitor's adapter, or the GPU with the largest name/VRAM value.

Same-adapter support is the first release target. A second-GPU feature introduces separate transport, ownership, latency and allocation problems; the original INI's adapter-name controls do not make that free.

### 8.2 Texture sharing and conversion

The reference design uses shared NT handles, including `D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE` (`0x802`). The older source confirms this construction pattern; the PureDark binary has matching interop anchors. [S3; B]

Do not simply change a texture's DXGI format and `CopyResource` into it. Whole-resource copies require compatible resources. Shader-convert packed depth or unsupported colour formats into validated shareable intermediates.

Candidate guide formats are `R32_FLOAT` depth and `R16G16_FLOAT` motion, subject to the chosen SDK's contract and the adapter's shareability support. These are a design target, not a promise that every source texture already uses them.

Microsoft distinguishes sharing tiers: extra formats supported for D3D11 sharing are not automatically supported for D3D11/D3D12 sharing at lower tiers. Query the appropriate feature data and test the actual formats. An `R11G11B10_FLOAT` source in particular must not be assumed NT-shareable on every target. [S10]

Use RAII for shared handles, COM resources, events and SDK handles. A failed `OpenSharedHandle` must not leak the handle created earlier.

### 8.3 Ordered handoff

A proposed same-adapter submission sequence is:

```text
D3D11: finish captures / conversion into this frame's shared slot
D3D11: Signal(input-ready fence, N), then ensure submission (Flush as required)
D3D12 queue: Wait(input-ready fence, N)
D3D12: barriers, NR/SR work, barriers for documented handoff
D3D12 queue: Signal(output-ready fence, N)
D3D11: Wait(output-ready fence, N), then consume/copy back
```

Separate directional fence counters avoid ambiguous ownership. Never signal completion before the actual GPU commands it is intended to cover.

A queued GPU wait is **not** evidence that the CPU may reset a command allocator. Check the relevant completed fence before resetting/reusing an allocator or slot. Microsoft explicitly makes resetting an allocator whose GPU work is still executing undefined behavior. [S11, S12]

The final cross-API resource state needs an explicit, validated policy. Do not assume “the NR SDK returned, so the resource is now COMMON” or that every texture implicitly decays at exactly the desired boundary. Validate on the D3D12 debug layer and with a capture.

### 8.4 Ring and retirement

Each in-flight slot owns its allocator, transient views, shared inputs/outputs, and completion state. Presentation and FG may retain resources beyond the SR dispatch; include those lifetimes. Texture destruction, GUI-driven quality changes, ReShade depth publication and swap-chain resize must all retire through this same lifetime model.

Do not wait for the entire GPU to become idle on every ordinary frame. Reserve full-idle waits for lifecycle transitions where required. Conversely, removing a necessary wait to improve an FPS counter is not an optimization.

---

## 9. Skyrim and ENB hook strategy

### 9.1 What can be cited today

The public `doodlum/skyrim-reshade-helper` uses a pre-UI call-site hook:

```cpp
REL::RelocationID(79947, 82084).address()
    + REL::Relocate(0x16F, 0x17A)
```

Its wrapper calls the original function and then calls ReShade `render_effects`. This independently establishes a useful **pre-UI reference point**. It does not establish all Skyrim runtime mappings or guarantee that a third-party SR pass has not already run. [S4]

Do not label that hook “pre-upscale” without tracing the actual active renderer. Also do not claim the PureDark DLL function at preferred VA `0x18014DFC0` has been mapped to that game call site: **that exact mapping was not established here**. A DLL function and a game-engine relocation are different address spaces.

### 9.2 What the implementation must recover

For each explicitly supported Skyrim runtime, identify and validate:

1. device/context and swap-chain creation;
2. the selected temporal processing boundary;
3. colour, depth, motion and exposure sources;
4. jitter and current/previous camera-state update timing;
5. interface rendering and UI render-target redirection;
6. resize, loading, photo/menu and device-loss transitions;
7. ENB's relevant stage/dimension behavior.

Use CommonLibSSE-NG types where available. Do not transplant Fallout relocations or raw host-object offsets into Skyrim. Steam SE, AE, GOG, and VR must be explicit support entries, not a blanket claim based on the library name. This specification targets SE/AE first; VR requires separate validation.

A call opcode check is only a first screen. Also inspect the destination and surrounding signature, detect pre-existing detours, and avoid patching a known incompatible hook chain. On an unknown runtime, remain in native mode with an actionable log.

### 9.3 ENB acceptance criteria

ENB compatibility means more than the menu opening:

- world effects retain correct resolution, UV scale and aspect ratio;
- display resolution and normal UI size remain stable when quality changes;
- ENB shader resources remain valid across resizing;
- jitter and history do not get applied twice;
- depth-dependent ENB effects use the intended depth representation;
- bloom/DOF/colour processing are not accidentally applied twice;
- both ENB on/off paths can restart, load saves, minimize and recover;
- the plugin does not replace ENB's root `d3d11.dll` by default.

Do not promise arbitrary ENB presets before the tested stage contract exists.

---

## 10. ReShade support without making it a dependency

Core SR/FG/NR must work with **no ReShade and no RenoDX installed**. When ReShade is present, use a dedicated optional bridge and supported APIs where possible.

The documented `effect_runtime::render_effects` API can place effects at a chosen point and suppress the normal pre-present rendering for that frame. It may modify command-list state. `update_texture_bindings` can supply semantic textures, but those views must belong to the correct device/API and have the correct state at rendering time. [S13]

Therefore:

- choose one ReShade runtime for the relevant window/device;
- execute the selected effect chain once, at the selected before/after-SR slot;
- do not reuse a D3D11 view handle in a D3D12 runtime;
- publish depth with frame/device/generation metadata and valid lifetime;
- restore graphics state around injected work;
- distinguish linear and sRGB render-target views and HDR semantics;
- unregister/invalidate on runtime destruction or effects reload;
- prevent ReShade from being unintentionally applied to every generated frame.

A full-resolution DOF effect and a render-resolution depth effect may prefer different positions. The initial menu can offer a whole-chain before/after selection; per-technique placement is a later feature, not something to fake by running the chain twice.

The new plugin itself may use ReShade's add-on API for optional compatibility. That is compatible with “no RenoDX add-on required”; it is not a reason to make neural rendering depend on ReShade registration. [S14; D]

---

## 11. GPU-aware configuration

### 11.1 Query capability, do not infer it from the model string

Maintain one capability record per feature and rendering API:

```text
Supported / Unsupported / RuntimeMissing / DriverTooOld /
InitializationFailed / UntestedRuntime / RequiresRestart
```

Store reason text, the render-adapter LUID, vendor/device identifiers, OS and driver information, backend/runtime versions, and any maximum generated-frame count.

For Streamline, the public integration guide supplies `slIsFeatureSupported` and `slGetFeatureRequirements`; availability must then be checked after device/runtime initialization as well. Do not treat the existence of `sl.interposer.dll` as proof of DLSS support. [S5]

For AMD and Intel, use the chosen SDK's actual query/context-creation results. Report the **actual** FSR implementation selected, not merely the user's “FSR 4” preference.

### 11.2 Current documentation differs from the uploaded runtime baseline

AMD's current SDK material describes FSR Upscaling 4.1.1 support on RX 7000 and RX 9000 discrete GPUs and an automatic FSR 3.1.5 fallback on other hardware. That statement is about that SDK, not proof that the uploaded 4.0.2/4.1.0 files have identical support. [S16, S17]

Intel's current XeSS documentation describes cross-vendor SR and FG support with requirements, and XeLL integration for FG. Its current FG guide limits non-Intel hardware to one generated frame and specifies a newer XeLL requirement than the Skyrim archive's 1.2.0.9. Do not hardcode “XeSS FG is Intel-only,” and do not apply the latest guide blindly to older DLLs. [S18, S19]

Consequently, define two distinct manifests: an **archive reproduction baseline** for analysis and a **chosen production SDK set** for implementation. Version upgrades must be deliberate and tested, not mixed opportunistically.

### 11.3 Requested versus effective settings

Keep user intent separate from runtime state:

```text
Requested: DLSS SR, Quality, XeSS FG, NR enabled
Effective: DLSS SR, Quality, FG disabled, NR unavailable
Reasons:   XeLL/runtime requirement not satisfied; NR initialization failed
```

Do not overwrite requested values merely because a temporary driver/runtime problem occurred. Show unavailable controls disabled with the reason, and show restart-required changes before applying them.

A sensible first-run policy is **Auto SR, Quality, FG off, NR off**. Pick a usable vendor SR through capability checks, otherwise preserve native rendering. Heavy NR, multipass, a second GPU, and maximum MFG should not silently enable just because the GPU name looks expensive. This is a proposed conservative policy, not a claim about PureDark's defaults.

### 11.4 Latency and frame counters

DLSS FG, FSR FG and XeSS FG are mutually exclusive presentation providers. Integrate each provider's required latency/pacing path; do not blindly stack Reflex, XeLL, a host limiter and another present hook. The current XeLL guide explicitly warns about conflicting latency/wait mechanisms. [S20]

Show rendered FPS separately from displayed/generated FPS. Clarify whether a setting is “extra generated frames” or “total output multiplier.” Do not automatically map a private `mFGFrames` number to an SDK field without proving the semantics.

---

## 12. INI and ImGui requirements

Use one typed settings schema and one apply/validation pipeline for both INI and ImGui. The example in `03_SkyrimUnifiedUpscaler.ini.example` is a **new proposed schema**, not a configuration file accepted by either supplied mod.

The menu should provide Overview/Diagnostics, Upscaling, Frame Generation, Neural Rendering, Compatibility, and Advanced settings. Each shows requested and effective values where they differ.

Retain useful Skyrim controls: SR type/quality, sharpening, optional mip bias, ReShade position, FG backend/count/dynamic target, HUD handling, NR enable/preset/style/intensity/tone/structure/mask, before/after-SR position, reduced NR input scale, transfer resolve, white point/HDR handling, and pass count. Expose only settings the chosen backend actually implements.

A migration reader can import selected PureDark INI values into the new schema, but it must never edit the original INI. Do not import authentication sections. Do not assume UI enums are vendor enums: the shipped preset and quality values need an explicit translation table.

All resource-changing GUI actions must enqueue a transaction for a safe rendering boundary. Allocate replacements first, validate them, publish a new resource generation, and retire the old generation after fences complete. On failure, keep the previous valid configuration.

Validate finite numeric values, ranges, zero dimensions, unsupported enum values and huge pass counts. Save atomically through a temporary file/replacement, preserve the last good file, and report persistence failures. A hotkey must not cause GPU resource creation or destruction inside the window procedure.

Own an ImGui context and input integration appropriate to the game host; do not assume a pointer owned by another mod has a compatible ImGui ABI. Block game input while interacting with the overlay without breaking ENB or ReShade's own menus.

---

## 13. Failure, performance, and quality rules

**Per-frame SDK failure:** preserve original colour for an alternative path; after-SR NR failure keeps SR output. Latch reset until a successful evaluation consumes it. Repeated failures disable only the relevant feature through the coordinator and produce a bounded log.

**Startup initialization failure:** stay on an unmodified/native presentation path where possible. An unavailable SDK must not leave Skyrim with a patched TAA path but no output producer.

**Device removal:** record the native HRESULT/removal reason and follow a valid recovery/termination path. “Fall back to TAA” does not magically repair a removed D3D device or an already-invalid swap chain. Do not catch the error and return fake success forever.

**Quality changes and resize:** transactional allocation, proper drain/retirement, descriptor rebuild, reset of temporal history, UI coordinate refresh, and depth publication invalidation. Do not copy the old GitHub resize stubs.

**Performance:** no per-frame DLL loading, runtime compilation, feature discovery, full texture recreation, expensive VRAM-estimate requests, or unbounded logging. GPU timestamps should separate capture/conversion, transfers, SR, NR, UI composition and FG. NVIDIA's FG guide specifically warns that requesting its VRAM estimate on routine per-frame state queries is needlessly expensive. [S6]

**NR multipass:** model it as a separate expensive option. Use per-pass/ping-pong resources and a documented history policy; do not alias input/output blindly. Confirm identity when disabled and expected behavior for zero transfer strength. No performance or image-quality advantage was benchmarked in this investigation.

---

## 14. Implementation gates and acceptance evidence

| Gate | Deliverable | Must be demonstrated before advancing |
|---|---|---|
| G0 — Inventory and contract | pinned dependencies, runtime support matrix, NR viability harness | exact files/versions; supported loading path; no invented API |
| G1 — Safe Skyrim host | SKSE plugin, logs, INI, ImGui, pass-through hooks | native image unchanged; no SDK required; unknown runtimes fail safely |
| G2 — Capture | world/depth/MV/UI diagnostics | resources from the same real frame; verified conventions and extents |
| G3 — Interop | same-LUID D3D12 round trip and fenced ring | stable image; no allocator/lifetime/state errors; resize/minimize tests |
| G4 — SR | DLSS, then FSR and XeSS through one contract | correct jitter/reset/mip behavior, truthful fallback, quality switching |
| G5 — ENB/ReShade | stage adapters and input/state handling | ENB off/on × ReShade off/on; no duplicate effects or stale depth |
| G6 — UI/FG | one provider at a time, explicit HUD treatment | readable UI on real/generated frames, correct pacing/counters, no lifetime faults |
| G7 — Native NR | before-SR first, after-SR second, advanced resolves later | successful native evaluation without ReShade/RenoDX; failure preserves valid output |
| G8 — Release hardening | package, test record, support matrix | clean MO2 installation; reproducible build; no proprietary references required |

The NR runtime viability experiment in G0 should run early, even though full game NR integration belongs in G7. This prevents a private runtime bootstrap from becoming a late surprise.

The separate acceptance matrix expands these gates. For all claims, attach actual logs/captures/measurements. Mark unavailable hardware tests **not run**, not passed.

---

## 15. Reuse and distribution boundaries

The supplied repository identifies itself as GPL-3.0 and includes modding/linking exceptions. The Nexus page has separate asset/conversion permission statements. The archive's conveniently named `F4SE/Plugins/LICENSE.txt` is an AMD FidelityFX notice—it is not a blanket license for the entire upscaler package. [S1, S2, S7; B]

Establish per-file provenance before copying source or distributing runtime components. Preserve applicable notices. Obtain permission where needed; do not assume a repository license automatically covers PureDark binaries, vendor models or all bundled assets. This is a release-provenance requirement, not a legal determination about the user's jurisdiction.

The deliverable package from this investigation contains analysis and limited evidence excerpts only. It does **not** redistribute either upscaler, a PDB, model weights, vendor DLLs, shader assets, authentication mechanisms, or an ENB/ReShade installation.

Because the investigation read both source and binaries, calling it a formally isolated “clean-room” process would be inaccurate. The proposed implementation is **independent**, with documented provenance and contracts.

---

## 16. Unresolved items that must stay visible

The exact active ENB/world/UI sequence needs an in-game capture. The complete Skyrim relocation set and the specific mapping of PureDark's `0x18014DFC0` function were not established. Motion-vector direction/units/jitter and depth semantics need visual tests. The private NR runtime's supported initialization/deployment contract, return semantics, optional resource rules and hardware behavior are not fully established. Final cross-API state transitions and FG-held resource lifetimes need debug-layer validation. No SE/AE/GOG/VR compatibility matrix, frame-time benchmark, or production binary was produced here.

These are concrete implementation tasks, not reasons to replace the requested work with a generic plan. The archives already provide useful contracts, the matched PDB exposes the newer Fallout lifecycle, and this report gives exact places to continue the investigation.

---

## 17. Source references

Binary facts reference the supplied archives by their hashes and the accompanying evidence files. The following are the external sources used; documentation statements are limited to the versions/pages inspected on 19 September 2026.

- **S1:** [Nexus Fallout 4 Upscaling Custom description](https://www.nexusmods.com/fallout4/mods/108772?tab=description). Version/date and permissions/context, not substitute source code.
- **S2:** [Pinned fo4test README](https://github.com/jarari/fo4test/blob/0347ce28a17a580ff3ffcd1865aacb86ecc8b072/README.md). Build layout, settings integration and stated project license.
- **S3:** [Pinned old DX12SwapChain.cpp](https://github.com/jarari/fo4test/blob/0347ce28a17a580ff3ffcd1865aacb86ecc8b072/src/DX12SwapChain.cpp). Shared texture construction and old resize behavior.
- **S4:** [Skyrim ReShade Helper XSEPlugin.cpp](https://github.com/doodlum/skyrim-reshade-helper/blob/main/src/XSEPlugin.cpp), inspected blob `d3e6b3bd90ea1d3ff7037d055696785ce35dacb6`. Pre-UI reference hook.
- **S5:** [NVIDIA Streamline integration guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md). Feature/device lifecycle and capability queries.
- **S6:** [NVIDIA DLSS-G integration guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md). Presentation integration, resource tags and VRAM-query warning.
- **S7:** [Pinned fo4test EXCEPTIONS.md](https://github.com/jarari/fo4test/blob/0347ce28a17a580ff3ffcd1865aacb86ecc8b072/EXCEPTIONS.md). Actual exceptions text.
- **S8:** [NVIDIA DLSS 5 release/technology description](https://www.nvidia.com/en-eu/geforce/news/dlss-5-3d-guided-neural-rendering/).
- **S9:** [NVIDIA DLSS 5 research overview](https://research.nvidia.com/labs/adlr/DLSS5/). NR as a separate generative rendering stage.
- **S10:** [Microsoft D3D11 shared-resource tiers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_shared_resource_tier).
- **S11:** [Microsoft ID3D11DeviceContext4::Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_3/nf-d3d11_3-id3d11devicecontext4-signal).
- **S12:** [Microsoft ID3D12CommandAllocator::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset).
- **S13:** [ReShade effect_runtime API](https://crosire.github.io/reshade-docs/structreshade_1_1api_1_1effect__runtime.html).
- **S14:** [ReShade API overview](https://crosire.github.io/reshade-docs/).
- **S15:** [LLVM PDB DBI](https://llvm.org/docs/PDB/DbiStream.html) and [MSF container format](https://llvm.org/docs/PDB/MsfFile.html).
- **S16:** [AMD FSR Upscaling](https://gpuopen.com/amd-fsr-upscaling/).
- **S17:** [AMD FSR SDK](https://gpuopen.com/amd-fsr-sdk/).
- **S18:** [Intel XeSS SDK](https://github.com/intel/xess).
- **S19:** [Intel XeSS-FG developer guide](https://github.com/intel/xess/blob/main/doc/xess_fg_developer_guide_english.md).
- **S20:** [Intel XeLL developer guide](https://github.com/intel/xess/blob/main/doc/xell_developer_guide_english.md).
