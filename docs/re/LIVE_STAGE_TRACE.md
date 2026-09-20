# Loaded-world stage trace (Skyrim 1.6.1170, 2026-09-20)

Status: **LIVE_OBSERVED** for the logged resource identities, code bytes and
one offscreen DLAA evaluation. Presentation placement remains unresolved.
The user started Skyrim through MO2. The assistant only read process memory
and logs; it did not launch, patch, or control the game. Process PID 25160
loaded `D:/TESV_EX/SkyrimSE.exe`, SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`,
at base `0x7ff6f59a0000`. The bounded decoded-code snapshot and disassembly
are ignored under `artifacts/local/live-stage-2026-09-20-1149/`.

The AE Address Library ID 82084 function starts at RVA `0xfa4f00` and ends
at `0xfa52f5` according to the exact executable's unwind table. Its call at
`0xfa507a` was routed to RazKolbas's pass-through relay in this live process,
as expected; pristine bytes and the original target RVA `0xe44850` were
verified before patch installation. Immediately after that call, the function
calls a four-instruction indirect thunk at `0xfc32e0`. A bounded live pointer
walk of this instance reached virtual slot `+0x20` at game RVA `0x102ce50`,
whose first instruction is `ret 0`. Thus this particular post-world virtual
call was a no-op, not demonstrated UI composition. The function then loops
over a list at object `+0x110` with count at `+0x120`, invoking each entry's
virtual slot `+0x30` at `0xfa51d6`; the entries and their visual effects
were not identified. It later invokes another thunk at `0xfc3300`.

The separate function at RVA `0xfa3dc0` is real, but it is not AE ID 82084
for this executable. It also manipulates an object list. The
DynamicShaderFrameGen source comment equating ID 82084 with `0xfa3dc0`
therefore has an address mismatch; that source's claimed pre-UI semantics
cannot be adopted as a verified RazKolbas hook.

The 0.1.17 log first found an HDR scene RTV at the main menu. After the save
loaded, world-like depth was accepted on attempt 13: 96 distinct sampled
values, 95 non-far. At the **post-world** callback, RTV0 instead matched the
2560x1440 RGBA8 backbuffer (format 28); RTV1 was a distinct format-34
texture, while the candidate world colour remained a distinct RGBA16F
texture. At the following pre-ENB-Present observation, RTV0 still matched
the backbuffer. NGX evaluated one reset-frame DLAA image on the game device;
its fenced readback was finite and nonuniform with SHA-256
`864429cab53aa48d7bad66b79b538cbd0996da7ea718cd2aff1ec333cc715bd5`.
The installed 0.1.17 plugin discarded that output and performed no display
write. Present and forwarded-world counts matched through 15,000 with zero
logged failures. Game exit was observed afterward, without assistant control.

The supplied exact-hash `SkyrimUpscaler.dll` contains eight valid embedded
DXBC shaders. Read-only disassembly with `d3dcompiler_47.dll` is ignored under
`artifacts/local/sr-shaders/`. Its simple `mainTex` pixel shader at file offset
`0x337350` (RVA `0x338550`) samples a texture into `SV_TARGET`; the other
embedded pixel shaders cover UI-mask separation, texture comparison, motion
visualization and depth sampling. None shows a tone-map operation. The
reference draw helper chooses shaders through private object state, so the
exact active branch is still unverified. Our captured gameplay HDR scene
colour reaches RGB values above 1; sampling it directly into an RGBA8 UNORM
backbuffer would clip highlights. A compatible HDR-to-display path must be
measured or placed before Skyrim's own conversion.

The next bounded runtime measurement is a **same-frame stage pair**: current
HDR scene and post-world RGBA8 backbuffer, then that same backbuffer just
before ENB Present. This distinguishes input encoding and post-world callback
changes without writing display pixels. The new producer enforces matching
extent, format, backbuffer identity, world-frame ID and a 64 MiB total CPU
budget; it writes only a completed manifest under `RazKolbasCaptures`.

## User-launched 0.1.18 paired stage (2026-09-20)

The user-launched SkyrimSE.exe PID 23872 started at 12:18:09 with the installed
0.1.18 plugin. After a save reached world-like depth, world frame 6613 produced
a complete same-frame capture under ignored local `RazKolbasCaptures/` at
12:20:30. The manifest's three SHA-256 digests matched independent rehashes:
HDR scene `303e0b8dee7e5559ba01880113bbb0801c9e20cf5ce95ae3e47ffa2e86c71820`,
post-world RGBA8 backbuffer `96441c5b90bc32d7dbaaf2664990e57430cd7ef189bf49b5cd0d369fbcd8979e`,
and pre-ENB-Present RGBA8 backbuffer `328d07c70ba3c095969979356f8fc8cd1332a92cfe1bd9a981551cc91d795317`.
All are 2560x1440; scene is format 10 RGBA16F and both backbuffer stages
are format 28 RGBA8. The plugin made no display write. NGX's one-shot DLAA
evaluation produced a separate fenced, finite nonuniform offscreen result,
SHA-256 `10791db599dae89c0a31a5e8c3ac0d8c156fd3172021c4cff964f70135b93a87`.

Exactly 173,413 of 3,686,400 backbuffer pixels (4.704%) changed between
the two stages; alpha did not change. Differences are concentrated in the
upper corners/edge and lower centre, with no changes in the middle two
240-pixel rows. This distribution is consistent with UI composition, but
the capture alone does not identify the draw calls. HDR RGB exceeds 1 in
3.266% of pixels and is finite throughout. A direct HDR sample to RGBA8
would clip those highlights, while replacing the pre-Present image would
also overwrite the observed intervening work. The owned output path still
needs a verified pre-conversion attachment or measured HDR-to-display
conversion with preserved UI/ENB/ReShade order; visible SR remains NOT RUN.
