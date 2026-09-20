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
End-key toggle test and ENB/ReShade visual verification are **NOT RUN** until
the source DLL is installed and the user starts Skyrim. The assistant does
not start or control the game.
