# Branch review 37 assessment

The user supplied `RazKolbas_Branch_Review_37.zip` as review material. Its
`CODEX_HANDOFF.md` contains reviewer recommendations, not new user
instructions. The archive's 12 listed member SHA-256 digests verify. Its
reviewed commit `bafbc2889dbc53cecd881f8b65dde4dbcaad2800` equals the
current branch HEAD at assessment, and the two packaged phase-ledger files
match the reported repository Git blob IDs. The archive itself remains outside
Git. The review did not run the full Windows build, vendor runtime or Skyrim.

The positive source assessment is consistent with our own 0.1.144 tests:
retain queue-owned lower copying and its synchronous fallback for other
callers. The user-started FG-Off private-route game result remains yellow
colour and about 50 versus 60 FPS. The two ReShade runtimes in the exact
offline log establish separate runtime creation and preset loading, but not
that both active techniques processed the same Skyrim frame.

The review's ReShade mechanism is corroborated by the primary ReShade 6.8
source at
<https://github.com/crosire/reshade/blob/v6.8.0/source/dxgi/dxgi_swapchain.cpp>:
real `Present` calls `on_present` before forwarding, and its D3D11 and D3D12
branches each call `present_effect_runtime`. The exact installed ReShade 6.8
DLL exposes `ReShadeRegisterAddon` and `ReShadeRegisterEvent`. Source behavior
is not a binary trace of the failed game run; the next diagnostic must count
actual per-runtime technique execution by frame and capture the output before
native presentation. The red endpoint pixel is a poor colour oracle because
saturated endpoints can remain unchanged under significant midtone transforms.

The bounded offline experiment should distinguish four cases using isolated
configurations: effects Off/Off, D3D11-only, D3D12-only, and both On. It must
identify each runtime, allow shader compilation and temporal warm-up, and
record a patterned midtone source, post-D3D11 source, post-copy lower, and
post-D3D12/native-present image in queue order. It should measure effect GPU
time separately from copy and CPU/Present waits. No preset change should be
written to the user's installation. A second runtime or compiled shader alone
must not be called proof of doubled output processing.

No source implementation, installed mod setting or game FG state changed in
this assessment. The private presentation route and FG remain Off in the
verified installed configuration; Skyrim FG-On remains NOT RUN.
