NATIVE UPSCALING BINARIES COMPATIBILITY PATCH - 0.73
https://github.com/brodrigz/xray-monolith

INSTALL
-------
1. Install Native Upscaling Binaries 0.73 using that release's instructions.
2. Install and enable SSS 23.5 with GAMMA's recommended options.
3. Install this ZIP through MO2:
   - Page 1: leave the required SSS 23.5 option checked.
   - Page 2: choose your installed Atmospherics version, or None.
     The RC8 and RC8.1 choices work with whatever options you
     picked in the Atmospherics installer.
   - Page 3: select only the add-ons you have installed.
4. Enable this patch BELOW SSS, Atmospherics and any other shader mods
   in MO2's left pane.
5. Launch the DX11 game (or the experimental DX12 one) through MO2. In
   Settings > Video > Basic: set MSAA to Off, choose DLSS or FSR, then click
   Apply. Frame generation is in the same menu on the DX12 executable.

The original mods and binaries must be installed separately.
DLSS requires an NVIDIA RTX GPU. No special launch arguments are needed.

OPTIONAL ADD-ONS
---------------
WearableDevices:
  Select only if the original mod is installed and enabled.
  Disable the old separate WearableDevices DLSS patch.

Better Beef's NVGs (.19):
  Leave unchecked for stock GAMMA / stock Beef's NVG.
  Install original Beef's NVG, then Better Beef's with these modules:
    00 - Main
    02 - PATCH - [ SSS ]
    03 - PATCH - [ SSS Weapons DOF ]   if you use SSS Weapons DOF
    01 - PATCH - [ FDDA Redone ]       if you use FDDA Redone
  Put its patches below its main files, and this compatibility patch last.
  Clear the shader cache through the launcher after changing NVG mods.

Glossy Puddles Overhaul v2.6 (Erandis):
  Select only if the original mod is installed and enabled.
  Requires Glossy Puddles v2.6; with older versions the game fails to
  compile shaders. Update Glossy Puddles first.
  The patch picks the matching version from your Atmospherics choice:
    Atmospherics RC8.x                 -> Glossy Puddles "PBR Shader Based Weathers"
    anything else                      -> Glossy Puddles "SSS Based Weathers"
  Put Glossy Puddles above this compatibility patch in MO2's left pane.

ATMOSPHERICS
------------
Supported: stock GAMMA Atmospherics (2.68.93 Hotfix 3), 2.69 RC8 hotfix 4
and 2.69 RC8.1. RC7.x and earlier RC8 hotfixes are no longer supported.

RC8.1 dithers some lighting data with noise that changes every frame. With
DLSS / FSR on, the RC8.1 choice holds that noise still, since the upscaler
shows it as grain:
  - material data on hands, weapons, foliage and other PBR objects
  - hand and weapon screen-space shadows, from the sun and from lights
With the upscaler off, RC8.1 is unchanged.

WHAT'S NEW IN 0.73
------------------
- Atmospherics RC8.1 support. RC8 hotfix 4 stays supported; the RC7 choice
  is gone.
  - Grass keeps RC8.1's per-blade lighting direction.
  - Flat ground keeps RC8.1's version of its shader.
  - Better Beef's + RC8.1 keeps RC8.1's bloom changes.
  - RC8.1's blood decals use their smoother dither with DLSS / FSR.
  - Less grain on hands, weapons and foliage with DLSS / FSR (see
    ATMOSPHERICS above).
- Glossy Puddles v2.6 (required with RC8 and RC8.1; v2.5 no longer
  supported): its afterglow tint now shows on flat ground.
- WearableDevices v0.10.1.

UPDATING OR CHANGING OPTIONS
---------------------------
Reinstall this patch when changing Atmospherics or any optional add-on.
Choose REPLACE in MO2 so files from your old selections are removed.
Keep only one Native Upscaling Binaries compatibility patch enabled.

CUSTOM SSS INSTALLATION ONLY
---------------------------
GAMMA users: use GAMMA's recommended SSS 23.5 options.
For a custom installation, include these SSS 23.5 modules:
  00 - MAIN FILE
  00 - MAIN FILE (MV)
  01 - AO
  02 - SSS
  03 - SSR
  04 - SSR Water                  (normal water, not tested with ice)
  05 - Rain - Puddles
  07 - Indirect Light
Keep any additional modules required by your modpack. Install SSS's
compatibility options only for mods you also have installed.

IF IT DOES NOT WORK
-------------------
Check your versions, installer choices and MO2 left-pane priority first.
If it still fails, share your full appdata/logs/xray_<username>.log,
enabled modlist and installer choices when asking for help.
