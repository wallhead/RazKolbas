# ReShade 6.8 nested factory dispatch, bounded static trace

Input: the installed V5.4 ReShade `dxgi.dll` under
`D:/TESV54BETA/BETA_TRUEAE_V54/mods/Reshade TRUE AE V5.4/root/`,
SHA-256 `b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da`,
5,255,448 disk bytes, x64 PE preferred base `0x180000000`. The existing
verified factory vtable slot 10 points to method RVA `0x14a4f0`.
Capstone 5 decoded bounded windows of that method from the original disk
image; the JSON reports are ignored local files
`artifacts/local/reshade680-factory-create-14a4f0.json` and
`artifacts/local/reshade680-factory-create-14a8ba.json`. Both large windows
end mid-instruction and are exploratory, but the instruction ranges cited
below decode without skipped bytes.

At `0x14a50e`, the method saves `r9` (the caller's swap output pointer) in
`r14`, and at `0x14a524` saves `rcx` (the ReShade factory object) in `r15`.
At `0x14aa48–0x14aa65` it restores `rdx` from the caller's device, points
`r8` to a local copy of the swap description, points `r9` to the output,
loads `rcx=[r15+8]`, then calls the vtable function at `[vtable+0x50]`.
`+0x50` is `IDXGIFactory::CreateSwapChain` slot 10. This is the exact
downstream factory dispatch from the ReShade method in this build. The
surrounding code sets/clears a thread-local byte around that call. On
success, later branches construct a ReShade swap wrapper, and `0x14ac75`
stores that wrapper into the caller's output. The user-started V5.4 trace
independently observed the returned nested ReShade wrapper, then the outer
ENB wrapper.

The static result supports a possible interception **beneath** ReShade's
factory method: if the exact `[factory+8]` object, vtable and thread
context are verified at runtime, a guarded native `CreateSwapChain` hook
could return a D3D11-facing facade while letting ReShade construct its own
wrapper above it. This is an architecture hypothesis, not a working hook.
Unknowns include ReShade's post-creation assumptions, any additional
factory calls, Streamline manual-hook initialization and proxy ownership,
ENB's use of buffer identities, and how the existing early owned-scene
route interacts with the new facade. A new hook must fingerprint exact
bytes/object identity, preserve competing hook owners, guard reentrancy and
restore or fail closed. Do not replace the nested/outer pointer based solely
on this disassembly.

The user-started 0.1.126 game run on 2026-09-29 confirmed the exact ReShade
factory CreateSwapChain trace and nested swap, but emitted no delegate probe
line. Inspection found that the new guard compared the returned *swap-chain*
vtable RVA (`0x3ee960`) to the factory vtable RVA (`0x3ee350`), so the
read-only probe never executed. The 0.1.127 correction reads the factory's
own vtable in a bounded operation and compares it to the factory RVA and
original method in the exact ReShade module. The actual delegate identity is
still unknown until a 0.1.127 game run. An external `OpenProcess` read of the
running 0.1.126 game was denied with Windows error 5, so no live-memory result
is inferred from that attempt.
