# RazKolbas reference baseline and first RE questions

## Evidence to preserve

These observations come from the preceding static investigation. They are starting evidence, not claims of new traces or runtime validation in this plan.

| Reference | Identity |
|---|---|
| Skyrim archive | `SkyrimUpscalerAIOBuild16-Hotfix1(1).7z`, SHA-256 `09a0ff6810e87a951fa5a51f1905e030e69f7fe28b15251d46e3afa2f7e53b74` |
| Fallout archive | `Upscaling Custom 108772 1.65 2026-09-13T17-12Z DzMlEf9Og(2).7z`, SHA-256 `792343d6ca89a06b06421a8180aa5815cb7d67aa0ee761065d65b8eb8afc5eb9` |
| SkyrimUpscaler.dll | `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb` |
| PDPerfPlugin.dll | `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d` |
| Fallout Upscaling.dll | `f2633bcf11f618569eac77d1c7e6bf136a498fdb53f5eb1f205bfbb3a74cac7b` |
| Shared nvngx_dlssnr.dll | `8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206` |
| Matched Fallout PDB | GUID `7057e107-1a03-4c53-ab14-7cd63ad16f3e`, age `71` |
| Older source comparison | `jarari/fo4test @ 0347ce28a17a580ff3ffcd1865aacb86ecc8b072` |

1. The two supplied NR DLLs are identical; host integration is the comparison target.
2. PureDark `EvaluateDLSSNR` at RVA `0x4FDE0` copies **0x138 bytes**. This is the observed copy extent, not NVIDIA's public ABI or a recovered original typedef size.
3. The observed resolve selector is **payload +0x114**, not +0x104. Preserve uncertainty about unresolved fields.
4. A Skyrim branch calls NR at `SkyrimUpscaler+0x1F4ED2`, then ordinary upscaling at `+0x1F5022`. Check **delay imports**, including `PDPerfPlugin!EvaluateDLSSNR`.
5. Fallout 1.65 has real resize handling; the older GitHub no-op `ResizeBuffers` is not the current binary's implementation.
6. Fallout 1.65 contains direct NGX NR and Streamline NR routes. Runtime initialization and actual branch activation still require tracing.
7. Pre-UI reference `RelocationID(79947,82084)+Relocate(0x16F,0x17A)` is not automatically pre-SR. `SkyrimUpscaler` preferred VA `0x18014DFC0` has not been conclusively mapped to a complete stable Skyrim engine contract.
8. Exact ENB ordering, guide semantics, API-state handoff and FG-held lifetimes remain runtime validation tasks.

## Targeted investigation map

All RVAs below belong to the **matching reference module**, not SkyrimSE.exe.

| Question | Starting landmarks | Required result |
|---|---|---|
| Skyrim hook installation and ENB domain | Skyrim `0x157B60`; ENB lookups `0x1F8220`/`0x1F8480`; `skyrim_integration_anchors.json` | Record engine callsites/ABI and scene/display/UI boundaries for each tested runtime. |
| Reference ReShade bridge | Skyrim `0x1FFBC0`; create/update/render_effects imports/xrefs | Trace who creates/selects the runtime, effect placement, duplicate suppression and depth lifetime. |
| NR host payload | PD `0x4FDB0`, `0x4FDE0`, `0x2CC10`, `0x29E10`; Skyrim `0x1F43E0` | Validate dataflow for fields needed by RazKolbas; do not bind the finished product to PD exports. |
| Native NR bootstrap | Fallout PrepareDirectDLSSNR `0x4FF30`; D3D12Backend Prepare `0x341D0`, EnsureFeature `0x31740`, Evaluate `0x32320` | Exact modules, device ownership, parameter factory/vtable, feature identifier and lifecycle. Recover/implement a versioned compatibility shim or byte patch where required. |
| Native NR parameter writes | Fallout `0x34ED0`, `0x351A0`; selection helper `0x4AF40` | Verify argument types and per-frame resource/metadata semantics, not just strings. |
| Presentation and retirement | Fallout `0x14EF0`, `0x18F00`, `0x194F0`, `0x1B050` | Allocation, submission, provider-held resources and final retirement graph. |
| Real resize lifecycle | Fallout `0x1D170`, `0x1CBF0`, `0x1F2A0` | Suspend/drain/recreate/resume model, including error branches. |
| ENB/ReShade lifetime examples | Fallout `0x206D0`, `0x1D6A0`, `0x40F50`, `0x45670` | Transfer lifecycle principles without copying Fallout layouts. |

## Experiment record format

For each question record input binary hashes, tool/version, process/runtime, exact breakpoint or signature, call stack, observed arguments/descriptors, competing hypothesis, changed variable, output, result and next experiment. Label STATIC_OBSERVED, INFERRED, PROPOSED, RUNTIME_OBSERVED separately. An existing report can be corrected by stronger evidence.

Use `docs/re/evidence/` for small existing records. New captures and extracted binaries belong under ignored `reference/working/` or `artifacts/local/`; commit summaries and reproducible scripts rather than large raw game files.
