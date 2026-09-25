# Open implementation problems

Checkpoint: installed 0.1.79 restored coloured health, stamina and magicka
fills in the actual game and held real DLSS beyond 9,300 submissions with zero
fallbacks. Installed 0.1.80 ran NR in DLSS mode but the user observed heavy
ghosting; NativeAA submitted more than 18,600 DLAA frames without NR because
the preprocessor was attached only to the reduced SR presenter. Source 0.1.81
routes NR through DLSS and DLAA, applies evaluation controls live with a
history reset, and adds the reference D3D11 Signal/Flush ordering. Its Skyrim
runtime is pending.
This file describes remaining work, not completed features.
The detailed run record is in [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
and the render-size evidence is in
[RENDER_SIZE_CONTRACT.md](re/RENDER_SIZE_CONTRACT.md).

| Priority | Problem and measured state | Next proof or implementation step |
|---|---|---|
| P0 | Long-run presentation ownership across ENB, ReShade, SKSE, SSE Display Tweaks, scene transitions and resize remains only partially verified. The current owned route chains the observed owners and preserved ENB appearance in the latest user report. | Exercise save load, interior/exterior transition and resize with the native UI route active; record any route downgrade, Present failure or visual-order change. |
| P1 | NVIDIA NGX accepted cropped R32_FLOAT depth in a synthetic 1280x720-to-2560x1440 30-frame replay, including a post-evaluation injected fallback with clean retirement. Actual Skyrim motion-vector units, jitter, stable reduced source pixels and same-frame game handoff remain unverified. | Inspect the new 0.1.32 source-acceptance and `Reduced DLSS SR displayed` logs, then check visual quality and UI before treating this branch as working in game. |
| P1 | The 0.1.68 user-started test proved the corrected `ControlMap +0x129` byte changes, but camera movement still reached gameplay. Runtime 0.1.69 successfully chained the verified input-dispatch CALL through the existing OpenAnimationReplacer owner. The user exercised the menu and sharpening controls, but camera suppression, focus-loss restoration and downstream-mod behavior were not explicitly reported as passed. | Verify no camera movement while dragging, normal controls after closing, focus-loss restoration and the next-launch quality/model transition. |
| P1 | The first 0.1.69 launch exhausted its 24 depth probes before the save loaded, permanently suppressing native DLAA and post-sharpening. A faster second launch accepted world depth and the user reported working sharpening. Installed 0.1.70 continues native-DLAA probing at a reduced rate after the initial window. Its reduced Quality route is game-tested separately and live sharpening updates reach the presenter. The extended native-DLAA retry has not yet been exercised past its initial 24 attempts. | Verify the extended retry on a deliberately slow NativeAA load only if that mode needs further testing; do not block reduced-SR stabilization on it. |
| P1 | DLSS Frame Generation is not integrated. NVIDIA's current Streamline FG guide is oriented around D3D12/Vulkan; it supplies no drop-in Skyrim D3D11 FG path. The DynamicShaderFrameGen project is a reference, not a RazKolbas runtime dependency. | Finish D3D11-compatible FG capability/presentation research, then implement generated-frame ownership, pacing, HUD/effects placement and teardown only on a validated route. |
| P1 | Fast-FP16 NR produces nontrivial output in the isolated probe and the 30-frame D3D11/D3D12 bridge passes live control changes. The 0.1.80 Skyrim DLSS run appeared active but had heavy ghosting; NativeAA never invoked NR. Source 0.1.81 fixes the missing DLAA stage, resets history after live changes and adds the reference Signal/Flush ordering. Skyrim temporal quality remains unverified, and the bridge still waits synchronously per frame. The modified runtime has retained NVIDIA signer metadata but `HashMismatch`; only exact SHA-256 `91ea...` is accepted. | Install 0.1.81, test DLSS and NativeAA, confirm NR submission logs in both modes, compare motion ghosting, exercise one live Intensity change, and verify ENB/ReShade/native UI. If guide semantics are then correct, replace the synchronous single-frame wait with a fenced context ring before calling NR performance-ready. |
| P2 | FSR and XeSS SR/FG providers, capability selection and full configuration behavior remain incomplete. The current working display path is experimental NVIDIA SDR DLAA. | Continue the plan's provider-specific integration after the shared world/source/presentation contract is proven. |

The owner no longer wants manual DRS console commands. Do not request another
command-driven game run. The next runtime evidence is a normal save load with
installed 0.1.81. Confirm NR submissions in DLSS and NativeAA, compare motion
ghosting after the interop flush, exercise one live evaluation control, then
inspect ENB/ReShade/native UI, frame time and stability. The assistant must
not start Skyrim.
