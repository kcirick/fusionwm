/* 
 * FusionWM - clientlist.c
 */

#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "inputs.h"
#include "config.h"
#include "systray.h"

unsigned long wincolors[NCOLORS][ColLast];

static Client* create_client() {
    Client* hc   = g_new0(Client, 1);
    hc->urgent     = false;
    hc->fullscreen = false;
    hc->floating   = false;
    return hc;
}

/* list of names of all _NET-atoms */
char* wmatom_names[WMCOUNT] = {
   [ WMProtocols     ] = "WM_PROTOCOLS"      ,
   [ WMDelete        ] = "WM_DELETE_WINDOW"  ,
   [ WMState         ] = "WM_STATE"          ,
   [ WMTakeFocus     ] = "WM_TAKE_FOCUS"     ,
};
char* netatom_names[NetCOUNT] = {
   [ NetSupported                ] = "_NET_SUPPORTED"                  ,
   [ NetSystemTray               ] = "_NET_SYSTEM_TRAY_S0"             ,
   [ NetSystemTrayOP             ] = "_NET_SYSTEM_TRAY_OPCODE"         ,
   [ NetSystemTrayOrientation    ] = "_NET_SYSTEM_TRAY_ORIENTATION"    ,
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
char* xatom_names[XCOUNT] = {
   [ Manager      ] = "MANAGER"     ,
   [ Xembed       ] = "_XEMBED"     ,
   [ XembedInfo   ] = "_XEMBED_INFO",
};

//------
void ewmh_handle_client_message(XEvent* event) {
   XWindowAttributes wa;
	XSetWindowAttributes swa;
   XClientMessageEvent* me = &(event->xclient);
   Client* client;

   if(systray_visible && 
         me->window == gSystray->window && 
         me->message_type == g_netatom[NetSystemTrayOP]) {
      // add systray icons
      if(me->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
         if(!(client = (Client *)calloc(1, sizeof(Client))))
            die("FATAL", "Could not malloc() %u bytes", sizeof(Client));

         client->window = me->data.l[2];
         client->next = gSystray->icon;
         gSystray->icon = client;
         XGetWindowAttributes(gDisplay, client->window, &wa);
         client->last_size.x = client->last_size.y = 0;
         client->last_size.width = wa.width;
         client->last_size.height = wa.height;
         client->float_size = client->last_size;
         client->floating = True;
         /* reuse tags field as mapped status */
         client->tag = g_array_index(g_tags, Tag*, 1);
         updatesystrayicongeom(client, wa.width, wa.height);
         XAddToSaveSet(gDisplay, client->window);
         XSelectInput(gDisplay, client->window, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
         XReparentWindow(gDisplay, client->window, gSystray->window, 0, 0);
         /* use parents background pixmap */
         swa.background_pixmap = ParentRelative;
         swa.background_pixel  = dc.colors[0][ColBG];
         XChangeWindowAttributes(gDisplay, client->window, CWBackPixmap|CWBackPixel, &swa);
         updatesystray();
         resizebarwin(get_current_monitor());
         //update_bars();
         //setclientstate(c, NormalState);
         long data[] = {NormalState, None};
         XChangeProperty(gDisplay, client->window, g_wmatom[WMState], g_wmatom[WMState], 32, PropModeReplace, (unsigned char*)data, 2);
      }
      return;
   }

   client = get_client_from_window(me->window);
   if (!client) return;

   if(me->message_type == g_netatom[NetWmState]){
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

   // init atoms
   for (int i = 0; i < WMCOUNT; i++) 
      g_wmatom[i] = ATOM(wmatom_names[i]);
   for (int i = 0; i < NetCOUNT; i++) 
      g_netatom[i] = ATOM(netatom_names[i]);
   for (int i = 0; i < XCOUNT; i++) 
      g_xatom[i] = ATOM(xatom_names[i]);

   // init actual client list
   g_clients = g_array_new(false, false, sizeof(Client*));

   //init colors
   for(int i=0; i<NCOLORS; i++){
      wincolors[i][ColFrameBorder] = getcolor(colors[i][ColFrameBorder]);
      wincolors[i][ColWindowBorder] = getcolor(colors[i][ColWindowBorder]);
      wincolors[i][ColFG] = getcolor(colors[i][ColFG]);
      wincolors[i][ColBG] = getcolor(colors[i][ColBG]);
   }

   /* tell which ewmh atoms are supported */
   XChangeProperty(gDisplay, gRoot, g_netatom[NetSupported], XA_ATOM, 32,
         PropModeReplace, (unsigned char *) g_netatom, NetCOUNT);
}

void clientlist_destroy() {
   g_array_free(g_clients, true);
}

Client* get_client_from_window(Window window) {
   for(int i=0; i<g_clients->len; i++){
      Client* client = g_array_index(g_clients, Client*, i);
      if(client->window == window) return client;
   }
   return NULL;
}

static void window_grab_button(Window win){
   XGrabButton(gDisplay, AnyButton, 0, win, False, ButtonPressMask,
      GrabModeSync, GrabModeSync, None, None);
}

Client* manage_client(Window win) {
   if (get_client_from_window(win)) return NULL;

   // init client
   Client* client = create_client();
   Monitor* m = get_current_monitor();
   // set to window properties
   client->window = win;
   client_update_title(client);

   // apply rules
   int manage = 1;
   rules_apply(client, &manage);
   if (!manage) {
      destroy_client(client);
      // map it... just to be sure
      XMapWindow(gDisplay, win);
      return NULL;
   }

   unsigned int border, depth;
   Window root_win;
   int x, y;
   unsigned int w, h;
   XGetGeometry(gDisplay, win, &root_win, &x, &y, &w, &h, &border, &depth);
   // treat wanted coordinates as floating coords
   XRectangle size = client->float_size;
   size.width = w;
   size.height = h;
   size.x = m->rect.x + m->rect.width/2 - size.width/2;
   size.y = m->rect.y + m->rect.height/2 - size.height/2 + bar_height;
   client->float_size = size;
   client->last_size = size;
   XMoveResizeWindow(gDisplay, client->window, size.x, size.y, size.width, size.height);

   // actually manage it
   g_array_append_val(g_clients, client);
   XSetWindowBorderWidth(gDisplay, win, window_border_width);
   // insert to layout
   if (!client->tag)   client->tag = m->tag;

   // get events from window
   client_update_wm_hints(client);
   XSelectInput(gDisplay, win, CLIENT_EVENT_MASK);
   window_grab_button(win);
   frame_insert_window(client->tag->frame, win);
   monitor_apply_layout(find_monitor_with_tag(client->tag));

   return client;
}

void unmanage_client(Window win) {
   Client* client = get_client_from_window(win);
   if (!client)   return;

   // remove from tag
   frame_remove_window(client->tag->frame, win);
   // and arrange monitor
   Monitor* m = find_monitor_with_tag(client->tag);
   if (m) monitor_apply_layout(m);
   // ignore events from it
   XSelectInput(gDisplay, win, 0);
   XUngrabButton(gDisplay, AnyButton, AnyModifier, win);
   // permanently remove it
   for(int i=0; i<g_clients->len; i++){
      if(g_array_index(g_clients, Client*, i) == client)
         g_array_remove_index(g_clients, i);
   }
}

// destroys a special client
void destroy_client(Client* client) {
   g_free(client);
}

void window_unfocus(Window window) {
    // grab buttons in old window again
    XSetWindowBorder(gDisplay, window, wincolors[0][ColWindowBorder]);
    window_grab_button(window);
}

static Window lastfocus = 0;
void window_unfocus_last() {
    if (lastfocus)    window_unfocus(lastfocus);
    lastfocus = 0;

    // give focus to root window
    XSetInputFocus(gDisplay, gRoot, RevertToPointerRoot, CurrentTime);
}

void window_focus(Window window) {
    window_unfocus(lastfocus);
    XSetWindowBorder(gDisplay, window, wincolors[1][ColWindowBorder]);
    if(!get_client_from_window(window)->neverfocus)
      XSetInputFocus(gDisplay, window, RevertToPointerRoot, CurrentTime);

    lastfocus = window;
}

void client_setup_border(Client* client, bool focused) {
    XSetWindowBorder(gDisplay, client->window, wincolors[focused ? 1:0][ColWindowBorder]);
}

void client_resize(Client* client, XRectangle rect) {
    Monitor* m;
    if (client->fullscreen && (m = find_monitor_with_tag(client->tag))) {
        XSetWindowBorderWidth(gDisplay, client->window, 0);
        client->last_size = m->rect;
        XMoveResizeWindow(gDisplay, client->window,
              m->rect.x, m->rect.y, m->rect.width, m->rect.height);
    } else if (!client->floating) {
       // apply border width
       rect.width  -= (window_border_width*2);
       rect.height -= (window_border_width*2);
       // ensure minimum size
       if (rect.width < WIN_MIN_WIDTH)    rect.width = WIN_MIN_WIDTH;
       if (rect.height < WIN_MIN_HEIGHT)  rect.height = WIN_MIN_HEIGHT;
       if (!client) return;

       Window win = client->window;
       if (client) {
          if (RECTANGLE_EQUALS(client->last_size, rect)) return;
          client->last_size = rect;
       }
       XSetWindowBorderWidth(gDisplay, win, window_border_width);
       XMoveResizeWindow(gDisplay, win, rect.x, rect.y, rect.width, rect.height);
    }
}

void client_center(Client* client, Monitor *m) {
   if(!client || !m) return;
   if(client->fullscreen) return;

   XRectangle size = client->float_size;
   size.x = m->rect.x + m->rect.width/2 - client->float_size.width/2;
   size.y = m->rect.y + m->rect.height/2 - client->float_size.height/2 + bar_height;
   client->float_size = size;
   client->last_size = size;
   XSetWindowBorderWidth(gDisplay, client->window, window_border_width);
   XMoveResizeWindow(gDisplay, client->window, size.x, size.y, size.width, size.height);
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
    XSendEvent(gDisplay, win, False, NoEventMask, &ev);
}

void window_set_visible(Window win, bool visible) {
    XGrabServer(gDisplay);
    XSelectInput(gDisplay, win, CLIENT_EVENT_MASK & ~StructureNotifyMask);
    XSelectInput(gDisplay, gRoot, ROOT_EVENT_MASK & ~SubstructureNotifyMask);
    if(visible)   XMapWindow(gDisplay, win);
    else          XUnmapWindow(gDisplay, win);
    XSelectInput(gDisplay, win, CLIENT_EVENT_MASK);
    XSelectInput(gDisplay, gRoot, ROOT_EVENT_MASK);
    XUngrabServer(gDisplay);
}

void client_update_wm_hints(Client* client) {
    XWMHints* wmh = XGetWMHints(gDisplay, client->window);
    if (!wmh)   return;

    if ((frame_focused_window(g_cur_frame) == client->window)
        && wmh->flags & XUrgencyHint) {
        // remove urgency hint if window is focused
        wmh->flags &= ~XUrgencyHint;
        XSetWMHints(gDisplay, client->window, wmh);
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

void client_update_title(Client* client) {
    gettextprop(client->window, g_netatom[NetWmName], client->title, sizeof(client->title));
    if(client->title[0] == '\0')
       strcpy(client->title, "broken");
}

void client_set_fullscreen(Client* client, bool state) {
    if (client->fullscreen == state)   return;

    client->fullscreen = state;
    if (state) {
       XChangeProperty(gDisplay, client->window, g_netatom[NetWmState], XA_ATOM,
             32, PropModeReplace, (unsigned char *)&g_netatom[NetWmStateFullscreen], 1);

        XRaiseWindow(gDisplay, client->window);
    } else {
       XChangeProperty(gDisplay, client->window, g_netatom[NetWmState], XA_ATOM,
             32, PropModeReplace, (unsigned char *)0, 0);
    }

    monitor_apply_layout(find_monitor_with_tag(client->tag));
}

void client_set_floating(Client* client, bool state) {
    client->floating = state;
    Frame* frame = get_current_monitor()->tag->frame;
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
   Client *client = get_client_from_window(win);
   if (!client) return;

   client_set_floating(client, !client->floating);
}

// rules applying //
void rules_apply(Client* client, int *manage) {
   int di;
   unsigned long dl;
   unsigned char *buf =  NULL;
   Atom da, wintype = None;
   int status = XGetWindowProperty( gDisplay, client->window, g_netatom[NetWmWindowType],
         0L, sizeof wintype, False, XA_ATOM, &da, &di, &dl, &dl, &buf );
   
   if(status == Success && buf) {
      wintype= *(Atom *)buf;
      XFree(buf);
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
   XGetClassHint(gDisplay, client->window, &hint);
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

