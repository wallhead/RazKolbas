# FG vs AIO review 26: source audit and disposition

The supplied `RazKolbas_FG_vs_AIO_Review_26.zip` (SHA-256
`10abef02a572105318ff6f4d704393e683324b15b5824c00c33297e33bcbf346`)
reviewed commit `404fc38`. The archive remains outside Git. Its synthetic
witnesses reproduce source behaviors; they are not game or GPU results.
Reference RE25's private RVAs are evidence from the reference modules, not
Skyrim addresses or a wire contract to copy verbatim.

## Corrected in source

- F1: `slGetNativeInterface` in public Streamline v2.14.1 AddRefs both
  proxy-backed and ordinary native returns. The standalone probe now scopes
  each returned pointer, including the swap query. The original creation
  pointer remains separately owned while `slUpgradeInterface` constructs its
  proxy. Five fresh-process FG-Off runs returned successful swap creation,
  TEST/real Present, resize and shutdown. This is not a full live-object leak
  report or an FG-On result.
- F2: `GetCompletedValue()==UINT64_MAX` and failed device health cannot be
  accepted as copy completion. The bridge exposes pending, complete,
  removed-device and unknown statuses; wait rechecks status after the event.
  The lease ring refuses preparation/submission/generation advance on a lost
  bridge. Pure retirement progress also rejects the removal sentinel in all
  five domains. A regression test covers the documented sentinel; forced
  physical device removal was NOT RUN.
- F3: the D3D11 producer writes one shared fence, and the D3D12 queue now
  writes a separate single-writer completion fence after waiting on the
  producer. A WARP test blocks the D3D12 queue, completes two D3D11 producer
  tickets, and confirms the first consumer ticket remains incomplete until
  the queue is released. No provider consumes these textures yet.
- F6: DXGI TEST Present forwards exactly one test call with per-call
  generation suppressed. It does not consume a source ID or cycle persistent
  On/Off state. Off/drain remains mandatory for real transitions and resize.

## Still blocking FG On

- F4: no immutable record yet binds the five resource leases, real source ID,
  Streamline frame token/viewport, actual camera constants, queue submissions
  and the **previously presented** input-consumption fence. Numeric
  retirement fields alone cannot prove this association.
- F5: a partially queued copy stops ring reuse, but the ring has no explicit
  owning stop/drain/quarantine lifetime. Its destructor may still release
  resources with work in flight. It must not be embedded in a live FG owner
  until this lifetime is implemented and failure-injection tested.
- F7: Streamline's `numFramesActuallyPresented` is interval telemetry, not a
  generated-frame count attributable to the current source. The current
  backend's per-call count is only from native fallback or test mocks; no
  Streamline provider maps this field yet.
- F8: actual legal tag resource states, motion normalization, camera/jitter
  provenance, Reflex markers and the D3D11-facing D3D12 owner under
  ENB/ReShade remain unbound. The v2.14.1 frame-based path requires
  `slSetTagForFrame`; AIO's observed state literals are not assumed valid for
  RazKolbas's SRV-only shared textures.

The installed V5.4 0.1.115 DLL is the read-only swap-owner preflight from
before this audit. This source work does not enable FG in Skyrim and did not
replace the installed DLL. The next game evidence is still the nested/outer
swap COM trace from a user-started title-screen session. A standalone FG-On
experiment needs coherent guides/camera and a real provider retirement path;
success codes alone will not count as generated output.
