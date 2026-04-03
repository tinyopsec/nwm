# swm - Simple Window Manager

> **Note:** Project was renamed from `nwm` to `swm` (Simple Window Manager) in April 2026.

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Issues](https://img.shields.io/github/issues/tinyopsec/swm.svg)](https://github.com/tinyopsec/swm/issues)
[![Stars](https://img.shields.io/github/stars/tinyopsec/swm.svg)](https://github.com/tinyopsec/swm/stargazers)
[![C99](https://img.shields.io/badge/C-C99%20%2F%20POSIX-lightgrey.svg)]()
[![LOC](https://img.shields.io/badge/source-under%201000%20lines-brightgreen.svg)]()

**swm** (Simple Window Manager) is a minimal, lightweight X11 window manager written in POSIX C99. Under 1000 lines across two files. No config files, no status bar, no runtime dependencies beyond Xlib. Tiling, floating, and monocle layouts. Tag-based workspaces. Compile-time configuration via `swm.h`. A clean dwm alternative for users who want full control without the patch surface.

---

## Overview

swm is a POSIX X11 window manager that follows the suckless philosophy: small, auditable, and configured at compile time. The full source fits in `swm.c` and `swm.h`. No external build system, no scripting language, no plugin API.

Key properties:

- Two source files, under 1000 lines of C99
- Pure Xlib - no xcb, no Cairo, no Pango
- Configured entirely in `swm.h` before compilation
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
![Tiling Screenshot](https://raw.githubusercontent.com/tinyopsec/swm-assets/main/screenshots/tiling.png)

---

## Requirements

| Dependency | Arch package | Debian / Ubuntu package |
|---|---|---|
| Xlib | `libx11` | `libx11-dev` |
| C compiler | `gcc` or `clang` | `build-essential` |

No other runtime dependencies. `st` and `dmenu` are the default terminal and launcher; replace them in `swm.h` before compiling.

**Supported platforms:**

- Linux (tested on Arch, Void, Debian)
- OpenBSD (with `pledge(2)`)
- FreeBSD (compiles; not regularly tested)

---

## Installation

### From source

```sh
git clone https://github.com/tinyopsec/swm
cd swm
make
sudo make install
```

Installs to `/usr/local/bin/swm`. Edit `PREFIX` in the `Makefile` to change the path.

### AUR (Arch Linux)

```sh
yay -S swm
# or
paru -S swm
```

AUR package: [https://aur.archlinux.org/packages/swm](https://aur.archlinux.org/packages/swm)

### Uninstall

```sh
sudo make uninstall
```

---

## Usage

Add swm to `~/.xinitrc`:

```sh
exec swm
```

Start X:

```sh
startx
```

For display managers, create a session entry:

```ini
# /usr/share/xsessions/swm.desktop
[Desktop Entry]
Name=swm
Comment=Nano Window Manager - minimal tiling X11 WM
Exec=swm
Type=Application
```

Print version and exit:

```sh
swm -v
```

---

## Key Bindings

Default modifier is **Super (Win)**. To switch to Alt, change to `Mod1Mask` in `swm.h`.

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
| `Mod + Shift + e` | Quit swm |

**Mouse bindings** (modifier key held over a client window):

| Button | Action |
|---|---|
| `Mod + Button1` | Move window |
| `Mod + Button2` | Toggle floating |
| `Mod + Button3` | Resize window |

Dragging or resizing a tiled window past the snap threshold auto-floats it.

All bindings are defined in `keys[]` and `buttons[]` in `swm.h` and are freely remappable.

---

## Configuration

swm has no runtime configuration files. All settings live in `swm.h` and take effect after recompilation:

```sh
$EDITOR swm.h
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

swm follows the [suckless philosophy](https://suckless.org/philosophy/):

- **Auditable by design.** Under 1000 lines of C99. No hidden complexity. The entire codebase fits in one reading session.
- **Compile-time configuration.** No config file parser, no IPC socket, no scripting runtime. Edit `swm.h`, recompile.
- **Zero dependencies.** Pure Xlib. No xcb, no Cairo, no external libraries.
- **No feature creep.** Features are added by patching the source, not by enabling runtime options.

swm is inspired by [dwm](https://dwm.suckless.org) but departs from it in several concrete ways:

- Deterministic tiling algorithm with no pixel drift or remainder accumulation
- Built-in gap support via `gappx` without requiring a patch
- Two-slot XOR system for both layouts and tags - `Mod+Tab` is consistent
- OpenBSD `pledge(2)` in the default build

---

## Contributing

Bug reports and patches are welcome via [GitHub issues](https://github.com/tinyopsec/swm/issues) and pull requests.

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
| AUR package | https://aur.archlinux.org/packages/swm |

