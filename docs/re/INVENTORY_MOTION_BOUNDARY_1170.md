# Inventory motion-target boundary, 2026-09-26

The user-started RazKolbas 0.1.92 run left inventory visible but flickering.
Its first late-route fault bound a reduced `R16G16_FLOAT` RTV1 with D3D11
render-target and shader-resource flags `0x28`; the same resource was requested
at PS slot 2 before pre-Present. A 0.1.93 experiment allocated a native-size
paired RTV/SRV, redirected both writes and reads, and resumed late routing two
frames later. The user then reported an invisible inventory and pixelated hero
preview. The 0.1.93 log shows no later compatibility fault through frame
18000. Successful resource substitution is therefore insufficient to infer
correct inventory composition.

The reference `SkyrimUpscalerAIOBuild16-Hotfix1/SKSE/Plugins/SkyrimUpscaler.dll`
has SHA-256 `94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb`.
Read-only Ghidra 12.1.3 decompilation of RVA `0x1f3c80` identifies its
source path as `SkyrimUpscaler::SetupMotionVector`. It stores the supplied
motion texture at host `+0x280`, reads its descriptor, changes width and
height to the display dimensions `+0x24/+0x28`, and creates a companion at
host `+0x2d8`. The reference OM hook at RVA `0x2015f0` obtains that
companion's RTV when the matching scene is bound with more than one target.

Capstone 5.0.7 independently decoded a complete 73-byte window at RVA
`0x1f3d78..0x1f3dc0`: it checks host `+0x2d8`, reads the source texture
descriptor through virtual slot `+0x50`, writes display width/height, and
passes `host+0x2d8` as the result address to the device's virtual slot
`+0x28`. A second complete bounded decode starts at the OM hook's RVA
`0x2015f0` and spans `0x360` bytes. Its branch at `0x201880..0x201898`
shows `cmp edi,1`, then `add rcx,0x2d8` and a call to the wrapper's RTV
accessor before forwarding OM. This window includes bytes after the hook's
return. The decoded windows and Ghidra outputs remain ignored under
`artifacts/local/`.

This establishes the reference's native motion-attachment role. It does not
identify where inventory UI and its hero preview are drawn, nor prove that
the reference remaps the motion SRV in the same way as 0.1.93. The next
RazKolbas diagnostic must observe all inventory render-target bindings after
the menu boundary, including binds that do not contain the owned scene.
