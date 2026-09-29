# V5.4 FG/UI capture, 2026-09-29

The user started the installed 0.1.115 build through MO2, loaded a save, and
opened Inventory and Magic. PID 21928 produced four complete bundles under
`Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/`:

| Bundle prefix | Frame | Images | Result |
|---|---:|---:|---|
| `owned-sr-stages-21928-11901` | 11901 | 3 | Prepared 1485x835 input, 2560x1440 DLSS output, 2560x1440 final |
| `owned-ui-sequence-21928-11901` | 11901 | 1 | Only after-all-menus; zero menu-entry snapshots |
| `cursor-replay-21928-14042` | 14042 | 3 | Inventory before/after Cursor Menu replay and after EndFrame |
| `magic-movie-replay-21928-14164` | 14164 | 3 | Magic before/after movie replay and after EndFrame |

All ten raw files matched their manifest lengths and SHA-256 values. The first
DLSS evaluation and image capture preceded activation of the native menu UI
route at frame 11932. Therefore its zero-entry UI sequence cannot identify
menu draw producers. It is a capture-trigger timing error, not evidence that
the HUD or menus were absent.

An independent RGBA8 byte comparison of the same-frame replay captures found:

| Replay | Changed pixels | Bounding rectangle, exclusive right/bottom | Further EndFrame changes |
|---|---:|---|---|
| Inventory Cursor Menu | 702 / 3,686,400 (0.019%) | `(1436,683)-(1464,724)` | 0 |
| Magic movie | 1,820,499 / 3,686,400 (49.384%) | `(0,0)-(1267,1440)` | 702 cursor pixels, `(1433,688)-(1461,729)` |

The Inventory replay is only the late cursor, so the inventory list and hero
were already in the native composite before it. The Magic replay writes a
large left-side area after the native rebind. In all six replay images, final
alpha is 255 everywhere, and neither replay changes final alpha. These
opaque backbuffer captures can locate writes but cannot supply a genuine
transparent UI plane or prove the original UI draw alpha. The DLSS-output
and final-composition raw images differ at 3,519,094 pixels (95.462%), which
also shows the SR output cannot simply be labelled final HUD-less colour
without attributing effects and later writes.

The log records successful native menu publication and 1,900 DLSS provider
submissions by frame 13800. It also records that an in-game FG On request
remained effective Off because no FG backend owns presentation. This run did
not test game FG-On. No new crash log was observed. A normal window-close
request did not exit the idle process within 40 seconds, so the assistant
stopped PID 21928 after the capture files were complete. The one-shot INI was
then restored byte-for-byte to its pre-capture SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`.

The 0.1.116 diagnostic build separates the first SR-stage capture from the first
**MenuDisplay DLSS provider** capture. It arms the UI sequence only when the
native-boundary provider frame has been published, before the first predicted
menu-stack entry. That should yield useful before-menu snapshots on the next
user-started game run. Release and Debug each passed 54/54 CTest groups.
The verified eight-file MO2 archive is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.116-ui-sequence.zip`,
SHA-256 `d52a48e1b87d4744fd3f873464835518297fac49dc52e75facb0e51689f55001`.
The installed `RazKolbas.dll` and one-shot INI hashes are respectively
`ed62abde2e4484113b51f0c45ac0310619064d5b69384d543dc66416737f8ce2`
and `0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
All eight installed manifest payloads matched; vendor runtime hashes were
unchanged. A byte-for-byte backup of the four replaced local files is in
ignored `artifacts/local/mo2-install-backup-0.1.116-2026-09-29`.
At packaging time the 0.1.116 game capture was **NOT RUN**; its subsequent
result follows. The UI sequence remains opaque-composite evidence. FG
integration additionally needs a verified separate transparent producer and
typed camera/Streamline constants.

## 0.1.116 user-started follow-up and 0.1.117 probe

The user started 0.1.116, loaded a save and opened Inventory and Magic. PID
27816 produced four complete bundles. An independent check matched the byte
length and SHA-256 of all 25 raw files against their manifests. The UI sequence
armed on first native-boundary DLSS provider frame 9836, after 32 prior DLSS
submissions, and recorded all 15 menu entries. Its 16 centre-crop images,
including `after-all-menus`, have the same SHA-256
`7e3d2c7b4ca9440f337b472090d622377d385f9bd8cc13f28c4d4506ab369572`.
The saved crop is the middle of a door. A separate full final frame from frame
9805 shows HUD text at the upper right and bars near the lower left, outside
that crop. The flat sequence therefore cannot locate the HUD writer; it is not
evidence that the HUD was absent or that no UI draws occurred.

The same-frame replay captures show real native colour changes:

| Replay | Changed pixels | Bounding rectangle, exclusive right/bottom | EndFrame continuation |
|---|---:|---|---|
| Inventory Cursor Menu | 696 / 3,686,400 | `(158,655)-(186,696)` | 0 pixels |
| Magic movie | 1,820,829 / 3,686,400 | `(0,0)-(1266,1440)` | 701 cursor pixels at `(280,654)-(308,696)` |

All six replay images had alpha 255 at every pixel; none of the replay steps
changed alpha. The saved final Inventory and Magic frames visibly contain their
menus and 3D hero, but this is capture evidence rather than a user assessment
of appearance. No new crash log appeared. A normal close request did not exit
the idle process in 12 seconds, so PID 27816 was stopped after all captures
were complete. The one-shot INI was restored byte-for-byte to SHA-256
`e05eed4f2a80237608f9c4e6a4a595bb7c36a4c19248239b38d827504f56ff5b`.

Version 0.1.117 changes only this diagnostic: it hashes the full native frame
before each predicted menu entry, retains the baseline and any changed entry,
then saves a full post-EndFrame image. This covers HUD at the screen edges and
keeps identical intermediate images out of the bundle. It does not introduce
a transparent UI producer or enable FG in Skyrim. Debug and Release builds
each passed 54/54 CTest groups; **0.1.117 Skyrim capture is NOT RUN**.
The eight-payload MO2 archive is
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.117-full-ui-probe.zip`,
SHA-256 `880a541b0f4ffd49732edb18c4130853296ead3bafff2e25dbfbf5f19091ef4f`.
All archive and installed payload hashes were verified. Installed DLL SHA-256
is `7165040ef6eb9d0ab87dd552ce6f9581abce0cb994cb1040bb350fd6c270b764`;
the armed one-shot INI SHA-256 is
`0a322215401ce23406a6ec7bd6697ff0f3edeebbf69b49c2c033df2710f43b7b`.
The earlier DLL, INI and manifest are backed up in ignored
`artifacts/local/mo2-install-backup-0.1.117-2026-09-29`. The next game run
needs only a save loaded into a world scene; the full-frame sequence triggers
on the first native-boundary provider frame. After collection, restore the
INI and manifest to the package default with the probe disabled.

**Superseded before a 0.1.117 game run:** Ghidra and Capstone confirmed that
the actual common `EndFrame` call follows all per-menu `PostDisplay` calls.
Version 0.1.118 now captures on both sides of that flush. The current package
and next runtime action are recorded in
`docs/re/UI_PIPELINE_GHIDRA_CAPSTONE_2026-09-29.md`.
