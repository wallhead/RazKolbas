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

## User-started 0.1.144 game trial and rollback

The user started Skyrim through MO2 at 22:25:42 with FG Off and the private
route On. The live log identified 0.1.144, accepted the private Streamline
lower and returned the D3D11 facade to ReShade. Outer Presents remained
`S_OK` with zero recorded failures through checkpoint 21,600; DLSS world
submissions resumed after loading. The closed-session log snapshot has
23,303,912 bytes and SHA-256
`f19738dd63ac6382a1eb2b72192f2220d12faba313285d459c164b29fa9ee090`
at ignored `artifacts/local/fg-phase-2026-09-30/runtime-0144-private-on/RazKolbas.log`.

At the requested same view, the user reported the world colours **yellow
again** and steady FPS **50 versus 60**. The user did not report a fast-travel
artwork result; no loading-artwork claim is made for this trial. The coarse
600-frame log checkpoints after world loading are consistent with roughly
50 submitted real frames per second, but do not measure scanout or isolate a
GPU/CPU cost. Successful Presents and the offline pixel tests therefore do
not establish visual equivalence. The async copy change did **not** fix the
private-route colour or FPS regressions. Actual game FG-On was not attempted.

After the user closed Skyrim and the process was absent, the verified
private-route Off 0.1.144 package was restored by replacing only the trial
INI and manifest. The failed trial INI/manifest remain in ignored
`artifacts/local/fg-phase-2026-09-30/backup-failed-0144-private-on`. The
Release DLL remains 0.1.144, SHA-256
`79b3d03236fc427857d91a40713f37555c04b1f794ba2c7befa53d235dec9427`.
The restored INI SHA-256 is
`9246b486cc214a0fc1c288e64c17835ee4d499609b8dbf71b4006a9b41914869`;
manifest SHA-256 is
`d83332a3a8f6b8d052854af8b99041e6e60ebffded708d2bc7b73675ff5dee78`.
All **14/14** installed payload hashes verify; FG and the private route are
Off, the user's other settings and the brighter reduced-loading route remain.

The next colour investigation should identify the final game-facing colour
resource and effect boundary in the exact ENB/ReShade chain before changing
gamma, blend or colour-space behavior. TRP's retained presentation buffers
are useful architecture evidence, but its separate game-facing texture and
effect handoff cannot be transplanted without proving ReShade's sRGB RTV
contract and the Skyrim wrapper order. Prior ENB Present disassembly shows
real-frame processing before forwarding and is a starting point, not a proof
of this run's pixel ownership. No speculative tint correction was applied.
