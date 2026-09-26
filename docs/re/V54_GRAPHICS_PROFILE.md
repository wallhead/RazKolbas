# V5.4 graphics profile checkpoint (2026-09-25)

The `V5.4 NO-LORE` MO2 profile contains ENB 0.505 at
`mods/ENB TRUE AE V5.4/root/d3d11.dll` and ReShade 6.8.0 at
`mods/Reshade TRUE AE V5.4/root/dxgi.dll`. This is a different graphics
stack from the earlier `D:/TESV_EX` test install. The first user-started
V5.4 launch loaded RazKolbas 0.1.82, but its log reported `Unverified D3D11
owner; existing hook left untouched`; SKSE PostLoad then reported renderer
observation disabled. The End menu could not draw because the current menu
boundary is reached through that renderer observation and its outer swap
Present hook. The End key setting itself remained enabled in the installed
INI.

Exact static identities and observed PE contracts:

| Component | SHA-256 | File size | PE image size | Site |
|---|---|---:|---:|---|
| V5.4 ENB `d3d11.dll` | `35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3` | 4,553,216 | `0xa92000` | `D3D11CreateDeviceAndSwapChain` export RVA `0x5e4b0` |
| V5.4 ReShade `dxgi.dll` | `b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da` | 5,255,448 | `0x534000` | DXGI exports differ from the earlier ReShade 6.7.3 profile |

The ENB export starts with the same verified 16-byte prologue as the earlier
profile. The new ENB outer swap table is at RVA `0x18f858`; its Release,
Present and ResizeBuffers slots are 2, 8 and 13, pointing to RVAs `0x6d420`,
`0x6c540` and `0x6c600`. Their entry bytes and the table entries were read
from the exact file. An opt-in test mapped this file without executing it and
passed the existing bounded swap-table validator (31 assertions). The
0.1.83 candidate adds only these exact ENB creation and outer Present profiles.
All other module identities still fail closed.

ReShade 6.8.0 has a candidate nested swap table at `0x3ee960`, but its owned
scene, factory and GetBuffer/GetDesc routing contracts have **not** been
ported or tested. ENB UI context tables and ENB caller-return sites also
still target the earlier binary. Consequently 0.1.83 is a **menu restoration
candidate**, not a claim of DLSS SR or NR in V5.4. The next user-started game
run must first establish creation, outer swap provenance, Present counts and
End-menu visibility. Only then should the separate owned scene contracts be
mapped and tested for this ENB/ReShade pair.

## Subsequent runtime evidence and 0.1.84 source candidate

User-started 0.1.83 runs on 2026-09-26 established that the End menu opens
and `Quality=Quality` is read from the INI at startup. World buffers and the
display stayed at 2560x1440 because no early owned reduced scene was routed.
The ReShade 6.8 factory table was observed live at RVA `0x3ee350`, with
`CreateSwapChain` at RVA `0x14a4f0`. One loaded-world interval submitted more
than 11,000 native-size DLAA frames and logged pre-SR NR submissions. A later
run remained in depth-rejected native fallback. Neither is reduced DLSS SR.

The 0.1.84 source candidate adds separate hash-checked ReShade 6.8 factory
and nested swap profiles, ENB 0.505 context slots, and version-specific ENB
GetBuffer/GetDesc caller-return sites. Static mapped-image tests against the
exact local binaries passed 81 assertions; the new ReShade swap-table audit
passed 39 assertions. These are offline binary-contract results. Reduced
scene publication, ENB/native-UI composition, DLSS SR output and stability
on V5.4 remain **NOT RUN** pending a user-started game test.

## 0.1.84 loaded-world and UI-boundary evidence

The user-started 0.1.84 game run on 2026-09-26 proved a 1707x960 owned scene
feeding DLSS and 2560x1440 display output, with thousands of continuous SR
and pre-SR NR submissions. The user reported that UI blurs while moving.
The same-frame prepared-colour capture includes HUD bars and text before SR.

The first 12 read-only menu-to-Present traces each recorded **nine** events:
four render-target/viewport pairs, then one additional singleton bind of the
same reduced scene target with the same reduced depth. The earlier exact UI
contract admits only eight events, so `observationContractFault_` prevented
display-sized companion allocation and `latePassRoutingAvailable()` remained
false. Consequently the frame was published at pre-Present, after HUD had
already entered the reduced input. The 0.1.85 source candidate adds a separate
ENB 0.505 observation layout requiring that ninth bind with the same scene,
depth identity and reduced dimensions. The older ENB contract still requires
exactly eight events. WARP integration covers both layouts. The 0.1.85 DLL
is installed in the V5.4 MO2 mod. **Runtime visual effect: NOT RUN** until
Skyrim is started by the user.
