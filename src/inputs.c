/*
 * FusionWM - inputs.c
 */

#include "globals.h"
#include "inputs.h"
#include "layout.h"
#include "clientlist.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/cursorfont.h>

#include "binds.h"

static unsigned int numlockmask = 0;

static XButtonPressedEvent g_button_drag_start;
static XRectangle       g_win_drag_start;
static HSClient*        g_win_drag_client = NULL;
static HSMonitor*       g_drag_monitor = NULL;
static MouseBinding*    g_drag_bind = NULL;

static Cursor g_cursor;
static unsigned int numlockmask;

#define REMOVEBUTTONMASK(mask) ((mask) & ~( Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask ))

void mouse_start_drag(XEvent* ev) {
    XButtonEvent* be = &(ev->xbutton);
    g_drag_bind = mouse_binding_find(be->state, be->button);
    if (!g_drag_bind) {
        // there is no valid bind for this type of mouse event
        return;
    }
    Window win = ev->xbutton.subwindow;
    g_win_drag_client = get_client_from_window(win);
    if (!g_win_drag_client) {
        g_drag_bind = NULL;
        return;
    }
    if (!g_win_drag_client->pseudotile) {
        // only can drag wins in  floating mode or pseudotile
        g_win_drag_client = NULL;
        g_drag_bind = NULL;
        return;
    }
    g_win_drag_start = g_win_drag_client->float_size;
    g_button_drag_start = ev->xbutton;
    g_drag_monitor = get_current_monitor();
    XGrabPointer(g_display, win, True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

void mouse_stop_drag() {
    g_win_drag_client = NULL;
    g_drag_bind = NULL;
    XUngrabPointer(g_display, CurrentTime);
}

void handle_motion_event(XEvent* ev) {
    if (g_drag_monitor != get_current_monitor()) {
        mouse_stop_drag();
        return;
    }
    if (!g_win_drag_client) return;
    if (!g_drag_bind) return;
    if (ev->type != MotionNotify) return;
    MouseFunction function = g_drag_bind->function;
    if (!function) return;
    // call function that handles it
    function(&(ev->xmotion));
}

static void grab_button(MouseBinding* mb) {
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    // grab button for each modifier that is ignored (capslock, numlock)
    for (int i = 0; i < LENGTH(modifiers); i++) {
        XGrabButton(g_display, mb->button, modifiers[i]|mb->mask,
                    g_root, True, ButtonPressMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }
}

void grab_buttons() {
   update_numlockmask();
   // init modifiers after updating numlockmask
   XUngrabButton(g_display, AnyButton, AnyModifier, g_root);
   for(int i=0; i<2; i++) grab_button(&buttons[i]);
}

MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button) {
   for(int i=0; i<LENGTH(buttons); i++){
      MouseBinding * mb = &buttons[i];
      if((REMOVEBUTTONMASK(CLEANMASK(modifiers)) == REMOVEBUTTONMASK(CLEANMASK(mb->mask))) &&
         (button == mb->button))
         return mb;
   }
   return NULL;
}

void mouse_function_move(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.x += x_diff;
    g_win_drag_client->float_size.y += y_diff;
    // snap it to other windows
    int dx, dy;
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                       SNAP_EDGE_ALL, &dx, &dy);
    g_win_drag_client->float_size.x += dx;
    g_win_drag_client->float_size.y += dy;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

void mouse_function_resize(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    g_win_drag_client->float_size = g_win_drag_start;
    // relative x/y coords in drag window
    int rel_x = g_button_drag_start.x_root - g_win_drag_start.x;
    int rel_y = g_button_drag_start.y_root - g_win_drag_start.y;
    bool top = false;
    bool left = false;
    if (rel_y < g_win_drag_start.height/2) {
        top = true;
        y_diff *= -1;
    }
    if (rel_x < g_win_drag_start.width/2) {
        left = true;
        x_diff *= -1;
    }
    // avoid an overflow
    int new_width  = g_win_drag_client->float_size.width + x_diff;
    int new_height = g_win_drag_client->float_size.height + y_diff;
    if (left)   g_win_drag_client->float_size.x -= x_diff;
    if (top)    g_win_drag_client->float_size.y -= y_diff;
    if (new_width <  WINDOW_MIN_WIDTH)  new_width = WINDOW_MIN_WIDTH;
    if (new_height < WINDOW_MIN_HEIGHT) new_height = WINDOW_MIN_HEIGHT;
    g_win_drag_client->float_size.width  = new_width;
    g_win_drag_client->float_size.height = new_height;
    // snap it to other windows
    int dx, dy;
    int snap_flags = 0;
    if (left)   snap_flags |= SNAP_EDGE_LEFT;
    else        snap_flags |= SNAP_EDGE_RIGHT;
    if (top)    snap_flags |= SNAP_EDGE_TOP;
    else        snap_flags |= SNAP_EDGE_BOTTOM;
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                       snap_flags, &dx, &dy);
    if (left) {
        g_win_drag_client->float_size.x += dx;
        dx *= -1;
    }
    if (top) {
        g_win_drag_client->float_size.y += dy;
        dy *= -1;
    }
    g_win_drag_client->float_size.width += dx;
    g_win_drag_client->float_size.height += dy;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

struct SnapData {
    HSClient*       client;
    XRectangle      rect;
    enum SnapFlags  flags;
    int             dx, dy; // the vector from client to other to make them snap
};

static bool is_point_between(int point, int left, int right) {
    return (point < right && point >= left);
}

static bool intervals_intersect(int a_left, int a_right, int b_left, int b_right) {
    return is_point_between(a_left, b_left, b_right)
        || is_point_between(a_right, b_left, b_right)
        || is_point_between(b_right, a_left, a_right)
        || is_point_between(b_left, a_left, a_right);
}

// compute vector to snap a point to an edge
static void snap_1d(int x, int edge, int* delta) {
    // whats the vector from subject to edge?
    int cur_delta = edge - x;
    // if distance is smaller then all other deltas
    if (abs(cur_delta) < abs(*delta)) {
        // then snap it, i.e. save vector
        *delta = cur_delta;
    }
}

static int client_snap_helper(HSClient* candidate, struct SnapData* d) {
   if (candidate == d->client)  return 0;
   
   XRectangle subject  = d->rect;
   XRectangle other    = client_outer_floating_rect(candidate);
   if (intervals_intersect(other.y, other.y + other.height, subject.y, subject.y + subject.height)) {
      // check if x can snap to the right
      if (d->flags & SNAP_EDGE_RIGHT)  snap_1d(subject.x + subject.width, other.x, &d->dx);
      // or to the left
      if (d->flags & SNAP_EDGE_LEFT)   snap_1d(subject.x, other.x + other.width, &d->dx);
   }
   if (intervals_intersect(other.x, other.x + other.width, subject.x, subject.x + subject.width)) {
      // if we can snap to the top
      if (d->flags & SNAP_EDGE_TOP)    snap_1d(subject.y, other.y + other.height, &d->dy);
      // or to the bottom
      if (d->flags & SNAP_EDGE_BOTTOM) snap_1d(subject.y + subject.height, other.y, &d->dy);
   }
   return 0;
}

// get the vector to snap a client to it's neighbour
void client_snap_vector(struct HSClient* client, struct HSTag* tag,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy) {
    struct SnapData d;
    int distance = (snap_distance > 0) ? snap_distance : 0;
    // init delta
    *return_dx = 0;
    *return_dy = 0;
    if (!distance)  return;
    
    d.client    = client;
    d.rect      = client_outer_floating_rect(client);
    d.flags     = flags;
    d.dx        = distance;
    d.dy        = distance;

    // snap to monitor edges
    HSMonitor* m = g_drag_monitor;
    if (flags & SNAP_EDGE_TOP)      snap_1d(d.rect.y, 0, &d.dy);
    if (flags & SNAP_EDGE_LEFT)     snap_1d(d.rect.x, 0, &d.dx);
    if (flags & SNAP_EDGE_RIGHT)    snap_1d(d.rect.x + d.rect.width, m->rect.width, &d.dx);
    if (flags & SNAP_EDGE_BOTTOM)   snap_1d(d.rect.y + d.rect.height, m->rect.height - bh, &d.dy);
    
    // snap to other clients
    frame_foreach_client(tag->frame, (ClientAction)client_snap_helper, &d);

    // write back results
    if (abs(d.dx) < abs(distance))  *return_dx = d.dx;
    if (abs(d.dy) < abs(distance))  *return_dy = d.dy;
}

//--------
void inputs_init() {
   grab_keys();
   grab_buttons();

   /* set cursor theme */
   g_cursor = XCreateFontCursor(g_display, XC_left_ptr);
   XDefineCursor(g_display, g_root, g_cursor);
}

void inputs_destroy() {
    XFreeCursor(g_display, g_cursor);
}

void key_press(XEvent* ev) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *event;

    event = &ev->xkey;
    keysym = XkbKeycodeToKeysym(g_display, (KeyCode)event->keycode, 0, 0);
    for(i =0; i<LENGTH(keys); i++)
       if(keysym == keys[i].keysym &&
          CLEANMASK(keys[i].mod) == CLEANMASK(event->state) &&
          keys[i].func)
          keys[i].func(&(keys[i].arg));
}

void grab_keys(void) {
   update_numlockmask();
   
   unsigned int i, j;
   unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
   KeyCode code;

   XUngrabKey(g_display, AnyKey, AnyModifier, g_root); //remove all current grabs
   for(i = 0; i < LENGTH(keys); i++)
      if((code = XKeysymToKeycode(g_display, keys[i].keysym)))
         for(j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(g_display, code, keys[i].mod | modifiers[j], g_root,
                  True, GrabModeAsync, GrabModeAsync);
}

// update the numlockmask
void update_numlockmask() {
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(g_display);
    for(i = 0; i < 8; i++)
        for(j = 0; j < modmap->max_keypermod; j++)
            if(modmap->modifiermap[i * modmap->max_keypermod + j]
               == XKeysymToKeycode(g_display, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

