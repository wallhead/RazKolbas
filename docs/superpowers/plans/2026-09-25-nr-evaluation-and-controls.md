# NR evaluation and controls implementation plan

**Goal:** prove the exact community DLSS-NR runtime can produce a nontrivial GPU image, then integrate the proven stage before DLSS SR and future FG.

**Pipeline invariant:** HUD-free scene -> NR -> DLSS SR -> optional FG/presentation -> native UI.

## 1. Align the settings model with the verified AIO controls

- Add config regression coverage for the shipping/default preset, style 0-7, recovered pass count 1-3, input scale semantics, explicit input HDR flag, and fixed before-SR placement.
- Update the typed schema, example INI, and ImGui controls. Keep unavailable backends/settings visibly experimental until the runtime path consumes them.
- Build and run the config and full unit suites.

## 2. Recover and codify the direct evaluation ABI

- Decode the matched-PDB `SetEvaluationParameters` and `Evaluate` traces into a typed parameter table.
- Add a platform-neutral mapping fixture where practical so key/type/offset behavior is regression tested.
- Record resource states and lifecycle evidence in `docs/re/NR_BOOTSTRAP.md`.

## 3. Extend the isolated GPU probe

- Create owned FP16 colour/output, RG16F motion, and R32F depth resources with deterministic input data.
- Set the recovered evaluation parameters and exact states, call `NVSDK_NGX_D3D12_EvaluateFeature`, submit, fence, and read back output.
- Require successful return, finite pixels, and a nontrivial difference from both unchanged output and input controls. Preserve the original-init failure and creation/retirement fault cases.
- Keep acceptance restricted to the selected fast-FP16 SHA-256 `91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7`; use the caller-name IAT shim. Add no global signature bypass unless a separately observed failing verifier requires one.

## 4. Integrate only the proven contract

- Add an NR backend/stage on the render adapter's D3D12 device before SR, with preserved original colour on failure and full fence/provider retirement.
- Map live settings and diagnostics, package/install the MO2 build, then stop for a user-started Skyrim test.

## Validation

- `ctest --preset win-dev -R "^unit.config$" --output-on-failure`
- `ctest --preset win-dev --output-on-failure`
- `python tools/re/run_nr_probe.py ...` against the exact local NR/core hashes
- Release build/package scan before MO2 installation
