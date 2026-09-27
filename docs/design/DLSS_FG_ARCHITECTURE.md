# DLSS Frame Generation architecture for RazKolbas

## Goal and current boundary

Generate one additional displayed frame for each real Skyrim frame on a
supported GPU while keeping SR, optional pre-SR NR, ENB, ReShade and native
UI correct. The user's installed 0.1.106 build is still a D3D11 real-frame
renderer with a pass-through ENB/ReShade swap-chain observer; this design
does not imply FG is active. `General.Presentation=ProxyD3D12` and the FG
settings are schema entries, not an implemented proxy/provider.

The supplied RE25 packet is evidence. Its recovered host exporter prepares
data; the actual DLSS-G plugin operates at a Streamline-managed lower
D3D12 Present. Official NVIDIA guidance also places resource tagging,
constants, Reflex markers and options around that Present, and requires
correct input lifetime and a supported Off path for resize. See
`../re/AIO_DLSS_FG_RE25_AUDIT.md` and the
[DLSS-G](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
and [manual-hooking](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideManualHooking.md)
guides. Pin headers and binaries to a matching Streamline release before
shipping; the current online guide is 2.14.1, whereas the preserved local
binary set may use older structure versions.

## Ownership and frame flow

One startup-selected presentation owner must provide Skyrim and the active
ENB/ReShade wrappers a coherent D3D11-facing swap interface while owning a
single D3D12 lower swap that Streamline can upgrade. The owner must be
chosen before the affected swap chain is created; switching a menu setting
cannot hot-replace an existing live swap. The exact V5.4 creation and
wrapper chain needs a bounded trace and COM-proxy harness before any game
replacement. Unknown owner or a failed upgrade leaves the existing native
D3D11 path intact with FG effectively Off and a clear reason.

For each **real** source frame, the coordinator records a generation and
token; D3D11 scene/guide production and optional NR then SR run once. A
post-effects, pre-UI HUD-less display-sized image and a separate
display-sized UI colour/alpha image are required if generated frames must
keep menus and HUD sharp. Their exact placement relative to ENB/ReShade is
established by capture, not presumed. The final real image reaches the
lower D3D12 backbuffer once. Depth and motion guides may remain at render
size, but tags use their true extents, formats and states. The coordinator
normalizes motion scale for the selected Streamline guide resolution and
uses one coherent camera/jitter/reset record, including camera cuts,
loading and size-generation changes.

The presenting thread sends one token's constants, tags and options, then
calls the Streamline-managed lower Present **once**. Streamline may insert
generated frames; no extra Skyrim simulation, jitter advance, SR/NR
evaluation or manual application Present occurs. `requestedExtra=1`,
`effectiveExtra` from support/options, and `actualPresented` from
Streamline telemetry remain separate. Start with Off and one-extra On;
dynamic/multi-frame modes follow only after capability and visual tests.

## Resource and failure contract

Each guide/UI slot carries D3D11 producer completion, D3D12 copy/render
completion, the Streamline input-processing completion fence/value, and the
generation to which it belongs. Queue waits order GPU work; CPU allocator
reuse waits for its own completion. A slot is reusable only after all
consumers retire. Shared descriptors, handles, swap buffers and SL tags
remain valid for their declared lifecycle; a successful D3D11 copy does
not retire asynchronous FG input use.

FG enable is conditional on exact same-adapter LUID, supported Streamline
feature/status, a compatible selected presentation owner, coherent guides,
valid camera/reset data, and a complete UI role. On missing guides, title,
loading, pause, photo-like screens, resize, device loss or unsupported
state, select Off before the next affected Present while preserving the
ordinary real frame. Resize is a transaction: Off, drain outstanding
render/SL consumers, clear tags, release old buffers, perform real DXGI
resize with its real HRESULT, rebuild generation, then re-query support.
The current NR patched runtime does not imply any FG entitlement or bypass.

Latency markers must describe actual simulation, input, render submit and
Present boundaries. A marker placed only at Present does not describe
Skyrim's simulation timing. The first provider can leave advanced pacing
disabled until marker ownership is verified; avoid stacking Reflex sleep
with SSE Display Tweaks, driver limiters or the mod's own FPS limit.

## Acceptance boundary

Offline harnesses must prove COM identity/refcounts, real resize/error
propagation, same-LUID interop, one real Present, two independent retirement
fences and no generated-source advancement. WARP can test infrastructure,
but cannot prove DLSS-G support. On a user-started Skyrim run, verify
Streamline reports supported status and actual-presented count above one,
capture real and generated frames with world motion and native HUD/menu,
compare present cadence against simulation/jitter counts, and exercise
camera cut, inventory/MagicMenu, ENB/ReShade, Alt-Tab and resize. If any
gate fails, keep SR/NR and the normal presentation path usable and report
FG Off with the failing condition. No game launch is required to author or
review this design.
