# ENB target dimensions and the reduced scene

2026-09-23. Static evidence from the user's installed `D:/TESV_EX/d3d11.dll`,
SHA-256 `47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58`,
4,664,320 bytes, PE image size `0xAAE000`. All addresses below are RVAs.
No original DLL was changed. Disassembly used the local Capstone binding and
the repository's PE reader. This is a necessary-condition finding, not a
runtime proof that a particular missing effect is fixed.

## Reference dimensions come from the swap description

ENB's creation function `0x5E410` calls the downstream swap's GetDesc at
`0x5E53B` (return `0x5E53E`). It stores Width at `0x24B510` and Height at
`0x24B514` via instructions `0x5E546` and `0x5E554`.

The dimensions are not immutable startup values. Function `0x486D0` clears
them, calls its underlying swap's GetDesc at `0x4872A` (return `0x4872D`),
and stores them again at `0x48735` / `0x4873F`, before calling `0x4F170`.
Direct calls to this refresh function occur at `0x6C474`, `0x6C53B` and
`0x6C5DB`. A scan of .pdata instruction ranges found no other direct
RIP-relative writes to these two globals. Indirect writes are not excluded.
The resource allocation code at `0x4F170` reads these globals repeatedly.
Changing only a comparison or writing the globals late would not establish
consistent resource allocation and is not the selected fix.

## Metadata is present in the device-wrapper contract

GUID `b272d61a-acbe-4117-8ede-e24c4ee88721` at `0x24C440` identifies a
48-byte private-data record. Setter `0x71090` calls SetPrivateData; reader
`0x710E0` calls GetPrivateData. Texture2D metadata has type 2 at +0, Width
at +4, Height at +8 and DXGI format at +0x10. The tail includes a pointer;
the new diagnostic does not dereference it.

CreateTexture2D `0x5BD60` fills this record and writes it at `0x5BE20`.
CreateRenderTargetView `0x5C2D0` reads resource metadata at `0x5C35A` and
copies it to the view at `0x5C36E`. This occurs even with a null view
descriptor. Therefore the current use of a null RTV descriptor is not
evidence that ENB metadata is missing.

The first early GetBuffer (return `0x5E580`) is tagged with a record whose
dimensions came from GetDesc. ENB releases the temporary buffer reference
after tagging it. The second call (return `0x5E795`) obtains a descriptor,
performs an UpdateSubresource path and releases its buffer. These calls
alone do not prove permanent cached views or ownership.

## The HDR target comparison controls additional MRT attachments

In OMSetRenderTargets `0x68F40`, the chunk at `0x69145` reads RTV0 metadata
at `0x691A5`. Width/height/format enter EBP/R12D/EBX. Subject to earlier
mode checks, at least two requested targets, valid metadata, and another
mode condition, the following checks control the attachment path:

| RVA | Condition |
|---|---|
| `0x691CB` | Metadata Width equals global `0x24B510` |
| `0x691D3` | Metadata Height equals global `0x24B514` |
| `0x691DC` / `0x691E1` | Format is 10 or 26 |

Width/height mismatch skips the attachment path. On the matching path,
`0x691ED` sets the forwarded target count to seven; `0x691F7` and
`0x69206` populate slots 5 and 6 from globals `0x45B100` / `0x45B118`.
The real context receives the bind at `0x6923B`. Effect names and the
contents of these two attachments have not been identified here.

The installed ENB OMGetRenderTargets at `0x5DCF0` forwards to the underlying
context at object +0x6C68, slot 89. The diagnostic uses that public getter
after the existing downstream bind; it neither changes targets nor invokes
the private method through a guessed pointer.

## Relation to our observed route and ReShade

The failed spatial run (PID 10660, 11:32 launch) returned 2560x1440 to
ENB's two early GetBuffer callers, then armed the 1707x960 owned scene
after outer ENB creation. Skyrim's `0xE4CC87` caller subsequently received
the reduced surface. RazKolbas has no GetDesc rewrite on this route.
This establishes a creation-contract difference. Actual live global values,
HDR view metadata and branch consequences were not measured in that run.

The working reference's caller-dependent GetDesc (`SkyrimUpscaler.dll`
`0x1EA000`) can report render dimensions to `d3d11.dll` callers, whereas
GetDesc1 forwards. Its scene resource is allocated during nested factory
creation. See the supplied investigation 09; this is a separate contract
from simply returning a smaller buffer late.

ReShade remains in the chain, and a reduced-resolution interaction with it
is still possible. Native DLAA looks correct with it enabled, but that does
not exonerate its reduced path. No ReShade-off comparison was performed.
Do not interpret a changed image hash at Present as proof that all ENB
effects were preserved.

## Bounded runtime probe

The probe validates the ENB file identity plus the exact 27-byte comparison
block in the loaded image before enabling private global reads. It reports
the actual HDR texture extent, RTV private metadata, current reference
dimensions and bound-target mask after ENB. Sampling is restricted to the
owned World phase with multiple targets and format 10 or 26: at most one
sample per 600-call observation window, up to 64 windows. Each window permits
at most eight candidate getter reads even if no HDR target qualifies; a
successful HDR sample closes the window early. All getter-acquired
COM references are released. No effect settings, binding, metadata or shader
is modified by the probe. The observations occur on the existing dispatch
thread; no cross-thread snapshot consistency is claimed.

`dimensionMatch=false` with readable reference and valid metadata proves
the necessary size condition is false for that observation. Missing slots
5/6 alone does not prove a size failure: other conditions also gate them.
Matching dimensions and present slots still do not prove full ENB appearance.

The earlier recommendation to preserve native ENB startup queries in
`OWNED_SCENE_LIVE_CHAIN_1170.md` was a provisional strategy. It must now be
reconsidered together with GetDesc and early scene-publication contracts.
Do not broaden GetBuffer routing in isolation.
