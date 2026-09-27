# Native UI resolution audit 21: local review

Reviewed 2026-09-27 against source commit `9590420` (installed 0.1.102). The supplied `RazKolbas_Native_UI_Resolution_Audit_21.zip` is an external analysis packet, not an implementation or game-runtime result. Its immutable copy remains in Downloads; no archive contents or binaries are staged.

## Confirmed in this checkout

| Packet claim | Local evidence | Limit |
|---|---|---|
| The first menu publication is gated by scene readiness. | `shouldUseMenuPublication(false, false, true)` is false in `include/rk/OwnedRouteProfile.hpp`. `beforeMenuDisplay` samples only after frame 12 with a ready world gate and calls `processOwnedWorldFrame` only if that predicate and late routing pass. | The predicate alone does not show which surface each title-screen draw uses. |
| The world publisher cannot provide a colour-only early UI route. | `processOwnedWorldFrame` validates both `numbers.motion` and `numbers.depth` before it can run `presentSdrSrFrame`; even spatial fallback is downstream of that check. | A separate colour-only route would still need a current scene background and a valid pre-UI boundary. |
| Full late-3D observation is used as a universal UI gate. | `NativeUiRedirector::companionsReady` requires two valid 8/9-event observations, depth SRV/DSV resources and exactly two auxiliaries; one mismatch sets `observationContractFault_` until teardown. `bindNativeTarget(true)` binds no DSV if this gate fails. | Relaxing this gate without a validated writable UI stencil would regress masked UI. |
| The reduced-menu latch is inferred from motion-MRT shape rather than menu identity. | `onOMSetRenderTargets` tests scene RTV0, two targets, a render-sized R16G16_FLOAT second view and known reduced DSV. It does not inspect `InventoryMenu`. | Whether this branch ran in the user's affected title/HUD frame is unmeasured. |
| Late publication outside inventory can replace native UI. | `beforeDeferredUiFlush` still calls `publishHeldMenuScene` whenever the reduced pass is pending and `InventoryMenu` is absent. The 0.1.99 inventory capture established that the same copy erased an already visible list; 0.1.100's inventory-specific skip restored the list. | The 0.1.99 capture proves the inventory instance, not every other menu. |

The last preserved 0.1.101 log shows pre-Present spatial publication at 1707x960 to 2560x1440 before the native route activates (for example frame 10200), then activation at frame 10862. This supports an early/late phase difference but is not a per-draw measurement of title text. The 0.1.102 cursor replay candidate is installed but has not been tested in Skyrim.

## Implementation boundary

The packet's separation of base native UI colour/stencil, late 3D remapping, and temporal DLSS inputs is technically sound. It is not safe to turn the route on by changing only the Boolean predicate: that would leave `processOwnedWorldFrame`'s guide validation and `companionsReady`'s stencil requirement intact. It is also not safe to skip every late copy: some passes still need a valid current background. The existing inventory skip must be preserved until a general pixel-ownership mechanism replaces it.

The next source change should be a focused, independently testable cold-start UI route only after the actual title-screen pre-UI draw boundary and native colour/stencil resources are identified. Separately, a later change should track whether native UI pixels have been written before any deferred full-frame copy. A motion target format or menu name alone must not authorize an overwrite. For each candidate, use a raster draw test with a one-pixel pattern and mask; `ClearRenderTargetView` is insufficient because it ignores viewport/scissor. Preserve NGX temporal admission and ENB/ReShade ownership.

## Verification status and next evidence

- **Source verified:** the five behaviors above against current checkout.
- **Synthetic probe:** packet states 13 GCC/Clang policy checks; they reproduce existing decisions, not corrected behavior. Local packet-integrity and predicate comparison results are recorded in the implementation status.
- **Windows/GPU fix test:** NOT RUN; no source repair has been made from this packet.
- **Skyrim visual fix:** NOT RUN. User-started 0.1.102 run is first needed for the pending cursor replay. A later bounded title/save trace must record publication reason, effective UI draw RTV/viewport, writable DSV, logical/movie dimensions, and native pixels before/after any late copy. Do not infer native resolution from an output texture's extent alone.
