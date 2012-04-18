/*
 * FusionWM - inputs.c
 */

#include "globals.h"
#include "inputs.h"
#include "layout.h"
#include "clientlist.h"
#include "config.h"
#include "binds.h"

#include <stdlib.h>
#include <X11/XKBlib.h>

static unsigned int numlockmask = 0;

//--------
void inputs_init() {
   grab_keys();
   grab_buttons();
}

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

void mouse_function(XEvent* ev) {
    XButtonEvent* be = &(ev->xbutton);
    MouseBinding* mb = mouse_binding_find(be->state, be->button);
    if (!mb) return;
    
    HSClient *client = get_client_from_window(ev->xbutton.subwindow);
    if (!client) return;
    if (!client->floating) return;

   static XWindowAttributes wa;
   XGetWindowAttributes(g_display, client->window, &wa);

    XGrabPointer(g_display, g_root, False,
        PointerMotionMask|ButtonReleaseMask|ButtonPressMask, GrabModeAsync,
        GrabModeAsync, None, None, CurrentTime);

   int x, y, z;
   int newx, newy, neww, newh;
   unsigned int v;
   Window w;
   XQueryPointer(g_display, g_root, &w, &w, &x, &y, &z, &z, &v);

   XEvent event;
   do {
      XMaskEvent(g_display, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, &event);
      if(event.type == MotionNotify){
         if(mb->mfunc == MOVE){
            newx = wa.x + event.xmotion.x-x;
            newy = wa.y + event.xmotion.y-y;
            neww = wa.width;
            newh = wa.height;
            XMoveWindow(g_display, client->window, newx, newy);
         }
         if(mb->mfunc == RESIZE){
            newx = wa.x;
            newy = wa.y;
            neww = wa.width + event.xmotion.x-x;
            newh = wa.height + event.xmotion.y-y;
            XResizeWindow(g_display, client->window,
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
   XUngrabPointer(g_display, CurrentTime);
}

void grab_buttons() {
   update_numlockmask();
   unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask|LockMask};

   XUngrabButton(g_display, AnyButton, AnyModifier, g_root);
   for(unsigned int i=0; i<LENGTH(buttons); i++) {
      MouseBinding* mb = &(buttons[i]);
      for (unsigned int j = 0; j < LENGTH(modifiers); j++)
         XGrabButton(g_display, mb->button, modifiers[j]|mb->mask, g_root, 
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

void key_press(XEvent* ev) {
    KeySym keysym;
    XKeyEvent *event;

    event = &ev->xkey;
    keysym = XkbKeycodeToKeysym(g_display, (KeyCode)event->keycode, 0, 0);
    for(unsigned int i =0; i<LENGTH(keys); i++)
       if(keysym == keys[i].keysym &&
          CLEANMASK(keys[i].mod) == CLEANMASK(event->state) &&
          keys[i].func)
          keys[i].func(&(keys[i].arg));
}

void grab_keys(void) {
   update_numlockmask();
   unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask|LockMask};
   KeyCode code;

   XUngrabKey(g_display, AnyKey, AnyModifier, g_root); 
   for(unsigned int i = 0; i < LENGTH(keys); i++)
      if((code = XKeysymToKeycode(g_display, keys[i].keysym)))
         for(unsigned int j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(g_display, code, keys[i].mod | modifiers[j], g_root,
               True, GrabModeAsync, GrabModeAsync);
}

