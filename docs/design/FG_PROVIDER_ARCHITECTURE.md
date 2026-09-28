# Swappable frame-generation providers

## Implemented boundary

`FgSourceFrame`, `FgCapability`, `FgDecision`, `FgProviderSession`,
`FgPresentLedger`, and `FgRetirementSet` form a vendor-neutral policy layer.
`FgPresentationCoordinator` now controls one backend mode and one lower real
Present per source frame. Its backend interface requires Off/drain before
presenting an ordinary frame after FG was active; a failed enable falls back
to the real frame. The lower call carries the exact DXGI Present or Present1
method, sync interval, flags, Present1 parameters and HRESULT. Test Presents
are forwarded with per-call generation suppressed, without cycling persistent
provider mode or consuming a real-source token.
Failed Off/drain can retry that source; once lower Present is attempted its
token is consumed even if the result is uncertain. These interfaces do not
yet create generated frames. A
session selects at most one provider
when its presentation owner is created. Auto prefers a validated DLSS-G
capability, then a validated FSR 3.1 FG capability on NVIDIA. AMD and Intel
adapters can select only a validated FSR capability. A runtime failure turns
the bound provider Off; it never hot-swaps to another live swap chain.
Requested provider, effective provider, and the Off reason remain distinct.
Enabling/disabling FG is a live request; changing its provider is restart-
scoped until a safe drain/rebuild transaction exists.

This adapter rule supersedes the earlier NVIDIA-only FG policy at the owner's
request. It is a policy allowance, not a hardware-support claim: the selected
adapter must pass the actual FSR requirements and backend checks. The pairing
with the chosen SR provider must be validated. The existing XeSS FG schema
choice remains reserved and has no bound backend. FSR 4 ML FG is outside this
scope. NR and the currently shipping DLSS SR path remain separate from this
FG adapter policy; AMD/Intel do not gain those features through this change.

## One owner, interchangeable lower backend

Skyrim, ENB, ReShade and Display Tweaks must see one coherent D3D11-facing
swap chain. A startup-selected owner controls exactly one lower D3D12
presentation path. Its provider implementation is either a Streamline-managed
DLSS-G lower swap or an FSR 3.1 frame-interpolation swap chain, never both.
The provider interface will expose capability query, real-frame input
submission, one real Present, telemetry, Off/drain, resize, and teardown.
The user-started V5.4 preflight confirmed both the ReShade nested and ENB
outer swap chains support `IDXGISwapChain1/3/4` and return the original D3D11
device identity, while D3D12 `GetDevice` returns `E_NOINTERFACE`. The
game-facing facade must preserve that D3D11 contract, expose D3D11 backbuffer
textures and hide the D3D12 lower owner. See
`docs/re/FG_V54_SWAP_FACADE_LIVE_2026-09-28.md`.
An offline WARP bridge now proves the physical D3D11 colour-to-D3D12 lower
backbuffer transfer and one real Present across two successive buffers. It
has been wrapped by a source-only D3D11-facing SwapChain1/3/4 facade with
explicit-size ResizeBuffers/1 WARP tests. The facade is not installed or
validated under the ENB/ReShade wrapper chain; see
`docs/re/FG_D3D11_PRESENT_BRIDGE_2026-09-28.md` and
`docs/re/FG_D3D11_COM_FACADE_2026-09-28.md`.
The pinned Streamline 2.14.1 FG-Off probe confirms proxy/native identity,
Present, state and resize. Its earlier post-Present FLIP_DISCARD pixel readback
was not a valid colour oracle. A test-only lower observer now reads the buffer
before each Present and verified three real-frame colours across physical
indices 0, 1, 0 through the Streamline proxy; WARP also covers the facade.
The Streamline proxy queue and native lower device have distinct COM
identities, resolved through a verified `slGetNativeInterface` pair; see
`docs/re/FG_STREAMLINE_FACADE_OFF_2026-09-28.md`.
The End-menu FG tab and configured toggle hotkey can request On/Off during a
session. Until a backend connects them to the lower Present, the menu must
report Effective Off. The in-game provider selector writes a restart-pending
choice; it does not pretend to replace a live swap chain.
Those methods operate on a common frame/lease record, with provider-specific
tagging and API calls behind the interface. The current D3D11 observer is not
that owner. An offline WARP lower-chain forwarder now proves real
Present/Present1, COM identity and resize HRESULTs, but it does not expose a
D3D11-facing D3D12 proxy. The live ENB/ReShade wrapper trace is captured;
offline COM/backbuffer/resize tests and game validation remain before any
replacement in the game.

For each real source frame, the coordinator supplies display-sized final
colour, HUD-less colour and native UI colour/alpha, plus correctly sized
motion and depth, camera/jitter/reset data, resource generation and one
Present token. It runs NR and SR once. The selected backend can present one
generated frame without advancing simulation, source ID, jitter or history.
The source-frame ledger rejects a second real Present. The producer, copy,
provider-input, Present and allocator completion fences must all retire
before a slot is reused; no provider may infer completion from the copy fence
alone.

Provider adapters translate the common inputs into their own API contracts:

| Backend | Lower presentation owner | UI input | Pacing and count |
| --- | --- | --- | --- |
| DLSS-G | Streamline D3D12 proxy/upgrade | HUD-less and native UI tags with exact resource states | Streamline status/telemetry and its Present path |
| FSR 3.1 FG | FSR D3D12 frame-interpolation swap chain | UI callback, alpha surface or HUD-less path, selected by actual validation | FSR swap-chain pacing and reported output |

The common layer must not pretend these APIs have identical resource states,
marker requirements, or swap-chain creation. FSR 3.1 is the cross-vendor
candidate documented by [AMD's SDK compatibility page](https://gpuopen.com/amd-fsr-sdk/);
the FSR 4 ML path targets AMD Radeon RX 9000.
See the official [FSR frame-interpolation swap-chain](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/),
[FSR frame interpolation](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation/),
and [DLSS-G programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md).

## Remaining implementation and acceptance

1. Bind the new five-input shared leases to a genuine HUD-less/UI capture and
   actual provider-input/Present/allocator fence signals. Their WARP-tested
   retirement policy must not be fed fabricated completed values.
2. Trace the V5.4 live swap-chain wrapper/creation order and use the WARP-tested
   lower-chain harness to build the D3D11-facing D3D12 owner. The harness
   already proves native lower Present, HRESULT, COM identity, 100 resizes and
   Off/drain; it does not prove the future facade under ENB/ReShade.
3. Use the now hash-pinned official Streamline 2.14.1 headers/runtime as one
   set for the DLSS-G backend, and test tags/resource lifetimes separately.
   The local RTX 4080 SUPER standalone probe reports DLSS-G support and D3D12
   device binding, not generated output. The supplied local Streamline
   v2.13.0-beta10 DLLs must not be paired with 2.14.1 headers. Never infer FG
   support from the NR runtime or GPU name.
4. Use a user-started game session to establish actual generated output,
   native HUD/menu appearance, ENB/ReShade composition, frame cadence and
   stability. Until then both backends are **NOT RUN** in Skyrim.
