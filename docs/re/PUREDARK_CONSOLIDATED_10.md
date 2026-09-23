# PureDark Consolidated Verified 10 review

## Provenance and integrity

Reviewed the user-supplied directory
`C:/Users/user/Downloads/PureDark_Consolidated_Verified_10/PureDark_Consolidated_Verified_10`
as reverse-engineering evidence, not as executable instructions or redistributable
product input. The bundle identifies the original host SHA-256 as
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`,
the PDPerf backend SHA-256 as
`8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d`,
and the reviewed RazKolbas source as commit
`ce2f55fbc960d7f433338405defd776058f34040`.

The bundle contains reports, source excerpts, disassembly, harnesses and result
records. It contains no original DLL, vendor runtime or compiled probe. Its
source provenance records the whole-file Git blob IDs and method-excerpt
SHA-256 `4b1b0f44a142aec85cfc51c8cda2ffa943159b1859289a4f427870d720d949ac`;
the retained excerpt matches that digest.

The bundled integrity checker examined all 2,034 manifest entries but reports a
false failure on Windows because it compares manifest `/` separators with
`Path.relative_to()` `\` separators. An independent normalized-path verifier
checked the same 2,034 entries with zero hash errors, zero missing files and
zero unlisted files. This is a checker portability defect, not a payload
integrity failure.

## Findings confirmed against the current source

1. `SdrDlssPresenter` advanced its last-frame marker before checking GPU
   completion and evaluation success. A failed first evaluation could therefore
   make the next accepted frame use `reset=false` despite there being no prior
   successful temporal submission.
2. Forward frame gaps and a change in source/publication phase were absent from
   the history-reset contract.
3. The owned spatial-fallback branch did not request a temporal reset.
4. Menu-boundary routing reused an admission gate warmed at pre-Present and also
   required an earlier successful DLSS submission. That mixed a producer-phase
   change with the spatial/temporal comparison.
5. The exact ENB `PSSetShaderResources` hash/RVA/prologue profile existed, but
   the installed UI hook covered only OM targets and viewports. The candidate
   therefore had no evidence for the reference's separate sampled late-depth
   consumer.
6. The current three-slot pool still waits for GPU completion during evaluation.
   Slot count alone is not evidence of asynchronous evaluation.

## Implemented source corrections

- Temporal history now distinguishes attempted, successfully evaluated and
  successfully published frames. Failure, a skipped frame, a forward frame gap
  or a pre-Present/menu source-phase change forces reset on the next evaluation.
- `SrFrameMetadata` now carries an explicit `SrSourcePhase` receipt.
- Owned spatial fallback requests a reset before the next temporal submission.
- Menu publication requires two independently sampled menu-boundary colour and
  depth receipts. It no longer depends on a prior successful DLSS submission,
  so spatial fallback and DLSS are compared at the same publication boundary.
- The exact verified ENB PS-resource slot is now installed as a read-only,
  pass-through observer. The first bounded menu-to-Present traces report exact
  singleton reads of the original depth resource and their start slot. No SRV
  substitution is enabled in this build.

The new WARP tests reproduce the failed-first-attempt history defect, frame-gap
reset and source-phase reset. They also prove that the PS observer recognizes
the exact underlying depth resource while forwarding the original SRV unchanged.

## Reference contracts retained as evidence

The original host keeps distinct roles for original scene depth (`host+0x228`),
writable late depth (`host+0x330`, DSV `+0x350`), sampled late depth
(`host+0x388`, SRV `+0x3A0`) and late motion (`host+0x2D8`, RTV `+0x2E8`). Its
PS replacement is narrowly scoped to published/late phase, one non-null incoming
SRV, and exact original-depth resource identity while preserving start slot and
count. These offsets describe the reference object only and are not copied into
RazKolbas.

The current candidate has a display-sized writable depth companion. It does not
yet have a separate sampled-depth texture/SRV or verified live evidence that the
observed Skyrim/ENB call matches the reference consumer. The pass-through trace
is the next runtime discriminator. If it confirms the exact singleton original
depth read, the next patch can allocate and clear a separate display-sized
sampled-depth resource outside the callback and substitute only that witnessed
call.

The bundle's synthetic/static checks do not establish live Skyrim output,
visual parity, performance, resize recovery, or GPU fault behavior. They also
show that early creation order alone is insufficient: allocation must return an
explicit compatible-resource receipt with rollback on failure. That broader
creation/lifetime work remains separate from the history and read-side trace
implemented here.
