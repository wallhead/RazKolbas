# Skyrim 1.6.1170 deferred Scaleform flush

## Binary identities

- `SkyrimSE.exe`: SHA-256
  `c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
- Active `TrueHUD.dll`: SHA-256
  `a7eec5e7e6997e04b158fa3add1e326682ee9720da98c5758c4e626bdb705f01`.
- Active Oathvein `TrueHUD_Widgets.swf`: SHA-256
  `b8236eff66d4483774f25a1d782382e2069b300393e42785bb6bf5850732944e`.

The Skyrim executable's disk `.text` is SteamStub protected. Skyrim addresses
below come from the bounded decoded live-code capture made from that exact
process image. TrueHUD addresses come from its conventional on-disk PE.

## Common flush after the menu stack

The verified menu loop calls `IMenu::PostDisplay` through virtual slot `+0x30`
at RVA `0xfa51d6`. After the loop exits, the same function executes:

```text
fa51df  mov rax, [rip+0x264bfe2]
fa51e6  mov rcx, [rax+0x10]
fa51ea  call 0xfc3300
```

Address Library AE 1.6.1170 maps RVA `0xfc3300` to ID 82733. Capstone 5.0.6
and the standalone Capstone 5 decoder independently produce:

```text
fc3300  sub  rsp, 0x28
fc3304  mov  rax, [rcx]
fc3307  mov  rcx, [rax+0x18]
fc330b  mov  rax, [rcx]
fc330e  call [rax+0x28]
fc3311  lea  rcx, [rip+0x22c54a8]
fc3318  add  rsp, 0x28
fc331c  jmp  0xe4af80
```

CommonLib's `RE::Scaleform::Render::Renderer` layout places `EndFrame` at
virtual slot 5, byte offset `0x28`. This establishes the shared deferred GPU
submission boundary after every observed menu's `PostDisplay`. It explains why
all 17 before-menu snapshots were identical and only the final pre-Present
snapshot contained the two fragments.

## TrueHUD ownership evidence

Ghidra 12.1.3 analysis of the exact active DLL found the actor-info-bar vtable
at VA `0x18008d7f8`. Slot 9 points to VA `0x18004fe80` (RVA `0x4fe80`). Its
decompiled data flow obtains the actor world position, applies the configured
Z offset, projects to screen space, applies distance scaling and
`fInfoBarScale`, then submits position and scale through the Scaleform value's
display-info path. Capstone decoded the same RVA and its direct calls without
an instruction-boundary disagreement.

The active ActionScript executes `Bars.setMask(IndicatorMask)` and the bar
implementation applies further transparency masks. The active INI enables
actor info bars. The two visible remnants resemble per-actor level text, while
the masked bar geometry is absent. This is high-confidence ownership evidence,
but it does not alone identify each final pixel.

## RazKolbas state defect

`NativeUiRedirector` already learned a reduced D24S8 attachment and created a
display-sized depth/stencil companion. `commitPublishedUi()` cleared that
companion, then `bindNativeTarget()` bound the native color target with a null
DSV. Because Scaleform submits queued masked geometry later in `EndFrame`, it
entered that flush without the stencil attachment needed by the masks. Plain
text can still draw, matching the captured symptom.

Version 0.1.75 leaves the prepared display-sized DSV bound with the native
color target when entering `NativeUi`. `bindNativeForProcessing()` continues to
bind color only, so DLSS/publication work does not inherit UI depth state. A
WARP regression demonstrates the old null-DSV failure and verifies that the UI
commit now leaves a display-sized DSV bound before any later menu target bind.

The causal link from this correction to the Skyrim image is not a runtime fact
until a user-started 0.1.75 run removes the fragments. The 0.1.74 scissor
translation executed hundreds of times and did not remove them, disproving the
prior scissor-only hypothesis.

## 0.1.77 exact-flush rebind follow-up

The user-started 0.1.75 run removed the two isolated centre fragments, which
is actual-game confirmation that the missing D24S8 attachment caused that
mask failure. Installed 0.1.76 kept the publication boundary latched and
retired scissor mutation, but the health, stamina and magicka fills still
periodically became black. Its log confirms that the route remained at the
menu boundary, so boundary oscillation was not the fill cause.

Capstone 5.0.7 independently decoded the exact reference
`SkyrimUpscaler.dll` SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
`SetupDepth` at RVA `0x1f3e40` creates distinct display-sized writable depth
at host `+0x330` and sampled UI depth at `+0x388`. The sampled resource is
cleared at creation through context virtual slot `+0x1a8` with flags `3`,
depth `1` and stencil `0`. The late world tail at RVA `0x156f75` separately
obtains the writable DSV from `+0x330` and clears it each frame with the same
values. This confirms RazKolbas's resource separation and clear values; it
does not support another change to them.

The remaining placement difference is submission timing. PureDark's late OM
translation remains active for the final draw calls. RazKolbas published and
bound the native DSV before Skyrim entered its menu loop, then relied on that
binding surviving until the common deferred Scaleform flush. Any intervening
menu can retain native colour while dropping the DSV. Version 0.1.77 therefore
adds a third exact-hash CALL profile at RVA `0xfa51ea` and reasserts the
current native MRT set with the existing full-size DSV immediately before the
verified `GRenderer::EndFrame` wrapper. It does not clear depth/stencil again,
does not change the MRT count or auxiliary identities, and always forwards the
original wrapper once. Actual-game fill stability remains **NOT RUN**.
