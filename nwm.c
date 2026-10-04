#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>

#ifdef __OpenBSD__
int pledge(const char *, const char *);
#endif

#define BUTTONMASK      (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK       (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(m)    ((m) & ~(numlockmask|LockMask) & \
                         (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define VIS(c)          ((c)->tags & tagsel[sel_group])
#define W(c)            ((c)->w + ((c)->bw << 1))
#define H(c)            ((c)->h + ((c)->bw << 1))
#define LEN(x)          (sizeof(x)/sizeof(*(x)))
#define TM              ((1u << LEN(tags)) - 1)
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#define ARR             (cur.lt[cur.li]->ar)             /* current layout, NULL = floating */
#define FREE(c)         ((c)->isfloating || !ARR)        /* positioned by the user, not by the layout */
#define CYCLE(c)        (VIS(c) && !(ARR && (c)->isfloating))
#define MOTION_THROTTLE_MS 16

typedef union { int i; unsigned int ui; float f; const void *v; } A;
typedef struct { unsigned int click, mask, button; void (*fn)(const A *); A arg; } B;
typedef struct { unsigned int mod; KeySym key; void (*fn)(const A *); A arg; } K;
typedef struct { void (*ar)(void); } L;
typedef struct C C;
typedef struct {
	const char *class, *instance, *title;
	unsigned int tags;
	int isfloating;
} Rule;

/* Per-tag state: two-slot layout history, master factor, master count. */
typedef struct { const L *lt[2]; unsigned int li; float mf; int nm; } PT;

struct C {
	float mina, maxa;
	int x, y, w, h, oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags, pubtags;
	unsigned int isfixed:1, isfloating:1, isurgent:1, neverfocus:1,
	             oldstate:1, isfullscreen:1, hintsvalid:1;
	C *next, *snext;
	Window win;
};

enum { ClkClientWin, ClkRootWin };
enum { NetSupported, NetWMState, NetWMFullscreen, NetActiveWindow,
       NetWMWindowType, NetWMWindowTypeDialog, NetClientList, NetWMCheck,
       NetWMWindowTypeDock, NetWMStrut, NetWMStrutPartial, NetWorkarea,
       NetNumberOfDesktops, NetCurrentDesktop, NetDesktopNames, NetWMDesktop,
       NetDesktopGeometry, NetDesktopViewport, NetLast };
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };

static int applyrules(C *);
static int applysizehints(C *, int *, int *, int *, int *, int);
static void ar(void);
static void at(C *);
static void bp(XEvent *);
static void checkwm(void);
static void cleanup(void);
static void clientmsg(XEvent *);
static void configurenotify(C *);
static void configurerequest(XEvent *);
static void destroynotify(XEvent *);
static void detach(C *);
static void detachstack(C *);
static unsigned int dockidx(Window);
static void enternotify(XEvent *);
static void focus(C *);
static void focusin(XEvent *);
static void focusnext(const A *);
static long getstate(Window);
static void grabbuttons(C *, int);
static void grabkeys(void);
static int hasatom(Window, Atom, Atom);
static void incnmaster(const A *);
static void keypress(XEvent *);
static void killclient(const A *);
static unsigned int lowtag(unsigned int);
static void manage(Window, XWindowAttributes *);
static void mappingnotify(XEvent *);
static void maprequest(XEvent *);
static void monocle(void);
static void movemouse(const A *);
static C *nexttiled(C *);
static void pop(C *);
static void propertynotify(XEvent *);
static void quit(const A *);
static void resize(C *, int, int, int, int, int);
static void resizeclient(C *, int, int, int, int);
static void resizemouse(const A *);
static void restack(void);
static void run(void);
static void scan(void);
static int sendevent(C *, Atom);
static void setclientstate(C *, long);
static void setfocus(C *);
static void setfullscreen(C *, int);
static void setlayout(const A *);
static void setmasterfact(const A *);
static void setup(void);
static void seturgency(C *, int);
static void showhide(C *);
static void spawn(const A *);
static void tag(const A *);
static void tile(void);
static void togglefloating(const A *);
static void togglefullscreen(const A *);
static void toggletag(const A *);
static void toggleview(const A *);
static void unfocus(C *);
static void undock(Window);
static void unmanage(C *, int);
static void unmapnotify(XEvent *);
static void updateclientlist(void);
static void updatedesktops(void);
static void updatenumlockmask(void);
static void updatesizehints(C *);
static void updatewindowtype(C *);
static void updateworkarea(void);
static void updatewmhints(C *);
static void view(const A *);
static C *wintoclient(Window);
static int xerror(Display *, XErrorEvent *);
static int xerrorstart(Display *, XErrorEvent *);
static void zoom(const A *);

#include "nwm.h"

typedef char nwm_tags_range[(LEN(tags) >= 1 && LEN(tags) < 32) ? 1 : -1];
typedef char nwm_layouts_any[(LEN(layouts) >= 1) ? 1 : -1];

#if NWM_WITH_GAPS
#define GAP() (gappx)
#else
#define GAP() 0
#endif

#if NWM_WITH_BORDERS
#define DEFAULT_BORDERPX borderpx
#define BORDER(c, pix) XSetWindowBorder(display, (c)->win, (pix))
#else
#define DEFAULT_BORDERPX 0
#define BORDER(c, pix) ((void)0)
#endif

static Display *display;
static Window root, wmcheck_win;
static int screen_w, screen_h, wx, wy, ww, wh, running = 1; /* wx..wh: area not covered by docks */
static unsigned int numlockmask, sel_group, tagsel[2];
static Cursor cursor[3];
static Atom wmatom[WMLast], netatom[NetLast];
static C *clients, *sel, *stack;
static Window docks[8];
static unsigned int ndock;
static PT cur;

static void (*handler[LASTEvent])(XEvent *) = {
	[ButtonPress]      = bp,
	[ClientMessage]    = clientmsg,
	[ConfigureRequest] = configurerequest,
	[DestroyNotify]    = destroynotify,
	[EnterNotify]      = enternotify,
	[FocusIn]          = focusin,
	[KeyPress]         = keypress,
	[MappingNotify]    = mappingnotify,
	[MapRequest]       = maprequest,
	[PropertyNotify]   = propertynotify,
	[UnmapNotify]      = unmapnotify,
};

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

/* Index of the lowest tag in mask m. */
static unsigned int
lowtag(unsigned int m)
{
	unsigned int i;

	for (i = 0; i < LEN(tags) - 1 && !(m & (1u << i)); i++)
		;
	return i;
}

#if NWM_WITH_PERTAG
static PT per_tag[LEN(tags)];

/* Per-tag state lives under the lowest tag of the current view. */
static unsigned int
curtag(void)
{
	return lowtag(tagsel[sel_group]);
}

static void
ptinit(void)
{
	unsigned int i;

	for (i = 0; i < LEN(tags); i++)
		per_tag[i] = cur;
}

static void ptsave(void) { per_tag[curtag()] = cur; }
static void ptload(void) { cur = per_tag[curtag()]; }
#else
#define ptinit() ((void)0)
#define ptsave() ((void)0)
#define ptload() ((void)0)
#endif

#if NWM_WITH_BORDERS
static unsigned long color_norm, color_sel, color_urg;

static unsigned long
getcolor(const char *name)
{
	XColor xc, exact;

	if (!XAllocNamedColor(display, DefaultColormap(display, DefaultScreen(display)),
	                      name, &xc, &exact))
		die("nwm: cannot allocate color %s", name);
	return xc.pixel;
}
#endif

static int
applyrules(C *c)
{
	const Rule *r;
	XClassHint ch = { NULL, NULL };
	char *name = NULL;
	unsigned int i;
	int floating = -1;

	XGetClassHint(display, c->win, &ch);
	XFetchName(display, c->win, &name);

	for (i = 0; i < LEN(rules); i++) {
		r = &rules[i];
		if ((!r->class || (ch.res_class && !strcmp(ch.res_class, r->class))) &&
		    (!r->instance || (ch.res_name && !strcmp(ch.res_name, r->instance))) &&
		    (!r->title || (name && strstr(name, r->title)))) {
			floating = r->isfloating;
			if (r->tags & TM)
				c->tags = r->tags & TM;
			break;
		}
	}

	XFree(ch.res_class);
	XFree(ch.res_name);
	XFree(name);
	return floating;
}

static int
applysizehints(C *c, int *x, int *y, int *w, int *h, int interact)
{
	int bw2 = c->bw << 1, bim;

	*w = MAX(1, *w);
	*h = MAX(1, *h);

	if (interact) {
		if (*x > screen_w)
			*x = screen_w - (*w + bw2);
		if (*x + *w + bw2 < 0)
			*x = 0;
		if (*y + *h + bw2 < 0)
			*y = 0;
	} else {
		if (*x >= screen_w)
			*x = screen_w - (*w + bw2);
		if (*y >= screen_h)
			*y = screen_h - (*h + bw2);
		if (*x + *w + bw2 <= 0)
			*x = 0;
		if (*y + *h + bw2 <= 0)
			*y = 0;
	}

	if (FREE(c)) {
		if (!c->hintsvalid)
			updatesizehints(c);
		bim = c->basew == c->minw && c->baseh == c->minh;
		if (!bim) {
			*w -= c->basew;
			*h -= c->baseh;
		}
		if (c->mina > 0.0f && c->maxa > 0.0f && *w > 0 && *h > 0) {
			if (c->maxa < (float)*w / *h)
				*w = (int)(*h * c->maxa + 0.5f);
			else if (c->mina < (float)*h / *w)
				*h = (int)(*w * c->mina + 0.5f);
		}
		if (bim) { /* the increment calculation needs it removed */
			*w -= c->basew;
			*h -= c->baseh;
		}
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}

	*w = MAX(1, *w);
	*h = MAX(1, *h);
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

static void
ar(void)
{
	showhide(stack);
	if (ARR)
		ARR();
	restack();
	updatedesktops();
}

static void
at(C *c)
{
	C **pp = &clients;

	if (attachbottom)
		while (*pp)
			pp = &(*pp)->next;
	c->next = *pp;
	*pp = c;
	c->snext = stack;
	stack = c;
}

static void
bp(XEvent *e)
{
	unsigned int i, click = ClkRootWin;
	XButtonPressedEvent *ev = &e->xbutton;
	C *c = wintoclient(ev->window);

	if (c) {
		focus(c);
		restack();
		click = ClkClientWin;
	}

	/*
	 * Unfocused clients carry a synchronous grab that freezes pointer and
	 * keyboard until released. Release it even if the client is already
	 * gone, otherwise all input stays frozen.
	 */
	XAllowEvents(display, ReplayPointer, CurrentTime);

	for (i = 0; i < LEN(buttons); i++) {
		if (click == buttons[i].click && buttons[i].fn &&
		    buttons[i].button == ev->button &&
		    CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].fn(&buttons[i].arg);
	}
}

static void
checkwm(void)
{
	XSetErrorHandler(xerrorstart);
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False);
	XSetErrorHandler(xerror);
}

static void
cleanup(void)
{
	unsigned int i;
	C *c;
	XWindowChanges wc;

	/* Bring every window back on screen, including hidden ones. */
	while ((c = clients)) {
		clients = c->next;
		wc.border_width = c->oldbw;
		XConfigureWindow(display, c->win, CWBorderWidth, &wc);
		XMoveWindow(display, c->win, c->x, c->y);
		setclientstate(c, WithdrawnState);
		free(c);
	}
	sel = stack = NULL;

	XUngrabKey(display, AnyKey, AnyModifier, root);
	for (i = 0; i < 3; i++)
		XFreeCursor(display, cursor[i]);
	XDeleteProperty(display, root, netatom[NetClientList]);
	XDeleteProperty(display, root, netatom[NetActiveWindow]);
	XDeleteProperty(display, root, netatom[NetSupported]);
	XDeleteProperty(display, root, netatom[NetWMCheck]);
	XDeleteProperty(display, root, netatom[NetNumberOfDesktops]);
	XDeleteProperty(display, root, netatom[NetCurrentDesktop]);
	XDeleteProperty(display, root, netatom[NetDesktopNames]);
	XDeleteProperty(display, root, netatom[NetWorkarea]);
	XDeleteProperty(display, root, netatom[NetDesktopGeometry]);
	XDeleteProperty(display, root, netatom[NetDesktopViewport]);
	XDestroyWindow(display, wmcheck_win);
	XSync(display, False);
	XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
}

static void
clientmsg(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;
	C *c = wintoclient(ev->window);
	A a;

	/* A panel switches desktops by messaging the root window. */
	if (ev->window == root && ev->message_type == netatom[NetCurrentDesktop]) {
		if ((unsigned long)ev->data.l[0] < LEN(tags)) {
			a.ui = 1u << ev->data.l[0];
			view(&a);
		}
		return;
	}
	if (!c)
		return;

	if (ev->message_type == netatom[NetWMState] &&
	    (ev->data.l[1] == (long)netatom[NetWMFullscreen] ||
	     ev->data.l[2] == (long)netatom[NetWMFullscreen])) {
		setfullscreen(c, ev->data.l[0] == 1 ||
		                 (ev->data.l[0] == 2 && !c->isfullscreen));
	} else if (ev->message_type == netatom[NetActiveWindow] &&
	           c != sel && !c->isurgent) {
		/* Avoid focus stealing: activation requests become urgency hints. */
		seturgency(c, 1);
	}
}

static void
configurenotify(C *c)
{
	XConfigureEvent ce = {
		.type = ConfigureNotify, .send_event = True, .display = display,
		.event = c->win, .window = c->win,
		.x = c->x, .y = c->y, .width = c->w, .height = c->h,
		.border_width = c->bw, .above = None, .override_redirect = False
	};

	XSendEvent(display, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

static void
configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	C *c = wintoclient(ev->window);

	if (!c) {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
		return;
	}

	/* Border width and fullscreen geometry belong to nwm, not to the client. */
	if (!c->isfullscreen && FREE(c)) {
		int x = c->x, y = c->y, w = c->w, h = c->h;

		if (ev->value_mask & CWX) x = ev->x;
		if (ev->value_mask & CWY) y = ev->y;
		if (ev->value_mask & CWWidth) w = ev->width;
		if (ev->value_mask & CWHeight) h = ev->height;

		if (VIS(c)) {
			applysizehints(c, &x, &y, &w, &h, 0);
			resizeclient(c, x, y, w, h);
			if (c->isfloating)
				XRaiseWindow(display, c->win);
			return;
		}
		c->x = x; c->y = y; c->w = w; c->h = h;
	}
	configurenotify(c);
}

static void
destroynotify(XEvent *e)
{
	C *c;

	if ((c = wintoclient(e->xdestroywindow.window)))
		unmanage(c, 1);
	else
		undock(e->xdestroywindow.window);
}

static void
detach(C *c)
{
	C **pp;

	for (pp = &clients; *pp && *pp != c; pp = &(*pp)->next)
		;
	if (*pp)
		*pp = c->next;
}

static void
detachstack(C *c)
{
	C **pp, *t;

	for (pp = &stack; *pp && *pp != c; pp = &(*pp)->snext)
		;
	if (*pp)
		*pp = c->snext;

	if (c == sel) {
		for (t = stack; t && !VIS(t); t = t->snext)
			;
		sel = t;
	}
}

/* Position of w in docks[]; ndock if it is not a known dock. */
static unsigned int
dockidx(Window w)
{
	unsigned int i;

	for (i = 0; i < ndock && docks[i] != w; i++)
		;
	return i;
}

static void
enternotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;
	C *c;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
	    ev->window != root)
		return;

	c = wintoclient(ev->window);
	if (!c || c == sel || c->isfullscreen || (sel && sel->isfullscreen))
		return;

	/* A floating window keeps focus against pointer motion over tiled ones. */
	if (sel && sel->isfloating && !c->isfloating && ARR)
		return;

	focus(c);
	restack();
}

static void
focus(C *c)
{
	if (!c || !VIS(c))
		for (c = stack; c && !VIS(c); c = c->snext)
			;

	if (sel && sel != c)
		unfocus(sel);

	if (c) {
		if (c->isurgent)
			seturgency(c, 0);
		detachstack(c);
		c->snext = stack;
		stack = c;
		grabbuttons(c, 1);
		BORDER(c, color_sel);
		setfocus(c);
	} else {
		XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(display, root, netatom[NetActiveWindow]);
	}
	sel = c;
}

static void
focusin(XEvent *e)
{
	/* Some clients steal focus; give it back to the selected one. */
	if (sel && e->xfocus.window != sel->win)
		setfocus(sel);
}

static void
focusnext(const A *arg)
{
	C *c = NULL, *i;

	if (!sel || sel->isfullscreen)
		return;

	if (arg->i > 0) {
		for (c = sel->next; c && !CYCLE(c); c = c->next)
			;
		if (!c)
			for (c = clients; c && !CYCLE(c); c = c->next)
				;
	} else {
		for (i = clients; i != sel; i = i->next)
			if (CYCLE(i))
				c = i;
		if (!c)
			for (i = sel->next; i; i = i->next)
				if (CYCLE(i))
					c = i;
	}

	if (c && c != sel) {
		focus(c);
		restack();
	}
}

static long
getstate(Window w)
{
	int fmt;
	long res = -1;
	unsigned char *p = NULL;
	unsigned long n, ex;
	Atom real;

	if (XGetWindowProperty(display, w, wmatom[WMState], 0L, 2L, False,
	                       wmatom[WMState], &real, &fmt, &n, &ex, &p) == Success) {
		if (n && fmt == 32 && p)
			memcpy(&res, p, sizeof(long));
		XFree(p);
	}
	return res;
}

static void
grabbuttons(C *c, int focused)
{
	unsigned int i, j, mods[] = { 0, LockMask, numlockmask, numlockmask | LockMask };

	XUngrabButton(display, AnyButton, AnyModifier, c->win);

	if (!focused) {
		XGrabButton(display, AnyButton, AnyModifier, c->win, False,
		            BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		return;
	}

	for (i = 0; i < LEN(buttons); i++) {
		if (buttons[i].click != ClkClientWin)
			continue;
		for (j = 0; j < LEN(mods); j++)
			XGrabButton(display, buttons[i].button, buttons[i].mask | mods[j],
			            c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync,
			            None, None);
	}
}

static void
grabkeys(void)
{
	unsigned int i, j;
	unsigned int mods[4];
	KeyCode code;

	updatenumlockmask();
	mods[0] = 0;
	mods[1] = LockMask;
	mods[2] = numlockmask;
	mods[3] = numlockmask | LockMask;

	XUngrabKey(display, AnyKey, AnyModifier, root);
	for (i = 0; i < LEN(keys); i++) {
		if (!(code = XKeysymToKeycode(display, keys[i].key)))
			continue;
		for (j = 0; j < LEN(mods); j++)
			XGrabKey(display, code, keys[i].mod | mods[j], root, True,
			         GrabModeAsync, GrabModeAsync);
	}
}

/* True if the atom list property `prop` of w contains `want`. */
static int
hasatom(Window w, Atom prop, Atom want)
{
	int fmt, found = 0;
	unsigned long n, rem, i;
	unsigned char *p = NULL;
	Atom type;

	if (XGetWindowProperty(display, w, prop, 0L, 1024L, False, XA_ATOM,
	                       &type, &fmt, &n, &rem, &p) == Success && p) {
		if (type == XA_ATOM && fmt == 32)
			for (i = 0; i < n; i++)
				found |= ((Atom *)p)[i] == want;
		XFree(p);
	}
	return found;
}

static void
incnmaster(const A *arg)
{
	cur.nm = MAX(cur.nm + arg->i, 0);
	ptsave();
	ar();
}

static void
keypress(XEvent *e)
{
	unsigned int i;
	XKeyEvent *ev = &e->xkey;
	KeySym sym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);

	for (i = 0; i < LEN(keys); i++) {
		if (sym == keys[i].key &&
		    CLEANMASK(keys[i].mod) == CLEANMASK(ev->state))
			keys[i].fn(&keys[i].arg);
	}
}

static void
killclient(const A *arg)
{
	(void)arg;

	if (!sel || sendevent(sel, wmatom[WMDelete]))
		return;

	XGrabServer(display);
	XSetCloseDownMode(display, DestroyAll);
	XKillClient(display, sel->win);
	XSync(display, False);
	XUngrabServer(display);
}

static void
manage(Window w, XWindowAttributes *wa)
{
	C *c, *t;
	Window trans = None;
	XWindowChanges wc;
	int rule_floating;

	/* Docks (panels) are not clients: map them, their struts shrink the work area. */
	if (hasatom(w, netatom[NetWMWindowType], netatom[NetWMWindowTypeDock])) {
		if (dockidx(w) == ndock && ndock < LEN(docks)) {
			docks[ndock++] = w;
			XSelectInput(display, w, PropertyChangeMask);
		}
		XMapWindow(display, w);
		updateworkarea();
		return;
	}

	if (!(c = calloc(1, sizeof(C))))
		die("nwm: calloc");

	c->win = w;
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updatesizehints(c);
	updatewmhints(c);

	c->tags = tagsel[sel_group] & TM;
	rule_floating = applyrules(c);
	if (XGetTransientForHint(display, w, &trans) && (t = wintoclient(trans)))
		c->tags = t->tags;

	c->bw = DEFAULT_BORDERPX;
	c->x = MAX(MIN(c->x, wx + ww - W(c)), wx);
	c->y = MAX(MIN(c->y, wy + wh - H(c)), wy);

	wc.border_width = c->bw;
	XConfigureWindow(display, w, CWBorderWidth, &wc);
	BORDER(c, color_norm);
	configurenotify(c);

	c->isfloating = c->oldstate = (trans != None) || c->isfixed;
	if (rule_floating >= 0)
		c->isfloating = c->oldstate = rule_floating;
	updatewindowtype(c);

	XSelectInput(display, w, EnterWindowMask | FocusChangeMask |
	                         PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, 0);
	at(c);
	updateclientlist();

	if (c->isfloating)
		XMapRaised(display, w);
	else
		XMapWindow(display, w);
	setclientstate(c, NormalState);

	if (focusonopen || !sel)
		focus(c);
	ar();
}

static void
mappingnotify(XEvent *e)
{
	XRefreshKeyboardMapping(&e->xmapping);
	if (e->xmapping.request == MappingKeyboard ||
	    e->xmapping.request == MappingModifier)
		grabkeys();
}

static void
maprequest(XEvent *e)
{
	XWindowAttributes wa;
	C *c;

	if (!XGetWindowAttributes(display, e->xmaprequest.window, &wa) ||
	    wa.override_redirect)
		return;

	if ((c = wintoclient(e->xmaprequest.window))) {
		XMapWindow(display, c->win);
		setclientstate(c, NormalState);
		ar();
	} else {
		manage(e->xmaprequest.window, &wa);
	}
}

static void
monocle(void)
{
	C *c;
	int gap = GAP();

	for (c = nexttiled(clients); c; c = nexttiled(c->next))
		resize(c, wx + gap, wy + gap,
		       MAX(1, ww - 2 * gap - (c->bw << 1)),
		       MAX(1, wh - 2 * gap - (c->bw << 1)), 0);
}

static void
movemouse(const A *arg)
{
	int x, y, ocx, ocy, nx, ny, di, needar = 0;
	unsigned int dui, i;
	KeySym sym;
	Window dw;
	XEvent ev;
	Time last = 0;
	C *c = sel;

	(void)arg;
	if (!c || c->isfullscreen)
		return;

	restack();
	ocx = c->x;
	ocy = c->y;

	if (!XQueryPointer(display, root, &dw, &dw, &x, &y, &di, &di, &dui) ||
	    XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
	                 GrabModeAsync, None, cursor[1], CurrentTime) != GrabSuccess)
		return;

	do {
		XMaskEvent(display, MOUSEMASK | KeyPressMask | ExposureMask |
		           SubstructureRedirectMask, &ev);

		if (ev.type == ConfigureRequest || ev.type == MapRequest) {
			handler[ev.type](&ev);
		} else if (ev.type == KeyPress) {
			/* A tag key mid-drag carries the window to that tag; the drag goes on. */
			sym = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
			for (i = 0; i < LEN(keys); i++) {
				if (keys[i].fn != view || !(keys[i].arg.ui & TM) ||
				    sym != keys[i].key ||
				    CLEANMASK(keys[i].mod) != CLEANMASK(ev.xkey.state))
					continue;
				c->tags = keys[i].arg.ui & TM;
				view(&keys[i].arg);
				ocx = c->x;
				ocy = c->y;
				XQueryPointer(display, root, &dw, &dw, &x, &y, &di, &di, &dui);
				break;
			}
		} else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= MOTION_THROTTLE_MS)
				continue;
			last = ev.xmotion.time;
			nx = ocx + ev.xmotion.x - x;
			ny = ocy + ev.xmotion.y - y;

			if (abs(nx - wx) < (int)snap)
				nx = wx;
			else if (abs(wx + ww - W(c) - nx) < (int)snap)
				nx = wx + ww - W(c);
			if (abs(ny - wy) < (int)snap)
				ny = wy;
			else if (abs(wy + wh - H(c) - ny) < (int)snap)
				ny = wy + wh - H(c);

			if (!c->isfloating && ARR &&
			    (abs(nx - ocx) > (int)snap || abs(ny - ocy) > (int)snap)) {
				c->isfloating = c->oldstate = 1;
				needar = 1;
			}
			if (FREE(c))
				resize(c, nx, ny, c->w, c->h, 1);
		}
	} while (ev.type != ButtonRelease);

	XUngrabPointer(display, CurrentTime);
	if (needar)
		ar();
}

static C *
nexttiled(C *c)
{
	for (; c && (c->isfloating || !VIS(c)); c = c->next)
		;
	return c;
}

static void
pop(C *c)
{
	detach(c);
	c->next = clients;
	clients = c;
	focus(c);
	ar();
}

static void
propertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	C *c;

	if (ev->atom == netatom[NetWMStrut] || ev->atom == netatom[NetWMStrutPartial]) {
		if (dockidx(ev->window) < ndock)
			updateworkarea();
		return;
	}
	if (ev->state == PropertyDelete || !(c = wintoclient(ev->window)))
		return;

	if (ev->atom == XA_WM_HINTS) {
		updatewmhints(c);
		BORDER(c, c->isurgent ? color_urg : (c == sel ? color_sel : color_norm));
	} else if (ev->atom == XA_WM_NORMAL_HINTS) {
		c->hintsvalid = 0;
	} else if (ev->atom == netatom[NetWMWindowType]) {
		updatewindowtype(c);
		ar();
	}
}

static void
quit(const A *arg)
{
	(void)arg;
	running = 0;
}

/*
 * Tiled windows intentionally ignore size hints to keep the layout
 * deterministic. Floating windows respect ICCCM size hints.
 */
static void
resize(C *c, int x, int y, int w, int h, int interact)
{
	if (!(interact || FREE(c)) || applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

static void
resizeclient(C *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(display, c->win,
	                 CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
}

static void
resizemouse(const A *arg)
{
	int ocx, ocy, ocw, och, nw, nh, needar = 0;
	XEvent ev;
	Time last = 0;
	C *c = sel;

	(void)arg;
	if (!c || c->isfullscreen)
		return;

	restack();
	ocx = c->x;
	ocy = c->y;
	ocw = c->w;
	och = c->h;

	if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
	                 GrabModeAsync, None, cursor[2], CurrentTime) != GrabSuccess)
		return;

	XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);

	do {
		XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);

		if (ev.type == ConfigureRequest || ev.type == MapRequest) {
			handler[ev.type](&ev);
		} else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= MOTION_THROTTLE_MS)
				continue;
			last = ev.xmotion.time;
			nw = MAX(ev.xmotion.x - ocx - (c->bw << 1) + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - (c->bw << 1) + 1, 1);

			if (!c->isfloating && ARR &&
			    (abs(nw - ocw) > (int)snap || abs(nh - och) > (int)snap)) {
				c->isfloating = c->oldstate = 1;
				needar = 1;
			}
			if (FREE(c))
				resize(c, c->x, c->y, nw, nh, 1);
		}
	} while (ev.type != ButtonRelease);

	XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(display, CurrentTime);
	if (needar)
		ar();
}

static void
restack(void)
{
	C *c;
	XWindowChanges wc;

	if (!sel)
		return;

	if (sel->isfullscreen || !ARR) {
		XRaiseWindow(display, sel->win);
		return;
	}

	/* Tiled windows go below each other in focus order, floating on top. */
	wc.stack_mode = Below;
	wc.sibling = None;
	for (c = stack; c; c = c->snext) {
		if (c->isfloating || !VIS(c))
			continue;
		XConfigureWindow(display, c->win,
		                 wc.sibling ? CWSibling | CWStackMode : CWStackMode, &wc);
		wc.sibling = c->win;
	}
	for (c = stack; c; c = c->snext)
		if (c->isfloating && VIS(c))
			XRaiseWindow(display, c->win);
	if (sel->isfloating)
		XRaiseWindow(display, sel->win);
}

static void
run(void)
{
	XEvent ev;

	XSync(display, False);
	while (running && !XNextEvent(display, &ev))
		if (!XFilterEvent(&ev, None) && ev.type < LASTEvent && handler[ev.type])
			handler[ev.type](&ev);
}

static void
scan(void)
{
	unsigned int i, n;
	int pass;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (!XQueryTree(display, root, &d1, &d2, &wins, &n))
		return;

	/* Manage ordinary windows first, transients second. */
	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < n; i++) {
			if (!XGetWindowAttributes(display, wins[i], &wa) ||
			    wa.override_redirect ||
			    !!XGetTransientForHint(display, wins[i], &d1) != pass)
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
	}
	XFree(wins);
}

static int
sendevent(C *c, Atom proto)
{
	int n, exists = 0;
	Atom *prots;
	XEvent ev = {0};

	if (XGetWMProtocols(display, c->win, &prots, &n)) {
		while (!exists && n--)
			exists = prots[n] == proto;
		XFree(prots);
	}

	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = (long)proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(display, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

static void
setclientstate(C *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(display, c->win, wmatom[WMState], wmatom[WMState], 32,
	                PropModeReplace, (unsigned char *)data, 2);
}

static void
setfocus(C *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(display, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(display, root, netatom[NetActiveWindow], XA_WINDOW, 32,
		                PropModeReplace, (unsigned char *)&c->win, 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

static void
setfullscreen(C *c, int fs)
{
	if (fs == c->isfullscreen)
		return;

	XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
	                PropModeReplace, (unsigned char *)(fs ? &netatom[NetWMFullscreen] : NULL),
	                fs);
	c->isfullscreen = fs;

	if (fs) {
		c->oldstate = c->isfloating;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, 0, 0, screen_w, screen_h);
		XRaiseWindow(display, c->win);
	} else {
		c->isfloating = c->oldstate;
		c->bw = DEFAULT_BORDERPX;
		resizeclient(c, c->oldx, c->oldy, c->oldw, c->oldh);
	}
	ar(); /* also hides the window again if its tag is not visible */
}

static void
setlayout(const A *arg)
{
	if (!arg->v || arg->v == cur.lt[cur.li])
		return;

	/* Two-slot XOR layout history. */
	cur.li ^= 1;
	cur.lt[cur.li] = (const L *)arg->v;
	ptsave();
	ar();
}

static void
setmasterfact(const A *arg)
{
	float f;

	if (!ARR)
		return;

	f = (arg->f < 1.0f) ? cur.mf + arg->f : arg->f - 1.0f;
	cur.mf = MAX(0.05f, MIN(0.95f, f));
	ptsave();
	ar();
}

static void
setup(void)
{
	static char *wmnames[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW", "WM_STATE",
	                           "WM_TAKE_FOCUS" };
	static char *netnames[] = { "_NET_SUPPORTED", "_NET_WM_STATE",
	                            "_NET_WM_STATE_FULLSCREEN", "_NET_ACTIVE_WINDOW",
	                            "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DIALOG",
	                            "_NET_CLIENT_LIST", "_NET_SUPPORTING_WM_CHECK",
	                            "_NET_WM_WINDOW_TYPE_DOCK", "_NET_WM_STRUT",
	                            "_NET_WM_STRUT_PARTIAL", "_NET_WORKAREA",
	                            "_NET_NUMBER_OF_DESKTOPS", "_NET_CURRENT_DESKTOP",
	                            "_NET_DESKTOP_NAMES", "_NET_WM_DESKTOP",
	                            "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_VIEWPORT" };
	static char *auxnames[] = { "_NET_WM_NAME", "UTF8_STRING" };
	XSetWindowAttributes wa;
	static long vp[2 * LEN(tags)]; /* all zero: no large desktops */
	Atom aux[2];
	unsigned int i;
	long nd = LEN(tags), geo[2];
	int scr = DefaultScreen(display);

	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
		die("nwm: signal");

	screen_w = DisplayWidth(display, scr);
	screen_h = DisplayHeight(display, scr);
	root = RootWindow(display, scr);

#if NWM_WITH_BORDERS
	color_norm = getcolor(col_nborder);
	color_sel = getcolor(col_sborder);
	color_urg = getcolor(col_uborder);
#endif

	cursor[0] = XCreateFontCursor(display, XC_left_ptr);
	cursor[1] = XCreateFontCursor(display, XC_fleur);
	cursor[2] = XCreateFontCursor(display, XC_sizing);

	XInternAtoms(display, wmnames, WMLast, False, wmatom);
	XInternAtoms(display, netnames, NetLast, False, netatom);
	XInternAtoms(display, auxnames, 2, False, aux);

	wmcheck_win = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(display, wmcheck_win, netatom[NetWMCheck], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)&wmcheck_win, 1);
	XChangeProperty(display, wmcheck_win, aux[0], aux[1], 8,
	                PropModeReplace, (unsigned char *)"nwm", 3);
	XChangeProperty(display, root, netatom[NetWMCheck], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *)&wmcheck_win, 1);
	XChangeProperty(display, root, netatom[NetSupported], XA_ATOM, 32,
	                PropModeReplace, (unsigned char *)netatom, NetLast);
	XDeleteProperty(display, root, netatom[NetClientList]);

	/* One desktop per tag, so external panels can show them. */
	geo[0] = screen_w;
	geo[1] = screen_h;
	XChangeProperty(display, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&nd, 1);
	XChangeProperty(display, root, netatom[NetDesktopGeometry], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)geo, 2);
	XChangeProperty(display, root, netatom[NetDesktopViewport], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)vp, 2 * LEN(tags));
	for (i = 0; i < LEN(tags); i++)
		XChangeProperty(display, root, netatom[NetDesktopNames], aux[1], 8,
		                i ? PropModeAppend : PropModeReplace,
		                (unsigned char *)tags[i], strlen(tags[i]) + 1);

	tagsel[0] = tagsel[1] = 1;
	cur.mf = (mfact < 0.05f || mfact > 0.95f) ? 0.5f : mfact;
	cur.nm = MAX(nmaster, 0);
	cur.lt[0] = &layouts[0];
	cur.lt[1] = &layouts[1 % LEN(layouts)];
	ptinit();
	updateworkarea(); /* publishes _NET_WORKAREA and the first desktop state */

	grabkeys();

	wa.cursor = cursor[0];
	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
	                ButtonPressMask | EnterWindowMask |
	                StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
	focus(NULL);
}

static void
seturgency(C *c, int urg)
{
	XWMHints *wm;

	c->isurgent = urg;
	BORDER(c, urg ? color_urg : (c == sel ? color_sel : color_norm));

	if (!(wm = XGetWMHints(display, c->win)))
		return;
	wm->flags = urg ? (wm->flags | XUrgencyHint) : (wm->flags & ~XUrgencyHint);
	XSetWMHints(display, c->win, wm);
	XFree(wm);
}

static void
showhide(C *c)
{
	for (; c; c = c->snext) {
		if (!VIS(c))
			XMoveWindow(display, c->win, -(W(c) + screen_w), c->y);
		else if (FREE(c)) /* includes fullscreen: it must come back from off-screen */
			XMoveWindow(display, c->win, c->x, c->y);
	}
}

static void
spawn(const A *arg)
{
	char *const *cmd = (char *const *)arg->v;

	if (fork() == 0) {
		close(ConnectionNumber(display));
		signal(SIGCHLD, SIG_DFL);
		setsid();
		execvp(cmd[0], cmd);
		fprintf(stderr, "nwm: execvp %s failed\n", cmd[0]);
		_exit(1);
	}
}

static void
tag(const A *arg)
{
	if (sel && (arg->ui & TM)) {
		sel->tags = arg->ui & TM;
		focus(NULL);
		ar();
	}
}

/* Lay out n tiled clients from c in one column; returns the first one left. */
static C *
col(C *c, unsigned int n, int x, int w)
{
	unsigned int i;
	int gap = GAP(), y = wy + gap, h, rem = MAX(1, wh - (int)(n + 1) * gap);

	for (i = 0; c && i < n; c = nexttiled(c->next), i++) {
		h = rem / (int)n + (i == n - 1 ? rem % (int)n : 0);
		resize(c, x, y, MAX(1, w - (c->bw << 1)), MAX(1, h - (c->bw << 1)), 0);
		y += h + gap;
	}
	return c;
}

static void
tile(void)
{
	C *c;
	unsigned int n, nm, ns;
	int gap = GAP(), mw;

	for (n = 0, c = nexttiled(clients); c; c = nexttiled(c->next))
		n++;
	if (!n)
		return;

	nm = MIN((unsigned int)cur.nm, n);
	ns = n - nm;
	mw = MAX(1, (nm && ns) ? (int)((ww - 3 * gap) * cur.mf) : ww - 2 * gap);

	c = col(nexttiled(clients), nm, wx + gap, mw);
	if (ns)
		col(c, ns, wx + (nm ? mw + 2 * gap : gap),
		    MAX(1, ww - (nm ? mw + 3 * gap : 2 * gap)));
}

static void
togglefloating(const A *arg)
{
	(void)arg;

	if (!sel || sel->isfullscreen)
		return;

	sel->isfloating = !sel->isfloating || sel->isfixed;
	sel->oldstate = sel->isfloating;
	if (sel->isfloating)
		resize(sel, sel->x, sel->y, sel->w, sel->h, 0);
	ar();
}

static void
togglefullscreen(const A *arg)
{
	(void)arg;
	if (sel)
		setfullscreen(sel, !sel->isfullscreen);
}

static void
toggletag(const A *arg)
{
	unsigned int t;

	if (!sel || !(t = sel->tags ^ (arg->ui & TM)))
		return;
	sel->tags = t;
	focus(NULL);
	ar();
}

static void
toggleview(const A *arg)
{
	unsigned int m = tagsel[sel_group] ^ (arg->ui & TM);

	if (!m)
		return;
	sel_group ^= 1;
	tagsel[sel_group] = m;
	ptload();
	focus(NULL);
	ar();
}

static void
unfocus(C *c)
{
	grabbuttons(c, 0);
	BORDER(c, c->isurgent ? color_urg : color_norm);
}

static void
undock(Window w)
{
	unsigned int i = dockidx(w);

	if (i < ndock) {
		docks[i] = docks[--ndock];
		updateworkarea();
	}
}

static void
unmanage(C *c, int destroyed)
{
	XWindowChanges wc;

	detach(c);
	detachstack(c);

	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(display); /* avoid races with the client */
		XSelectInput(display, c->win, NoEventMask);
		XConfigureWindow(display, c->win, CWBorderWidth, &wc);
		XUngrabButton(display, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(display, False);
		XUngrabServer(display);
	}

	free(c);
	focus(NULL);
	updateclientlist();
	ar();
}

static void
unmapnotify(XEvent *e)
{
	C *c;

	if ((c = wintoclient(e->xunmap.window))) {
		if (e->xunmap.send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else {
		undock(e->xunmap.window);
	}
}

static void
updateclientlist(void)
{
	C *c;

	XDeleteProperty(display, root, netatom[NetClientList]);
	for (c = clients; c; c = c->next)
		XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
		                PropModeAppend, (unsigned char *)&c->win, 1);
}

/* Publish _NET_CURRENT_DESKTOP and _NET_WM_DESKTOP; X is only touched on change. */
static void
updatedesktops(void)
{
	static long last = -1;
	long d = lowtag(tagsel[sel_group]);
	C *c;

	if (d != last) {
		XChangeProperty(display, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
		                PropModeReplace, (unsigned char *)&d, 1);
		last = d;
	}
	for (c = clients; c; c = c->next) {
		if (c->tags == c->pubtags)
			continue;
		c->pubtags = c->tags;
		d = (c->tags == TM) ? 0xFFFFFFFFL : (long)lowtag(c->tags); /* all desktops */
		XChangeProperty(display, c->win, netatom[NetWMDesktop], XA_CARDINAL, 32,
		                PropModeReplace, (unsigned char *)&d, 1);
	}
}

static void
updatenumlockmask(void)
{
	unsigned int i, j;
	KeyCode nlk = XKeysymToKeycode(display, XK_Num_Lock);
	XModifierKeymap *mm = XGetModifierMapping(display);

	if (!mm)
		return;

	numlockmask = 0;
	for (i = 0; nlk && i < 8; i++)
		for (j = 0; j < (unsigned int)mm->max_keypermod; j++)
			if (mm->modifiermap[i * mm->max_keypermod + j] == nlk)
				numlockmask = 1 << i;
	XFreeModifiermap(mm);
}

static void
updatesizehints(C *c)
{
	long ms;
	XSizeHints sz;

	if (!XGetWMNormalHints(display, c->win, &sz, &ms))
		sz.flags = PSize;

	c->basew = (sz.flags & PBaseSize) ? sz.base_width  : (sz.flags & PMinSize) ? sz.min_width  : 0;
	c->baseh = (sz.flags & PBaseSize) ? sz.base_height : (sz.flags & PMinSize) ? sz.min_height : 0;
	c->incw  = (sz.flags & PResizeInc) ? sz.width_inc  : 0;
	c->inch  = (sz.flags & PResizeInc) ? sz.height_inc : 0;
	c->maxw  = (sz.flags & PMaxSize)   ? sz.max_width  : 0;
	c->maxh  = (sz.flags & PMaxSize)   ? sz.max_height : 0;
	c->minw  = (sz.flags & PMinSize)   ? sz.min_width  : c->basew;
	c->minh  = (sz.flags & PMinSize)   ? sz.min_height : c->baseh;

	if ((sz.flags & PAspect) && sz.min_aspect.x && sz.max_aspect.y &&
	    sz.min_aspect.y && sz.max_aspect.x) {
		c->mina = (float)sz.min_aspect.y / sz.min_aspect.x;
		c->maxa = (float)sz.max_aspect.x / sz.max_aspect.y;
	} else {
		c->maxa = c->mina = 0.0f;
	}

	c->isfixed = c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh;
	c->hintsvalid = 1;
}

static void
updatewindowtype(C *c)
{
	if (hasatom(c->win, netatom[NetWMState], netatom[NetWMFullscreen]))
		setfullscreen(c, 1);
	if (hasatom(c->win, netatom[NetWMWindowType], netatom[NetWMWindowTypeDialog]))
		c->isfloating = c->oldstate = 1;
}

static void
updatewmhints(C *c)
{
	XWMHints *wm;

	if (!(wm = XGetWMHints(display, c->win)))
		return;

	if (c == sel && (wm->flags & XUrgencyHint)) {
		wm->flags &= ~XUrgencyHint;
		XSetWMHints(display, c->win, wm);
		c->isurgent = 0;
	} else {
		c->isurgent = !!(wm->flags & XUrgencyHint);
	}
	c->neverfocus = !!(wm->flags & InputHint) && !wm->input;
	XFree(wm);
}

/*
 * Docks reserve screen edges via _NET_WM_STRUT_PARTIAL or _NET_WM_STRUT; the
 * biggest strut per edge wins. Strut ranges are ignored: the X screen is one
 * monitor. Tiling, monocle and snapping use the area that is left.
 */
static void
updateworkarea(void)
{
	Atom type, prop[2] = { netatom[NetWMStrutPartial], netatom[NetWMStrut] };
	long wa[4 * LEN(tags)], e[4] = { 0, 0, 0, 0 }; /* left right top bottom */
	unsigned long n, rem;
	unsigned int i, j, k;
	unsigned char *p;
	int fmt, found, x, y, w, h;

	for (i = 0; i < ndock; i++)
		for (j = 0, found = 0; j < 2 && !found; j++) {
			p = NULL;
			if (XGetWindowProperty(display, docks[i], prop[j], 0L, 4L, False,
			                       XA_CARDINAL, &type, &fmt, &n, &rem, &p) != Success || !p)
				continue;
			if ((found = type == XA_CARDINAL && fmt == 32 && n == 4))
				for (k = 0; k < 4; k++)
					e[k] = MAX(e[k], ((long *)p)[k]);
			XFree(p);
		}

	for (k = 0; k < 4; k++)
		e[k] = MIN(e[k], k < 2 ? screen_w : screen_h);
	x = (int)e[0];
	y = (int)e[2];
	w = MAX(1, screen_w - x - (int)e[1]);
	h = MAX(1, screen_h - y - (int)e[3]);
	if (x == wx && y == wy && w == ww && h == wh)
		return;

	wx = x; wy = y; ww = w; wh = h;
	for (i = 0; i < LEN(tags); i++) {
		wa[4 * i] = x;
		wa[4 * i + 1] = y;
		wa[4 * i + 2] = w;
		wa[4 * i + 3] = h;
	}
	XChangeProperty(display, root, netatom[NetWorkarea], XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)wa, 4 * LEN(tags));
	ar();
}

static void
view(const A *arg)
{
	unsigned int m = arg->ui & TM;

	if (arg->ui && !m) /* tag out of range, not Mod+Tab */
		return;
	/* Mod+Tab (empty mask) swaps between the two saved tag slots. */
	if (m == tagsel[sel_group] || (!m && tagsel[0] == tagsel[1]))
		return;

	sel_group ^= 1;
	if (m)
		tagsel[sel_group] = m;
	ptload();
	focus(NULL);
	ar();
}

static C *
wintoclient(Window w)
{
	C *c;

	for (c = clients; c && c->win != w; c = c->next)
		;
	return c;
}

/*
 * Windows vanish under us all the time (BadWindow) and some requests race
 * with that. None of it is worth killing the session over: log and go on.
 */
static int
xerror(Display *dpy, XErrorEvent *ee)
{
	(void)dpy;

	if (ee->error_code == BadWindow ||
	    (ee->error_code == BadMatch &&
	     (ee->request_code == X_SetInputFocus || ee->request_code == X_ConfigureWindow)))
		return 0;

	if (ee->error_code == BadAccess &&
	    (ee->request_code == X_GrabButton || ee->request_code == X_GrabKey))
		fprintf(stderr, "nwm: warning: failed to grab key/button (BadAccess conflict)\n");
	else
		fprintf(stderr, "nwm: error req=%d code=%d\n", ee->request_code, ee->error_code);
	return 0;
}

static int
xerrorstart(Display *dpy, XErrorEvent *e)
{
	(void)dpy;
	(void)e;
	die("nwm: another wm is running");
	return -1;
}

static void
zoom(const A *arg)
{
	C *c = sel;

	(void)arg;
	if (!ARR || !c || c->isfloating)
		return;
	if (c == nexttiled(clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1])) {
		puts("nwm-" VERSION);
		return 0;
	}
	if (argc != 1)
		die("usage: nwm [-v]");
	if (!(display = XOpenDisplay(NULL)))
		die("nwm: cannot open display");

	checkwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("nwm: pledge");
#endif
	scan();
	run();
	cleanup();
	XCloseDisplay(display);
	return 0;
}
