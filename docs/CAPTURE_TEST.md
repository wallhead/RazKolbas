# Gameplay candidate capture test — 0.1.7

Launch through MO2 with RazKolbas enabled. Keep the current ENB/ReShade setup. This build has no automatic startup capture; the test installation enables `Diagnostics.CaptureHotkey = CtrlShiftF10`. Changing that setting requires a restart. The normal sample/default is Off and the verified experimental renderer hooks remain required.

1. Load a save outdoors and close menus. Wait until loading finishes.
2. Hold the camera still and press **Ctrl+Shift+F10**, then release the keys.
3. Wait at least five seconds. Turn the camera slowly and press the chord again while turning.
4. Wait at least five seconds. Turn the camera faster and press it a third time. If possible, include a moving NPC in view.
5. Exit the game and report completion, plus any visible problem or key conflict.

A brief hitch per capture is expected because the diagnostic synchronously reads GPU textures. Each request produces at most one candidate bundle. Six requests are allowed per game session, including failures/cancellations; holding the chord does not repeat. Presses during the five-second cooldown are discarded, not queued. A request expires after two seconds or32 unsuccessful eligibility checks. Observed loss of foreground focus or a gap of more than250ms between eligible Present callbacks cancels pending work and requires release before rearming. The trigger is a polled diagnostic; it does not claim to observe every brief OS focus transition between polls.

Output: `Documents/My Games/Skyrim Special Edition/SKSE/RazKolbasCaptures/<process>-<tick>/`. The plugin log records request numbers and paths. Only manifests ending `complete=true` represent fully written bundles. Raw files stay local; no manual upload is needed for this workspace.

These are candidate colour/motion/depth textures at the verified ENB Present boundary. A gameplay capture is needed to inspect their contents, but does not by itself prove pre-UI timing, motion-vector conventions, depth interpretation, or frame coherence. SR/FG/NR remain inactive.

For local inspection, run `python tools/re/inspect_candidate_capture.py "<capture-directory>"` (NumPy required). It verifies raw hashes and dimensions and prints numerical ranges; it does not certify guide semantics.
