# Quake II RTX Head Tracking

![Quake II RTX running with this mod](https://raw.githubusercontent.com/itsloopyo/quake-ii-rtx-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Quake II RTX that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; your mouse or controller still aims
- **6DOF positional tracking** - lean and peek with head position, with the lean stopped at the wall instead of going through it
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- A legitimately installed copy of
  [Quake II RTX](https://store.steampowered.com/app/1089130/Quake_II_RTX/) on
  Steam. This release supports Quake II RTX 1.8.0, the Steam build dated
  2025-03-26; `QuakeIIRTXHeadTracking.log` names the build it matched as
  `steam-win64-20250326`.
- A tracking source that sends the OpenTrack UDP protocol:
  [OpenTrack](https://github.com/opentrack/opentrack/releases) with a webcam,
  headset or other supported device, or a phone app that sends it directly.
- Windows 10 or 11, 64-bit.

The mod checks which build of `q2rtx.exe` it has been loaded into before it
touches anything. On a build it does not recognize it installs no hooks at all,
writes a line to its log saying so, and the game runs exactly as it does
without the mod.

## Installation

1. Download `QuakeIIRTXHeadTracking-v<version>-installer.zip` from the
   [Releases](https://github.com/itsloopyo/quake-ii-rtx-headtracking/releases)
   page.
2. Extract it anywhere.
3. Double-click `install.cmd`. It finds your Quake II RTX install, drops the
   Ultimate ASI Loader (`winmm.dll`) and `QuakeIIRTXHeadTracking.asi` next to
   `q2rtx.exe`, and writes a default `HeadTracking.ini`.
4. Set OpenTrack's output to UDP over network, host `127.0.0.1`, port `4242`.
5. Launch the game.

If the installer cannot find your game, pass the install folder as an
argument:

```powershell
install.cmd "D:\Games\Quake II RTX"
```

Or set the `QUAKE_II_RTX_PATH` environment variable to the install folder and
run `install.cmd` with no argument:

```powershell
$env:QUAKE_II_RTX_PATH = "D:\Games\Quake II RTX"
install.cmd
```

### Manual Installation

Everything comes out of the same installer ZIP. Copy three files into the Quake
II RTX install root, next to `q2rtx.exe`:

- `vendor\ultimate-asi-loader\dinput8.dll`, **renamed to `winmm.dll`**. This is
  the Ultimate ASI Loader, and `winmm.dll` is the name `q2rtx.exe` loads it
  under. Copied without renaming it, nothing happens.
- `plugins\QuakeIIRTXHeadTracking.asi`, the mod itself.
- `plugins\HeadTracking.ini`, the config file. The mod writes a default one if
  it is missing, so this step only matters if you want to edit settings before
  the first launch.

Mod managers do not install this mod. The payload has to sit in the game root
beside the exe, and a manager deploys into one fixed subfolder of the game, so
a manager install would put the files somewhere the loader never looks, load
nothing, and still report success. Use `install.cmd`, or copy the three files
by hand.

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimeters, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### VR Headset Setup

If OpenTrack can take your headset's pose as an input, the output settings above
are all this mod needs from it. Get the headset running the way its own software
wants first. Nobody has tested that route against this mod, so treat it as a
starting point rather than a procedure.

### Webcam Setup

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam, with no
markers and no IR hardware. Select it under **Input**, pick your camera in its
settings, and use the output settings above. How well it tracks depends on your
camera and your lighting, so try it before buying anything.

### Phone App Setup

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Not every phone tracker does, so check yours for
an OpenTrack or UDP output option first. I made [Headcam](https://headcam.app)
so decent tracking was free for anybody with a phone already in their pocket; it
filters on-device before it sends.

What decides the wiring is how much filtering the app does before the packet
leaves the phone. An app that filters on-device can send direct: point it at
this PC's LAN address (run `ipconfig` to find it) on port `4242`. The mod smooths
what arrives with the two values under `[Rotation]` and does not filter the
signal beyond that, so how steady the direct route looks depends mostly on how
much the app does before it sends.

Try direct first, then hold your head still and watch the view. If it drifts or
shakes, point the app at OpenTrack's **UDP over network** *input* on some other
port, say 5252, and let OpenTrack's filters and curves clean the feed up before
its output forwards to `127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Centering

Centering belongs to your tracker. The mod applies the pose it receives exactly
as it arrives, so a stream of zeros holds the view where the game itself puts
it. Press the center control in your tracker (OpenTrack's **Center** bind, the
CENTER button in Headcam, SteamVR's reset) and the view sits centered.

## Controls

Two equivalent binding sets, use whichever your keyboard has:

| Action | Nav-cluster | Chord |
|--------|-------------|-------|
| Toggle tracking | `End` | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up` | `Ctrl+Shift+G` |
| Toggle yaw mode (world / camera-local) | `Page Down` | `Ctrl+Shift+H` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

## Configuration

`HeadTracking.ini` sits next to `q2rtx.exe` and is read at launch.

```ini
; Quake II RTX Head Tracking configuration
; Send OpenTrack UDP output to 127.0.0.1:4242 (Output: UDP over network).

[General]
Enabled=1
UdpPort=4242

[Rotation]
YawSensitivity=1
PitchSensitivity=1
RollSensitivity=1
InvertYaw=0
InvertPitch=0
InvertRoll=0
; Smoothing is picked per connection from the tracker's source address
; and covers rotation and position. 0 = none, 1 = heavy.
; LocalSmoothing: tracker runs on this machine (loopback)
LocalSmoothing=0
; RemoteSmoothing: tracker is a remote device on the network
RemoteSmoothing=0.15
; WorldSpaceYaw: head yaw about world up, so the horizon stays level however
; far the mouse is pitched. 0 turns it about the camera's own up axis instead.
; Page Down switches it in game.
WorldSpaceYaw=1

[Position]
Enabled=1
SensitivityX=1
SensitivityY=1
SensitivityZ=1
; How far the eye may leave the body, in meters. 0.01 - 0.5.
LimitX=0.3
LimitY=0.2
LimitZ=0.4
LimitZBack=0.1
; Position uses the [Rotation] LocalSmoothing / RemoteSmoothing values
; Quake units per metre (vieworg is in Quake units, ~1 per inch).
; This converts units. It does not set how far you lean - the
; Limit values above do that, and your tracker sets the rest.
UnitsPerMeter=40

[Collision]
; Stops a lean putting the view inside a wall. Rotation is unaffected.
CollisionEnabled=1
; Quake units the eye is held off a blocking surface
CollisionMargin=4
; How fast the lean reopens once the obstruction clears (0 = instant)
CollisionReleaseSmoothing=0.9

[Controls]
; Virtual key codes in hex. Chord alternatives (no edit needed):
;   Toggle Ctrl+Shift+Y, Cycle tracking mode Ctrl+Shift+G,
;   Toggle yaw mode Ctrl+Shift+H
ToggleKey=0x23
TogglePositionKey=0x21
ToggleYawKey=0x22
```

Smoothing is the two keys under `[Rotation]`, picked automatically per
connection from the tracker's source address. Both cover rotation and position.

| Key | Default | Range | Applies to |
|-----|---------|-------|------------|
| `LocalSmoothing` | 0.0 | 0.0-1.0 | Tracker running on this machine (loopback) |
| `RemoteSmoothing` | 0.15 | 0.0-1.0 | Tracker on a remote network device, such as a phone |

A value that is not a number, uses a comma for the decimal point, or falls
outside the range the setting allows is corrected and named on a `config:` line
in `QuakeIIRTXHeadTracking.log`, which says what it used instead. That log line
is the answer to "why did my edit not change anything".

Field of view stays with the game: its own **"field of view"** slider is in the
video menu, 60 to 160 degrees, and it is the `fov` console variable if you
prefer typing it. Head tracking is measured against whatever you set there, so
moving the slider does not change how far your head moves the picture. What it
covers is the moments the game renders a field of view you did not pick, where
the same head movement would otherwise sweep further across the screen for as
long as that lasts. The `[fov]` line in the log records the values it read and
the factor it derived from them; in ordinary play it reads `1.0000`.

## Troubleshooting

**Mod not loading**

- Check `QuakeIIRTXHeadTracking.log` next to `q2rtx.exe`. It records loader
  attach, the matched build profile, the UDP port, and the hook status.
- "no matching build profile" means the mod does not recognize this build of
  `q2rtx.exe`. The same log line says which of three cases it is: the game is
  newer than the mod knows about (grab a newer mod build from Releases), the
  game is older (let Steam finish updating), or the exe has been modified, which
  the mod will not engage on. All three leave the game running vanilla.
- The log is written fresh every launch and the launch before it is kept as
  `QuakeIIRTXHeadTracking.prev.log`, which is the one to send if the game
  crashed and you relaunched.

**No tracking response**

- Confirm OpenTrack is started and its output is UDP over network to
  `127.0.0.1:4242`. The log gets a `[udp] First UDP packet received` line the
  moment the tracker reaches the game.
- A `[udp] Failed to bind UDP port` line carries the reason the OS gave. When
  that reason is another program holding 4242, closing that program is the whole
  fix: the mod retries every half second and starts tracking within about a
  second of the port freeing up, with no restart.
- In a deathmatch or coop game there is no tracking by design. The mod checks
  every frame, so returning to a single-player game brings it back without a
  restart.
- By design, tracking stops while the console, a menu or a message prompt has
  the keyboard, and resumes when you return to the game.
- By design, the mod follows the first tracker it hears from and ignores a
  second one until the first goes quiet. Running OpenTrack and a phone app at
  once means one of them is doing nothing; the log says when a second sender
  turns up.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` for a phone or other network tracker, or
  `LocalSmoothing` for one running on this PC.
- If a direct phone feed is unsteady, point the app at OpenTrack's **UDP over
  network** input instead and let its filters and curves clean the feed up
  before OpenTrack forwards it to `127.0.0.1:4242`.

**Wrong rotation axis**

- Flip the matching `InvertYaw`, `InvertPitch` or `InvertRoll` flag. Pitch is
  the one that is reversed on some trackers.
- If yaw feels wrong when looking steeply up or down, toggle between
  world-locked and camera-local yaw with `Page Down`. World-locked (the default)
  keeps yaw on the horizon; camera-local follows the camera's current up axis,
  which leans the view at extreme pitch.
- If the view sits off to one side, center it in your tracker app.

**Leaning still puts the view through a wall**

- The lean clamp is the `[Collision]` block in `HeadTracking.ini`.
  `CollisionEnabled=1` turns it on, `CollisionMargin` is how many Quake units
  the eye is held off a blocking surface, and `CollisionReleaseSmoothing` is how
  quickly the lean reopens once you step clear.
- The log says what it is doing. `[lean] collision clamp` records whether it is
  on and at what standoff, and `[lean] contact=1` appears the moment a lean is
  being cut short. `queryFailed=1` means the clamp could not ask the level
  whether the path was clear and let the lean through unclamped.
- Raise `CollisionMargin` if you can still see through a surface you have leaned
  into.

**Other things you may see**

- The crosshair vanishes on a big head turn: the point your shot will reach has
  left the screen, so there is nothing on screen to mark. It comes back as the
  aim point does.
- The gun swings out of frame on a hard sideways lean. It is held by your
  body, and the view has moved 30 cm away from it, so it slides across and then
  off the edge of the screen. It comes back as you return to center.
- The game window moves when you launch windowed: once the game has finished
  placing its window, the mod centers it on the work area of the monitor it
  opened on. A window the game centered itself, and a fullscreen or borderless
  one that already fills the screen, are left where they are.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod, your `HeadTracking.ini` and the mod's
logs. The Ultimate ASI Loader is only removed if the installer put it there. Use
`uninstall.cmd /force` to remove it anyway. Copy `HeadTracking.ini` somewhere
else first if you want your settings back afterwards.

## Building from Source

Requires CMake, Visual Studio with the C++ workload, and
[pixi](https://pixi.sh).

```powershell
git clone --recursive https://github.com/itsloopyo/quake-ii-rtx-headtracking
cd quake-ii-rtx-headtracking
pixi run build-release
pixi run deploy-release
```

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- [**id Software**](https://www.idsoftware.com) and [**NVIDIA Lightspeed
  Studios**](https://www.nvidia.com/en-us/geforce/campaigns/quake-II-rtx/) for
  Quake II and Quake II RTX.
- [**ThirteenAG**](https://github.com/ThirteenAG) for the [Ultimate ASI
  Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
- [**Tsuda Kageyu**](https://github.com/TsudaKageyu) for
  [MinHook](https://github.com/TsudaKageyu/minhook).
- The [**OpenTrack**](https://github.com/opentrack/opentrack) project.
- Built on [**cameraunlock-core**](https://github.com/itsloopyo/cameraunlock-core).

Third-party licenses are recorded in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by id Software,
NVIDIA, or Bethesda. Use at your own risk.
