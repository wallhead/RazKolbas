# Open implementation problems

Checkpoint: RazKolbas 0.1.30, source commit `9045933`, installed in the user's
MO2 `RazKolbas` mod. This file describes remaining work, not completed features.
The detailed run record is in [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
and the render-size evidence is in
[RENDER_SIZE_CONTRACT.md](re/RENDER_SIZE_CONTRACT.md).

| Priority | Problem and measured state | Next proof or implementation step |
|---|---|---|
| P0 | Genuine reduced DLSS Super Resolution is not submitted. Skyrim's native console commands produced a 1704x960 world viewport at 2560x1440 display, but the observed colour, motion and depth **allocations** remained 2560x1440. The one-time post-world descriptor log ran after only the width command, so it did not capture stable reduced guide pixels. | Capture colour, motion and depth together after both ratios settle, identify the viewport-owning pre-upscale render phase, and verify the valid pixels/units/extent before NGX submission. A prepared top-left crop and R24-to-R32 depth conversion pass WARP tests but are not connected to the game. |
| P0 | DLAA is deliberately suspended whenever the engine's current or previous DRS ratio is nonnative. The 0.1.29 user run confirmed this guard; reduced frames display Skyrim's native fallback, not DLSS. | After a verified reduced source exists, add an owned SR submission and a complete native fallback/temporal-reset transaction. Keep the guard until that path is validated. |
| P0 | Presentation ownership with ENB, ReShade, SKSE and SSE Display Tweaks is unresolved for reduced SR. The texture-creation entry is detoured by SKSE and the resize trampoline belongs to SSE Display Tweaks. Blindly replacing either would risk breaking the modlist. | Choose and validate one pre-UI SR output path that chains or avoids existing owners and preserves the full-size HUD and ENB/ReShade order. |
| P1 | NVIDIA NGX acceptance of the cropped R32_FLOAT depth input, reduced motion-vector semantics, jitter scaling and same-frame resource lifetime has not been tested in Skyrim. The 0.1.28 game run proved full-resolution SDR DLAA submission only. | Run one bounded offscreen reduced SR evaluation using owned copies and GPU retirement, then permit display only after output and fallback checks pass. |
| P1 | The End menu's new ratio-derived Skyrim DRS target and suspension text passed build/tests but version 0.1.30 has not run in game. A prior menu run reported the cursor visible underneath the menu. | Verify the readout and cursor behavior on the next necessary game run; fix menu interaction separately from SR correctness. |
| P1 | DLSS Frame Generation is not integrated. NVIDIA's current Streamline FG guide is oriented around D3D12/Vulkan; it supplies no drop-in Skyrim D3D11 FG path. The DynamicShaderFrameGen project is a reference, not a RazKolbas runtime dependency. | Finish D3D11-compatible FG capability/presentation research, then implement generated-frame ownership, pacing, HUD/effects placement and teardown only on a validated route. |
| P1 | Neural Rendering is not producing frames. The supplied community `nvngx_dlssnr.dll` lacks an NVIDIA signature; the exact-hash experimental compatibility route reached init/create/release/shutdown with balanced callbacks on the RTX 4080 SUPER, but evaluation and output were not verified. | Trace the exact evaluation ABI, run an isolated GPU-output probe and implement a version-gated runtime path with resource retirement. Do not label create success as working NR. |
| P2 | FSR and XeSS SR/FG providers, capability selection and full configuration behavior remain incomplete. The current working display path is experimental NVIDIA SDR DLAA. | Continue the plan's provider-specific integration after the shared world/source/presentation contract is proven. |

The immediate test dependency is a user-started Skyrim run with both native DRS
commands and one `Ctrl+Shift+F10` candidate capture while a world scene is
visible. The assistant must not start or close Skyrim. The 0.1.30 MO2 package
is installed, and `Diagnostics.CaptureHotkey=CtrlShiftF10` remains enabled.
