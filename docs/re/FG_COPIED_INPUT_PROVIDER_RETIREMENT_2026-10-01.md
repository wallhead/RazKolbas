# Copied D3D11 inputs through standalone DLSS-G retirement, 2026-10-01

This is an offline native RTX 4080 SUPER test of the pinned Streamline 2.14.1
runtime, not a Skyrim FG-On result. The installed MO2 plugin remains 0.1.154
with FG Off and the user's INI unchanged.

The `--facade-on` synthetic probe previously supplied its depth, motion,
HUD-less and UI inputs from D3D12-only textures and assigned a single-queue
synthetic copy ticket. It now creates all five inputs on D3D11, clears them on
the same immediate context, and sends them through the same
`FgSharedInputs`/`FgInputLeaseRing` batch used by the 0.1.154 game probe. The
five shared resources are transitioned from COMMON to D3D12 shader-read
state after the actual shared-copy fence. `prepareFgSubmission` validates the
source, camera and separate UI against that lease before Streamline tags are
submitted. The direct `--on` D3D12 control remains a separate route.

After each lower Present, the probe obtains Streamline's actual
`inputsProcessingCompletionFence` and value. It rejects a changed fence
identity. The D3D12 queue signals its own post-Present fence, waits on the
provider fence, transitions the five resources back to COMMON, and signals
the allocator fence. The input ring receives all five retirement values and
reuses a slot only after each real fence passes. This serial CPU-waiting probe
establishes correctness of the boundary, not final game pacing.

The updated `tools/Test-FgD3D11On.ps1` failed against the pre-change binary
because it lacked the shared-copy and retained-retirement records. After
implementation, the local test exited 0: eight source frames copied with
producer/copy values 1–8, slot 0 was reused only after the prior frame's
provider/Present/allocator retirement, eight Present calls returned `S_OK`,
and Streamline reported `numFramesActuallyPresented=2` each time. Provider
input values were 1–8; post-Present values 3,7,...,31; allocator values
4,8,...,32. The ring drained; DLSS-G Off/drain Present and `slShutdown`
succeeded. The D3D12-only `--on` control also exited 0 and reported eight
two-presentation frames.

The exact V5.4 ReShade 6.8 `dxgi.dll` (SHA-256
`b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`)
was used in `--reshade-facade-on`. It exited 0 with the same eight copy,
provider-retirement and two-presentation results. Its ignored raw output is
`artifacts/local/fg-d11-copy-reshade-0155-offline.txt`. Release build/CTest
passed **70/70**; Debug build/CTest passed **65/65**. The raw ReShade output
SHA-256 is
`063093deafc7368047c94e9b6881885c3a05d57a5acce01a8f3ab7b135d734b8`.

The game still has only a bounded FG-Off copy probe. Its candidate camera
and five D3D11 images are not yet bound to a Streamline real-frame token, the
startup-selected lower swap, game phase markers and live provider fence.
Those require exact phase/owner validation and a controlled game integration
before any FG-On Skyrim claim. **Skyrim FG-On/generated frames: NOT RUN.**
