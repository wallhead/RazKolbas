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
