# Patch implementation boundary

T04 is partially implemented and tested:

- Exact image SHA-256 and section-bounded unique expected-byte matching.
- Zydis 4.1.1 instruction-boundary validation for equal-sized in-place replacements.
- Owned executable fixtures with exclusive execution gates, page protection restoration, cache flush, readback, patch and restore. Tests execute 1 → 2 → 1 and concurrent callers during 100 patch cycles.
- Atomic pointer-slot leases with original-pointer checks and ownership-aware rollback; later owners are not overwritten.
- Working-copy disk patch tool with exact original/result hashes, expected bytes, overlap rejection, backup and journal. Dry-run creates no files. Restore refuses an updated patched file. PE patches validate x64 PE structure before and after writing.

Not implemented: arbitrary Skyrim thread quiescence, engine detour/trampoline relocation and chaining, production patch registry/menu, full manifest schema enforcement, in-place deployment/rollback journal recovery. `OwnedCode` only proves quiescence for its own callers; its mutex must never be presented as stopping game threads. The current host installs no game rendering patches.

`patches/nr/caller-name.json` is one explicit standalone experimental IAT contract. It is enabled only by the probe's `--caller-shim` argument; the game plugin never reads or activates it.
