/*
 * FusionWM - layout.c
 */

#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "config.h"

#include <assert.h>
#ifdef XINERAMA
   #include <X11/extensions/Xinerama.h>
#endif //XINERAMA //

// status
static char stext[256];

typedef struct {
   int x, y, w, h;
   unsigned long colors[NUMCOLORS][ColLast];
   Drawable drawable;
   GC gc;
   struct {
      int ascent;
      int descent;
      int height;
      XFontSet set;
      XFontStruct *xfont;
   } font;
} DC; // draw context
static DC dc;

char* g_layout_names[] = {
    "vertical",
    "horizontal",
    "max",
    NULL,
};

GArray*     g_tags; // Array of HSTag*
GArray*     g_monitors; // Array of HSMonitor
//extern char* g_layout_names[];

void layout_init() {
    g_cur_monitor = 0;
    g_tags = g_array_new(false, false, sizeof(HSTag*));
    g_monitors = g_array_new(false, false, sizeof(HSMonitor));
   
    // init font
    initfont(font);

    //init colors
    for(int i=0; i<NUMCOLORS; i++){
      dc.colors[i][ColFrameBorder] = getcolor(colors[i][ColFrameBorder]);
      dc.colors[i][ColWindowBorder] = getcolor(colors[i][ColWindowBorder]);
      dc.colors[i][ColFG] = getcolor(colors[i][ColFG]);
      dc.colors[i][ColBG] = getcolor(colors[i][ColBG]);
    }
    dc.drawable = XCreatePixmap(g_display, g_root, 
                                 DisplayWidth(g_display, DefaultScreen(g_display)), 
                                 bh, DefaultDepth(g_display, DefaultScreen(g_display)));
    dc.gc = XCreateGC(g_display, g_root, 0, NULL);
    dc.h = bh;

    if(!dc.font.set) XSetFont(g_display, dc.gc, dc.font.xfont->fid);

    for(int i=0; i<NUMTAGS; i++)
       add_tag(tags[i]);

    monitors_init();
}

void layout_destroy() {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        frame_do_recursive(tag->frame, frame_show_clients, 2);
        g_free(tag);
    }
    g_array_free(g_tags, true);
    g_array_free(g_monitors, true);

   if(dc.font.set)   XFreeFontSet(g_display, dc.font.set);
   else              XFreeFont(g_display, dc.font.xfont);
   XFreePixmap(g_display, dc.drawable);
   XFreeGC(g_display, dc.gc);

   // Destroy monitors
   for(int i=0; i < g_monitors->len; i++){
      HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
      XUnmapWindow(g_display, m->barwin);
      XDestroyWindow(g_display, m->barwin);
      free(m);
   }
   XSync(g_display, False);
}

HSFrame* frame_create_empty() {
    HSFrame* frame = g_new0(HSFrame, 1);
    frame->type = TYPE_CLIENTS;
    frame->window_visible = false;
    frame->content.clients.layout = default_frame_layout;
    // set window atributes
    XSetWindowAttributes at;
    at.background_pixmap = ParentRelative;
    at.override_redirect = True;
    at.bit_gravity       = StaticGravity;
    at.event_mask        = SubstructureRedirectMask|SubstructureNotifyMask
         |ExposureMask|VisibilityChangeMask
         |EnterWindowMask|LeaveWindowMask|FocusChangeMask;
    frame->window = XCreateWindow(g_display, g_root,
                        42, 42, 42, 42, frame_border_width,
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        CWOverrideRedirect|CWBackPixmap|CWEventMask, &at);
    return frame;
}

void frame_insert_window(HSFrame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        // insert it here
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        count++;

        HSClient *c = get_client_from_window(window);
        size_t floatcount = frame->content.clients.floatcount;
        if(c->floating)
           floatcount++;

        // insert it after the selection
        int index = frame->content.clients.selection + 1;
        index = CLAMP(index, 0, count - 1);
        buf = g_renew(Window, buf, count);
        // shift other windows to the back to insert the new one at index
        memmove(buf + index + 1, buf + index, sizeof(*buf) * (count - index - 1));
        buf[index] = window;
        // write results back
        frame->content.clients.count = count;
        frame->content.clients.buf = buf;
        frame->content.clients.floatcount = floatcount;
        // check for focus
        if ((g_cur_frame == frame && frame->content.clients.selection >= (count-1))
           || focus_new_clients ) {
            frame->content.clients.selection = count - 1;
            window_focus(window);
        }
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        frame_insert_window((layout->selection == 0)? layout->a : layout->b, window);
    }
}

bool frame_remove_window(HSFrame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        HSClient *c = get_client_from_window(window);
        size_t count = frame->content.clients.count;
        size_t floatcount = frame->content.clients.floatcount;
        int i;
        for (i = 0; i < count; i++) {
            if (buf[i] == window) {
                // if window was found, then remove it
                memmove(buf+i, buf+i+1, sizeof(Window)*(count - i - 1));
                count--;
                if(c->floating)   floatcount--;
                buf = g_renew(Window, buf, count);
                frame->content.clients.buf = buf;
                frame->content.clients.count = count;
                frame->content.clients.floatcount = floatcount;
                // find out new selection
                int selection = frame->content.clients.selection;
                // if selection was before removed window
                // then do nothing, otherwise shift by 1
                selection -= (selection < i) ? 0 : 1;
                selection = count ? CLAMP(selection, 0, count-1) : 0;
                frame->content.clients.selection = selection;
                return true;
            }
        }
        return false;
    } else { /* frame->type == TYPE_FRAMES */
        bool found = frame_remove_window(frame->content.layout.a, window);
        found = found || frame_remove_window(frame->content.layout.b, window);
        return found;
    }
}

void frame_destroy(HSFrame* frame, Window** buf, size_t* count) {
    if (frame->type == TYPE_CLIENTS) {
        *buf = frame->content.clients.buf;
        *count = frame->content.clients.count;
    } else { /* frame->type == TYPE_FRAMES */
        size_t c1, c2;
        Window *buf1, *buf2;
        frame_destroy(frame->content.layout.a, &buf1, &c1);
        frame_destroy(frame->content.layout.b, &buf2, &c2);
        // append buf2 to buf1
        buf1 = g_renew(Window, buf1, c1 + c2);
        memcpy(buf1+c1, buf2, sizeof(Window) * c2);
        // free unused things
        g_free(buf2);
        // return;
        *buf = buf1;
        *count = c1 + c2;
    }
    // free other things
    XDestroyWindow(g_display, frame->window);
    g_free(frame);
}

void monitor_apply_layout(HSMonitor* monitor) {
    if (monitor) {
        XRectangle rect = monitor->rect;
        // apply pad
        rect.y += bh;
        rect.height -= bh;
        // apply window gap
        rect.x += window_gap;
        rect.y += window_gap;
        rect.height -= window_gap;
        rect.width -= window_gap;
        frame_apply_layout(monitor->tag->frame, rect);
        
        if (get_current_monitor() == monitor)
            frame_focus_recursive(monitor->tag->frame);
        
        draw_bar(monitor); 
    }
}

void set_layout(const Arg *arg) {
   int layout = 0;

   for(int i=0; i<LENGTH(g_layout_names); i++){
      if(!g_layout_names[i]) continue;
      if(!strcmp(g_layout_names[i], (char*)arg->v)) layout = i;
   }
   if (g_cur_frame && g_cur_frame->type == TYPE_CLIENTS) {
      g_cur_frame->content.clients.layout = layout;
      monitor_apply_layout(get_current_monitor());
   }
   return;
}

void frame_apply_client_layout(HSFrame* frame, XRectangle rect, int layout) {
   Window* buf = frame->content.clients.buf;
   size_t count = frame->content.clients.count;
   size_t count_wo_floats = count - frame->content.clients.floatcount; 
   int selection = frame->content.clients.selection;
   XRectangle cur = rect;
   int last_step_y;
   int last_step_x;
   int step_y;
   int step_x;

   if(layout == LAYOUT_MAX){
      for (int i = 0; i < count; i++) {
         HSClient* client = get_client_from_window(buf[i]);
         client_setup_border(client, (g_cur_frame == frame) && (i == selection));
         client_resize(client, rect);
      }
      return;
   }
   if(count_wo_floats > 0) {
      if (layout == LAYOUT_VERTICAL) {
         // only do steps in y direction
         last_step_y = cur.height % count_wo_floats; // get the space on bottom
         last_step_x = 0;
         cur.height /= count_wo_floats;
         step_y = cur.height;
         step_x = 0;
      } else {
         // only do steps in x direction
         last_step_y = 0;
         last_step_x = cur.width % count_wo_floats; // get the space on the right
         cur.width /= count_wo_floats;
         step_y = 0;
         step_x = cur.width;
      }
      for (int i = 0; i < count; i++) {
         HSClient* client = get_client_from_window(buf[i]);
         if(client->floating) continue;
         // add the space, if count doesnot divide frameheight without remainder
         cur.height += (i == count-1) ? last_step_y : 0;
         cur.width += (i == count-1) ? last_step_x : 0;
         client_setup_border(client, (g_cur_frame == frame) && (i == selection));
         client_resize(client, cur);
         cur.y += step_y;
         cur.x += step_x;
      }
   }
}

void frame_apply_layout(HSFrame* frame, XRectangle rect) {
   if (frame->type == TYPE_CLIENTS) {
      size_t count = frame->content.clients.count;
      // frame only -> apply window_gap
      rect.height -= window_gap;
      rect.width -= window_gap;
      // apply frame width
      rect.x += frame_border_width;
      rect.y += frame_border_width;
      rect.height -= frame_border_width * 2;
      rect.width -= frame_border_width * 2;
      if (rect.width <= WINDOW_MIN_WIDTH || rect.height <= WINDOW_MIN_HEIGHT) 
         return;

      XSetWindowBorderWidth(g_display, frame->window, frame_border_width);
      // set indicator frame
      unsigned long border_color = dc.colors[0][ColFrameBorder];
      if (g_cur_frame == frame) 
         border_color = dc.colors[1][ColFrameBorder];

      XSetWindowBorder(g_display, frame->window, border_color);
      XMoveResizeWindow(g_display, frame->window,
            rect.x-frame_border_width, rect.y-frame_border_width,
            rect.width, rect.height);
      XSetWindowBackgroundPixmap(g_display, frame->window, ParentRelative);
      XClearWindow(g_display, frame->window);
      XLowerWindow(g_display, frame->window);
      frame_set_visible(frame, (count != 0) || (g_cur_frame == frame));
      if (count == 0) return;

      frame_apply_client_layout(frame, rect, frame->content.clients.layout);
   } else { /* frame->type == TYPE_FRAMES */
      HSLayout* layout = &frame->content.layout;
      XRectangle first = rect;
      XRectangle second = rect;
      if (layout->align == ALIGN_VERTICAL) {
         first.height = (rect.height * layout->fraction) / FRACTION_UNIT;
         second.y += first.height;
         second.height -= first.height;
      } else { // (layout->align == ALIGN_HORIZONTAL)
         first.width = (rect.width * layout->fraction) / FRACTION_UNIT;
         second.x += first.width;
         second.width -= first.width;
      }
      frame_set_visible(frame, false);
      frame_apply_layout(layout->a, first);
      frame_apply_layout(layout->b, second);
   }
}

void add_monitor(XRectangle rect, HSTag* tag, int primary) {
    assert(tag != NULL);
    HSMonitor m;
    memset(&m, 0, sizeof(m));
    m.rect = rect;
    m.tag = tag;
    m.mouse.x = 0;
    m.mouse.y = 0;
    m.primary = primary;
    create_bar(&m);
    g_array_append_val(g_monitors, m);
}

HSMonitor* find_monitor_with_tag(HSTag* tag) {
    for (int i=0; i<g_monitors->len; i++) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
        if (m->tag == tag) 
            return m;
    }
    return NULL;
}

HSTag* find_tag(const char* name) {
    for (int i=0; i<g_tags->len; i++) {
        if (!strcmp(g_array_index(g_tags, HSTag*, i)->name, name)) 
            return g_array_index(g_tags, HSTag*, i);
    }
    return NULL;
}

void add_tag(const char* name) {
    HSTag* find_result = find_tag(name);
    if (find_result)  return;
    
    HSTag* tag = g_new(HSTag, 1);
    tag->frame = frame_create_empty();
    tag->name = name;
    tag->urgent = false;
    g_array_append_val(g_tags, tag);
}

#ifdef XINERAMA
Bool is_unique_geometry(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
   while(n--)
      if(unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
         && unique[n].width == info->width && unique[n].height == info->height)
         return False;

   return True;
}
#endif

void monitors_init() {
#ifdef XINERAMA
   if(XineramaIsActive(g_display)){
      int i, j, nn;
      XineramaScreenInfo *info = XineramaQueryScreens(g_display, &nn);
      XineramaScreenInfo *unique = NULL;

      if(!(unique = (XineramaScreenInfo *)malloc(sizeof(XineramaScreenInfo)*nn)))
         die("fatal: could not malloc() %u bytes\n", sizeof(XineramaScreenInfo)*nn);
      for(i=0, j=0; i<nn; i++)
         if(is_unique_geometry(unique, j, &info[i]))
            memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
      XFree(info);
      nn = j;

      for(i=0; i<nn; i++){
         XRectangle rect = {
            .x = unique[i].x_org,
            .y = unique[i].y_org,
            .width =  unique[i].width,
            .height = unique[i].height,
         }; 
         HSTag *cur_tag = g_array_index(g_tags, HSTag*, i);
         if(i==0){ // first one is primary monitor
            add_monitor(rect, cur_tag, 1);
            g_cur_monitor = 0;
            g_cur_frame = cur_tag->frame;
         } else {
            add_monitor(rect, cur_tag, 0);
         }
      }
   } else 
#endif //XINERAMA //
   {  // Default monitor setup
      XRectangle rect = {
         .x = 0, 
         .y = 0,
         .width = DisplayWidth(g_display, DefaultScreen(g_display)),
         .height = DisplayHeight(g_display, DefaultScreen(g_display)),
      };
      // add monitor with first tag
      HSTag *cur_tag = g_array_index(g_tags, HSTag*, 0);
      add_monitor(rect, cur_tag, 1);
      g_cur_monitor = 0;
      g_cur_frame = cur_tag->frame;
   }
}

HSFrame* frame_descend(HSFrame* frame){
   while (frame->type == TYPE_FRAMES) {
      frame = (frame->content.layout.selection == 0) ?
               frame->content.layout.a : frame->content.layout.b;
   }
   return frame;
}

void cycle(const Arg *arg) {
    // find current selection
    HSFrame* frame = get_current_monitor()->tag->frame;
    frame = frame_descend(frame);
    if (frame->content.clients.count == 0)  return;
    
    int index = frame->content.clients.selection;
    int count = (int) frame->content.clients.count;
    if(index == count-1) index = 0;
    else                 index++;
    frame->content.clients.selection = index;
    Window window = frame->content.clients.buf[index];
    window_focus(window);
    XRaiseWindow(g_display, window);
}

void frame_split(HSFrame* frame, int align, int fraction) {
    // ensure fraction is allowed
    fraction = CLAMP(fraction, FRACTION_UNIT * (0.0 + FRAME_MIN_FRACTION),
                               FRACTION_UNIT * (1.0 - FRAME_MIN_FRACTION));

    HSFrame* first = frame_create_empty();
    first->content = frame->content;
    first->type = TYPE_CLIENTS;
    first->parent = frame;

    HSFrame* second = frame_create_empty();
    second->type = TYPE_CLIENTS;
    second->parent = frame;

    frame->type = TYPE_FRAMES;
    frame->content.layout.align = align;
    frame->content.layout.a = first;
    frame->content.layout.b = second;
    frame->content.layout.selection = 0;
    frame->content.layout.fraction = fraction;
    // set focus
    g_cur_frame = first;
    if(first->content.clients.count > 1){
       Window thiswin = first->content.clients.buf[first->content.clients.count-1];
      frame_remove_window(first, thiswin);
      frame_insert_window(second, thiswin);
    }
    // redraw monitor if exists
    monitor_apply_layout(get_current_monitor());
}

void split_v(const Arg *arg){
   int fraction = FRACTION_UNIT* CLAMP(arg->f, 0.0 + FRAME_MIN_FRACTION,
                                               1.0 - FRAME_MIN_FRACTION);
   HSFrame* frame = get_current_monitor()->tag->frame;
   frame = frame_descend(frame);
   frame_split(frame, ALIGN_VERTICAL, fraction);
}

void split_h(const Arg *arg){
   int fraction = FRACTION_UNIT* CLAMP(arg->f, 0.0 + FRAME_MIN_FRACTION,
                                               1.0 - FRAME_MIN_FRACTION);
   HSFrame* frame = get_current_monitor()->tag->frame;
   frame = frame_descend(frame);
   frame_split(frame, ALIGN_HORIZONTAL, fraction);
}

void resize_frame(const Arg *arg){
    char direction = ((char*)arg->v)[0];
    int delta = FRACTION_UNIT * resize_step;

    // if direction is left or up we have to flip delta because e.g. resize up 
    // by 0.1 actually means: reduce fraction by 0.1, i.e. delta = -0.1
    switch (direction) {
        case 'l':   delta *= -1; break;
        case 'r':   break;
        case 'u':   delta *= -1; break;
        case 'd':   break;
        default:    return;
    }
    HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
    if (!neighbour) {
        // then try opposite direction
        switch (direction) {
            case 'l':   direction = 'r'; break;
            case 'r':   direction = 'l'; break;
            case 'u':   direction = 'd'; break;
            case 'd':   direction = 'u'; break;
            default:    assert(false); break;
        }
        neighbour = frame_neighbour(g_cur_frame, direction);
        if (!neighbour)  return ;
    }
    HSFrame* parent = neighbour->parent;
    assert(parent != NULL); // if has neighbour, it also must have a parent
    assert(parent->type == TYPE_FRAMES);
    int fraction = parent->content.layout.fraction;
    fraction += delta;
    fraction = CLAMP(fraction, 
                     (int)(FRAME_MIN_FRACTION * FRACTION_UNIT), 
                     (int)((1.0 - FRAME_MIN_FRACTION) * FRACTION_UNIT));
    parent->content.layout.fraction = fraction;
    // arrange monitor
    monitor_apply_layout(get_current_monitor());
}

HSFrame* frame_neighbour(HSFrame* frame, char direction) {
    HSFrame* other;
    bool found = false;
    while (frame->parent) {
        // find frame, where we can change the
        // selection in the desired direction
        HSLayout* layout = &frame->parent->content.layout;
        switch(direction) {
            case 'r':
                if (layout->align == ALIGN_HORIZONTAL && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'l':
                if (layout->align == ALIGN_HORIZONTAL && layout->b == frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            case 'd':
                if (layout->align == ALIGN_VERTICAL && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'u':
                if (layout->align == ALIGN_VERTICAL && layout->b == frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            default:
                return NULL;
                break;
        }
        if (found)   break;
        // else: go one step closer to root
        frame = frame->parent;
    }
    if (!found)   return NULL;
   
    return other;
}

// finds a neighbour within frame in the specified direction
// returns its index or -1 if there is none
int frame_inner_neighbour_index(HSFrame* frame, char direction) {
    int index = -1;
    if (frame->type != TYPE_CLIENTS) {
        fprintf(stderr, "warning: frame has invalid type\n");
        return -1;
    }
    int selection = frame->content.clients.selection;
    int count = frame->content.clients.count;
    if(frame->content.clients.layout == LAYOUT_VERTICAL) {
       if (direction == 'd') index = selection + 1;
       if (direction == 'u') index = selection - 1;
    } else if (frame->content.clients.layout == LAYOUT_HORIZONTAL) {
       if (direction == 'r') index = selection + 1;
       if (direction == 'l') index = selection - 1;
    }
    // check that index is valid
    if (index < 0 || index >= count) index = -1;
    
    return index;
}

void focus(const Arg *arg){
   char direction = ((char*)arg->v)[0];
   int index;

   if ((index = frame_inner_neighbour_index(g_cur_frame, direction)) != -1) {
      g_cur_frame->content.clients.selection = index;
      frame_focus_recursive(g_cur_frame);
      monitor_apply_layout(get_current_monitor());
   } else {
      HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
      if (neighbour != NULL) { // if neighbour was found
         HSFrame* parent = neighbour->parent;
         // alter focus (from 0 to 1, from 1 to 0)
         int selection = parent->content.layout.selection;
         selection = (selection == 1) ? 0 : 1;
         parent->content.layout.selection = selection;
         // change focus if possible
         frame_focus_recursive(parent);
         monitor_apply_layout(get_current_monitor());
      }
   }
}

void shift(const Arg *arg) {
    char direction = ((char *)arg->v)[0];
    int index;

    if ((index = frame_inner_neighbour_index(g_cur_frame, direction)) != -1) {
        int selection = g_cur_frame->content.clients.selection;
        Window* buf = g_cur_frame->content.clients.buf;
        // if internal neighbour was found, then swap
        Window tmp = buf[selection];
        buf[selection] = buf[index];
        buf[index] = tmp;

        if (focus_follows_shift) 
            g_cur_frame->content.clients.selection = index;
        
        frame_focus_recursive(g_cur_frame);
        monitor_apply_layout(get_current_monitor());
    } else {
        HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
        Window win = frame_focused_window(g_cur_frame);
        if (win && neighbour != NULL) { // if neighbour was found
            // move window to neighbour
            frame_remove_window(g_cur_frame, win);
            frame_insert_window(neighbour, win);
            if (focus_follows_shift) {
                // change selection in parrent
                HSFrame* parent = neighbour->parent;
                assert(parent);
                parent->content.layout.selection = ! parent->content.layout.selection;
                frame_focus_recursive(parent);
                // focus right window in frame
                HSFrame* frame = g_cur_frame;
                assert(frame);
                Window* buf = frame->content.clients.buf;
                size_t count = frame->content.clients.count;
                for (int i=0; i<count; i++) {
                    if (buf[i] == win) {
                        frame->content.clients.selection = i;
                        window_focus(buf[i]);
                        break;
                    }
                }
            } else {
                frame_focus_recursive(g_cur_frame);
            }
            // layout was changed, so update it
            monitor_apply_layout(get_current_monitor());
        }
    }
}

Window frame_focused_window(HSFrame* frame) {
    if (!frame) return (Window)0;
    
    // follow the selection to a leave
    frame = frame_descend(frame);
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        return frame->content.clients.buf[selection];
    } // else, if there are no windows
    return (Window)0;
}

bool frame_focus_window(HSFrame* frame, Window win){
   if(!frame) return false;

   if(frame->type == TYPE_CLIENTS){
      size_t count = frame->content.clients.count;
      Window* buf = frame->content.clients.buf;
      // search for win iin buf
      for(int i=0; i<count; i++){
         if(buf[i] == win){
            //if found, set focus to it
            frame->content.clients.selection = i;
            return true;
         }
      }
   } else { // type == TYPE_FRAMES
      //search in subframes
      bool found = frame_focus_window(frame->content.layout.a, win);
      if(found) {
         frame->content.layout.selection = 0;
         return true;
      }
      found = frame_focus_window(frame->content.layout.b, win);
      if(found) {
         frame->content.layout.selection = 1;
         return true;
      }
   }
   return false;
}

// focus a window
// switch_tag if switch tag to focus to window
// switch_monitor if switch monitor to focus to window
// returns if window was focused or not
void focus_window(Window win, bool switch_tag, bool switch_monitor) {
    HSClient* client = get_client_from_window(win);
    if (!client) return;
    
    HSTag* tag = client->tag;
    assert(client->tag);
    HSMonitor* monitor = find_monitor_with_tag(tag);
    HSMonitor* cur_mon = get_current_monitor();
    // if we are not allowed to switch tag and tag is not on 
    // current monitor (or on no monitor) then we cannot focus the window
    if (monitor != cur_mon && !switch_monitor)  return;
    if (monitor == NULL && !switch_tag)         return;
    
    if (monitor != cur_mon && monitor != NULL) {
        if (!switch_monitor) {
            return;
        } else {
            // switch monitor
            monitor_focus_by_index(monitor_index_of(monitor));
            cur_mon = get_current_monitor();
            assert(cur_mon == monitor);
        }
    }
    monitor_set_tag(cur_mon, tag);
    cur_mon = get_current_monitor();
    if (cur_mon->tag != tag) return;
    
    // now the right tag is visible, now focus it
    frame_focus_window(tag->frame, win);
    frame_focus_recursive(tag->frame);
    monitor_apply_layout(cur_mon);
}

int frame_focus_recursive(HSFrame* frame) {
    // follow the selection to a leave
    frame = frame_descend(frame);
    g_cur_frame = frame;
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        window_focus(frame->content.clients.buf[selection]);
    } else {
        window_unfocus_last();
    }
    return 0;
}

// do recursive for each element of the (binary) frame tree
// if order <= 0 -> action(node); action(left); action(right);
// if order == 1 -> action(left); action(node); action(right);
// if order >= 2 -> action(left); action(right); action(node);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order) {
    if (!frame) return;
    
    if (frame->type == TYPE_FRAMES) {
        // clients and subframes
        HSLayout* layout = &(frame->content.layout);
        if (order <= 0) action(frame);
        frame_do_recursive(layout->a, action, order);
        if (order == 1) action(frame);
        frame_do_recursive(layout->b, action, order);
        if (order >= 2) action(frame);
    } else {
        // action only
        action(frame);
    }
}

static void frame_hide(HSFrame* frame) {
    frame_set_visible(frame, false);
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (int i=0; i<count; i++) 
            window_set_visible(buf[i], false);
    }
}

void frame_show_clients(HSFrame* frame) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (int i=0; i<count; i++) 
            window_set_visible(buf[i], true);
    }
}

void frame_remove(const Arg *arg){
    if (!g_cur_frame->parent) return;
    
    assert(g_cur_frame->type == TYPE_CLIENTS);
    HSFrame* parent = g_cur_frame->parent;
    HSFrame* first = g_cur_frame;
    HSFrame* second;
    if (first == parent->content.layout.a) {
        second = parent->content.layout.b;
    } else {
        assert(first == parent->content.layout.b);
        second = parent->content.layout.a;
    }
    size_t count;
    Window* wins;
    // get all wins from first child
    frame_destroy(first, &wins, &count);
    // and insert them to other child.. inefficiently
    for (int i=0; i<count; i++)
        frame_insert_window(second, wins[i]);
    
    g_free(wins);
    XDestroyWindow(g_display, parent->window);
    // now do tree magic
    // and make second child the new parent set parent
    second->parent = parent->parent;
    // copy all other elements
    *parent = *second;
    // fix childs' parent-pointer
    if (parent->type == TYPE_FRAMES) {
        parent->content.layout.a->parent = parent;
        parent->content.layout.b->parent = parent;
    }
    g_free(second);
    // re-layout
    frame_focus_recursive(parent);
    monitor_apply_layout(get_current_monitor());
}

HSMonitor* get_current_monitor() {
    return &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
}

void frame_set_visible(HSFrame* frame, bool visible) {
    if (!frame)                           return;
    if (frame->window_visible == visible) return;
    
    window_set_visible(frame->window, visible);
    frame->window_visible = visible;
}

// executes action for each client within frame and its subframes
// if action fails (i.e. returns something != 0), then it abborts with this code
int frame_foreach_client(HSFrame* frame, ClientAction action, void* data) {
    int status;
    if (frame->type == TYPE_FRAMES) {
        status = frame_foreach_client(frame->content.layout.a, action, data);
        if (0 != status)   return status;
        
        status = frame_foreach_client(frame->content.layout.b, action, data);
        if (0 != status)   return status;
    } else {
        // frame->type == TYPE_CLIENTS
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        HSClient* client;
        for (int i = 0; i < count; i++) {
            client = get_client_from_window(buf[i]);
            // do action for each client
            status = action(client, data);
            if (0 != status)
                return status;
        }
    }
    return 0;
}

void all_monitors_apply_layout() {
    for (int i=0; i<g_monitors->len; i++) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
        monitor_apply_layout(m);
    }
}

void monitor_set_tag(HSMonitor* monitor, HSTag* tag) {
    HSMonitor* other = find_monitor_with_tag(tag);
    if (monitor == other) return;
    if (other != NULL) return;
    
    HSTag* old_tag = monitor->tag;
    // 1. hide old tag
    frame_do_recursive(old_tag->frame, frame_hide, 2);
    // 2. show new tag
    monitor->tag = tag;
    // first reset focus and arrange windows
    frame_focus_recursive(tag->frame);
    monitor_apply_layout(monitor);
    // then show them (should reduce flicker)
    frame_do_recursive(tag->frame, frame_show_clients, 2);
    // focus again to give input focus
    frame_focus_recursive(tag->frame);
}

void use_tag(const Arg *arg) {
   int tagindex = arg->i;
   HSMonitor* monitor = get_current_monitor();
   HSTag*  tag = find_tag(tags[tagindex]);

   if (monitor && tag)
      monitor_set_tag(get_current_monitor(), tag);
}

void move_tag(const Arg *arg) {
   int tagindex = arg->i;
   HSTag* target = find_tag(tags[tagindex]);
   
   if (target)
      tag_move_window(target);
}

void tag_move_window(HSTag* target) {
    HSFrame*  frame = g_cur_frame;
    Window window = frame_focused_window(frame);
    if (!g_cur_frame || !window) return;
    
    HSMonitor* monitor = get_current_monitor();
    if (monitor->tag == target)  return;
    
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    frame_remove_window(frame, window);
    // insert window into target
    frame_insert_window(target->frame, window);
    HSClient* client = get_client_from_window(window);
    assert(client != NULL);
    client->tag = target;

    // refresh things
    if (monitor && !monitor_target) {
        // window is moved to unvisible tag so hide it
        window_set_visible(window, false);
    }
    frame_focus_recursive(frame);
    monitor_apply_layout(monitor);
    if (monitor_target) 
        monitor_apply_layout(monitor_target);
}

void focus_monitor(const Arg *arg) {
    int new_selection = arg->i;
    monitor_focus_by_index(new_selection);
}

int monitor_index_of(HSMonitor* monitor) {
    return monitor - (HSMonitor*)g_monitors->data;
}

void monitor_focus_by_index(int new_selection) {
    new_selection = CLAMP(new_selection, 0, g_monitors->len - 1);
    HSMonitor* old = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, new_selection);
    if (old == monitor)    return;
    
    // change selection globals
    assert(monitor->tag);
    assert(monitor->tag->frame);
    g_cur_monitor = new_selection;
    frame_focus_recursive(monitor->tag->frame);
    // repaint monitors
    monitor_apply_layout(old);
    monitor_apply_layout(monitor);
    int rx, ry;
    // save old mouse position
    Window win, child;
    int wx, wy;
    unsigned int mask;
    if (True == XQueryPointer(g_display, g_root, &win, &child,
             &rx, &ry, &wx, &wy, &mask)) {
       old->mouse.x = rx - old->rect.x;
       old->mouse.y = ry - old->rect.y;
       old->mouse.x = CLAMP(old->mouse.x, 0, old->rect.width-1);
       old->mouse.y = CLAMP(old->mouse.y, 0, old->rect.height-1);
    }
    
    // restore position of new monitor
    // but only if mouse pointer is not already on new monitor
    int new_x, new_y;
    if ((monitor->rect.x <= rx) && (rx < monitor->rect.x + monitor->rect.width)
        && (monitor->rect.y <= ry) && (ry < monitor->rect.y + monitor->rect.height)) {
        // mouse already is on new monitor
    } else {
        new_x = monitor->rect.x + monitor->mouse.x;
        new_y = monitor->rect.y + monitor->mouse.y;
        XWarpPointer(g_display, None, g_root, 0, 0, 0, 0, new_x, new_y);
    }
}

void create_bar(HSMonitor *mon) {
   XSetWindowAttributes wa = {
      .override_redirect = True,
      .background_pixel = dc.colors[0][ColBG],
      .background_pixmap = ParentRelative,
      .event_mask = ButtonPressMask|ExposureMask
   };

   int width = mon->rect.width;
   if(mon->primary==1) width -= systray_width;
   mon->barwin = XCreateWindow(g_display, g_root,
         mon->rect.x, mon->rect.y, width, bh, 0,
         DefaultDepth(g_display, DefaultScreen(g_display)),
         CopyFromParent,
         DefaultVisual(g_display, DefaultScreen(g_display)),
         CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);

   XMapWindow(g_display, mon->barwin);
}

void drawborder(unsigned long col[ColLast]){
   XGCValues gcv;
   XRectangle r = {dc.x, dc.y, dc.w, 2 };

   gcv.foreground = col[ColWindowBorder];
   XChangeGC(g_display, dc.gc, GCForeground, &gcv);
   XFillRectangles(g_display, dc.drawable, dc.gc, &r, 1);
}

void drawcoloredtext(char *text, HSMonitor* mon){
   Bool first=True;
   char *buf = text, *ptr = buf, c = 1;
   unsigned long *col = dc.colors[0];
   int i, ox = dc.x;

   while( *ptr ) {
      for( i = 0; *ptr < 0 || *ptr > NUMCOLORS; i++, ptr++);
      if( !*ptr ) break;
      c=*ptr;
      *ptr=0;
      if( i ) {
         dc.w = mon->rect.width - dc.x;
         drawtext(buf, col);
         dc.x += textnw(buf, i) + textnw(&c,1);
         if( first ) dc.x += (dc.font.ascent + dc.font.descent ) /2;
         first = False;
      } else if( first ) {
         ox = dc.x += textnw(&c, 1);
      }
      *ptr = c;
      col = dc.colors[ c-1 ];
      buf = ++ptr;
   }
   drawtext(buf, col);
   dc.x = ox;
}

void drawtext(const char *text, unsigned long col[ColLast]){
   char buf[256];
   int i, x, y, h, len, olen;

   XSetForeground(g_display, dc.gc, col[ColBG]);
   XFillRectangle(g_display, dc.drawable, dc.gc, dc.x, dc.y+2, dc.w, dc.h);
   if(!text)   return;

   olen = strlen(text);
   h = dc.font.ascent + dc.font.descent;
   y = dc.y + 2 + (dc.h / 2) - (h/2) + dc.font.ascent;
   x = dc.x + (h/2);

   // shorten text if necessary
   for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w-h; len--);
   if(!len) return;
   memcpy(buf, text, len);
   if(len < olen)
      for(i = len; i && i > len-3; buf[--i] = '.');
   XSetForeground(g_display, dc.gc, col[ColFG]);
   if(dc.font.set)
      XmbDrawString(g_display, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
   else
      XDrawString(g_display, dc.drawable, dc.gc, x, y, buf, len);
}

void initfont(const char *fontstr) {
   char *def, **missing;
   int n;

   dc.font.set = XCreateFontSet(g_display, fontstr, &missing, &n, &def);
   if(missing) {
      while(n--)
         fprintf(stderr, "fusionwm: missing fontset: %s\n", missing[n]);
      XFreeStringList(missing);
   }
   if(dc.font.set) {
      XFontStruct **xfonts;
      char **font_names;

      dc.font.ascent = dc.font.descent = 0;
      XExtentsOfFontSet(dc.font.set);
      n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
      while(n--) {
         dc.font.ascent = MAX(dc.font.ascent, (*xfonts)->ascent);
         dc.font.descent = MAX(dc.font.descent,(*xfonts)->descent);
         xfonts++;
      }
   } else {
      if(!(dc.font.xfont = XLoadQueryFont(g_display, fontstr))
            && !(dc.font.xfont = XLoadQueryFont(g_display, "fixed")))
         die("error, cannot load font: '%s'\n", fontstr);
      dc.font.ascent = dc.font.xfont->ascent;
      dc.font.descent = dc.font.xfont->descent;
   }
   dc.font.height = dc.font.ascent + dc.font.descent;
}

int textnw(const char *text, unsigned int len) {
   XRectangle r;

   if(dc.font.set) {
      XmbTextExtents(dc.font.set, text, len, NULL, &r);
      return r.width;
   }
   return XTextWidth(dc.font.xfont, text, len);
}

int get_textw(const char *text){
   int textw = textnw(text, strlen(text)) + dc.font.height;
   return textw;
}

HSMonitor* wintomon(Window w){
   int x, y;
   HSClient *c;
   HSMonitor *m, *r;
   int di;
   unsigned int dui;
   Window dummy;
   int a, area = 0;

   if(w == g_root && XQueryPointer(g_display, g_root, &dummy, &dummy, &x, &y, &di, &di, &dui)){
      m = get_current_monitor();
      
      for(int i=0; i<g_monitors->len; i++){
         r = &g_array_index(g_monitors, HSMonitor, i);
         a = MAX(0, MIN(x+1,r->rect.x+r->rect.width) - MAX(x,r->rect.x))
             * MAX(0, MIN(y+1,r->rect.y+r->rect.height) - MAX(y,r->rect.y));
         if(a > area) {
            area = a;
            m = r;
         }
      }
      return m;
   }
   for(int i=0; i<g_monitors->len; i++){
      m = &g_array_index(g_monitors, HSMonitor, i);
      if(w == m->barwin) return m;
   }
   if((c = get_client_from_window(w))){
      m = find_monitor_with_tag(c->tag);
      return m;
   }
   return get_current_monitor();
}

void updatestatus(void) {
   if(!gettextprop(g_root, XA_WM_NAME, stext, sizeof(stext)))
      strcpy(stext, "fusionwm-"VERSION);
   draw_bar(get_current_monitor());
}

void draw_bars(){
   for(int i=0; i<g_monitors->len; i++){
      HSMonitor*m = &g_array_index(g_monitors, HSMonitor, i);
      draw_bar(m);
   }
}

void draw_bar(HSMonitor* mon){
   unsigned long *col, *bordercol;
   char separator[] = "|";
   dc.x = 0;
   dc.w = mon->rect.width;
   drawborder(dc.colors[2]);
   int barwidth = mon->rect.width;
   barwidth -= (mon->primary) ? systray_width : 0;
   HSTag* thistag;

   // Draw tag names
   for(int i=0; i < NUMTAGS; i++){
      dc.w = get_textw(tags[i]);
      thistag = find_tag(tags[i]);
      col = dc.colors[2];
      bordercol = dc.colors[2];
      if(thistag->frame->content.clients.count > 0 ) col = dc.colors[0];
      if(thistag->urgent) col = dc.colors[3];
      if(!strcmp(tags[i], mon->tag->name)){
         col = dc.colors[0];
         bordercol = dc.colors[1];
      }
      drawtext(tags[i], col);
      drawborder(bordercol);
      dc.x += dc.w;
   }
   dc.w = get_textw(separator);
   drawtext(separator, dc.colors[1]);
   dc.x+= dc.w;

   // status text
   int x = dc.x;
   if(mon->primary){
      dc.w = get_textw(stext) + 5;
      dc.x = barwidth - dc.w;
      if(dc.x < x) {
         dc.x = x;
         dc.w = mon->rect.width - x;
      }
      drawcoloredtext(stext, mon);
   } else
      dc.x = mon->rect.width;

   // window title
   if((dc.w = dc.x - x) > bh) {
         dc.x = x;
         Window win = frame_focused_window(mon->tag->frame);
         char* client_title;
         if(!win || mon != get_current_monitor())    
            client_title = "";
         else        
            client_title = get_client_from_window(win)->title;
         drawtext(client_title, dc.colors[0]);
   }

   XCopyArea(g_display, dc.drawable, mon->barwin, dc.gc, 0, 0, barwidth, bh, 0, 0);
   XSync(g_display, False);
}

