/*
 * FusionWM - layout.h
 */

#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include <stdlib.h>
#include "globals.h"

enum { ALIGN_VERTICAL, ALIGN_HORIZONTAL };
enum { LAYOUT_VERTICAL, LAYOUT_HORIZONTAL, LAYOUT_MAX, LAYOUT_COUNT };
enum { TYPE_CLIENTS, TYPE_FRAMES };

enum { ColFrameBorder, ColWindowBorder, ColFG, ColBG, ColLast }; /* color */

typedef int (*ClientAction)(struct Client*, void* data);

#define FRACTION_UNIT 10000

typedef struct Layout {
    int align;       // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    struct Frame* a; // first child
    struct Frame* b; // second child
    int selection;
    int fraction;    // size of first child relative to whole size
} Layout;

typedef struct Frame {
    union {
        Layout layout;
        struct {
            Window* buf;
            size_t  count;
            size_t  floatcount;
            int     selection;
            int     layout;
        } clients;
    } content;
    int type;
    struct Frame* parent;
    Window window;
    bool   window_visible;
} Frame;

typedef struct Monitor {
    struct Tag*      tag;    // currently viewed tag
    struct {
        int x;
        int y;
    } mouse; // last saved mouse position
    XRectangle  rect;   // area for this monitor
    Window barwin;
    int primary;
} Monitor;

typedef struct Tag {
    const char* name;   // name of this tag
    Frame*    frame;    // the master frame
    bool       urgent;
} Tag;

typedef struct {
   int x, y, w, h;
   unsigned long colors[10][ColLast];
   Drawable drawable;
   GC gc;
   struct {
      int ascent;
      int descent;
      int height;
      XFontSet set;
      XFontStruct *xfont;
   } font;
} DC; // draw context

//--- Variables
int         g_cur_monitor;
Frame*      g_cur_frame;   // currently selected frame
GArray*     g_tags;        // Array of Tags
GArray*     g_monitors;    // Array of Monitors
DC          dc;

char stext[256];


//--- Functions
void layout_init();
void layout_destroy();

// for frames
Frame* frame_create_empty();
void frame_insert_window(Frame* frame, Window window);
Frame* frame_descend(Frame* frame);
bool frame_remove_window(Frame* frame, Window window);
void frame_destroy(Frame* frame, Window** buf, size_t* count);
void frame_split(Frame* frame, int align, int fraction);
void split_v(const Arg *arg);
void split_h(const Arg *arg);
void resize_frame(const Arg *arg);

void frame_apply_layout(Frame* frame, XRectangle rect);

void cycle(const Arg * arg);

Frame* frame_neighbour(Frame* frame, char direction);
int frame_inner_neighbour_index(Frame* frame, char direction);
void focus(const Arg* arg);

int frame_focus_recursive(Frame* frame);
void frame_do_recursive(Frame* frame, void (*action)(Frame*), int order);
void frame_show_clients(Frame* frame);
int frame_foreach_client(Frame* frame, ClientAction action, void* data);
void frame_hide(Frame* frame); 

void set_layout(const Arg *arg);

Window frame_focused_window(Frame* frame);
void focus_window(Window win, bool switch_tag, bool switch_monitor);
void shift(const Arg *arg);
void frame_remove(const Arg *arg);
void frame_remove_function(Frame *frame);
void frame_set_visible(Frame* frame, bool visible);

// for tags
void add_tag(const char* name);
Tag* find_tag(const char* name);
void move_tag(const Arg *arg);
void tag_move_window(Tag* target);
void use_tag(const Arg *arg);

// for monitors
Monitor* find_monitor_with_tag(Tag* tag);
void add_monitor(XRectangle rect, Tag* tag, int primary);
void monitor_focus_by_index(int new_selection);
int monitor_index_of(Monitor* monitor);
void focus_monitor(const Arg *arg);
Monitor* get_current_monitor();
Monitor* get_primary_monitor();
void monitor_set_tag(Monitor* monitor, Tag* tag);
void monitor_apply_layout(Monitor* monitor);
void all_monitors_apply_layout();
void update_monitors();

// for bars and miscellaneous
void update_bar(Monitor *mon);
void draw_bar(Monitor *mon);
void draw_bars();

void update_status(void);
void drawtext(const char *text, unsigned long col[ColLast]);
void initfont(const char *fontstr);
int textnw(const char *text, unsigned int len);
int get_textw(const char *text);
 
Monitor* wintomon(Window w);

void resizebarwin(Monitor *m);

#endif

