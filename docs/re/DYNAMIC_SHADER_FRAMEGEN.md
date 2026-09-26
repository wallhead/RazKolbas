# DynamicShaderFrameGen source reference (2026-09-20)

User-supplied source: <https://github.com/jatelop8/DynamicShaderFrameGen>.
Read-only local clone under ignored `artifacts/local/DynamicShaderFrameGen-reference`
is pinned to commit `daaba8aadb2dbc8c5e52b028f12475c3450b6866`. This is an
open-source implementation reference, not a binary or runtime dependency for
RazKolbas. No source from it has been copied into this project. The source
identifies its license as GPL-3.0-or-later with its modding and linking
exceptions; any future reuse needs a deliberate license/provenance review.

## Directly observed architecture

- Its [README](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/README.md)
  describes a Skyrim 1.6.1170 SKSE plugin with FSR 3.1 FG, DLSS SR and optional
  ENB/ReShade. The implementation substitutes a D3D11 shared texture for the
  game's swap-chain backbuffer and presents through an owned D3D12 swap chain.
- The [swap-chain proxy](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/src/DX12SwapChain.cpp#L1022-L1041)
  returns that D3D11 shared texture from `GetBuffer(0)`. Its Present path
  copies the shared texture or DLSS output into the D3D12 backbuffer, with
  explicit D3D11/D3D12 fence ordering in the source. This is a concrete
  presentation-owner design; RazKolbas must not add a competing owner.
- The [render-time hooks](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/src/FrameGen.cpp#L243-L337)
  set Skyrim's dynamic-resolution ratios and Halton jitter in
  `Main_UpdateJitter`, adjust scissor coordinates, and disable vanilla DRS.
  The source names AE Address Library IDs 77245, 77365 and 36555 with
  addends. These are hypotheses for our exact-game profile, not verified
  RazKolbas patch addresses or authority to alter those sites blindly.
- An attempted [pre-UI route](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/src/FrameGen.cpp#L398-L459)
  evaluates from `kMAIN` and copies to `kFRAMEBUFFER`. Its own source comments
  report `kFRAMEBUFFER.texture == nullptr` in a Skyrim SE runtime, preventing
  that route. The active [Present route](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/src/DX12SwapChain.cpp#L657-L724)
  instead feeds DLSS from the shared backbuffer after the scene/ENB composite.
  Comments describe preserving ENB effects by doing so, but also acknowledge
  unresolved UI placement. Code and comments alone do not establish visual
  quality or compatibility in the user's modlist.
- The current README and [source](https://github.com/jatelop8/DynamicShaderFrameGen/blob/daaba8aadb2dbc8c5e52b028f12475c3450b6866/src/DX12SwapChain.cpp#L850-L859)
  route NR to external ReShade add-ons (the in-plugin path was removed). That
  cannot satisfy RazKolbas's independent, addon-free NR requirement.

## Use in the current investigation

The 0.1.17 user-run map found distinct post-world colour and pre-ENB-Present
backbuffer resources in this modlist. A saved renderer snapshot has a null
first render-target slot, consistent with the reference's reported failure
of its `kFRAMEBUFFER` route, although the exact slot name remains unverified
here. Its source comment identifying AE UI-entry RVA `0xfa3dc0` differs from
RazKolbas's independently decoded Address Library ID 82084 base RVA
`0xfa4f00`. Neither address is yet an established pre-UI interception point
for this product. Present-time replacement would risk including effects and
UI in the SR input. The reference's DRS/jitter and D3D11/D3D12 fence paths
will be revisited after tracing the exact game transition. The supplied
reference remains outside the product package; see `SR_REPLAY.md`.

## Upstream revision check, 2026-09-26

The user pointed out a new upstream revision. A read-only fetch into the
ignored reference clone resolved `origin/main` to
`879ab2c13404e02f352fa10515abd0fc25996d4f` (v1.52, committed
2026-09-24). The original pinned checkout remains at `daaba8a`; no upstream
source or binaries entered RazKolbas. The earlier architectural description
above is historical rather than the current upstream implementation.

- Upstream [commit `5c0dcfb`](https://github.com/jatelop8/DynamicShaderFrameGen/commit/5c0dcfbd30e754cd21b827503e66f4700b9c6a81)
  explicitly removed the DLSS SR path: `Main_PostProcessing`,
  `SetDLSSOptions`/`EvaluateDLSS`, its output texture and associated GUI
  controls. The current [`OnMenuDrawStart`](https://github.com/jatelop8/DynamicShaderFrameGen/blob/879ab2c13404e02f352fa10515abd0fc25996d4f/src/FrameGen.cpp#L507-L529)
  only logs conditions; it does not publish an upscaled scene before UI. A
  surviving `EnableUpscale` INI parser, logging and README claims do not
  re-establish a callable SR path. This code cannot be transplanted as a fix
  for RazKolbas's invisible inventory or downscaled title.
- The current [Present path](https://github.com/jatelop8/DynamicShaderFrameGen/blob/879ab2c13404e02f352fa10515abd0fc25996d4f/src/DX12SwapChain.cpp#L1740-L1867)
  copies the already composed swap-chain buffer into `colorOut`, optionally
  applies native NR to that image, then draws its own overlay and dispatches
  FG. The source itself says the overlay enters the DLSS-G input because a
  separate UI plane is not wired. This path is relevant to later NR/FG
  ownership, but it does not identify Skyrim's inventory preview target or
  supply a native UI handoff for our current SR route.
- The v1.52 [commit](https://github.com/jatelop8/DynamicShaderFrameGen/commit/879ab2c13404e02f352fa10515abd0fc25996d4f)
  adds switches to avoid competing CommunityShaders detours; the menu hook
  can be skipped because its callback has become a no-op. Later diagnostic
  code also compares the plugin's produced image, D3D12 back buffer and
  desktop image to localize an FG freeze. The transferable method is to
  measure pixels at each ownership boundary, with source, target and menu
  identity recorded in the same frame. It is not a validated patch address
  or a live result in the user's modlist.

The upstream README at this HEAD still advertises DLSS SR and an active
`Main_PostProcessing` hook; that conflicts with the code above. For the
RazKolbas investigation, the current source and removal commit take
precedence over those descriptive claims. NR/FG runtime success reported by
the upstream project is not a RazKolbas runtime result.
