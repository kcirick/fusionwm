//-- Key binds --------------------------------------------------------------------------
#define MODKEY Mod4Mask // Win key
#define TAGKEYS(KEY,TAG) \
{MODKEY,             KEY,        use_tag,       {.i = TAG} }, \
{MODKEY|ShiftMask,   KEY,        move_tag,      {.i = TAG} },

static KeyBinding keys[] = {
   // modifier                      key         function       argument
   { MODKEY|ShiftMask,              XK_Return,  spawn,         {.v = "urxvt"} },
   { MODKEY|ShiftMask,              XK_w,       spawn,         {.v = "chromium"} },
   { MODKEY|ShiftMask,              XK_g,       spawn,         {.v = "gvim"} },
   { MODKEY|ShiftMask,              XK_t,       spawn,         {.v = "thunar"} },
   { MODKEY|ShiftMask,              XK_r,       spawn,         {.v = "gmrun"} },
   { MODKEY,                        XK_q,       quit,          {0} },
   { MODKEY,                        XK_c,       client_close,  {0} },
   { MODKEY,                        XK_m,       set_layout,    {.v = "max"} },
   { MODKEY,                        XK_v,       set_layout,    {.v = "vertical"} },
   { MODKEY,                        XK_h,       set_layout,    {.v = "horizontal"} },
   { MODKEY,                        XK_f,       set_pseudotile, {0} },
   { MODKEY|ShiftMask,              XK_v,       split_v,       {.f = 0.5} },
   { MODKEY|ShiftMask,              XK_h,       split_h,       {.f = 0.5} },
   { MODKEY,                        XK_r,       frame_remove,  {0} },
   { MODKEY,                        XK_Left,    focus,         {.v = "left"} },
   { MODKEY,                        XK_Right,   focus,         {.v = "right"} },
   { MODKEY,                        XK_Up,      focus,         {.v = "up"} },
   { MODKEY,                        XK_Down,    focus,         {.v = "down"} },
   { MODKEY,                        XK_comma,   focus_monitor, {.i = 0} },
   { MODKEY,                        XK_period,  focus_monitor, {.i = 1} },
   { MODKEY|ShiftMask,              XK_Left,    shift,         {.v = "left"} },
   { MODKEY|ShiftMask,              XK_Right,   shift,         {.v = "right"} },
   { MODKEY|ShiftMask,              XK_Up,      shift,         {.v = "up"} },
   { MODKEY|ShiftMask,              XK_Down,    shift,         {.v = "down"} },
   { MODKEY,                        XK_Tab,     cycle,         {0} },
   TAGKEYS(                         XK_1,                      0)
   TAGKEYS(                         XK_2,                      1)
   TAGKEYS(                         XK_3,                      2)
   TAGKEYS(                         XK_4,                      3)
   TAGKEYS(                         XK_5,                      4)
   TAGKEYS(                         XK_6,                      5)
   TAGKEYS(                         XK_7,                      6)
   { MODKEY|ControlMask,            XK_Left,    resize_frame,  {.v="left"} },
   { MODKEY|ControlMask,            XK_Right,   resize_frame,  {.v="right"} },
   { MODKEY|ControlMask,            XK_Up,      resize_frame,  {.v="up"} },
   { MODKEY|ControlMask,            XK_Down,    resize_frame,  {.v="down"} },
};

// -- Mouse bindings --------------------------------------------------------------------
static MouseBinding buttons[] = {
   //event mask        button         function       
   { MODKEY,           Button1,       mouse_function_move },
   { MODKEY,           Button3,       mouse_function_resize },
};

