# AIO DLSS-G camera and token RE30 audit, 2026-09-29

The supplied `C:/Users/user/Downloads/AIO_DLSSG_Camera_Tokens_RE_30.zip`
is reference evidence, not an implementation directive. Its SHA-256 is
`3ffeb333593af0068a396ce6d64780c5cbd26cfd39f91539b28aaa43ca5373ce`.
An independent read-only manifest check matched all 121 declared member
lengths and SHA-256 values, with no missing or extra files. The cited
`SkyrimUpscaler.dll` and `PDPerfPlugin.dll` SHA-256 values match the pristine
local AIO binaries recorded in the RE29 audit. Mapping RVAs through the PE
section table, all 60 listed original instruction-byte anchors (10 host,
50 PDPerf) matched those binaries. This confirms byte identity, not every
semantic label. The archive's Linux CPU probes and matrix model were not
rerun here; they do not include a game, Windows vendor runtime or GPU test.

## Findings relevant to RazKolbas

- The named `SetCameraData` export is positively connected to a Latewarp
  owner. The report does not connect it to the RTTI-identified Streamline
  frame-generation method. Calling that setter cannot certify current
  DLSS-G camera constants.
- The selected reference builder emits row-major `clipToPrevClip` from four
  method matrices `C*A*B*D` and emits its inverse as `prevClipToClip`. The
  exact game-camera producers and proof that the inputs are unjittered remain
  unknown. Constructor values are not valid camera samples.
- The host jitter generator and its packet stores are traced, but the
  selected FG preparation method reads different method fields. The last
  writer of those fields is still missing. The declared motion/depth flags
  and normalized scale are not evidence that the actual guide texture has
  those semantics.
- The reference host's source number is a 32-bit packet field; the adjacent
  byte is a separate flag. Its Streamline frame counter advances
  independently. The retained marker token and local constants token can
  have different pointer values while naming the same explicit frame index.
- Controlled CPU schedules can submit an old prepared constants slot under
  a new token, continue after failed token allocation, or mark a failed
  constants submission as already submitted. These are possible path
  behaviors under constructed dependencies, **not observed Skyrim failures**.
- The reported Streamline completion value belongs to a previously
  presented input slot. It is not the current producer-copy fence and must
  retire the matching previous lease before that lease is overwritten.

`FgInputLeaseRing`, `FgPreparedSubmission` and `FgPresentLedger` already
distinguish real source, generation, reset epoch, present token, exact copied
resources and provider-input retirement. The synthetic Streamline FG-On
probe already aborts on missing token or failed constants/tag calls. RE30
exposed a narrower gap: the source-only `FgCameraData` could previously be
numerically valid but unassociated with the current real frame. It now has a
source/generation/token/reset stamp plus nonzero producer revision, all
checked against the prepared frame. The submission also rejects temporal
matrices whose row-major forward and reverse products are not approximately
identity. A noncommuting valid pair and stale camera stamps are covered by
offline tests. Debug and Release each passed 54/54 CTest groups after the
change. These guards cannot prove that the eventual Skyrim producer
sampled unjittered matrices or correctly encoded motion/depth.

No AIO offset, matrix default, token pointer identity or reset=false policy
was transplanted. This is source-only FG work and changes neither the
installed 0.1.116 DLL nor its armed one-shot UI capture. Skyrim FG-On and
actual camera production remain **NOT RUN**. The next reference-camera
observation, if pursued, must identify writes to the RTTI-verified method's
camera fields across two real frames, including indirect/interior-pointer
writes, and correlate the emitted constants with engine projection jitter,
guide motion and the explicit Streamline frame index. The immediate game
task remains collecting the corrected native UI sequence from 0.1.116.
