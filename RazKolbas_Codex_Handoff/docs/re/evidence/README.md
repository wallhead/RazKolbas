# Evidence index and interpretation

All binary addresses are relative to the exact hashes in `pe_inventory.json` unless a disassembly line shows a preferred virtual address beginning `180...`. Those listings use image base `0x180000000`; apply the loaded image base for runtime inspection. Never copy a mod DLL RVA into SkyrimSE.exe.

## Inventory

- `archive_hashes.json`: original 7z sizes, hashes and extracted file counts.
- `skyrim_manifest.json`, `fallout_manifest.json`: relative paths, sizes and SHA-256 values of extracted files. Listed files are **not** redistributed here.
- `pe_inventory.json`: all extracted DLL identities, PE versions, sections, exports, imports and delay imports. Compiler paths reduced to basenames. Metadata does not establish digital-signature trust or redistribution rights.
- `fallout_pdb_identity.json`: PDB stream count and GUID/age match against Upscaling.dll.
- `fallout_project_symbols.tsv`: filtered named project functions from the matching PDB; duplicate library/template noise and source-machine paths removed. Some compiler-generated lambdas/unwind helpers remain useful for navigation. A name is not a runtime trace.

## Narrow disassembly excerpts

- `pd_nr_abi.asm`: Init/Evaluate/Release wrappers. Evaluate's two 0x80-byte iterations plus three 16-byte copies and one 8-byte copy establish **0x138 copied bytes**.
- `skyrim_resolve_setting.asm`: reading the named INI resolve setting and storing the host field.
- `skyrim_nr_caller.asm`: construction of the NR payload, including host resolve setting → payload +0x114, and the delayed backend call.
- `pd_resolve_selector.asm`: helper reads +0x114. +0x104 is not established as resolve mode.
- `skyrim_nr_before_sr.asm`: the selected NR-before-upscaler path, not all possible pipeline modes.

These short extracts support interoperability findings; full proprietary disassemblies are not packaged.

## Cross-references and strings

- `fallout_rendering_xrefs.json`: current release's named rendering, resize, NR and compatibility functions and selected direct calls/string references.
- `fallout_nr_dispatch_xrefs.json`: NR preparation/selection lambdas and proxy resize dispatch. Its filename reflects the main investigation, but it also includes the two resize wrappers and shared resize implementation.
- `skyrim_selected_xrefs.json`, `pd_selected_xrefs.json`: selected current-binary regions, including delayed imports where resolved.
- `skyrim_integration_anchors.json`, `pd_integration_anchors.json`: string-reference anchors for ENB/ReShade/backend integration. A surrounding unwind-range start is not always the logical function entry.
- `*_strings.json`: filtered rendering/configuration strings with RVAs. These are **not complete string inventories**. Long embedded shader source, irrelevant strings, authentication/licensing text and build paths were omitted.

The JSON reference lists are lightweight static navigation aids. Conditional paths, unresolved indirect calls and possible interior data references must be checked in a proper disassembler before implementing a contract. Some referenced unnamed `sub_...` labels remain intentionally unresolved.

## Findings versus tests

The documents distinguish observed binary facts, public source/documentation, inference, proposed design and unresolved work. File checks in `../ORIGINAL_PACKAGE_VERIFICATION.json` (historical record) establish package integrity and selected static observations only. They do not validate an NR runtime bootstrap, ENB order, actual motion-vector semantics, hardware support, a new plugin build or in-game correctness.
