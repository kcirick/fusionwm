/*
 * FusionWM - globals.c
 */

#include "globals.h"

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xproto.h>
#include <X11/Xutil.h>

void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

// get X11 color from color string
unsigned long getcolor(const char *colstr) {
    Colormap cmap = DefaultColormap(gDisplay, gScreen);
    XColor color;
    if(!XAllocNamedColor(gDisplay, cmap, colstr, &color, &color))
        die("error, cannot allocate color '%s'\n", colstr);
    return color.pixel;
}

bool gettextprop(Window w, Atom atom, char *text, unsigned int size) {
   char **list = NULL;
   int n;
   XTextProperty name;

   if(!text || size==0) return False;

   text[0] = '\0';
   XGetTextProperty(gDisplay, w, &name, atom);
   if(!name.nitems) return False;

   if(name.encoding == XA_STRING) {
      strncpy(text, (char *)name.value, size - 1);
   } else if(XmbTextPropertyToTextList(gDisplay, &name, &list, &n) >= Success && n > 0 && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
   }
   text[size-1] = '\0';
   XFree(name.value);
   return True;
}
 
void spawn(const Arg *arg){
   char *sh = NULL;
   pid_t pid;

   if(!(sh = getenv("SHELL"))) sh = "/bin/sh";

   if((pid = fork()) == 0){
      if(gDisplay) close(ConnectionNumber(gDisplay));

      setsid();
      execl(sh, sh, "-c", (char*)arg->v, (char*)NULL);
   }
}

void quit(const Arg *arg) {
   gAboutToQuit = true;
}
