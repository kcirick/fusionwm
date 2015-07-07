/*
 * FusionWM Main Code - main.c 
 */

#include "clientlist.h"
#include "inputs.h"
#include "layout.h"
#include "globals.h"
#include "config.h"
#include "systray.h"

#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

static int (*g_xerrorxlib)(Display *, XErrorEvent *);

// handler for X-Events
void configurerequest(XEvent* event);
void configurenotify(XEvent* event);
void destroynotify(XEvent* event);
void enternotify(XEvent* event);
void mappingnotify(XEvent* event);
void mapnotify(XEvent *event);
void maprequest(XEvent* event);
void propertynotify(XEvent* event);
void resizerequest(XEvent* event);
void unmapnotify(XEvent* event);
void expose(XEvent* event);

/* There's no way to check accesses to destroyed windows, thus those 
 * cases are ignored (especially on UnmapNotify's).  Other types of 
 * errors call Xlibs default error handler, which may call exit.  */
int xerror(Display *dpy, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    say("FATAL", "Request code=%d, error code=%d",
            ee->request_code, ee->error_code);
    if (ee->error_code == BadDrawable)
        return 0;
    
    return g_xerrorxlib(dpy, ee); // may call exit
}

/* Startup Error handler to check if another window manager is already 
 * running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
   die("ERROR", "Another window manager is already running"); 
   return -1;
}

void checkotherwm(void) {
    g_xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(gDisplay, DefaultRootWindow(gDisplay), SubstructureRedirectMask);
    XSync(gDisplay, False);
    XSetErrorHandler(xerror);
    XSync(gDisplay, False);
}

// scan for windows and add them to the list of managed clients
void scan(void) {
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if(XQueryTree(gDisplay, gRoot, &d1, &d2, &wins, &num)) {
        for(i = 0; i < num; i++) {
            if(!XGetWindowAttributes(gDisplay, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(gDisplay, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable)
                manage_client(wins[i]);
        }
        for(i = 0; i < num; i++){ // now the transients
           if(!XGetWindowAttributes(gDisplay, wins[i], &wa))
              continue;
           if(XGetTransientForHint(gDisplay, wins[i], &d1)
              && (wa.map_state == IsViewable))
              manage_client(wins[i]);
        }
        if(wins) XFree(wins);
    }
}

void sigchld(int unused){
   if(signal(SIGCHLD, sigchld) == SIG_ERR)
      die("ERROR", "Can't install SIGCHLD handler");
   while(0 < waitpid(-1, NULL, WNOHANG));
}

static void (*handler[LASTEvent]) (XEvent *) = {
   [ ButtonPress      ] = buttonpress,
   [ ClientMessage    ] = ewmh_handle_client_message,
   [ ConfigureRequest ] = configurerequest,
   [ ConfigureNotify  ] = configurenotify,
   [ DestroyNotify    ] = destroynotify,
   [ EnterNotify      ] = enternotify,
   [ Expose           ] = expose,
   [ KeyPress         ] = keypress,
   [ MappingNotify    ] = mappingnotify,
   [ MapNotify        ] = mapnotify,
   [ MapRequest       ] = maprequest,
   [ PropertyNotify   ] = propertynotify,
   [ ResizeRequest    ] = resizerequest,
   [ UnmapNotify      ] = unmapnotify
};

// event handler implementations 
void configurerequest(XEvent* event) {
   XConfigureRequestEvent* cre = &event->xconfigurerequest;
   Client* client = get_client_from_window(cre->window);
   if (client) {
      XConfigureEvent ce;
      ce.type = ConfigureNotify;
      ce.display = gDisplay;
      ce.event = cre->window;
      ce.window = cre->window;
      ce.x = client->last_size.x;
      ce.y = client->last_size.y;
      ce.width = client->last_size.width;
      ce.height = client->last_size.height;
      ce.override_redirect = False;
      ce.border_width = cre->border_width;
      ce.above = cre->above;
      XSendEvent(gDisplay, cre->window, False, StructureNotifyMask, (XEvent*)&ce);
   } else {
      // if client not known.. then allow configure.
      // its probably a nice conky or dzen2 bar :)
      XWindowChanges wc;
      wc.x = cre->x;
      wc.y = cre->y;
      wc.width = cre->width;
      wc.height = cre->height;
      wc.border_width = cre->border_width;
      wc.sibling = cre->above;
      wc.stack_mode = cre->detail;
      XConfigureWindow(gDisplay, cre->window, cre->value_mask, &wc);
   }
}

void configurenotify(XEvent* event){
   XConfigureEvent *ev = &event->xconfigure;
   Monitor *m = get_current_monitor();

   if(ev->window == gRoot){
      update_monitors();
      if(m->rect.width != ev->width){
         if(dc.drawable != 0)
            XFreePixmap(gDisplay, dc.drawable);
         dc.drawable = XCreatePixmap(gDisplay, gRoot, ev->width, bar_height, DefaultDepth(gDisplay, gScreen));
      }
      //update_bar(m);
      //draw_bars();
   }
}

void destroynotify(XEvent* event) {
    unmanage_client(event->xdestroywindow.window);
}

void enternotify(XEvent* event) {
    if (focus_follows_mouse && !event->xcrossing.focus) 
        focus_window(event->xcrossing.window, false, true); // sloppy focus
}

void expose(XEvent* event){
   XExposeEvent *ev = &event->xexpose;
   if(ev->count == 0) draw_bar(get_current_monitor());
}

void mappingnotify(XEvent* event) {
   // regrab when keyboard map changes
   XMappingEvent *ev = &event->xmapping;
   XRefreshKeyboardMapping(ev);
   if(ev->request == MappingKeyboard) grab_keys();
}

void mapnotify(XEvent* event) {
   Client* c;
   if ((c = get_client_from_window(event->xmap.window))) {
      // reset focus. so a new window gets the focus if it shall have the input focus
      frame_focus_recursive(g_cur_frame);
      // also update the window title - just to be sure
      client_update_title(c);
   }
}

void maprequest(XEvent* event) {
    XMapRequestEvent* mapreq = &event->xmaprequest;
    Client *c;

    if((c = wintosystrayicon(mapreq->window))) {
       resizebarwin(get_current_monitor());
       updatesystray();
    }

    if (!get_client_from_window(mapreq->window)) {
        // client should be managed (is not ignored but is not managed yet
        Client* client = manage_client(mapreq->window);
        if (client && find_monitor_with_tag(client->tag)) 
            XMapWindow(gDisplay, mapreq->window);
    } 
}

void propertynotify(XEvent* event) {
    XPropertyEvent *ev = &event->xproperty;
    Client* client;

    if((client = wintosystrayicon(ev->window))) {
       if(ev->atom == XA_WM_NORMAL_HINTS)
          updatesystrayicongeom(client, client->last_size.width, client->last_size.height);

       resizebarwin(get_current_monitor());
       updatesystray();
    }

    if((ev->window == gRoot) && (ev->atom == XA_WM_NAME))  update_status();

    if (ev->state == PropertyNewValue &&
          (client = get_client_from_window(ev->window))) {
       if (ev->atom == XA_WM_HINTS)       client_update_wm_hints(client);
       else if (ev->atom == XA_WM_NAME)   client_update_title(client);
    }
    draw_bars();
}

void resizerequest(XEvent *event) {
   XResizeRequestEvent *ev = &event->xresizerequest;
   Client *client;

   if((client = wintosystrayicon(ev->window))) {
      updatesystrayicongeom(client, ev->width, ev->height);
      resizebarwin(get_current_monitor());
      updatesystray();
   }
}

void unmapnotify(XEvent* event) {
    unmanage_client(event->xunmap.window);
    
    Client* client;
    XUnmapEvent *ev = &event->xunmap;

    if((client = wintosystrayicon(ev->window))) {
		removesystrayicon(client);
		resizebarwin(get_current_monitor());
		updatesystray();
	}
}

void setup(){
   // remove zombies on SIGCHLD
   sigchld(0);

   gScreen = DefaultScreen(gDisplay);
   gRoot = RootWindow(gDisplay, gScreen);
   XSelectInput(gDisplay, gRoot, ROOT_EVENT_MASK);

   // initialize subsystems
   inputs_init();
   clientlist_init();
   layout_init();
	systray_init();
   update_status();
}

void cleanup() {
   inputs_destroy();
   clientlist_destroy();
   layout_destroy();
   systray_destroy();
}

// --- Main Function ------------------------------------------------------
int main(int argc, char* argv[]) {

   if(argc == 2 && !strcmp("-v", argv[1]))
      die("INFO", "FusionWM-"VERSION);
   else if (argc != 1)
      die("INFO", "Usage: fusionwm [-v]");

   if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
      say("WARNING", "No locale support");

   if(!(gDisplay = XOpenDisplay(NULL)))
      die("ERROR", "Cannot open display");
   checkotherwm();

   // Setup
   setup();
   scan();
   all_monitors_apply_layout();

   // Main loop
   XEvent event;
   XSync(gDisplay, False);
   while (!gAboutToQuit && !XNextEvent(gDisplay, &event))
      if(handler[event.type])
         handler[event.type](&event); // call handler

   // Cleanup
   cleanup();
   XCloseDisplay(gDisplay);

   return EXIT_SUCCESS;
}

