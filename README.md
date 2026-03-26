# nwm

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/issues)
[![GitHub stars](https://img.shields.io/github/stars/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/stargazers)

A minimal tiling window manager for X11, inspired by dwm. nwm follows the philosophy of small, auditable code with configuration done entirely at compile time.

## Features

* Tiling, floating, and monocle layout modes
* Tag-based workspace system (9 tags)
* Mouse support for moving, resizing, and toggling float
* Fully configurable keybindings and mouse buttons
* Urgent window highlighting
* No status bar, no font rendering, no bloat
* Under 1000 lines of C

## Installation

Install the X11 development headers, then build and install:

```
make
sudo make install
```

To uninstall:

```
sudo make uninstall
```

## Configuration

All configuration is done in `nwm.h` before compiling. No runtime config files are used.

**Appearance**

* `borderpx` — border width in pixels
* `gappx` — gap size between windows
* `col_nborder` — normal border color
* `col_sborder` — focused border color
* `col_uborder` — urgent border color

**Behavior**

* `mfact` — master area size ratio (0.05–0.95)
* `nmaster` — number of windows in the master area
* `snap` — snap distance to screen edges in pixels
* `attachbottom` — attach new windows at the bottom of the stack
* `focusonopen` — focus newly opened windows
* `resizehints` — respect application size hints in tiled mode

**Bindings**

* `MODKEY` — modifier key (`Mod1Mask` for Alt, `Mod4Mask` for Super)
* `keys[]` — keyboard shortcuts
* `buttons[]` — mouse button actions
* `termcmd[]` — terminal emulator command
* `dmenucmd[]` — launcher command

## Key Bindings

| Key | Action |
|---|---|
| Mod + Enter | Spawn terminal |
| Mod + d | Spawn dmenu |
| Mod + j / k | Focus next / previous window |
| Mod + h / l | Shrink / grow master area |
| Mod + i / o | Increase / decrease master count |
| Mod + Space | Promote window to master |
| Mod + Tab | Toggle between last two tags |
| Mod + t | Tile layout |
| Mod + f | Float layout |
| Mod + m | Monocle layout |
| Mod + F11 | Toggle fullscreen |
| Mod + Shift + Space | Toggle floating |
| Mod + q | Kill focused window |
| Mod + 1..9 | View tag |
| Mod + Shift + 1..9 | Move window to tag |
| Mod + 0 | View all tags |
| Mod + Shift + e | Quit nwm |

## Mouse Bindings

All mouse actions are performed while holding the modifier key over a client window.

| Button | Action |
|---|---|
| Mod + Button1 | Move window |
| Mod + Button2 | Toggle floating |
| Mod + Button3 | Resize window |

## License

MIT License. See LICENSE for details.

## Acknowledgements

nwm is heavily inspired by [dwm](https://dwm.suckless.org) by the suckless community.
