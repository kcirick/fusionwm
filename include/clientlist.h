/*
 * FusionWM - clientlist.h
 */

#ifndef _CLIENTLIST_H_
#define _CLIENTLIST_H_

#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <stdbool.h>

#include "layout.h"

#define ENUM_WITH_ALIAS(Identifier, Alias) Identifier, Alias = Identifier

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

enum {
    NetSupported = 0,
    NetActiveWindow,
    NetWmName,
    NetWmWindowType,
    NetWmState,
    /* window states */
    NetWmStateFullscreen,
    /* window types */
    ENUM_WITH_ALIAS(NetWmWindowTypeDesktop, NetWmWindowTypeFIRST),
    NetWmWindowTypeDock,
    NetWmWindowTypeUtility,
    NetWmWindowTypeSplash,
    NetWmWindowTypeDialog,
    NetWmWindowTypeNotification,
    ENUM_WITH_ALIAS(NetWmWindowTypeNormal, NetWmWindowTypeLAST),
    /* the count of hints */
    NetCOUNT
};

struct HSTag;
struct HSClient;

Atom g_netatom[NetCOUNT];
extern char* g_netatom_names[];

typedef struct HSClient {
    Window      window;
    XRectangle  last_size;
    HSTag*      tag;
    XRectangle  float_size;
    char        title[256];  // This is never NULL
    bool        urgent;
    bool        fullscreen;
    bool        pseudotile; // should be "floating" 
} HSClient;

typedef struct {
   char *condition;
   char *cond_value;
   int   tag;
   int   manage;
   int   pseudotile;
} Rule;

//---
void clientlist_init();
void clientlist_destroy();

void window_focus(Window window);
void window_unfocus(Window window);
void window_unfocus_last();

// adds a new client to list of managed client windows
HSClient* manage_client(Window win);
void unmanage_client(Window win);

// destroys a special client
void destroy_client(HSClient* client);

HSClient* get_client_from_window(Window window);
HSClient* get_current_client();
XRectangle client_outer_floating_rect(HSClient* client);

void client_setup_border(HSClient* client, bool focused);
void client_resize(HSClient* client, XRectangle rect);
void client_resize_floating(HSClient* client, HSMonitor* m);
void client_resize_fullscreen(HSClient* client, HSMonitor* m);
void client_clear_urgent(HSClient* client);
void client_update_wm_hints(HSClient* client);
void client_update_title(HSClient* client);
void client_close(const Arg *arg);

void client_set_fullscreen(HSClient* client, bool state);
void client_set_pseudotile(HSClient* client, bool state);
void set_pseudotile(const Arg* arg);

void window_set_visible(Window win, bool visible);

// set the desktop property of a window
void ewmh_handle_client_message(XEvent* event);

void rules_apply(struct HSClient* client, int *manage);

#endif

