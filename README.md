# nwm

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/issues)
[![GitHub stars](https://img.shields.io/github/stars/tinyopsec/nwm.svg)](https://github.com/tinyopsec/nwm/stargazers)

A minimal X11 window manager. Two files. Under 1000 lines of POSIX C.

nwm is configured entirely at compile time. No config files, no runtime dependencies beyond Xlib, no status bar. The full codebase fits in `nwm.c` and `nwm.h`, making it easy to read, audit, and modify.

---

## Features

- Tiling, floating, and monocle layouts
- Tag-based workspaces (9 tags)
- Mouse support: move, resize, toggle float
- Urgent window highlighting with a distinct border color
- Compile-time color palette: `#1e1e1e` / `#7c9e7e` / `#c47f50`
- No status bar, no font rendering, no bloat
- POSIX-compliant, under 1000 lines of C
- OpenBSD `pledge(2)` support

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

- `borderpx` - border width in pixels
- `gappx` - gap between windows and screen edges
- `col_nborder` - inactive border color (hex)
- `col_sborder` - focused border color (hex)
- `col_uborder` - urgent border color (hex)

**Behavior**

- `mfact` - master area ratio (0.05–0.95)
- `nmaster` - number of master windows
- `snap` - edge snap distance in pixels
- `attachbottom` - attach new windows at stack bottom instead of top
- `focusonopen` - focus newly opened windows

**Bindings**

- `MODKEY` - modifier key (`Mod1Mask` = Alt, `Mod4Mask` = Super)
- `keys[]` - keyboard shortcuts (type `K`)
- `buttons[]` - mouse actions (type `B`)
- `termcmd[]` - terminal command
- `dmenucmd[]` - launcher command

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

## Code Structure

Two files: `nwm.c` contains all logic; `nwm.h` is the user configuration, `#include`d near the top of `nwm.c` after type and forward declarations. This means `nwm.h` has access to all typedefs and can reference handler functions by name directly.

### Types

All types use short aliases defined at the top of `nwm.c`:

| Alias | Full type | Purpose |
|---|---|---|
| `A` | `union { int i; unsigned int ui; float f; const void *v; }` | Generic argument to key/button handlers |
| `B` | `struct { click, mask, button, fn, arg }` | Mouse binding entry |
| `K` | `struct { mod, key, fn, arg }` | Keyboard binding entry |
| `L` | `struct { void (*ar)(void); }` | Layout - just a function pointer |
| `C` | `struct C` | Client window |

### Client struct (`C`)

Each managed window is represented by a `C` on the heap (`calloc` in `mg()`). Fields:

- `x, y, w, h` - current geometry
- `oldx, oldy, oldw, oldh` - saved geometry (restored on fullscreen exit)
- `basew/h, incw/h, minw/h, maxw/h` - ICCCM size hints
- `mina, maxa` - aspect ratio constraints
- `bw, oldbw` - border width (set to 0 during fullscreen)
- `tags` - bitmask of tags this client belongs to
- `isfloating, isfullscreen, isurgent, isfixed, neverfocus` - state flags
- `next` - next in client list `cs` (insertion order)
- `snext` - next in focus stack `st` (MRU order)
- `win` - the underlying `Window` XID

### Global State

All globals are `static`. Key ones:

| Variable | Type | Meaning |
|---|---|---|
| `d` | `Display*` | Xlib display connection |
| `r` | `Window` | Root window |
| `cs` | `C*` | Head of the client list (insertion order) |
| `st` | `C*` | Head of the focus stack (MRU order) |
| `s` | `C*` | Currently focused client |
| `mf` | `float` | Current master factor |
| `nm` | `int` | Current master count |
| `lt[2]` | `const L*[2]` | Two layout slots (current + previous) |
| `lt2` | `unsigned int` | Index into `lt[]` (0 or 1), toggled by `setlayout` |
| `ts[2]` | `unsigned int` | Two tag bitmask slots (current + previous view) |
| `sg` | `unsigned int` | Index into `ts[]` (0 or 1), toggled by `view` |
| `nborder/sborder/uborder` | `unsigned long` | Allocated pixel values for border colors |

The two-slot pattern for both `lt` and `ts` is what makes `Mod+Tab` (toggle last tag/layout) work with no extra state.

### Startup Sequence

`main()` calls:

1. `XOpenDisplay` - connect to X server
2. `checkotherwm()` - installs `xerrorstart` as error handler, calls `XSelectInput` on root with `SubstructureRedirectMask`; if another WM is running this triggers an error and nwm exits
3. `setup()` - allocates colors, cursors, interns atoms, creates the `_NET_SUPPORTING_WM_CHECK` window, initializes globals, calls `grabkeys()`, sets root event mask
4. `pledge()` (OpenBSD only) - drops to `stdio rpath proc exec`
5. `scan()` - walks existing mapped windows via `XQueryTree`, calls `mg()` on each to adopt them; processes non-transient windows first, then transients
6. `run()` - event loop
7. `cleanup()` - unmanages all clients, frees cursors, destroys check window

### Event Loop

`run()` is a plain `while (running && !XNextEvent(d, &ev))` loop dispatching through a static handler table:

```
handler[ButtonPress]      = bp
handler[ClientMessage]    = clientmessage
handler[ConfigureRequest] = configurerequest
handler[DestroyNotify]    = destroynotify
handler[EnterNotify]      = enternotify
handler[FocusIn]          = focusin
handler[KeyPress]         = kp
handler[MappingNotify]    = mappingnotify
handler[MapRequest]       = maprequest
handler[PropertyNotify]   = propertynotify
handler[UnmapNotify]      = unmapnotify
```

Unhandled event types are silently ignored.

### Window Management Lifecycle

**Mapping** (`maprequest` → `mg()`): when a client calls `XMapWindow`, the WM receives `MapRequest`. `mg()` allocates a `C`, reads geometry and size hints, inherits tags from transient parent if applicable, clamps position to screen, sets border width, calls `at()` to prepend to `cs` (or append if `attachbottom`), then maps and focuses.

**Unmapping / Destruction**: `unmapnotify` and `destroynotify` both route to `unmanage()`, which detaches from both lists, restores the border width if the window still exists, frees the `C`, and triggers a re-layout.

**Focus** (`fc()`): sets `s`, moves the client to the head of `st`, sets the focused border color, calls `setfocus()` which does `XSetInputFocus` and updates `_NET_ACTIVE_WINDOW`. `unfocus()` restores the inactive border and optionally reverts input focus to root.

### Layout Engine

`ar()` (arrange) is the single entry point for all geometry changes:

1. calls `showhide()` - recursively walks `st`, moves visible clients on-screen and hidden ones off-screen (`x = W(c) * -2`) without unmapping them
2. calls the current layout function if one is set (`lt[lt2]->ar`)
3. calls `restack()` - raises floating/fullscreen clients, stacks tiled clients below root in focus order

The three layout functions:

- **`tile()`** - classic master/stack. Computes master width as `ww * mf` when both regions have clients. Each region is divided into equal-height slots with `gappx` gaps on all sides including screen edges.
- **`monocle()`** - all tiled clients fill the same area (`wx+g, wy+g, ww-2bw-2g, wh-2bw-2g`). Stacking order determines which is visible.
- **`NULL` (floating)** - `ar()` skips the layout call; all clients retain their own geometry.

`rs()` → `applysizehints()` → `resizeclient()` is the geometry pipeline. `applysizehints` enforces ICCCM constraints (base size, increment, min/max, aspect ratio) and screen boundary clamping. It returns non-zero only if geometry actually changed, so `resizeclient` is only called when needed.

### Tag System

Tags are bitmasks. Each client has a `tags` field; the current view is `ts[sg]`. `VIS(c)` expands to `(c)->tags & ts[sg]` - a client is visible if any of its tag bits overlap the current view mask. Setting `tags = ~0` (all bits) shows a client on all tags; `view({.ui = ~0})` shows all clients.

`ts[2]` and `sg` implement the two-slot toggle: `view()` XORs `sg` to switch slots, stores the new tag mask in the freshly active slot. `Mod+Tab` calls `view({0})` which skips the mask update and just flips `sg`, effectively swapping to whatever was in the other slot.

### Mouse Operations

`mv()` and `rz()` both run a nested event loop inside the main event loop. They grab the pointer, then call `XMaskEvent` in a do-while loop until `ButtonRelease`, processing only `MotionNotify` (rate-limited to 60 fps via timestamp delta), `ConfigureRequest`, `Expose`, and `MapRequest`. On motion, if the client is tiled and dragged/resized past `snap` pixels, `togglefloating()` is called automatically. Edge snapping is computed by comparing `nx`/`ny` to `wx`, `wy`, `wx+ww`, `wy+wh`.

### EWMH / ICCCM

nwm handles a minimal subset:

- `_NET_WM_STATE_FULLSCREEN` - via `clientmessage` and `setfullscreen()`
- `_NET_ACTIVE_WINDOW` - set on focus, triggers urgent on foreign activation requests
- `_NET_WM_WINDOW_TYPE_DIALOG` - auto-floated in `updatewindowtype()`
- `_NET_CLIENT_LIST` - maintained as clients are managed/unmanaged
- `_NET_SUPPORTING_WM_CHECK` - compliance window created in `setup()`
- `WM_DELETE_WINDOW` - `killclient()` sends this first; falls back to `XKillClient` if unsupported
- `WM_TAKE_FOCUS` - sent via `sendevent()` in `setfocus()`
- `WM_HINTS` / `WM_NORMAL_HINTS` - read in `updatewmhints()` / `updatesizehints()`

---

## Contributing

The codebase is small enough to read in one sitting. Bug reports, patches, and improvements are welcome via GitHub issues or pull requests. Keep changes minimal and in the spirit of the project.

---

## License

MIT. See [LICENSE](LICENSE) for details.

## Acknowledgements

nwm is inspired by [dwm](https://dwm.suckless.org) by the suckless community.
