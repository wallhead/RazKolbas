# Reference jitter and motion-scale contract

Status: **STATIC_OBSERVED** for the reference and **LIVE_READ_ONLY** for game
bytes, 2026-09-20. The supplied files were read only:
`SkyrimUpscalerAIOBuild16-Hotfix1/SKSE/Plugins/SkyrimUpscaler.dll`
SHA-256 `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`
and `UpscalerBasePlugin/PDPerfPlugin.dll` SHA-256
`8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`.
These are evidence, not product dependencies. The RVA values below are relative
to their named DLL unless explicitly called game RVAs.

## Reference data flow

The Skyrim reference installer at `0x157b60` resolves a version-dependent
pair that includes game AE ID 77245 for Renderer Begin. One branch passes
base `+0xe2` to its five-byte CALL helper `0x159270`; the alternative branch
passes `+0xe5`. Another branch passes `+0x133` or `+0xe5`. The reference
installer uses helper `0x1592c0` for a separate `GetClientRect`-area hook;
it does **not** use that helper for the camera hook. Earlier notes incorrectly
assigned this helper and addends `+0x7a1/+0x7a4` to jitter. Those addends
belong to the adjacent screen-size hook, not camera jitter.

For the later `updateJitterHook` and `buildCameraStateDataHook` descriptors,
the reference selects the second of two relocation IDs when its internal
version byte is 1, and the first when it is 2. The first pair is 75709/75711;
the second is 77518/77520. It copies six original bytes from the selected
first ID `+0x11` (version 1) or `+0xe` (version 2), and ten from the selected
second ID `+0x1d5`. It then supplies patch descriptors for the selected
first ID at that `+0x11/+0xe` site and again at `+0x1d5`. The latter is a
distinct patch target; the selected second ID `+0x1d5` is a **source** of
camera bytes, not the target. The descriptor writer's full patch semantics
and the game's matching version branch remain unverified.

The user-started exact game `D:/TESV_EX/SkyrimSE.exe` (SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`,
PID 28592, image base `0x7ff6f59a0000`) was read without modifying it. The
hash-verified 1.6.1170 Address Library maps ID 77245 to `0xe44590`, 75709
to `0xe01ac0`, 75711 to `0xe01c90`, 77518 to `0xe58a10`, and 77520 to
`0xe58b80`. At `0xe44672` (77245 `+0xe2`) the live bytes
`e8 99 43 01 00` are an aligned CALL to `0xe58a10`. At `0xe446c3` (77245
`+0x133`) the bytes begin `48 8b 01`, an aligned MOV rather than a CALL.
At `0xe58a21` (77518 `+0x11`) the first six bytes are
`80 7a 18 00 74 68`, an aligned TAA-gated compare/branch followed by the
game's eight-phase jitter counter. At `0xe58d55` (77520 `+0x1d5`) the ten
source bytes `80 79 18 00 0f 84 c0 00 00 00` are aligned. But the other
candidate target, `0xe58be5` (77518 `+0x1d5`), lands **inside** the live
three-byte `mov r9,r14` at `0xe58be3`. This blocks transplanting the
reference's patch sites into this executable. No camera/jitter patch was
installed; exact runtime ownership and ABIs remain to be established.

The decoded game code suggests a narrower independent route. The valid
Renderer Begin CALL invokes game routine `0xe58a10`, whose gated eight-phase
path writes its own normalized jitter to the camera object's `+0x44/+0x48`.
The caller's `lea rcx` at `0xe44664` resolves to game RVA `0x328cc20`. The
separate camera-builder function starts at `0xe58b80`; its branch at
`0xe58d55` reads `+0x44/+0x48` when the game gate is enabled. RazKolbas can
sample that exact camera object from its existing post-world callback without
installing another patch. Source 0.1.21 now does a bounded read after the
original world call on early and periodic frames, gated by the exact caller,
CALL and target bytes. It logs the object's dimensions and projection offsets
only; it does not change NGX jitter or Skyrim TAA. The source build and local
tests pass. In the user-started 0.1.21 run, the first 12 world frames at
2560x1440 yielded a finite eight-sample cycle: converted pixel offsets were
`(-.25,-.166667)`, `(.25,.388889)`, `(-.375,.055556)`,
`(.125,-.277778)`, `(-.125,.277778)`, `(.375,-.055556)`,
`(-.4375,-.388889)`, `(0,.166667)`, then the first four repeated.
The `x=projectionX*width/2`, `y=-projectionY*height/2` conversion follows
the reference's projection/payload signs. All samples passed validity checks;
the observation alone left NGX jitter unchanged. The same run logged more
than 25,000 world frames and continuous DLAA submissions without jitter-read
or DLAA-disable errors at inspection time.

Source 0.1.22 now reads the camera object immediately after the original
world call for each prospective DLAA frame and passes that converted sample
to NGX. It requires the camera dimensions to match the backbuffer, finite
offsets within a half pixel and the previously verified caller/CALL/target
bytes. If the sample is unavailable, it leaves that frame native and marks
NGX history for reset on the next accepted frame. The Release build passed
all 22 local test groups. A standalone NVIDIA replay used the 0.1.21
post-world SDR capture, synthetic guides and the measured eight-sample jitter
cycle for 30 DLAA submissions; its changed output SHA-256 was
`863ef90ba5686d8d464baed17a2a5985c50f90653c9b174f644c6f035cadf322`,
and teardown completed. This does not validate game motion vectors or
in-game 0.1.22 behavior.

The user-started 0.1.22 game run loaded the installed DLL (SHA-256
`ee337006f7ea10988f6a26222299046737eac46ad659bf8f1042ae7efa05975b`)
and signed NVIDIA runtime. Continuous DLAA began at world frame 8417 with
logged NGX jitter `(-0.25,-0.16666669)`, matching the decoded game-camera
sample for that phase. At inspection, the run had reached 5,400 submitted
DLAA frames with skipped=0; no jitter-read rejection, busy-slot message or
continuous-path disable appeared. World and Present counters matched through
at least 13,800 with Present failures zero. The user reported that the moving
scene looked good. That is a positive visual report, not a measured comparison
of final ReShade pixels or proof of motion-vector convention. Skyrim TAA
remains active, and reduced-resolution SR, FG and NR are still unimplemented.
Skyrim TAA remains active; motion-vector jitter convention and visual quality
remain open.

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

Before enabling a RazKolbas jitter/TAA replacement: identify compatible,
aligned hook sites and verify their forwarding ABIs under the existing
exact-hash and ownership policy; define one frame counter and a safe reset on
scene or render-size transitions; bind camera and NGX offsets to that frame; validate
motion direction, units and jitter inclusion; and retain native rendering if
the transaction cannot fully activate. Reduced-resolution SR also needs a
separate, validated world-render extent from the native UI/display extent.
No such hook or visual game test is claimed here. The live inspection was
read-only while the user-started game was at its main menu.

Reproduce the read-only disassembly with `tools/re/trace_sr.py --package
SkyrimUpscalerAIOBuild16-Hotfix1 --host skyrim --output
artifacts/local/sr-jitter-trace --rva 0x157190 --rva 0x157ac0 --rva
0x157b60 --rva 0x1f4e80 --rva 0x1f4f80`, and repeat for `--host pd` at
RVAs `0x50040`, `0x4fff0`, `0x4ff60`, `0x4ff90`. Generated assembly remains
ignored under `artifacts/local/`.
