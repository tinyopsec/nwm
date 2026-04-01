# nwm - Nano Window Manager

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Issues](https://img.shields.io/github/issues/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/issues)
[![Stars](https://img.shields.io/github/stars/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/stargazers)
[![C99](https://img.shields.io/badge/C-C99%20%2F%20POSIX-lightgrey.svg)]()
[![LOC](https://img.shields.io/badge/source-under%201000%20lines-brightgreen.svg)]()

**nwm** (Nano Window Manager) is a minimal, lightweight X11 window manager written in POSIX C99. Under 1000 lines across two files. No config files, no status bar, no runtime dependencies beyond Xlib. Tiling, floating, and monocle layouts. Tag-based workspaces. Compile-time configuration via `nwm.h`. A clean dwm alternative for users who want full control without the patch surface.

---

## Overview

nwm is a POSIX X11 window manager that follows the suckless philosophy: small, auditable, and configured at compile time. The full source fits in `nwm.c` and `nwm.h`. No external build system, no scripting language, no plugin API.

Key properties:

- Two source files, under 1000 lines of C99
- Pure Xlib - no xcb, no Cairo, no Pango
- Configured entirely in `nwm.h` before compilation
- Partial EWMH and full ICCCM compliance
- OpenBSD `pledge(2)` support; portable to FreeBSD

---

## Features

| Category | Details |
|---|---|
| Layouts | Tiling (master/stack), floating, monocle |
| Workspaces | 9 tag-based workspaces using bitmasks; multi-tag assignment |
| Mouse support | Move, resize, toggle float via modifier + button |
| Gaps | Configurable `gappx` on all sides, built in |
| Borders | Inactive, focused, and urgent colors - all compile-time |
| Tab switching | `Mod+Tab` toggles between last two tag views |
| Fullscreen | Toggle via keybind or `_NET_WM_STATE_FULLSCREEN` |
| Urgent hints | `XUrgencyHint` and `_NET_ACTIVE_WINDOW` handled |
| Auto-float | `_NET_WM_WINDOW_TYPE_DIALOG` windows float automatically |
| EWMH | `_NET_WM_STATE`, `_NET_ACTIVE_WINDOW`, `_NET_CLIENT_LIST`, `_NET_SUPPORTING_WM_CHECK` |
| ICCCM | `WM_DELETE_WINDOW`, `WM_TAKE_FOCUS`, `WM_NORMAL_HINTS`, `WM_HINTS` |
| BSD | OpenBSD `pledge(2)`; compiles on FreeBSD |
| Build | `gcc -std=c99 -pedantic -Wall -Wextra`, zero warnings |

---

## Screenshots

<!-- Add screenshots or a demo GIF to this section. -->
<!-- Suggested alt-text for accessibility and SEO: -->
<!-- ![nwm tiling window manager on Arch Linux with Nord color theme and two terminals tiled in master/stack layout](screenshots/nwm-tiling.png) -->
<!-- ![nwm monocle layout showing a single maximized window with minimal border](screenshots/nwm-monocle.png) -->

*Screenshots not yet added. To contribute one, open a pull request against this file.*

---

## Requirements

| Dependency | Arch package | Debian / Ubuntu package |
|---|---|---|
| Xlib | `libx11` | `libx11-dev` |
| C compiler | `gcc` or `clang` | `build-essential` |

No other runtime dependencies. `st` and `dmenu` are the default terminal and launcher; replace them in `nwm.h` before compiling.

**Supported platforms:**

- Linux (tested on Arch, Void, Debian)
- OpenBSD (with `pledge(2)`)
- FreeBSD (compiles; not regularly tested)

---

## Installation

### From source

```sh
git clone https://github.com/tinyopsec/nwm
cd nwm
make
sudo make install
```

Installs to `/usr/local/bin/nwm`. Edit `PREFIX` in the `Makefile` to change the path.

### AUR (Arch Linux)

```sh
yay -S nwm
# or
paru -S nwm
```

AUR package: [https://aur.archlinux.org/packages/nwm](https://aur.archlinux.org/packages/nwm)

### Uninstall

```sh
sudo make uninstall
```

---

## Usage

Add nwm to `~/.xinitrc`:

```sh
exec nwm
```

Start X:

```sh
startx
```

For display managers, create a session entry:

```ini
# /usr/share/xsessions/nwm.desktop
[Desktop Entry]
Name=nwm
Comment=Nano Window Manager - minimal tiling X11 WM
Exec=nwm
Type=Application
```

Print version and exit:

```sh
nwm -v
```

---

## Key Bindings

| Key | Action |
|---|---|
| `Mod + Return` | Spawn terminal |
| `Mod + d` | Spawn dmenu |
| `Mod + j / k` | Focus next / previous window |
| `Mod + h / l` | Shrink / grow master area |
| `Mod + i / o` | Increase / decrease master count |
| `Mod + Space` | Promote focused window to master |
| `Mod + Tab` | Toggle between last two tags |
| `Mod + t` | Tiling layout |
| `Mod + f` | Floating layout |
| `Mod + m` | Monocle layout |
| `Mod + F11` | Toggle fullscreen |
| `Mod + Shift + Space` | Toggle floating |
| `Mod + q` | Kill focused window |
| `Mod + 1-9` | Switch to tag |
| `Mod + Shift + 1-9` | Move window to tag |
| `Mod + 0` | View all tags |
| `Mod + Shift + 0` | Assign window to all tags |
| `Mod + Shift + e` | Quit nwm |

**Mouse bindings** (modifier key held over a client window):

| Button | Action |
|---|---|
| `Mod + Button1` | Move window |
| `Mod + Button2` | Toggle floating |
| `Mod + Button3` | Resize window |

Dragging or resizing a tiled window past the snap threshold auto-floats it.

All bindings are defined in `keys[]` and `buttons[]` in `nwm.h` and are freely remappable.

---

## Configuration

nwm has no runtime configuration files. All settings live in `nwm.h` and take effect after recompilation:

```sh
$EDITOR nwm.h
make && sudo make install
```

### Appearance

| Option | Default | Description |
|---|---|---|
| `borderpx` | `2` | Border width in pixels |
| `gappx` | `8` | Gap between windows and screen edges |
| `col_nborder` | `#3b4252` | Inactive border color (Nord Polar Night) |
| `col_sborder` | `#88c0d0` | Focused border color (Nord Frost) |
| `col_uborder` | `#ebcb8b` | Urgent window border color (Nord Aurora) |

### Behavior

| Option | Default | Description |
|---|---|---|
| `mfact` | `0.55` | Master area ratio (0.05-0.95) |
| `nmaster` | `1` | Initial number of master windows |
| `snap` | `16` | Edge snap distance in pixels |
| `attachbottom` | `0` | Attach new windows at bottom of stack |
| `focusonopen` | `1` | Focus newly opened windows |

### Modifier key

```c
#define MODKEY Mod1Mask   /* Alt */
#define MODKEY Mod4Mask   /* Super / Win */
```

### Terminal and launcher

```c
static const char *termcmd[]  = { "st", NULL };
static const char *dmenucmd[] = { "dmenu_run", NULL };
```

Replace with any terminal or launcher: `"alacritty"`, `"foot"`, `"rofi"`, etc.

---

## Development Philosophy

nwm follows the [suckless philosophy](https://suckless.org/philosophy/):

- **Auditable by design.** Under 1000 lines of C99. No hidden complexity. The entire codebase fits in one reading session.
- **Compile-time configuration.** No config file parser, no IPC socket, no scripting runtime. Edit `nwm.h`, recompile.
- **Zero dependencies.** Pure Xlib. No xcb, no Cairo, no external libraries.
- **No feature creep.** Features are added by patching the source, not by enabling runtime options.

nwm is inspired by [dwm](https://dwm.suckless.org) but departs from it in several concrete ways:

- Deterministic tiling algorithm with no pixel drift or remainder accumulation
- Built-in gap support via `gappx` without requiring a patch
- Two-slot XOR system for both layouts and tags - `Mod+Tab` is consistent
- OpenBSD `pledge(2)` in the default build

---

## Contributing

Bug reports and patches are welcome via [GitHub issues](https://github.com/tinyopsec/nwm/issues) and pull requests.

Code style requirements:

- No comments in production code
- No external dependencies
- No over-abstraction or wrapper layers
- Compiles clean: `gcc -std=c99 -pedantic -Wall -Wextra`
- Total source stays under 1000 lines

Optional features behind `#ifdef` or compile-time constants are considered. Changes to the core event loop are reviewed carefully.

---

## License

MIT. See [LICENSE](LICENSE) for details.

---

## Related Links

| Resource | URL |
|---|---|
| dwm (inspiration) | https://dwm.suckless.org |
| suckless.org | https://suckless.org |
| st terminal | https://st.suckless.org |
| dmenu | https://tools.suckless.org/dmenu/ |
| EWMH specification | https://specifications.freedesktop.org/wm-spec/latest/ |
| ICCCM specification | https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html |
| Xlib manual | https://www.x.org/releases/current/doc/libX11/libX11/libX11.html |
| Nord color palette | https://www.nordtheme.com |
| AUR package | https://aur.archlinux.org/packages/nwm |
