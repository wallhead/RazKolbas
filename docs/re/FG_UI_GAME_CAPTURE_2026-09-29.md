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
The actual 0.1.116 game capture is **NOT RUN**. A correct UI sequence will still be
opaque-composite evidence. FG integration additionally needs a verified
separate transparent producer and typed camera/Streamline constants.
