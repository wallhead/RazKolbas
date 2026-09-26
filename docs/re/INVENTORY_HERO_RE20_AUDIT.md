# Inventory preview boundary audit, 2026-09-26

The user supplied `PureDark_Inventory_Hero_RE_20.zip` (SHA-256
`f8a6e507543bd7182036130c4eac64c407ab69de4f066747203b5c10a275bcf5`).
Its `REPORT.md`, `CODEX_HANDOFF.md`, and semantic landmarks are reference
claims, not implementation instructions. They were extracted only under
ignored `artifacts/local/inventory-re20-supplied/`; the manifest hashes were
verified. An independent subagent checked the critical game and AIO addresses
against the exact preserved binaries and decoded game text.

The exact Skyrim 1.6.1170 executable is SHA-256
`c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9`.
The saved decoded `.text` under ignored
`artifacts/local/skyrim-decoded-text-2026-09-20-1657/text_1000.bin` is
SHA-256 `75105f3ae0c7bcb7ece2ab5bc6b41ae1be062ccb8379ac9ea5e557eafe2a34f3`.
Address Library AE ID 215494 maps the `InventoryMenu` vtable to RVA
`0x18F9998`; its slot 6 (+`0x30`) points to `PostDisplay` at `0x92D260`.
That path tail-jumps to `Inventory3DManager::Render` at `0x927560`, and then
to a downstream function at `0x972590`. The common menu loop invokes slot 6
at `0xFA51D6`, returns at `0xFA51D9`, and reaches the deferred EndFrame call
at `0xFA51EA`. Thus the inventory preview work starts before RazKolbas's
deferred UI flush. This **does not** establish which RTV/DSV/viewport is bound
at the actual preview draws.

The exact AIO `SkyrimUpscaler.dll` is SHA-256
`94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
Its native UI transition callback at RVA `0x1572A0` hooks a game call at
`0x972D34`, inside the preview's downstream function. It forwards that
call at `0x1572AE` **before** binding native colour/depth and setting its
late flag. The relevant game call in the preserved decoded capture is
`E8 47 EE B0 00`, targeting `0x1481B80`; this is a static/saved identity,
not a verified current live patch site. The saved live manager prologue at
`0x927560` already starts with `FF 25 5F 9E 6B FF`, so a proposed pristine
prologue patch would overwrite another owner. No such patch was applied.

The supplied report's stronger claim that the hero necessarily paints the
RTV bound at manager entry is unproved. The AIO transition can occur inside
the manager before subsequent renderer calls near `0x972DE8` and `0x972E85`.
A useful live trace must record RTV0/RTV1, DSV, viewport, render-target
resource identities, menu identity and frame at the actual draw boundary,
including both sides of `0x972D34`. Any experimental call-site interception
requires an exact-version descriptor and startup ownership checks under
`docs/PATCHING_POLICY.md`; neither static addresses nor the supplied report
authorize a blind live patch.

RazKolbas 0.1.97's first one-frame capture occurred in a world/dialogue scene,
not `InventoryMenu`, because its motion-MRT trigger was too broad. The source
diagnostic now waits for `InventoryMenu` on the actual UI stack before
consuming that one-shot capture. This has not yet been built or run in Skyrim
at the time of this audit. The earlier world capture cannot decide why the
inventory is invisible.
