# AIO DLSS/FG deep dive RE28: local audit, 2026-09-28

The owner supplied `C:/Users/user/Downloads/AIO_DLSS_FG_DeepDive_RE_28.zip`
(SHA-256 `d18b0f0be9b32656846bf61d6d7050a25d14dc8a1bc7672bc825cb110b9afcd8`).
The ZIP remains outside Git. An independent read-only ZIP check matched the
length and SHA-256 of all 84 files listed in its manifest; the 85th entry is
the manifest itself. This checks package integrity, not the recovered
semantics. The packet reports original-byte disassembly and isolated original
CPU probes; those probes were **not rerun locally** for this audit. No
Windows/GPU/Skyrim result follows from them.

The packet separates the reference host's outer stable D3D11 source alias
from its companion's indexed transfer resources. Its reported lower Present
packet preserves a source-colour slot, a guide/consumer slot and a current
physical output index independently. These are different objects and not
interchangeable frame IDs. The packet's original-byte CPU probes support
selected source-to-destination copy ordering, but do not prove which branch
runs with this user's live ENB/FG settings. No private AIO offsets or GUID
lists are needed in RazKolbas production code.

Our source at the packet's pinned `c00e1b8` still selected the D3D11 source
by physical lower index. Commit `478f467` corrected that before this ZIP
arrived: one cached logical D3D11 source now copies to the rotating lower
D3D12 destination. WARP tests validate two- and three-buffer rotation. A
subsequent test-only observer validated the full facade before lower Present
on WARP, and the same observer measured red, green and blue real frames at
physical indices 0, 1 and 0 through the Streamline 2.14.1 FG-Off proxy. That
is local GPU evidence for the corrected colour route, not a generated frame.

The packet also records a colour-copy state tracker and one-shot pending
copy. Its selected barriers are COPY_SOURCE and COPY_DEST around colour
publication, returning both resources to COMMON. This is evidence for that
copy path only. Our bridge transitions its owned shared colour and lower
backbuffer around its copy; guide/UI resources and provider consumption need
their own real state/flag checks. Literal AIO tag states are not imported.

The reference FG writer reuses depth/motion resources staged earlier by SR.
The packet's deliberately manipulated CPU fixture shows that a new frame
number alone does not prove fresh guide contents; it is **not** a claim of a
live stale-frame bug. RazKolbas `FgResourceStamp` and `FgInputLeaseRing`
already require matching source, generation, reset epoch and extents before
copying guide resources. The following offline change also binds the copied
lease to its presentation token, reset epoch, source, exact D3D12 resource
identities and producer/copy ticket; altered lease copies cannot be submitted.
The WARP regression passed with Debug and Release 52/52 CTest groups. This
does not prove source texture contents or guide freshness in Skyrim. The lease
still lacks a typed camera payload, Streamline token/viewport, physical output
index and the provider's returned previous-input completion. No provider
currently consumes the ring.

The packet traces a named SetCameraData export to a separate singleton; it
does not establish that export as the producer of Streamline's full current
and previous camera constants. RazKolbas currently has only a `cameraValid`
policy flag, not a submitted typed DLSS-G camera record. Calling the named
reference export would not solve this gap.

The next implementation contract is one immutable prepared real-frame record
that associates actual copied colour, guide lease, source/generation/reset
epoch, typed camera/jitter, Streamline token/viewport, chosen physical
destination, and real producer/copy/provider/Present/allocator fence values.
Bind the returned provider-input completion to the **previously presented**
record before any lease is reused. Only then can FG-On tagging and generated
output be tested. The installed V5.4 0.1.115 game DLL remains unchanged;
Skyrim FG-On runtime verification is **NOT RUN**.
