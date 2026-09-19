# RazKolbas implementation status

Prepared 19 September 2026. The repository was empty when inspected. This handoff contains a plan and inherited static evidence; no product implementation, compiler run or game/GPU pass is claimed.

| Task | Deliverable | Implementation | Build validation | Runtime validation |
|---|---|---|---|---|
| T01 | Repository baseline, reference inventory and dependency selection | NOT STARTED | NOT RUN | NOT RUN |
| T02 | Buildable safe SKSE host and build/test conventions | NOT STARTED | NOT RUN | NOT RUN |
| T03 | Typed settings, capability policy and temporal invariants | NOT STARTED | NOT RUN | NOT RUN |
| T04 | Versioned patch manager and reversible file-patch tooling | NOT STARTED | NOT RUN | NOT RUN |
| T05 | Recover Skyrim hooks and the early provider bootstrap boundary | NOT STARTED | NOT RUN | NOT RUN |
| T06 | Capture real frame inputs and control render-domain metadata | NOT STARTED | NOT RUN | NOT RUN |
| T07 | Same-adapter interop and complete slot retirement | NOT STARTED | NOT RUN | NOT RUN |
| T08 | Early native NR bootstrap experiment and compatibility implementation | NOT STARTED | NOT RUN | NOT RUN |
| T09 | Single frame coordinator, transactional activation and valid fallbacks | NOT STARTED | NOT RUN | NOT RUN |
| T10 | Independent ImGui menu and safe input/settings integration | NOT STARTED | NOT RUN | NOT RUN |
| T11 | First complete DLSS Super Resolution pipeline | NOT STARTED | NOT RUN | NOT RUN |
| T12 | ENB world/display separation and verified processing order | NOT STARTED | NOT RUN | NOT RUN |
| T13 | FSR Super Resolution backend | NOT STARTED | NOT RUN | NOT RUN |
| T14 | XeSS Super Resolution backend | NOT STARTED | NOT RUN | NOT RUN |
| T15 | ReShade coexistence, controlled placement and depth publication | NOT STARTED | NOT RUN | NOT RUN |
| T16 | D3D12-backed presentation and real resize lifecycle | NOT STARTED | NOT RUN | NOT RUN |
| T17 | Native-resolution HUD/UI plane and generated-frame composition contract | NOT STARTED | NOT RUN | NOT RUN |
| T18 | DLSS Frame Generation and coordinated latency | NOT STARTED | NOT RUN | NOT RUN |
| T19 | FSR Frame Generation as an independent presentation provider | NOT STARTED | NOT RUN | NOT RUN |
| T20 | XeSS Frame Generation with its required XeLL path | NOT STARTED | NOT RUN | NOT RUN |
| T21 | Integrate genuine NR into Skyrim before SR | NOT STARTED | NOT RUN | NOT RUN |
| T22 | Advanced NR placement, artistic controls and resolve | NOT STARTED | NOT RUN | NOT RUN |
| T23 | Full combination testing, hardening and additional Skyrim profiles | NOT STARTED | NOT RUN | NOT RUN |
| T24 | Reproducible release, MO2 package and final implementation handoff | NOT STARTED | NOT RUN | NOT RUN |

## Initial next action

Run T01: inspect the current checkout and local tools/reference directory. Preserve any changes since the empty-repository snapshot. Continue T02–T04 and then the dependency graph; activate T08 NR investigation early.

## Per-session checkpoint

Record current task/substep, source commit, changed files, exact commands/results, test hardware/runtime identity, unresolved contract and the next executable action. Build/test skips and absent GPUs must remain visible. A negative unsupported-capability test is not a successful rendering test.
