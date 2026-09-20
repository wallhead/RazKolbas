# Reference jitter and motion-scale contract

Status: **STATIC_OBSERVED**, 2026-09-20. The supplied files were read only:
`SkyrimUpscalerAIOBuild16-Hotfix1/SKSE/Plugins/SkyrimUpscaler.dll`
SHA-256 `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`
and `UpscalerBasePlugin/PDPerfPlugin.dll` SHA-256
`8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
These are evidence, not product dependencies. The RVA values below are relative
to their named DLL unless explicitly called game RVAs.

## Reference data flow

The Skyrim reference installer at `0x157b60` resolves game AE ID 77245 and
selects renderer Begin jitter CALL sites at addends `0xe5` or `0x133` on its
version branches. For the user's 1.6.1170 Address Library these map to game
RVAs `0xe44675` and `0xe446c3`; only their unpatched startup bytes have
previously been observed live. The hook helper at `0x159270` points to
reference callback `0x157190`. A separate helper at `0x1592c0` points to
`0x157ac0` and the installer derives a site from game AE ID 77518 at addend
`0x7a1` or `0x7a4`. It also derives game AE ID 77520 +`0x1d5`. The exact
decoded game bytes, existing owners and ABIs at those latter sites have not
been captured. These reference patch addresses are not RazKolbas patch
descriptors.

At `0x157190`, the reference callback first calls its retained original
target, then `PDPerfPlugin!GetJitterPhaseCount` and `GetJitterOffset`. It
increments its phase counter at state `+0x04` before converting it to an
integer. `GetJitterOffset` at PDPerf RVA `0x50040` uses
`phase % max(8, phaseCount) + 1`, evaluates a radical-inverse sequence in
bases 2 and 3 through `0x4ff60`, and subtracts the verified float constant
`0.5` from both results. The provider-specific phase count remains dynamic.
Let the resulting subpixel offsets be `(jx, jy)`. Reference constants at
RVAs `0x33a5a8` and `0x33a540` are `-2.0` and `+2.0`; the sign mask at
`0x33a7d0` negates floats. The callback writes:

```text
game camera projection +0x44 = -2 * jx / width
game camera projection +0x48 = +2 * jy / height
reference state +0x08       = -jx
reference state +0x0c       = -jy
```

The projection uses dimensions from reference state `+0x2c/+0x30`. The
callback zeros both camera-projection and reference-state offsets when its
SR/jitter gates are off. The other camera callback at `0x157ac0` makes an
indirect call, then copies the same state dimensions to camera-state
`+0x08/+0x0c` while enabled; that indirect callee's ownership is not yet
resolved.

Reference evaluation routine `0x1f4d8d..0x1f504f` builds the host's SR
payload. Its payload `+0x44/+0x48` (jitter X/Y) comes directly from reference
state `+0x08/+0x0c`. Payload `+0x4c/+0x50` (motion scale X/Y) comes from
float-to-integer-to-float conversions of reference state `+0x10/+0x14`;
payload `+0x38/+0x3c` dimensions come from state `+0x2c/+0x30`. The delayed
`PDPerfPlugin!EvaluateUpscaler` call is at `0x1f5022`. Its DLSS conversion
at PDPerf `0x4a900..0x4a988` forwards these fields to ordinary NGX D3D11.
The positive scale fields look dimension-sized in this path; their exact
engine meaning and the game's motion-vector direction still need validation.

The pinned NGX SDK's `nvsdk_ngx_helpers_d3d.h` says `InJitterOffsetX/Y` are
in input/render pixel space and `InMVScaleX/Y` convert motion vectors to
pixel space. NVIDIA's [Streamline programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md)
also specifies pixel-space jitter, although its common-constant motion scale
normalizes to `[-1,1]` and is therefore a different API contract.

## RazKolbas decision

Deployed 0.1.20 submits zero NGX jitter and positive display-width/height
motion scale on every persistent full-resolution DLAA frame. It does not
own the camera jitter or disable Skyrim TAA. The user's reduced-flicker
observation is encouraging, but it does not prove temporal correctness or
justify changing either value by guesswork. The static reference shows that
jitter supplied to NGX must correspond to the **same frame's** camera
projection, with an explicit sign conversion; independently advancing an
NGX-only Halton counter would break that pairing.

Before enabling a RazKolbas jitter/TAA replacement: verify the decoded
1.6.1170 patch-site bytes and forwarding ABIs under the existing exact-hash
and ownership policy; define one frame counter and a safe reset on scene or
render-size transitions; bind camera and NGX offsets to that frame; validate
motion direction, units and jitter inclusion; and retain native rendering if
the transaction cannot fully activate. Reduced-resolution SR also needs a
separate, validated world-render extent from the native UI/display extent.
No such hook or new game test is claimed here.

Reproduce the read-only disassembly with `tools/re/trace_sr.py --package
SkyrimUpscalerAIOBuild16-Hotfix1 --host skyrim --output
artifacts/local/sr-jitter-trace --rva 0x157190 --rva 0x157ac0 --rva
0x157b60 --rva 0x1f4e80 --rva 0x1f4f80`, and repeat for `--host pd` at
RVAs `0x50040`, `0x4fff0`, `0x4ff60`, `0x4ff90`. Generated assembly remains
ignored under `artifacts/local/`.
