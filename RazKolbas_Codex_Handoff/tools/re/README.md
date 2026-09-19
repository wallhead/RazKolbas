# Read-only binary research utilities

These Python standard-library utilities were used for **static inspection**. They never load a Windows DLL or call an SDK. They are deliberately small research parsers for the supplied trusted PE64/MSF7 inputs, not security-hardened parsers for arbitrary hostile files. They do not replace a full disassembler/decompiler.

## Inputs

Extract the original archives locally using an ordinary archive tool, keeping this layout:

```text
WORK/
  skyrim/     contents of the Skyrim 7z, unchanged
  fallout/    contents of the Fallout 7z, unchanged
```

`fallout/F4SE/Plugins/Upscaling.dll` and `Upscaling.pdb` must exist. The script verifies their CodeView GUID/age match. Do not mistake that for source-code or runtime-behavior verification.

## Metadata and symbols

Set `UPSCALER_WORK` to the extracted directory and `UPSCALER_AUDIT_OUT` to a **separate private scratch directory**. Run:

```powershell
$env:UPSCALER_WORK = 'D:\UpscalerResearch\work'
$env:UPSCALER_AUDIT_OUT = 'D:\UpscalerResearch\raw-audit'
python .\tools\re\binary_audit.py
```

Equivalent shell syntax:

```sh
UPSCALER_WORK=/path/to/work UPSCALER_AUDIT_OUT=/path/to/private-raw \
  python tools/re/binary_audit.py
```

The script reads PE versions, exports, ordinary and delay imports, debug identity, and selected text sections; it also extracts PDB symbol records. Its raw output is broader than the supplied evidence and can include irrelevant strings, duplicates, compiler paths and unrelated symbols. Do not distribute that output indiscriminately. The report's `fallout_project_symbols.tsv` is a filtered index, not the full raw stream.

## Static direct-call/string references

Generate a GNU objdump listing in the expected location:

```sh
objdump -d -M intel "$UPSCALER_WORK/fallout/F4SE/Plugins/Upscaling.dll" \
  > "$UPSCALER_WORK/Upscaling.asm"
python tools/re/trace_rendering.py
```

Keep both environment variables set to the same values as the metadata run. `trace_rendering.py` reads the raw `fallout_pdb_symbols.json` produced by that run, and writes selected `fallout_rendering_xrefs.json` into the private output directory. GNU objdump output syntax is expected; other disassemblers may require a parser change.

`Trace.refs(rva, size)` can be used interactively to select another exact region. Symbols and exception/unwind ranges help bound a region but are not equivalent: a PE unwind range can cover only a function fragment, and a leaf can lack one. Pass a verified explicit size where necessary. The direct-reference parser does not reconstruct all indirect calls, virtual dispatch, branch conditions or dynamic execution. A string reference is an anchor, not an execution trace.

## Limits

The scripts were exercised on the uploaded archives in the analysis environment. No Windows game, vendor runtime initialization or D3D GPU test was run. Their final syntax and selected metadata/excerpts were checked as recorded in `docs/re/ORIGINAL_PACKAGE_VERIFICATION.json` (historical checks, not a new run). Complete source recovery, a universal PDB parser, symbol downloading, auth analysis and installing runtimes are outside their scope.

These utilities are read-only by design; that describes these two scripts, not a project prohibition. Codex may add dynamic probes and byte-patching tools under the active RazKolbas policy. Set both environment variables explicitly; the scripts retain their original analysis-environment fallback paths.
