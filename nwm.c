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
#define ISVISIBLE(c)  ((c)->tags & tagset[seltags])
#define WIDTH(c)      ((c)->w + 2*(c)->bw)
#define HEIGHT(c)     ((c)->h + 2*(c)->bw)
#define TAGMASK       ((1u << LENGTH(tags)) - 1)
#define LENGTH(x)     (sizeof(x)/sizeof(*(x)))
#define MAX(a,b)      ((a)>(b)?(a):(b))
#define MIN(a,b)      ((a)<(b)?(a):(b))

typedef union  { int i; unsigned int ui; float f; const void *v; } Arg;
typedef struct { unsigned int click, mask, button; void (*fn)(const Arg*); Arg arg; } Button;
typedef struct { unsigned int mod; KeySym key; void (*fn)(const Arg*); Arg arg; } Key;
typedef struct { void (*arrange)(void); } Layout;
typedef struct Client Client;

struct Client {
	float mina, maxa;
	int x, y, w, h, oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next, *snext;
	Window win;
};

enum { ClkClientWin, ClkRootWin };
enum { NetWMState, NetWMFullscreen, NetActiveWindow,
       NetWMWindowType, NetWMWindowTypeDialog, NetClientList, NetLast };
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };

static void arrange(void);
static void attach(Client*);
static void attachstack(Client*);
static void buttonpress(XEvent*);
static void checkotherwm(void);
static void cleanup(void);
static void clientmessage(XEvent*);
static void configure(Client*);
static void configurerequest(XEvent*);
static void destroynotify(XEvent*);
static void detach(Client*);
static void detachstack(Client*);
static void enternotify(XEvent*);
static void focus(Client*);
static void focusin(XEvent*);
static void focusstack(const Arg*);
static Atom getatom(Client*, Atom);
static int  getrootptr(int*, int*);
static long getstate(Window);
static void grabbuttons(Client*, int);
static void grabkeys(void);
static void incnmaster(const Arg*);
static void keypress(XEvent*);
static void killclient(const Arg*);
static void manage(Window, XWindowAttributes*);
static void mappingnotify(XEvent*);
static void maprequest(XEvent*);
static void monocle(void);
static void movemouse(const Arg*);
static Client *nexttiled(Client*);
static void pop(Client*);
static void propertynotify(XEvent*);
static void quit(const Arg*);
static void resize(Client*, int, int, int, int, int);
static void resizeclient(Client*, int, int, int, int);
static void resizemouse(const Arg*);
static void restack(void);
static void run(void);
static void scan(void);
static int  sendevent(Client*, Atom);
static void setclientstate(Client*, long);
static void setfocus(Client*);
static void setfullscreen(Client*, int);
static void setlayout(const Arg*);
static void setmfact(const Arg*);
static void setup(void);
static void seturgent(Client*, int);
static void showhide(Client*);
static void spawn(const Arg*);
static void tag(const Arg*);
static void tile(void);
static void togglefloating(const Arg*);
static void togglefullscreen(const Arg*);
static void toggletag(const Arg*);
static void toggleview(const Arg*);
static void unfocus(Client*, int);
static void unmanage(Client*, int);
static void unmapnotify(XEvent*);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(Client*);
static void updatewindowtype(Client*);
static void updatewmhints(Client*);
static void view(const Arg*);
static Client *wintoclient(Window);
static int  xerror(Display*, XErrorEvent*);
static int  xerrordummy(Display*, XErrorEvent*);
static int  xerrorstart(Display*, XErrorEvent*);
static void zoom(const Arg*);

static Display      *dpy;
static Window        root, wmcheck;
static int           screen, sw, sh, wx, wy, ww, wh;
static int           running = 1;
static unsigned int  numlockmask, seltags, sellt, tagset[2];
static float         mfact_cur;
static int           nmaster_cur;
static Cursor        cursor[3];
static Atom          wmatom[WMLast], netatom[NetLast];
static Client       *clients, *sel, *stack;
static const Layout *lt[2];
static unsigned long nborder, sborder, uborder;
static int         (*xerrorxlib)(Display*, XErrorEvent*);

static void (*handler[LASTEvent])(XEvent*) = {
	[ButtonPress]      = buttonpress,
	[ClientMessage]    = clientmessage,
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
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)                  *x = sw - WIDTH(c);
		if (*y > sh)                  *y = sh - HEIGHT(c);
		if (*x + *w + 2*c->bw < 0)   *x = 0;
		if (*y + *h + 2*c->bw < 0)   *y = 0;
	} else {
		if (*x >= wx+ww)              *x = wx+ww - WIDTH(c);
		if (*y >= wy+wh)              *y = wy+wh - HEIGHT(c);
		if (*x + *w + 2*c->bw <= wx)  *x = wx;
		if (*y + *h + 2*c->bw <= wy)  *y = wy;
	}
	if (resizehints || c->isfloating || !lt[sellt]->arrange) {
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

void arrange(void) { showhide(stack); if (lt[sellt]->arrange) lt[sellt]->arrange(); restack(); }

void attach(Client *c) {
	if (attachbottom) {
		Client **tc;
		for (tc = &clients; *tc; tc = &(*tc)->next);
		c->next = NULL; *tc = c;
	} else {
		c->next = clients; clients = c;
	}
}

void attachstack(Client *c) { c->snext = stack; stack = c; }

void buttonpress(XEvent *e) {
	unsigned int i, click = ClkRootWin;
	XButtonPressedEvent *ev = &e->xbutton;
	Client *c;
	if ((c = wintoclient(ev->window))) {
		focus(c); restack();
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].fn
		&& buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].fn(&buttons[i].arg);
}

void checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void) {
	Arg a = {.ui = ~0};
	view(&a);
	while (stack) unmanage(stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XFreeCursor(dpy, cursor[0]);
	XFreeCursor(dpy, cursor[1]);
	XFreeCursor(dpy, cursor[2]);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	XDestroyWindow(dpy, wmcheck);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void clientmessage(XEvent *e) {
	XClientMessageEvent *ev = &e->xclient;
	Client *c = wintoclient(ev->window);
	if (!c) return;
	if (ev->message_type == netatom[NetWMState]
	&& (ev->data.l[1] == (long)netatom[NetWMFullscreen]
	||  ev->data.l[2] == (long)netatom[NetWMFullscreen]))
		setfullscreen(c, ev->data.l[0] == 1
			|| (ev->data.l[0] == 2 && !c->isfullscreen));
	else if (ev->message_type == netatom[NetActiveWindow] && c != sel && !c->isurgent)
		seturgent(c, 1);
}

void configure(Client *c) {
	XConfigureEvent ev = {
		.type = ConfigureNotify, .display = dpy, .event = c->win, .window = c->win,
		.x = c->x, .y = c->y, .width = c->w, .height = c->h,
		.border_width = c->bw, .above = None, .override_redirect = False,
	};
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent*)&ev);
}

void configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	Client *c = wintoclient(ev->window);
	XWindowChanges wc;
	if (c) {
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
			XSetWindowBorderWidth(dpy, c->win, c->bw);
		} else if (c->isfloating || !lt[sellt]->arrange) {
			if (ev->value_mask & CWX)      { c->oldx = c->x; c->x = ev->x; }
			if (ev->value_mask & CWY)      { c->oldy = c->y; c->y = ev->y; }
			if (ev->value_mask & CWWidth)  { c->oldw = c->w; c->w = ev->width; }
			if (ev->value_mask & CWHeight) { c->oldh = c->h; c->h = ev->height; }
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else {
			configure(c);
		}
	} else {
		wc.x = ev->x; wc.y = ev->y; wc.width = ev->width; wc.height = ev->height;
		wc.border_width = ev->border_width; wc.sibling = ev->above; wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

void destroynotify(XEvent *e) {
	Client *c;
	if ((c = wintoclient(e->xdestroywindow.window))) unmanage(c, 1);
}

void detach(Client *c) {
	Client **tc;
	for (tc = &clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void detachstack(Client *c) {
	Client **tc, *t;
	for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;
	if (c == sel) {
		for (t = stack; t && !ISVISIBLE(t); t = t->snext);
		sel = t;
	}
}

void enternotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	if ((c = wintoclient(ev->window)) && c != sel) focus(c);
}

void focus(Client *c) {
	if (!c || !ISVISIBLE(c))
		for (c = stack; c && !ISVISIBLE(c); c = c->snext);
	if (sel && sel != c) unfocus(sel, 0);
	if (c) {
		if (c->isurgent) seturgent(c, 0);
		detachstack(c); attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, sborder);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	sel = c;
}

void focusin(XEvent *e) { if (sel && e->xfocus.window != sel->win) setfocus(sel); }

void focusstack(const Arg *arg) {
	Client *c = NULL, *i = NULL;
	if (!sel || sel->isfullscreen) return;
	if (arg->i > 0) {
		for (c = sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c) for (c = clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = clients; i != sel; i = i->next) if (ISVISIBLE(i)) c = i;
		if (!c) for (; i; i = i->next)            if (ISVISIBLE(i)) c = i;
	}
	if (c) { focus(c); restack(); }
}

Atom getatom(Client *c, Atom prop) {
	int di; unsigned long dl; unsigned char *p = NULL; Atom da, a = None;
	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof(a), False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) { a = *(Atom*)p; XFree(p); }
	return a;
}

int getrootptr(int *x, int *y) {
	int di; unsigned int dui; Window dw;
	return XQueryPointer(dpy, root, &dw, &dw, x, y, &di, &di, &dui);
}

long getstate(Window w) {
	int fmt; long res = -1; unsigned char *p = NULL; unsigned long n, ex; Atom real;
	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &fmt, &n, &ex, &p) == Success && n) res = *p;
	if (p) XFree(p);
	return res;
}

void grabbuttons(Client *c, int focused) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j;
	updatenumlockmask();
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (!focused) {
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		return;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < 4; j++)
				XGrabButton(dpy, buttons[i].button, buttons[i].mask | mods[j],
					c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}

void grabkeys(void) {
	unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	unsigned int i, j; KeyCode code;
	updatenumlockmask();
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < LENGTH(keys); i++)
		if ((code = XKeysymToKeycode(dpy, keys[i].key)))
			for (j = 0; j < 4; j++)
				XGrabKey(dpy, code, keys[i].mod | mods[j],
					root, True, GrabModeAsync, GrabModeAsync);
}

void incnmaster(const Arg *arg) { nmaster_cur = MAX(nmaster_cur + arg->i, 0); arrange(); }

void keypress(XEvent *e) {
	unsigned int i;
	XKeyEvent *ev = &e->xkey;
	KeySym sym = XLookupKeysym(ev, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (sym == keys[i].key
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].fn)
			keys[i].fn(&keys[i].arg);
}

void killclient(const Arg *arg) {
	(void)arg;
	if (!sel) return;
	if (!sendevent(sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void manage(Window w, XWindowAttributes *wa) {
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;
	if (!(c = calloc(1, sizeof(Client)))) die("nwm: calloc");
	c->win   = w;
	c->x     = c->oldx = wa->x;
	c->y     = c->oldy = wa->y;
	c->w     = c->oldw = wa->width;
	c->h     = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	updatesizehints(c);
	updatewmhints(c);
	c->tags = tagset[seltags];
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans)))
		c->tags = t->tags;
	if (c->x + WIDTH(c)  > wx+ww) c->x = wx+ww - WIDTH(c);
	if (c->y + HEIGHT(c) > wy+wh) c->y = wy+wh - HEIGHT(c);
	c->x = MAX(c->x, wx);
	c->y = MAX(c->y, wy);
	c->bw = borderpx;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, nborder);
	configure(c);
	updatewindowtype(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating) XRaiseWindow(dpy, c->win);
	attach(c); attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char*)&w, 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2*sw, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	if (focusonopen) {
		unfocus(sel, 0);
		sel = c;
	}
	arrange();
	XMapWindow(dpy, c->win);
	if (focusonopen) focus(sel);
	else             focus(NULL);
}

void mappingnotify(XEvent *e) {
	XRefreshKeyboardMapping(&e->xmapping);
	if (e->xmapping.request == MappingKeyboard) grabkeys();
}

void maprequest(XEvent *e) {
	static XWindowAttributes wa;
	if (!XGetWindowAttributes(dpy, e->xmaprequest.window, &wa) || wa.override_redirect) return;
	if (!wintoclient(e->xmaprequest.window)) manage(e->xmaprequest.window, &wa);
}

void monocle(void) {
	Client *c;
	int g = gappx;
	for (c = nexttiled(clients); c; c = nexttiled(c->next))
		resize(c, wx+g, wy+g, ww - 2*c->bw - 2*g, wh - 2*c->bw - 2*g, 0);
}

void movemouse(const Arg *arg) {
	(void)arg;
	int x, y, ocx, ocy, nx, ny;
	XEvent ev;
	Time last = 0;
	if (!sel || sel->isfullscreen) return;
	restack();
	ocx = sel->x; ocy = sel->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[1], CurrentTime) != GrabSuccess) return;
	if (!getrootptr(&x, &y)) return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= 1000/60) continue;
			last = ev.xmotion.time;
			nx = ocx + ev.xmotion.x - x;
			ny = ocy + ev.xmotion.y - y;
			if (abs(wx - nx) < (int)snap)                       nx = wx;
			else if (abs(wx+ww - nx - WIDTH(sel)) < (int)snap)  nx = wx+ww - WIDTH(sel);
			if (abs(wy - ny) < (int)snap)                       ny = wy;
			else if (abs(wy+wh - ny - HEIGHT(sel)) < (int)snap) ny = wy+wh - HEIGHT(sel);
			if (!sel->isfloating && lt[sellt]->arrange
			&& (abs(nx-sel->x) > (int)snap || abs(ny-sel->y) > (int)snap))
				togglefloating(NULL);
			if (!lt[sellt]->arrange || sel->isfloating)
				resize(sel, nx, ny, sel->w, sel->h, 1);
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
}

Client *nexttiled(Client *c) {
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void pop(Client *c) { detach(c); attach(c); focus(c); arrange(); }

void propertynotify(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;
	Client *c;
	if (ev->state == PropertyDelete || !(c = wintoclient(ev->window))) return;
	if      (ev->atom == XA_WM_HINTS)               updatewmhints(c);
	else if (ev->atom == XA_WM_NORMAL_HINTS)         c->hintsvalid = 0;
	else if (ev->atom == netatom[NetWMWindowType])   updatewindowtype(c);
}

void quit(const Arg *arg) { (void)arg; running = 0; }

void resize(Client *c, int x, int y, int w, int h, int interact) {
	if (applysizehints(c, &x, &y, &w, &h, interact)) resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
	XWindowChanges wc;
	c->oldx = c->x; c->x = wc.x      = x;
	c->oldy = c->y; c->y = wc.y      = y;
	c->oldw = c->w; c->w = wc.width   = w;
	c->oldh = c->h; c->h = wc.height  = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void resizemouse(const Arg *arg) {
	(void)arg;
	int ocx, ocy, nw, nh;
	XEvent ev;
	Time last = 0;
	if (!sel || sel->isfullscreen) return;
	restack();
	ocx = sel->x; ocy = sel->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[2], CurrentTime) != GrabSuccess) return;
	XWarpPointer(dpy, None, sel->win, 0, 0, 0, 0, sel->w + sel->bw - 1, sel->h + sel->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
			handler[ev.type](&ev);
		else if (ev.type == MotionNotify) {
			if (ev.xmotion.time - last <= 1000/60) continue;
			last = ev.xmotion.time;
			nw = MAX(ev.xmotion.x - ocx - 2*sel->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2*sel->bw + 1, 1);
			if (!sel->isfloating && lt[sellt]->arrange
			&& (abs(nw-sel->w) > (int)snap || abs(nh-sel->h) > (int)snap))
				togglefloating(NULL);
			if (!lt[sellt]->arrange || sel->isfloating)
				resize(sel, sel->x, sel->y, nw, nh, 1);
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, sel->win, 0, 0, 0, 0, sel->w + sel->bw - 1, sel->h + sel->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void restack(void) {
	Client *c;
	XEvent ev;
	XWindowChanges wc;
	if (!sel) return;
	if (sel->isfloating || !lt[sellt]->arrange) XRaiseWindow(dpy, sel->win);
	if (lt[sellt]->arrange) {
		wc.stack_mode = Below; wc.sibling = root;
		for (c = stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void run(void) {
	XEvent ev;
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type]) handler[ev.type](&ev);
}

void scan(void) {
	unsigned int i, n;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;
	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &n)) return;
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1)) continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
			manage(wins[i], &wa);
	}
	for (i = 0; i < n; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)) continue;
		if (XGetTransientForHint(dpy, wins[i], &d1)
		&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
			manage(wins[i], &wa);
	}
	XFree(wins);
}

int sendevent(Client *c, Atom proto) {
	int n, exists = 0; Atom *prots; XEvent ev;
	if (XGetWMProtocols(dpy, c->win, &prots, &n)) {
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
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void setclientstate(Client *c, long s) {
	long d[] = { s, None };
	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char*)d, 2);
}

void setfocus(Client *c) {
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
			PropModeReplace, (unsigned char*)&c->win, 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, int fs) {
	if (fs && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating; c->oldbw = c->bw;
		c->bw = 0; c->isfloating = 1;
		resizeclient(c, 0, 0, sw, sh);
		XRaiseWindow(dpy, c->win);
	} else if (!fs && c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0; c->isfloating = c->oldstate; c->bw = c->oldbw;
		c->x = c->oldx; c->y = c->oldy; c->w = c->oldw; c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange();
	}
}

void setlayout(const Arg *arg) {
	if (!arg || !arg->v || arg->v != lt[sellt]) sellt ^= 1;
	if (arg && arg->v) lt[sellt] = (Layout*)arg->v;
	if (sel) arrange();
}

void setmfact(const Arg *arg) {
	float f;
	if (!arg || !lt[sellt]->arrange) return;
	f = arg->f < 1.0f ? arg->f + mfact_cur : arg->f - 1.0f;
	if (f < 0.05f || f > 0.95f) return;
	mfact_cur = f; arrange();
}

void setup(void) {
	XSetWindowAttributes wa;
	XColor xc; Colormap cmap; Atom utf8;

	struct sigaction sa = { .sa_handler = SIG_IGN, .sa_flags = SA_RESTART };
	sigaction(SIGCHLD, &sa, NULL);
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	wx = wy = 0; ww = sw; wh = sh;

	cmap = DefaultColormap(dpy, screen);
	if (!XAllocNamedColor(dpy, cmap, col_nborder, &xc, &xc)) die("nwm: cannot allocate color");
	nborder = xc.pixel;
	if (!XAllocNamedColor(dpy, cmap, col_sborder, &xc, &xc)) die("nwm: cannot allocate color");
	sborder = xc.pixel;
	if (!XAllocNamedColor(dpy, cmap, col_uborder, &xc, &xc)) die("nwm: cannot allocate color");
	uborder = xc.pixel;

	if (!(cursor[0] = XCreateFontCursor(dpy, 68)))  die("nwm: XCreateFontCursor");
	if (!(cursor[1] = XCreateFontCursor(dpy, 52)))  die("nwm: XCreateFontCursor");
	if (!(cursor[2] = XCreateFontCursor(dpy, 120))) die("nwm: XCreateFontCursor");

	utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS",      False);
	wmatom[WMDelete]    = XInternAtom(dpy, "WM_DELETE_WINDOW",   False);
	wmatom[WMState]     = XInternAtom(dpy, "WM_STATE",           False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS",      False);
	netatom[NetWMState]            = XInternAtom(dpy, "_NET_WM_STATE",              False);
	netatom[NetWMFullscreen]       = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN",   False);
	netatom[NetActiveWindow]       = XInternAtom(dpy, "_NET_ACTIVE_WINDOW",         False);
	netatom[NetWMWindowType]       = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE",        False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList]         = XInternAtom(dpy, "_NET_CLIENT_LIST",           False);

	wmcheck = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheck,
		XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False),
		XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmcheck, 1);
	XChangeProperty(dpy, wmcheck,
		XInternAtom(dpy, "_NET_WM_NAME", False),
		utf8, 8, PropModeReplace, (unsigned char*)"nwm", 3);
	XChangeProperty(dpy, root,
		XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False),
		XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmcheck, 1);
	XDeleteProperty(dpy, root, netatom[NetClientList]);

	tagset[0] = tagset[1] = 1;
	mfact_cur   = mfact;
	nmaster_cur = nmaster;
	lt[0] = &layouts[0];
	lt[1] = &layouts[1 % LENGTH(layouts)];

	grabkeys();
	wa.cursor     = cursor[0];
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
	              | ButtonPressMask|PointerMotionMask|EnterWindowMask
	              | StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	focus(NULL);
}

void seturgent(Client *c, int urg) {
	XWMHints *wh;
	c->isurgent = urg;
	XSetWindowBorder(dpy, c->win, urg ? uborder : (c == sel ? sborder : nborder));
	if (!(wh = XGetWMHints(dpy, c->win))) return;
	wh->flags = urg ? wh->flags | XUrgencyHint : wh->flags & ~XUrgencyHint;
	XSetWMHints(dpy, c->win, wh); XFree(wh);
}

void showhide(Client *c) {
	if (!c) return;
	if (ISVISIBLE(c)) {
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!lt[sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void spawn(const Arg *arg) {
	if (fork() == 0) {
		if (dpy) close(ConnectionNumber(dpy));
		setsid();
		execvp(((char**)arg->v)[0], (char**)arg->v);
		die("nwm: execvp %s", ((char**)arg->v)[0]);
	}
}

void tag(const Arg *arg) {
	if (sel && arg->ui & TAGMASK) { sel->tags = arg->ui & TAGMASK; focus(NULL); arrange(); }
}

void tile(void) {
	Client *c;
	unsigned int i, n, nm, ns;
	int g = gappx, mw;
	for (n = 0, c = nexttiled(clients); c; c = nexttiled(c->next), n++);
	if (!n) return;
	nm = n > (unsigned)nmaster_cur ? (unsigned)nmaster_cur : n;
	ns = n > nm ? n - nm : 0;
	mw = nm && ns ? (int)(ww * mfact_cur) : ww;
	for (i = 0, c = nexttiled(clients); c; c = nexttiled(c->next), i++) {
		if (i < nm) {
			int bh  = (wh - (int)(nm + 1) * g) / (int)nm;
			int rem = (wh - (int)(nm + 1) * g) % (int)nm;
			int y0  = wy + g + (int)i * (bh + g) + (int)(i < (unsigned)rem ? i : (unsigned)rem);
			int h   = bh + (i < (unsigned)rem ? 1 : 0);
			resize(c, wx + g, y0, mw - 2*c->bw - 2*g, h - 2*c->bw, 0);
		} else if (ns > 0) {
			unsigned int si = i - nm;
			int bh  = (wh - (int)(ns + 1) * g) / (int)ns;
			int rem = (wh - (int)(ns + 1) * g) % (int)ns;
			int y0  = wy + g + (int)si * (bh + g) + (int)(si < (unsigned)rem ? si : (unsigned)rem);
			int h   = bh + (si < (unsigned)rem ? 1 : 0);
			resize(c, wx + mw + g, y0, ww - mw - 2*c->bw - 2*g, h - 2*c->bw, 0);
		}
	}
}

void togglefloating(const Arg *arg) {
	(void)arg;
	if (!sel || sel->isfullscreen) return;
	sel->isfloating = !sel->isfloating || sel->isfixed;
	if (sel->isfloating) resize(sel, sel->x, sel->y, sel->w, sel->h, 0);
	arrange();
}

void togglefullscreen(const Arg *arg) {
	(void)arg;
	if (sel) setfullscreen(sel, !sel->isfullscreen);
}

void toggletag(const Arg *arg) {
	unsigned int t;
	if (!sel || !(t = sel->tags ^ (arg->ui & TAGMASK))) return;
	sel->tags = t; focus(NULL); arrange();
}

void toggleview(const Arg *arg) {
	unsigned int t = tagset[seltags] ^ (arg->ui & TAGMASK);
	if (t) { tagset[seltags] = t; focus(NULL); arrange(); }
}

void unfocus(Client *c, int sf) {
	if (!c) return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, c->isurgent ? uborder : nborder);
	if (sf) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client *c, int destroyed) {
	XWindowChanges wc;
	detach(c); detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL); updateclientlist(); arrange();
}

void unmapnotify(XEvent *e) {
	XUnmapEvent *ev = &e->xunmap;
	Client *c;
	if ((c = wintoclient(ev->window))) {
		if (ev->send_event) setclientstate(c, WithdrawnState);
		else unmanage(c, 0);
	}
}

void updateclientlist(void) {
	Client *c;
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (c = clients; c; c = c->next)
		XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
			PropModeAppend, (unsigned char*)&c->win, 1);
}

void updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap *mm = XGetModifierMapping(dpy);
	numlockmask = 0;
	for (i = 0; i < 8; i++)
		for (j = 0; j < (unsigned)mm->max_keypermod; j++)
			if (mm->modifiermap[i*mm->max_keypermod+j] == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = 1 << i;
	XFreeModifiermap(mm);
}

void updatesizehints(Client *c) {
	long ms; XSizeHints s;
	if (!XGetWMNormalHints(dpy, c->win, &s, &ms)) s.flags = PSize;
	c->basew = (s.flags & PBaseSize) ? s.base_width  : (s.flags & PMinSize) ? s.min_width  : 0;
	c->baseh = (s.flags & PBaseSize) ? s.base_height : (s.flags & PMinSize) ? s.min_height : 0;
	c->incw  = (s.flags & PResizeInc) ? s.width_inc  : 0;
	c->inch  = (s.flags & PResizeInc) ? s.height_inc : 0;
	c->maxw  = (s.flags & PMaxSize)   ? s.max_width  : 0;
	c->maxh  = (s.flags & PMaxSize)   ? s.max_height : 0;
	c->minw  = (s.flags & PMinSize)   ? s.min_width  : c->basew;
	c->minh  = (s.flags & PMinSize)   ? s.min_height : c->baseh;
	if (s.flags & PAspect) {
		c->mina = (float)s.min_aspect.y / s.min_aspect.x;
		c->maxa = (float)s.max_aspect.x / s.max_aspect.y;
	} else {
		c->maxa = c->mina = 0.0f;
	}
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void updatewindowtype(Client *c) {
	if (getatom(c, netatom[NetWMState])      == netatom[NetWMFullscreen])       setfullscreen(c, 1);
	if (getatom(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) c->isfloating = 1;
}

void updatewmhints(Client *c) {
	XWMHints *wh;
	if (!(wh = XGetWMHints(dpy, c->win))) return;
	if (c == sel && wh->flags & XUrgencyHint) {
		wh->flags &= ~XUrgencyHint; XSetWMHints(dpy, c->win, wh);
	} else {
		c->isurgent = (wh->flags & XUrgencyHint) ? 1 : 0;
	}
	c->neverfocus = (wh->flags & InputHint) ? !wh->input : 0;
	XFree(wh);
}

void view(const Arg *arg) {
	if ((arg->ui & TAGMASK) == tagset[seltags]) return;
	seltags ^= 1;
	if (arg->ui & TAGMASK) tagset[seltags] = arg->ui & TAGMASK;
	focus(NULL); arrange();
}

Client *wintoclient(Window w) {
	Client *c;
	for (c = clients; c; c = c->next) if (c->win == w) return c;
	return NULL;
}

int xerror(Display *d, XErrorEvent *ee) {
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
	return xerrorxlib(d, ee);
}

int xerrordummy(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }
int xerrorstart(Display *d, XErrorEvent *e) { (void)d; (void)e; die("nwm: another wm is running"); return -1; }

void zoom(const Arg *arg) {
	(void)arg;
	Client *c = sel;
	if (!lt[sellt]->arrange || !c || c->isfloating) return;
	if (c == nexttiled(clients) && !(c = nexttiled(c->next))) return;
	pop(c);
}

int main(int argc, char *argv[]) {
	if (argc == 2 && !strcmp("-v", argv[1])) die("nwm-0.3");
	else if (argc != 1) die("usage: nwm [-v]");
	if (!(dpy = XOpenDisplay(NULL))) die("nwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1) die("nwm: pledge");
#endif
	scan(); run(); cleanup();
	XCloseDisplay(dpy);
	return 0;
}
