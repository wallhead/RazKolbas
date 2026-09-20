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

Our 0.1.17 read-only target map should compare the game's world-colour
candidate, active OM targets and swap backbuffer. The reference's split
between `kMAIN` and the backbuffer makes these identities especially useful.
If they are different, a Present-time overwrite would operate after effects
and potentially UI; if they alias, timing still requires verification before
any display write. The reference's DRS/jitter and D3D11/D3D12 fence paths
will be revisited only after our live resource map and temporal contracts are
known. The supplied reference remains outside the product package.
