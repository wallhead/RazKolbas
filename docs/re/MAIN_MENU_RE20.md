# Main Menu viewport reference cross-check, 2026-09-26

An independent subagent performed read-only PE mapping and Capstone 5.0.7
disassembly on the supplied AIO `SkyrimUpscaler.dll`, SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
These are reference observations, not instructions to copy code or modify
the DLL. The mapped bounded windows and RVA findings are:

- Menu tracking at RVA `0x1e9450` compares `Main Menu` (string object RVA
  `0x329300`) and records its state at menu-state `+0x12`; `MapMenu` uses
  `+0x11`. Bounded window `0x1e9450+0x240` SHA-256 is
  `ff6ebb840b5abf104938e814a4f883d790036b0094a7f0fc0427ebcca8406869`.
- Helper RVA `0x1e96a0` returns true if either menu state is set or the
  active menu name matches. Window `0x1e96a0+0x55` SHA-256 is
  `2b9ca6777e54c381747b3c9f1df42dab066d281e6d6b70f6646339e1c8e16214`.
- Viewport hook RVA `0x2012c0` has a Main Menu/MapMenu branch at
  `0x2012e9..0x201331` controlled by initialized byte RVA `0x378498=1`.
  That branch supplies the reference's render width/height at host
  `+0x2c/+0x30`. The ordinary late branch at `0x20138b..0x20141a`
  converts one exact render-sized viewport to display dimensions at
  `+0x24/+0x28`. Window `0x2012c0+0x175` SHA-256 is
  `68538eff1472fa2c55b0d89f14d30777d2bbd42ca6b9339de2566eea9b124164`.
- Native UI transitions at `0x1573fa..0x157404` and
  `0x1579fa..0x157a04` clear byte `0x378498` after binding native colour
  and depth and setting a late-state flag. The late OM hook at
  `0x2015f0` remaps recognized scene colour, motion companion and depth;
  unknown RTV0 is forwarded. The late PS hook at `0x201440` remaps known
  source depth only, not arbitrary colour.

The 0.1.96 Skyrim log independently shows our owned scene is reduced to
1707x960 at swap creation, before world depth admission, while the actual
swap is 2560x1440. The title screen may therefore need a distinct native
UI handoff. The reference's static branch does not prove its live flag
timing or establish why RazKolbas inventory pixels disappear. A targeted
title-screen diagnostic should compare menu state, viewport input/output,
OM identities and the publication phase before and after a save loads.

The subagent's further read-only check found no `InventoryMenu`,
`Inventory`, or `Hero` ANSI/UTF-16 literal in this DLL. The bounded menu
tracker handles Map, Main, Stats, Magic and Loading menus, with no named
inventory branch. This does not rule out indirect or hash-based inventory
logic elsewhere. In callback `0x1572a0`, the saved original call at
`0x1572ae` precedes the native colour/depth binding and late-state change.
The static code does not identify the hero colour target or place the
inventory offscreen composite relative to deferred `EndFrame`. That timing
still needs runtime evidence, including target identity, viewport, later
sampling resource and the sampling draw.
