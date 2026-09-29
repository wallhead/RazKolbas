# Branch review 34: loader ownership and probe admission

The supplied `RazKolbas_Branch_Review_34.zip` reviewed commit `a844050`.
Its report is external review evidence. The three new actionable findings
were verified against that source before changes. All entries listed in the
packet's manifest matched their recorded size and SHA-256.

## Corrected probe acceptance

The private native-first probe now requires a created lower swap, a successful
native-interface unwrap with a distinct native lower for this exact scenario,
a Present method owned by the pinned interposer, a resolved pinned DLSS-G
option function, successful FG Off, and matching D3D11/D3D12 adapter LUIDs.
Each failed stage has a nonzero exit. A scoped probe releases its interfaces
and window before the runtime shuts down. Seven fault injections exercised
the expected exit codes: lower 21, unwrap 23, managed Present 24, function
25, FG Off 26, adapter 27, and shutdown 4. These injections test acceptance
flow; they do not simulate a failing vendor GPU call. The normal real-vendor
probe exited 0 with all named checks true.

## Loader ownership

Initialization now refuses a process with any of the six pinned Streamline
module names already loaded. It holds read-only, no-write/no-delete sharing
handles to all six selected files while hashing, loading and initializing.
It checks module paths/hashes and the interposer handle after load and after
initialization, and confirms that core export addresses belong to that
interposer. Public feature-function calls verify that the returned function
belongs to one of the pinned modules. Startup interface calls recheck loaded
module ownership. The class serializes its own check/load interval; it
cannot serialize an unrelated mod that does not use its mutex.

The normal staged six-DLL runtime initialized, bound the D3D12 device,
created the lower swap, applied FG Off and shut down. Preloading the exact
interposer from a different directory was refused before initialization;
a one-byte-mutated staged `sl.pcl.dll` was refused before loading. These are
separate-process Windows/NVIDIA observations, not Skyrim tests. MO2's
virtualized module paths and concurrent foreign initialization remain
unverified in the game, so no game-side loader was enabled.

The source change passed 60/60 Release and 57/57 Debug CTest groups. The
real-vendor and injected-failure observations used the Release probe.

## Factory entry path

PE import inspection of the two built Release probes found
`CreateDXGIFactory1` imported from `sl.interposer.dll` in the older static
probe and from `dxgi.dll` in the private-loader probe. That confirms the
different entry paths hypothesized by the review. It explains why a native
vtable hook appears in the older path and not the private path, consistent
with the pinned SDK source; it does not make global vtable mutation a
production requirement. Both paths return a usable explicit Streamline
factory proxy. The next combined private-loader test must verify the
ReShade-returned **swap** and lower Present, which this review still leaves
open. Review 33's producer failure, prior success predicates and camera
admission items also remain tracked separately.
