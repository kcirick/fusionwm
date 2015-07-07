/*
 * FusionWM - inputs.c
 */

#include "globals.h"
#include "inputs.h"
#include "layout.h"
#include "clientlist.h"
#include "config.h"
#include "binds.h"
#include "systray.h"

#include <stdlib.h>
#include <X11/XKBlib.h>

static unsigned int numlockmask = 0;

// button clicks
enum { ClkTagBar, ClkStatusText, ClkWinTitle, ClkClientWin, ClkRootWin, ClkLast }; 

//--------
void inputs_init() {
   // init cursors 
	cursor[CurNormal] = XCreateFontCursor(gDisplay, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(gDisplay, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(gDisplay, XC_fleur);

   XDefineCursor(gDisplay, gRoot, cursor[CurNormal]);

   grab_keys();
   grab_buttons();
}

void inputs_destroy(){
	XFreeCursor(gDisplay, cursor[CurNormal]);
	XFreeCursor(gDisplay, cursor[CurResize]);
	XFreeCursor(gDisplay, cursor[CurMove]);
}

void update_numlockmask() {
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(gDisplay);
    for(i = 0; i < 8; i++)
        for(j = 0; j < modmap->max_keypermod; j++)
            if(modmap->modifiermap[i * modmap->max_keypermod + j]
               == XKeysymToKeycode(gDisplay, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void mouse_function(XEvent* ev) {
    XButtonEvent* be = &(ev->xbutton);
    MouseBinding* mb = mouse_binding_find(be->state, be->button);
    if (!mb) return;
    
    Client *client = get_client_from_window(ev->xbutton.subwindow);
    if (!client) return;
    if (!client->floating) return;

   static XWindowAttributes wa;
   XGetWindowAttributes(gDisplay, client->window, &wa);

    XGrabPointer(gDisplay, gRoot, False,
        PointerMotionMask|ButtonReleaseMask|ButtonPressMask, GrabModeAsync,
        GrabModeAsync, None, cursor[CurMove], CurrentTime);

   int x, y, z;
   int newx, newy, neww, newh;
   unsigned int v;
   Window w;
   XQueryPointer(gDisplay, gRoot, &w, &w, &x, &y, &z, &z, &v);

   XEvent event;
   do {
      XMaskEvent(gDisplay, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, &event);
      if(event.type == MotionNotify){
         if(mb->mfunc == MOVE){
            newx = wa.x + event.xmotion.x-x;
            newy = wa.y + event.xmotion.y-y;
            neww = wa.width;
            newh = wa.height;
            XMoveWindow(gDisplay, client->window, newx, newy);
         }
         if(mb->mfunc == RESIZE){
            newx = wa.x;
            newy = wa.y;
            neww = wa.width + event.xmotion.x-x;
            newh = wa.height + event.xmotion.y-y;
            XResizeWindow(gDisplay, client->window,
                  neww > WIN_MIN_WIDTH ? neww:WIN_MIN_WIDTH,
                  newh > WIN_MIN_HEIGHT? newh:WIN_MIN_HEIGHT);
         }
      }
   } while (event.type != ButtonRelease);

   //update the client geometry
   client->float_size.x = newx;
   client->float_size.y = newy;
   client->float_size.width = neww;
   client->float_size.height = newh;

   client->last_size = client->float_size;
   //XMoveResizeWindow(gDisplay, client->window, newx, newy, neww, newh);   
   XUngrabPointer(gDisplay, CurrentTime);
}

void grab_buttons() {
   update_numlockmask();
   unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask|LockMask};

   XUngrabButton(gDisplay, AnyButton, AnyModifier, gRoot);
   for(unsigned int i=0; i<LENGTH(buttons); i++) {
      MouseBinding* mb = &(buttons[i]);
      for (unsigned int j = 0; j < LENGTH(modifiers); j++)
         XGrabButton(gDisplay, mb->button, modifiers[j]|mb->mask, gRoot, 
            True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
   }
}

MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button) {
   for(int i=0; i<LENGTH(buttons); i++){
      MouseBinding * mb = &buttons[i];
      if((CLEANMASK(modifiers) == CLEANMASK(mb->mask)) && (button == mb->button))
         return mb;
   }
   return NULL;
}

void buttonpress(XEvent* event) {
   XButtonEvent* be = &(event->xbutton);
   unsigned int i, x, click;
   Arg arg = {0};
   Monitor *m;

   // focus monitor if necessary
   if((m = wintomon(be->window)) && m != get_current_monitor()) 
      monitor_focus_by_index(monitor_index_of(m));
   
   if (be->window == gRoot && be->subwindow != None) {
      mouse_function(event);
   } else {
      if(be->window == get_current_monitor()->barwin){
         i = x = 0;
         do    x += get_textw(tags[i]);
         while (be->x >= x && ++i < NTAGS);
         if(i < NTAGS){
            click = ClkTagBar;
            arg.i = i;
         } else if (be->x > get_current_monitor()->rect.width - get_systray_width() - get_textw(stext))
            click = ClkStatusText;
         else
            click = ClkWinTitle;
      }
      if(click==ClkTagBar && be->button==Button1 && CLEANMASK(be->state)==0)
         use_tag(&arg);
      if(click==ClkStatusText && be->button==Button1 && CLEANMASK(be->state)==0){
         Arg sparg={0};
         sparg.v = "gsimplecal"; 
         spawn(&sparg);
      }

      if (be->button==Button1 || be->button==Button2 || be->button==Button3) {
         // only change focus on real clicks... not when scrolling
         if (raise_on_click)
            XRaiseWindow(gDisplay, be->window);

         focus_window(be->window, false, true);
      }
      // handling of event is finished, now propagate event to window
      XAllowEvents(gDisplay, ReplayPointer, CurrentTime);
   }
}


void keypress(XEvent* event) {
    KeySym keysym;
    XKeyEvent *ke = &(event->xkey);

    keysym = XkbKeycodeToKeysym(gDisplay, (KeyCode)ke->keycode, 0, 0);
    for(unsigned int i =0; i<LENGTH(keys); i++)
       if(keysym == keys[i].keysym &&
          CLEANMASK(keys[i].mod) == CLEANMASK(ke->state) &&
          keys[i].func)
          keys[i].func(&(keys[i].arg));
}

void grab_keys(void) {
   update_numlockmask();
   unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask|LockMask};
   KeyCode code;

   XUngrabKey(gDisplay, AnyKey, AnyModifier, gRoot); 
   for(unsigned int i = 0; i < LENGTH(keys); i++)
      if((code = XKeysymToKeycode(gDisplay, keys[i].keysym)))
         for(unsigned int j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(gDisplay, code, keys[i].mod | modifiers[j], gRoot,
               True, GrabModeAsync, GrabModeAsync);
}

