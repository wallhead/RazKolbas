# D3D11 facade plus DLSS-G standalone probe, 2026-09-29

The new `--facade-on` path creates a D3D11 device on the same RTX 4080
SUPER adapter as the Streamline-managed D3D12 lower swap. It obtains the
lower native device through `slGetNativeInterface`, wraps the lower swap in
`FgD3D11SwapFacade`, clears its game-facing D3D11 render buffer, and calls
the facade's Present. The facade's existing bridge copies that real colour
through a shared D3D11/D3D12 surface into the current physical lower
backbuffer before forwarding exactly one Present. The probe submits the
existing source-stamped Streamline camera/options/tag packet for each frame.
Its depth, motion, HUD-less colour and UI tag textures are synthetic D3D12
surfaces, not captured Skyrim resources.

The test command was first run against the old probe and failed because
`--facade-on` was not accepted. After implementation, the release build and
all **60/60** CTest groups passed. The command
`pwsh -NoProfile -File tools/Test-FgD3D11On.ps1 -SdkBin
artifacts/local/vendor/streamline-2.14.1/sdk/bin/x64` exited 0 on the
local NVIDIA adapter. It observed successful SL initialization, DLSS-G
support, D3D11 facade creation, eight `Present` calls returning `S_OK`,
`slDLSSGGetState` status 0 with `numFramesActuallyPresented=2` and
`numFramesToGenerateMax=1` after each, a successful Off/drain Present,
and successful SL shutdown. The prior `--on` D3D12-only and `--facade`
FG-Off paths both independently exited 0 afterward. The FG-Off facade path
read back red, green and blue D3D11-cleared pixels from rotating lower
backbuffers before Present, then exercised explicit and zero-size resize.

This establishes that this adapter and pinned Streamline SDK can generate
an extra frame when the ordinary real colour arrives through the D3D11
facade. It does **not** establish moving-object guide quality, continuous
Skyrim UI/camera freshness, ENB/ReShade wrapper compatibility, game-side
Streamline initialization, or provider resource retirement in Skyrim. The
standalone bridge waits on the CPU for the copy before Present; that is a
correctness probe, not an acceptable final pacing design. The installed
MO2 mod remains 0.1.125 with FG Off. Skyrim FG-On is **NOT RUN**.

The next integration dependency is a verified startup-selected lower swap
owner inside the observed ReShade/ENB factory chain. It must preserve the
D3D11-facing COM identity and existing early owned-scene publication,
publish continuously fresh UI and guides, and retain each slot through
Streamline's reported input-processing completion fence. A plain replacement
of the nested or outer swap pointer is not sufficient.
