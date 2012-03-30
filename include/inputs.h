/*
 * FusionWM - inputs.h
 */

#ifndef _INPUTS_H_
#define _INPUTS_H_

// foreward declarations
struct HSClient;
struct HSTag;

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

//---
void inputs_init();
void inputs_destroy();

MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);
void grab_keys();
void grab_buttons();

/* some mouse functions */
void mouse_function(XEvent* ev);

void key_press(XEvent* ev);

#endif

