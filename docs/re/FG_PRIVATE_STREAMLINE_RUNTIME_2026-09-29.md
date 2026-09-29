# Private Streamline 2.14.1 loader and factory upgrade probe

The exact local SDK's six x64 runtime DLLs are verified by SHA-256 before
loading `sl.interposer.dll` from a private directory. The loader resolves
the core exports dynamically, initializes DLSS-G/Reflex/PCL with manual
hooking and D3D12 preferences, and owns explicit shutdown. The optional MO2
stage path copies those six files only after hash and NVIDIA Authenticode
checks. The staged runtime remains **disabled in the game**: `RazKolbas.dll`
does not yet call this loader or submit FG frames.

Separate-process `RazKolbasFgRuntimeProbe` results against the privately
staged DLLs and the exact ReShade 6.8 DLL:

| Case | Observed result |
|---|---|
| Private initialize / shutdown | Both succeed. |
| One-byte mutation of staged `sl.pcl.dll` | Initialization refused before loading: pinned file identity differs. |
| ReShade factory made before `slInit` | `slSetD3DDevice` and `slUpgradeInterface` return success; upgraded factory is distinct, its method belongs to the hash-verified `sl.interposer.dll` (slot 10 RVA `0x289e0`); ReShade's existing delegate stays System32 `dxgi.dll` RVA `0x67c90`; clean shutdown. |
| ReShade factory made after `slInit` | Same result and clean shutdown. |

The first probe mistakenly inspected only ReShade's unchanged delegate and
treated that as upgrade failure. The corrected probe inspects the returned
factory proxy as well. The ordering experiments do **not** establish that
Streamline must initialize before ReShade creates its factory. They establish
that `slUpgradeInterface` returns a new interface pointer rather than
rewriting ReShade's stored delegate. The next integration must explicitly
route the swap creation through that returned proxy while preserving
ReShade's outer facade and ENB's D3D11-facing contract. A success result from
`slUpgradeInterface` alone is insufficient evidence for this routing.

Release CTest: 60/60. This probe is outside Skyrim and does not establish
game stability, generated frames, presentation cadence, or UI separation.
