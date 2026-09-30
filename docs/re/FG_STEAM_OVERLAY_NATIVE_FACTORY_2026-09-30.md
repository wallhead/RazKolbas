# Steam overlay native factory forwarding

## Measured 0.1.131 failure

The user-started V5.4 run beginning at 09:25:42 on 2026-09-30 prepared the
private Streamline lower but rejected the native factory code before facade
substitution. The code had already changed **before** private preparation and
remained identical afterward. The direct read and local image snapshot agreed.
The entry protection was `PAGE_EXECUTE_READ` (`0x20`); the previously considered
execute-only snapshot hypothesis is ruled out for this run.

Native `System32/dxgi.dll` SHA-256 was
`25678116473558a56524b5b391f66f5c81febe8b2c463422f35807f77f3207c1`,
base `0x7ffde7050000`, factory table RVA `0xa1428`, and slot-10 method RVA
`0x67c90`. At `0x7ffde70b7c90`, its first sixteen bytes were
`e9f585ab0341564157488dac2468ffff`. The E9 reached a private executable
allocation at `0x7ffdeab7028a`, whose first fourteen bytes were
`ff250000000050c25dd1fc7f0000`. This relay resolved callback
`0x7ffcd15dc250`. Native fallback subsequently reached at least 145,200
successful outer Presents, with zero observed failures; it did not bind the
FG facade. Saved raw log and Capstone output remain in ignored
`artifacts/local/live-0131`.

The process exited during a read-only module-snapshot attempt; that attempt
returned Windows error 87. Therefore the external game module map did **not**
independently certify the callback owner. Attribution below is supported by
the exact local binary and an isolated reproduction. The new production
validator must establish the full loaded owner/chain in Skyrim before use.

## Exact Steam callback and reproduction

Pristine local reference: `C:/Program Files (x86)/Steam/GameOverlayRenderer64.dll`.
File SHA-256:
`fe4440b1027e96052b376ee04290d91df95a76b0d97d808cd92c993f0dec47b9`.
File size 1,679,512; mapped image size `0x1d0000`; file version
`11.05.74.16`; AMD64 PE32+.

Capstone disassembly at callback RVA `0x9c250` identified the 129-byte
`IWrapDXGIFactory1::CreateSwapChain` handler. It retains all four Windows x64
arguments, loads the original pointer from RVA `0x180340`, calls it once at
RVA `0x9c2a8`, optionally postprocesses a successful non-null output, then
returns the original HRESULT. Exact bytes are recorded in the machine-readable
[patch descriptor](../../patches/skyrim/steam11057416.factory.inline-native-chain-v1.json).

Loading the unmodified Steam overlay DLL and calling the native
`CreateDXGIFactory1` in an isolated process naturally reproduced the hook:
the native first five bytes became E9; its private FF25 relay targeted overlay
base plus `0x9c250`; the original slot at `0x180340` pointed to relay minus ten.
The trampoline copies native `40 55 53 56 57`, then E9 returns to native entry
plus five. No Steam file or memory was patched by the experiment.

## Compatibility implementation

0.1.132 admits this exact chain only after checking native identity, Steam
filename/hash/size/PE/image identity, all 129 callback bytes, unchanged native
tail, both relative jumps, absolute callback pointer, original-pointer slot,
memory owners/protections, and relay/trampoline allocation equality. It pins
the Steam module, then re-reads every mutable link. Unknown chains still reject.
Only the **local** validation snapshot's first five bytes are normalized.

The existing atomic native vtable callback keeps the native entry as its saved
next. Calling it preserves Steam's inline owner and forwards auxiliary D3D11
swap creation through Steam before constructing our facade. Steam's code,
relay, trampoline, data and installed hook are untouched. The Steam module
is pinned; its private trampoline allocation remains owned by Steam. This is
a forwarding compatibility path, not a replacement Steam DLL. Steam binaries
are neither copied into the package nor committed.

The user noted that the reference AIO disables Steam overlay for FG and asked
whether a proxy could preserve it. This implementation preserves the factory
chain without disabling the overlay. It does not yet prove that visible overlay
rendering/input or generated frames work in Skyrim.

## Validation and next gate

The new pure chain test initially failed to compile because the validator API
did not exist, then passed after implementation. Focused Release factory tests
passed 84 assertions in eight cases, including altered identity, callback,
relay, original pointer, copied instructions, continuation and address bounds.

`RazKolbasFgPrivateGameRouteProbe` now optionally preloads the exact Steam DLL.
With installed Streamline 2.14.1 and exact ReShade 6.8, both Steam-preloaded and
no-Steam control processes exited zero. The Steam case explicitly accepted
the full factory proof; both cases substituted once, passed lower/upper
`Present(TEST)`, performed a real upper Present, resized from 160x96 to 192x108,
then released facade, wrapper, route, D3D11, factory and ReShade in order.
FG was Off. These are actual GPU/presentation outcomes in standalone processes.

Final 0.1.132 builds passed Release CTest 61/61 and Debug CTest 57/57.
Independent review found no significant actionable issues, ran the factory
cases (84 assertions/eight cases) and owned-route cases (102 assertions/ten
cases), and confirmed the saved native entry avoids a borrowed Steam
relay/trampoline lifetime. Its descriptor/documentation reminder is addressed
in this checkpoint.

An additional recovery regression found the INI parser rejected the documented
native/Streamline factory disable IDs and the new Steam ID. The three profile
IDs are now registered with separate bits; unknown and duplicate IDs remain
invalid. The regression failed before the fix and passed six assertions after.
Final Release 61/61 and Debug 57/57 passed again. The reviewer checked this
addition and passed configuration tests (128 assertions in 19 cases).

The complete staged runtime passed the Steam-preloaded route probe. Final
0.1.132 was installed while Skyrim was closed in the V5.4 MO2 mod, preserving
the INI and all companion files. All 14 installed manifest hashes matched;
DLL SHA-256 `c483578f52648aea8ad9ee564fcfd49d7573e2220bddf462279b613de50f50b8`.
Previous 0.1.131 DLL/INI/manifest are backed up under ignored
`artifacts/local/backup-v54-before-0132`. Package staging is ignored
`artifacts/local/stage-v54-fg-steam-0132`; the real probe output is ignored
`artifacts/local/steam-route-0132.txt`.

**0.1.132 Skyrim/ENB facade binding: NOT RUN. Steam overlay appearance/input:
NOT RUN. FG-On generated frames: NOT RUN.** The next required game run is a
user-started launch/load with FG Off, checking acceptance, facade substitution,
real Presents, world/UI/effects and Steam overlay if available. Keep these
runtime results distinct from standalone compatibility success.
