//-- Appearance and Tags ----------------------------------------------------------------
static const char font[]               = "-*-clean-medium-r-*-*-12-*-*-*-*-*-*-*";
static const int window_gap            = 3;
static const int snap_distance         = 10;
static const int frame_border_width    = 1;
static const int window_border_width   = 1;
static const int bh                    = 20;  // bar height
static const int systray_width         = 100;

#define NUMCOLORS 4
static const char colors[NUMCOLORS][ColLast][8] = {
   // frame-border   window-border  foreground  background
   { "#000000",      "#000000",     "#000000",  "#F6F6F6"}, // 0 - normal
   { "#000000",      "#1793D0",     "#1793D0",  "#F6F6F6"}, // 1 - selected
   { "#000000",      "#F6F6F6",     "#333333",  "#F6F6F6"}, // 2 - inactive tags
   { "#000000",      "#FF0000",     "#FF0000",  "#F6F6F6"}, // 3 - urgent tags
};

#define NUMTAGS 7
static const char tags[NUMTAGS][10] = { "Eins", "Zwei", "Drei", 
                                         "Vier", "Fünf", "Sechs", "Sieben" };

//-- Other configurations ---------------------------------------------------------------
static const int focus_follows_mouse = 1;
static const int focus_follows_shift = 1;
static const int focus_new_clients = 1;
static const int raise_on_click = 1;
static const int default_frame_layout  = 0;
static const float resize_step = 0.025;

//-- Rules ------------------------------------------------------------------------------
static const Rule custom_rules[] = {
   //Condition, Value || Tag, Manage, Pseudotile
   { "windowtype", "_NET_WM_WINDOW_TYPE_DIALOG",       -1, 1, 1 },
   { "windowtype", "_NET_WM_WINDOW_TYPE_UTILITY",      -1, 1, 1 },
   { "windowtype", "_NET_WM_WINDOW_TYPE_SPLASH",       -1, 1, 1 },
   { "windowtype", "_NET_WM_WINDOW_TYPE_NOTIFICATION", -1, 0, 0 },
   { "windowtype", "_NET_WM_WINDOW_TYPE_DOCK",         -1, 0, 0 },
   { "class", "Gmrun",                                 -1, 1, 1 },
   { "class", "Canvas",                                -1, 1, 1 },
   { "class", "net-sourceforge-jnlp-runtime-Boot",     -1, 1, 1 },
};

