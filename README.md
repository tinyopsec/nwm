<div align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=0E2426&height=80&section=header&text=nwm&fontSize=52&fontColor=59C6B5&fontAlignY=72&animation=fadeIn" alt="nwm header">
  <br>
  <img src="https://readme-typing-svg.demolab.com?font=JetBrains+Mono&size=14&pause=1200&color=59C6B5&center=true&vCenter=true&width=600&lines=Minimal+tiling+X11+window+manager;Small+source.+No+runtime+dependencies;Compile-time+configuration+only" alt="nwm typing">
  <br>
  <img src="https://raw.githubusercontent.com/tinyopsec/assets/main/nwm/nwm.webp" style="width: 65%; height: auto;" alt="logo">
</div>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-33ACB4?style=flat&labelColor=0E2426&logo=opensourceinitiative&logoColor=33ACB4" alt="license MIT">
  <img src="https://img.shields.io/badge/C99%20%2F%20POSIX-46B9B4?style=flat&label=lang&labelColor=0E2426&logo=c&logoColor=46B9B4" alt="C99 / POSIX">
  <img src="https://img.shields.io/badge/%7E2147%20lines-59C6B5?style=flat&label=source&labelColor=0E2426&logo=files&logoColor=59C6B5" alt="about 2147 lines">
  <img src="https://img.shields.io/badge/1.5.2-6CD4B5?style=flat&label=version&labelColor=0E2426&logo=tag&logoColor=6CD4B5" alt="version 1.5.2">
  &nbsp;
  <img src="https://img.shields.io/badge/Linux-7EE1B5?style=flat&labelColor=0E2426&logo=linux&logoColor=7EE1B5" alt="Linux">
  <img src="https://img.shields.io/badge/OpenBSD-91EEB6?style=flat&labelColor=0E2426&logo=openbsd&logoColor=91EEB6" alt="OpenBSD">
  <img src="https://img.shields.io/badge/FreeBSD-A4FBB6?style=flat&labelColor=0E2426&logo=freebsd&logoColor=A4FBB6" alt="FreeBSD">
  &nbsp;
  <img src="https://img.shields.io/badge/X11%20%2F%20Xlib-33ACB4?style=flat&label=display&labelColor=0E2426&logo=x.org&logoColor=33ACB4" alt="X11 / Xlib">
  <img src="https://img.shields.io/badge/make-46B9B4?style=flat&label=build&labelColor=0E2426&logo=gnu&logoColor=46B9B4" alt="make">
  <img src="https://img.shields.io/badge/AUR-59C6B5?style=flat&label=package&labelColor=0E2426&logo=archlinux&logoColor=59C6B5" alt="AUR">
  <img src="https://img.shields.io/badge/%7E3%20MB%20RAM-6CD4B5?style=flat&label=idle&labelColor=0E2426&logo=speedtest&logoColor=6CD4B5" alt="about 3 MB RAM">
  <img src="https://img.shields.io/badge/EWMH%20%2F%20ICCCM-7EE1B5?style=flat&label=spec&labelColor=0E2426&logo=xdg&logoColor=7EE1B5" alt="EWMH / ICCCM">
</p>

## Contents

- [Quick Start](#quick-start)
- [vs dwm](#vs-dwm)
- [Features](#features)
- [Screenshots](#screenshots)
- [Installation](#installation)
- [Usage](#usage)
- [Key Bindings](#key-bindings)
- [Configuration](#configuration)
- [Window Rules](#window-rules)
- [How It Works](#how-it-works)
- [Contributing](#contributing)
- [Star History](#star-history)
- [Related](#related)

---

## Quick Start

```sh
git clone https://github.com/tinyopsec/nwm
cd nwm
make && sudo make install
echo "exec nwm" >> ~/.xinitrc
startx
```

> [!NOTE]
> The default terminal is `st` and the default launcher is `dmenu_run`. Change them in `nwm.h` before compiling if you use something else (see [Configuration](#configuration)).
>
> For display managers, place a session file at `/usr/share/xsessions/nwm.desktop` (see [Usage](#usage)).

---

## vs dwm

`nwm` takes direct inspiration from dwm but diverges in a few concrete ways:

| Area | dwm | nwm |
|---|---|---|
| Lines of code | ~2901 total (.h / .c) | ~2147 (fits in one reading session) |
| RAM at idle | ~5–15 MB | ~3–8 MB on my PC (leaner process image) |
| Tiling arithmetic | Can accumulate pixel remainder | Integer division, remainder assigned to the last window |
| Gap support | Requires patching | Built in via `gappx` |
| Mod+Tab behavior | Inconsistent across patches | Deterministic XOR two-slot history |
| OpenBSD `pledge(2)` | Not supported | Supported natively |
| POSIX compliance | Uses GNU extensions in places | Strict C99 / POSIX orientation |
| Status bar | Built-in bar, requires patching to remove | No bar; use any external panel or none |
| Config complexity | ~100–150 lines of config + patch management | Single flat `nwm.h`, no patch stack |
| Audit surface | Large: bar, fonts, drawing code | Minimal: window management only |

dwm's bar and font rendering alone account for a significant portion of its codebase. `nwm` drops all of that. No drawing, no text, no color schemes beyond three border hex values.

If you already run a patched dwm, `nwm` is roughly what you end up with after applying gaps and pertag-like patches, except the behavior is defined once, not assembled from diffs.

---

## Features

| Category | Details |
|---|---|
| Layouts | Tiling (master/stack), floating, monocle |
| Workspaces | 9 tags via bitmasks; windows may carry multiple tags |
| Mouse support | Move, resize, toggle floating via modifier + button |
| Gaps | Configurable `gappx`; all layouts respect gaps |
| Borders | Inactive, focused, and urgent colors at compile time |
| Fullscreen | Toggle via keybind or `_NET_WM_STATE_FULLSCREEN` |
| Urgent hints | `XUrgencyHint` and `_NET_ACTIVE_WINDOW` handled as urgency |
| Auto-float | `_NET_WM_WINDOW_TYPE_DIALOG` windows float automatically |
| EWMH | `_NET_SUPPORTED`, `_NET_WM_STATE`, `_NET_ACTIVE_WINDOW`, `_NET_CLIENT_LIST`, `_NET_SUPPORTING_WM_CHECK`, `_NET_WM_WINDOW_TYPE`, `_NET_WM_WINDOW_TYPE_DIALOG` |
| ICCCM | `WM_DELETE_WINDOW`, `WM_TAKE_FOCUS`, `WM_NORMAL_HINTS`, `WM_HINTS`, `WM_STATE` |
| OpenBSD | `pledge(2)` support; FreeBSD support is expected but not actively tested |
| Compilation | Clean target under `gcc` or `clang` with `-std=c99 -pedantic -Wall -Wextra` |

---

## Screenshots

<img src="https://raw.githubusercontent.com/tinyopsec/distrohop/main/assets/alpine.webp" width="49%" alt="nwm on Alpine Linux rice, my pc btw: three terminals tiled in master/stack layout">
<img src="https://raw.githubusercontent.com/tinyopsec/distrohop/main/assets/arch.webp" width="49%" alt="nwm master/stack with browser in master and two terminals in stack">

### These are my screenshots.

---

## Installation

### Requirements

| Dependency | Arch | Debian / Ubuntu | Void | Alpine |
|---|---|---|---|---|
| Xlib | libx11 | libx11-dev | libX11-devel | libx11-dev |
| C compiler | `gcc` or `clang` | build-essential | gcc | build-base |

No runtime dependencies beyond Xlib.

### From Source

```sh
git clone --depth 1 https://github.com/tinyopsec/nwm
cd nwm
make
sudo make install   # installs to /usr/local/bin/nwm
```

Change `PREFIX` in the `Makefile` to install elsewhere.

### AUR (Arch Linux)

```sh
yay -S nwm
```

Package: [aur.archlinux.org/packages/nwm](https://aur.archlinux.org/packages/nwm)

<details>
<summary>FreeBSD / OpenBSD / NetBSD / DragonFly</summary>

Install Xlib via the system package manager, then edit the top of the `Makefile` to point at your system's X11 paths. The relevant lines are already present but commented out:

```make
# Uncomment for OpenBSD / FreeBSD:
# INCS = -I/usr/X11R6/include
# LIBS = -L/usr/X11R6/lib -lX11
```

Then build normally:

```sh
make && sudo make install
```

`nwm` uses `pledge(2)` on OpenBSD automatically. No extra steps needed.

</details>

### Uninstall

```sh
sudo make uninstall
```

---

## Usage

### Starting nwm

Add to `~/.xinitrc`:

```sh
picom &
feh --bg-scale ~/wallpaper.png &
exec nwm
```

`nwm` has no built-in autostart. Launch background processes from `.xinitrc` or a wrapper script before the `exec` line.

### Display Managers

For display managers:

```ini
# /usr/share/xsessions/nwm.desktop
[Desktop Entry]
Name=nwm
Comment=Minimal tiling X11 window manager
Exec=nwm
Type=Application
```

### Terminal and Launcher

Defaults in `nwm.h`:

```c
static const char *termcmd[]  = { "st", NULL };
static const char *dmenucmd[] = { "dmenu_run", NULL };
```

To use `alacritty` and `rofi`:

```c
static const char *termcmd[]  = { "alacritty", NULL };
static const char *dmenucmd[] = { "rofi", "-show", "run", NULL };
```

> [!IMPORTANT]
> Recompile after any change to `nwm.h`: `make && sudo make install`

---

## Key Bindings

The default modifier is Super (Win). To use Alt instead, change `#define MODKEY Mod4Mask` to `Mod1Mask` in `nwm.h`.

### Windows and Layouts

| Key | Action |
|---|---|
| Mod + Return | Spawn terminal |
| Mod + d | Spawn launcher (dmenu or whatever you chose) |
| Mod + j | Focus next window in stack |
| Mod + k | Focus previous window in stack |
| Mod + h | Shrink master area by 5% |
| Mod + l | Grow master area by 5% |
| Mod + i | Increase master window count |
| Mod + o | Decrease master window count |
| Mod + Space | Promote focused window to master |
| Mod + t | Tiling layout |
| Mod + f | Floating layout |
| Mod + m | Monocle layout |
| Mod + F11 | Toggle fullscreen |
| Mod + Shift + Space | Toggle floating for focused window |
| Mod + q | Kill focused window |
| Mod + Shift + e | Quit nwm |

### Tags

| Key | Action |
|---|---|
| Mod + 1-9 | Switch to tag |
| Mod + Ctrl + 1-9 | Toggle tag view (show alongside current) |
| Mod + Shift + 1-9 | Move focused window to tag |
| Mod + Ctrl + Shift + 1-9 | Toggle tag assignment on focused window |
| Mod + 0 | View all tags |
| Mod + Shift + 0 | Assign focused window to all tags |
| Mod + Tab | Return to previous tag view (XOR two-slot) |

> [!NOTE]
> `Mod+Tab` is not a simple "previous tag" shortcut. It uses a two-slot XOR mechanism. It restores the previous tag bitmask saved by the last view change, including combined multi-tag views.

### Mouse (modifier held over a client window)

| Button | Action |
|---|---|
| Mod + Button1 | Move window |
| Mod + Button2 | Toggle floating |
| Mod + Button3 | Resize window |

Dragging or resizing a tiled window beyond `snap` pixels from its position automatically makes it floating. The `snap` threshold is configurable in `nwm.h`.

All bindings are defined in the `keys[]` and `buttons[]` arrays in `nwm.h`.

---

## Configuration

`nwm` is configured at compile time by editing `nwm.h`. There is no config file, no IPC, no reload mechanism.

> [!IMPORTANT]
> After every change to `nwm.h`, run `make && sudo make install` and restart nwm. pls

### Options

| Option | Default | Description |
|---|---:|---|
| `borderpx` | `2` | Border width in pixels |
| `gappx` | `6` | Gap size between windows and screen edges |
| `col_nborder` | `#0E2426` | Inactive border color |
| `col_sborder` | `#59C6B5` | Focused border color |
| `col_uborder` | `#c47f50` | Urgent window border color |
| `mfact` | `0.5` | Master area ratio (0.05–0.95) |
| `nmaster` | `1` | Initial number of master windows |
| `snap` | `16` | Edge snap / float-on-drag threshold in pixels |
| `attachbottom` | `0` | Set to `1` to append new windows at bottom of stack |
| `focusonopen` | `1` | Set to `0` to keep focus on the current window when a new one opens |

### Modifier Key

```c
#define MODKEY Mod4Mask   /* Super / Win key */
// #define MODKEY Mod1Mask   /* Alt key */
```

### Border Colors

```c
static const char col_nborder[] = "#0E2426";  /* inactive */
static const char col_sborder[] = "#59C6B5";  /* focused  */
static const char col_uborder[] = "#c47f50";  /* urgent   */
```

---

## Window Rules

Window rules are defined in `nwm.h`:

```c
static const Rule rules[] = {
	/* class     instance  title  tags  isfloating */
	{ "Gimp",    NULL,     NULL,  0,    1 },
	{ "MPlayer", NULL,     NULL,  0,    1 },
};
```

Fields:

| Field | Meaning |
|---|---|
| `class` | `WM_CLASS` class |
| `instance` | `WM_CLASS` instance |
| `title` | window title substring |
| `tags` | tag bitmask, or `0` to keep current tags |
| `isfloating` | `1` floating, `0` tiled, `-1` keep default decision |

If no rule matches, `nwm` keeps its default floating decision.

---

## How It Works

Thanks for https://github.com/mermaid-js/mermaid.

`nwm` manages windows through a flat client list and a parallel focus stack. The tiling algorithm divides the screen into a master area and a stack area, computing tile sizes with integer arithmetic: no floating-point accumulation, no pixel drift across redraws. The remaining pixels are assigned to the last window in each column.

Tags are bitmasks. Each client carries a tag bitmask; the active view is a bitmask. A client is visible when the bitwise AND of its tags and the current view is nonzero. This means one window can appear on multiple tags simultaneously.

Layout and tag history both use a two-slot XOR system. `nwm` keeps the current and previous values in a two-element array and flips an index bit on each change. `Mod+Tab` flips the index back for tag views, returning to the previous saved tag mask.

`nwm` also keeps minimal per-tag state:

- current and previous layout
- layout selection slot
- master factor
- master window count

When multiple tags are visible, per-tag state is restored from the lowest numbered visible tag.

Some behavior is intentionally explicit:

- `_NET_ACTIVE_WINDOW` requests are not used to force focus; they mark the window as urgent instead.
- Tiled windows ignore size hints where needed to keep tiling deterministic.
- Floating windows respect ICCCM size hints.
- Dialog windows (`_NET_WM_WINDOW_TYPE_DIALOG`) float automatically.
- Monocle respects `gappx`.

It’s hard to explain in words, but you can grasp the entire code structure if you spend an hour reading the code.

---

## Contributing

Bug reports and patches are welcome via [GitHub Issues](https://github.com/tinyopsec/nwm/issues) and pull requests.

<details>
<summary>Code requirements</summary>

- No external dependencies
- No over-abstraction or wrapper layers
- Compiles clean: `gcc -std=c99 -pedantic -Wall -Wextra`
- Optional features behind `#ifdef` or compile-time constants are considered. Core event loop changes are reviewed carefully.

</details>

---

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=tinyopsec/nwm&type=Date)](https://star-history.com/#tinyopsec/nwm&Date)

---

## Related

### Suckless ecosystem

| Project | Link |
|---|---|
| dwm | [dwm.suckless.org](https://dwm.suckless.org) |
| st terminal | [st.suckless.org](https://st.suckless.org) |
| dmenu | [tools.suckless.org/dmenu](https://tools.suckless.org/dmenu/) |
| suckless.org | [suckless.org](https://suckless.org) |

### Specifications

| Spec | Link |
|---|---|
| EWMH | [freedesktop.org](https://specifications.freedesktop.org/wm-spec/latest/) |
| ICCCM | [x.org](https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html) |
| Xlib manual | [x.org](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html) |

### Packages

| Distro | Link |
|---|---|
| AUR (Arch) | [aur.archlinux.org/packages/nwm](https://aur.archlinux.org/packages/nwm) |

<details>
<summary>Thanks / Resources used</summary>

Thanks to my friends for their help, and I also thank these resources:

- shields.io
- capsule-render from vercel (Very beautiful)
- demolab.com
- contrib.rocks
- repobeats.axiom.co
- star-history.com
- github-profile-trophy
- Ross Maloney for Fundamentals of Xlib Programming by Examples

https://www.linux.co.cr/desktops/review/acrobat/030103.pdf

Sorry for the English; I'm using a translator.

I removed the original Russian comments from the code; the remaining comments are short and in English.

All screenshots are mine.

https://upload.wikimedia.org/wikipedia/commons/d/d2/Terry_A.Davis%28cropped%29.jpg

</details>

<img src="https://capsule-render.vercel.app/api?type=waving&color=0E2426&height=80&section=footer" alt="nwm footer">
