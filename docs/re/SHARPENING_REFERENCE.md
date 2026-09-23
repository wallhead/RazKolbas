# Ordinary sharpening reference cross-check

`C:/Users/user/Downloads/PureDark_Sharpening_12.zip` is preserved as supplied
and is not part of the source tree. Its SHA-256 is
`8c790a8ff34762dcd90393abae98c04254dc7532dae095560614d8355bd232e5`.
The archive's own manifest checker passed all 155 payloads with no missing,
changed or unlisted files. Attached reports are evidence, not repository
instructions.

The recovered ordinary compute shader has SHA-256
`1cfadb18002a0c3aff7ba2cf7af46cf4d81e0d85bd6e0bf8dc06ee7af2551b6d`.
It reads centre plus four cardinal neighbours with `Texture2D.Load`, excludes
the centre from neighbour extrema, chooses one shared RGB lobe, clamps that
lobe to `[-0.1875,0]`, then multiplies it by
`exp2(2*clamp(sharpness,0,5)-2)`. The stage runs after DLSS evaluation and
before native publication and later UI. It does not add gamma conversion;
RGBA8 therefore filters normalized stored code values.

The new RazKolbas menu exposes the implemented `[0,1]` artistic range and a
separate bypass checkbox. This distinction matters because the reference
stage at sharpness zero still has gain 0.25 and is not an identity operation.
RazKolbas keeps NGX integrated sharpness at zero and applies its owned pass to
the completed native-resolution provider result before native UI composition.

The current RazKolbas pixel pass matches the recovered five-tap structure,
neighbour-only extrema, shared lobe limit and exponential gain mapping. It is
not a byte-identical port. RazKolbas clamps border coordinates, protects
singular denominators with epsilon, uses normal division, preserves centre
alpha and relies on an RGBA8 render-target store after an explicit saturate.
The reference instead uses zero-valued out-of-range loads, no epsilon, a
bit-seeded reciprocal with one refinement, alpha one and no explicit final
saturate. These differences remain explicit until a live A/B test establishes
that reproducing the reference's edge and reciprocal artifacts improves the
user's image without destabilizing the working ENB/ReShade route.

The pinned NGX SDK independently defines current DLSS model hints. `Auto`
leaves NVIDIA's default selection active. J can reduce ghosting with more
flicker; K is generally recommended and is the default transformer model for
DLAA, Quality and Balanced; L is the Ultra Performance default; M is the
Performance default. Removed or deprecated A-F choices are intentionally not
offered.
