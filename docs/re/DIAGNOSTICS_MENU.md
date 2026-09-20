# End-key diagnostics menu — source checkpoint 0.1.23

The verified ENB swap-chain Present callback now offers a read-only Dear ImGui
panel. End toggles it on a rising key edge while Skyrim owns the foreground
window. The panel uses the most recent world frame's effective submission
state: Native on a skipped/failed DLAA frame, DLAA after a successful
full-resolution submission, and DLSS Super Resolution only when a future
reduced-render integration explicitly publishes that mode. It displays render
and native display dimensions, scale, world and DLSS submission counts, native
fallback count, a session-disable indicator, and the current Skyrim TAA state.
At this checkpoint Skyrim TAA remains on and reduced game rendering is off.

The callback creates a temporary RTV for the current backbuffer only while
the panel is visible. It restores the D3D11 context state before forwarding
Present and retains no backbuffer reference across resize. Mouse position and
buttons are polled for this read-only panel; there is no WndProc hook, input
capture, settings mutation or game-code patch. The menu is tied to the
currently verified ENB swap profile; other swap owners are not claimed to
support it yet. Whether the composed panel survives this modlist's actual
ENB/ReShade pipeline requires a user-started game observation.

The build uses upstream Dear ImGui from `https://github.com/ocornut/imgui.git`
at pinned commit `f5befd2d29e66809cd1110a152e375a7f1981f06` (1.91.9b),
already listed in `runtime/dependencies.lock.json`. Dear ImGui is MIT licensed;
the runtime package contains compiled code, not the community frame-generation
reference source. The pinned community reference remains read-only evidence.

Release `Build.ps1` passed all 22 CTest groups. A real on-screen menu test,
End-key toggle test and ENB/ReShade visual verification remain **NOT RUN**
until the user starts Skyrim. The assistant does not start or control the game.

The 0.1.23 build was staged and installed after the Release checks. Package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.23-menu-ef9570b.zip` has SHA-256
`b6772ee4d4176441d1285711d3b2a260a90d5b76d88f9a3c373819ede746647e`.
The installed DLL SHA-256 is
`325c4f74d3ac8d725044bbdfc595fa295941b341bab3cf1395e64bf0fc502a32`.
All four installed manifest payload hashes and all ZIP entries were checked.
The user's INI and signed NVIDIA runtime retained their prior hashes. The
prior DLL/manifest are backed up in ignored local artifacts; no reference
host DLL, PDB or capture was packaged.

## 0.1.23 user observation and 0.1.24 cursor fix

The user started the installed 0.1.23 build. Its log recorded the End menu
opening at 14:19:06 and toggling again at 14:22:50. World and Present counts
matched through at least 37,200 with zero Present failures. The user confirmed
the panel appeared, but its visible game cursor passed underneath and could
not move the panel. During the menu test the log repeatedly sampled menu-like
far-plane depth, so DLAA was not established for that session. Earlier 0.1.22
DLAA results remain separate.

The 0.1.23 panel polled `GetCursorPos` and mouse buttons for ImGui but left
`ImGuiIO::MouseDrawCursor` at its default false. Skyrim's own drawn cursor is
composited before the RazKolbas Present overlay, explaining why that cursor
appears beneath the panel. The upstream community reference explicitly draws
an ImGui cursor above its menu for the same Skyrim condition; no reference
source was copied. Source 0.1.24 enables the ImGui cursor only while the panel
is open and focused, resets the flag on close/focus loss, and logs a bounded
sample of ImGui mouse position/button/capture state. The exact mouse-drag
result is **NOT RUN** until the user starts the updated build. The code does
not yet suppress Skyrim's own gameplay input while the panel is open.

The 0.1.24 package
`D:/TESV_EX/MO2/downloads/RazKolbas-0.1.24-cursor-6b3cc05.zip` has SHA-256
`7573897e9e2e8c8c204d6701388dc78d055c538fba76f4cded2690cba67c353c`.
Its ZIP entries and four payload hashes were independently verified. Skyrim
was closed; the old 0.1.23 DLL/manifest were verified and backed up before
installing the new DLL SHA-256
`7494c13238712bfe5595ba8c7b1cb60d0656d1381d92c1ad713b80f22be772ee`.
Installed manifest verification passed; user INI, signed NVIDIA SR runtime and
MIT notice were preserved. Live cursor behavior remains **NOT RUN**.
