/*
 * FusionWM - inputs.h
 */

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include <glib.h>
#include <X11/Xlib.h>

enum SnapFlags {
    // which edges are considered to snap
    SNAP_EDGE_TOP       = 0x01,
    SNAP_EDGE_BOTTOM    = 0x02,
    SNAP_EDGE_LEFT      = 0x04,
    SNAP_EDGE_RIGHT     = 0x08,
    SNAP_EDGE_ALL       =
        SNAP_EDGE_TOP | SNAP_EDGE_BOTTOM | SNAP_EDGE_LEFT | SNAP_EDGE_RIGHT,
};

// foreward declarations
struct HSClient;
struct HSTag;

typedef void (*MouseFunction)(XMotionEvent*);

typedef struct MouseBinding {
    unsigned int mask;
    unsigned int button;
    MouseFunction function;
} MouseBinding;

typedef struct KeyBinding {
   unsigned int mod;
   KeySym keysym;
   void (*func)(const Arg *);
   const Arg arg;
} KeyBinding;

//---
void inputs_init();
void inputs_destroy();

void key_find_binds(char* needle, GString** output);
MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);
void grab_keys();
void grab_buttons();

void mouse_start_drag(XEvent* ev);
void mouse_stop_drag();
void handle_motion_event(XEvent* ev);

// get the vector to snap a client to it's neighbour
void client_snap_vector(struct HSClient* client, struct HSTag* tag,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

/* some mouse functions */
void mouse_function_move(XMotionEvent* me);
void mouse_function_resize(XMotionEvent* me);

void key_press(XEvent* ev);
void update_numlockmask();

#endif

