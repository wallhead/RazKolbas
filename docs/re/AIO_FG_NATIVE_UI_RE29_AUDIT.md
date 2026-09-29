# AIO FG native UI RE29: local audit, 2026-09-29

The supplied `C:/Users/user/Downloads/AIO_FG_Native_UI_RE_29.zip` is reference
evidence, not implementation instructions. Its ZIP SHA-256 is
`ad8184999f2c148278f68b51d2f99fd8e43ec1748957c3ef7ca5401d040baf22`.
An independent read-only check matched the declared lengths and SHA-256 values
of all 144 files in its manifest, with no missing, extra or changed member.
The pristine AIO binaries in this repository match its exact module identities:

| Module | Size | SHA-256 |
|---|---:|---|
| `SkyrimUpscaler.dll` | 14,211,584 | `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb` |
| `PDPerfPlugin.dll` | 962,048 | `8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d` |

Using the DLL PE section table to map RVAs to file offsets, an independent
comparison matched all 97 listed instruction-byte anchors (60 host, 37
PDPerf). This confirms the bytes and binary identity, not the report's full
control-flow interpretation or live Skyrim behavior. The archive's CPU tests
and shader arithmetic fixtures were not rerun here; its own verification
explicitly reports no Windows/GPU/Skyrim test.

## Implementation-relevant findings

- The reference's direct route binds and transparent-clears a separate
  display-sized UI target at host `+0x438`. It later copies that target to
  exported UI `+0x3E0`. The detection route writes different content to the
  same export: composite RGB plus binary alpha generated from a difference
  shader. These two routes must not be treated as equivalent proof of a
  transparent UI producer.
- The reference's final direct UI draw uses RGB `ONE` and `INV_SRC_ALPHA`,
  compatible with premultiplied `U + (1-a)B`. That blend state alone does not
  prove the engine's actual UI pixels have suitable alpha, or that B was
  captured after the same effects as the displayed F. A difference-derived
  opaque mask can also yield zero recomposition residual while failing on
  generated frames with a changed background.
- PDPerf allocates separate companions for optional UI and HUD-less inputs and
  explicitly copies each into them. A SHARED source does not make the
  companion a live alias. The two copy flags reset after its lower Present.
  The reported local CPU experiment shows stale companion content when a
  source changes after an eligible copy; it does not prove staleness in the
  complete live AIO schedule.
- Main/Loading and Magic take different export paths in the report's
  controlled predicates. It does not establish Inventory draw behavior.
  ReShade refresh of the background is conditional on its selected effect
  route and, for the FG HUD-less resource, on a specific alias. ENB ordering
  remains a live trace question.

RazKolbas already uses an owned D3D11-to-D3D12 input lease and exact source,
generation, reset epoch, token, source texture and copy-ticket checks in
`FgInputLeaseRing` and `FgPreparedSubmission`. `FgUiPlanes` snapshots a
separate alpha-capable UI input but has no game caller or verified producer.
It cannot claim genuine UI alpha just because a texture has an alpha channel.
No private AIO offset or difference shader should be transplanted into that
contract. No FG-On Skyrim path was enabled from this archive.

The armed V5.4 one-shot capture remains the next executable evidence step:
user-started Skyrim, load save, then Inventory and Magic. It can locate the
native draw interval and compare prepared/raw/final pixels, but it cannot
create U. After that result, trace the actual RazKolbas render target and
typed RTV/DSV, viewport/scissor enablement, blend/stencil state and last
writer at the first and last UI draws. Only then implement a distinct
transparent UI destination and copy it after its final write, binding B/U/F
to one real-frame token and input-consumption lifetime. Camera production and
typed Streamline constants remain independent FG blockers. Skyrim test for
this archive: **NOT RUN**; installed V5.4 plugin and INI: unchanged by this
audit.
