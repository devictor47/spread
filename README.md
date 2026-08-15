<h3 align="center">ReGameDLL_CS Spread Control Plugin<br/>Forked from SmileYzn</h3>

<p align="center">Control the FireBullets3 function vecSpread parameter</p>

<p align="center">
    <a href="https://github.com/SmileYzn/spread/issues"><img alt="GitHub Issues" src="https://img.shields.io/github/issues-raw/smileyzn/spread?style=flat-square"></a>
    <a href="https://github.com/SmileYzn/spread/actions"><img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/SmileYzn/spread/build.yml?branch=main&label=Build&style=flat-square"></a>
</p>

<hr>

<p align="center">This plugin control the vecSpread variable of FireBullets3 on <a href="https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/cbase.cpp#L1268" target="_blank">cbase.cpp</a></p>
<p align="center">The plugin multiply the vecSpread with specified factor on some situations.</p>

<br>

<p align="center">The plugin optimizes shot spread.</p>
<p align="center">It does not affect recoil, nor does it change the default behavior of aim, it just adjusts the insane pattern of shots in the game.</p>
<p align="center">You can use this plugin as replacement for my <a href="https://github.com/smileYzn/accuracyfix" target="_blank">Accuracy Fix Plugin</a>.</p>
<p align="center">Since it is more optmized than accuracy fix plugin.</p>

<hr>

## How can I use it?
To install, copy the `addons` folder to you server's `cstrike/addons` folder such that `cstrike/addons/spread` folder exists and contains both a dll (or .so) and a spread.cfg file.
After that, go to `cstrike/addons/metamod`, open `plugins.ini` and add either `win32 addons\spread\spread_mm.dll` or `linux addons/spread/spread_mm.so`.
Start your server.

## Spread Control Settings
<details>
  <summary align="center">Click to expand</summary>

```
// Spread Control Plugin
// This plugin controls the vecSpread variable of FireBullets3
// https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/cbase.cpp#L1268
//								
// The plugin multiply the vecSpread with specified factor on some situations.
//
// 1. Any value bellow 0.0 will be ignored, which means you just need to set it to -1.0 to ignore that scenario;
// 2. To completly remove spread of weapon shots, you may set everything to -1.0, and set default to 0.0 for all;
// 3. The spread_deadCenterFirstShot cvar works by completely removing the spread to make the first shot go right to the center of the crosshair.
//								
// weapon		Name of the weapon WITHOUT prefix, e.g., ak47.
// in air		pread mitigation while player is airborne.
// moving & standing	Spread mitigation while player is moving and NOT ducking.
// moving & ducking	Spread mitigation while player is moving AND ducking.
// standing still	Spread mitigation while player is NOT moving and NOT ducking.
// ducking still	Spread mitigation while player is NOT moving AND ducking.
// default		Default spread mitigation when the player a settings does not apply to a specific situation, or for other situations not mentioned here.
//
// Usage:	spread_wpn <weapon_name> <in_air> <moving_standing> <moving_ducking> <standing_still> <ducking_still> <default>
// Example:     spread_wpn ak47 -1.0 -1.0 0.8 0.75 0.70 -1.0
//
//      COMMAND  |  WEAPON  |  IN   |  MOVING &  |  MOVING &  |  STANDING  |  DUCKING  |  DEFAULT  |
//                          |  AIR  |  STANDING  |  DUCKING   |   STILL    |   STILL   |           |
//
      spread_wpn    galil     -1.0       -1.0         0.85          0.8         0.7         -1.0
      spread_wpn    ak47      -1.0       -1.0         0.85          0.6         0.5         -1.0
      spread_wpn    scout     -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    sg55      -1.0       -1.0         0.85          0.85        0.8         -1.0
      spread_wpn    awp       -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    g3sg1     -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    famas     -1.0       -1.0         0.85          0.75        0.65        -1.0
      spread_wpn    m4a1      -1.0       -1.0         0.85          0.65        0.55        -1.0
      spread_wpn    aug       -1.0       -1.0         0.85          0.9         0.8         -1.0
      spread_wpn    sg550     -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    glock     -1.0       -1.0         0.85          0.6         0.5         -1.0
      spread_wpn    usp       -1.0       -1.0         0.85          0.7         0.6         -1.0
      spread_wpn    p228      -1.0       -1.0         0.85          0.6         0.5         -1.0
      spread_wpn    deagle    -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    elites    -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    fn57      -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    mac10     -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    tmp       -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    mp5       -1.0       -1.0         0.85          0.8         0.7         -1.0
      spread_wpn    ump45     -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    p90       -1.0       -1.0         0.85         -1.0        -1.0         -1.0
      spread_wpn    m249      -1.0       -1.0         0.85         -1.0        -1.0         -1.0

      spread_deadCenterFirstShot "1"

      // Percentage of each shot's original recoil to keep. 100 keeps default recoil; 0 removes it.
      spread_recoilMultiplier "100"
```
</details>
