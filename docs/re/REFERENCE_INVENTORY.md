# Local reference inventory — 2026-09-19

The user supplied extracted reference directories at the repository root. `pwsh -File tools/Inventory-References.ps1 -ReferenceRoot . -Verify` verified **57/57 extracted manifest entries**. Both original archive names were absent (`MISSING_REFERENCE`); exit 1 correctly reports that the full archive baseline is incomplete. No extracted file failed its recorded size/SHA-256.

The read-only inventory records sizes, SHA-256, PE versions/CodeView records and PDB GUID/age in ignored `artifacts/local/reference-inventory.json`. These are fresh checks against the inherited manifests, not a claim of runtime verification. It never loads a vendor DLL.

Supplied extracted directories remain pristine and ignored. Additional local files such as an IDA database are not treated as baseline evidence or staged. Experiments must create working copies under `reference/working/` or `artifacts/local/`.

MSVC 14.44.35207, Visual Studio Build Tools 2022 17.14.37216.2, CMake 3.31.6-msvc6 and Windows SDK 10.0.26100.0 are installed. CMake is under Visual Studio's bundled CMake directory. OS adapter inventory includes RTX 4080 SUPER and AMD Radeon Graphics; neither is claimed to be Skyrim's actual rendering adapter.

No Skyrim test installation has been selected. Game hooks, ENB ordering, NR ABI and all feature rendering remain unverified.

## Capstone decoder supplied 2026-09-23

The user supplied a local Capstone installation at
`C:/Program Files/capstone`, corresponding to the official
[capstone-engine/capstone](https://github.com/capstone-engine/capstone)
project. `cstool.exe -v` reports Capstone 6.0.0 with x86-64 support.

Local identities:

- `bin/cstool.exe`: SHA-256
  `df50bcecefa2e9d6464bb34bec5a72678dbb28927a104f1d527a8bc2b8d38e64`
- `bin/capstone.dll`: SHA-256
  `0282828751737eca6e0431ff3e4f5b3dd2661b46a3d5468f6e9795df747fe05a`
- `lib/capstone.lib`: SHA-256
  `172d18c0cb68ce0a7bb8f747762733bcf0a0d24a635ab4dd1462ffcf4cb09bf8`

As a smoke check, Capstone decoded the exact 16-byte verified ENB
PSSetShaderResources prologue as three stack-register saves followed by
`push rdi`, with instruction boundaries at offsets 0, 5, 10 and 15. This agrees
with the profile boundary already validated by the mapped-image hash/RVA check.
Capstone is available as a third decoder for future bounded RE comparisons; its
output alone does not replace exact file identity, relocation or live-owner
validation. No Capstone binaries or generated databases are staged in Git or
the MO2 package.

## User-supplied Capstone skill archive

`C:/Users/user/Downloads/capstone-disassembly-codex.zip` has SHA-256
`11a5745af760fa81524460c2d6be7afd358f6f81262c1384333dc54f84f9e118`.
All 14 archive entries use safe relative paths. The package was treated as
reference material rather than as instructions or automatic installation
authorization. Its helper `scripts/capstone_tool.py` has SHA-256
`c9515b4ccf0e84bc0f6d52f3acd926931a7e0b54de82d16a6b2f3f4cd62a8cff`.

Static inspection found a bounded read-only PE/raw-byte decoder. It opens input
files read-only and creates reports only through exclusive-new output mode. The
tests use subprocesses and temporary synthetic files but do not load analyzed
DLLs. The archive pins Python Capstone 5.0.9; the existing Windows Python 3.14.4
environment has binding 5.0.7/native API 5.0. The helper's doctor and native
smoke test passed, followed by 49 passing tests and one expected skip for the
installed-dependency diagnostic. This is stronger target-machine evidence than
the archive's original Linux report, where ten native tests were skipped.

The helper then decoded bounded 0x100-byte windows from the installed exact-hash
ENB wrapper (`D:/TESV_EX/d3d11.dll`, SHA-256
`47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58`)
at PS-resource RVA `0x5c730`, viewport RVA `0x5d070`, and OM-target RVA
`0x68f40`. The first decoded instruction boundaries and bytes match all three
existing `OwnedRouteProfile` prologues. Reports remain ignored under
`artifacts/local/capstone-enb-route-2026-09-23/`; they are bounded linear
decodes and do not prove reachability or runtime semantics. The skill itself was
not installed globally because the user supplied it for investigation without
an explicit installation request.
