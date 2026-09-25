# Open implementation problems

Checkpoint: installed 0.1.78 keeps real DLSS and menu publication continuous,
turning the prior flicker into stable black resource fills. Full-frame captures
place the failure in late native UI and capture history pins its first
appearance to 0.1.75, when the display-sized DSV was first bound. Source 0.1.79
stops the owned writable and sampled-clear DSVs from inheriting source
read-only flags and logs the live descriptor contract.
This file describes remaining work, not completed features.
The detailed run record is in [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
and the render-size evidence is in
[RENDER_SIZE_CONTRACT.md](re/RENDER_SIZE_CONTRACT.md).

| Priority | Problem and measured state | Next proof or implementation step |
|---|---|---|
| P0 | Installed 0.1.78 keeps `mode=2` and real provider submissions continuous with zero fallbacks, but health, stamina and magicka fills remain black. Prepared/raw DLSS images contain no UI, the final image adds frames/text without fills, and alpha is 255 throughout. Capture history shows coloured fills through 0.1.74 and black fills from the 0.1.75 DSV-binding change onward. | Test 0.1.79 in the same scene. Its log must report the actual source DSV flags and owned writable/clear flags zero. If fills remain black even with writable views, trace DrawIndexed and Draw state only inside one deferred Scaleform flush. |
| P0 | Long-run presentation ownership across ENB, ReShade, SKSE, SSE Display Tweaks, scene transitions and resize remains only partially verified. The current owned route chains the observed owners and preserved ENB appearance in the latest user report. | Exercise save load, interior/exterior transition and resize with the native UI route active; record any route downgrade, Present failure or visual-order change. |
| P1 | NVIDIA NGX accepted cropped R32_FLOAT depth in a synthetic 1280x720-to-2560x1440 30-frame replay, including a post-evaluation injected fallback with clean retirement. Actual Skyrim motion-vector units, jitter, stable reduced source pixels and same-frame game handoff remain unverified. | Inspect the new 0.1.32 source-acceptance and `Reduced DLSS SR displayed` logs, then check visual quality and UI before treating this branch as working in game. |
| P1 | The 0.1.68 user-started test proved the corrected `ControlMap +0x129` byte changes, but camera movement still reached gameplay. Runtime 0.1.69 successfully chained the verified input-dispatch CALL through the existing OpenAnimationReplacer owner. The user exercised the menu and sharpening controls, but camera suppression, focus-loss restoration and downstream-mod behavior were not explicitly reported as passed. | Verify no camera movement while dragging, normal controls after closing, focus-loss restoration and the next-launch quality/model transition. |
| P1 | The first 0.1.69 launch exhausted its 24 depth probes before the save loaded, permanently suppressing native DLAA and post-sharpening. A faster second launch accepted world depth and the user reported working sharpening. Installed 0.1.70 continues native-DLAA probing at a reduced rate after the initial window. Its reduced Quality route is game-tested separately and live sharpening updates reach the presenter. The extended native-DLAA retry has not yet been exercised past its initial 24 attempts. | Verify the extended retry on a deliberately slow NativeAA load only if that mode needs further testing; do not block reduced-SR stabilization on it. |
| P1 | DLSS Frame Generation is not integrated. NVIDIA's current Streamline FG guide is oriented around D3D12/Vulkan; it supplies no drop-in Skyrim D3D11 FG path. The DynamicShaderFrameGen project is a reference, not a RazKolbas runtime dependency. | Finish D3D11-compatible FG capability/presentation research, then implement generated-frame ownership, pacing, HUD/effects placement and teardown only on a validated route. |
| P1 | Neural Rendering is not producing frames. The supplied community `nvngx_dlssnr.dll` lacks an NVIDIA signature; the exact-hash experimental compatibility route reached init/create/release/shutdown with balanced callbacks on the RTX 4080 SUPER, but evaluation and output were not verified. | Trace the exact evaluation ABI, run an isolated GPU-output probe and implement a version-gated runtime path with resource retirement. Do not label create success as working NR. |
| P2 | FSR and XeSS SR/FG providers, capability selection and full configuration behavior remain incomplete. The current working display path is experimental NVIDIA SDR DLAA. | Continue the plan's provider-specific integration after the shared world/source/presentation contract is proven. |

The owner no longer wants manual DRS console commands. Do not request another
command-driven game run. The next evidence is one normal user-started 0.1.79
save-load run at the same scene; DSV descriptor logging is automatic. The
assistant must not start Skyrim.
