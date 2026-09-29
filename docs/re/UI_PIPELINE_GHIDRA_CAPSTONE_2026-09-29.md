# Skyrim/UI pipeline trace with Ghidra and Capstone, 2026-09-29

## Evidence and identities

The user's request is to reverse engineer the UI pipeline before changing FG
routing. The supplied AIO reports remain reference evidence, not instructions.
This trace independently reopened the existing Ghidra 12.1.3 project for the
pristine AIO `SkyrimUpscaler.dll` (SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`).
The installed Skyrim 1.6.1170 executable SHA-256 is
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
Its disk `.text` is SteamStub protected. Ghidra analyzed only two bounded raw
functions taken from the previously captured decoded live text of that exact
build: RVA `0xFA4F00` (0x400 bytes, SHA-256
`bf8d74987e9b07b8f6b9ae2882afaebfdf1008d9c08a06125135b33e34958ec5`)
and RVA `0xFC3300` (0x40 bytes, SHA-256
`f0e45329e80880502393ae04c99fc16b81ba55217000b64344714dfae63ea589`).
These raw imports do not include all game data or external callees. Python
Capstone 5.0.7 independently decoded the same game ranges and the original
AIO PE instruction bytes. Logs, raw excerpts and disassembly remain under
ignored `artifacts/local/ghidra/`; no binary evidence was staged.

## Verified game order

Ghidra decompiled the decoded AE Address Library ID 82084 function from RVA
`0xFA4F00`. It loops through the UI stack at object `+0x110` with count at
`+0x120`, calls the virtual method at `+0x30` for each eligible entry, exits
the loop, then calls RVA `0xFC3300`. Ghidra decompiled that wrapper as a
virtual call at `+0x28` on the renderer reached via its `+0x18` member.
CommonLibSSE-NG identifies those slots as `IMenu::PostDisplay` and
`GRenderer::EndFrame`, respectively. Capstone independently decoded:

```text
FA51D6  ff5030       call qword ptr [rax+0x30]
FA51EA  e811e10100   call 0xFC3300
FC330E  ff5028       call qword ptr [rax+0x28]
```

This proves call order and the common deferred submission boundary. It does
not prove that every UI pixel is written by that one virtual call or identify
the effective D3D11 target, blend, stencil or alpha state of its draws.
Therefore the 0.1.117 diagnostic's 15 full-frame reads **before** individual
`PostDisplay` calls cannot attribute writes issued during `EndFrame`. Enlarging
the earlier centre crop was useful for coverage but did not repair this timing
limitation. The prior 0.1.116 sequence remains valid evidence of a flat
centre region only.

## Reference route and owned-route difference

Ghidra decompiled AIO host RVA `0x1572A0`: its guarded direct-UI branch gets
an RTV from host `+0x438`, binds it at D3D11 context virtual `+0x108`, and
clears it to zero through virtual `+0x190`. Capstone confirmed the byte sites
at `0x1579A8` (select `+0x438`), `0x1579E5` (OM bind), `0x157A64` (zero the
clear vector), and `0x157A8D` (call the fetched `+0x190` method). Ghidra
decompiled the late OM hook at
`0x2015F0`: it recognizes its main scene target and conditionally substitutes
`+0x438`, the motion companion and the native DSV. The pre-Present function at
`0x201B70` conditionally copies direct UI `+0x438` to exported UI `+0x3E0`
through context virtual `+0x178` (Capstone site `0x20245A`, with a separate
Main/Loading site `0x202823`), then selects a final full-screen draw at
`0x202A5C`. These are version-specific **AIO** addresses, not Skyrim patch
sites or a complete live-game call schedule. The alternative difference
method can fill the same export with composite RGB and binary alpha; export
identity alone does not prove a genuine UI producer.

RazKolbas currently binds its native final RTV and display-sized DSV in
`NativeUiRedirector::commitPublishedUi()` and reasserts them immediately
before the verified `EndFrame` wrapper. It does not redirect these game draws
to a separate transparent UI RTV. Its `FgUiPlanes` class is source-only and
has no game caller. Thus the present opaque final backbuffer cannot be tagged
as a genuine FG UI plane. AIO's offset and exact draw branch are reference
contracts, not values to transplant into RazKolbas.

## RE-guided next observation

Version 0.1.118 moves the one-shot full-frame probe to four same-frame points:
before the first `PostDisplay`, after the complete menu loop and RazKolbas's
pre-flush rebind, immediately after the original `EndFrame`, and pre-Present.
This isolates the verified flush interval while keeping the existing native
UI route unchanged. It does not yet capture per-draw state or prove alpha.
If the new image delta is inside EndFrame, the next bounded trace must record
the first/last actual D3D11 UI draw destination, effective RTV/DSV and typed
extent, blend write mask, depth/stencil/raster/scissor and source-frame ID.
If not, trace the other changed interval rather than assuming the reference
route applies. Only after the actual producer and last writer are identified
should a separate transparent target be integrated and tagged for FG.

Debug and Release builds each passed 54/54 CTest groups. The verified
eight-payload MO2 archive is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.118-ui-endframe.zip`,
SHA-256 `3dc02a55bfcc78a720f220b6d5a4cb1655859520c81c5e115c4dca594e23d6cb`.
The installed DLL SHA-256 is
`0fbcb39165eaa588703c399f65caab866a3ce9bcb73bcb0ddf63e61ac3a8129e`;
the one-shot INI remains armed at SHA-256
`0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
All eight installed payloads matched the updated local manifest. The previous
DLL, INI and manifest are backed up in ignored
`artifacts/local/mo2-install-backup-0.1.118-2026-09-29`. Skyrim 0.1.118
runtime capture and game FG-On are **NOT RUN**. The assistant did not start
Skyrim; the next runtime step requires the user to start it and load a save.
