# GPU-aware Neural Rendering runtime selection

The source change began from commit `26b9ac8` on `codex/razkolbas-bootstrap`.
It changes NR runtime selection and loading only. The D3D12 NR evaluation,
pre-SR/DLAA insertion, guide formats, fences, recovery and UI route remain
the same. The user's report confirms the installed 0.1.106 UI was visually
fixed; subsequent 0.1.107 Skyrim runs are recorded below.

`NrStage` starts from Skyrim's actual rendering `ID3D11Device`, obtains its
`IDXGIAdapter` and exact DXGI vendor/device/subsystem/LUID, rejects software,
AMD, Intel and unknown GPUs, and verifies the NR D3D12 device has the same
LUID. Hardware classification uses 65 exact desktop GeForce RTX PCI IDs
from this host's NVIDIA `nv_dispi.inf`, SHA-256
`1ca553b7c9518e953b0ac35950750b4dbcd64a4c5c48c7638c49405f499a9dfb`.
This is a reviewed PCI-ID fallback, not a marketing-name or compute-capability
heuristic. NVIDIA's [GPU compute-capability table](https://developer.nvidia.com/cuda/gpus)
confirms the family mapping, but SM version alone does not distinguish a
GeForce from a workstation/non-RTX device. Mobile and future IDs absent from
this desktop INF fail closed. No CUDA toolkit or CUDA context is needed.

The trusted catalog is compiled into `src/core/NrRuntimeSelection.cpp`.
Profiles have exact size/hash, a controlled relative path under
`SKSE/Plugins/RazKolbasRuntime`, a D3D12 host-contract ID, family mask,
validation status, priority and caller-identity policy. The old installed
`RazKolbasRuntime/nvngx_dlssnr.dll` location remains the legacy profile.
Selection uses `NeuralRendering.RuntimeProfile=Auto` and
`NeuralRendering.AllowExperimentalRuntime=false` by default. Auto chooses
only validated, installed, exact-matching artifacts. An experimental profile
requires its explicit ID plus `AllowExperimentalRuntime=true`; an invalid
override returns an NR-off reason and never silently falls back. Settings
changes to these fields require restarting Skyrim. One NR DLL identity is
latched per process. SR/DLAA continues when NR selection or evaluation fails.

| Profile | Artifact SHA-256 | Size | Candidate family | Auto scope | Caller-name shim |
| --- | --- | ---: | --- | --- | --- |
| `legacy-fastfp16` | `91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7` | 165,840,496 | RTX 40 | RTX 4080 SUPER PCI `2702` only, based on prior game and current harness results | Yes |
| `plain-fp16-20-30` | `6dac1b40f0c87af84a8177b18c741e84fb0c914f204c9d87d95916b665ba3af8` | 309,671,536 | RTX 20/30 | None; experimental | Yes, provisional and not hardware-tested |
| `ada-fastfp16` | `e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a` | 165,840,496 | RTX 40 | None; DLAA run passed on RTX 4080 SUPER, DLSS SR pending | Yes; required for the RTX 4080 SUPER standalone harness |
| `nvidia-50` | `e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e` | 165,840,496 | RTX 50 | None; experimental | No; signed original has not been tested with this host |

The 20/30 and 40 builds have `HashMismatch` Authenticode status with an
NVIDIA signer because their bytes were modified; the 50 build has a valid
NVIDIA signature. All three are x64 and expose the five D3D12 NR entry
points used by this host, but those static facts do not prove runtime ABI or
GPU output. The 40-series standalone 30-frame harness on this machine's
RTX 4080 SUPER and driver 616.92 failed `Init_Ext` without the narrow
caller-name shim and passed with it, producing nonconstant changed colour
and clean stage retirement. The legacy profile passed the same 30-frame
harness. RTX 20/30 and RTX 50 hardware tests: **NOT RUN**. The RTX 40
Skyrim DLAA run passed on this RTX 4080 SUPER; DLSS SR and dedicated
ENB/ReShade parity checks are **NOT RUN**. Full-image comparison in the harness
gave equal output hashes for the legacy and 40 builds on its synthetic input;
that is not a performance or visual-equivalence result.

The loader checks the controlled profile path, size and hash before selecting,
opens the chosen file with an exclusive write/delete-denying read lease,
rehashes the held handle, refuses a pre-existing NR module, loads by absolute
path with DLL-directory and System32 dependency search, then confirms the
mapped module is the locked file by volume/file ID. It retains the driver
core module reference used for NGX parameter exports. On uncertain GPU
retirement or failed vendor release/shutdown/shim restoration, it retains
module/resources until process exit rather than risking use after free.

`tools/Stage-MO2.ps1` accepts explicit `-NrRuntimeProfiles` source paths and
verifies exact identities before placing optional DLLs side by side. It does
not discover/download DLLs. The original `-NvidiaNrRuntime` legacy argument
still works. A local package with all four NR profiles was staged under
ignored `artifacts/local`; all eight manifest payload hashes were checked.
The new plugin and three candidate NR profiles were installed into the V5.4
MO2 mod while Skyrim was closed. The existing INI, SR DLL, legacy NR DLL,
license and `meta.ini` were preserved. All eight installed manifest payload
hashes match. The previous plugin and manifest are backed up under ignored
`artifacts/local/v54-0.1.107-install-backup`. Installed plugin SHA-256:
`bccbeda2ae2fd2fff064e8b45b83bf1790f19fa535c6cd1ed05e7bc18726b05a`.
No supplied DLL is tracked by Git.

The End menu shows requested profile, experimental opt-in, active session
profile, stage, renderer PCI IDs/LUID and selection reason. Logs include the
profile path/hash and the first successful evaluation. Runtime selection
does not display “working” on mere DLL load or initialization; the displayed
stage distinguishes those from a submitted evaluation. In-game output
validation of each new candidate requires a user-started Skyrim run.

In the first 0.1.107 owner-started Skyrim run, DLAA + Auto selected the
legacy runtime on PCI `10de:2702` and submitted thousands of NR and DLAA
frames without an NR error. The owner reported normal image and UI, and the
End menu showed the legacy runtime active. This confirms the Auto/legacy
path. In a second owner-started DLAA run, the supplied RTX 40 runtime was
explicitly selected, its exact hash appeared in the log, and it reached
5,400 NR submissions without an NR error. The owner reported that it seemed
to work. The installed INI is now prepared for a DLSS Quality run with this
runtime; that combination has not yet been tested in Skyrim.

A second code review found two defects before final installation: a failed
vendor teardown could be retried by the destructor, and a pending profile
change could block live NR sliders. Teardown failure is now a terminal
process-lifetime quarantine, and live evaluation updates retain the active
profile while the requested profile remains saved for restart. The Release
build, 44 CTest groups and sequential 30-frame legacy/RTX-40 bridge runs
were repeated after these corrections.

The source's profile hashes and packager's identities must be updated
together when a new reviewed binary is added. A new GPU/profile should be
promoted to Auto only after exact hardware/driver and full Skyrim NR
validation, with evidence recorded here and in `docs/IMPLEMENTATION_STATUS.md`.
