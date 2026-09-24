# Native UI target and viewport order

## Evidence boundary

The user supplied `RazKolbas_UI_Boundary_Codex_Report.md` (SHA-256
`687382c2f295fca0016af9d156ef0ecc28df817d6fdfbc478a30b9dd8e94ff45`)
and `RazKolbas_UI_State_DeepDive_14.zip` (SHA-256
`ae8470da530613cd26e7508fc171fc3b08e68117c690ceec2102aeb9fad8d8d2`).
They remain reference inputs and are not staged. The archive was extracted
only below ignored `artifacts/local/`; every entry matched its supplied
`SHA256SUMS.txt`.

The boundary report's correction is confirmed by the pinned CommonLibSSE-NG
source: base `IMenu::PostDisplay()` invokes `uiMovie->Display()`. At game RVA
`0xfa51d0`, RCX is loaded from the menu collection immediately before the
virtual call at `0xfa51d6`. The preceding call at `0xfa51cb` is a helper and
does not establish a second menu display virtual. The current menu callback
therefore runs before the base movie draw. The screenshot alone cannot prove
whether the centre symbols were in the DLSS input, the DLSS output, or only
the final UI composition.

## Reproduced state defect

`NativeUiRedirector::onRSSetViewports` translated a reduced full-scene
viewport only when the native output target was already bound. This legal
ordering escaped that condition:

1. bind a reduced offscreen target;
2. set the reduced full-scene viewport;
3. restore the cached reduced scene RTV without setting the viewport again.

During the native UI phase, step 3 translated the cached scene RTV to the
native output. The viewport remained reduced because step 2 occurred while an
offscreen target was active. This created a native render target and reduced
viewport pair for the following draw.

A WARP regression using render 32x16 and display 64x32 first measured width
32 after this sequence. The production repair queries the effective viewport
immediately after a compatible scene-target translation and applies the
existing full-UI viewport mapping if it is still reduced. It uses the chained
viewport setter and leaves viewport origin and depth range intact. Unknown
attachments continue through the existing compatibility-fault path.

Debug and Release complete builds each passed all 40 CTest groups after the
repair. This proves the D3D11 state transition in isolation. The user then ran
0.1.71 and reported that both symbols remained, so this defect is not their
cause. Installed 0.1.72 captures the exact prepared colour input, raw DLSS
output before sharpening/UI and final composition from one frame; its runtime
capture is pending.
