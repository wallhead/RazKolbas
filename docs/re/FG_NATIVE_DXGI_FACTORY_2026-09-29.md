# V5.4 native DXGI factory below ReShade

The user started Skyrim with RazKolbas 0.1.127 in the V5.4 MO2 modlist on
2026-09-29. The installed DLL hash was
`2d314a7e403d0198beed523e0af5ccd8f52b554b5dac6fe97d110f211ec2300f`.
At 19:57:31 local time, the log confirmed the exact ReShade 6.8 factory
wrapper (`dxgi.dll` hash
`b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`,
factory table RVA `0x3ee350`, method RVA `0x14a4f0`). Its `[this+8]`
delegate was `0x1933ccfae10`, with vtable `0x7ffc0b5f1428` and slot-10
`CreateSwapChain` method `0x7ffc0b5b7c90`. The method page was executable;
`GetModuleHandleExW` identified its owner filename as `dxgi.dll`. The
ReShade factory call succeeded and returned its own swap wrapper. The
existing owned-scene route published 1485×835 scene colour before ENB
GetDesc/GetBuffer at 2560×1440. A later 37,200th observed Present returned
`S_OK`. These facts are runtime observations; no FG frame was requested.

Separately, a local x64 process loaded the Microsoft-signed
`C:/Windows/System32/dxgi.dll` and obtained the **same absolute** vtable and
slot-10 method addresses. Its loaded base was `0x7ffc0b550000`, yielding
vtable RVA `0xa1428` and method RVA `0x67c90`. The exact disk file was
1,018,016 bytes, mapped image size `0xf7000`, SHA-256
`25678116473558a56524b5b391f66f5c81febe8b2c463422f35807f77f3207c1`.
The 16-byte method prologue was
`40 55 53 56 57 41 56 41 57 48 8D AC 24 68 FF FF`.
The equal absolute addresses are strong evidence that the observed game
delegate is this system DXGI factory, but the 0.1.127 game log recorded only
its owner filename. The next hook verifies the full path, hash, table,
method and bytes in the game process before any write. A different Windows
DXGI build fails closed.

0.1.128 adds an exact-profile, process-lifetime, atomic vtable slot-10
pass-through hook during the ReShade factory callback, before ReShade calls
its native delegate. It calls the original native method once with unchanged
arguments and returns the exact HRESULT and raw swap pointer to ReShade. It
only observes the selected raw lower swap's DXGI interfaces and D3D11 device
identity. The existing ReShade/ENB wrappers and early owned-scene route
remain in place. FG and any D3D12 lower replacement remain Off. A WARP
integration test successfully patched this exact system DLL vtable in its
own process, forwarded one real `CreateSwapChain`, observed a swap, and
restored the pointer. This does not prove the game hook until a user-started
0.1.128 run.

A later separate-process ReShade/Streamline probe found that
`slUpgradeInterface(factory)` replaces this same native vtable slot with
`sl.interposer.dll` RVA `0x26510`. Its exact hash and bounded Capstone
control-flow result, plus successful FG-Off and focused FG-On wrapper runs,
are recorded in `FG_RESHADE_STREAMLINE_WRAP_OFFLINE_2026-09-29.md`. This
does not change 0.1.128's pass-through behavior or game-test status.
