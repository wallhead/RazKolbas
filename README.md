# RazKolbas

Independent Skyrim SKSE upscaler implementation. Work is in progress; no rendering feature or supported game/GPU combination is claimed yet.

The original handoff is preserved in `RazKolbas_Codex_Handoff/`. Its specification, patch policy and implementation plan govern the code at this repository root. Current implementation status will be tracked in `docs/IMPLEMENTATION_STATUS.md`.

Reference binaries, extracted packages and build products are ignored and must not be committed. Reference hosts are research inputs, never product dependencies.

## Build

Prerequisites: Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.26100.0, CMake 3.25+, Git, Python 3.11+, PowerShell 7. The source dependency pins are in `runtime/dependencies.lock.json`; CMake fetches them on first configure.

```powershell
pwsh -File tools/Build.ps1 -Preset win-dev
pwsh -File tools/Build.ps1 -Preset win-release
```

The helper discovers Visual Studio's bundled CMake when absent from PATH. Equivalent commands are `cmake --preset win-dev`, `cmake --build --preset win-dev`, and `ctest --preset win-dev --no-tests=error --output-on-failure` (substitute `win-release` for Release).

Native-only DLL output: `build/win-dev/bin/Debug/RazKolbas.dll` or `build/win-release/bin/Release/RazKolbas.dll`. The SE 1.5.97 / AE 1.6.1170 loader gates are initial test targets, not claims of game-validated compatibility. No rendering hooks or vendor providers are activated.

To create an isolated staging folder, run `pwsh -File tools/Stage-MO2.ps1 -Destination <new-folder>`. Existing destinations are refused. No personal game or modlist directory is selected automatically.
