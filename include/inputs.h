/*
 * FusionWM - inputs.h
 */

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include <X11/cursorfont.h>

enum { CurNormal, CurResize, CurMove, CurLast };
enum { RESIZE, MOVE };

typedef struct MouseBinding {
    unsigned int mask;
    unsigned int button;
    unsigned int mfunc;
} MouseBinding;

typedef struct KeyBinding {
   unsigned int mod;
   KeySym keysym;
   void (*func)(const Arg *);
   const Arg arg;
} KeyBinding;


Cursor cursor[CurLast];

//---
void inputs_init();
void inputs_destroy();

MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);
void grab_keys();
void grab_buttons();

/* some mouse functions */
void mouse_function(XEvent* ev);

void buttonpress(XEvent* event);
void keypress(XEvent* event);

#endif

