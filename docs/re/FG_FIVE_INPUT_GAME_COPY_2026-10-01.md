# Five-input game copy trial, 2026-10-01

Source 0.1.153 pairs the converted depth and motion, HUD-free native colour,
separate premultiplied UI and final display colour from one real Skyrim frame.
The pairer requires matching source, generation, real Present token, reset
epoch and extents. It verifies `R32_FLOAT` depth, `R16G16_FLOAT` motion and
native RGBA8 colour planes on one D3D11 device; stale or aliased roles fail
closed. The result retains its five source textures and stamps the matching
D3D11 inputs. It does not mark a presentation owner or camera ready.

When the existing one-shot UI candidate completes in a loaded world, a
source-only interop probe creates a D3D12 device and direct queue on the
verified render adapter. The existing five-slot input ring copies the five
textures through shared D3D11/D3D12 surfaces. A later real pre-Present polls
the copy fence without blocking; on completion it discards the unsubmitted
lease and stops the ring. An uncertain/failed copy retains its owners through
the existing quarantine path. Resize abandons an outstanding diagnostic
probe safely. **There is no Streamline token, FG provider call, generated
Present or continuous UI route.**

The new pairer test initially failed to compile because its API was absent.
The new probe test likewise failed on its absent API. WARP then verified exact
five-resource roles, stale/typeless/null rejection, same-adapter shared copy,
copy-fence completion and clean unsubmitted lease retirement. Release
build/CTest passed **70/70** and Debug passed **65/65** after the version bump.
**0.1.153 Skyrim runtime
and FG-On/generated frames: NOT RUN.**

The Release package was staged at
`artifacts/local/stage-v54-fg-five-input-0153` using the existing INI.
The stage and old installed package each verified **14/14**; Skyrim was not
running. The previous DLL/INI/manifest were backed up under ignored
`artifacts/local/backup-v54-before-0153`. Only the DLL and manifest were
replaced, and the new install verified **14/14** with the INI byte-identical.
Staged/installed DLL SHA-256:
`655468180d879e6d4491ebcd44cb5dfe887739f216f24f10dda25dabf712632d`;
INI SHA-256:
`3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`;
manifest SHA-256:
`e79393fe1d6c3f548c07258fb66a80a1765253953e4f4d62c09eb0ef373aaf3e`.
The user started Skyrim through MO2, loaded the same save with FG Off, and
reported normal world image, native UI, and steady FPS compared with 0.1.152.
The 12:48:54 game session queued one five-input copy for source/Present 9472,
generation 1, with producer/copy fence values 5/5. The next real Present
reported `status=complete`. The captured 0.1.153 session had one queue,
one completion, zero copy rejection/failure, 18 guide/camera candidates,
zero SR suspensions, and zero sampled nonzero Present failures. The ignored
log snapshot is `artifacts/local/fg-five-input-0153-live-snapshot.log`
(25,009,893 bytes, SHA-256
`14c76758ab6bac97c421e7cffb5f05d2447d487b5678881afd91455b8865facf`).
This proves a sampled in-game D3D11-to-D3D12 copy-fence completion and a
normal FG-Off visual regression check. It does not prove copied pixel values,
continuous leases, provider consumption, or generated Skyrim frames. The user
then closed Skyrim through its menu; the process was absent on inspection.
