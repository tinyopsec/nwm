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
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>

#define BUTTONMASK   (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK    (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(m) ((m) & ~(numlockmask|LockMask) & \
        (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define VIS(c)       ((c)->tags & tagsel[sel_group])
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

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
} Rule;

typedef struct {
	const L *lt[2];
	unsigned int li;
	float mf;
	int nm;
} PT;

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
static void checkwm(void);
static void cleanup(void);
static void clientmsg(XEvent*);
static void configurenotify(C*);
static void configurerequest(XEvent*);
static void destroynotify(XEvent*);
static void detach(C*);
static void detachstack(C*);
static void enternotify(XEvent*);
static void focus(C*);
static void focusin(XEvent*);
static void focusnext(const A*);
static Atom getatom(C*, Atom);
static int  getrootptr(int*, int*);
static long getstate(Window);
static void grabbuttons(C*, int);
static void grabkeys(void);
static void incnmaster(const A*);
static void keypress(XEvent*);
static void killclient(const A*);
static void manage(Window, XWindowAttributes*);
static void mappingnotify(XEvent*);
static void maprequest(XEvent*);
static void monocle(void);
static void movemouse(const A*);
static Bool movepred(Display*, XEvent*, XPointer);
static C   *nexttiled(C*);
static void pop(C*);
static void propertynotify(XEvent*);
static void quit(const A*);
static void resize(C*, int, int, int, int, int);
static void resizeclient(C*, int, int, int, int);
static void resizemouse(const A*);
static void restack(void);
static void run(void);
static void scan(void);
static int  sendevent(C*, Atom);
static void setclientstate(C*, long);
static void setfocus(C*);
static void setfullscreen(C*, int);
static void setlayout(const A*);
static void setmasterfact(const A*);
static void setup(void);
static void seturgency(C*, int);
static void showhide(C*);
static void spawn(const A*);
static void tag(const A*);
static void tile(void);
static void togglefloating(const A*);
static void togglefullscreen(const A*);
static void toggletag(const A*);
static void toggleview(const A*);
static void unfocus(C*);
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
static int  applyrules(C*);

static Display      *display;
static Window        root, wmcheck_win;
static int           screen_idx, screen_w, screen_h, win_area_x, win_area_y, win_area_w, win_area_h;
static int           running = 1;
static unsigned int  numlockmask, sel_group, layout_sel, tagsel[2];
static float         master_factor;
static int           master_count;
static Cursor        cursor[3];
static Atom          wmatom[WMLast], netatom[NetLast];
static C            *clients, *sel, *stack;
static const L      *cur_layouts[2];
static unsigned long color_norm, color_sel, color_urg;
static int         (*xerrorxlib)(Display*, XErrorEvent*);
static Time          last_time = CurrentTime;

static void (*handler[LASTEvent])(XEvent*) = {
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

#include "nwm.h"

static PT per_tag[LEN(tags)];

static void
die(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
        if (display) XCloseDisplay(display);
        exit(1);
}

static void
pertagupdate(int tag_idx) {
	if (tag_idx < 0 || tag_idx >= (int)LEN(tags)) return;
	per_tag[tag_idx].lt[0] = cur_layouts[0];
	per_tag[tag_idx].lt[1] = cur_layouts[1];
	per_tag[tag_idx].li = layout_sel;
	per_tag[tag_idx].mf = master_factor;
	per_tag[tag_idx].nm = master_count;
}

static int
firsttag(unsigned int tag_mask) {
	int i;
	for (i = 0; i < (int)LEN(tags); i++)
		if (tag_mask & (1u << i)) return i;
	return 0;
}

static void
pertagrestore(unsigned int tag_mask) {
	int i;
	for (i = 0; i < (int)LEN(tags); i++) {
		if (tag_mask & (1u << i)) {
			cur_layouts[0] = per_tag[i].lt[0];
			cur_layouts[1] = per_tag[i].lt[1];
			layout_sel = per_tag[i].li;
			master_factor = per_tag[i].mf;
			master_count = per_tag[i].nm;
			return;
		}
	}
}

static void
pertagsave(unsigned int tag_mask) {
	int i;
	for (i = 0; i < (int)LEN(tags); i++)
		if (tag_mask & (1u << i))
			pertagupdate(i);
}

static int
applysizehints(C *c, int *x, int *y, int *w, int *h, int interact) {
        int bw2 = c->bw << 1;
        *w = MAX(1, *w);
        *h = MAX(1, *h);
        if (interact) {
                if (*x > screen_w)            *x = screen_w - (*w + bw2);
                if (*x + *w + bw2 < 0) *x = 0;
                if (*y + *h + bw2 < 0) *y = 0;
        } else {
                if (*x >= win_area_x+win_area_w)         *x = win_area_x+win_area_w - (*w + bw2);
                if (*y >= win_area_y+win_area_h)         *y = win_area_y+win_area_h - (*h + bw2);
                if (*x + *w + bw2 <= win_area_x) *x = win_area_x;
                if (*y + *h + bw2 <= win_area_y) *y = win_area_y;
        }
        if (c->isfloating || !cur_layouts[layout_sel]->ar) {
                if (!c->hintsvalid) updatesizehints(c);
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
        showhide(stack);
        if (cur_layouts[layout_sel]->ar) cur_layouts[layout_sel]->ar();
        restack();
}

static void
at(C *c) {
        if (attachbottom) {
                C **pp;
                for (pp = &clients; *pp; pp = &(*pp)->next);
                c->next = NULL; *pp = c;
        } else {
                c->next = clients; clients = c;
        }
        c->snext = stack; stack = c;
}

static void
bp(XEvent *e) {
        unsigned int i, click = ClkRootWin;
        XButtonPressedEvent *ev = &e->xbutton;
        C *c;
        last_time = ev->time;
        if ((c = wintoclient(ev->window))) {
                focus(c); restack();
                XAllowEvents(display, ReplayPointer, ev->time);
                click = ClkClientWin;
        }
        for (i = 0; i < LEN(buttons); i++)
                if (click == buttons[i].click && buttons[i].fn
                && buttons[i].button == ev->button
                && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
                        buttons[i].fn(&buttons[i].arg);
}

static void
checkwm(void) {
        xerrorxlib = XSetErrorHandler(xerrorstart);
        XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
        XSync(display, False);
        XSetErrorHandler(xerror);
}

static void
cleanup(void) {
        unsigned int i;
        C *c;
        while ((c = clients)) {
                detach(c); detachstack(c);
                XMoveWindow(display, c->win, c->x, c->y);
                setclientstate(c, WithdrawnState);
                free(c);
        }
        sel = stack = NULL;
        XUngrabKey(display, AnyKey, AnyModifier, root);
        for (i = 0; i < 3; i++) XFreeCursor(display, cursor[i]);
        XDeleteProperty(display, root, netatom[NetClientList]);
        XDestroyWindow(display, wmcheck_win);
        XSync(display, False);
        XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
}

static void
clientmsg(XEvent *e) {
        XClientMessageEvent *ev = &e->xclient;
        C *c = wintoclient(ev->window);
        if (!c) return;
        if (ev->message_type == netatom[NetWMState]
        && (ev->data.l[1] == (long)netatom[NetWMFullscreen]
        ||  ev->data.l[2] == (long)netatom[NetWMFullscreen]))
                setfullscreen(c, ev->data.l[0] == 1
                        || (ev->data.l[0] == 2 && !c->isfullscreen));
        else if (ev->message_type == netatom[NetActiveWindow] && c != sel && !c->isurgent)
                seturgency(c, 1);
}

static void
configurenotify(C *c) {
        XConfigureEvent ev = {
                .type = ConfigureNotify, .send_event = True, .display = display,
                .event = c->win, .window = c->win,
                .x = c->x, .y = c->y, .width = c->w, .height = c->h,
                .border_width = c->bw, .above = None, .override_redirect = False,
        };
        XSendEvent(display, c->win, False, StructureNotifyMask, (XEvent*)&ev);
}

static void
configurerequest(XEvent *e) {
        XConfigureRequestEvent *ev = &e->xconfigurerequest;
        XWindowChanges wc;
        C *c = wintoclient(ev->window);
        if (!c) {
                wc.x = ev->x; wc.y = ev->y;
                wc.width = ev->width; wc.height = ev->height;
                wc.border_width = ev->border_width;
                wc.sibling = ev->above; wc.stack_mode = ev->detail;
                XConfigureWindow(display, ev->window, ev->value_mask, &wc);
                return;
        }
        if (ev->value_mask & CWBorderWidth) {
                c->bw = ev->border_width;
                XSetWindowBorderWidth(display, c->win, c->bw);
                ar();
        }
        if (c->isfloating || !cur_layouts[layout_sel]->ar) {
                if (ev->value_mask & CWX)      { c->oldx = c->x; c->x = ev->x; }
                if (ev->value_mask & CWY)      { c->oldy = c->y; c->y = ev->y; }
                if (ev->value_mask & CWWidth)  { c->oldw = c->w; c->w = ev->width; }
                if (ev->value_mask & CWHeight) { c->oldh = c->h; c->h = ev->height; }
                if (VIS(c)) {
                        XMoveResizeWindow(display, c->win, c->x, c->y, c->w, c->h);
                        if (c->isfloating) XRaiseWindow(display, c->win);
                } else {
                        configurenotify(c);
                }
        } else {
                configurenotify(c);
        }
}

static void destroynotify(XEvent *e) { C *c; if ((c = wintoclient(e->xdestroywindow.window))) unmanage(c, 1); }

static void
detach(C *c) {
        C **pp;
        for (pp = &clients; *pp && *pp != c; pp = &(*pp)->next);
        if (*pp) *pp = c->next;
}

static void
detachstack(C *c) {
        C **pp, *t;
        for (pp = &stack; *pp && *pp != c; pp = &(*pp)->snext);
        if (*pp) *pp = c->snext;
        if (c == sel) {
                for (t = stack; t && !VIS(t); t = t->snext);
                sel = t;
        }
}

static void
enternotify(XEvent *e) {
        XCrossingEvent *ev = &e->xcrossing;
        C *c;
        if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
                return;
        last_time = ev->time;
        c = wintoclient(ev->window);
        if (!c || c == sel) return;
        if (sel && sel->isfullscreen) return;
        if (c->isfullscreen) return;
        if (sel && sel->isfloating && !c->isfloating && cur_layouts[layout_sel]->ar) return;
        focus(c);
        restack();
}

static void
focus(C *c) {
        C **pp;
        if (!c || !VIS(c))
                for (c = stack; c && !VIS(c); c = c->snext);
        if (sel && sel != c) unfocus(sel);
        if (c) {
                if (c->isurgent) seturgency(c, 0);
                for (pp = &stack; *pp && *pp != c; pp = &(*pp)->snext);
                if (*pp) *pp = c->snext;
                c->snext = stack; stack = c;
                grabbuttons(c, 1);
                XSetWindowBorder(display, c->win, color_sel);
                setfocus(c);
        } else {
                XSetInputFocus(display, root, RevertToPointerRoot, last_time);
                XDeleteProperty(display, root, netatom[NetActiveWindow]);
        }
        sel = c;
}

static void focusin(XEvent *e) { if (sel && e->xfocus.window != sel->win) setfocus(sel); }

static void
focusnext(const A *arg) {
        C *c = NULL, *cur;
        int tiled = !!cur_layouts[layout_sel]->ar;
        if (!sel || sel->isfullscreen) return;
        if (arg->i > 0) {
                for (c = sel->next; c && (!VIS(c) || (tiled && c->isfloating)); c = c->next);
                if (!c) for (c = clients; c && (!VIS(c) || (tiled && c->isfloating)); c = c->next);
        } else {
                for (cur = clients; cur != sel; cur = cur->next) if (VIS(cur) && (!tiled || !cur->isfloating)) c = cur;
                if (!c) for (; cur; cur = cur->next)    if (VIS(cur) && (!tiled || !cur->isfloating)) c = cur;
        }
        if (c) { focus(c); restack(); }
}

static Atom
getatom(C *c, Atom prop) {
        int fmt; unsigned long n, rem; unsigned char *p = NULL; Atom type, a = None;
        if (XGetWindowProperty(display, c->win, prop, 0L, 1L, False, XA_ATOM,
                &type, &fmt, &n, &rem, &p) == Success && p) {
                if (type == XA_ATOM && n > 0) memcpy(&a, p, sizeof(Atom));
                XFree(p);
        }
        return a;
}

static int getrootptr(int *x, int *y) { int di; unsigned int dui; Window dw; return XQueryPointer(display, root, &dw, &dw, x, y, &di, &di, &dui); }

static long
getstate(Window w) {
        int fmt; long res = -1; unsigned char *p = NULL; unsigned long n, ex; Atom real;
        if (XGetWindowProperty(display, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
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
        XUngrabButton(display, AnyButton, AnyModifier, c->win);
        if (!focused) {
                XGrabButton(display, AnyButton, AnyModifier, c->win, False,
                        BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
                return;
        }
        for (i = 0; i < LEN(buttons); i++)
                if (buttons[i].click == ClkClientWin)
                        for (j = 0; j < 4; j++)
                                XGrabButton(display, buttons[i].button, buttons[i].mask | mods[j],
                                        c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}

static void
grabkeys(void) {
        unsigned int mods[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        unsigned int i, j; KeyCode code;
        updatenumlockmask();
        XUngrabKey(display, AnyKey, AnyModifier, root);
        for (i = 0; i < LEN(keys); i++)
                if ((code = XKeysymToKeycode(display, keys[i].key)))
                        for (j = 0; j < 4; j++)
                                XGrabKey(display, code, keys[i].mod | mods[j],
                                        root, True, GrabModeAsync, GrabModeAsync);
}

static void incnmaster(const A *arg) {
	master_count = (master_count + arg->i > 0) ? master_count + arg->i : 0;
	pertagupdate(firsttag(tagsel[sel_group]));
	ar();
}

static void
keypress(XEvent *e) {
        unsigned int i;
        XKeyEvent *ev = &e->xkey;
        KeySym sym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);
        last_time = ev->time;
        for (i = 0; i < LEN(keys); i++)
                if (sym == keys[i].key
                && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
                && keys[i].fn)
                        keys[i].fn(&keys[i].arg);
}

static void
killclient(const A *arg) {
        (void)arg;
        if (!sel) return;
        if (!sendevent(sel, wmatom[WMDelete])) {
                XGrabServer(display);
                XSetErrorHandler(xerrordummy);
                XSetCloseDownMode(display, DestroyAll);
                XKillClient(display, sel->win);
                XSetCloseDownMode(display, RetainPermanent);
                XSync(display, False);
                XSetErrorHandler(xerror);
                XUngrabServer(display);
        }
}

static int
applyrules(C *c) {
	const Rule *rule;
	XClassHint ch = { NULL, NULL };
	char title[256] = "";
	XTextProperty tp;
	unsigned int i;
	int rule_floating = -1;

	XGetClassHint(display, c->win, &ch);
	if (XGetWMName(display, c->win, &tp) && tp.nitems) {
		strncpy(title, (char *)tp.value, sizeof(title) - 1);
		XFree(tp.value);
	}

	for (i = 0; i < LEN(rules); i++) {
		rule = &rules[i];
		if ((!rule->class    || (ch.res_class && !strcmp(ch.res_class, rule->class)))
		 && (!rule->instance || (ch.res_name  && !strcmp(ch.res_name, rule->instance)))
		 && (!rule->title    || (title[0]     && strstr(title, rule->title)))) {
			rule_floating = rule->isfloating;
			if (rule->tags)
				c->tags = rule->tags & TM;
			break;
		}
	}

	if (ch.res_class) XFree(ch.res_class);
	if (ch.res_name)  XFree(ch.res_name);
	return rule_floating;
}

static void
manage(Window w, XWindowAttributes *wa) {
        C *c, *trans_client = NULL;
        Window trans = None;
        XWindowChanges wc;
	int rule_floating;
        if (!(c = calloc(1, sizeof(C)))) die("nwm: calloc");
        c->win   = w;
        c->x     = c->oldx = wa->x;
        c->y     = c->oldy = wa->y;
        c->w     = c->oldw = wa->width;
        c->h     = c->oldh = wa->height;
        c->oldbw = wa->border_width;
        updatesizehints(c);
        updatewmhints(c);
        c->tags = tagsel[sel_group];
	rule_floating = applyrules(c);
        if (XGetTransientForHint(display, w, &trans) && (trans_client = wintoclient(trans)))
                c->tags = trans_client->tags;
        c->bw = borderpx;
        if (c->x + W(c) > win_area_x+win_area_w) c->x = win_area_x+win_area_w - W(c);
        if (c->y + H(c) > win_area_y+win_area_h) c->y = win_area_y+win_area_h - H(c);
        c->x = MAX(c->x, win_area_x);
        c->y = MAX(c->y, win_area_y);
        wc.border_width = c->bw;
        XConfigureWindow(display, w, CWBorderWidth, &wc);
        XSetWindowBorder(display, w, color_norm);
        configurenotify(c);
        c->isfloating = c->oldstate = trans != None || c->isfixed;
        updatewindowtype(c);
        if (rule_floating >= 0)
                c->isfloating = c->oldstate = rule_floating;
        XSelectInput(display, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
        grabbuttons(c, 0);
        at(c);
        XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
                PropModeAppend, (unsigned char*)&w, 1);
        if (c->isfloating) XMapRaised(display, c->win);
        else               XMapWindow(display, c->win);
        setclientstate(c, NormalState);
        if (focusonopen) focus(c);
        ar();
}

static void
mappingnotify(XEvent *e) {
        XRefreshKeyboardMapping(&e->xmapping);
        if (e->xmapping.request == MappingKeyboard) grabkeys();
}

static void
maprequest(XEvent *e) {
        XWindowAttributes wa;
        C *c;
        if (!XGetWindowAttributes(display, e->xmaprequest.window, &wa) || wa.override_redirect) return;
        if ((c = wintoclient(e->xmaprequest.window))) {
                XMapWindow(display, c->win);
                setclientstate(c, NormalState);
                ar();
                return;
        }
        manage(e->xmaprequest.window, &wa);
}

static void
monocle(void) {
        C *c;
        for (c = nexttiled(clients); c; c = nexttiled(c->next))
                resize(c, win_area_x, win_area_y, MAX(1, win_area_w - (c->bw << 1)), MAX(1, win_area_h - (c->bw << 1)), 0);
}

static Bool
movepred(Display *dpy, XEvent *e, XPointer arg) {
        (void)dpy; (void)arg;
        switch (e->type) {
        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
        case Expose:
        case ConfigureRequest:
        case MapRequest:
                return True;
        case KeyPress: {
                KeySym sym = XkbKeycodeToKeysym(display, e->xkey.keycode, 0, 0);
                return CLEANMASK(e->xkey.state) == MODKEY && sym >= XK_1 && sym <= XK_9;
        }
        }
        return False;
}

static void
movemouse(const A *arg) {
        (void)arg;
        int x, y, ocx, ocy, nx, ny, needar = 0;
        XEvent ev;
        Time last = 0;
        C *c = sel;
        if (!c || c->isfullscreen) return;
        restack();
        ocx = c->x; ocy = c->y;
        if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, cursor[1], last_time) != GrabSuccess) return;
        if (!getrootptr(&x, &y)) { XUngrabPointer(display, CurrentTime); return; }
        do {
                XIfEvent(display, &ev, movepred, NULL);
                if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
                        handler[ev.type](&ev);
                else if (ev.type == MotionNotify) {
                        if (ev.xmotion.time - last <= FRAMERATE) continue;
                        last = ev.xmotion.time;
                        nx = ocx + ev.xmotion.x - x;
                        ny = ocy + ev.xmotion.y - y;
                        if (abs(win_area_x - nx)              < (int)snap) nx = win_area_x;
                        else if (abs(win_area_x+win_area_w-W(c) - nx) < (int)snap) nx = win_area_x+win_area_w - W(c);
                        if (abs(win_area_y - ny)              < (int)snap) ny = win_area_y;
                        else if (abs(win_area_y+win_area_h-H(c) - ny) < (int)snap) ny = win_area_y+win_area_h - H(c);
                        if (!c->isfloating && cur_layouts[layout_sel]->ar
                        && (abs(nx-ocx) > (int)snap || abs(ny-ocy) > (int)snap)) {
                                c->isfloating = c->oldstate = 1; needar = 1;
                        }
                        if (!cur_layouts[layout_sel]->ar || c->isfloating)
                                resize(c, nx, ny, c->w, c->h, 1);
                } else if (ev.type == KeyPress) {
                        unsigned int tag_idx = (unsigned int)(XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0) - XK_1);
                        if (tag_idx < LEN(tags)) {
                                unsigned int new_mask = 1u << tag_idx;
                                unsigned int old_mask = tagsel[sel_group];
                                c->isfloating = c->oldstate = 1;
                                c->tags = new_mask;
                                if (new_mask != tagsel[sel_group]) {
                                        pertagsave(old_mask);
                                        sel_group ^= 1; tagsel[sel_group] = new_mask;
                                        pertagrestore(tagsel[sel_group]);
                                }
                                needar = 0;
                                ar();
                        }
                }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(display, CurrentTime);
        if (needar) ar();
}

static C *nexttiled(C *c) { for (; c && (c->isfloating || !VIS(c)); c = c->next); return c; }

static void pop(C *c) { detach(c); c->next = clients; clients = c; focus(c); ar(); }

static void
propertynotify(XEvent *e) {
        XPropertyEvent *ev = &e->xproperty;
        C *c;
        if (ev->state == PropertyDelete || !(c = wintoclient(ev->window))) return;
        if      (ev->atom == XA_WM_HINTS) {
                updatewmhints(c);
                XSetWindowBorder(display, c->win, c->isurgent ? color_urg : (c == sel ? color_sel : color_norm));
        }
        else if (ev->atom == XA_WM_NORMAL_HINTS)       c->hintsvalid = 0;
        else if (ev->atom == netatom[NetWMWindowType]) updatewindowtype(c);
}

static void quit(const A *arg) { (void)arg; running = 0; }

static void
resize(C *c, int x, int y, int w, int h, int interact) {
        if (interact || c->isfloating || !cur_layouts[layout_sel]->ar) {
                if (applysizehints(c, &x, &y, &w, &h, interact)) resizeclient(c, x, y, w, h);
        } else {
                resizeclient(c, x, y, w, h);
        }
}

static void
resizeclient(C *c, int x, int y, int w, int h) {
        XWindowChanges wc;
        c->oldx = c->x; c->x = x;
        c->oldy = c->y; c->y = y;
        c->oldw = c->w; c->w = w;
        c->oldh = c->h; c->h = h;
        wc.x = x; wc.y = y; wc.width = w; wc.height = h; wc.border_width = c->bw;
        XConfigureWindow(display, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
}

static void
resizemouse(const A *arg) {
        (void)arg;
        int ocx, ocy, ocw, och, nw, nh, needar = 0;
        XEvent ev;
        Time last = 0;
        C *c = sel;
        if (!c || c->isfullscreen) return;
        restack();
        ocx = c->x; ocy = c->y; ocw = c->w; och = c->h;
        if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, cursor[2], last_time) != GrabSuccess) return;
        XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
        do {
                XMaskEvent(display, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
                if (ev.type == ConfigureRequest || ev.type == Expose || ev.type == MapRequest)
                        handler[ev.type](&ev);
                else if (ev.type == MotionNotify) {
                        if (ev.xmotion.time - last <= FRAMERATE) continue;
                        last = ev.xmotion.time;
                        nw = MAX(ev.xmotion.x - ocx - (c->bw << 1) + 1, 1);
                        nh = MAX(ev.xmotion.y - ocy - (c->bw << 1) + 1, 1);
                        if (!c->isfloating && cur_layouts[layout_sel]->ar
                        && (abs(nw-ocw) > (int)snap || abs(nh-och) > (int)snap)) {
                                c->isfloating = c->oldstate = 1; needar = 1;
                        }
                        if (!cur_layouts[layout_sel]->ar || c->isfloating)
                                resize(c, c->x, c->y, nw, nh, 1);
                }
        } while (ev.type != ButtonRelease);
        XWarpPointer(display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
        XUngrabPointer(display, CurrentTime);
        if (needar) ar();
}

static void
restack(void) {
        C *c;
        XWindowChanges wc = { .stack_mode = Above };
        if (!sel) return;
        if (sel->isfullscreen) {
                XRaiseWindow(display, sel->win);
                return;
        }
        if (sel->isfloating || !cur_layouts[layout_sel]->ar) XRaiseWindow(display, sel->win);
        if (cur_layouts[layout_sel]->ar) {
                for (c = stack; c; c = c->snext)
                        if (!c->isfloating && VIS(c)) {
                                XConfigureWindow(display, c->win,
                                        wc.sibling ? CWSibling|CWStackMode : CWStackMode, &wc);
                                wc.sibling = c->win;
                        }
                for (c = stack; c; c = c->snext)
                        if (c->isfloating && VIS(c))
                                XRaiseWindow(display, c->win);
                if (sel->isfloating) XRaiseWindow(display, sel->win);
        }
}

static void
run(void) {
        XEvent ev;
        XSync(display, False);
        while (running && !XNextEvent(display, &ev))
                if (!XFilterEvent(&ev, None) && ev.type < LASTEvent && handler[ev.type])
                        handler[ev.type](&ev);
}

static void
scan(void) {
        unsigned int i, n;
        Window d1, d2, trans, *wins = NULL;
        XWindowAttributes wa;
        if (!XQueryTree(display, root, &d1, &d2, &wins, &n)) return;
        for (i = 0; i < n; i++) {
                if (!XGetWindowAttributes(display, wins[i], &wa)
                || wa.override_redirect || XGetTransientForHint(display, wins[i], &trans)) continue;
                if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
                        manage(wins[i], &wa);
        }
        for (i = 0; i < n; i++) {
                if (!XGetWindowAttributes(display, wins[i], &wa) || wa.override_redirect) continue;
                if (XGetTransientForHint(display, wins[i], &trans)
                && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                        manage(wins[i], &wa);
        }
        XFree(wins);
}

static int
sendevent(C *c, Atom proto) {
        int n, exists = 0; Atom *prots; XEvent ev = {0};
        if (XGetWMProtocols(display, c->win, &prots, &n)) {
                while (!exists && n--) exists = prots[n] == proto;
                XFree(prots);
        }
        if (exists) {
                ev.type                 = ClientMessage;
                ev.xclient.window       = c->win;
                ev.xclient.message_type = wmatom[WMProtocols];
                ev.xclient.format       = 32;
                ev.xclient.data.l[0]    = (long)proto;
                ev.xclient.data.l[1]    = last_time;
                XSendEvent(display, c->win, False, NoEventMask, &ev);
        }
        return exists;
}

static void
setclientstate(C *c, long state) {
        long data[] = { state, None };
        XChangeProperty(display, c->win, wmatom[WMState], wmatom[WMState], 32,
                PropModeReplace, (unsigned char*)data, 2);
}

static void
setfocus(C *c) {
        if (!c->neverfocus) {
                XSetInputFocus(display, c->win, RevertToPointerRoot, last_time);
                XChangeProperty(display, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                        PropModeReplace, (unsigned char*)&c->win, 1);
        }
        sendevent(c, wmatom[WMTakeFocus]);
}

static void
setfullscreen(C *c, int fs) {
        int fmt, i, j;
        unsigned long n, rem;
        unsigned char *p = NULL;
        Atom type, *atoms, *newatoms;
        if (fs && !c->isfullscreen) {
                XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
                c->isfullscreen = 1;
                c->oldstate = c->isfloating; c->oldbw = c->bw;
                c->bw = 0; c->isfloating = 1;
                resizeclient(c, 0, 0, screen_w, screen_h);
                XRaiseWindow(display, c->win);
        } else if (!fs && c->isfullscreen) {
                if (XGetWindowProperty(display, c->win, netatom[NetWMState], 0L,
                        1024L, False, XA_ATOM, &type, &fmt, &n, &rem, &p) == Success && p) {
                        if (type != XA_ATOM || fmt != 32 || n == 0) {
                                XDeleteProperty(display, c->win, netatom[NetWMState]);
                        } else {
                                atoms = (Atom *)p;
                                if (!(newatoms = (Atom *)malloc(n * sizeof(Atom)))) die("nwm: malloc");
                                for (i = 0, j = 0; i < (int)n; i++)
                                        if (atoms[i] != netatom[NetWMFullscreen])
                                                newatoms[j++] = atoms[i];
                                if (j == 0)
                                        XDeleteProperty(display, c->win, netatom[NetWMState]);
                                else
                                        XChangeProperty(display, c->win, netatom[NetWMState], XA_ATOM, 32,
                                                PropModeReplace, (unsigned char*)newatoms, j);
                                free(newatoms);
                        }
                        XFree(p);
                } else {
                        XDeleteProperty(display, c->win, netatom[NetWMState]);
                }
                c->isfullscreen = 0; c->isfloating = c->oldstate; c->bw = c->oldbw;
                resizeclient(c, c->oldx, c->oldy, c->oldw, c->oldh);
                ar();
        }
}

static void
setlayout(const A *arg) {
        if (arg->v == cur_layouts[layout_sel]) return;
        layout_sel ^= 1;
        cur_layouts[layout_sel] = (const L *)arg->v;
	pertagupdate(firsttag(tagsel[sel_group]));
        if (sel) ar();
}

static void
setmasterfact(const A *arg) {
        float f;
        if (!arg || !cur_layouts[layout_sel]->ar) return;
        f = (arg->f < 1.0f) ? master_factor + arg->f : arg->f - 1.0f;
        if (f < 0.05f || f > 0.95f) return;
        master_factor = f;
	pertagupdate(firsttag(tagsel[sel_group]));
        ar();
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
        unsigned int i;

        sa.sa_handler = SIG_IGN;
        sa.sa_flags   = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGCHLD, &sa, NULL) == -1) die("nwm: sigaction");

        screen_idx = DefaultScreen(display);
        screen_w = DisplayWidth(display, screen_idx);
        screen_h = DisplayHeight(display, screen_idx);
        root  = RootWindow(display, screen_idx);
        win_area_x = win_area_y = 0; win_area_w = screen_w; win_area_h = screen_h;

        cmap = DefaultColormap(display, screen_idx);
        if (!XAllocNamedColor(display, cmap, colnb, &xc, &exact)) die("nwm: cannot allocate color");
        color_norm = xc.pixel;
        if (!XAllocNamedColor(display, cmap, colsb, &xc, &exact)) die("nwm: cannot allocate color");
        color_sel = xc.pixel;
        if (!XAllocNamedColor(display, cmap, colub, &xc, &exact)) die("nwm: cannot allocate color");
        color_urg = xc.pixel;

        cursor[0] = XCreateFontCursor(display, XC_left_ptr);
        cursor[1] = XCreateFontCursor(display, XC_fleur);
        cursor[2] = XCreateFontCursor(display, XC_sizing);
        if (!cursor[0] || !cursor[1] || !cursor[2]) die("nwm: XCreateFontCursor");

        if (!XInternAtoms(display, wmnames,  WMLast,  False, wmatom))  die("nwm: XInternAtoms");
        if (!XInternAtoms(display, netnames, NetLast, False, netatom)) die("nwm: XInternAtoms");
        if (!XInternAtoms(display, auxnames, 3,       False, aux))     die("nwm: XInternAtoms");
        wmcheck_win = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
        if (wmcheck_win == None) die("nwm: XCreateSimpleWindow");
        XChangeProperty(display, wmcheck_win, aux[0], XA_WINDOW, 32,
                PropModeReplace, (unsigned char*)&wmcheck_win, 1);
        XChangeProperty(display, wmcheck_win, aux[1], aux[2], 8,
                PropModeReplace, (unsigned char*)"nwm", 3);
        XChangeProperty(display, root, aux[0], XA_WINDOW, 32,
                PropModeReplace, (unsigned char*)&wmcheck_win, 1);
        XDeleteProperty(display, root, netatom[NetClientList]);

        sel_group = layout_sel = 0;
        tagsel[0] = tagsel[1] = 1;
        master_factor  = mfact;
        master_count  = nmaster;
        cur_layouts[0] = &layouts[0];
        cur_layouts[1] = &layouts[1 % LEN(layouts)];

        for (i = 0; i < LEN(tags); i++) {
                per_tag[i].lt[0] = &layouts[0];
                per_tag[i].lt[1] = &layouts[1 % LEN(layouts)];
                per_tag[i].li = 0;
                per_tag[i].mf = mfact;
                per_tag[i].nm = nmaster;
        }

        grabkeys();
        wa.cursor     = cursor[0];
        wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
                      | ButtonPressMask|PointerMotionMask|EnterWindowMask
                      | StructureNotifyMask|PropertyChangeMask;
        XChangeWindowAttributes(display, root, CWEventMask|CWCursor, &wa);
        focus(NULL);
}

static void
seturgency(C *c, int urg) {
        XWMHints *wm;
        c->isurgent = urg;
        XSetWindowBorder(display, c->win, urg ? color_urg : (c == sel ? color_sel : color_norm));
        if (!(wm = XGetWMHints(display, c->win))) return;
        wm->flags = urg ? wm->flags | XUrgencyHint : wm->flags & ~XUrgencyHint;
        XSetWMHints(display, c->win, wm); XFree(wm);
}

static void
showhide(C *c) {
        for (; c; c = c->snext) {
                if (VIS(c)) {
                        if ((!cur_layouts[layout_sel]->ar || c->isfloating) && !c->isfullscreen)
                                resizeclient(c, c->x, c->y, c->w, c->h);
                } else {
                        XMoveWindow(display, c->win, -(W(c) + screen_w), c->y);
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
                if (display) close(ConnectionNumber(display));
                setsid();
                execvp(((const char**)arg->v)[0], (char*const*)arg->v);
                fprintf(stderr, "nwm: execvp %s\n", ((const char**)arg->v)[0]);
                _exit(1);
        }
}

static void tag(const A *arg) { if (sel && arg->ui & TM) { sel->tags = arg->ui & TM; focus(NULL); ar(); } }

static void
tile(void) {
        C *c;
        unsigned int i, n, nmaster_vis, nstack;
        int gap = gappx, master_w, master_h, stack_h, y0;
        for (n = 0, c = nexttiled(clients); c; c = nexttiled(c->next), n++);
        if (!n) return;
        nmaster_vis = (unsigned)master_count < n ? (unsigned)master_count : n;
        nstack     = n > nmaster_vis ? n - nmaster_vis : 0;
        master_w = nmaster_vis && nstack ? (int)((win_area_w - 3*gap) * master_factor) : (nmaster_vis ? win_area_w - 2*gap : 0);
        master_h = nmaster_vis ? MAX(1, (win_area_h - (int)(nmaster_vis + 1) * gap) / (int)nmaster_vis) : 0;
        stack_h  = nstack     ? MAX(1, (win_area_h - (int)(nstack + 1) * gap) / (int)nstack)  : 0;
        for (i = 0, c = nexttiled(clients); c; c = nexttiled(c->next), i++) {
                int bw2 = c->bw << 1;
                if (i < nmaster_vis) {
                        y0 = win_area_y + gap + (int)i * (master_h + gap);
                        resize(c, win_area_x + gap, y0, MAX(1, master_w - bw2), MAX(1, master_h - bw2), 0);
                } else {
                        y0 = win_area_y + gap + (int)(i - nmaster_vis) * (stack_h + gap);
                        resize(c, win_area_x + (nmaster_vis ? master_w + 2*gap : gap), y0,
                                MAX(1, (nmaster_vis ? win_area_w - master_w - 3*gap : win_area_w - 2*gap) - bw2),
                                MAX(1, stack_h - bw2), 0);
                }
        }
}

static void
togglefloating(const A *arg) {
        (void)arg;
        if (!sel || sel->isfullscreen) return;
        sel->isfloating = !sel->isfloating || sel->isfixed;
        if (sel->isfloating) resize(sel, sel->x, sel->y, sel->w, sel->h, 0);
        ar();
}

static void togglefullscreen(const A *arg) { (void)arg; if (sel) setfullscreen(sel, !sel->isfullscreen); }

static void
toggletag(const A *arg) {
        unsigned int t;
        if (!sel || !(t = sel->tags ^ (arg->ui & TM))) return;
        sel->tags = t; focus(NULL); ar();
}

static void
toggleview(const A *arg) {
        unsigned int new_mask, old_mask;

	new_mask = tagsel[sel_group] ^ (arg->ui & TM);
	if (!new_mask) return;

	old_mask = tagsel[sel_group];
	pertagsave(old_mask);
	tagsel[sel_group] = new_mask;
	pertagrestore(tagsel[sel_group]);

	focus(NULL); ar();
}

static void
unfocus(C *c) {
        if (!c) return;
        grabbuttons(c, 0);
        XSetWindowBorder(display, c->win, c->isurgent ? color_urg : color_norm);
}

static void
unmanage(C *c, int destroyed) {
        XWindowChanges wc;
        detach(c); detachstack(c);
        if (!destroyed) {
                wc.border_width = c->oldbw;
                XGrabServer(display);
                XSetErrorHandler(xerrordummy);
                XSelectInput(display, c->win, NoEventMask);
                XConfigureWindow(display, c->win, CWBorderWidth, &wc);
                XUngrabButton(display, AnyButton, AnyModifier, c->win);
                setclientstate(c, WithdrawnState);
                XSync(display, False);
                XSetErrorHandler(xerror);
                XUngrabServer(display);
        }
        free(c);
        focus(NULL); updateclientlist(); ar();
}

static void
unmapnotify(XEvent *e) {
        XUnmapEvent *ev = &e->xunmap;
        C *c;
        if ((c = wintoclient(ev->window))) {
                if (ev->send_event) setclientstate(c, WithdrawnState);
                else unmanage(c, 0);
        }
}

static void
updateclientlist(void) {
        C *c;
        XDeleteProperty(display, root, netatom[NetClientList]);
        for (c = clients; c; c = c->next)
                XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32,
                        PropModeAppend, (unsigned char*)&c->win, 1);
}

static void
updatenumlockmask(void) {
        unsigned int i, j;
        KeyCode nlk;
        XModifierKeymap *mm = XGetModifierMapping(display);
        if (!mm) return;
        numlockmask = 0;
        nlk = XKeysymToKeycode(display, XK_Num_Lock);
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
updatesizehints(C *c) {
        long ms; XSizeHints sz;
        if (!XGetWMNormalHints(display, c->win, &sz, &ms)) sz.flags = PSize;
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
updatewindowtype(C *c) {
        int fmt, i, fs = 0;
        unsigned long n, rem;
        unsigned char *p = NULL;
        Atom type, *atoms;
        if (XGetWindowProperty(display, c->win, netatom[NetWMState], 0L,
                1024L, False, XA_ATOM, &type, &fmt, &n, &rem, &p) == Success && p) {
                if (type == XA_ATOM && fmt == 32) {
                        atoms = (Atom *)p;
                        for (i = 0; i < (int)n; i++)
                                if (atoms[i] == netatom[NetWMFullscreen]) { fs = 1; break; }
                }
                XFree(p);
        }
        if (fs && !c->isfullscreen) setfullscreen(c, 1);
        else if (!fs && c->isfullscreen) setfullscreen(c, 0);
        if (getatom(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog])
                c->isfloating = 1;
        else
                c->isfloating = c->oldstate || c->isfixed;
}

static void
updatewmhints(C *c) {
        XWMHints *wm;
        if (!(wm = XGetWMHints(display, c->win))) return;
        if (c == sel && wm->flags & XUrgencyHint) {
                wm->flags &= ~XUrgencyHint; XSetWMHints(display, c->win, wm);
        } else {
                c->isurgent = !!(wm->flags & XUrgencyHint);
        }
        c->neverfocus = !!(wm->flags & InputHint) && !wm->input;
        XFree(wm);
}

static void
view(const A *arg) {
        unsigned int old_mask;

        if ((arg->ui & TM) == tagsel[sel_group]) return;
        if (!(arg->ui & TM) && tagsel[0] == tagsel[1]) return;

        old_mask = tagsel[sel_group];
        pertagsave(old_mask);

        sel_group ^= 1;
        if (arg->ui & TM) tagsel[sel_group] = arg->ui & TM;

        pertagrestore(tagsel[sel_group]);

        focus(NULL); ar();
}

static C *wintoclient(Window w) { C *c; for (c = clients; c; c = c->next) if (c->win == w) return c; return NULL; }

static int
xerror(Display *dpy, XErrorEvent *ee) {
        if (ee->error_code == BadWindow
        || (ee->request_code == XP_SetInputFocus   && ee->error_code == BadMatch)
        || (ee->request_code == XP_ConfigureWindow && ee->error_code == BadMatch))
                return 0;
        if ((ee->request_code == XP_GrabButton && ee->error_code == BadAccess)
        ||  (ee->request_code == XP_GrabKey    && ee->error_code == BadAccess)) {
                fprintf(stderr, "nwm: warning: failed to grab key/button (BadAccess conflict)\n");
                return 0;
        }
        fprintf(stderr, "nwm: error req=%d code=%d\n", ee->request_code, ee->error_code);
        return xerrorxlib(dpy, ee);
}

static int xerrordummy(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; return 0; }
static int xerrorstart(Display *dpy, XErrorEvent *e) { (void)dpy; (void)e; fprintf(stderr, "nwm: another wm is running\n"); exit(1); }

static void
zoom(const A *arg) {
        (void)arg;
        C *c = sel;
        if (!cur_layouts[layout_sel]->ar || !c || c->isfloating) return;
        if (c == nexttiled(clients) && !(c = nexttiled(c->next))) return;
        pop(c);
}

int
main(int argc, char *argv[]) {
        if (argc == 2 && !strcmp("-v", argv[1])) die("nwm-1.5");
        else if (argc != 1) die("usage: nwm [-v]");
        if (!(display = XOpenDisplay(NULL))) die("nwm: cannot open display");
        checkwm();
        setup();
#ifdef __OpenBSD__
        pledge("stdio rpath proc exec ps", NULL);
#endif
        scan(); run(); cleanup();
        XCloseDisplay(display);
        return 0;
}
