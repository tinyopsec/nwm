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
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#define BUTTONMASK   (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK    (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(m) ((m) & ~(numlockmask|LockMask) & \
	(ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define VIS(c)       ((c)->tags & ts[sg])
#define W(c)         ((c)->w + ((c)->bw << 1))
#define H(c)         ((c)->h + ((c)->bw << 1))
#define TM           ((1u << LEN(tags)) - 1)
#define LEN(x)       (sizeof(x)/sizeof(*(x)))
#define MAX(a,b)     ((a)>(b)?(a):(b))
#define MIN(a,b)     ((a)<(b)?(a):(b))
#define FRAMERATE    16

#define XP_ConfigureWindow  12
#define XP_GrabButton       28
#define XP_GrabKey          33
#define XP_SetInputFocus    42

typedef union  { int i; unsigned int ui; float f; const void *v; } A;
typedef struct { unsigned int click, mask, button; void (*fn)(const A*); A arg; } B;
typedef struct { unsigned int mod; KeySym key; void (*fn)(const A*); A arg; } K;
typedef struct { void (*ar)(void); } L;
typedef struct C C;

struct C {
	float mina, maxa;
	int x, y, w, h, oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	unsigned int isfixed:1, isfloating:1, isurgent:1, neverfocus:1, oldstate:1, isfullscreen:1, hintsvalid:1;
	C *next, *snext;
	Window win;
};

enum { ClkClientWin, ClkRootWin };
enum { NetWMState, NetWMFullscreen, NetActiveWindow,
       NetWMWindowType, NetWMWindowTypeDialog, NetClientList, NetLast };
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };

static void ar(void);
static void at(C*);
static void bp(XEvent*);
static void chkwm(void);
static void cleanup(void);
static void cmsg(XEvent*);
static void cfgnt(C*);
static void cfgreq(XEvent*);
static void destnot(XEvent*);
static void detach(C*);
static void detachstack(C*);
static void entnot(XEvent*);
static void fc(C*);
static void fcin(XEvent*);
static void fcs(const A*);
static Atom getatom(C*, Atom);
static int  getrootptr(int*, int*);
static long getstate(Window);
static void grabbuttons(C*, int);
static void grabkeys(void);
static void incnm(const A*);
static void kp(XEvent*);
static void killcl(const A*);
static void mg(Window, XWindowAttributes*);
static void mapnot(XEvent*);
static void mapreq(XEvent*);
static void monocle(void);
static void mv(const A*);
static C   *nextt(C*);
static void pop(C*);
static void propnot(XEvent*);
static void quit(const A*);
static void rs(C*, int, int, int, int, int);
static void rcl(C*, int, int, int, int);
static void rz(const A*);
static void rst(void);
static void run(void);
static void scan(void);
static int  sendevent(C*, Atom);
static void setcs(C*, long);
static void setfocus(C*);
static void setfs(C*, int);
static void setlt(const A*);
static void setmf(const A*);
static void setup(void);
static void seturg(C*, int);
static void shide(C*);
static void spawn(const A*);
static void tag(const A*);
static void tile(void);
static void tglfl(const A*);
static void tglfs(const A*);
static void tgltag(const A*);
static void tglview(const A*);
static void unfcs(C*);
static void unmng(C*, int);
static void unmapnt(XEvent*);
static void updcl(void);
static void updnm(void);
static void updsz(C*);
static void updtype(C*);
static void updwmh(C*);
static void view(const A*);
static C   *wintoc(Window);
static int  xerror(Display*, XErrorEvent*);
static int  xe0(Display*, XErrorEvent*);
static int  xerrorstart(Display*, XErrorEvent*);
static void zoom(const A*);

static Display      *d;
static Window        r, wmcheck;
static int           screen, sw, sh, wx, wy, ww, wh;
static int           running = 1;
static unsigned int  numlockmask, sg, li, ts[2];
static float         mf;
static int           nm;
static Cursor        cursor[3];
static Atom          wmatom[WMLast], netatom[NetLast];
static C            *cs, *s, *st;
static const L      *lt[2];
static unsigned long nborder, sborder, uborder;
static int         (*xerrorxlib)(Display*, XErrorEvent*);
static Time          ltime = CurrentTime;

static void (*handler[LASTEvent])(XEvent*) = {
	[ButtonPress]      = bp,
	[ClientMessage]    = cmsg,
	[ConfigureRequest] = cfgreq,
	[DestroyNotify]    = destnot,
	[EnterNotify]      = entnot,
	[FocusIn]          = fcin,
	[KeyPress]         = kp,
	[MappingNotify]    = mapnot,
	[MapRequest]       = mapreq,
	[PropertyNotify]   = propnot,
	[UnmapNotify]      = unmapnt,
};

#include "nwm.h"

static void
die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	if (d) XCloseDisplay(d);
	exit(1);
}

static int
aph(C *c, int *x, int *y, int *w, int *h, int i) {
	int bw2 = c->bw << 1;
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (i) {
		if (*x > sw)            *x = sw - (*w + bw2);
		if (*y > sh)            *y = sh - (*h + bw2);
		if (*x + *w + bw2 < 0) *x = 0;
		if (*y + *h + bw2 < 0) *y = 0;
	} else {
		if (*x >= wx+ww)         *x = wx+ww - (*w + bw2);
		if (*y >= wy+wh)         *y = wy+wh - (*h + bw2);
		if (*x + *w + bw2 <= wx) *x = wx;
		if (*y + *h + bw2 <= wy) *y = wy;
	}
	if (c->isfloating || !lt[li]->ar) {
		if (!c->hintsvalid) updsz(c);
		*w -= c->basew; *h -= c->baseh;
		if (c->mina > 0 && c->maxa > 0 && *w > 0 && *h > 0) {
			if      (c->maxa < (float)*w / *h) *w = (int)(*h * c->maxa + 0.5f);
			else if (c->mina < (float)*h / *w) *h = (int)(*w * c->mina + 0.5f);
		}
		if (c->incw) *w -= *w % c->incw;
		if (c->inch) *h -= *h % c->inch;
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw) *w = MIN(*w, c->maxw);
		if (c->maxh) *h = MIN(*h, c->maxh);
	}
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

static void
ar(void) {
	shide(st);
	if (lt[li]->ar) lt[li]->ar();
	rst();
}

static void
at(C *c) {
	if (attachbottom) {
		C **tc;
		for (tc = &cs; *tc; tc = &(*tc)->next);
		c->next = NULL; *tc = c;
	} else {
		c->next = cs; cs = c;
	}
	c->snext = st; st = c;
}

static void
bp(XEvent *e) {
	unsigned int i, click = ClkRootWin;
	XButtonPressedEvent *ev = &e->xbutton;
	C *c;
	ltime = ev->time;
	if ((c = wintoc(ev->window))) {
		fc(c); rst();
		XAllowEvents(d, ReplayPointer, ev->time);
		click = ClkClientWin;
	}
	for (i = 0; i < LEN(buttons); i++)
		if (click == buttons[i].click && buttons[i].fn
		&& buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].fn(&buttons[i].arg);
}

static void
chkwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	XSelectInput(d, DefaultRootWindow(d), SubstructureRedirectMask);
	XSync(d, False);
	XSetErrorHandler(xerror);
}

static void
cleanup(void) {
	unsigned int i;
	C *c;
	while ((c = cs)) {
		detach(c); detachstack(c);
		XMoveWindow(d, c->win, c->x, c->y);
		setcs(c, WithdrawnState);
		free(c);
	}
	s = st = NULL;
	XUngrabKey(d, AnyKey, AnyModifier, r);
	for (i = 0; i < 3; i++) XFreeCursor(d, cursor[i]);
	XDeleteProperty(d, r, netatom[NetClientList]);
	XDestroyWindow(d, wmcheck);
	XSync(d, False);
	XSetInputFocus(d, PointerRoot, RevertToPointerRoot, CurrentTime);
}

static void
cmsg(XEvent *e) {
	XClientMessageEvent *ev = &e->xclient;
	C *c = wintoc(ev->window);
	if (!c) return;
	if (ev->message_type == netatom[NetWMState]
	&& (ev->data.l[1] == (long)netatom[NetWMFullscreen]
	||  ev->data.l[2] == (long)netatom[NetWMFullscreen]))
		setfs(c, ev->data.l[0] == 1
			|| (ev->data.l[0] == 2 && !c->isfullscreen));
	else if (ev->message_type == netatom[NetActiveWindow] && c != s && !c->isurgent)
		seturg(c, 1);
}

static void
cfgnt(C *c) {
	XConfigureEvent ev = {
		.type = ConfigureNotify, .send_event = True, .display = d,
		.event = c->win, .window = c->win,
		.x = c->x, .y = c->y, .width = c->w, .height = c->h,
		.border_width = c->bw, .above = None, .override_redirect = False,
	};
	XSendEvent(d, c->win, False, StructureNotifyMask, (XEvent*)&ev);
}

static void
cfgreq(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	C *c = wintoc(ev->window);
	if (!c) {
		wc.x = ev->x; wc.y = ev->y;
		wc.width = ev->width; wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above; wc.stack_mode = ev->detail;
		XConfigureWindow(d, ev->window, ev->value_mask, &wc);
		return;
	}
	if (ev->value_mask & CWBorderWidth) {
		c->bw = ev->border_width;
		XSetWindowBorderWidth(d, c->win, c->bw);
		ar();
	}
	if (c->isfloating || !lt[li]->ar) {
		if (ev->value_mask & CWX)      { c->oldx = c->x; c->x = ev->x; }
		if (ev->value_mask & CWY)      { c->oldy = c->y; c->y = ev->y; }
		if (ev->value_mask & CWWidth)  { c->oldw = c->w; c->w = ev->width; }
		if (ev->value_mask & CWHeight) { c->oldh = c->h; c->h = ev->height; }
		if (VIS(c)) {
			XMoveResizeWindow(d, c->win, c->x, c->y, c->w, c->h);
			if (c->isfloating) XRaiseWindow(d, c->win);
		} else {
			cfgnt(c);
		}
	} else {
		cfgnt(c);
	}
}

static void destnot(XEvent *e) { C *c; if ((c = wintoc(e->xdestroywindow.window))) unmng(c, 1); }

static void
detach(C *c) {
	C **tc;
	for (tc = &cs; *tc && *tc != c; tc = &(*tc)->next);
	if (*tc) *tc = c->next;
}

static void
detachstack(C *c) {
	C **tc, *t;
	for (tc = &st; *tc && *tc != c; tc = &(*tc)->snext);
	if (*tc) *tc = c->snext;
	if (c == s) {
		for (t = st; t && !VIS(t); t = t->snext);
		s = t;
	}
}

static void
entnot(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;
	C *c;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != r)
		return;
	ltime = ev->time;
	c = wintoc(ev->window);
	if (!c || c == s) return;
	if (s && s->isfullscreen) return;
	if (c->isfullscreen) return;
	if (s && s->isfloating && !c->isfloating && lt[li]->ar) return;
	fc(c);
	rst();
}

static void
fc(C *c) {
	C **tc;
	if (!c || !VIS(c))
		for (c = st; c && !VIS(c); c = c->snext);
	if (s && s != c) unfcs(s);
	if (c) {
		if (c->isurgent) seturg(c, 0);
		for (tc = &st; *tc && *tc != c; tc = &(*tc)->snext);
		if (*tc) *tc = c->snext;
		c->snext = st; st = c;
		grabbuttons(c, 1);
		XSetWindowBorder(d, c->win, sborder);
		setfocus(c);
	} else {
		XSetInputFocus(d, r, RevertToPointerRoot, ltime);
		XDeleteProperty(d, r, netatom[NetActiveWindow]);
	}
	s = c;
}

static void fcin(XEvent *e) { if (s && e->xfocus.window != s->win) setfocus(s); }

static void
fcs(const A *arg) {
	C *c = NULL, *i;
	int tiled = !!lt[li]->ar;
	if (!s || s->isfullscreen) return;
	if (arg->i > 0) {
		for (c = s->next; c && (!VIS(c) || (tiled && c->isfloating)); c = c->next);
		if (!c) for (c = cs; c && (!VIS(c) || (tiled && c->isfloating)); c = c->next);
	} else {
		for (i = cs; i != s; i = i->next) if (VIS(i) && (!tiled || !i->isfloating)) c = i;
		if (!c) for (; i; i = i->next)    if (VIS(i) && (!tiled || !i->isfloating)) c = i;
	}
	if (c) { fc(c); rst(); }
}

static Atom
getatom(C *c, Atom prop) {
	int fmt; unsigned long n, rem; unsigned char *p = NULL; Atom type, a = None;
	if (XGetWindowProperty(d, c->win, prop, 0L, 1L, False, XA_ATOM,
		&type, &fmt, &n, &rem, &p) == Success && p) {
		if (type == XA_ATOM && n > 0) memcpy(&a, p, sizeof(Atom));
		XFree(p);
	}
	return a;
}

static int getrootptr(int *x, int *y) { int di; unsigned int dui; Window dw; return XQueryPointer(d, r, &dw, &dw, x, y, &di, &di, &dui); }

static long
getstate(Window w) {
	int fmt; long res = -1; unsigned char *p = NULL; unsigned long n, ex; Atom real;
	if (XGetWindowProperty(d, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &fmt, &n, &ex, &p) == Success) {
		if (n && fmt == 32 && p) memcpy(&res, p, sizeof(long));
		if (p) XFree(p);
	}
	return res;
}

static void
grabbuttons(C *c, int focused) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j;
	XUngrabButton(d, AnyButton, AnyModifier, c->win);
	if (!focused) {
		XGrabButton(d, AnyButton, AnyModifier, c->win, False,
			BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		return;
	}
	for (i = 0; i < LEN(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < 4; j++)
				XGrabButton(d, buttons[i].button, buttons[i].mask | mods[j],
					c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}

static void
grabkeys(void) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j; KeyCode code;
	updnm();
	XUngrabKey(d, AnyKey, AnyModifier, r);
	for (i = 0; i < LEN(keys); i++)
		if ((code = XKeysymToKeycode(d, keys[i].key)))
			for (j = 0; j < 4; j++)
				XGrabKey(d, code, keys[i].mod | mods[j],
					r, True, GrabModeAsync, GrabModeAsync);
}

static void incnm(const A *arg) { nm = (nm + arg->i > 0) ? nm + arg->i : 0; ar(); }

static void
kp(XEvent *e) {
	unsigned int i;
	XKeyEvent *ev = &e->xkey;
	KeySym sym = XLookupKeysym(ev, 0);
	ltime = ev->time;
	for (i = 0; i < LEN(keys); i++)
		if (sym == keys[i].key
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].fn)
			keys[i].fn(&keys[i].arg);
}

static void
killcl(const A *arg) {
	(void)arg;
	if (!s) return;
	if (!sendevent(s, wmatom[WMDelete])) {
		XGrabServer(d);
		XSetErrorHandler(xe0);
		XSetCloseDownMode(d, DestroyAll);
		XKillClient(d, s->win);
		XSetCloseDownMode(d, RetainPermanent);
		XSync(d, False);
		XSetErrorHandler(xerror);
		XUngrabServer(d);
	}
}

static void
mg(Window w, XWindowAttributes *wa) {
	C *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;
	if (!(c = calloc(1, sizeof(C)))) die("nwm: calloc");
	c->win   = w;
	c->x     = c->oldx = wa->x;
	c->y     = c->oldy = wa->y;
	c->w     = c->oldw = wa->width;
	c->h     = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	updsz(c);
	updwmh(c);
	c->tags = ts[sg];
	if (XGetTransientForHint(d, w, &trans) && (t = wintoc(trans)))
		c->tags = t->tags;
	c->bw = borderpx;
	if (c->x + W(c) > wx+ww) c->x = wx+ww - W(c);
	if (c->y + H(c) > wy+wh) c->y = wy+wh - H(c);
	c->x = MAX(c->x, wx);
	c->y = MAX(c->y, wy);
	wc.border_width = c->bw;
	XConfigureWindow(d, w, CWBorderWidth, &wc);
	XSetWindowBorder(d, w, nborder);
	cfgnt(c);
	c->isfloating = c->oldstate = trans != None || c->isfixed;
	updtype(c);
	XSelectInput(d, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	at(c);
	XChangeProperty(d, r, netatom[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char*)&w, 1);
	if (c->isfloating) XMapRaised(d, c->win);
	else               XMapWindow(d, c->win);
	setcs(c, NormalState);
	if (focusonopen) fc(c);
	ar();
}

static void
mapnot(XEvent *e) {
	XRefreshKeyboardMapping(&e->xmapping);
	if (e->xmapping.request == MappingKeyboard) grabkeys();
}

static void
mapreq(XEvent *e) {
	XWindowAttributes wa;
	C *c;
	if (!XGetWindowAttributes(d, e->xmaprequest.window, &wa) || wa.override_redirect) return;
	if ((c = wintoc(e->xmaprequest.window))) {
		XMapWindow(d, c->win);
		setcs(c, NormalState);
		ar();
		return;
	}
	mg(e->xmaprequest.window, &wa);
}

static void
monocle(void) {
	C *c;
	for (c = nextt(cs); c; c = nextt(c->next))
		rs(c, wx, wy, MAX(1, ww - (c->bw << 1)), MAX(1, wh - (c->bw << 1)), 0);
}

static void
mv(const A *arg) {
	(void)arg;
	int x, y, ocx, ocy, nx, ny, needar = 0;
	XEvent ev;
	Time last = 0;
	C *c = s;
	if (!c || c->isfullscreen) return;
	rst();
	ocx = c->x; ocy = c->y;
	if (XGrabPointer(d, r, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[1], ltime) != GrabSuccess) return;
	if (!getrootptr(&x, &y)) { XUngrabPointer(d, CurrentTime); return; }
	do {
		XMaskEvent(d, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= FRAMERATE) continue;
			last = ev.xmotion.time;
			nx = ocx + ev.xmotion.x - x;
			ny = ocy + ev.xmotion.y - y;
			if (abs(wx - nx)              < (int)snap) nx = wx;
			else if (abs(wx+ww-W(c) - nx) < (int)snap) nx = wx+ww - W(c);
			if (abs(wy - ny)              < (int)snap) ny = wy;
			else if (abs(wy+wh-H(c) - ny) < (int)snap) ny = wy+wh - H(c);
			if (!c->isfloating && lt[li]->ar
			&& (abs(nx-ocx) > (int)snap || abs(ny-ocy) > (int)snap)) {
				c->isfloating = c->oldstate = 1; needar = 1;
			}
			if (!lt[li]->ar || c->isfloating)
				rs(c, nx, ny, c->w, c->h, 1);
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(d, CurrentTime);
	if (needar) ar();
}

static C *nextt(C *c) { for (; c && (c->isfloating || !VIS(c)); c = c->next); return c; }

static void pop(C *c) { detach(c); c->next = cs; cs = c; fc(c); ar(); }

static void
propnot(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;
	C *c;
	if (ev->state == PropertyDelete || !(c = wintoc(ev->window))) return;
	if      (ev->atom == XA_WM_HINTS) {
		updwmh(c);
		XSetWindowBorder(d, c->win, c->isurgent ? uborder : (c == s ? sborder : nborder));
	}
	else if (ev->atom == XA_WM_NORMAL_HINTS)       c->hintsvalid = 0;
	else if (ev->atom == netatom[NetWMWindowType]) updtype(c);
}

static void quit(const A *arg) { (void)arg; running = 0; }

static void
rs(C *c, int x, int y, int w, int h, int i) {
	if (i || c->isfloating || !lt[li]->ar) {
		if (aph(c, &x, &y, &w, &h, i)) rcl(c, x, y, w, h);
	} else {
		rcl(c, x, y, w, h);
	}
}

static void
rcl(C *c, int x, int y, int w, int h) {
	XWindowChanges wc;
	c->oldx = c->x; c->x = x;
	c->oldy = c->y; c->y = y;
	c->oldw = c->w; c->w = w;
	c->oldh = c->h; c->h = h;
	wc.x = x; wc.y = y; wc.width = w; wc.height = h; wc.border_width = c->bw;
	XConfigureWindow(d, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
}

static void
rz(const A *arg) {
	(void)arg;
	int ocx, ocy, ocw, och, nw, nh, needar = 0;
	XEvent ev;
	Time last = 0;
	C *c = s;
	if (!c || c->isfullscreen) return;
	rst();
	ocx = c->x; ocy = c->y; ocw = c->w; och = c->h;
	if (XGrabPointer(d, r, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[2], ltime) != GrabSuccess) return;
	XWarpPointer(d, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(d, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= FRAMERATE) continue;
			last = ev.xmotion.time;
			nw = MAX(ev.xmotion.x - ocx - (c->bw << 1) + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - (c->bw << 1) + 1, 1);
			if (!c->isfloating && lt[li]->ar
			&& (abs(nw-ocw) > (int)snap || abs(nh-och) > (int)snap)) {
				c->isfloating = c->oldstate = 1; needar = 1;
			}
			if (!lt[li]->ar || c->isfloating)
				rs(c, c->x, c->y, nw, nh, 1);
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(d, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(d, CurrentTime);
	if (needar) ar();
}

static void
rst(void) {
	C *c;
	XWindowChanges wc = { .stack_mode = Above };
	if (!s) return;
	if (s->isfullscreen) {
		XRaiseWindow(d, s->win);
		return;
	}
	if (s->isfloating || !lt[li]->ar) XRaiseWindow(d, s->win);
	if (lt[li]->ar) {
		for (c = st; c; c = c->snext)
			if (!c->isfloating && VIS(c)) {
				XConfigureWindow(d, c->win,
					wc.sibling ? CWSibling|CWStackMode : CWStackMode, &wc);
				wc.sibling = c->win;
			}
		for (c = st; c; c = c->snext)
			if (c->isfloating && VIS(c))
				XRaiseWindow(d, c->win);
		if (s->isfloating) XRaiseWindow(d, s->win);
	}
}

static void
run(void) {
	XEvent ev;
	XSync(d, False);
	while (running && !XNextEvent(d, &ev))
		if (!XFilterEvent(&ev, None) && ev.type < LASTEvent && handler[ev.type])
			handler[ev.type](&ev);
}

static void
scan(void) {
	unsigned int i, n;
	Window d1, d2, trans, *wins = NULL;
	XWindowAttributes wa;
	if (!XQueryTree(d, r, &d1, &d2, &wins, &n)) return;
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(d, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(d, wins[i], &trans)) continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
			mg(wins[i], &wa);
	}
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(d, wins[i], &wa) || wa.override_redirect) continue;
		if (XGetTransientForHint(d, wins[i], &trans)
		&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
			mg(wins[i], &wa);
	}
	XFree(wins);
}

static int
sendevent(C *c, Atom proto) {
	int n, exists = 0; Atom *prots; XEvent ev = {0};
	if (XGetWMProtocols(d, c->win, &prots, &n)) {
		while (!exists && n--) exists = prots[n] == proto;
		XFree(prots);
	}
	if (exists) {
		ev.type                 = ClientMessage;
		ev.xclient.window       = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format       = 32;
		ev.xclient.data.l[0]    = proto;
		ev.xclient.data.l[1]    = ltime;
		XSendEvent(d, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

static void
setcs(C *c, long state) {
	long data[] = { state, None };
	XChangeProperty(d, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char*)data, 2);
}

static void
setfocus(C *c) {
	if (!c->neverfocus) {
		XSetInputFocus(d, c->win, RevertToPointerRoot, ltime);
		XChangeProperty(d, r, netatom[NetActiveWindow], XA_WINDOW, 32,
			PropModeReplace, (unsigned char*)&c->win, 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

static void
setfs(C *c, int fs) {
	int fmt, i, j;
	unsigned long n, rem;
	unsigned char *p = NULL;
	Atom type, *atoms, *na;
	if (fs && !c->isfullscreen) {
		XChangeProperty(d, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating; c->oldbw = c->bw;
		c->bw = 0; c->isfloating = 1;
		rcl(c, 0, 0, sw, sh);
		XRaiseWindow(d, c->win);
	} else if (!fs && c->isfullscreen) {
		if (XGetWindowProperty(d, c->win, netatom[NetWMState], 0L,
			1024L, False, XA_ATOM, &type, &fmt, &n, &rem, &p) == Success && p) {
			if (type != XA_ATOM || fmt != 32 || n == 0) {
				XDeleteProperty(d, c->win, netatom[NetWMState]);
			} else {
				atoms = (Atom *)p;
				if (!(na = (Atom *)malloc(n * sizeof(Atom)))) die("nwm: malloc");
				for (i = 0, j = 0; i < (int)n; i++)
					if (atoms[i] != netatom[NetWMFullscreen])
						na[j++] = atoms[i];
				if (j == 0)
					XDeleteProperty(d, c->win, netatom[NetWMState]);
				else
					XChangeProperty(d, c->win, netatom[NetWMState], XA_ATOM, 32,
						PropModeReplace, (unsigned char*)na, j);
				free(na);
			}
			XFree(p);
		} else {
			XDeleteProperty(d, c->win, netatom[NetWMState]);
		}
		c->isfullscreen = 0; c->isfloating = c->oldstate; c->bw = c->oldbw;
		rcl(c, c->oldx, c->oldy, c->oldw, c->oldh);
		ar();
	}
}

static void
setlt(const A *arg) {
	if (arg->v == lt[li]) return;
	li ^= 1;
	lt[li] = (const L*)arg->v;
	if (s) ar();
}

static void
setmf(const A *arg) {
	float f;
	if (!arg || !lt[li]->ar) return;
	f = (arg->f < 1.0f) ? mf + arg->f : arg->f - 1.0f;
	if (f < 0.05f || f > 0.95f) return;
	mf = f; ar();
}

static void
setup(void) {
	static char *wmnames[]  = { "WM_PROTOCOLS", "WM_DELETE_WINDOW",
	                            "WM_STATE", "WM_TAKE_FOCUS" };
	static char *netnames[] = { "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN",
	                            "_NET_ACTIVE_WINDOW", "_NET_WM_WINDOW_TYPE",
	                            "_NET_WM_WINDOW_TYPE_DIALOG", "_NET_CLIENT_LIST" };
	static char *auxnames[] = { "_NET_SUPPORTING_WM_CHECK", "_NET_WM_NAME", "UTF8_STRING" };
	struct sigaction sa = {0};
	XSetWindowAttributes wa;
	XColor xc, exact; Colormap cmap;
	Atom aux[3];

	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) == -1) die("nwm: sigaction");

	screen = DefaultScreen(d);
	sw = DisplayWidth(d, screen);
	sh = DisplayHeight(d, screen);
	r  = RootWindow(d, screen);
	wx = wy = 0; ww = sw; wh = sh;

	cmap = DefaultColormap(d, screen);
	if (!XAllocNamedColor(d, cmap, colnb, &xc, &exact)) die("nwm: cannot allocate color");
	nborder = xc.pixel;
	if (!XAllocNamedColor(d, cmap, colsb, &xc, &exact)) die("nwm: cannot allocate color");
	sborder = xc.pixel;
	if (!XAllocNamedColor(d, cmap, colub, &xc, &exact)) die("nwm: cannot allocate color");
	uborder = xc.pixel;

	cursor[0] = XCreateFontCursor(d, XC_left_ptr);
	cursor[1] = XCreateFontCursor(d, XC_fleur);
	cursor[2] = XCreateFontCursor(d, XC_sizing);
	if (!cursor[0] || !cursor[1] || !cursor[2]) die("nwm: XCreateFontCursor");

	if (!XInternAtoms(d, wmnames,  WMLast,  False, wmatom))  die("nwm: XInternAtoms");
	if (!XInternAtoms(d, netnames, NetLast, False, netatom)) die("nwm: XInternAtoms");
	if (!XInternAtoms(d, auxnames, 3,       False, aux))     die("nwm: XInternAtoms");
	wmcheck = XCreateSimpleWindow(d, r, 0, 0, 1, 1, 0, 0, 0);
	if (wmcheck == None) die("nwm: XCreateSimpleWindow");
	XChangeProperty(d, wmcheck, aux[0], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheck, 1);
	XChangeProperty(d, wmcheck, aux[1], aux[2], 8,
		PropModeReplace, (unsigned char*)"nwm", 3);
	XChangeProperty(d, r, aux[0], XA_WINDOW, 32,
		PropModeReplace, (unsigned char*)&wmcheck, 1);
	XDeleteProperty(d, r, netatom[NetClientList]);

	sg = li = 0;
	ts[0] = ts[1] = 1;
	mf  = mfact;
	nm  = nmaster;
	lt[0] = &layouts[0];
	lt[1] = &layouts[1 % LEN(layouts)];

	grabkeys();
	wa.cursor     = cursor[0];
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
	              | ButtonPressMask|PointerMotionMask|EnterWindowMask
	              | StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(d, r, CWEventMask|CWCursor, &wa);
	fc(NULL);
}

static void
seturg(C *c, int urg) {
	XWMHints *wm;
	c->isurgent = urg;
	XSetWindowBorder(d, c->win, urg ? uborder : (c == s ? sborder : nborder));
	if (!(wm = XGetWMHints(d, c->win))) return;
	wm->flags = urg ? wm->flags | XUrgencyHint : wm->flags & ~XUrgencyHint;
	XSetWMHints(d, c->win, wm); XFree(wm);
}

static void
shide(C *c) {
	for (; c; c = c->snext) {
		if (VIS(c)) {
			if ((!lt[li]->ar || c->isfloating) && !c->isfullscreen)
				rcl(c, c->x, c->y, c->w, c->h);
		} else {
			XMoveWindow(d, c->win, -(W(c) + sw), c->y);
		}
	}
}

static void
spawn(const A *arg) {
	struct sigaction sa = {0};
	pid_t pid = fork();
	if (pid == -1) { fprintf(stderr, "nwm: fork\n"); return; }
	if (pid == 0) {
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGCHLD, &sa, NULL);
		if (d) close(ConnectionNumber(d));
		setsid();
		execvp(((const char**)arg->v)[0], (char*const*)arg->v);
		fprintf(stderr, "nwm: execvp %s\n", ((const char**)arg->v)[0]);
		_exit(1);
	}
}

static void tag(const A *arg) { if (s && arg->ui & TM) { s->tags = arg->ui & TM; fc(NULL); ar(); } }

static void
tile(void) {
	C *c;
	unsigned int i, n, nmv, ns;
	int g = gappx, mw, mch, sch, y0;
	for (n = 0, c = nextt(cs); c; c = nextt(c->next), n++);
	if (!n) return;
	nmv = (unsigned)nm < n ? (unsigned)nm : n;
	ns  = n > nmv ? n - nmv : 0;
	mw  = nmv && ns ? (int)((ww - 3*g) * mf) : (nmv ? ww - 2*g : 0);
	mch = nmv ? MAX(1, (wh - (int)(nmv + 1) * g) / (int)nmv) : 0;
	sch = ns  ? MAX(1, (wh - (int)(ns  + 1) * g) / (int)ns)  : 0;
	for (i = 0, c = nextt(cs); c; c = nextt(c->next), i++) {
		int bw2 = c->bw << 1;
		if (i < nmv) {
			y0 = wy + g + (int)i * (mch + g);
			rs(c, wx + g, y0, MAX(1, mw - bw2), MAX(1, mch - bw2), 0);
		} else {
			y0 = wy + g + (int)(i - nmv) * (sch + g);
			rs(c, wx + (nmv ? mw + 2*g : g), y0,
				MAX(1, (nmv ? ww - mw - 3*g : ww - 2*g) - bw2),
				MAX(1, sch - bw2), 0);
		}
	}
}

static void
tglfl(const A *arg) {
	(void)arg;
	if (!s || s->isfullscreen) return;
	s->isfloating = !s->isfloating || s->isfixed;
	if (s->isfloating) rs(s, s->x, s->y, s->w, s->h, 0);
	ar();
}

static void tglfs(const A *arg) { (void)arg; if (s) setfs(s, !s->isfullscreen); }

static void
tgltag(const A *arg) {
	unsigned int t;
	if (!s || !(t = s->tags ^ (arg->ui & TM))) return;
	s->tags = t; fc(NULL); ar();
}

static void
tglview(const A *arg) {
	unsigned int t = ts[sg] ^ (arg->ui & TM);
	if (t) { ts[sg] = t; fc(NULL); ar(); }
}

static void
unfcs(C *c) {
	if (!c) return;
	grabbuttons(c, 0);
	XSetWindowBorder(d, c->win, c->isurgent ? uborder : nborder);
}

static void
unmng(C *c, int destroyed) {
	XWindowChanges wc;
	detach(c); detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(d);
		XSetErrorHandler(xe0);
		XSelectInput(d, c->win, NoEventMask);
		XConfigureWindow(d, c->win, CWBorderWidth, &wc);
		XUngrabButton(d, AnyButton, AnyModifier, c->win);
		setcs(c, WithdrawnState);
		XSync(d, False);
		XSetErrorHandler(xerror);
		XUngrabServer(d);
	}
	free(c);
	fc(NULL); updcl(); ar();
}

static void
unmapnt(XEvent *e) {
	XUnmapEvent *ev = &e->xunmap;
	C *c;
	if ((c = wintoc(ev->window))) {
		if (ev->send_event) setcs(c, WithdrawnState);
		else unmng(c, 0);
	}
}

static void
updcl(void) {
	C *c;
	XDeleteProperty(d, r, netatom[NetClientList]);
	for (c = cs; c; c = c->next)
		XChangeProperty(d, r, netatom[NetClientList], XA_WINDOW, 32,
			PropModeAppend, (unsigned char*)&c->win, 1);
}

static void
updnm(void) {
	unsigned int i, j;
	KeyCode nlk;
	XModifierKeymap *mm = XGetModifierMapping(d);
	if (!mm) return;
	numlockmask = 0;
	nlk = XKeysymToKeycode(d, XK_Num_Lock);
	if (nlk)
		for (i = 0; i < 8; i++)
			for (j = 0; j < (unsigned)mm->max_keypermod; j++)
				if (mm->modifiermap[i * mm->max_keypermod + j] == nlk) {
					numlockmask = 1 << i;
					goto done;
				}
done:
	XFreeModifiermap(mm);
}

static void
updsz(C *c) {
	long ms; XSizeHints sz;
	if (!XGetWMNormalHints(d, c->win, &sz, &ms)) sz.flags = PSize;
	c->basew = (sz.flags & PBaseSize) ? sz.base_width
	         : (sz.flags & PMinSize)  ? sz.min_width  : 0;
	c->baseh = (sz.flags & PBaseSize) ? sz.base_height
	         : (sz.flags & PMinSize)  ? sz.min_height : 0;
	c->incw  = (sz.flags & PResizeInc) ? sz.width_inc  : 0;
	c->inch  = (sz.flags & PResizeInc) ? sz.height_inc : 0;
	c->maxw  = (sz.flags & PMaxSize)   ? sz.max_width  : 0;
	c->maxh  = (sz.flags & PMaxSize)   ? sz.max_height : 0;
	c->minw  = (sz.flags & PMinSize)   ? sz.min_width  : c->basew;
	c->minh  = (sz.flags & PMinSize)   ? sz.min_height : c->baseh;
	if ((sz.flags & PAspect) && sz.min_aspect.x && sz.max_aspect.y
	    && sz.min_aspect.y && sz.max_aspect.x) {
		c->mina = (float)sz.min_aspect.y / sz.min_aspect.x;
		c->maxa = (float)sz.max_aspect.x / sz.max_aspect.y;
	} else {
		c->maxa = c->mina = 0.0f;
	}
	c->isfixed    = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

static void
updtype(C *c) {
	int fmt, i, fs = 0;
	unsigned long n, rem;
	unsigned char *p = NULL;
	Atom type, *atoms;
	if (XGetWindowProperty(d, c->win, netatom[NetWMState], 0L,
		1024L, False, XA_ATOM, &type, &fmt, &n, &rem, &p) == Success && p) {
		if (type == XA_ATOM && fmt == 32) {
			atoms = (Atom *)p;
			for (i = 0; i < (int)n; i++)
				if (atoms[i] == netatom[NetWMFullscreen]) { fs = 1; break; }
		}
		XFree(p);
	}
	if (fs && !c->isfullscreen) setfs(c, 1);
	else if (!fs && c->isfullscreen) setfs(c, 0);
	if (getatom(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
	else
		c->isfloating = c->oldstate || c->isfixed;
}

static void
updwmh(C *c) {
	XWMHints *wm;
	if (!(wm = XGetWMHints(d, c->win))) return;
	if (c == s && wm->flags & XUrgencyHint) {
		wm->flags &= ~XUrgencyHint; XSetWMHints(d, c->win, wm);
	} else {
		c->isurgent = !!(wm->flags & XUrgencyHint);
	}
	c->neverfocus = !!(wm->flags & InputHint) && !wm->input;
	XFree(wm);
}

static void
view(const A *arg) {
	if ((arg->ui & TM) == ts[sg]) return;
	if (!(arg->ui & TM) && ts[0] == ts[1]) return;
	sg ^= 1;
	if (arg->ui & TM) ts[sg] = arg->ui & TM;
	fc(NULL); ar();
}

static C *wintoc(Window w) { C *c; for (c = cs; c; c = c->next) if (c->win == w) return c; return NULL; }

static int
xerror(Display *dpy, XErrorEvent *ee) {
	if (ee->error_code == BadWindow
	|| (ee->request_code == XP_SetInputFocus   && ee->error_code == BadMatch)
	|| (ee->request_code == XP_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == XP_GrabButton      && ee->error_code == BadAccess)
	|| (ee->request_code == XP_GrabKey         && ee->error_code == BadAccess))
		return 0;
	fprintf(stderr, "nwm: error req=%d code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee);
}

static int xe0(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; return 0; }
static int xerrorstart(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; fprintf(stderr, "nwm: another wm is running\n"); exit(1); }

static void
zoom(const A *arg) {
	(void)arg;
	C *c = s;
	if (!lt[li]->ar || !c || c->isfloating) return;
	if (c == nextt(cs) && !(c = nextt(c->next))) return;
	pop(c);
}

int
main(int argc, char *argv[]) {
	if (argc == 2 && !strcmp("-v", argv[1])) die("nwm-1.5");
	else if (argc != 1) die("usage: nwm [-v]");
	if (!(d = XOpenDisplay(NULL))) die("nwm: cannot open display");
	chkwm();
	setup();
#ifdef __OpenBSD__
	pledge("stdio rpath proc exec ps", NULL);
#endif
	scan(); run(); cleanup();
	XCloseDisplay(d);
	return 0;
}
