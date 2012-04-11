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

typedef int (*ClientAction)(struct HSClient*, void* data);

#define FRACTION_UNIT 10000

typedef struct HSLayout {
    int align;         // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    struct HSFrame* a; // first child
    struct HSFrame* b; // second child
    int selection;
    int fraction; // size of first child relative to whole size
} HSLayout;

typedef struct HSFrame {
    union {
        HSLayout layout;
        struct {
            Window* buf;
            size_t  count;
            size_t  floatcount;
            int     selection;
            int     layout;
        } clients;
    } content;
    int type;
    struct HSFrame* parent;
    Window window;
    bool   window_visible;
} HSFrame;

typedef struct HSMonitor {
    struct HSTag*      tag;    // currently viewed tag
    struct {
        int x;
        int y;
    } mouse; // last saved mouse position
    XRectangle  rect;   // area for this monitor
    Window barwin;
    int primary;
} HSMonitor;

typedef struct HSTag {
    const char* name;   // name of this tag
    HSFrame*    frame;  // the master frame
    bool       urgent;
} HSTag;

int         g_cur_monitor;
HSFrame*    g_cur_frame; // currently selected frame

//--- Functions
void layout_init();
void layout_destroy();

// for frames
HSFrame* frame_create_empty();
void frame_insert_window(HSFrame* frame, Window window);
HSFrame* frame_descend(HSFrame* frame);
bool frame_remove_window(HSFrame* frame, Window window);
void frame_destroy(HSFrame* frame, Window** buf, size_t* count);
void frame_split(HSFrame* frame, int align, int fraction);
void split_v(const Arg *arg);
void split_h(const Arg *arg);
void resize_frame(const Arg *arg);

void frame_apply_layout(HSFrame* frame, XRectangle rect);

void cycle(const Arg * arg);

HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_inner_neighbour_index(HSFrame* frame, char direction);
void focus(const Arg* arg);

int frame_focus_recursive(HSFrame* frame);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order);
void frame_show_clients(HSFrame* frame);
int frame_foreach_client(HSFrame* frame, ClientAction action, void* data);

void set_layout(const Arg *arg);

Window frame_focused_window(HSFrame* frame);
void focus_window(Window win, bool switch_tag, bool switch_monitor);
void shift(const Arg *arg);
void frame_remove(const Arg *arg);
void frame_remove_function(HSFrame *frame);
void frame_set_visible(HSFrame* frame, bool visible);

// for tags
void add_tag(const char* name);
HSTag* find_tag(const char* name);
void move_tag(const Arg *arg);
void tag_move_window(HSTag* target);

// for monitors
HSMonitor* find_monitor_with_tag(HSTag* tag);
void add_monitor(XRectangle rect, HSTag* tag, int primary);
void monitor_focus_by_index(int new_selection);
int monitor_index_of(HSMonitor* monitor);
void focus_monitor(const Arg *arg);
HSMonitor* get_current_monitor();
void monitor_set_tag(HSMonitor* monitor, HSTag* tag);
void use_tag(const Arg *arg);
void monitor_apply_layout(HSMonitor* monitor);
void all_monitors_apply_layout();
void monitors_init();

// for bars and miscellaneous
void create_bar(HSMonitor *mon);
void draw_bar(HSMonitor *mon);
void draw_bars();

void updatestatus(void);
void drawtext(const char *text, unsigned long col[ColLast]);
void initfont(const char *fontstr);
int textnw(const char *text, unsigned int len);
int get_textw(const char *text);
 
HSMonitor* wintomon(Window w);

#endif

