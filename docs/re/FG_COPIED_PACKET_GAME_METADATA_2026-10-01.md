# FG copied packet metadata trial, 2026-10-01

The 0.1.154 Skyrim run completed eight FG-Off five-input copies, but source
review found a provider admission mismatch. `pairFgGameInputs` copied
`world.hudless`, a separate pre-UI snapshot, while
`prepareFgSubmission` requires the exact `FgUiPlaneFrame.hudless` COM object.
The two copies come from the same display in the MenuDisplay publication
block before UI drawing, but they have distinct resource identities. The
previous packet could never pass the final source-identity check.

Source 0.1.155 selects `ui.hudless` as the copied HUD-less input and rejects
aliases with the UI or final-colour texture. The world snapshot remains a
required phase witness for this diagnostic. The WARP test first failed in
two places against the old code: the fourth source pointer differed and the
real copied lease failed `prepareFgSubmission`. Both passed after the fix.
It also checks altered source identity and stale camera rejection.

`FgGameInputProbe::inspectPrepared` now checks the pending lease against the
candidate's five exact D3D11 source objects and validates its frame, native
UI and same-frame camera metadata using `prepareFgSubmission`. The live
FG-Off hook invokes it only for the existing bounded eight-frame copy trial
when a camera candidate exists. It uses the current native flip target's
already validated index and buffer count. The function does not wait for the
GPU copy, submit a Streamline token, establish lower-swap ownership or retire
provider inputs. Its log says metadata validation only. The installed FG
setting remains Off.

An independent subagent review found no FG-Off correctness blocker in this
change. It confirmed that the UI snapshot and world snapshot are captured
within the same pre-UI publication interval and recommended using the
already validated native target rather than querying a stored swap pointer;
that recommendation was implemented. Pixel equivalence of the two snapshots
was not measured, and this result does not validate a live generated image.

Release build/CTest passed **70/70** and Debug build/CTest passed **65/65**.
The 0.1.155 package was staged at ignored
`artifacts/local/stage-v54-fg-metadata-0155`, verified **14/14**, with the
user's INI byte-identical. After verifying Skyrim was closed and the prior
installed package was **14/14**, the old DLL/INI/manifest were backed up in
ignored `artifacts/local/backup-v54-before-0155`. Only the DLL and manifest
were replaced. The installed package again verified **14/14** and the INI
hash remained unchanged.

- 0.1.155 DLL SHA-256: `497577883715b26e33db98a8ad9d0063098d46659751bf297f5f7558bd11e4a3`
- INI SHA-256: `3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`
- Manifest SHA-256: `983a933724719c906a062fe35fce7306f593b80a8dad56957ca305e9e44d5815`

**0.1.155 Skyrim runtime metadata check: NOT RUN. FG-On/generated Skyrim
frames: NOT RUN.** The next user-started MO2 save load should report normal
image/UI/FPS and allow collection of the eight copied-packet metadata lines
and copy completions. A valid metadata line is not provider readiness; the
game-owned lower swap, verified Streamline phase markers, read transitions
and actual provider/Present/allocator retirement remain to be joined.
