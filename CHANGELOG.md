# Changelog

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
