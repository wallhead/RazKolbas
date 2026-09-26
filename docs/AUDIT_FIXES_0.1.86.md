# 0.1.82 review applied to 0.1.86

Source: user-supplied `RazKolbas_Latest_Code_Improvement_Suggestions_0.1.82.md`
dated 2026-09-25. This review predates the V5.4 ENB/ReShade and native UI
work, so each finding was checked against the current source.

| Finding | Current disposition |
|---|---|
| Per-frame NR CPU wait and single command allocator (#3) | Implemented a three-slot retired D3D12 command ring and same-frame GPU fence handoff. Standalone NVIDIA 30-frame paced and unpaced tests passed; Skyrim test pending. |
| Redundant prepared-input to NR-shared-input copies (#4) | Still present. Requires direct sharing validation and separate image comparison after the async route is game-tested. |
| Implicit depth-normalization requirement (#5) | Still coupled to the pre-SR callback. Keep current proven R32 depth path until a separate typed preparation-policy change. |
| Whole-file NR hash and first-use initialization (#6) | Whole-file allocation removed with streaming SHA-256 and a chunk-boundary file test. Hashing and feature creation still occur on first use; hitch measurement/preflight relocation remain open. |
| Permanent disable after recoverable error (#7) | Still present. Separate typed failure and retry policy requires a fault-injection regression. |
| Model-native UI correction without UI/alpha inputs (#8) | Forced to zero; menu control disabled; INI request retained but ignored by the NR evaluator. |
| GPU timing (#9) | Not implemented; no performance claim from CPU wall time. |
| DLSS Quality ghosting comparison (#10) | Pending user-started 0.1.86 game test. Motion-vector scales and depth convention were kept unchanged. |
| Versioned NR parameter writer (#11), Auto skin semantics (#12), reproducible openNR runtime (#13) | Pending independent evidence/work; no inferred ABI changes in this candidate. |

The exact next action is the V5.4 loaded-world runtime test. Do not infer a
Skyrim pass from the standalone bridge, and do not combine direct input
sharing or parameter changes with that validation.
