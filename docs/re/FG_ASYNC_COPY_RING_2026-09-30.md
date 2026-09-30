# Queue-owned FG copy ring, 0.1.144

The private FG presentation path previously created a D3D12 allocator,
command list and fence for every real frame and waited on the CPU for the
D3D11 shared copy and the D3D12 backbuffer copy. This was a plausible cost in
the earlier FG-Off private-route FPS regression, but the cause of the user's
current roughly six-FPS gap has **not** been established. This change removes
those two per-frame CPU waits only when the private route created the lower
swap on the exact queue used by the bridge. All other bridge callers retain
the synchronous copy.

`FgPrivateSwapRoute::prepare` creates the lower swap with `queue_`, and its
facade passes that same `queue_` into `FgD3D11PresentBridge`. The explicit
queue-owned option is set only there and in controlled WARP tests.
`FgSharedInputs::copy` queues `Wait(producerGate)` after the D3D11 copy and
signal. The retained D3D12 commands, their completion signal and lower Present
then follow on that queue. One allocator/list is retained per physical lower
buffer after its first use; its fence must complete before reset. Resize
drains copy slots and releases lower backbuffer references before
`ResizeBuffers`. Uncertain retirement retains the provider and GPU resources
in quarantine. No synthetic success is returned for failed resize/copy.

The blocked-queue WARP test was written before the new option and failed to
compile without it. The implementation then passed rotating 2/3-buffer exact
pixel tests, facade colour inspection at the lower Present boundary, blocked
queue admission, resize and uncertain-retirement tests. Final
`tools/Build.ps1 -Preset win-release` passed **65/65** CTest groups;
`tools/Build.ps1 -Preset win-dev` passed **60/60**. Build logs are ignored
under `artifacts/local/fg-phase-2026-09-30/copy-ring-build-*.txt`.

The pinned exact ENB 0.505/ReShade 6.8 standalone game-route reproduction
completed **240 FG-Off Presents and resize to 192×108** both with and without
the Steam overlay preload. Both exited 0, reported `copy-prepared-queued`,
`first-failure=none/0x0`, substitution 1 and first real Present `0x0`.
Ignored outputs and SHA-256:

| Reproduction | Output SHA-256 |
|---|---|
| Without Steam | `0b6e38afbc151f3a5cfc7fb33c9253e4a1f625bc334c6b304debba7978ed4895` |
| With Steam | `e451106a6f1fc20df3fad730d03b9cab764d40e8c0bdddc5c4ee5e177947fa61` |

The separate synthetic facade-on FG probe passed with
`generatedObserved=1`, eight paired real/generated outputs, successful input
retirement and shutdown (`slShutdown=0`). Its ignored output SHA-256 is
`86fb03989bbe772d80ac1009675cbac6ae93004cc965191ff7f7a900abb8ce5f`.
That probe uses the default synchronous facade; the queue-owned facade is
covered by WARP and the private-route reproduction. These tests do **not**
prove Skyrim visual fidelity, loading artwork, game FPS or game FG-On.

With Skyrim closed, an unchanged-settings 0.1.144 package was staged first
with private route Off and all **14/14** payload hashes valid. Its INI SHA-256
matches the prior installed INI:
`9246b486cc214a0fc1c288e64c17835ee4d499609b8dbf71b4006a9b41914869`.
For the necessary game trial, only `ProbeFgPrivateSwapOff = false` was changed
to `true` in a byte-reversible copy of that INI. The Release DLL and existing
runtime payloads were packaged again, all **14/14** hashes verified, and only
DLL, INI and manifest were installed after another closed-game check. The
previous three files are backed up in ignored
`artifacts/local/fg-phase-2026-09-30/backup-before-0144-private-on`; the
private-route Off 0.1.144 package is in ignored `stage-0144` for rollback.

Installed trial SHA-256:

| File | SHA-256 |
|---|---|
| `RazKolbas.dll` | `79b3d03236fc427857d91a40713f37555c04b1f794ba2c7befa53d235dec9427` |
| `RazKolbas.ini` | `738c85e78197d28d43f066e269f140241f7ba12704ef9345868b4ecf20d9f04d` |
| `install-manifest.json` | `22bb69a42b05fc7893ef562bb71f5cd4eab8bda5537ed43bfaae3af5995b1354` |

FG remains **Off**, provider selection and all other user settings remain as
before, phase trace remains Off and the brighter reduced-loading route remains
On. The private-route On flag is a temporary restart-scoped test. **0.1.144
Skyrim image, UI, loading, FPS and actual FG-On: NOT RUN.** The earlier warm
colour shift, loading picture regression and FPS gap remain open until a
user-started run supplies evidence. The next action is one MO2 launch, save
load and same-view FPS/colour inspection with FG Off; restore the Off package
if presentation regresses, then diagnose the resulting live log and colour
boundary offline.
