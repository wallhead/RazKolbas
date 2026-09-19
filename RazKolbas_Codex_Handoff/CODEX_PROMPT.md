# Codex task — implement RazKolbas

Repository: **https://github.com/wallhead/RazKolbas**
Main output: **RazKolbas.dll**. Configuration: **RazKolbas.ini**.

You are the implementer of a native Windows x64 Skyrim SKSE upscaler. Build the product in this repository. This is not a request to write another general plan or to rename/patch Fallout's DLL into a supposed Skyrim port.

Read in order:
1. `AGENTS.md`
2. `docs/SPEC.md`
3. `docs/PATCHING_POLICY.md`
4. `docs/plans/2026-09-19-razkolbas-implementation.md`
5. `docs/re/BASELINE_FINDINGS.md`, then the relevant sections of `docs/re/REFERENCE_REPORT.md` and `docs/re/evidence/`
6. `docs/testing/ACCEPTANCE_MATRIX.md`

## Owner-authorized methods

**Reverse engineering and byte-patching are explicitly allowed.** You may use static disassembly/decompilation, matched PDBs, dynamic tracing/debuggers, shader inspection, ABI reconstruction, hooks/trampolines, IAT/delay-IAT/EAT/vtable interception, compatibility shims, in-memory patches and controlled on-disk patches. Use the method justified by the actual integration problem; do not rule it out merely because an interface is private or undocumented. Earlier blanket no-RE/no-bytepatch/no-modification restrictions in old handoffs are superseded.

Research may run/modify reference working copies. Keep pristine files/hash identities for repeatable comparisons. When a runtime bootstrap needs a compatibility correction, trace the failing path, implement the evidenced shim/patch, then verify creation, real GPU output and teardown. Record hash/signature/expected bytes, patch ownership, safe application and rollback. A successful patched condition is not proof of new GPU capability; report the original probe and experimentally validated behavior honestly.

## Required product

- Skyrim SE/AE support through verified game-runtime profiles.
- ENB support comparable in function to the Skyrim reference: world/display separation, correct effects order and native-resolution UI.
- Optional ReShade support, useful effect placement and fresh depth; no duplicate application.
- DLSS/FSR/XeSS SR; DLSS/FSR/XeSS FG on viable hardware/runtime combinations; genuine independent DLSS 5 Neural Rendering.
- **No RenoDX or ReShade dependency for SR, FG or NR.** No dependency on PureDark's or Fallout's reference host DLLs in the final product.
- Actual-render-GPU detection, per-feature capability validation, automatic defaults and requested/effective settings.
- One typed INI model plus an independent ImGui menu, hotkeys, safe Apply/Reload/Save and diagnostics.

## Start working

Inspect repository status, branch, local tools, files and supplied references. The repository was empty when this handoff was prepared; preserve any code added since then. Keep large archives, extracted binaries, PDBs and game captures outside git. Locate reference archives through `RAZKOLBAS_REFERENCE_ROOT` or the explicitly supplied local reference directory. Use the original names/hashes in `docs/re/BASELINE_FINDINGS.md`; do not assume ChatGPT's `/mnt/data` exists on this machine.

Bootstrap the project with the plan's build/test conventions. Implement T01–T04 first, then continue the dependency graph. Run the NR probe track T08 early alongside independent capture/SR work. Do not make SR/FG wait for every NR research question, and do not declare NR finished by leaving an unavailable stub. If a particular resource/access requirement is absent, record exactly what is missing and continue all independently executable work.

Important existing evidence:
- Fallout 1.65's PDB matches `Upscaling.dll` (GUID `7057e107-1a03-4c53-ab14-7cd63ad16f3e`, age 71). The old GitHub source is not complete 1.65 source.
- PureDark's observed evaluation copy is **0x138**, not 0x140; the resolve field is **+0x114**, not +0x104. These are reference-wrapper observations, not NVIDIA ABI guarantees.
- Current Fallout has real resize/recreation and direct + Streamline NR routes.
- Pre-UI is not automatically pre-upscale. Recover actual Skyrim hooks/callers and ENB ordering rather than transplanting a DLL RVA.

## Implementation discipline

Own the whole frame contract: source-frame identity, current-generation resources, camera/jitter, depth/MV/colour semantics, valid fallbacks and complete fence/provider retirement. Exactly one SR provider, one FG provider and one presentation owner. Generated frames never advance Skyrim simulation or source temporal history.

Initialize providers in the required lifecycle order outside loader lock; verify the early Streamline/custom-hook path. Make replacements transactional. Preserve original colour for a valid display-sized fallback. Do not reset a D3D12 allocator or retire UI/FG inputs early. Do not use a fake-success ResizeBuffers or capability/evaluation stub.

Every task should produce real code and tests or a concrete RE result that feeds the implementation. Run targeted tests, then regression tests, before marking the task build-tested. Record missing GPU/game tests as NOT RUN. Use small commits and keep `docs/IMPLEMENTATION_STATUS.md` current. No repeated confirmation is needed simply to proceed through the approved scope or use the permitted methods.

At the end of the session report changed files, actual commands/results, working features, feature/runtime limitations, unresolved contracts, and the exact next executable task. A buildable milestone is progress, not a claim that all six original requirements already work.
