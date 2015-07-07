/*
 * FusionWM - systray.c
 */

#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "config.h"
#include "systray.h"


void systray_init(){
	if(!systray_visible || gSystray) return;

   unsigned long systray_orientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
	XSetWindowAttributes wa;
   Monitor *monitor = get_primary_monitor();

   // init systray
   if(!(gSystray = (Systray *)calloc(1, sizeof(Systray))))
      die("FATAL", "Could not malloc() %u bytes", sizeof(Systray));

   gSystray->window = XCreateSimpleWindow(gDisplay, gRoot, 
         monitor->rect.x+ monitor->rect.width, monitor->rect.y, 1, bar_height, 0, 0, dc.colors[1][ColBG]);
   wa.event_mask        = ButtonPressMask | ExposureMask;
   wa.override_redirect = True;
   wa.background_pixmap = ParentRelative;
   wa.background_pixel  = dc.colors[0][ColBG];
   XSelectInput(gDisplay, gSystray->window, SubstructureNotifyMask);
   XChangeProperty(gDisplay, gSystray->window, g_netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
         PropModeReplace, (unsigned char *)&systray_orientation, 1);
   XChangeWindowAttributes(gDisplay, gSystray->window, CWEventMask|CWOverrideRedirect|CWBackPixel|CWBackPixmap, &wa);
   XMapRaised(gDisplay, gSystray->window);
   XSetSelectionOwner(gDisplay, g_netatom[NetSystemTray], gSystray->window, CurrentTime);

   if(XGetSelectionOwner(gDisplay, g_netatom[NetSystemTray]) == gSystray->window) {
      XSync(gDisplay, False);
   } else {
      say("ERROR", "Unable to obtain system tray.");
      free(gSystray);
      gSystray = NULL;
      return;
   }
}

void systray_destroy(){
   if(systray_visible) {
      XUnmapWindow(gDisplay, gSystray->window);
      XDestroyWindow(gDisplay, gSystray->window);
      free(gSystray);
   }
}

unsigned int get_systray_width() {
	unsigned int w = 0;
	Client *c;
	if(systray_visible && gSystray)
		for(c=gSystray->icon; c; w += c->last_size.width+systray_spacing, c=c->next);
	
   return w + systray_spacing + systray_initial_gap;
}

void updatesystray() {
	Client *client;
	Monitor *monitor = get_primary_monitor();
   unsigned int x = monitor->rect.x + monitor->rect.width;
	unsigned int w = 1;

	if(!systray_visible) return;

	for(w = 0, client = gSystray->icon; client; client = client->next) { 
		XMapRaised(gDisplay, client->window);
		w += systray_spacing;
		XMoveResizeWindow(gDisplay, client->window, (client->last_size.x = w), 0, client->last_size.width, client->last_size.height);
		w += client->last_size.width;
	}
	w = w + systray_spacing+systray_initial_gap;
 	x -= w;

   XMoveResizeWindow(gDisplay, gSystray->window, x, monitor->rect.y, w, bar_height);
	
   // redraw background
	XSetForeground(gDisplay, dc.gc, dc.colors[0][ColBG]);
	XFillRectangle(gDisplay, gSystray->window, dc.gc, 0, 0, w, bar_height);
	XSync(gDisplay, False);
}

Client *wintosystrayicon(Window w) {
	Client *c = NULL;

	if(!systray_visible || !w) return c;

	for(c=gSystray->icon; c && c->window != w; c= c->next);
   return c;
}

void updatesystrayicongeom(Client *c, int w, int h) {
	if(!c) return;

   c->last_size.height = bar_height;
   if(w == h)                 c->last_size.width = bar_height;
   else if(h == bar_height)   c->last_size.width = w;
   else                       c->last_size.width = (int) ((float)bar_height * ((float)w / (float)h));
   
   // force icons into the systray dimensions 
   if(c->last_size.height > bar_height) {
      if(c->last_size.width == c->last_size.height)
         c->last_size.width = bar_height;
      else
         c->last_size.width = (int) ((float)bar_height * ((float)c->last_size.width / (float)c->last_size.height));
      c->last_size.height = bar_height;
   }
   //c->float_size = c->last_size;
}

void remove_systray_icon(Client *c) {
	if(!systray_visible || !c) return;

	Client **cc;
	for(cc = &gSystray->icon; *cc && *cc != c; cc = &(*cc)->next);
   if(cc) *cc = c->next;
   free(c);
}

