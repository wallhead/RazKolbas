# AIO native UI RE22: local cross-check

Reviewed 2026-09-27. The supplied `C:/Users/user/Downloads/AIO_Native_UI_RE_22.zip`
is reference evidence, not instructions to copy reference host code. The
original `SkyrimUpscaler.dll` has SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`
and 14,211,584 bytes, exactly the packet's identity. The original stays
unchanged and is not staged. The packet's 74-file manifest passed its SHA-256
check. Independently mapping the original PE verified all 23 selected code
span hashes, five menu object pointers/string bytes and the 660-byte UI shader
hash. Capstone 5.0.7 locally decoded the client, mouse, screen and menu
predicate windows. The packet reports 47 synthetic original-CPU checks per
compiler; these were **not** re-executed locally and do not test Skyrim/GPU.

## Corrected menu identity and separate dimensions

The Main Menu string object is DLL RVA `0x329320`, Loading Menu is `0x329300`,
and MapMenu is `0x3292a0`. Their tracker bytes are `+0x11`, `+0x12`, and
`+0x15`, respectively. Capstone confirms the helper at `0x1e96a0` tests
`+0x11/+0x12`; the startup viewport override is therefore **Main/Loading**.
This corrects the old Main/Map labels in `MAIN_MENU_RE20.md`.

Three independently bounded AIO wrappers carry different dimensions:

| DLL RVA | Observed write | Implication |
|---|---|---|
| `0x157ac0` | Render width/height into `RECT.right/bottom` after one saved client-rect call when host `+0xb7` is zero. | Reduced renderer sizing is scoped, not a global client rectangle. |
| `0x157b00` | Display width/height as floats into mouse object `RCX+0x14/+0x18`, then tail-calls saved original. | Mouse metadata is independent of render size; the fields' game-side meaning needs a trace. |
| `0x157b30` | Display width/height as uints into a two-value `RDX` argument before saved original, when host `+0xb7` is zero. | Screen-size metadata can stay native without changing every viewport. |

AIO's native transition can bind a native colour/depth pair and clear its
bootstrap viewport override after its SR function returns early for missing
guide pointers. That is a CPU control-flow finding, **not** proof of a valid
background, stencil contents, or title-screen pixels. Its separate
Present-window OM scope and branch-conditional native-RGBA blend path are
plausible ways to preserve final UI; neither has been observed live here.
The reported blend `ONE/INV_SRC_ALPHA` for RGB and `MAX` for alpha is
consistent with premultiplied UI pixels; the producer's alpha convention and
the active branch remain unverified.

## Exact 1.6.1170 game-side candidate mapping

The V5.4 Stock Game `SkyrimSE.exe` SHA-256 is
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`,
the same exact executable as the preserved decoded `.text` snapshot (snapshot
SHA-256 `75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3`).
The installed 1.6.1170 Address Library hash is
`c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452`;
its format-2 decoder consumed all 428,461 records. Capstone on the decoded
game bytes distinguished aligned CALLs from the alternate pair:

| AIO candidate | Address Library ID / base RVA | Site and decoded state |
|---|---|---|
| Mouse metadata | `51498` / `0x913ef0` | `+0xb` = CALL at `0x913efb` to game `0xfba6c0`; the saved callee reads object float fields including `+0x14`. |
| Screen-size pair | `77397` / `0xe4cc50` | `+0x102` = CALL at `0xe4cd52` to `0xe4fb90`, which copies a 28-byte structure from a stack argument containing width/height. |
| Native transition | `52727` / `0x972590` | `+0x7a4` = CALL at `0x972d34`, matching the earlier inventory internal-transition finding. |
| Alternate IDs | `50604`, `75590`, `51855` | The packet's corresponding offsets land inside an instruction or in `INT3` padding in this exact snapshot; they are not the selected 1.6.1170 sites. |

These are static call-site candidates, **not installed RazKolbas patches**.
Their live hook owners, input/output values, game ABI and actual UI draw
target remain unverified. RazKolbas currently scopes only its verified
`Renderer::Begin` client-rect replacement; source search found no
`SetScreenSize` or `ProcessMouseMove` wrapper. The 0.1.102 cursor replay
candidate has not yet run in Skyrim. Its inventory result should be observed
before changing these independent metadata consumers.

## Next discriminating game evidence

In a user-started cold title screen and an affected HUD/inventory frame,
record before/after values at the exact `0x913efb` and `0xe4cd52` consumers,
the contributing UI draw's effective RTV/viewport and writable DSV, and the
native image before/after any late full-frame publication. If a consumer is
fed reduced values while its UI target is native, a narrowly scoped metadata
fix can then be specified and tested without altering jitter, MV scale, NR,
DLSS model or unknown offscreen surfaces. Static mapping is complete;
Windows/GPU and game visual verification of this hypothesis are **NOT RUN**.
