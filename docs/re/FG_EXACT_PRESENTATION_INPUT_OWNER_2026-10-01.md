# FG game inputs on the presentation owner, 2026-10-01

## Change and ownership

The user confirmed normal image/UI/steady FPS in 0.1.155 and closed Skyrim.
Its eight valid copied packets used a companion D3D12 direct queue. That
did not prove their compatibility with the startup-selected Streamline
presentation owner.

Source 0.1.156 exposes a retained `FgPresentationInputOwner` only after the
private facade has been issued. The pinned Streamline native-interface API
provides the native lower swap and direct queue. Acquisition checks the
queue, native swap and proxy swap's device COM identities against the
verified native D3D12 device; adapter equality alone is insufficient.
Both swap descriptions must match in window, size, format and swap effect.
Their buffer counts are recorded separately and independently validated.

The serialized game pre-Present callback acquires this endpoint only for
the attached active outer swap, with no deferred resize cleanup and matching
display extent. It calls `FgGameInputProbe::beginOnOwner`, which creates no
replacement D3D12 device/queue and checks native queue/device identity.
The route proves this particular queue created the lower swap; the generic
copy API alone cannot establish that association. An absent private route
can use the explicitly labelled source-only companion diagnostic. An
attached but invalid owner rejects the copy instead of hiding the failure
behind that companion path.

The provider runtime is retained by the probe and interop lifetime. A
pending copy abandoned during resize/destruction transfers that pin with
its surfaces, devices, queue and fences into process-lifetime quarantine.
Normal close verifies copy retirement, then releases GPU interfaces before
the runtime pin. This remains a bounded eight-frame FG-Off copy test; it
does not tag these game inputs to a provider or generate a Skyrim frame.

## Tests and observed failures

The test-first API and endpoint assertions failed to compile against the
previous implementation because `beginOnOwner`/`acquireInputOwner` did not
exist. The WARP tests now pass valid exact-device/direct-queue copies,
reject missing device and COPY queues, retain the runtime until normal
close/destruction, and retain it after an intentionally blocked queue's
pending copy is quarantined. Existing five-format/pixel-copy tests remain
in the regression suite.

The first hardware endpoint run passed its identity and presentation
checks but crashed on teardown: the newly retained endpoint and two
`GetDevice` COM references outlived ReShade's explicit module unload in
the harness. The final harness releases all three before route shutdown
and module unload; complete runs now exit 0.

A proposed equal-buffer-count check correctly rejected another concrete
observation: pinned Streamline 2.14.1 reports **three proxy buffers over two
native buffers**, with matching window/160x96 size/RGBA8 format/flip-discard
effect. Both counts remain 3/2 after the 192x108 resize. The endpoint keeps
both descriptions; a proxy index must never be used as a native index or
validated against the native buffer count. The diagnostic recorded this
before removing the unjustified equality assumption. The local TRP
`SourceDLSSGSwapChain.cpp` also explicitly requests two native buffers;
it is a reference, not a transplanted ABI.

Release build/CTest passed **70/70**. Debug build/CTest passed **65/65**.
The standalone D3D11-facade FG-On regression still passed eight SDK reports
of two presented frames, eight copied-input/provider retirements, Off/drain
and shutdown. Its ignored log is
`artifacts/local/fg-owner-0156-synthetic-on-regression.txt`, SHA-256
`27dfb463f732fc0b8033c4432fac88388e57bd93375fde9f88c95cb47ca97e16`.
An independent subagent reviewed the final ownership, rejection, resize
and quarantine paths and found no blocker for this bounded FG-Off trial.
It noted that the eight copies can briefly stall the presentation queue,
so steady FPS and the explicit owner log must be checked separately.

## Standalone hardware results

On the RTX 4080 SUPER with the exact installed ENB 0.505 and ReShade 6.8,
both the no-Steam and exact Steam-overlay-preload reproductions passed:

- Pre-issue endpoint acquisition rejected.
- Exact native device/direct queue identities verified, runtime retained.
- Eight five-input copies queued/completed on that presentation queue;
  producer/copy fences 1 through 8, clean close.
- 240 FG-Off Presents and a real resize to 192x108 completed.
- Endpoint reacquired after resize with proxy/native buffer counts 3/2.
- Full D3D11/D3D12 presentation colour buffers matched at both sampled sizes.
- No frame executed ReShade techniques in two runtimes; clean process exit 0.

Ignored final logs:

- `artifacts/local/fg-exact-owner-0156-no-steam-final.txt`, SHA-256
  `4e1a70648fdc6a7061d707e950dd362a49689f3f8aac4e836e4c0ca52c026d52`.
- `artifacts/local/fg-exact-owner-0156-steam-final.txt`, SHA-256
  `f5d39c70f4b99cada0cc0039f95360df632d1364fa64c49c26866346c52fcce8`.

These harness input textures are synthetic. The eight copies establish
owner/synchronization compatibility, not correct Skyrim guide content or
provider consumption. The colour comparisons exercise the separate
presentation path. Skyrim FG-On/generated frames remain **NOT RUN**.

## Game checkpoint and next action

The stage is `artifacts/local/stage-v54-fg-exact-owner-0156`, verified
**14/14**. The trial INI changes only `ProbeFgPrivateSwapOff=false` to
`true`; requested FG remains Off, and SR/NR/loading/user settings are
unchanged. Skyrim was verified closed immediately before replacement, and
the previous installed package reverified **14/14**. Its INI/DLL/manifest
were backed up at ignored `artifacts/local/backup-v54-before-0156`. Only
those three files were replaced. Installed 0.1.156 again verified **14/14**.

- DLL SHA-256: `0653274f9bd5b192c56cc411ce06dad81723f944e84ca97892cc516522dea354`.
- Trial INI SHA-256: `40c17f08b63c764fb5eb457085843be52a2201f9755713e01f7d8c41c29bedbc`.
- Installed/stage manifest SHA-256: `ada892fe46b5b3b941a123f738f5cbf566f513d044efe155e1a2b1a2e74152d4`.

**0.1.156 Skyrim exact-owner trial: NOT RUN.** The next user-started MO2
save load must report normal image/UI/FPS and capture the exact-private-owner
log plus eight metadata/copy completions. Source-only companion fallback
does not count as an exact-owner pass. Then attach the existing real-frame
Streamline session at the verified game phases, make eligible UI/guide
capture continuous, and join copied read states and actual
provider/Present/allocator retirement on this owner before an FG-On test.
