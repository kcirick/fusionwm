//-- Appearance and Tags --------------------------------------------------
//static const char font[]               = "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*";
static const char font[]               = "-*-ohsnap-medium-r-*-*-12-*-*-*-*-*-*-*" ","
                                         "-*-xbmicons-medium-r-*-*-12-*-*-*-*-*-*-*";
static const int  window_gap           = 3;
static const int  frame_border_width   = 1;
static const int  window_border_width  = 2;
static const int  bar_height           = 20; 

// Systray
static const bool          systray_visible       = true;
static const unsigned int  systray_initial_gap   = 8;
static const unsigned int  systray_spacing       = 0;

#define NCOLORS 4
static const char colors[NCOLORS][ColLast][8] = {
   // frame-border   window-border  foreground  background
   { "#000000",      "#000000",     "#FFFFFF",  "#111111"}, // 0 - normal
   { "#333333",      "#1793D0",     "#1793D0",  "#111111"}, // 1 - selected
   { "#000000",      "#111111",     "#333333",  "#111111"}, // 2 - inactive tags
   { "#000000",      "#FF0000",     "#FF0000",  "#111111"}, // 3 - urgent tags
};

#define NTAGS 5
static const char tags[NTAGS][10] = { "One", "Two", "Three", 
                                       "Four", "Five" };

//-- Other configurations -------------------------------------------------
static const int     focus_follows_mouse  = 1;
static const int     focus_follows_shift  = 1;
static const int     focus_new_clients    = 1;
static const int     raise_on_click       = 1;
static const int     default_frame_layout = 0;
static const float   resize_step          = 0.025;


//-- Rules ----------------------------------------------------------------
static const Rule custom_rules[] = {
   //Class                    Tag,  Float
   { "Gmrun",                 -1,   1 },
   { "Canvas",                -1,   1 },
   { "Wpa_gui",               -1,   1 },
   { "Gsimplecal",            -1,   1 },
};

