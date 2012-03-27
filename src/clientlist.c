/* 
 * FusionWM - clientlist.c
 */

#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "inputs.h"
#include "config.h"

GArray* g_clients; // Array of HSClient*
unsigned long wincolors[NUMCOLORS][ColLast];

enum { WMProtocols, WMDelete, WMState, WMLast }; /* default atoms */
static Atom g_wmatom[WMLast];

static HSClient* create_client() {
    HSClient* hc   = g_new0(HSClient, 1);
    hc->urgent     = false;
    hc->fullscreen = false;
    hc->floating   = false;
    return hc;
}

/* list of names of all _NET-atoms */
char* g_netatom_names[NetCOUNT] = {
    [ NetSupported                ] = "_NET_SUPPORTED"                  ,
    [ NetWmName                   ] = "_NET_WM_NAME"                    ,
    [ NetWmWindowType             ] = "_NET_WM_WINDOW_TYPE"             ,
    [ NetWmState                  ] = "_NET_WM_STATE"                   ,
    [ NetWmStateFullscreen        ] = "_NET_WM_STATE_FULLSCREEN"        ,
    [ NetWmWindowTypeDesktop      ] = "_NET_WM_WINDOW_TYPE_DESKTOP"     ,
    [ NetWmWindowTypeDock         ] = "_NET_WM_WINDOW_TYPE_DOCK"        ,
    [ NetWmWindowTypeSplash       ] = "_NET_WM_WINDOW_TYPE_SPLASH"      ,
    [ NetWmWindowTypeDialog       ] = "_NET_WM_WINDOW_TYPE_DIALOG"      ,
    [ NetWmWindowTypeNotification ] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    [ NetWmWindowTypeNormal       ] = "_NET_WM_WINDOW_TYPE_NORMAL"      ,
};

//------
void ewmh_handle_client_message(XEvent* event) {
   XClientMessageEvent* me = &(event->xclient);
   int index;
   for (index = 0; index < NetCOUNT; index++) {
      if (me->message_type == g_netatom[index])
         break;
   }
   if (index >= NetCOUNT) return;

   HSClient* client = get_client_from_window(me->window);
   if (!client) return;

   if(index == NetWmState){
      /* me->data.l[1] and [2] describe the properties to alter */
      for (int prop = 1; prop <= 2; prop++) {
         if (me->data.l[prop] == 0) continue;
         if (!(g_netatom[NetWmStateFullscreen] == me->data.l[prop])) continue;

         bool new_value[] = {
            [ _NET_WM_STATE_REMOVE ] = false,
            [ _NET_WM_STATE_ADD    ] = true,
            [ _NET_WM_STATE_TOGGLE ] = !client->fullscreen,
         };
         int action = me->data.l[0];

         client_set_fullscreen(client, new_value[action]);
      }
   }
}

void clientlist_init() {
    g_wmatom[WMProtocols] = XInternAtom(g_display, "WM_PROTOCOLS", False);
    g_wmatom[WMDelete] = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    g_wmatom[WMState] = XInternAtom(g_display, "WM_STATE", False);
    // init actual client list
    g_clients = g_array_new(false, false, sizeof(HSClient*));

    //init colors
    for(int i=0; i<NUMCOLORS; i++){
       wincolors[i][ColFrameBorder] = getcolor(colors[i][ColFrameBorder]);
       wincolors[i][ColWindowBorder] = getcolor(colors[i][ColWindowBorder]);
       wincolors[i][ColFG] = getcolor(colors[i][ColFG]);
       wincolors[i][ColBG] = getcolor(colors[i][ColBG]);
    }

    /* init ewmh net atoms */
    for (int i = 0; i < NetCOUNT; i++) 
        g_netatom[i] = ATOM(g_netatom_names[i]);

    /* tell which ewmh atoms are supported */
    XChangeProperty(g_display, g_root, g_netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) g_netatom, NetCOUNT);
}

void clientlist_destroy() {
   g_array_free(g_clients, true);
}

HSClient* get_client_from_window(Window window) {
   for(int i=0; i<g_clients->len; i++){
      HSClient* client = g_array_index(g_clients, HSClient*, i);
      if(client->window == window) return client;
   }
   return NULL;
}

static void window_grab_button(Window win){
   XGrabButton(g_display, AnyButton, 0, win, False, ButtonPressMask,
      GrabModeSync, GrabModeSync, None, None);
}

HSClient* manage_client(Window win) {
   if (get_client_from_window(win)) return NULL;

   // init client
   HSClient* client = create_client();
   HSMonitor* m = get_current_monitor();
   // set to window properties
   client->window = win;
   client_update_title(client);

   // apply rules
   int manage = 1;
   rules_apply(client, &manage);
   if (!manage) {
      destroy_client(client);
      // map it... just to be sure
      XMapWindow(g_display, win);
      return NULL;
   }

   unsigned int border, depth;
   Window root_win;
   int x, y;
   unsigned int w, h;
   XGetGeometry(g_display, win, &root_win, &x, &y, &w, &h, &border, &depth);
   // treat wanted coordinates as floating coords
   XRectangle size = client->float_size;
   size.width = w;
   size.height = h;
   size.x = m->rect.x + m->rect.width/2 - size.width/2;
   size.y = m->rect.y + m->rect.height/2 - size.height/2 + bh;
   client->float_size = size;
   client->last_size = size;
   XMoveResizeWindow(g_display, client->window, size.x, size.y, size.width, size.height);

   // actually manage it
   g_array_append_val(g_clients, client);
   XSetWindowBorderWidth(g_display, win, window_border_width);
   // insert to layout
   if (!client->tag)   client->tag = m->tag;

   // get events from window
   client_update_wm_hints(client);
   XSelectInput(g_display, win, CLIENT_EVENT_MASK);
   window_grab_button(win);
   frame_insert_window(client->tag->frame, win);
   monitor_apply_layout(find_monitor_with_tag(client->tag));

   return client;
}

void unmanage_client(Window win) {
   HSClient* client = get_client_from_window(win);
   if (!client)   return;

   // remove from tag
   frame_remove_window(client->tag->frame, win);
   // and arrange monitor
   HSMonitor* m = find_monitor_with_tag(client->tag);
   if (m) monitor_apply_layout(m);
   // ignore events from it
   XSelectInput(g_display, win, 0);
   XUngrabButton(g_display, AnyButton, AnyModifier, win);
   // permanently remove it
   for(int i=0; i<g_clients->len; i++){
      if(g_array_index(g_clients, HSClient*, i) == client)
         g_array_remove_index(g_clients, i);
   }
}

// destroys a special client
void destroy_client(HSClient* client) {
   g_free(client);
}

void window_unfocus(Window window) {
    // grab buttons in old window again
    XSetWindowBorder(g_display, window, wincolors[0][ColWindowBorder]);
    window_grab_button(window);
}

static Window lastfocus = 0;
void window_unfocus_last() {
    if (lastfocus)    window_unfocus(lastfocus);
    lastfocus = 0;

    // give focus to root window
    XSetInputFocus(g_display, g_root, RevertToPointerRoot, CurrentTime);
}

void window_focus(Window window) {
    window_unfocus(lastfocus);
    XSetWindowBorder(g_display, window, wincolors[1][ColWindowBorder]);
    if(!get_client_from_window(window)->neverfocus)
      XSetInputFocus(g_display, window, RevertToPointerRoot, CurrentTime);

    lastfocus = window;
}

void client_setup_border(HSClient* client, bool focused) {
    XSetWindowBorder(g_display, client->window, wincolors[focused ? 1:0][ColWindowBorder]);
}

void client_resize(HSClient* client, XRectangle rect) {
    HSMonitor* m;
    if (client->fullscreen && (m = find_monitor_with_tag(client->tag))) {
        client_resize_fullscreen(client, m);
    } else if (client->floating && (m = find_monitor_with_tag(client->tag))) {
         client_resize_floating(client, m);
    } else {
       // ensure minimum size
       if (rect.width < WINDOW_MIN_WIDTH)    rect.width = WINDOW_MIN_WIDTH;
       if (rect.height < WINDOW_MIN_HEIGHT)  rect.height = WINDOW_MIN_HEIGHT;
       if (!client) return;

       Window win = client->window;
       if (client) {
          if (RECTANGLE_EQUALS(client->last_size, rect)) return;
          client->last_size = rect;
       }
       // apply border width
       rect.width -= window_border_width * 2;
       rect.height -= window_border_width * 2;
       XSetWindowBorderWidth(g_display, win, window_border_width);
       XMoveResizeWindow(g_display, win, rect.x, rect.y, rect.width, rect.height);
    }
}

void client_resize_fullscreen(HSClient* client, HSMonitor* m) {
    if (!client || !m) return;
    
    XSetWindowBorderWidth(g_display, client->window, 0);
    client->last_size = m->rect;
    XMoveResizeWindow(g_display, client->window,
                      m->rect.x, m->rect.y, m->rect.width, m->rect.height);
}

void client_resize_floating(HSClient* client, HSMonitor* m) {
    if (!client || !m) return;
    if (client->fullscreen) {
        client_resize_fullscreen(client, m);
        return;
    }

    // ensure minimal size
    if (client->float_size.width < WINDOW_MIN_WIDTH)
        client->float_size.width = WINDOW_MIN_WIDTH;
    if (client->float_size.height < WINDOW_MIN_HEIGHT)
        client->float_size.height = WINDOW_MIN_HEIGHT;
    client->last_size = client->float_size;
    
    XRectangle rect = client->last_size;
    XSetWindowBorderWidth(g_display, client->window, window_border_width);
    XMoveResizeWindow(g_display, client->window,
        rect.x, rect.y, rect.width, rect.height);
}

void client_center(HSClient* client, HSMonitor *m) {
   if(!client || !m) return;
   if(client->fullscreen) {
      client_resize_fullscreen(client, m);
      return;
   }

   XRectangle size = client->float_size;
   size.x = m->rect.x + m->rect.width/2 - client->float_size.width/2;
   size.y = m->rect.y + m->rect.height/2 - client->float_size.height/2 + bh;
   client->float_size = size;
   client->last_size = size;
   XSetWindowBorderWidth(g_display, client->window, window_border_width);
   XMoveResizeWindow(g_display, client->window, size.x, size.y, size.width, size.height);
}

XRectangle client_outer_floating_rect(HSClient* client) {
    XRectangle rect = client->float_size;
    rect.width  += window_border_width * 2;
    rect.height += window_border_width * 2;
    return rect;
}

void client_close(const Arg *arg) {
    XEvent ev;
    // if there is no focus, then there is nothing to do
    if (!g_cur_frame) return;
    Window win = frame_focused_window(g_cur_frame);
    if (!win) return;
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = g_wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = g_wmatom[WMDelete];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(g_display, win, False, NoEventMask, &ev);
}

void window_set_visible(Window win, bool visible) {
    XGrabServer(g_display);
    XSelectInput(g_display, win, CLIENT_EVENT_MASK & ~StructureNotifyMask);
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK & ~SubstructureNotifyMask);
    if(visible)   XMapWindow(g_display, win);
    else          XUnmapWindow(g_display, win);
    XSelectInput(g_display, win, CLIENT_EVENT_MASK);
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK);
    XUngrabServer(g_display);
}

void client_update_wm_hints(HSClient* client) {
    XWMHints* wmh = XGetWMHints(g_display, client->window);
    if (!wmh)   return;

    if ((frame_focused_window(g_cur_frame) == client->window)
        && wmh->flags & XUrgencyHint) {
        // remove urgency hint if window is focused
        wmh->flags &= ~XUrgencyHint;
        XSetWMHints(g_display, client->window, wmh);
    } else {
        bool newval = (wmh->flags & XUrgencyHint) ? true : false;
        if (newval != client->urgent) {
            client->urgent = newval;
            client->tag->urgent = client->urgent;
        }
    }
    if(wmh->flags & InputHint)  client->neverfocus = !wmh->input;
    else                        client->neverfocus = false;
    XFree(wmh);
}

void client_update_title(HSClient* client) {
    gettextprop(client->window, g_netatom[NetWmName], client->title, sizeof(client->title));
    if(client->title[0] == '\0')
       strcpy(client->title, "broken");
}

void client_set_fullscreen(HSClient* client, bool state) {
    if (client->fullscreen == state)   return;

    client->fullscreen = state;
    if (state) {
       XChangeProperty(g_display, client->window, g_netatom[NetWmState], XA_ATOM,
             32, PropModeReplace, (unsigned char *)&g_netatom[NetWmStateFullscreen], 1);

        XRaiseWindow(g_display, client->window);
    } else {
       XChangeProperty(g_display, client->window, g_netatom[NetWmState], XA_ATOM,
             32, PropModeReplace, (unsigned char *)0, 0);
    }

    monitor_apply_layout(find_monitor_with_tag(client->tag));
}

void client_set_floating(HSClient* client, bool state) {
    client->floating = state;
    HSFrame* frame = get_current_monitor()->tag->frame;
    frame = frame_descend(frame);
    size_t floatcount = frame->content.clients.floatcount;
    floatcount += state ? 1 : -1;
    frame->content.clients.floatcount = floatcount;
    if(state)
      client_center(client, find_monitor_with_tag(client->tag));

    monitor_apply_layout(find_monitor_with_tag(client->tag));
}

void set_floating(const Arg *arg){
   Window win = frame_focused_window(g_cur_frame);
   if (!win) return;
   HSClient *client = get_client_from_window(win);
   if (!client) return;

   client_set_floating(client, !client->floating);
}

// rules applying //
void rules_apply(HSClient* client, int *manage) {
   int di;
   unsigned long dl;
   unsigned char *buf =  NULL;
   Atom da, wintype = None;
   int status = XGetWindowProperty( g_display, client->window, g_netatom[NetWmWindowType],
         0L, sizeof wintype, False, XA_ATOM, &da, &di, &dl, &dl, &buf );
   
   if(status == Success && buf) {
      wintype= *(Atom *)buf;
      XFree(buf);
   } else {
      return;
   }
   
   if(wintype == g_netatom[NetWmWindowTypeDialog] || 
      wintype == g_netatom[NetWmWindowTypeSplash]){
      client->tag = find_tag(tags[-1]);
      client->floating = 1;
      return;
   } else if(wintype == g_netatom[NetWmWindowTypeNotification] ||
             wintype == g_netatom[NetWmWindowTypeDock]){
      *manage = 0;
      return;
   }

   XClassHint hint;
   XGetClassHint(g_display, client->window, &hint);
   char* class_str = hint.res_class ? hint.res_class : "broken";
   for(int i=0; i<LENGTH(custom_rules); i++){
      if(!strcmp(custom_rules[i].class_str, class_str)){
         client->tag = find_tag(tags[custom_rules[i].tag]);
         client->floating = custom_rules[i].floating;
      }
   }
   if(hint.res_name)   XFree(hint.res_name);
   if(hint.res_class)  XFree(hint.res_class);
}

