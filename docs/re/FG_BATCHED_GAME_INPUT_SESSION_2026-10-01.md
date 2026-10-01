# Batched five-input game session, 2026-10-01

Source 0.1.154 changes the 0.1.153 five-input copy from five D3D11
`Flush`/D3D12 wait/signal pairs to one verified batch per source frame. The
interop bridge checks every source owner, target owner and matching texture
descriptor before issuing any GPU copy. It then copies the five textures,
signals one producer fence, flushes once, and queues one D3D12 wait/copy-fence
signal. The input ring retains the five sources and shared targets under the
single ticket. A failed/uncertain submission still quarantines its owners.

The existing test failed before implementation: its first five-input lease
reported producer fence 5 where the new contract requires 1. WARP now checks
that a bad fifth input leaves the first target's sentinel pixels untouched,
then checks five distinct copied pixel values and fence 1/1. The probe's new
reuse test failed to compile before its `enqueue`/`close` API existed. It now
submits a second increasing source/Present pair on the same companion device,
rejects a new copy while the old one is pending, retires both and closes.

The game-side FG-Off diagnostic arms at most eight ordinary loaded-world UI
frames. It reuses one companion D3D12 device and shared-input ring across
successive frames, polls the prior copy without blocking before another
enqueue, and closes after the final copy retires. Menu/loading/preview frames
remain excluded. Each frame still uses the verified native UI route and
same-frame converted guide pair. This is a bounded, sampled input-lifetime
trial: no Streamline token, provider submission, lower swap ownership or
generated Skyrim Present occurs. A copy can be skipped while the prior GPU
copy is pending; the log reports attempts and completed copies separately.

Release build and CTest passed **70/70**; Debug build and CTest passed
**65/65**. The eight-frame Skyrim test is **NOT RUN** until a user-started
MO2 launch. FG-On/generated Skyrim frames remain **NOT RUN**.

The final Release package was staged at ignored
`artifacts/local/stage-v54-fg-batched-session-0154b`. The previous installed
package and final stage each verified **14/14**. After the user closed
Skyrim, the previous DLL/INI/manifest were backed up under ignored
`artifacts/local/backup-v54-before-0154`. Only the DLL and manifest were
replaced; the installed 0.1.154 package verified **14/14**, with the user INI
byte-identical. Final staged/installed SHA-256 values are:

- DLL: `06bb6ce49e8fa2cf75ac4ebe00f3f4091f6698ffb3a1d8ebe242c9810e3fe02a`
- INI: `3df48892da15b6d20236653bb8b65423b52f2e35835cdc9585b0e9728f974914`
- manifest: `968ba530d17e3a9f547ddf81969e41abc43881b88d9305cf59e570d44439e14b`

## User-started Skyrim result

The user started 0.1.154 through MO2, loaded the same save with FG Off, and
reported that the image and steady FPS looked fine. The 13:17:07 session
logged eight native UI/converted-guide pairs at 2560x1440, for source and
real Present frames 17959 through 17973 at two-frame intervals. Eight
five-input copies queued with one producer/copy fence pair per frame,
numbered 1/1 through 8/8; all eight completed. The session closed with
`attempts=8 copies=8 clean=true`. In the captured session there were zero
copy-unavailable lines, UI-pair rejections, SR suspensions or sampled nonzero
Present failures. The user did not separately report a native-UI visual
comparison in this message; the complete native UI route is a log result.

Ignored live log snapshot:
`artifacts/local/fg-batched-session-0154-live-snapshot.log`, 25,192,532
bytes, SHA-256
`e3ceb7b033702e2470faa4e823e5182dc6a1b31efb644450adc53106e52d1fc2`.
The snapshot includes earlier appended sessions; counts above are only after
the latest 0.1.154 bootstrap marker. **Eight-frame FG-Off input copy:
RUN/PASS. Provider use and generated Skyrim frames: NOT RUN.**
