# V5.4 FG swap facade: user-started live read-only result

The user started Skyrim through the V5.4 modlist on 2026-09-28. The running
process used the installed RazKolbas 0.1.115 DLL with SHA-256
`bd775e7ae9f6dd76492fc5fd1e8958f26fb780fa12dc2d9d0653196796f77db4`.
The SKSE `RazKolbas.log` captured these creation-time facts at 19:24:24 and
19:24:31 local time. RazKolbas changed no swap interfaces in this probe.

| Boundary | DXGI owner | QI SwapChain1/3/4 | GetDevice D3D11 | GetDevice D3D12 | Creation-device COM identity |
| --- | --- | --- | --- | --- | --- |
| Nested factory return | ReShade 6.8 (table SHA-256 `b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`) | all `S_OK` | `S_OK` | `E_NOINTERFACE` (`0x80004002`) | true |
| Outer return to Skyrim | ENB 0.505 (table SHA-256 `35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3`) | all `S_OK` | `S_OK` | `E_NOINTERFACE` (`0x80004002`) | true |

Both `GetDesc` calls returned `S_OK`; the outer description was 2560x1440,
DXGI format 28, three buffers, one sample, flip-discard and windowed. The
nested ReShade factory call used the D3D11 creation device, and the outer ENB
swap is what Skyrim received. The active adapter was NVIDIA RTX 4080 SUPER,
vendor `0x10de`, device `0x2702`. Later Present observations returned `S_OK`
through 12,000 frames; this verifies only the existing read-only path, not a
new lower presentation owner or generated frames.

**Consequence:** a Streamline-owned D3D12 lower swap cannot simply replace
either game-visible pointer. The future outer-facing route must preserve
`IDXGISwapChain1/3/4` identity and `GetDevice(ID3D11Device)` identity, provide
D3D11 texture backbuffers to the existing ENB/ReShade/Skyrim consumers, and
route one real Present to a D3D12 lower swap after same-adapter GPU transfer.
`GetDevice(ID3D12Device)` must remain `E_NOINTERFACE` at the game-facing
boundary. Buffer references, resize HRESULTs and the existing early SR/native
UI GetBuffer publication contract must remain valid. This is a required
implementation contract, not evidence that the facade already exists.

After the trace was captured, `CloseMainWindow` did not exit the process;
the previously authorized process close succeeded with `Stop-Process -Force`.
No game files, INI, installed DLL or vendor runtimes were changed during this
observation. FG On and generated-frame output remain **NOT RUN** in Skyrim.
