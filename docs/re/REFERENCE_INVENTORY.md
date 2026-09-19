# Local reference inventory — 2026-09-19

The user supplied extracted reference directories at the repository root. `pwsh -File tools/Inventory-References.ps1 -ReferenceRoot . -Verify` verified **57/57 extracted manifest entries**. Both original archive names were absent (`MISSING_REFERENCE`); exit 1 correctly reports that the full archive baseline is incomplete. No extracted file failed its recorded size/SHA-256.

The read-only inventory records sizes, SHA-256, PE versions/CodeView records and PDB GUID/age in ignored `artifacts/local/reference-inventory.json`. These are fresh checks against the inherited manifests, not a claim of runtime verification. It never loads a vendor DLL.

Supplied extracted directories remain pristine and ignored. Additional local files such as an IDA database are not treated as baseline evidence or staged. Experiments must create working copies under `reference/working/` or `artifacts/local/`.

MSVC 14.44.35207, Visual Studio Build Tools 2022 17.14.37216.2, CMake 3.31.6-msvc6 and Windows SDK 10.0.26100.0 are installed. CMake is under Visual Studio's bundled CMake directory. OS adapter inventory includes RTX 4080 SUPER and AMD Radeon Graphics; neither is claimed to be Skyrim's actual rendering adapter.

No Skyrim test installation has been selected. Game hooks, ENB ordering, NR ABI and all feature rendering remain unverified.
