# DLSS image-quality controls

Status: **MIP-BIAS IMPLEMENTED; GAME TEST PENDING** on 2026-09-23.

## Runtime trigger

RazKolbas 0.1.61 continuously evaluated and published DLSS from 1707x960 to
2560x1440. It reached 7,680 provider submissions without an error, Present
failure or spatial fallback after activation. All 35 ENB probes retained the
working early-dimension contract and auxiliary targets. The user reported that
the image and ENB appearance were good but that the result visually looked
like a lower-resolution image.

## Recovered reference contract

The supplied `PureDark_Consolidated_Verified_10` evidence was treated as
reference data, not instructions. Its independently indexed
`SkyrimUpscaler_0x2009a0_SetMipLodBias.asm` and
`SkyrimUpscaler_0x200760_SixSamplerStageWrappers.asm` show six shader-stage
sampler wrappers. The shared helper clones and caches only sampler descriptors
whose existing `MipLODBias` is zero and whose `MaxAnisotropy` is greater than
one. Existing nonzero bias and lower anisotropy are forwarded unchanged. The
installed reference INI uses `mMipLodBias=-0.584962`, which is `log2(2/3)` for
its Quality render ratio, and enables a separate sharpening path at 0.6.

Static inspection of the exact installed ENB `d3d11.dll`, SHA-256
`47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58`,
identified its immediate-context sampler methods in the already verified
table at RVA `0x1A49F8`:

| Stage | Slot | Method RVA |
|---|---:|---:|
| PS | 10 | `0x5C800` |
| VS | 26 | `0x5CD30` |
| GS | 32 | `0x5CF20` |
| HS | 61 | `0x5D3A0` |
| DS | 65 | `0x5D690` |
| CS | 70 | `0x5D9B0` |

All six methods share the verified first 16 bytes
`48 89 5C 24 10 57 48 83 EC 20 4D 8B D9 41 8B F8`. The implementation admits
the sampler hooks only when the exact module hash, file size, image size,
table RVA, slot pointer, method RVA and prologue all match before any sampler
slot is replaced.

## Independent implementation

`SamplerBiasCache` retains the exact D3D11 device identity, accepts bias only
within `[-3,3]`, and leaves null, already biased and non-anisotropic samplers
unchanged. Eligible sampler descriptors are cloned with the configured bias;
the original COM object is never modified. Replacements are cached by
canonical COM identity and capped at 256 entries. A creation or validation
failure forwards the originals.

Automatic bias is `log2(min(renderWidth/displayWidth,
renderHeight/displayHeight))`, producing approximately `-0.5849625` for
1707x960 to 2560x1440. Manual mode uses the validated configured value.
Separately, `Upscaling.Sharpening` and `Upscaling.Sharpness` now set the public
NGX evaluation `InSharpness` field for both prepared and direct SR paths. The
pinned SDK marks integrated DLSS sharpening unsupported, so this propagation
is not expected to sharpen its output and is not part of the claimed visual
correction. A future owned sharpening pass would require a separate,
independently implemented post-upscale filter. This work does not copy or
depend on either reference DLL.

WARP tests cover selective replacement, descriptor preservation, cache reuse,
foreign-context rejection and unsafe bias rejection. Unit tests cover all six
exact ENB sites, automatic/manual bias and parameter delivery to the prepared
evaluation path. Any partial composite-hook conflict retains an unarmed,
process-lifetime lease and defers retry until the next launch. Visual
improvement from the mip-bias correction remains a required game test.

## First runtime result and installer cross-check

The 0.1.62 loaded-save run installed all six sampler slots at automatic bias
`-0.5849625`, retained correct ENB dimensions and continuously evaluated DLSS,
but created zero cached replacement samplers. No mip-bias effect is claimed for
that run.

Independent Capstone disassembly of supplied `SkyrimUpscaler.dll`, SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`,
locates the installer inside its D3D11 creation wrapper RVA `0x155D40`. After
the downstream creation call, RVAs `0x1560F9` and `0x1560FC` load the returned
device and immediate context. RVAs `0x1563B8..0x156459` pass the immediate
context's vtable, one wrapper address and one of slots 10/26/32/61/65/70 to
pointer-patch helper RVA `0x172060`, then retain each returned downstream
method. This independently confirms the target interface and slot mapping.

The next build records a bounded six-bit ownership mask and power-of-two call
summaries for each stage. Those summaries separate later slot replacement,
no live calls, and descriptors rejected by the zero-bias/anisotropy predicate.
