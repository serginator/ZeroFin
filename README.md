# ZeroFin

<p align="center">
  <img src="assets/images/banner.jpg" width="720" alt="ZeroFin: a non-official Jellyfin client for the Cardputer Zero">
</p>

A video client for [Jellyfin](https://jellyfin.org) on the M5Stack Cardputer Zero (320x170 screen, 46-key keyboard).
Not an official Jellyfin app.

Status: in development. Login, browsing with posters, search and the player work.

## Build and test on a computer

```sh
cmake -S . -B build -DZEROFIN_BUILD_APP=OFF   # library and unit tests only
cmake --build build && ctest --test-dir build
```

## Simulator (macOS, SDL2)

```sh
brew install cmake sdl2 freetype
cmake -S . -B build-app -DZEROFIN_BUILD_TESTS=OFF
cmake --build build-app
ZEROFIN_DEMO=1 ./build-app/src/app/zerofin     # a fake server, no network needed
```

Keys in the simulator: arrows, Enter, Esc, Tab, Backspace, typing. F1-F5 are the media keys.

## Home screen and posters

The home screen starts with a row of tiles (one per library, then Search and Settings). Under it are rows of
posters: Keep watching, Next up, and Recent in each movie, TV or collection library. Up and Down change the row.
Left and Right move inside a row. Enter opens the item. The library, series and season lists and Search show the
posters in a grid of four columns.

Posters are small JPEG files (64x96). One download runs at a time, on its own worker. They are kept in
`$ZEROFIN_HOME/cache/images` (default `~/.config/ZeroFin/cache/images`, up to 50 MB, the oldest files go first).
Delete that folder at any time. A title with no poster shows a grey card with its first letter.

## Player keys

Media keys (fn+Q, fn+W, fn+E, fn+S, fn+D) always act. Any other key opens the controls. F, X, Z and C also work as
arrows in every screen without a text field (home, lists, detail, settings). The other letters below work in the
player only. Screens with a text field (address, search, password) type the letters as text.

| Key | Action |
|---|---|
| F / X / Z / C | up / down / left / right (move between the control icons) |
| Q | play / pause |
| S / D | volume down / up (the device volume, shared with the OS) |
| A | mute (the device mute switch) |
| W / E | back 15 s / forward 15 s (act at once, controls can stay hidden) |
| fn+W / fn+E | previous / next episode |
| Enter | press the focused icon |
| Tab | open the menu (Audio, Subs, and with a monitor Quality, Sound, Video). Same as the gear icon: move to it with the arrows and press Enter |
| Esc | hide the controls, then leave the player |

## Playing on a monitor (HDMI)

Plug a monitor into the HDMI port. The next video plays on the monitor, and the LCD shows the title and the controls
(always visible; the keys are the same). With no monitor, or with "Video output: LCD only", the video plays on the
LCD as before.

- **Quality:** 720p by default. 1080p is available and may drop frames on hard scenes (the CP0 decodes in software).
  A file that is 1280x720 or smaller and 6 Mbps or less plays direct. Larger files are transcoded by the server to
  the chosen size. For 1080p ZeroFin asks for 8 Mbps, because Jellyfin lowers the picture below that.
- **Sound:** goes to the monitor (ALSA `vc4hdmi`). A monitor with no speakers still reports that it accepts audio,
  so the CP0 cannot tell. Choose "CP0 speaker" in that case. With monitor sound, the volume keys change the player
  volume. The monitor has its own volume knob.
- **Where to change it:** Settings, "Monitor (HDMI)" (video output, quality, sound). While a video plays, open the gear
  icon (arrows and Enter, or Tab). The menu has tabs: Audio, Subs, Quality, Sound, Video. Left and Right (or Tab) change
  the tab, Up and Down move in the list, Enter picks, Esc closes. The menu opens on the tab you used last. Quality and
  video output restart the video at the same position. Sound changes at once.
- **If the monitor fails or the cable is pulled,** the video continues on the LCD at the same position.
- `ZEROFIN_OUTPUT=lcd|auto|hdmi` overrides the setting (`hdmi` also works in the simulator, with no monitor).

## Server address

The address field starts empty. Type the address as you know it: an IP, a name, with or without `http://` and a port.
ZeroFin asks the likely addresses one after the other (3.5 s each) and keeps the first one that answers:

| You type | ZeroFin tries |
|---|---|
| `192.168.1.5` | `http://192.168.1.5:8096`, `http://192.168.1.5`, `https://192.168.1.5:8096`, `https://192.168.1.5` |
| `jellyfin.example.com` | `https://`, `http://`, then the same with `:8096` |
| `192.168.1.5:8096` | `http://` then `https://` |
| `https://jf.example.com` | that address, then the same on `:8096` |

## Typing symbols

All 95 printable ASCII characters can be typed in the address, search and password fields. Letters and digits work
as normal, and Shift makes capitals. The sym key gives the rest:

| sym + | gives |
|---|---|
| 1 2 3 4 5 6 7 8 9 0 | `! @ # $ % ^ & * ( )` |
| Q W E R T Y U I O P | ``~ ` _ - + = [ ] { }`` |
| A S D G H J K L | `; : ' " < > \ \|` |
| V B N M | `, . / ?` |

F, Z, X and C have no symbol, because they are the arrow keys. The app reads the codes that the CP0 keyboard driver
sends (the table is in `src/ui_core/keys.cpp`). A USB keyboard sends the standard US codes and works as well.

## Device (Cardputer Zero, native build)

```sh
cmake -S . -B build -DUSE_DESKTOP=OFF -DZEROFIN_BUILD_TESTS=OFF
cmake --build build -j4
./build/src/app/zerofin
```

Needs `cmake`, `g++`, `git` and `libfreetype-dev`. The first build compiles LVGL and takes about 20 minutes.

## Install on the Cardputer Zero (.deb)

```sh
tools/build-deb.sh            # needs Docker on an arm64 host; writes dist/zerofin_<version>_arm64.deb
scp dist/zerofin_*.deb user@cp0:
ssh user@cp0 sudo apt install ./zerofin_*.deb
```

The package puts the program in `/usr/share/APPLaunch/bin`, so ZeroFin appears in the launcher. It depends on
`libmpv2`, `libfreetype6` and `curl`. GitHub Actions builds the unit tests and the `.deb` on every push
(`.github/workflows/ci.yml`). `release-please` keeps `version.txt` and attaches the `.deb` to each release.

## Publish to the CardputerZero Store

After the release job has attached the `.deb`, one command sends it to the Store:

```sh
tools/publish-store.sh        # the latest release. Or: tools/publish-store.sh v1.1.0
```

It downloads the `.deb` from the release and takes the Store listing (`app-builder.json`, screenshots, icon)
from the same git tag. Then it runs `czdev publish`, which opens a PR in `CardputerZero/packages`. Updates of an
existing app merge there on their own. It needs `gh`, `git`, `czdev` (log in once with `czdev login`) and `dpkg-deb`
(macOS: `brew install dpkg`). `czdev` is in [m5stack/CardputerZero-AppBuilder](https://github.com/m5stack/CardputerZero-AppBuilder).

CI does not publish for you. `czdev login` saves a GitHub token with the `public_repo` scope, which can write to
all your public repositories. That is too much to keep as a repository secret.

## Developer settings (environment variables)

| Variable | Effect |
|---|---|
| `ZEROFIN_DEMO=1` | Use the built-in fake server. Quick Connect approves itself after a few polls. Password login: `demo` / `demo`. |
| `ZEROFIN_HOME=DIR` | Keep the settings in DIR instead of `~/.config/ZeroFin`. |
| `ZEROFIN_TRANSPORT=cli` | Run `/usr/bin/curl` for HTTP. The default on macOS, which blocks LAN access for locally built programs. |
| `ZEROFIN_HEADLESS=1` | Simulator without a window (SDL dummy driver). |
| `ZEROFIN_SCRIPT=...` | Play scripted keys: `wait:MS;idle:MS;key:NAME;text:STR;shot:NAME;quit`. With `ZEROFIN_SHOT_DIR`, `shot:` saves a PPM picture. |
| `ZEROFIN_KEYLOG=1` | Device: print the raw key codes. |
| `ZEROFIN_ASSETS=DIR` | Where to find `fonts/`. |
| `ZEROFIN_DEMO_DURATION=SECONDS` | Demo player: length of the fake video (default 5760). |
| `ZEROFIN_GRAB_KEYBOARD=1` | Device, for SSH test runs: take the keyboard from the launcher. Off by default, so the OS still sees the long Esc press. |
| `ZEROFIN_OUTPUT=lcd\|auto\|hdmi` | Where the video plays. `hdmi` forces the monitor layout (simulator tests). |
| `ZEROFIN_DRM_DIR=DIR` | Where to look for the monitor instead of `/sys/class/drm`: a folder with `card0-HDMI-A-1/status` holding `connected` or `disconnected`. Tests plugging with no cable. |
| `ZEROFIN_FB=PATH` | Framebuffer device (default `/dev/fb_lcd`, a link to `/dev/fb0`). |

`$ZEROFIN_HOME/profile.json` can override the playback numbers (direct play limit, transcode size and bitrate).
See `src/jellyfin/device_profile.h` for the keys.

## Layout

| Folder | Content |
|---|---|
| `src/jellyfin/` | Jellyfin client library, with no UI code |
| `src/storage/` | Settings file (`config.json`, mode 0600) and the poster cache |
| `src/ui_core/` | UI logic without LVGL: key map, list and grid cursors, card text, request queue, text editing, formatting |
| `src/app/` | The LVGL application: screens, widgets, platform code |
| `spike/` | The playback spike (libmpv into an LVGL canvas) |
| `tools/` | `jf_probe`: a live check of the client against a real server. `gen-demo-posters.sh` makes the demo posters (needs ffmpeg) |
| `tests/` | Unit tests (doctest, run with CTest) |

## License

MIT. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
