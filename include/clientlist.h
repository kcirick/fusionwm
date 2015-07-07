/*
 * FusionWM - clientlist.h
 */

#ifndef _CLIENTLIST_H_
#define _CLIENTLIST_H_

#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "layout.h"

#define ATOM(A) XInternAtom(gDisplay, (A), False)
#define ENUM_WITH_ALIAS(Identifier, Alias) Identifier, Alias = Identifier

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

enum {
    NetSupported, 
    NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation,
    NetWmName, NetWmWindowType, NetWmState,
    NetWmStateFullscreen,
    ENUM_WITH_ALIAS(NetWmWindowTypeDesktop, NetWmWindowTypeFIRST),
    NetWmWindowTypeDock, NetWmWindowTypeSplash,
    NetWmWindowTypeDialog, NetWmWindowTypeNotification,
    ENUM_WITH_ALIAS(NetWmWindowTypeNormal, NetWmWindowTypeLAST),
    NetCOUNT
}; /* ewhm atoms */

enum { Manager, Xembed, XembedInfo, XCOUNT }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMCOUNT }; /* default atoms */

typedef struct Client Client;
struct Client {
    Window        window;
    Tag*          tag;
    XRectangle    last_size;
    XRectangle    float_size;
    char          title[256];  // This is never NULL
    bool          urgent;
    bool          fullscreen;
    bool          floating;
    bool          neverfocus;
    Client*       next;
};

typedef struct {
   char*    class_str;
   int      tag;
   int      floating;
} Rule;

//--- Variables
GArray*  g_clients; // Array of Client*
Atom     g_netatom[NetCOUNT];
Atom     g_xatom[XCOUNT];
Atom     g_wmatom[WMCOUNT];


//--- Functions
void     clientlist_init();
void     clientlist_destroy();

void     window_focus(Window window);
void     window_unfocus(Window window);
void     window_unfocus_last();

Client*  manage_client(Window win);
void     unmanage_client(Window win);
void     destroy_client(Client* client);

Client*  get_client_from_window(Window window);

void     client_setup_border(Client* client, bool focused);
void     client_resize(Client* client, XRectangle rect);
void     client_update_wm_hints(Client* client);
void     client_update_title(Client* client);
void     client_close(const Arg *arg);

void     client_set_fullscreen(Client* client, bool state);
void     client_set_floating(Client* client, bool state);
void     set_floating(const Arg* arg);

void     window_set_visible(Window win, bool visible);

// set the desktop property of a window
void     ewmh_handle_client_message(XEvent* event);
void     rules_apply(struct Client* client, int *manage);

#endif

