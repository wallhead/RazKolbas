# MagicMenu boundary capture and native movie replay candidate

The owner reports that 0.1.105 restores MagicMenu visibility, but its UI is
blurry. In the user-started process PID 17620, the one-shot capture at frame
11251 completed under the local, untracked
`RazKolbasCaptures/magic-boundary-17620-11251-83734828` directory. Its
manifest records six raw RGBA images. The reduced source is 1707x960; the
native colour image is 2560x1440. All six file sizes and row extents match
the manifest. The reduced-before and reduced-pre-Present hashes match;
native-after-copy, native-after-EndFrame and native-pre-Present hashes match.

Visual comparison shows the world and rendered hero on native colour before
the late copy, but no MagicMenu list. The reduced source has the list over
the world/hero. The full-image difference between native before and after
copy spans `(0,0)–(2560,1440)`, because publication replaces the image.
The late copy thus carries a reduced-resolution UI into the display-sized
image. No later EndFrame draw changes that captured native image.
This directly explains the 0.1.104 invisible-list regression and the
0.1.105 blurry-list result. The capture does not establish which engine
draw call initially submitted the list or whether replay will draw it.

CommonLibSSE-NG identifies MagicMenu as a custom-rendering menu with a
`PostDisplay()` override; base `IMenu::PostDisplay()` only invokes the menu
movie's `Display()`. Earlier Skyrim inventory replay demonstrated a
Skyrim-owned cursor movie can produce surviving native pixels after the
deferred EndFrame boundary. The bounded 0.1.106 candidate therefore keeps
the native world/hero and invokes **only** the live MagicMenu movie's
`Display()` after native colour/depth rebind. It does not replay the custom
`MagicMenu::PostDisplay()` override or the 3D hero renderer. The existing
Cursor Menu replay remains after it. If the movie is absent, the prior
reduced-scene publication remains the fallback. This is a hypothesis to
verify in Skyrim, not a proven sharp-menu fix.

Release build and 43 CTest groups passed. Staged ZIP:
`D:/TESV54BETA/BETA_TRUEAE_V54/downloads/RazKolbas-0.1.106-v54-magic-native.zip`,
SHA-256 `506c2f4413ffb1f69c97d39c4540c350ef32a0b337bbceb4fdf11368a1e32dd5`.
An independent ZIP entry and payload hash check passed. Candidate DLL
SHA-256 `b5eeac5c4d1c5b9a2cf1f61c79420a3a115f1f0754bf8defd8130ac053001ccf`.
**Skyrim runtime/visual verification: NOT RUN.** Install after game exit;
inspect `magic-movie-replay-*` capture from the next user-started run and
check list clarity, hero rendering, cursor, inventory and title.
