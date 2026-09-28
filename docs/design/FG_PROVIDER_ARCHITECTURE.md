# Swappable frame-generation providers

## Implemented boundary

`FgSourceFrame`, `FgCapability`, `FgDecision`, `FgProviderSession`,
`FgPresentLedger`, and `FgRetirementSet` form a vendor-neutral policy layer.
They do not yet create generated frames. A session selects at most one provider
when its presentation owner is created. Auto prefers a validated DLSS-G
capability, then a validated FSR 3.1 FG capability. A runtime failure turns
that provider Off; it never hot-swaps to another live swap chain. Requested
provider, effective provider, and the Off reason remain distinct. Enabling
FG or changing its provider is restart-scoped in `SettingsTransaction`.

The render adapter is still required to be NVIDIA, as requested for this
product. FSR 3.1 FG is a *candidate backend on an NVIDIA adapter*; it is not
an AMD-render-adapter support path. The pairing with the chosen SR provider
must be validated before it is enabled. The existing XeSS FG schema choice
remains reserved and has no bound backend. FSR 4 ML FG is outside this scope.

## One owner, interchangeable lower backend

Skyrim, ENB, ReShade and Display Tweaks must see one coherent D3D11-facing
swap chain. A startup-selected owner controls exactly one lower D3D12
presentation path. Its provider implementation is either a Streamline-managed
DLSS-G lower swap or an FSR 3.1 frame-interpolation swap chain, never both.
The provider interface will expose capability query, real-frame input
submission, one real Present, telemetry, Off/drain, resize, and teardown.
Those methods operate on a common frame/lease record, with provider-specific
tagging and API calls behind the interface. The current D3D11 observer is not
that owner; a matching COM/ENB/ReShade wrapper trace and harness must precede
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
candidate documented by AMD; the FSR 4 ML path targets AMD Radeon RX 9000.
See the official [FSR frame-interpolation swap-chain](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/),
[FSR frame interpolation](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation/),
and [DLSS-G programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md).

## Remaining implementation and acceptance

1. Build same-adapter D3D11-to-D3D12 source/guide leases with independently
   measured consumer fences and generation-safe resize. Reuse the verified
   NR interop techniques only where the lifetimes actually match.
2. Trace the V5.4 swap-chain wrapper chain and build an offline COM/presentation
   harness. Prove one real Present, HRESULT propagation, reference lifetime,
   resize and Off/drain before changing Skyrim's owner.
3. Pin matching vendor SDK/runtime versions, implement each backend behind
   the owner, and test resource/tag contracts separately. Never infer FG
   support from the NR runtime or GPU name.
4. Use a user-started game session to establish actual generated output,
   native HUD/menu appearance, ENB/ReShade composition, frame cadence and
   stability. Until then both backends are **NOT RUN** in Skyrim.
