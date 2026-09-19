# RazKolbas — Codex implementation handoff

This package is the implementation plan and research handoff for `wallhead/RazKolbas`. **It contains documents, prior static evidence and analysis scripts—not a compiled upscaler or a pre-tested game integration.** Nothing has been pushed to GitHub by this handoff.

Place these files in the RazKolbas checkout, preserving any work already there, and give Codex `CODEX_PROMPT.md`. `AGENTS.md` records the project's working rules. The full task plan is `docs/plans/2026-09-19-razkolbas-implementation.md`.

Reverse engineering, debugging, compatibility shims and controlled runtime/on-disk byte patches are explicitly permitted. The new `PATCHING_POLICY.md` replaces blanket restrictions from earlier prompts with reproducibility, version validation, ownership, rollback and runtime tests. The historical report is a reference, not a conflicting instruction source.

The original two archives are still needed for deeper RE. Set a local `RAZKOLBAS_REFERENCE_ROOT` or provide their directory to Codex. They are not duplicated in this ZIP. The package includes the prior evidence, matched-PDB function index and analysis scripts, so basic navigation does not have to be repeated.

## Contents

- `CODEX_PROMPT.md`: launch task.
- `AGENTS.md`: repository-level working instructions.
- `docs/SPEC.md`: six original requirements plus RE/patch permission and architecture.
- `docs/plans/2026-09-19-razkolbas-implementation.md`: dependency-ordered tasks with files, tests, acceptance gates and concrete probe questions.
- `docs/PATCHING_POLICY.md`: permitted methods, patch records and safe lifecycle.
- `docs/testing/ACCEPTANCE_MATRIX.md`: original matrix updated for RazKolbas plus patch/bootstrap/fallback tests.
- `docs/IMPLEMENTATION_STATUS.md`: honest initial status and checkpoint format.
- `config/RazKolbas.ini.example`: proposed configuration; not an existing working plugin config.
- `docs/re/`: reference findings, prior report and evidence.
- `tools/re/`: inherited analysis scripts, not newly re-run binary analysis.
- `docs/SOURCES.md`: repository snapshot and documentation references.
- `SHA256SUMS.txt`, `HANDOFF_VERIFICATION.json`: package-integrity and document checks only.

All new task names, project interfaces, presets and commands are an implementation contract. Codex must create them; their presence in the plan is not a claim they already exist or pass. The initial milestone is a loadable safe host and verified pipeline slice; completion requires the full requirement/test matrix.
