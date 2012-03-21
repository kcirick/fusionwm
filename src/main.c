/*
 * FusionWM Main Code - main.c 
 */

#include "clientlist.h"
#include "inputs.h"
#include "layout.h"
#include "globals.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <assert.h>

static int (*g_xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;

// handler for X-Events
void buttonpress(XEvent* event);
void buttonrelease(XEvent* event);
void configurerequest(XEvent* event);
void configurenotify(XEvent* event);
void clientmessage(XEvent* event);
void destroynotify(XEvent* event);
void enternotify(XEvent* event);
void keypress(XEvent* event);
void mappingnotify(XEvent* event);
void motionnotify(XEvent* event);
void mapnotify(XEvent *event);
void maprequest(XEvent* event);
void propertynotify(XEvent* event);
void unmapnotify(XEvent* event);
void expose(XEvent* event);

enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast };             /* clicks */

// handle x-events:
void event_on_configure(XEvent event) {
    XConfigureRequestEvent* cre = &event.xconfigurerequest;
    HSClient* client = get_client_from_window(cre->window);
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = g_display;
    ce.event = cre->window;
    ce.window = cre->window;
    if (client) {
        ce.x = client->last_size.x;
        ce.y = client->last_size.y;
        ce.width = client->last_size.width;
        ce.height = client->last_size.height;
        ce.override_redirect = False;
        ce.border_width = cre->border_width;
        ce.above = cre->above;
        // FIXME: why send event and not XConfigureWindow or XMoveResizeWindow??
        XSendEvent(g_display, cre->window, False, StructureNotifyMask, (XEvent*)&ce);
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
        XConfigureWindow(g_display, cre->window, cre->value_mask, &wc);
    }
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
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
    fprintf(stderr, "fusionwm: fatal error: request code=%d, error code=%d\n",
            ee->request_code, ee->error_code);
    if (ee->error_code == BadDrawable)
        return 0;
    
    return g_xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) {
   return 0;
}

/* Startup Error handler to check if another window manager is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
   die("fusionwm: abother window manager is already running\n"); 
   return -1;
}

void checkotherwm(void) {
    g_xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(g_display, DefaultRootWindow(g_display), SubstructureRedirectMask);
    XSync(g_display, False);
    XSetErrorHandler(xerror);
    XSync(g_display, False);
}

// scan for windows and add them to the list of managed clients
void scan(void) {
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if(XQueryTree(g_display, g_root, &d1, &d2, &wins, &num)) {
        for(i = 0; i < num; i++) {
            if(!XGetWindowAttributes(g_display, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(g_display, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable)
                manage_client(wins[i]);
        }
        for(i = 0; i < num; i++){ // now the transients
           if(!XGetWindowAttributes(g_display, wins[i], &wa))
              continue;
           if(XGetTransientForHint(g_display, wins[i], &d1)
              && (wa.map_state == IsViewable))
              manage_client(wins[i]);
        }
        if(wins) XFree(wins);
    }
}

void sigchld(int unused){
   if(signal(SIGCHLD, sigchld) == SIG_ERR)
      die("Can't install SIGCHLD handler");
   while(0 < waitpid(-1, NULL, WNOHANG));
}

static void (*handler[LASTEvent]) (XEvent *) = {
   [ ButtonPress       ] = buttonpress,
   [ ButtonRelease     ] = buttonrelease,
   [ ClientMessage     ] = ewmh_handle_client_message,
   [ ConfigureRequest  ] = configurerequest,
   [ ConfigureNotify   ] = configurenotify,
   [ DestroyNotify     ] = destroynotify,
   [ EnterNotify       ] = enternotify,
   [ Expose            ] = expose,
   [ KeyPress          ] = keypress,
   [ MappingNotify     ] = mappingnotify,
   [ MotionNotify      ] = motionnotify,
   [ MapNotify         ] = mapnotify,
   [ MapRequest        ] = maprequest,
   [ PropertyNotify    ] = propertynotify,
   [ UnmapNotify       ] = unmapnotify
};

// event handler implementations 
void buttonpress(XEvent* event) {
   XButtonEvent* be = &(event->xbutton);
   unsigned int i, x, click;
   Arg arg = {0};
   HSMonitor *m;

   // focus monitor if necessary
   if((m = wintomon(be->window)) && m != get_current_monitor()) {
      monitor_focus_by_index(monitor_index_of(m));
   }
   if (be->window == g_root && be->subwindow != None) {
      if (mouse_binding_find(be->state, be->button)) {
         mouse_start_drag(event);
      }
   } else {
      if(be->window == get_current_monitor()->barwin){
         i = x = 0;
         do
            x += get_textw(tags[i]);
         while(be->x >= x && ++i < LENGTH(tags));
         if(i < LENGTH(tags)){
            click = ClkTagBar;
            arg.i = i;
         } else
            click = ClkWinTitle;
      }
      if(click == ClkTagBar && be->button == Button1 && CLEANMASK(be->state) == 0)
         use_tag(&arg);

      if (be->button == Button1 || be->button == Button2 || be->button == Button3) {
         // only change focus on real clicks... not when scrolling
         if (raise_on_click)
            XRaiseWindow(g_display, be->window);

         focus_window(be->window, false, true);
      }
      // handling of event is finished, now propagate event to window
      XAllowEvents(g_display, ReplayPointer, CurrentTime);
   }
}

void buttonrelease(XEvent* event) {
    mouse_stop_drag();
}

void configurerequest(XEvent* event) {
    event_on_configure(*event);
}

void configurenotify(XEvent* event){
   XConfigureEvent *ev = &event->xconfigure;
   HSMonitor *m = get_current_monitor();

   if(ev->window == g_root)
      XMoveResizeWindow(g_display, m->barwin, m->rect.x, m->rect.y, m->rect.width, bh);
}

void clientmessage(XEvent* event) {
    ewmh_handle_client_message(event);
}

void destroynotify(XEvent* event) {
    // try to unmanage it
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

void keypress(XEvent* event) {
    key_press(event);
}

void mappingnotify(XEvent* event) {
   // regrab when keyboard map changes
   XMappingEvent *ev = &event->xmapping;
   XRefreshKeyboardMapping(ev);
   if(ev->request == MappingKeyboard) {
      grab_keys();
      grab_buttons();
   }
}

void motionnotify(XEvent* event) {
    handle_motion_event(event);
}

void mapnotify(XEvent* event) {
   HSClient* c;
   if ((c = get_client_from_window(event->xmap.window))) {
      // reset focus. so a new window gets the focus if it shall have the input focus
      frame_focus_recursive(g_cur_frame);
      // also update the window title - just to be sure
      client_update_title(c);
   }
}

void maprequest(XEvent* event) {
    XMapRequestEvent* mapreq = &event->xmaprequest;
    if (!get_client_from_window(mapreq->window)) {
        // client should be managed (is not ignored)
        // but is not managed yet
        HSClient* client = manage_client(mapreq->window);
        if (client && find_monitor_with_tag(client->tag)) 
            XMapWindow(g_display, mapreq->window);
    } 
}

void propertynotify(XEvent* event) {
    XPropertyEvent *ev = &event->xproperty;
    HSClient* client;
    if((ev->window == g_root) && (ev->atom == XA_WM_NAME))
         updatestatus();

    if (ev->state == PropertyNewValue &&
          (client = get_client_from_window(ev->window))) {
       switch (ev->atom) {
          case XA_WM_HINTS:
             client_update_wm_hints(client);
             break;
          case XA_WM_NAME:
             client_update_title(client);
             break;
          default:
             break;
        }
    }
    draw_bars();
}

void unmapnotify(XEvent* event) {
    unmanage_client(event->xunmap.window);
}

void setup(void){
   // remove zombies on SIGCHLD
   sigchld(0);

   // set some globals
   g_screen = DefaultScreen(g_display);
   g_screen_width = DisplayWidth(g_display, g_screen);
   g_screen_height = DisplayHeight(g_display, g_screen);
   g_root = RootWindow(g_display, g_screen);
   XSelectInput(g_display, g_root, ROOT_EVENT_MASK);

   // initialize subsystems
   inputs_init();
   clientlist_init();
   layout_init();
}

void cleanup(void) {
   inputs_destroy();
   clientlist_destroy();
   layout_destroy();
}

// --- Main Function --------------------------------------------------------------------
int main(int argc, char* argv[]) {

   if(argc == 2 && !strcmp("-v", argv[1]))
      die("fusionwm-"VERSION" \n");
   else if (argc != 1)
      die("usage: fusionwm [-v]\n");

   if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
      fputs("warning: no locale support\n", stderr);

   if(!(g_display = XOpenDisplay(NULL)))
      die("fusionwm: cannot open display\n");
   checkotherwm();

   // Setup
   setup();
   scan();
   all_monitors_apply_layout();
   updatestatus();

   // Main loop
   XEvent event;
   XSync(g_display, False);
   while (!g_aboutToQuit && !XNextEvent(g_display, &event))
      if(handler[event.type])
         handler[event.type](&event); // call handler

   // Cleanup
   cleanup();
   XCloseDisplay(g_display);

   return EXIT_SUCCESS;
}

