# Private FG-Off facade auxiliary resize

The preceding private-loader probe established a distinct ReShade outer swap
and three Streamline FG-Off lower Presents, but the auxiliary D3D11
swap-chain buffer could not resize. This checkpoint tests the resize lifetime
without starting Skyrim or changing the installed MO2 package.

`FgD3D11AuxSwapSource` now owns one hidden-window D3D11 swap-chain generation.
It uses the already verified native factory method and native D3D11 device,
checks the returned swap device and descriptor, and requires both ordinary and
sRGB render-target views. `prepare` creates a **new** swap/window/buffer at
the requested extent while the current generation remains alive. The bridge
then prepares shared D3D11/D3D12 surfaces, asks the lower swap to resize, and
commits the new render generation only after the lower swap reports the
expected descriptor. On an earlier failure it releases the candidate and
retains the previous generation. A poisoned bridge quarantines the auxiliary
swap alongside its render buffer and in-flight GPU objects. Native method
ownership and render-thread lifetime still have to be enforced at game
integration.

In the exact installed ReShade 6.8.0 / pinned private Streamline 2.14.1
probe on RTX 4080 SUPER, the outer swap resized from 160×96 to 192×108,
ReShade recreated its runtime, and the next upper Present reached the lower
swap (four callbacks total). A test-only lower observer then rejected a valid
224×126 resize *after* candidate preparation but before the actual lower
resize. The returned error was `DXGI_ERROR_INVALID_CALL`; the public
descriptor remained 192×108 and another upper Present succeeded (five
callbacks). A second rejected resize with reserved flag `0x80000000`
likewise preserved the old size and a further Present succeeded (six
callbacks). ReShade logged its expected warnings for the two injected
failures and recreated the runtime each time; it logged no `ERROR`.

The reserved-flags guard was added after a concrete failed probe: forwarding
`0x80000000` to the real Streamline lower swap returned
`DXGI_ERROR_INVALID_CALL`, but the following Present returned
`DXGI_ERROR_ACCESS_DENIED` (`0x887a002b`). The bridge now refuses reserved
DXGI swap flags before preparing a candidate or calling the lower proxy.
This is a pinned-Windows-SDK mask; it does not establish that every defined
flag combination is valid for this particular swap.

Release CTest passed 60/60, Debug CTest 57/57, and the final standalone
private probe exited zero with successful `slShutdown`. These results cover
FG Off only. No FG-On generated frame, Skyrim creation hook, ENB ordering,
fullscreen transition, device removal, or cross-thread teardown was tested.
The V5.4 MO2 installation remains 0.1.128 with FG Off.

Next: bind the pinned private loader, native factory callback, auxiliary
source and ReShade outer-swap path to the guarded V5.4 creation boundary, but
keep generated frames disabled. Verify startup and FG-Off Present in one
user-started Skyrim run before enabling FG-On evaluation.
