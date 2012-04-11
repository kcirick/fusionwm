/*
 * FusionWM - globals.h
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define LENGTH(X) (sizeof(X)/sizeof(*X))
#define ATOM(A) XInternAtom(g_display, (A), False)

#define WIN_MIN_HEIGHT 32
#define WIN_MIN_WIDTH 32
#define FRAME_MIN_FRACTION 0.1
#define STRING_BUF_SIZE 256

#define RECTANGLE_EQUALS(a, b) (\
        (a).x == (b).x && (a).y == (b).y && \
        (a).width == (b).width && (a).height == (b).height )

#define ROOT_EVENT_MASK   (PropertyChangeMask|SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask)
#define CLIENT_EVENT_MASK (EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask)
#define CLEANMASK(mask)   (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

Display*    g_display;
int         g_screen;
Window      g_root;
bool        g_aboutToQuit;

struct HSClient;
struct HSFrame; 
struct HSTag;

typedef union {
   int i;
   unsigned int ui;
   float f;
   const void *v;
} Arg;

//---
void die(const char *errstr, ...);
unsigned long getcolor(const char *colstr);
bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
void spawn(const Arg *arg);
void quit(const Arg *arg);

#endif
