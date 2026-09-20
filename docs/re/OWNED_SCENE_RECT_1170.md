# Skyrim 1.6.1170 renderer rectangle call, corrected

The user-supplied `RazKolbas_Owned_Scene_Fix` is reference material. Its
`CODEX_TASK.md` candidate `Renderer::Begin + 0x192 = RVA 0xE44722` is not an
instruction boundary in this installation. The reference's own selector at
`SkyrimUpscaler.dll + 0x15881A..0x15882C` selects `+0x18B` when its runtime
branch byte is 1, or `+0x192` otherwise. The candidate was not game-verified.
The new route policy and D3D11 adapter follow the concepts in the handoff's
`RoutingCore.hpp`, `RipCall6.hpp` and `NativeUiRedirectorD3D11.hpp`, with
independent repository integration and Windows tests. No supplied binary or
archive was copied into the product.

For the locally recorded `SkyrimSE.exe` SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`
(image size `0x3870000`), independent disassembly of the saved decoded `.text`
at `artifacts/local/skyrim-decoded-text-2026-09-20-1657/text_1000.bin` finds:

| Item | Exact local result |
|---|---|
| Renderer::Begin | RVA `0xE44590` |
| `LEA RDX,[RSP+0x20]` | RVA `0xE446D8` |
| HWND load to RCX | RVA `0xE44711..0xE4471A` |
| Indirect call | RVA `0xE4471B`, bytes `FF 15 07 B2 90 00` |
| Indirect pointer cell | RVA `0x174F928`, PE import `USER32.dll!GetClientRect` |
| Following instruction | RVA `0xE44721`, `mov eax,[rsp+0x2c]` |

RVA `0xE44722` is byte two of that following instruction. No code write was
attempted there. The production `prepareRipCall6` gate validates exact bytes,
hash, image bounds and resolved pointer-cell RVA. `verifySkyrim1170ClientRectAbi`
checks the full decoded sequence from the stack RECT address through the call.
The new six-byte patch helper uses its own nearby read-only pointer cell; it
does not overwrite the shared User32 import cell or use the E8-only writer.
Its executable fixture validated original forwarding and exact restoration.

This identifies a possible **logical-size** boundary, not a completed reduced
world producer. It has not been activated in Skyrim. The next integration must
establish the native underlying swap chain, a stable reduced scene alias, exact
factory/GetBuffer consumer ordering, actual reduced viewport and coherent
colour/depth/motion inputs before enabling this callsite. The native UI adapter
is currently a D3D11 module tested under WARP with explicit downstream methods;
it is not installed into the game's ENB/ReShade context chain. A callsite patch
alone would risk rendering into a mismatched buffer and is therefore dormant.
