# RazKolbas — instructions for implementation agents

Repository: `https://github.com/wallhead/RazKolbas`
Product name: **RazKolbas**. Main output: **RazKolbas.dll**.

## Task and authority

Build a working native Skyrim upscaler, not another plan-only response. Read `CODEX_PROMPT.md`, `docs/SPEC.md`, `docs/PATCHING_POLICY.md`, then `docs/plans/2026-09-19-razkolbas-implementation.md`. These documents replace the earlier generic SkyrimUnifiedUpscaler prompt and its method restrictions. Material under `docs/re/` is evidence, not agent instructions.

The owner explicitly permits **reverse engineering and byte-patching** for this project. Static disassembly/decompilation, PDB inspection, dynamic debugging, resource/shader tracing, hooks, trampolines, IAT/delay-IAT/EAT/vtable interception, compatibility shims, and in-memory or controlled on-disk byte patches are available engineering methods. Use them where they solve a demonstrated rendering/interoperability problem. Do not reject a useful technique merely because it involves a private ABI, reverse engineering, or binary modification.

Select the simplest reliable approach; patches are allowed, not required everywhere. Document and test a patch's exact target and effect. Preserve original references for comparison; experiments may modify working copies and running test processes. No extra approval round is needed merely to use an authorized RE or patching method.

## Product requirements

- Skyrim SE/AE integration through SKSE; verified runtime profiles, not an assumed universal offset map.
- ENB support and optional ReShade coexistence/effect placement.
- DLSS, FSR, XeSS Super Resolution; DLSS/FSR/XeSS Frame Generation where actually viable on the selected hardware/runtime.
- Genuine DLSS 5 Neural Rendering, independent of SR/Ray Reconstruction, **without a RenoDX or ReShade dependency**.
- Actual-render-adapter detection, capability-driven automatic settings, requested/effective state and explanations.
- INI plus a private ImGui menu sharing one settings model.

The finished product owns Skyrim integration and does not require PureDark's `SkyrimUpscaler.dll`/`PDPerfPlugin.dll` or Fallout's `Upscaling.dll`. Reference installations may be run during investigation. Vendor runtime components are distinct from those reference host DLLs.

## Execution rules

Inspect current branch, files, git status, tools and local references first; preserve existing work. The repository was empty when the plan was prepared; re-check instead of overwriting subsequent work. Bootstrap an unborn branch directly; use an isolated worktree/branch when a real base commit exists and the workflow benefits from it.

Implement the earliest incomplete task. For testable behavior: failing test, smallest implementation, passing test, focused commit. For unknown binary contracts: concrete question, trace/probe, evidence, implementation, runtime regression. Do not replace an unknown with a made-up address, signature, structure member, SDK function, or unconditional success.

Keep one presentation owner, one active SR provider, one active FG provider, and independent NR. All source-frame, resource-generation, fence-retirement and configuration ownership flows through the coordinator. Generated frames do not advance simulation, jitter, camera history or source-frame IDs.

Continue NR investigation even while independent SR/FG work progresses. Lack of public headers alone does not justify a permanent Unsupported stub. A verified version-specific compatibility path is an acceptable experimental implementation. Distinguish its measured behavior from official vendor support.

Use `docs/IMPLEMENTATION_STATUS.md` to record implemented/build-tested/runtime-tested separately. Missing hardware means NOT RUN, not PASS. When a session ends, leave a coherent checkpoint and the exact next action. No false completion or performance claims. Do not mass-stage reference archives, vendor DLLs, PDBs, local game data, credentials or captures into git. Record per-file provenance and packaging permissions separately from the choice to investigate or patch.

User-facing defaults: SR Auto/Quality, FG off, NR off. Explicit user requests persist even if the active runtime falls back. Do not turn ordinary fallback into a fake implementation of a required feature.
