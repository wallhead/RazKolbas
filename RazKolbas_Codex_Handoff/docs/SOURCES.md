# Sources and evidence scope

Prepared 19 September 2026. The plan's architecture, task names, interfaces, tests and defaults are proposed RazKolbas design, not claims about existing code. Research facts below refer to the previous handoff's files; they were read, not newly reverse engineered in this planning session. No game/GPU validation was performed in this planning session.

## Repository snapshot

- `https://api.github.com/repos/wallhead/RazKolbas`: public repository, `size=0`, default branch `main`, no assigned licence, created 2026-09-19T16:29:08Z.
- `https://api.github.com/repos/wallhead/RazKolbas/contents`: returned `This repository is empty.` during this session. Re-check at implementation start.

## Local references

- [R1] `re/REFERENCE_REPORT.md`: prior report; technical evidence only; new instructions supersede older prescriptions.
- [R2] `re/evidence/archive_hashes.json`, `skyrim_manifest.json`, `fallout_manifest.json`, `pe_inventory.json`: original archive/runtime identities.
- [R3] `re/evidence/fallout_pdb_identity.json`, `fallout_project_symbols.tsv`: matched Fallout 1.65 symbols.
- [R4] `re/evidence/pd_nr_abi.asm`, `skyrim_nr_caller.asm`, `skyrim_resolve_setting.asm`, `pd_resolve_selector.asm`: corrected PureDark wrapper observations.
- [R5] `re/evidence/fallout_rendering_xrefs.json`, `fallout_nr_dispatch_xrefs.json`: current-release lifecycle/NR landmarks.
- [R6] `re/evidence/skyrim_integration_anchors.json`, `pd_integration_anchors.json`: game/ENB/ReShade and backend investigation starting points.

## Primary documentation checked for this plan

- [W1] Microsoft Detours, Using Detours: https://github.com/microsoft/Detours/wiki/Using-Detours — signature agreement, transactions, thread enlistment, trampoline ownership.
- [W2] Microsoft VirtualProtect: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect — protection scope and executable-code cache coherency.
- [W3] Microsoft FlushInstructionCache (linked from W2): https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-flushinstructioncache — relevant API reference to consult during implementation.
- [W4] Microsoft ID3D12CommandAllocator::Reset: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset — allocator reset requires completion of its GPU work.
- [W5] NVIDIA Streamline ProgrammingGuide: https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md — early initialization, feature checks, device binding, lifecycle. Page inspected identifies version 2.14.1; pin the exact selected checkout, not a floating `main` dependency.
- [W6] NVIDIA Streamline Manual Hooking: https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideManualHooking.md — consult the selected-version manual path for custom game integration.

Other vendor/game source links and their previous inspection scope are retained in R1. During T01/T08/T11/T13/T14/T18–T20, read the exact current headers/runtime documentation you select. This plan deliberately does not invent a production NR header, assert an unverified latest SDK bundle, or hard-code a universal GPU-generation table.
