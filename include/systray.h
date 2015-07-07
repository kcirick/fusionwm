/*
 * FusionWM - systray.h
 */

#ifndef _SYSTRAY_H_
#define _SYSTRAY_H_

#include "globals.h"

#define SYSTEM_TRAY_REQUEST_DOCK          0
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0

// XEMBED messages
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

typedef struct Systray {
	Window window;
	Client *icon;
} Systray;

Systray* gSystray;

void systray_init();
void systray_destroy();
unsigned int get_systray_width();
void remove_systray_icon(Client *i);
void updatesystray(void);
void updatesystrayicongeom(Client *i, int w, int h);
Client *wintosystrayicon(Window w);

#endif

