# Existing DLSS runtime reuse — September 20, 2026

The supplied NVIDIA SR runtime has executed a captured Skyrim frame through
RazKolbas-owned D3D11 code. This is offline reset-only DLAA, not live Skyrim,
reduced-resolution SR, or a temporal-quality test.

## Recovered interface

`tools/re/trace_sr.py` accepts only SkyrimUpscaler SHA256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`
and PDPerfPlugin `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
It disassembles exception-directory function regions; leaf functions without
those records and chained regions require separate inspection. Output is static
evidence, not proof that every indirect call is resolved.

Skyrim SR call RVA `0x1f5022` passes a `0xb0`-byte payload to
`PDPerfPlugin!EvaluateUpscaler` (`0x4f670`), which copies it and dispatches via
virtual slot `0x28`. The DLSS routine `0x4a900`, continued at `0x4a988`, converts
payload fields to the ordinary NGX D3D11 structure. Helper `0x48dc0` writes named
NGX parameters and tail-calls `NVSDK_NGX_D3D11_EvaluateFeature_C`.

| Host payload offset | DLSS conversion |
|---|---|
| `0x08` | Color |
| `0x10` | MotionVectors |
| `0x18` | Depth |
| `0x20` | Bias current color mask |
| `0x28`, `0x30` | Output; nonnull `0x30` takes precedence |
| `0x38`, `0x3c` | Float dimensions converted to integer render subrect width/height |
| `0x44`, `0x48` | Jitter.Offset.X/Y |
| `0x4c`, `0x50` | MV.Scale.X/Y |
| `0x54` | Reset byte converted to integer |
| `0x68` | D3D11 context |

Creation routine `0x49dc0` sets dimensions, quality, feature flags and subrect
enablement, then calls ordinary SuperSampling feature `1` at `0x4a0a8`. These
contracts match the official NVIDIA D3D11 SDK helpers. They are separate from
the private NR `0x138`-byte payload and feature `0x12`. Upstream resource
transformations, virtual-dispatch selection and our live hook/jitter source
remain to be established.

## Implementation and result

`harness/SrReplay.cpp` uses official SDK commit
`374959484e79a640feaba44c93ac8cfb0a03f5b5` with its own project ID. It requires no
PureDark/Fallout host. It locks and verifies the supplied `nvngx_dlss.dll` SHA256
`c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e`, then checks the
module actually loaded by NGX has that same path/hash. This ordinary SR runtime
needs no signature workaround; community-patched NR remains a separate path.

Input: stationary capture `18080-136199593`, 2560x1440 RGBA16_FLOAT colour,
RG16_FLOAT motion and R24G8_TYPELESS depth. Only low24 depth bits are normalized
to R32_FLOAT. Experiment flags: HDR, MVLowRes, AutoExposure; reset=true,
jitter=(0,0), MV scale=(2560,1440), exposure=1. These are explicit experiment
settings, not verified live guide conventions. Captures do not contain jitter.

On RTX 4080 SUPER, init/capability/create/evaluate/release/destroy/shutdown all
returned `0x1`. A D3D11 event completed before mapped readback. NaN-initialized
output was overwritten with 11,059,200 finite RGB components; alpha also finite.
RGB range: 0.000307798–11.90625; 78.7469% of components differ from input. That
difference is not an image-quality measurement. Output SHA256:
`facc7b1e54bcc2f30fdcc3aac9c110c39e3732c5c8a1b2f782fb30967938642c`.
The independent validator reproduced this hash. Viewed preview and measurements:
`artifacts/local/sr-replay-validated/`. Both previews clip RGB to 0..1 and use
display gamma 2.2.

## Reproduce

```powershell
python tools/re/fetch_sr_sdk.py --output artifacts/local/ngx-sdk
cmake --preset win-release -DRK_NGX_SDK_ROOT="$PWD/artifacts/local/ngx-sdk"
cmake --build --preset win-release --target RazKolbasSrReplay
python tools/re/run_sr_replay.py --exe build/win-release/bin/Release/RazKolbasSrReplay.exe --runtime-directory SkyrimUpscalerAIOBuild16-Hotfix1/UpscalerBasePlugin --capture '<capture directory>' --output '<new output directory>'
```

Fetcher verifies Git blob hashes at the pinned commit and records SHA256.
CMake verifies Release/Debug library identities and selects the matching CRT.
Both configurations built successfully. The final source replay reproduced the
same output hash; all15 existing Release CTest groups passed. An injected
`RAZKOLBAS_SR_REPLAY_FAULT=throw_after_evaluate` exited10 with
`EXCEPTION_WITH_GPU_OWNERS` and no PASS, confirming the fatal-unwind path.
The runner's stale-output rejection was also exercised before child launch.
Runner checks input hashes, formats, row extents and finite values; refuses stale
outputs; invokes the isolated child with a 90-second timeout. GPU waits are
bounded to 20 seconds. Unexpected exceptions terminate before persistent GPU
owners unwind. SDK binaries, reference DLLs and captures stay out of git/MO2.

Next: use this NGX call path in the owned backend after recovering pre-SR
resource preparation and jitter/hook timing. No repeated stationary/pan test is
needed for that step. Installed 0.1.7 remains diagnostic; live DLSS is inactive.

## Reference hook map, next investigation

Static inspection of the exact supplied `SkyrimUpscaler.dll` install routine
`0x157b60` found two renderer Begin jitter modifications based on AE relocation
ID 77245 at addends `0xe5` and `0x133`. It also identified a call-site hook
labelled `Main_DrawWorld_MainDraw`, based on AE ID 82084 at addend `0x17a`.
`tools/re/map_skyrim_sr_hooks.py` decoded all 428,461 records from the exact
hash-verified 1.6.1170 Address Library and mapped these to game RVAs
`0xe44675`, `0xe446c3`, and `0xfa507a`, respectively. The latter is a stronger
reference for the world-draw stage than ENB Present, but its exact place
relative to UI and the SR input preparation remains unverified. The reference
also uses AE IDs 77518/77520 during jitter/camera hook setup; their final
patched sites require further instruction/data-flow tracing.

RazKolbas 0.1.8 adds one read-only live-byte log for each mapped site after
its exact game profile check. It writes no instructions there. This checks
whether the running modlist has already changed those sites before a new
hook descriptor is considered.
