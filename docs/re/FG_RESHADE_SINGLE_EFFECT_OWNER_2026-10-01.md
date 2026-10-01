# Private FG route: one ReShade effect owner

## Finding

The exact ENB 0.505/ReShade 6.8 private FG-Off reproduction was applying the
user's three-technique preset twice to a real synthetic frame. The upper
D3D11 and lower D3D12 ReShade runtimes both executed techniques. The D3D12
runtime belongs to a separate two-buffer native swap on the same adapter,
with a different D3D12 device from RazKolbas's three-buffer copy surface.
Readback of the latter therefore missed the final effect output. ReShade
6.8's public `effect_runtime::capture_screenshot` at its `reshade_present`
event supplied the final output instead.

Four quadrant midtones were submitted through 240 Presents with a resize at
frame 120. At post-resize frame 220, the three unobstructed RGB samples were:

| Active preset stages | Top-right | Bottom-left | Bottom-right |
| --- | --- | --- | --- |
| Neither | 150,130,100 | 80,100,70 | 180,170,160 |
| D3D11 only | 156,112,71 | 61,82,50 | 184,169,157 |
| D3D12 only | 156,112,71 | 61,82,50 | 184,169,157 |
| Both | 165,88,33 | 44,63,34 | 190,167,151 |
| Shared effect-owner code, D3D11 only | 156,112,70 | 61,81,50 | 183,168,157 |

The top-left sample was excluded because a wrapper overlay contaminated it
in some control runs. Values vary by roughly one channel unit across runs.
The D3D11 and D3D12 one-stage controls produced nearly the same final
colour; the two-stage run changed it a second time. This supports the
duplicate-effect explanation for the user's yellow image, but does not
measure the game's image or the reported 50-versus-60 FPS gap. One isolated
preset-change run access-violated during shader compilation; the restored
copy and a repeat of that control completed. No live preset was modified.

## Correction and bounds

`FgReShadeEffectOwner` uses the exact SHA-256-gated ReShade 6.8 add-on ABI.
It registers before private lower-swap preparation, matches the lower D3D12
runtime by selected adapter LUID, HWND, initial extent, two-buffer native
swap and creation thread, then disables effects only there. The D3D11
runtime retains the user's preset. If registration or initial suppression
fails, the private route is not published and native creation continues.
The owner remains with the route through resize and restores its prior state
on an early rollback when the runtime remains live. The copied INI and
preset SHA-256 values match the originals after the tests.

The official ReShade 6.8 `runtime_api.cpp` setter only assigns the in-memory
effects flag. ReShade's `reshade_begin_effects` callback occurs before the
per-technique enabled check. The owner reasserts the lower runtime's Off
state at that callback, so a direct API re-enable does not render a duplicate
technique. The controlled probe deliberately re-enabled D3D12 at frame 100;
the callback reasserted Off, with zero D3D12 techniques and zero dual-effect
frames. This does not validate arbitrary third-party add-on lifetimes.

## Verification

- Exact copied ENB/ReShade/Steam colour probe: 240 Presents and 192x108
  resize passed without and with Steam. Lower D3D12 runtime: zero technique
  callbacks in both epochs; upper D3D11 runtime: active techniques in both.
  Final D3D12 screenshot capture succeeded in both epochs.
- Separate production-style add-on registration: 240 Presents and resize
  passed without and with Steam; two lower-runtime initializations were
  suppressed. An intentional early failure returned its expected code 42
  after resource cleanup instead of crashing.
- Full Release build and CTest: 65/65 passed. Full Debug build and CTest:
  60/60 passed. The 0.1.145 Release and Debug plugin rebuilds passed after
  the version change.
- Installed 0.1.144 remained FG Off with the private route Off throughout
  the offline investigation. Skyrim colour, UI, loading picture and FPS with
  this correction: **NOT RUN**. Actual game FG On: **NOT RUN**.

## Controlled game trial staged

With Skyrim and its loader absent, `tools/Stage-MO2.ps1` produced ignored
`artifacts/local/fg-single-owner-2026-10-01/stage-0145-private-on`. Its
14/14 manifest entries passed SHA-256 verification. A byte-preserving copy
of the installed INI changed only `ProbeFgPrivateSwapOff` from `false` to
`true ` (five differing bytes); FG remains Off, NR Off, and the brighter
reduced-loading route On. The three user-facing files replaced in the MO2
mod were backed up under ignored
`artifacts/local/fg-single-owner-2026-10-01/backup-before-0145`.
The installed 0.1.145 DLL, INI and manifest hashes are respectively:

- `3e77e5a531a6c44bb2ca232cde81aa9fb7e5f276a8b2ffd209058969e1e768bb`
- `d4ad2750d9dcfe2d5e9d1ead50c917a14a707a2a24561fa3258c756d8bfdb51e`
- `1a8015f35d2c33a883755782698c74ac2dfdce34cf026b60660fa16e6765bbbe`

Installed payload verification passed 14/14. The pinned SR, NR and FG
runtime DLLs were not replaced because their installed hashes already match
the newly staged package. **Skyrim 0.1.145 FG-Off private-route visuals,
loading and FPS: NOT RUN. Game FG-On: NOT RUN.** Next action is a user-started
MO2 save-load and same-view colour/FPS check, followed by one fast travel.

Raw outputs, copied ReShade files and crash evidence are ignored under
`artifacts/local/fg-effects-2026-10-01`. The canonical effect/event contracts
are in [ReShade 6.8 API](https://github.com/crosire/reshade/blob/v6.8.0/include/reshade_api.hpp),
[events](https://github.com/crosire/reshade/blob/v6.8.0/include/reshade_events.hpp),
and [runtime API implementation](https://github.com/crosire/reshade/blob/v6.8.0/source/runtime_api.cpp).

## User-started Skyrim trial

The user started Skyrim through the test setup on 2026-10-01 at 08:47:07 and
reported that the initial image "seems fine". Fresh `skse64.log` records
RazKolbas 0.1.145 loaded correctly. At 08:48:02 the live plugin log records
one lower D3D12 ReShade runtime suppressed, the D3D11 preset retained, and
the FG-Off private lower/facade returned to ReShade. The observed Present
#149400 at 09:12:51 reports `failed=0`; owned world submissions are active.
This is a live route/presentation observation, not a matched colour or FPS
measurement. Same-view steady FPS and fast-travel loading artwork were
pending at this first checkpoint. Actual game FG-On remains **NOT RUN**. The ignored log
snapshot is `artifacts/local/fg-single-owner-2026-10-01/runtime-0145/RazKolbas.log`
(23,561,443 bytes; SHA-256
`83b1791771a5e11dffc0636c660410c090d43e4fcf7a374f250c5d000a7bbfa3`).

The completed trial report is **pixelated loading artwork** and roughly
**10 FPS below the prior 60-FPS same-view baseline**. The user's initial
"seems fine" image report is qualitative; no matched game screenshot or
pixel comparison proves exact colour parity. The final closed-session log
reaches Present #256800 at 09:42:15 with `failed=0` and 145,809 owned DLSS
submissions; its ignored snapshot is `runtime-0145/RazKolbas-final.log`
(23,690,973 bytes; SHA-256
`d1dcfe1bc534b7af0cc707e445b76beff6a170e2b19b6e05c5e239c747d0df51`).
Successful Presents do not explain the ten-FPS private-route cost. Loading
pixelation is expected from the active 1485x835 spatial loading workaround;
it is not a full-resolution repair.

After the process exited, only the installed INI private-route flag was
restored Off, with the same 0.1.145 DLL and FG Off. The installed 14-file
manifest verifies **14/14**. INI SHA-256 is
`9246b486cc214a0fc1c288e64c17835ee4d499609b8dbf71b4006a9b41914869`;
manifest SHA-256 is
`31ea5707019dda3a073ab05ac48ffd7371c0d8c3879d5494033bf6c1f9c3bc57`.
The previous trial INI/manifest are in ignored `backup-after-0145-trial`.
This rollback has **NOT RUN** in Skyrim; a return to 60 FPS is not claimed.
The private-route cost and full-resolution loading remain open. Actual game
FG-On remains **NOT RUN**.
