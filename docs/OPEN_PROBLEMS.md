# Open implementation problems

Checkpoint: RazKolbas 0.1.70 is installed. The last game-tested path is 0.1.69
native SDR DLAA with working post-sharpening; 0.1.70 has not run in Skyrim.
This file describes remaining work, not completed features.
The detailed run record is in [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
and the render-size evidence is in
[RENDER_SIZE_CONTRACT.md](re/RENDER_SIZE_CONTRACT.md).

| Priority | Problem and measured state | Next proof or implementation step |
|---|---|---|
| P0 | A guarded reduced Skyrim DLSS SR branch is implemented in `WorldDrawHook`, but has not run in game. It requires a stable native DRS ratio, a same-frame SDR source whose pixels outside the reduced rectangle are uniform, and world-like reduced depth before it publishes. The 1704x960 DRS viewport inside 2560x1440 allocations has not been shown to satisfy that source contract. It does not turn on SR from the user's Quality setting. | Implement an owned reduced scene and display handoff driven by the configured quality, rather than requiring a console-set DRS ratio or bypassing the source check. Verify UI target/cache ownership before publication. |
| P0 | DLAA remains suspended whenever current or previous DRS ratio is nonnative. The new SR branch can publish a prepared DLSS frame or current-frame spatial fallback after source verification, but its game mode changes, jitter/MV conventions and UI order are only build-tested. | Finish the owned scene route, then verify a real SR submission, same-frame fallback and native recovery in the game without manual DRS commands. |
| P0 | Presentation ownership with ENB, ReShade, SKSE and SSE Display Tweaks is unresolved for reduced SR. The texture-creation entry is detoured by SKSE and the resize trampoline belongs to SSE Display Tweaks. Blindly replacing either would risk breaking the modlist. | Choose and validate one pre-UI SR output path that chains or avoids existing owners and preserves the full-size HUD and ENB/ReShade order. |
| P1 | NVIDIA NGX accepted cropped R32_FLOAT depth in a synthetic 1280x720-to-2560x1440 30-frame replay, including a post-evaluation injected fallback with clean retirement. Actual Skyrim motion-vector units, jitter, stable reduced source pixels and same-frame game handoff remain unverified. | Inspect the new 0.1.32 source-acceptance and `Reduced DLSS SR displayed` logs, then check visual quality and UI before treating this branch as working in game. |
| P1 | The 0.1.68 user-started test proved the corrected `ControlMap +0x129` byte changes, but camera movement still reached gameplay. Runtime 0.1.69 successfully chained the verified input-dispatch CALL through the existing OpenAnimationReplacer owner. The user exercised the menu and sharpening controls, but camera suppression, focus-loss restoration and downstream-mod behavior were not explicitly reported as passed. | Verify no camera movement while dragging, normal controls after closing, focus-loss restoration and the next-launch quality/model transition on 0.1.70. |
| P1 | The first 0.1.69 launch exhausted its 24 depth probes before the save loaded, permanently suppressing DLAA and post-sharpening. A faster second launch accepted world depth and the user reported working sharpening after at least 7,800 continuous DLAA submissions. Installed 0.1.70 continues probing at a reduced rate after the initial window and logs live sharpening updates; this new build is not game-tested. | Start 0.1.70, load a save after an ordinary or deliberately slow main-menu interval, confirm DLAA submissions begin, then change sharpness and verify the new live-update log plus visible response. |
| P1 | DLSS Frame Generation is not integrated. NVIDIA's current Streamline FG guide is oriented around D3D12/Vulkan; it supplies no drop-in Skyrim D3D11 FG path. The DynamicShaderFrameGen project is a reference, not a RazKolbas runtime dependency. | Finish D3D11-compatible FG capability/presentation research, then implement generated-frame ownership, pacing, HUD/effects placement and teardown only on a validated route. |
| P1 | Neural Rendering is not producing frames. The supplied community `nvngx_dlssnr.dll` lacks an NVIDIA signature; the exact-hash experimental compatibility route reached init/create/release/shutdown with balanced callbacks on the RTX 4080 SUPER, but evaluation and output were not verified. | Trace the exact evaluation ABI, run an isolated GPU-output probe and implement a version-gated runtime path with resource retirement. Do not label create success as working NR. |
| P2 | FSR and XeSS SR/FG providers, capability selection and full configuration behavior remain incomplete. The current working display path is experimental NVIDIA SDR DLAA. | Continue the plan's provider-specific integration after the shared world/source/presentation contract is proven. |

The owner no longer wants manual DRS console commands. Do not request another
command-driven game run. The next implementation step is to make the selected
SR quality own its reduced scene/display transition, including a verified
native-resolution UI handoff and fallback. Existing ratio diagnostics remain
useful if DRS changes independently, but they do not establish a product path.
The assistant must not start or close Skyrim.
