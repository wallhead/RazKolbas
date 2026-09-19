# Native NR bootstrap experiment — 2026-09-19

**T08 remains open.** Initialization and shutdown have executed; genuine feature creation, evaluation and GPU output have not. No NR hardware-support claim follows from the patched initialization result.

## Verified inputs

- `nvngx_dlssnr.dll`: SHA-256 `8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206`; 165,840,496 bytes; version 310.8.0.0.
- Reference host `Upscaling.dll`: `f2633bcf11f618569eac77d1c7e6bf136a498fdb53f5eb1f205bfbb3a74cac7b`.
- Matched Fallout PDB: `7057e107-1a03-4c53-ab14-7cd63ad16f3e`, age 71, freshly compared with DLL CodeView identity.
- Tools: inherited standard-library PE/PDB parser plus Capstone 5.0.6; native probe built with MSVC 14.44.35207, Windows SDK 10.0.26100.0, Debug.

## New static evidence

`tools/re/trace_nr.py` recovers named function instruction listings using the matched PDB. It uses PROC extents because chained `.pdata` unwind regions can cover only part of a function. Listings are under ignored `artifacts/local/nr-trace/`; regenerate with:

```powershell
python -m pip install --target artifacts/local/python capstone==5.0.6
python tools/re/trace_nr.py --reference-root . --output artifacts/local/nr-trace
```

STATIC_OBSERVED:

- `LoadRuntime` at 0x33120 resolves `NVSDK_NGX_D3D12_Init_Ext`, creation/evaluation/release/shutdown exports. It looks for a shared loaded NGX module to provide the complete feature-operation set and parameter factory.
- `InstallModuleNameHook` at 0x32c70 validates the PE import directory, searches `GetModuleFileNameW`, checks the original pointer, and writes a proxy. It runs before the Init_Ext call in `Prepare`.
- Proxy 0x33e10 substitutes `nvngx.dll` only when asked about the reference host module; it forwards other handles. Its short-buffer behavior is visible in the listing.
- `Prepare` at 0x3424e–0x34278 calls Init_Ext with appId `0x0876232c`, a wide data directory, a D3D12 device, SDK version `0x15`, and null fifth argument. These are experimentally reproduced reference arguments, not production vendor registration or a public ABI guarantee.
- `RestoreModuleNameHook` at 0x34e80 checks ownership before restoring its IAT pointer.
- `EnsureFeature` calls feature ID `0x12` at 0x3195d, after parameter allocation and `SetCreationParameters`. Creation parameters include allocation/release callbacks and dimensions. Further factory/callback recovery is required before calling it.

## Controlled runtime comparison

The probe loads only a working copy of the exact vendor NR DLL, never Fallout/PureDark hosts, ReShade or RenoDX. D3D12 was created on **NVIDIA GeForce RTX 4080 SUPER**, probe LUID **0:101a9**. OS driver inventory reports **32.0.16.1692**. This is a standalone adapter selection; Skyrim's render adapter is not yet known.

| Run | Init_Ext raw result | Shutdown1 raw result | Patch restore | Process exit |
|---|---|---|---|---|
| Original, shim off | `0xBAD00002` | Not called after failed init | N/A | 5 |
| Exact runtime, caller-name shim on | `0x00000001` | `0x00000001` | Restored | 0 |

Both runs used fresh processes, the same working-copy DLL hash and device selection. The external runner imposed a 30-second timeout. Logs are `artifacts/local/nr-original-probe.txt` and `artifacts/local/nr-shim-probe.txt`.

```powershell
.\build\win-dev\bin\Debug\RazKolbasNrBootstrapProbe.exe artifacts/local/nr-working/nvngx_dlssnr.dll
.\build\win-dev\bin\Debug\RazKolbasNrBootstrapProbe.exe artifacts/local/nr-working/nvngx_dlssnr.dll --caller-shim
```

The second run used the aligned atomic pointer patch primitive tested against original-pointer mismatch and subsequent-owner replacement. The exact experimental patch record is `patches/nr/caller-name.json`. No on-disk reference patch was applied.

INFERRED: the changed caller-name response resolves this initialization failure in this exact environment. The experiment does not prove that the RTX 4080 SUPER can evaluate the NR model, that the runtime produces useful pixels, or that its full lifecycle is safe in Skyrim.

## Exact next executable experiment

Recover/select the parameter factory used by `LoadRuntime`, verify the `NVSDK_NGX_Parameter` vtable and resource allocation/release callback ABI at `SetCreationParameters` 0x34ed0. The NR DLL does **not** export AllocateParameters/DestroyParameters. Locate the corresponding NGX core provider or reconstruct an independently verified parameter object. Then create feature `0x12` on an owned D3D12 command list, record its raw result, fence submissions and release it. Only after successful creation should the known-image evaluate/readback sequence be wired. Preserve the unmodified failure as a regression and keep T07 interop work independent.
