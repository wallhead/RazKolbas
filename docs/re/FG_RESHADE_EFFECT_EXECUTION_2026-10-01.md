# Exact ReShade effect execution on the private presentation route

## Scope and method

The 0.1.144 FG-Off private-route Skyrim trial produced a yellow colour shift
and about 50 FPS where the user previously saw 60. The prior exact-wrapper
offline trace showed two ReShade runtimes and two preset loads, but could not
tell whether either runtime actually rendered a technique. This experiment
adds an optional, read-only `effect-trace` mode to the existing
`FgPrivateGameRouteProbe`. It subscribes to ReShade 6.8's public add-on
`reshade_present`, `reshade_begin_effects`, `reshade_render_technique`, and
`reshade_finish_effects` events, associates callbacks with the synthetic
frame number and resize epoch, and reports the number of frames where at least
two runtime identities render techniques. The trace mode pauses 30 ms after
each Present to allow shader compilation and preset activation. It is not a
performance benchmark. The normal probe mode has no added pause or event
registration.

The ReShade DLL, `ReShade.ini`, `ReShade2.ini`, three-technique
`True AE V5.ini` preset, and shader tree were copied to ignored
`artifacts/local/fg-effects-2026-10-01/reshade-root`. ENB 0.505 and the
optional Steam overlay were loaded from their verified existing installations.
The probe hash-gates the exact ReShade and Steam DLLs. All four copied
ReShade root files matched the source SHA-256 again after the trials; the
user's installed mod, preset and ReShade configurations were not changed.
Raw outputs and the restored positive run's ReShade log remain under the
ignored `artifacts/local/fg-effects-2026-10-01` directory.

## Offline results

| Isolated preset configuration | Technique executions before/after resize | Frames with both runtimes executing before/after resize |
| --- | --- | --- |
| Both runtimes active, no Steam preload | D3D12 336/288; D3D11 333/288 | 111/96 |
| Both runtimes active, exact Steam preload | D3D12 312/297; D3D11 309/297 | 103/99 |
| Both runtimes' `Techniques` empty | 0/0 on both | 0/0 |
| D3D11 preset active, D3D12 preset empty | D3D11 303/291; D3D12 0/0 | 0/0 |
| D3D12 preset active, D3D11 preset empty | D3D12 336/297; D3D11 0/0 | 0/0 |

All runs returned 0, completed 240 real Presents and a 192x108 resize, and
reported zero trace-record overflow. In the restored positive run, ReShade's
own log maps runtime `000001CA24A25FD0` to `ReShade.ini` and runtime
`000001CA256680B0` to `ReShade2.ini`; these respectively belong to the lower
D3D12 and upper D3D11 wrapper stages in this reproduction. Each active
runtime renders all three preset techniques per observed effect pass. The
negative and single-runtime controls establish that the event counts reflect
active technique execution rather than runtime creation, shader compilation,
or a bare Present. The script `tools/Test-FgReShadeEffects.ps1` asserts two
executing runtimes and overlapping frames in both resize epochs for the
normal copied preset.
The counts vary slightly with shader-load timing: after the final source
build, repeat positive runs observed 112/99 overlapping frames without Steam
and 112/102 with Steam. The test script refuses a ReShade DLL path outside
ignored `artifacts/local` so it cannot probe the live installed preset by
accident.
The guarded positive run passed, and its installed-path rejection control
passed. Full Release/Debug builds and CTest passed **65/65** and **60/60**.

This is direct evidence of duplicate effect *execution* on the same synthetic
frame number in the exact-wrapper route. It is not yet evidence that the
final Skyrim pixels were processed twice: the trace does not capture
post-D3D11 and post-D3D12 images, GPU cost, or a live game run. The saturated
red first-pixel sample cannot establish midtone colour behaviour. Next, use
a patterned midtone source and read back the post-D3D11, post-copy, and
post-D3D12/native-present stages in queue order under the four isolated
effect configurations. Only then choose which stage should own effects in
the game route. The installed private route and FG remain Off; game FG-On is
**NOT RUN**.

The event contracts and Present placement are in the official
[ReShade 6.8 add-on header](https://github.com/crosire/reshade/blob/v6.8.0/include/reshade_events.hpp)
and [DXGI swap implementation](https://github.com/crosire/reshade/blob/v6.8.0/source/dxgi/dxgi_swapchain.cpp).
