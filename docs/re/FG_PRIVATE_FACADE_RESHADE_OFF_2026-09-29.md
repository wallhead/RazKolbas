# Private Streamline and ReShade D3D11 facade, FG Off

## Question and selected stack

Can the pinned private Streamline 2.14.1 runtime own a D3D12 lower swap while
the exact V5.4 ReShade 6.8.0 build wraps a game-facing D3D11 facade, initializes
its effect runtime, and forwards more than one FG-Off Present? This is an
isolated-process proof, not a Skyrim or generated-frame result. Skyrim was not
started or modified.

The probe uses the private loader from the preceding checkpoint, a NVIDIA RTX
4080 SUPER on driver 616.92, and the installed ReShade `dxgi.dll` with SHA-256
`b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`.
The vendor DLLs and ReShade DLL are loaded from local, untracked paths; none is
in the commit. The selected adapter LUID matches across D3D11 and D3D12.

## Observed ownership

The Streamline-upgraded factory creates the D3D12 lower swap. Its factory
CreateSwapChain and lower Present methods are owned by the pinned
`sl.interposer.dll`; `slGetNativeInterface` identifies the lower native D3D12
device. ReShade is loaded before system `D3D11CreateDevice`, which it hooks.
Its factory subsequently calls the native DXGI delegate with a D3D11 device
that has the same adapter LUID as, but different COM identity from, the outer
ReShade device. The probe admits only the selected delegate, adapter, window
and 160x96 descriptor at the exact validated factory slot. It creates the
facade on the **delegate's native D3D11 device** and returns it to ReShade.
ReShade returns a distinct outer swap whose Present method is owned by its
exact DLL. The older static harness had returned the facade itself at this
boundary (`sameIdentity=1`); that observation was insufficient to claim a
ReShade swap wrapper. The private-loader probe confirms `sameIdentity=0`.

## Render-target failure and correction

With the original ordinary `DXGI_FORMAT_R8G8B8A8_UNORM` D3D11 texture,
ReShade logged `Failed to create back buffer render targets!`. An explicit
sRGB render-target view on that texture failed with `0x80070057` on both
native and wrapped D3D11 devices. ReShade 6.8.0's
[runtime code](https://github.com/crosire/reshade/blob/v6.8.0/source/runtime.cpp)
creates both ordinary and sRGB render-target views for each back buffer.

An auxiliary native D3D11 swap chain on a separate hidden window yields a
back buffer with the same typed UNORM format. Both ordinary and sRGB view
creation returned `S_OK`. The bridge can now accept such an externally owned
buffer after checking D3D11 device identity, dimensions, format, sample count
and render-target binding. The facade exposes it through `GetBuffer`. External
buffer `ResizeBuffers` returns `DXGI_ERROR_UNSUPPORTED` before changing state;
the auxiliary swap-chain resize and retirement contract is not implemented.
This fail-closed path is confined to the offline facade proof and is not wired
into the Skyrim plugin.

## Actual probe result and limits

The Release probe used the private loader and exact installed ReShade. It
verified a distinct ReShade upper swap and ReShade-owned Present, two usable
render-target view formats, `Present(TEST)=S_OK`, three real upper Presents,
and three lower-Present callbacks. `slDLSSGSetOptions` accepted Off;
`slDLSSGGetState` returned `eOk` with `numFramesActuallyPresented=1` after
the last Present (the state field is per evaluation, not an accumulated
three-frame count). Private `slShutdown` succeeded. ReShade logged
`Recreated runtime environment` and compiled `AmbientLight.fx`; it did not
log the earlier render-target error. This proves ReShade runtime
initialization, not that a particular effect visibly changed the image.

Release CTest passed 60/60; Debug CTest passed 57/57. The standalone private
probe exited zero. The prior direct call to ReShade's exported
`D3D11CreateDevice` crashed in this standalone process, so the final probe
uses the system entry after loading ReShade and relies on its observed hook.
No Skyrim runtime test, FG-On test, generated-frame test, visual output
comparison, resize, fullscreen, or device-recovery test occurred. V5.4 MO2
still has 0.1.128 with FG Off.

Next: give the auxiliary D3D11 swap-chain buffer a transactional resize and
retirement owner, then bind the validated private-loader and wrapper path to
the guarded V5.4 creation boundary. Keep FG Off until the full frame tags,
camera, UI, and Present contract passes one bundled user-started Skyrim test.
