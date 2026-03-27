#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY, view,        {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY, toggleview,  {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY, tag,         {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY, toggletag,   {.ui = 1 << TAG} },

static const unsigned int borderpx    = 2;
static const unsigned int gappx       = 6;
static const unsigned int snap        = 16;
static const int          attachbottom = 0;
static const int          focusonopen  = 1;
static const float        mfact        = 0.55;
static const int          nmaster      = 1;

static const char col_nborder[] = "#1e1e1e";
static const char col_sborder[] = "#7c9e7e";
static const char col_uborder[] = "#c47f50";

static const char *tags[] = { "1","2","3","4","5","6","7","8","9" };

static const L layouts[] = {
	{ tile    },
	{ NULL    },
	{ monocle },
};

static const char *termcmd[]  = { "st", NULL };
static const char *dmenucmd[] = { "dmenu_run", NULL };

static const K keys[] = {
	{ MODKEY,            XK_Return, spawn,            {.v = termcmd  } },
	{ MODKEY,            XK_d,      spawn,            {.v = dmenucmd } },
	{ MODKEY,            XK_q,      killclient,       {0} },
	{ MODKEY|ShiftMask,  XK_e,      quit,             {0} },
	{ MODKEY,            XK_j,      focusstack,       {.i = +1} },
	{ MODKEY,            XK_k,      focusstack,       {.i = -1} },
	{ MODKEY,            XK_i,      incnmaster,       {.i = +1} },
	{ MODKEY,            XK_o,      incnmaster,       {.i = -1} },
	{ MODKEY,            XK_h,      setmfact,         {.f = -0.05} },
	{ MODKEY,            XK_l,      setmfact,         {.f = +0.05} },
	{ MODKEY,            XK_space,  zoom,             {0} },
	{ MODKEY,            XK_Tab,    view,             {0} },
	{ MODKEY,            XK_t,      setlayout,        {.v = &layouts[0]} },
	{ MODKEY,            XK_f,      setlayout,        {.v = &layouts[1]} },
	{ MODKEY,            XK_m,      setlayout,        {.v = &layouts[2]} },
	{ MODKEY,            XK_F11,    togglefullscreen, {0} },
	{ MODKEY|ShiftMask,  XK_space,  togglefloating,   {0} },
	{ MODKEY,            XK_0,      view,             {.ui = ~0} },
	{ MODKEY|ShiftMask,  XK_0,      tag,              {.ui = ~0} },
	TAGKEYS(XK_1,0) TAGKEYS(XK_2,1) TAGKEYS(XK_3,2)
	TAGKEYS(XK_4,3) TAGKEYS(XK_5,4) TAGKEYS(XK_6,5)
	TAGKEYS(XK_7,6) TAGKEYS(XK_8,7) TAGKEYS(XK_9,8)
};

static const B buttons[] = {
	{ ClkClientWin, MODKEY, Button1, mv, {0} },
	{ ClkClientWin, MODKEY, Button2, togglefloating, {0} },
	{ ClkClientWin, MODKEY, Button3, rz, {0} },
};
