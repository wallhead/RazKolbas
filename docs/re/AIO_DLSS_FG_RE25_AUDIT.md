# AIO DLSS Frame Generation RE25 local audit, 2026-09-27

The owner supplied `C:/Users/user/Downloads/AIO_DLSS_FrameGen_RE_25.zip`
(SHA-256 `543af57eb30cf85486c0826e8311fe1bc73081f0e687bf4c47b355f29290a41b`).
The packet's `CODEX_HANDOFF.md` and implementation suggestions are reference
material, not instructions to RazKolbas. The archive and original binaries
remain outside git and were not modified.

An independent Windows check verified all 121 manifest payload hashes and
the exact 122-file ZIP entry set. The four original local AIO binaries match
the report's byte lengths and SHA-256 identities: `SkyrimUpscaler.dll`
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`,
`PDPerfPlugin.dll`
`8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`,
`sl.dlss_g.dll`
`b8b5effd7debdb750abd216de43385fb653261712bc315d85eba68811fb3ee02`,
and `nvngx_dlssg.dll`
`5d5cbf14d2727d47f93fd10bf77bd91708ae122482a6f86fd564971641ebd47b`.
Read-only PE mapping independently matched all 42 bounded code-span hashes:
3 host, 36 companion backend and 3 Streamline plugin spans. Capstone 5.0.7
spot-checked the host `dec eax` at H:`0x1F4878`, the backend tag call at
P:`0x18BD9`, the lower swap Present call at P:`0x43720`, and the fence-record
call at P:`0x19BAF`. This confirms those instruction anchors, not every
semantic label or branch in the packet. Its 216 backend plus 75 host
original-CPU assertions per compiler were **packet-reported, not rerun
locally**. No Skyrim/Streamline GPU execution or generated-frame capture
was performed for this audit.

The strongest supported contract is that the host's
`EvaluateFrameGeneration` export prepares a private 0xB0-byte packet; it is
not NVIDIA FG evaluation. The companion maps D3D11 inputs to persistent
D3D12 guide slots, then submits constants/tags/options to Streamline before
one lower swap-chain Present. The Streamline plugin owns interpolation at
that Present boundary. The packet reports Streamline feature 1000, separate
from NGX NR feature `0x12`, and AIO provider selector 2. These number spaces
must not be conflated. NVIDIA's [DLSS-G guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
independently describes Present-driven generation and per-frame resource
tags; its current 2.14.1 API is conceptual cross-checking, not proof that
the preserved 2.13-era binaries use identical ABI fields.

The packet's useful implementation observations are:

- The host converts total frames `N` to `N-1` generated frames; start with
  exactly one extra frame and keep requested, supported and actually
  presented counts separate.
- Motion vectors and depth are required guide roles; HUD-less colour and
  UI colour/alpha are distinct roles and may be at display resolution. Tag
  extents come from the actual resource descriptor, not an assumed render
  size. The reference's literal resource-state declarations are not GPU
  barriers and must not be copied without tracing our actual states.
- The packet's pixel-scale motion values are normalized before Streamline
  constants. Importing SR motion scales directly would double-scale them.
  Camera matrices, cut/reset provenance and live formats remain unproved.
- The reference records an SL input-processing completion fence separately
  from its own render/copy fence. A ring slot cannot be reused merely because
  an NR or copy queue finished. CPU allocator reuse may also wait.
- A prepared byte and a nonzero success return from the host exporter do
  not prove a generated frame. The status and actual-presented count must
  be observed after Streamline's lower Present.

The official [manual-hooking guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideManualHooking.md)
requires its common Present path to run every frame when manual hooking is
used. The [DLSS-G guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
also requires coherent frame tokens, guide/UI lifetimes through Present,
and turning FG Off before resize/window transitions. None of this is
provided by RazKolbas's current pass-through D3D11 Present observer.

Current RazKolbas has typed FG settings and `FrameIdentity`, but no FG
provider, Streamline initialization, D3D12-backed presentation owner,
HUD-less/UI split for generated frames, or SL-consumption retirement.
The direct NR bridge in `src/backends/nr/NrStage.cpp` is private to NR and
cannot be treated as an FG swap-chain proxy. The current
`RendererBootstrap::createProxy` name is historical: it forwards
`D3D11CreateDeviceAndSwapChain` and observes the returned swap. The exact
ENB/ReShade swap observers also forward Present/Resize. Thus an FG toggle
cannot make the installed 0.1.106 build generate frames. No vendor DLL or
reference host DLL was staged or installed by this audit.

Next implementation sequence: see
`docs/superpowers/plans/2026-09-27-dlss-fg-integration.md`. Before enabling
FG in Skyrim, establish one early D3D12/Streamline presentation owner,
same-adapter shared guide slots with both render and SL-consumption
retirement, correct per-real-frame constants and markers, and a native UI
role that survives generated frames. The first game run must measure an
actual extra frame and check HUD/menu correctness; packet CPU probes and
ordinary Present counts cannot substitute for that result.
