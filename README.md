# nwm

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/issues)
[![GitHub stars](https://img.shields.io/github/stars/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/stargazers)

A minimal X11 window manager. Two files. Under 1000 lines of POSIX C.

nwm is configured entirely at compile time, no config files, no runtime dependencies beyond Xlib, no status bar. The full codebase fits in `nwm.c` and `nwm.h`, making it easy to read, audit, and modify.

---

## Features

* Tiling, floating, and monocle layouts
* Tag-based workspaces (9 tags)
* Mouse support: move, resize, toggle float
* Urgent window highlighting with a distinct border color
* Compile-time color palette: `#1e1e1e` / `#7c9e7e` / `#c47f50`
* No status bar, no font rendering, no bloat
* POSIX-compliant, under 1000 lines of C

---

## Quick Start

**Dependency:** Xlib (`libx11`)

```sh
git clone https://github.com/tinyopsec/nwm
cd nwm
make
sudo make install
```

**AUR (Arch Linux):**

```sh
yay -S nwm
```

**Uninstall:**

```sh
sudo make uninstall
```

---

## Configuration

Edit `nwm.h` before compiling. There are no runtime config files.

**Appearance**

* `borderpx` — border width in pixels
* `gappx` — gap between windows
* `col_nborder` — inactive border color
* `col_sborder` — focused border color
* `col_uborder` — urgent border color

**Behavior**

* `mfact` — master area ratio (0.05–0.95)
* `nmaster` — number of master windows
* `snap` — edge snap distance in pixels
* `attachbottom` — attach new windows at stack bottom
* `focusonopen` — focus newly opened windows
* `resizehints` — respect application size hints in tiled mode

**Bindings**

* `MODKEY` — modifier key (`Mod1Mask` = Alt, `Mod4Mask` = Super)
* `keys[]` — keyboard shortcuts
* `buttons[]` — mouse actions
* `termcmd[]` — terminal command
* `dmenucmd[]` — launcher command

After any change, recompile:

```sh
make && sudo make install
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
| `Mod + Space` | Promote window to master |
| `Mod + Tab` | Toggle between last two tags |
| `Mod + t` | Tiling layout |
| `Mod + f` | Floating layout |
| `Mod + m` | Monocle layout |
| `Mod + F11` | Toggle fullscreen |
| `Mod + Shift + Space` | Toggle floating |
| `Mod + q` | Kill focused window |
| `Mod + 1–9` | Switch to tag |
| `Mod + Shift + 1–9` | Move window to tag |
| `Mod + 0` | View all tags |
| `Mod + Shift + e` | Quit nwm |

## Mouse Bindings

All actions require holding the modifier key over a client window.

| Button | Action |
|---|---|
| `Mod + Button1` | Move window |
| `Mod + Button2` | Toggle floating |
| `Mod + Button3` | Resize window |

---

## Contributing

The codebase is small enough to read in one sitting. Bug reports, patches, and improvements are welcome via GitHub issues or pull requests. Keep changes minimal and in the spirit of the project.

---

## License

MIT. See [LICENSE](LICENSE) for details.

## Acknowledgements

nwm is inspired by [dwm](https://dwm.suckless.org) by the suckless community.
