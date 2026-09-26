# 0.1.82 review applied to 0.1.86

Source: user-supplied `RazKolbas_Latest_Code_Improvement_Suggestions_0.1.82.md`
dated 2026-09-25. This review predates the V5.4 ENB/ReShade and native UI
work, so each finding was checked against the current source.

| Finding | Current disposition |
|---|---|
| Per-frame NR CPU wait and single command allocator (#3) | Implemented a three-slot retired D3D12 command ring and same-frame GPU fence handoff. Standalone NVIDIA 30-frame paced and unpaced tests passed. A user-started 0.1.86 Quality run reached 8400 NR submissions without NR/device/Present errors; NR off/on worked visually. Comparative frame time and NativeAA remain unverified. |
| Redundant prepared-input to NR-shared-input copies (#4) | Still present. Requires direct sharing validation and separate image comparison after the async route is game-tested. |
| Implicit depth-normalization requirement (#5) | 0.1.89 makes R32 depth an explicit pre-SR requirement. Full-frame normalization can be requested independently of cropping; cropping still requires conversion because D3D11 cannot partially copy the depth/stencil surface. Focused WARP tests cover both required R32 and an unrelated callback retaining R24 depth. Game test pending. |
| Whole-file NR hash and first-use initialization (#6) | Whole-file allocation removed with streaming SHA-256 and a chunk-boundary file test. Hashing and feature creation still occur on first use; hitch measurement/preflight relocation remain open. |
| Permanent disable after recoverable error (#7) | 0.1.88 retries a failed evaluation after a same-frame D3D11 fence wait/flush, resets NR history, and disables after three consecutive failures. Device removal and failed handoff remain immediately fatal. Policy/fence fault-injection tests pass; in-game recovery is pending. Other initialization/rebuild failures still require a separate typed classification. |
| Model-native UI correction without UI/alpha inputs (#8) | Forced to zero; menu control disabled; INI request retained but ignored by the NR evaluator. |
| GPU timing (#9) | Not implemented; no performance claim from CPU wall time. |
| DLSS Quality ghosting comparison (#10) | Pending user-started 0.1.86 game test. Motion-vector scales and depth convention were kept unchanged. |
| Versioned NR parameter writer (#11), Auto skin semantics (#12), reproducible openNR runtime (#13) | Pending independent evidence/work; no inferred ABI changes in this candidate. |

The exact next action is the V5.4 loaded-world runtime test. Do not infer a
Skyrim pass from the standalone bridge, and do not combine direct input
sharing or parameter changes with that validation.
