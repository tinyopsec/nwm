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

#define BUTTONMASK    (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK     (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(m)  ((m) & ~(numlockmask|LockMask) & \
	(ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define VIS(c)        ((c)->tags & ts[sg])
#define W(c)          ((c)->w + 2*(c)->bw)
#define H(c)          ((c)->h + 2*(c)->bw)
#define TM            ((1u << LEN(tags)) - 1)
#define LEN(x)        (sizeof(x)/sizeof(*(x)))
#define MAX(a,b)      ((a)>(b)?(a):(b))
#define MIN(a,b)      ((a)<(b)?(a):(b))

typedef union  { int i; unsigned int ui; float f; const void *v; } A;
typedef struct { unsigned int click, mask, button; void (*fn)(const A*); A arg; } B;
typedef struct { unsigned int mod; KeySym key; void (*fn)(const A*); A arg; } K;
typedef struct { void (*ar)(void); } L;
typedef struct C C;

struct C {
	float mina, maxa;
	int x, y, w, h, oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
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
static void checkotherwm(void);
static void cleanup(void);
static void clientmessage(XEvent*);
static void configure(C*);
static void configurerequest(XEvent*);
static void destroynotify(XEvent*);
static void detach(C*);
static void enternotify(XEvent*);
static void fc(C*);
static void focusin(XEvent*);
static void focusstack(const A*);
static Atom getatom(C*, Atom);
static int  getrootptr(int*, int*);
static long getstate(Window);
static void grabbuttons(C*, int);
static void grabkeys(void);
static void incnmaster(const A*);
static void kp(XEvent*);
static void killclient(const A*);
static void mg(Window, XWindowAttributes*);
static void mappingnotify(XEvent*);
static void maprequest(XEvent*);
static void monocle(void);
static void mv(const A*);
static C   *nexttiled(C*);
static void pop(C*);
static void propertynotify(XEvent*);
static void quit(const A*);
static void rs(C*, int, int, int, int, int);
static void resizeclient(C*, int, int, int, int);
static void rz(const A*);
static void restack(void);
static void run(void);
static void scan(void);
static int  sendevent(C*, Atom);
static void setclientstate(C*, long);
static void setfocus(C*);
static void setfullscreen(C*, int);
static void setlayout(const A*);
static void setmfact(const A*);
static void setup(void);
static void seturgent(C*, int);
static void showhide(C*);
static void spawn(const A*);
static void tag(const A*);
static void tile(void);
static void togglefloating(const A*);
static void togglefullscreen(const A*);
static void toggletag(const A*);
static void toggleview(const A*);
static void unfocus(C*, int);
static void unmanage(C*, int);
static void unmapnotify(XEvent*);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(C*);
static void updatewindowtype(C*);
static void updatewmhints(C*);
static void view(const A*);
static C   *wintoclient(Window);
static int  xerror(Display*, XErrorEvent*);
static int  xerrordummy(Display*, XErrorEvent*);
static int  xerrorstart(Display*, XErrorEvent*);
static void zoom(const A*);

static Display      *d;
static Window        r, wmcheck;
static int           screen, sw, sh, wx, wy, ww, wh;
static int           running = 1;
static unsigned int  numlockmask, sg, lt2, ts[2];
static float         mf;
static int           nm;
static Cursor        cursor[3];
static Atom          wmatom[WMLast], netatom[NetLast];
static C            *cs, *s, *st;
static const L      *lt[2];
static unsigned long nborder, sborder, uborder;
static int         (*xerrorxlib)(Display*, XErrorEvent*);

static void (*handler[LASTEvent])(XEvent*) = {
	[ButtonPress]      = bp,
	[ClientMessage]    = clientmessage,
	[ConfigureRequest] = configurerequest,
	[DestroyNotify]    = destroynotify,
	[EnterNotify]      = enternotify,
	[FocusIn]          = focusin,
	[KeyPress]         = kp,
	[MappingNotify]    = mappingnotify,
	[MapRequest]       = maprequest,
	[PropertyNotify]   = propertynotify,
	[UnmapNotify]      = unmapnotify,
};

#include "nwm.h"

static void
die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static int
applysizehints(C *c, int *x, int *y, int *w, int *h, int interact) {
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)                  *x = sw - W(c);
		if (*y > sh)                  *y = sh - H(c);
		if (*x + *w + 2*c->bw < 0)   *x = 0;
		if (*y + *h + 2*c->bw < 0)   *y = 0;
	} else {
		if (*x >= wx+ww)              *x = wx+ww - W(c);
		if (*y >= wy+wh)              *y = wy+wh - H(c);
		if (*x + *w + 2*c->bw <= wx)  *x = wx;
		if (*y + *h + 2*c->bw <= wy)  *y = wy;
	}
	if (c->isfloating || !lt[lt2]->ar) {
		if (!c->hintsvalid) updatesizehints(c);
		int bim = c->basew == c->minw && c->baseh == c->minh;
		if (!bim) { *w -= c->basew; *h -= c->baseh; }
		if (c->mina > 0 && c->maxa > 0) {
			if      (c->maxa < (float)*w / *h) *w = (int)(*h * c->maxa + 0.5f);
			else if (c->mina < (float)*h / *w) *h = (int)(*w * c->mina + 0.5f);
		}
		if (bim) { *w -= c->basew; *h -= c->baseh; }
		if (c->incw) *w -= *w % c->incw;
		if (c->inch) *h -= *h % c->inch;
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw) *w = MIN(*w, c->maxw);
		if (c->maxh) *h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void ar(void) { showhide(st); if (lt[lt2]->ar) lt[lt2]->ar(); restack(); }

void at(C *c) {
	if (attachbottom) {
		C **tc;
		for (tc = &cs; *tc; tc = &(*tc)->next);
		c->next = NULL; *tc = c;
	} else {
		c->next = cs; cs = c;
	}
	c->snext = st; st = c;
}

void bp(XEvent *e) {
	unsigned int i, click = ClkRootWin;
	XButtonPressedEvent *ev = &e->xbutton;
	C *c;
	if ((c = wintoclient(ev->window))) {
		fc(c); restack();
		XAllowEvents(d, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LEN(buttons); i++)
		if (click == buttons[i].click && buttons[i].fn
		&& buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].fn(&buttons[i].arg);
}

void checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	XSelectInput(d, DefaultRootWindow(d), SubstructureRedirectMask);
	XSync(d, False);
	XSetErrorHandler(xerror);
	XSync(d, False);
}

void cleanup(void) {
	A a = {.ui = ~0};
	view(&a);
	while (st) unmanage(st, 0);
	XUngrabKey(d, AnyKey, AnyModifier, r);
	XFreeCursor(d, cursor[0]);
	XFreeCursor(d, cursor[1]);
	XFreeCursor(d, cursor[2]);
	XDeleteProperty(d, r, netatom[NetClientList]);
	XDestroyWindow(d, wmcheck);
	XSync(d, False);
	XSetInputFocus(d, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void clientmessage(XEvent *e) {
	XClientMessageEvent *ev = &e->xclient;
	C *c = wintoclient(ev->window);
	if (!c) return;
	if (ev->message_type == netatom[NetWMState]
	&& (ev->data.l[1] == (long)netatom[NetWMFullscreen]
	||  ev->data.l[2] == (long)netatom[NetWMFullscreen]))
		setfullscreen(c, ev->data.l[0] == 1
			|| (ev->data.l[0] == 2 && !c->isfullscreen));
	else if (ev->message_type == netatom[NetActiveWindow] && c != s && !c->isurgent)
		seturgent(c, 1);
}

void configure(C *c) {
	XConfigureEvent ev = {
		.type = ConfigureNotify, .display = d, .event = c->win, .window = c->win,
		.x = c->x, .y = c->y, .width = c->w, .height = c->h,
		.border_width = c->bw, .above = None, .override_redirect = False,
	};
	XSendEvent(d, c->win, False, StructureNotifyMask, (XEvent*)&ev);
}

void configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	C *c = wintoclient(ev->window);
	XWindowChanges wc;
	if (c) {
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
			XSetWindowBorderWidth(d, c->win, c->bw);
		} else if (c->isfloating || !lt[lt2]->ar) {
			if (ev->value_mask & CWX)      { c->oldx = c->x; c->x = ev->x; }
			if (ev->value_mask & CWY)      { c->oldy = c->y; c->y = ev->y; }
			if (ev->value_mask & CWWidth)  { c->oldw = c->w; c->w = ev->width; }
			if (ev->value_mask & CWHeight) { c->oldh = c->h; c->h = ev->height; }
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (VIS(c))
				XMoveResizeWindow(d, c->win, c->x, c->y, c->w, c->h);
		} else {
			configure(c);
		}
	} else {
		wc.x = ev->x; wc.y = ev->y; wc.width = ev->width; wc.height = ev->height;
		wc.border_width = ev->border_width; wc.sibling = ev->above; wc.stack_mode = ev->detail;
		XConfigureWindow(d, ev->window, ev->value_mask, &wc);
	}
	XSync(d, False);
}

void destroynotify(XEvent *e) {
	C *c;
	if ((c = wintoclient(e->xdestroywindow.window))) unmanage(c, 1);
}

void detach(C *c) {
	C **tc;
	for (tc = &cs; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void enternotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;
	C *c;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != r)
		return;
	if ((c = wintoclient(ev->window)) && c != s) fc(c);
}

void fc(C *c) {
	if (!c || !VIS(c))
		for (c = st; c && !VIS(c); c = c->snext);
	if (s && s != c) unfocus(s, 0);
	if (c) {
		if (c->isurgent) seturgent(c, 0);
		{ C **tc, *t; for (tc = &st; *tc && *tc != c; tc = &(*tc)->snext); *tc = c->snext; if (c == s) { for (t = st; t && !VIS(t); t = t->snext); s = t; } } c->snext = st; st = c;
		grabbuttons(c, 1);
		XSetWindowBorder(d, c->win, sborder);
		setfocus(c);
	} else {
		XSetInputFocus(d, r, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(d, r, netatom[NetActiveWindow]);
	}
	s = c;
}

void focusin(XEvent *e) { if (s && e->xfocus.window != s->win) setfocus(s); }

void focusstack(const A *arg) {
	C *c = NULL, *i = NULL;
	if (!s || s->isfullscreen) return;
	if (arg->i > 0) {
		for (c = s->next; c && !VIS(c); c = c->next);
		if (!c) for (c = cs; c && !VIS(c); c = c->next);
	} else {
		for (i = cs; i != s; i = i->next) if (VIS(i)) c = i;
		if (!c) for (; i; i = i->next)    if (VIS(i)) c = i;
	}
	if (c) { fc(c); restack(); }
}

Atom getatom(C *c, Atom prop) {
	int di; unsigned long dl; unsigned char *p = NULL; Atom da, a = None;
	if (XGetWindowProperty(d, c->win, prop, 0L, sizeof(a), False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) { a = *(Atom*)p; XFree(p); }
	return a;
}

int getrootptr(int *x, int *y) {
	int di; unsigned int dui; Window dw;
	return XQueryPointer(d, r, &dw, &dw, x, y, &di, &di, &dui);
}

long getstate(Window w) {
	int fmt; long res = -1; unsigned char *p = NULL; unsigned long n, ex; Atom real;
	if (XGetWindowProperty(d, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &fmt, &n, &ex, &p) == Success && n) res = *p;
	if (p) XFree(p);
	return res;
}

void grabbuttons(C *c, int focused) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j;
	updatenumlockmask();
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

void grabkeys(void) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j; KeyCode code;
	updatenumlockmask();
	XUngrabKey(d, AnyKey, AnyModifier, r);
	for (i = 0; i < LEN(keys); i++)
		if ((code = XKeysymToKeycode(d, keys[i].key)))
			for (j = 0; j < 4; j++)
				XGrabKey(d, code, keys[i].mod | mods[j],
					r, True, GrabModeAsync, GrabModeAsync);
}

void incnmaster(const A *arg) { nm = MAX(nm + arg->i, 0); ar(); }

void kp(XEvent *e) {
	unsigned int i;
	XKeyEvent *ev = &e->xkey;
	KeySym sym = XLookupKeysym(ev, 0);
	for (i = 0; i < LEN(keys); i++)
		if (sym == keys[i].key
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].fn)
			keys[i].fn(&keys[i].arg);
}

void killclient(const A *arg) {
	(void)arg;
	if (!s) return;
	if (!sendevent(s, wmatom[WMDelete])) {
		XGrabServer(d);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(d, DestroyAll);
		XKillClient(d, s->win);
		XSync(d, False);
		XSetErrorHandler(xerror);
		XUngrabServer(d);
	}
}

void mg(Window w, XWindowAttributes *wa) {
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
	updatesizehints(c);
	updatewmhints(c);
	c->tags = ts[sg];
	if (XGetTransientForHint(d, w, &trans) && (t = wintoclient(trans)))
		c->tags = t->tags;
	if (c->x + W(c)  > wx+ww) c->x = wx+ww - W(c);
	if (c->y + H(c) > wy+wh) c->y = wy+wh - H(c);
	c->x = MAX(c->x, wx);
	c->y = MAX(c->y, wy);
	c->bw = borderpx;
	wc.border_width = c->bw;
	XConfigureWindow(d, w, CWBorderWidth, &wc);
	XSetWindowBorder(d, w, nborder);
	configure(c);
	updatewindowtype(c);
	XSelectInput(d, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating) XRaiseWindow(d, c->win);
	at(c);
	XChangeProperty(d, r, netatom[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char*)&w, 1);
	XMoveResizeWindow(d, c->win, c->x + 2*sw, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	if (focusonopen) {
		unfocus(s, 0);
		s = c;
	}
	ar();
	XMapWindow(d, c->win);
	if (focusonopen) fc(s);
	else             fc(NULL);
}

void mappingnotify(XEvent *e) {
	XRefreshKeyboardMapping(&e->xmapping);
	if (e->xmapping.request == MappingKeyboard) grabkeys();
}

void maprequest(XEvent *e) {
	static XWindowAttributes wa;
	if (!XGetWindowAttributes(d, e->xmaprequest.window, &wa) || wa.override_redirect) return;
	if (!wintoclient(e->xmaprequest.window)) mg(e->xmaprequest.window, &wa);
}

void monocle(void) {
	C *c;
	int g = gappx;
	for (c = nexttiled(cs); c; c = nexttiled(c->next))
		rs(c, wx+g, wy+g, ww - 2*c->bw - 2*g, wh - 2*c->bw - 2*g, 0);
}

void mv(const A *arg) {
	(void)arg;
	int x, y, ocx, ocy, nx, ny;
	XEvent ev;
	Time last = 0;
	if (!s || s->isfullscreen) return;
	restack();
	ocx = s->x; ocy = s->y;
	if (XGrabPointer(d, r, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[1], CurrentTime) != GrabSuccess) return;
	if (!getrootptr(&x, &y)) return;
	do {
		XMaskEvent(d, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= 1000/60) continue;
			last = ev.xmotion.time;
			nx = ocx + ev.xmotion.x - x;
			ny = ocy + ev.xmotion.y - y;
			if (abs(wx - nx) < (int)snap)                     nx = wx;
			else if (abs(wx+ww - nx - W(s)) < (int)snap)     nx = wx+ww - W(s);
			if (abs(wy - ny) < (int)snap)                     ny = wy;
			else if (abs(wy+wh - ny - H(s)) < (int)snap)     ny = wy+wh - H(s);
			if (!s->isfloating && lt[lt2]->ar
			&& (abs(nx-s->x) > (int)snap || abs(ny-s->y) > (int)snap))
				togglefloating(NULL);
			if (!lt[lt2]->ar || s->isfloating)
				rs(s, nx, ny, s->w, s->h, 1);
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(d, CurrentTime);
}

C *nexttiled(C *c) {
	for (; c && (c->isfloating || !VIS(c)); c = c->next);
	return c;
}

void pop(C *c) { detach(c); at(c); fc(c); ar(); }

void propertynotify(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;
	C *c;
	if (ev->state == PropertyDelete || !(c = wintoclient(ev->window))) return;
	if      (ev->atom == XA_WM_HINTS)               updatewmhints(c);
	else if (ev->atom == XA_WM_NORMAL_HINTS)         c->hintsvalid = 0;
	else if (ev->atom == netatom[NetWMWindowType])   updatewindowtype(c);
}

void quit(const A *arg) { (void)arg; running = 0; }

void rs(C *c, int x, int y, int w, int h, int interact) {
	if (applysizehints(c, &x, &y, &w, &h, interact)) resizeclient(c, x, y, w, h);
}

void resizeclient(C *c, int x, int y, int w, int h) {
	XWindowChanges wc;
	c->oldx = c->x; c->x = wc.x      = x;
	c->oldy = c->y; c->y = wc.y      = y;
	c->oldw = c->w; c->w = wc.width   = w;
	c->oldh = c->h; c->h = wc.height  = h;
	wc.border_width = c->bw;
	XConfigureWindow(d, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(d, False);
}

void rz(const A *arg) {
	(void)arg;
	int ocx, ocy, nw, nh;
	XEvent ev;
	Time last = 0;
	if (!s || s->isfullscreen) return;
	restack();
	ocx = s->x; ocy = s->y;
	if (XGrabPointer(d, r, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[2], CurrentTime) != GrabSuccess) return;
	XWarpPointer(d, None, s->win, 0, 0, 0, 0, s->w + s->bw - 1, s->h + s->bw - 1);
	do {
		XMaskEvent(d, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= 1000/60) continue;
			last = ev.xmotion.time;
			nw = MAX(ev.xmotion.x - ocx - 2*s->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2*s->bw + 1, 1);
			if (!s->isfloating && lt[lt2]->ar
			&& (abs(nw-s->w) > (int)snap || abs(nh-s->h) > (int)snap))
				togglefloating(NULL);
			if (!lt[lt2]->ar || s->isfloating)
				rs(s, s->x, s->y, nw, nh, 1);
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(d, None, s->win, 0, 0, 0, 0, s->w + s->bw - 1, s->h + s->bw - 1);
	XUngrabPointer(d, CurrentTime);
	while (XCheckMaskEvent(d, EnterWindowMask, &ev));
}

void restack(void) {
	C *c;
	XEvent ev;
	XWindowChanges wc;
	if (!s) return;
	if (s->isfloating || !lt[lt2]->ar) XRaiseWindow(d, s->win);
	if (lt[lt2]->ar) {
		wc.stack_mode = Below; wc.sibling = r;
		for (c = st; c; c = c->snext)
			if (!c->isfloating && VIS(c)) {
				XConfigureWindow(d, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(d, False);
	while (XCheckMaskEvent(d, EnterWindowMask, &ev));
}

void run(void) {
	XEvent ev;
	XSync(d, False);
	while (running && !XNextEvent(d, &ev))
		if (handler[ev.type]) handler[ev.type](&ev);
}

void scan(void) {
	unsigned int i, n;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;
	if (!XQueryTree(d, r, &d1, &d2, &wins, &n)) return;
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(d, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(d, wins[i], &d1)) continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
			mg(wins[i], &wa);
	}
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(d, wins[i], &wa)) continue;
		if (XGetTransientForHint(d, wins[i], &d1)
		&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
			mg(wins[i], &wa);
	}
	XFree(wins);
}

int sendevent(C *c, Atom proto) {
	int n, exists = 0; Atom *prots; XEvent ev;
	if (XGetWMProtocols(d, c->win, &prots, &n)) {
		while (!exists && n--) exists = prots[n] == proto;
		XFree(prots);
	}
	if (exists) {
		ev.type                  = ClientMessage;
		ev.xclient.window        = c->win;
		ev.xclient.message_type  = wmatom[WMProtocols];
		ev.xclient.format        = 32;
		ev.xclient.data.l[0]     = proto;
		ev.xclient.data.l[1]     = CurrentTime;
		XSendEvent(d, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void setclientstate(C *c, long state) {
	long data[] = { state, None };
	XChangeProperty(d, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char*)data, 2);
}

void setfocus(C *c) {
	if (!c->neverfocus) {
		XSetInputFocus(d, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(d, r, netatom[NetActiveWindow], XA_WINDOW, 32,
			PropModeReplace, (unsigned char*)&c->win, 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(C *c, int fs) {
	if (fs && !c->isfullscreen) {
		XChangeProperty(d, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating; c->oldbw = c->bw;
		c->bw = 0; c->isfloating = 1;
		resizeclient(c, 0, 0, sw, sh);
		XRaiseWindow(d, c->win);
	} else if (!fs && c->isfullscreen) {
		XChangeProperty(d, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0; c->isfloating = c->oldstate; c->bw = c->oldbw;
		c->x = c->oldx; c->y = c->oldy; c->w = c->oldw; c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		ar();
	}
}

void setlayout(const A *arg) {
	if (!arg || !arg->v || arg->v != lt[lt2]) lt2 ^= 1;
	if (arg && arg->v) lt[lt2] = (L*)arg->v;
	if (s) ar();
}

void setmfact(const A *arg) {
	float f;
	if (!arg || !lt[lt2]->ar) return;
	f = arg->f < 1.0f ? arg->f + mf : arg->f - 1.0f;
	if (f < 0.05f || f > 0.95f) return;
	mf = f; ar();
}

void setup(void) {
	XSetWindowAttributes wa;
	XColor xc; Colormap cmap; Atom utf8;

	struct sigaction sa = { .sa_handler = SIG_IGN, .sa_flags = SA_RESTART };
	sigaction(SIGCHLD, &sa, NULL);
	screen = DefaultScreen(d);
	sw = DisplayWidth(d, screen);
	sh = DisplayHeight(d, screen);
	r = RootWindow(d, screen);
	wx = wy = 0; ww = sw; wh = sh;

	cmap = DefaultColormap(d, screen);
	if (!XAllocNamedColor(d, cmap, col_nborder, &xc, &xc)) die("nwm: cannot allocate color");
	nborder = xc.pixel;
	if (!XAllocNamedColor(d, cmap, col_sborder, &xc, &xc)) die("nwm: cannot allocate color");
	sborder = xc.pixel;
	if (!XAllocNamedColor(d, cmap, col_uborder, &xc, &xc)) die("nwm: cannot allocate color");
	uborder = xc.pixel;

	if (!(cursor[0] = XCreateFontCursor(d, 68)))  die("nwm: XCreateFontCursor");
	if (!(cursor[1] = XCreateFontCursor(d, 52)))  die("nwm: XCreateFontCursor");
	if (!(cursor[2] = XCreateFontCursor(d, 120))) die("nwm: XCreateFontCursor");

	utf8 = XInternAtom(d, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(d, "WM_PROTOCOLS",      False);
	wmatom[WMDelete]    = XInternAtom(d, "WM_DELETE_WINDOW",   False);
	wmatom[WMState]     = XInternAtom(d, "WM_STATE",           False);
	wmatom[WMTakeFocus] = XInternAtom(d, "WM_TAKE_FOCUS",      False);
	netatom[NetWMState]            = XInternAtom(d, "_NET_WM_STATE",              False);
	netatom[NetWMFullscreen]       = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN",   False);
	netatom[NetActiveWindow]       = XInternAtom(d, "_NET_ACTIVE_WINDOW",         False);
	netatom[NetWMWindowType]       = XInternAtom(d, "_NET_WM_WINDOW_TYPE",        False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(d, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList]         = XInternAtom(d, "_NET_CLIENT_LIST",           False);

	wmcheck = XCreateSimpleWindow(d, r, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(d, wmcheck,
		XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False),
		XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmcheck, 1);
	XChangeProperty(d, wmcheck,
		XInternAtom(d, "_NET_WM_NAME", False),
		utf8, 8, PropModeReplace, (unsigned char*)"nwm", 3);
	XChangeProperty(d, r,
		XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False),
		XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmcheck, 1);
	XDeleteProperty(d, r, netatom[NetClientList]);

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

void seturgent(C *c, int urg) {
	XWMHints *wh;
	c->isurgent = urg;
	XSetWindowBorder(d, c->win, urg ? uborder : (c == s ? sborder : nborder));
	if (!(wh = XGetWMHints(d, c->win))) return;
	wh->flags = urg ? wh->flags | XUrgencyHint : wh->flags & ~XUrgencyHint;
	XSetWMHints(d, c->win, wh); XFree(wh);
}

void showhide(C *c) {
	if (!c) return;
	if (VIS(c)) {
		XMoveWindow(d, c->win, c->x, c->y);
		if ((!lt[lt2]->ar || c->isfloating) && !c->isfullscreen)
			rs(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		showhide(c->snext);
		XMoveWindow(d, c->win, W(c) * -2, c->y);
	}
}

void spawn(const A *arg) {
	if (fork() == 0) {
		if (d) close(ConnectionNumber(d));
		setsid();
		execvp(((char**)arg->v)[0], (char**)arg->v);
		die("nwm: execvp %s", ((char**)arg->v)[0]);
	}
}

void tag(const A *arg) {
	if (s && arg->ui & TM) { s->tags = arg->ui & TM; fc(NULL); ar(); }
}

void tile(void) {
	C *c;
	unsigned int i, n, nmv, ns;
	int g = gappx, mw;
	for (n = 0, c = nexttiled(cs); c; c = nexttiled(c->next), n++);
	if (!n) return;
	nmv = n > (unsigned)nm ? (unsigned)nm : n;
	ns = n > nmv ? n - nmv : 0;
	mw = nmv && ns ? (int)(ww * mf) : ww;
	for (i = 0, c = nexttiled(cs); c; c = nexttiled(c->next), i++) {
		if (i < nmv) {
			int bh  = (wh - (int)(nmv + 1) * g) / (int)nmv;
			int y0  = wy + g + (int)i * (bh + g);
			int h   = bh;
			rs(c, wx + g, y0, mw - 2*c->bw - 2*g, h - 2*c->bw, 0);
		} else if (ns > 0) {
			unsigned int si = i - nmv;
			int bh  = (wh - (int)(ns + 1) * g) / (int)ns;
			int y0  = wy + g + (int)si * (bh + g);
			int h   = bh;
			rs(c, wx + mw + g, y0, ww - mw - 2*c->bw - 2*g, h - 2*c->bw, 0);
		}
	}
}

void togglefloating(const A *arg) {
	(void)arg;
	if (!s || s->isfullscreen) return;
	s->isfloating = !s->isfloating || s->isfixed;
	if (s->isfloating) rs(s, s->x, s->y, s->w, s->h, 0);
	ar();
}

void togglefullscreen(const A *arg) {
	(void)arg;
	if (s) setfullscreen(s, !s->isfullscreen);
}

void toggletag(const A *arg) {
	unsigned int t;
	if (!s || !(t = s->tags ^ (arg->ui & TM))) return;
	s->tags = t; fc(NULL); ar();
}

void toggleview(const A *arg) {
	unsigned int t = ts[sg] ^ (arg->ui & TM);
	if (t) { ts[sg] = t; fc(NULL); ar(); }
}

void unfocus(C *c, int sf) {
	if (!c) return;
	grabbuttons(c, 0);
	XSetWindowBorder(d, c->win, c->isurgent ? uborder : nborder);
	if (sf) {
		XSetInputFocus(d, r, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(d, r, netatom[NetActiveWindow]);
	}
}

void unmanage(C *c, int destroyed) {
	XWindowChanges wc;
	detach(c); { C **tc, *t; for (tc = &st; *tc && *tc != c; tc = &(*tc)->snext); *tc = c->snext; if (c == s) { for (t = st; t && !VIS(t); t = t->snext); s = t; } }
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(d);
		XSetErrorHandler(xerrordummy);
		XSelectInput(d, c->win, NoEventMask);
		XConfigureWindow(d, c->win, CWBorderWidth, &wc);
		XUngrabButton(d, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(d, False);
		XSetErrorHandler(xerror);
		XUngrabServer(d);
	}
	free(c);
	fc(NULL); updateclientlist(); ar();
}

void unmapnotify(XEvent *e) {
	XUnmapEvent *ev = &e->xunmap;
	C *c;
	if ((c = wintoclient(ev->window))) {
		if (ev->send_event) setclientstate(c, WithdrawnState);
		else unmanage(c, 0);
	}
}

void updateclientlist(void) {
	C *c;
	XDeleteProperty(d, r, netatom[NetClientList]);
	for (c = cs; c; c = c->next)
		XChangeProperty(d, r, netatom[NetClientList], XA_WINDOW, 32,
			PropModeAppend, (unsigned char*)&c->win, 1);
}

void updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap *mm = XGetModifierMapping(d);
	numlockmask = 0;
	for (i = 0; i < 8; i++)
		for (j = 0; j < (unsigned)mm->max_keypermod; j++)
			if (mm->modifiermap[i*mm->max_keypermod+j] == XKeysymToKeycode(d, XK_Num_Lock))
				numlockmask = 1 << i;
	XFreeModifiermap(mm);
}

void updatesizehints(C *c) {
	long ms; XSizeHints sz;
	if (!XGetWMNormalHints(d, c->win, &sz, &ms)) sz.flags = PSize;
	c->basew = (sz.flags & PBaseSize) ? sz.base_width  : (sz.flags & PMinSize) ? sz.min_width  : 0;
	c->baseh = (sz.flags & PBaseSize) ? sz.base_height : (sz.flags & PMinSize) ? sz.min_height : 0;
	c->incw  = (sz.flags & PResizeInc) ? sz.width_inc  : 0;
	c->inch  = (sz.flags & PResizeInc) ? sz.height_inc : 0;
	c->maxw  = (sz.flags & PMaxSize)   ? sz.max_width  : 0;
	c->maxh  = (sz.flags & PMaxSize)   ? sz.max_height : 0;
	c->minw  = (sz.flags & PMinSize)   ? sz.min_width  : c->basew;
	c->minh  = (sz.flags & PMinSize)   ? sz.min_height : c->baseh;
	if (sz.flags & PAspect) {
		c->mina = (float)sz.min_aspect.y / sz.min_aspect.x;
		c->maxa = (float)sz.max_aspect.x / sz.max_aspect.y;
	} else {
		c->maxa = c->mina = 0.0f;
	}
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void updatewindowtype(C *c) {
	if (getatom(c, netatom[NetWMState])      == netatom[NetWMFullscreen])       setfullscreen(c, 1);
	if (getatom(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) c->isfloating = 1;
}

void updatewmhints(C *c) {
	XWMHints *wh;
	if (!(wh = XGetWMHints(d, c->win))) return;
	if (c == s && wh->flags & XUrgencyHint) {
		wh->flags &= ~XUrgencyHint; XSetWMHints(d, c->win, wh);
	} else {
		c->isurgent = (wh->flags & XUrgencyHint) ? 1 : 0;
	}
	c->neverfocus = (wh->flags & InputHint) ? !wh->input : 0;
	XFree(wh);
}

void view(const A *arg) {
	if ((arg->ui & TM) == ts[sg]) return;
	sg ^= 1;
	if (arg->ui & TM) ts[sg] = arg->ui & TM;
	fc(NULL); ar();
}

C *wintoclient(Window w) {
	C *c;
	for (c = cs; c; c = c->next) if (c->win == w) return c;
	return NULL;
}

int xerror(Display *dpy, XErrorEvent *ee) {
	if (ee->error_code == BadWindow
	|| (ee->request_code == 42  && ee->error_code == BadMatch)
	|| (ee->request_code == 74  && ee->error_code == BadDrawable)
	|| (ee->request_code == 70  && ee->error_code == BadDrawable)
	|| (ee->request_code == 66  && ee->error_code == BadDrawable)
	|| (ee->request_code == 12  && ee->error_code == BadMatch)
	|| (ee->request_code == 28  && ee->error_code == BadAccess)
	|| (ee->request_code == 33  && ee->error_code == BadAccess)
	|| (ee->request_code == 62  && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "nwm: error req=%d code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee);
}

int xerrordummy(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; return 0; }
int xerrorstart(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; die("nwm: another wm is running"); return -1; }

void zoom(const A *arg) {
	(void)arg;
	C *c = s;
	if (!lt[lt2]->ar || !c || c->isfloating) return;
	if (c == nexttiled(cs) && !(c = nexttiled(c->next))) return;
	pop(c);
}

int main(int argc, char *argv[]) {
	if (argc == 2 && !strcmp("-v", argv[1])) die("nwm-0.3");
	else if (argc != 1) die("usage: nwm [-v]");
	if (!(d = XOpenDisplay(NULL))) die("nwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1) die("nwm: pledge");
#endif
	scan(); run(); cleanup();
	XCloseDisplay(d);
	return 0;
}
