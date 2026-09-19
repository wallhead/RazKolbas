# Native NR bootstrap experiment — 2026-09-19

**T08 remains open.** Initialization, direct feature `0x12` creation, fenced submission, feature release and shutdown have executed. Evaluation and NR image output have not. This is a creation-only compatibility result, not a complete NR hardware-support claim.

## Verified inputs

- User-supplied **community-patched** `nvngx_dlssnr.dll`: SHA-256 `8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206`; 165,840,496 bytes; version 310.8.0.0. The user identifies it as the RTX 20/30/40 compatibility build. That hardware claim is user-reported; measured coverage below is narrower.
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

The probe loads only a working copy of the exact community-patched NR DLL, never Fallout/PureDark hosts, ReShade or RenoDX. D3D12 was created on **NVIDIA GeForce RTX 4080 SUPER**, probe LUID **0:101a9**. OS driver inventory reports **32.0.16.1692**. This is a standalone adapter selection; Skyrim's render adapter is not yet known.

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

## Signature compatibility and provenance

RUNTIME_OBSERVED, 2026-09-19: `Get-AuthenticodeSignature` reports **HashMismatch**, with an embedded NVIDIA Corporation Authenticode signer. The signature is retained but does not validate the modified bytes; calling this an unmodified, validly NVIDIA-signed runtime would be incorrect. The actual local file is `SkyrimUpscalerAIOBuild16-Hotfix1/UpscalerBasePlugin/nvngx_dlssnr.dll`; the user's reminder spelling `nvngx/_dlssnr.dll` does not exist locally.

`tools/Inspect-NrTrust.ps1` reproduces the read-only signature and SHA-256 report. The native experiment accepts only this exact community build, then calls `LoadLibraryExW` and the snippet's exports directly. It does not require a valid publisher signature. Windows loading succeeds with the signature mismatch. The caller-name IAT shim resolves the subsequent observed initialization failure; **this does not establish that the caller-name failure is a cryptographic signature check**. Both fresh-process bootstrap runs were repeated after retirement error handling was fixed, with the same results above.

This is the tested bootstrap compatibility route. No `WinVerifyTrust` hook, system certificate change, or driver file patch was needed. A separate core feature-loader signature gate has not been demonstrated. If later execution encounters one, capture its exact failing call and target identity before implementing a scoped compatibility patch. Do not turn a missing signature into either an unconditional reject of this known community build or unconditional acceptance of arbitrary modules.

## Parameter factory experiment

The installed driver contains `_nvngx.dll` under `C:/Windows/System32/DriverStore/FileRepository/nv_dispi.inf_amd64_b20cc8aeaed64fc2/`, SHA-256 `91d3742f0df3f2dd85141fa39ea257f7b9243c3e6755cd7fea894c721e7491aa`. This is a local experiment identity, not a portable installation path or a component to package.

STATIC_OBSERVED: parameter allocation export RVA `0x67f50` accepts an output pointer; destruction RVA `0x68030` takes the parameter pointer. Constructor `0x66b40` installs vtable RVA `0xb59b8`. Verified Set/Get byte offsets are pointer `0x00/0x40`, int32 `0x18/0x58`, uint32 `0x20/0x60`, float `0x30/0x70`; Reset is `0x80`. These match the overloaded parameter interface in the [pinned NVIDIA header](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/include/nvsdk_ngx_params.h). The [pinned snippet Init_Ext declaration](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/include/nvsdk_ngx.h) also corroborates the five-argument init ABI recovered from the reference.

RUNTIME_OBSERVED: `tools/re/probe_ngx_parameters.py` exact-hash gates the driver core and verifies export RVAs, the allocated object's vtable and all used slots. In an isolated process, without a core Init call, allocation returned `0x1`; all four typed round trips returned `0x1` with equal values; Reset removed the key (Get `0xbad00010`); destruction returned `0x1`; child exit was 0. The parent enforces a 30-second timeout. The core stays loaded until process exit. This proves the parameter factory and tested slots, **not** NR consumption of that object.

```powershell
python tools/re/probe_ngx_parameters.py C:/Windows/System32/DriverStore/FileRepository/nv_dispi.inf_amd64_b20cc8aeaed64fc2/_nvngx.dll
pwsh -File tools/Inspect-NrTrust.ps1
```

The logs are `artifacts/local/nr-parameter-probe.txt` and `artifacts/local/nr-trust.json`. Direct and delay-import inventories of this exact core and NR DLL contain no `WinVerifyTrust`/cryptographic trust imports; dynamically resolved or internal checks remain possible, so import absence is not proof that feature creation skips validation.

## Direct feature creation with the community runtime

STATIC_OBSERVED: `AllocateNRResource` at `0x31370` receives a resource descriptor (RCX), initial state (EDX), heap properties (R8) and output resource pointer (R9); it calls the device's `CreateCommittedResource`, with heap flags zero and no clear value. Default-heap resources receive a best-effort HIGH residency priority. `ReleaseNRResource` at `0x34e50` releases a nonnull resource. Scaling callback `0x33da0` reads `DLSSNR.Upscaling`, propagates Get failure, otherwise sets `DLSSNR.ScalingRatio` to `1.0f` and returns `0x1`. The leaf release callback has no `.pdata` entry; the trace now uses its matched PDB PROC extent.

`harness/NrFeatureExperiment.cpp` implements these callbacks and the exact creation keys from `SetCreationParameters` at `0x34ed0`. The concrete hypothesis uses 640x360 input/output, scaling 1, upscaling 0, render preset 0, PerfQualityValue 2, create flags `0x42`, and node masks 1. The core supplies parameters only; CreateFeature and ReleaseFeature are called directly on the hash-gated community NR module.

RUNTIME_OBSERVED on RTX 4080 SUPER: parameter allocation `0x1`; direct NR feature `0x12` creation `0x1`, nonnull handle, four resource allocations; owned command list submitted and GPU fence completed; feature release `0x1`, four resource release callbacks; parameter destruction `0x1`; shutdown `0x1`; shim restored; process exit 0. No valid NVIDIA signature was required along this tested route. This is not evidence that every NGX loader accepts the modified signature, or that evaluation can render a valid image. Raw first-run log: `artifacts/local/nr-feature-probe.txt`.

The feature probe requires allocation/release counts to balance before reporting retirement PASS. Any uncertain lifecycle failure exits the isolated child without unwinding retained GPU owners. The post-init call boundary also catches exceptions before the main device/shim unwind; a guard inside the experiment covers exceptions after command recording begins. The driver core is deliberately retained until process exit.

Repeatable runtime checks, including opt-in fault injection confined to each child:

```powershell
python tools/re/run_nr_probe.py --executable build/win-dev/bin/Debug/RazKolbasNrBootstrapProbe.exe --nr artifacts/local/nr-working/nvngx_dlssnr.dll --core C:/Windows/System32/DriverStore/FileRepository/nv_dispi.inf_amd64_b20cc8aeaed64fc2/_nvngx.dll --output artifacts/local/nr-regression
```

The four cases preserve the original init failure, inject a post-init exception, deliberately omit one resource release to require controlled failure, and repeat successful creation/retirement. This local GPU suite is separate from CTest and is not registered in CI, where the exact runtime and adapter are absent. Fault cases must return 10 without a retirement PASS; the positive case must return 0 with shutdown and shim restoration. No game host or on-disk binary patch is involved.

## Exact next executable experiment

Recover evaluation parameters, resource formats/states, dispatch and temporal contracts from `SetEvaluationParameters` and the Evaluate call path. Add owned known-image inputs, invoke the genuine NR evaluate export, fence execution and inspect readback against an unchanged-input control. Only a successful, nontrivial image result can support an NR rendering claim. Preserve both the original-init failure and creation/retirement regressions; T07 interop and T05 game integration remain open.
