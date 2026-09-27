# Changelog

## [Unreleased]

### Changed

- Settings move to `CameraUnlock.ini`, beside `q2rtx.exe`. Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `HeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale or axis inversion you changed from its default: `YawSensitivity`, `PitchSensitivity`, `RollSensitivity`, `InvertYaw`, `InvertPitch`, `InvertRoll`, `[Position] SensitivityX/Y/Z` and `UnitsPerMeter`. Set these in your tracker instead.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. `TogglePositionKey` is now `CycleTrackingModeKey` and `ToggleYawKey` is `YawModeKey`, both under `[Hotkeys]`.
- Several settings have the fleet's names and sections: `[General] Enabled` is `EnableOnStartup`, `[Position] Enabled` is the tracking mode pair `RotationEnabled` and `PositionEnabled`, the smoothing values sit under `[Smoothing]`, `WorldSpaceYaw` under `[General]`, the lean limits are `PositionLimitX`, `PositionLimitZ` and `PositionLimitZBack`, and the collision settings sit under `[Position]`. The one vertical lean limit is now two, `PositionLimitY` upward and `PositionLimitYDown` downward, and an old `LimitY` is imported into both.
- The tracking mode the mode hotkey picks and the yaw mode the yaw hotkey picks are saved in `CameraUnlock.ini` and come back at the next start. `End` still changes the session only.
- Uninstalling leaves `CameraUnlock.ini` and `HeadTracking.ini` in place, so a reinstall keeps your settings. Earlier versions removed `HeadTracking.ini`.
- The installer and the launcher no longer copy a config file into the game folder. The mod creates `CameraUnlock.ini` the first time it starts.

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Removed

- The sensitivity, scale and axis inversion settings. Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did before.

## [0.0.0] - 2026-09-06

### Added
- Initial release.
- Head tracking for Quake II RTX. Your head moves the view; your mouse or
  controller still aims, and shots go where you are aiming rather than where you
  are looking.
- Lean and peek with head position as well as rotation, with the lean stopped at
  the wall instead of putting the view inside it. `[Collision] CollisionEnabled`,
  `CollisionMargin` and `CollisionReleaseSmoothing` tune that.
- The game's crosshair is drawn on the point the shot will reach, so it stays on
  target at any range and under any lean, and is hidden rather than guessed at
  when that point is off screen.
- `End` toggles tracking, `Page Up` cycles between full tracking, rotation only
  and position only, and `Page Down` switches head yaw between world-locked and
  camera-local. `Ctrl+Shift+Y`, `Ctrl+Shift+G` and `Ctrl+Shift+H` do the same on
  a keyboard with no navigation cluster.
- Head tracking is measured against the field of view you set, so the game's own
  FOV slider does not change how far your head moves the picture, and a field of
  view the game renders for itself does not exaggerate it.
- Tracking is suppressed outside single-player gameplay: in menus, on loading
  screens, while the console has the keyboard, in coop or deathmatch, and on the
  end-of-level intermission, where the camera has been parked somewhere that is
  no longer yours to move.
- A windowed game is centered on the monitor it opened on, once the engine has
  finished placing its window. A window the game already centered, and a
  fullscreen or borderless one, are left where they are.
- Settings live in `HeadTracking.ini` next to the game. A value that is not a
  number, uses a comma for the decimal point, or falls outside the range the
  setting allows is corrected and named in the log rather than accepted
  silently.
- `QuakeIIRTXHeadTracking.log` next to the game records the tracker connection,
  the camera and where the crosshair was placed, and the previous launch is kept
  as `QuakeIIRTXHeadTracking.prev.log` so a crash does not erase the session
  worth reading.
- On a build of the game the mod does not recognize it installs nothing at all
  and the game runs vanilla.
- Centering is left to your tracker. There is no recenter key in the mod: center
  in OpenTrack, Headcam or SteamVR and the mod applies the pose it receives.
