# Open implementation problems

Checkpoint: RazKolbas 0.1.31 source candidate. The last game-tested path is
native SDR DLAA; 0.1.31 has not run in Skyrim. This file describes remaining
work, not completed features.
The detailed run record is in [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
and the render-size evidence is in
[RENDER_SIZE_CONTRACT.md](re/RENDER_SIZE_CONTRACT.md).

| Priority | Problem and measured state | Next proof or implementation step |
|---|---|---|
| P0 | Genuine reduced Skyrim DLSS Super Resolution is not submitted. The prepared R32 path now passes WARP and a 30-frame NVIDIA replay, but `WorldDrawHook` still calls native DLAA only. The 1704x960 DRS viewport inside 2560x1440 allocations has not been shown to contain matching reduced colour, motion and depth pixels at the producer phase. | Use the new one-shot stable-ratio, same-frame guide diagnostic in a user-started world run. If it does not prove a coherent reduced source, implement the selected owned reduced-scene/display-domain route before enabling game SR. |
| P0 | DLAA is deliberately suspended whenever the engine's current or previous DRS ratio is nonnative. The 0.1.29 user run confirmed this guard; reduced frames display Skyrim's native fallback, not DLSS. | After a verified reduced source exists, add an owned SR submission and a complete native fallback/temporal-reset transaction. Keep the guard until that path is validated. |
| P0 | Presentation ownership with ENB, ReShade, SKSE and SSE Display Tweaks is unresolved for reduced SR. The texture-creation entry is detoured by SKSE and the resize trampoline belongs to SSE Display Tweaks. Blindly replacing either would risk breaking the modlist. | Choose and validate one pre-UI SR output path that chains or avoids existing owners and preserves the full-size HUD and ENB/ReShade order. |
| P1 | NVIDIA NGX accepted cropped R32_FLOAT depth in a synthetic 1280x720-to-2560x1440 30-frame replay, including a post-evaluation injected fallback with clean retirement. Actual Skyrim motion-vector units, jitter, stable reduced source pixels and same-frame game handoff remain unverified. | Validate the real source/guides at the pre-UI boundary and only then connect prepared evaluation and fallback to the game coordinator. |
| P1 | The End menu's new ratio-derived Skyrim DRS target and suspension text passed build/tests but version 0.1.30 has not run in game. A prior menu run reported the cursor visible underneath the menu. | Verify the readout and cursor behavior on the next necessary game run; fix menu interaction separately from SR correctness. |
| P1 | DLSS Frame Generation is not integrated. NVIDIA's current Streamline FG guide is oriented around D3D12/Vulkan; it supplies no drop-in Skyrim D3D11 FG path. The DynamicShaderFrameGen project is a reference, not a RazKolbas runtime dependency. | Finish D3D11-compatible FG capability/presentation research, then implement generated-frame ownership, pacing, HUD/effects placement and teardown only on a validated route. |
| P1 | Neural Rendering is not producing frames. The supplied community `nvngx_dlssnr.dll` lacks an NVIDIA signature; the exact-hash experimental compatibility route reached init/create/release/shutdown with balanced callbacks on the RTX 4080 SUPER, but evaluation and output were not verified. | Trace the exact evaluation ABI, run an isolated GPU-output probe and implement a version-gated runtime path with resource retirement. Do not label create success as working NR. |
| P2 | FSR and XeSS SR/FG providers, capability selection and full configuration behavior remain incomplete. The current working display path is experimental NVIDIA SDR DLAA. | Continue the plan's provider-specific integration after the shared world/source/presentation contract is proven. |

The next game evidence needs a user-started visible-world run with both native
DRS commands. The stable four-ratio diagnostic captures same-frame post-world
guide hashes, depth statistics, descriptors and viewport once per generation;
this phase still may be after enlargement. The assistant must not start or
close Skyrim. The hotkey capture remains available if a producer-phase trace
is needed afterward.
