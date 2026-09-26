# Inventory/hero reference audit 19, 2026-09-26

The user supplied `C:/Users/user/Downloads/PureDark_Inventory_Hero_RE_19.zip`
(SHA-256 `81c934fdfc019e5eeaf05ee2c4776bffc9559593b46cad58e44bb4303183cdb0`).
It is reference evidence, not an implementation instruction. The archive's
`REPORT.md`, `NEXT_GHIDRA_CAPSTONE.md`, disassembly excerpts, and JSON findings
remain in the supplied archive; none are staged here.

The archive names the same `SkyrimUpscaler.dll` already audited locally,
SHA-256 `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
All ten byte-span hashes in its `evidence/spans.json` match independently
mapped bytes in that exact local PE file. These checks authenticate the
reported windows, not every interpretation in the report.

Independent Capstone 5.0.7 complete bounded decodes of RVA
`0x201440..0x2015ef` and `0x2012c0..0x20143f` confirm the two most relevant
new claims:

- The late PS hook compares the incoming SRV resource with host `+0x228`
  original depth and selects host `+0x388` only on that match. Ghidra 12.1.3
  decompilation of the exact PS function corroborates this; it provides no
  generic colour-SRV substitution.
- The late viewport hook compares a single viewport against host render
  width/height at `+0x2c/+0x30`, then substitutes display dimensions from
  `+0x24/+0x28`. No render-target identity check is visible in this hook.

The archive's late OM evidence is consistent with our prior Ghidra/Capstone
motion-attachment audit: known main-scene bindings get native replacements;
an unknown RTV0 falls through to the original OM call. The reference thus
supports keeping unknown offscreen RTV/SRV identities intact. It does not
prove which observed RazKolbas target is the inventory colour image or hero
preview. Format and RTV slot alone cannot establish that identity, even
though the observed `R16G16_FLOAT` RTV1 is consistent with motion.

No code or installed payload was changed from this report. The installed
0.1.94 read-only trace is the next evidence needed: correlate offscreen OM
target/depth and viewport with a later PS read and return to the native main
target. The actual inventory/hero resource identities, private depth, and
whether a full-render viewport mapper touches them remain runtime unknowns.

Local cross-check output remains ignored at
`artifacts/local/inventory-re19-ps-capstone.json`,
`artifacts/local/inventory-re19-vp-full-capstone.json`, and
`artifacts/local/inventory-re19-ghidra-ps-20260926.log`.
