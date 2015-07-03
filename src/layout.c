/*
 * FusionWM - layout.c
 */

#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "config.h"
#include "systray.h"
#include "inputs.h"

#include <assert.h>
#ifdef XINERAMA
   #include <X11/extensions/Xinerama.h>
#endif //XINERAMA

char* g_layout_names[] = { "vertical", "horizontal", "max" };

void layout_init() {
    g_tags = g_array_new(false, false, sizeof(Tag*));
    g_monitors = g_array_new(false, false, sizeof(Monitor));
   
    // init font
    initfont(font);

    //init colors
    for(int i=0; i<NUMCOLORS; i++){
      dc.colors[i][ColFrameBorder] = getcolor(colors[i][ColFrameBorder]);
      dc.colors[i][ColWindowBorder] = getcolor(colors[i][ColWindowBorder]);
      dc.colors[i][ColFG] = getcolor(colors[i][ColFG]);
      dc.colors[i][ColBG] = getcolor(colors[i][ColBG]);
    }
    dc.drawable = XCreatePixmap(gDisplay, gRoot, 
                                 DisplayWidth(gDisplay, gScreen), 
                                 bar_height, DefaultDepth(gDisplay, gScreen));
    dc.gc = XCreateGC(gDisplay, gRoot, 0, NULL);
    dc.h = bar_height;

    if(!dc.font.set) XSetFont(gDisplay, dc.gc, dc.font.xfont->fid);

    for(int i=0; i<NUMTAGS; i++)
       add_tag(tags[i]);

    update_monitors();
}

void layout_destroy() {
    for (int i = 0; i < g_tags->len; i++) {
        Tag* tag = g_array_index(g_tags, Tag*, i);
        frame_do_recursive(tag->frame, frame_show_clients, 2);
        g_free(tag);
    }
    g_array_free(g_tags, true);
    g_array_free(g_monitors, true);

   if(dc.font.set)   XFreeFontSet(gDisplay, dc.font.set);
   else              XFreeFont(gDisplay, dc.font.xfont);
   XFreePixmap(gDisplay, dc.drawable);
   XFreeGC(gDisplay, dc.gc);

   // Destroy monitors
   for(int i=0; i < g_monitors->len; i++){
      Monitor* m = &g_array_index(g_monitors, Monitor, i);
      XUnmapWindow(gDisplay, m->barwin);
      XDestroyWindow(gDisplay, m->barwin);
      free(m);
   }

   XSync(gDisplay, False);
}

Frame* frame_create_empty() {
    Frame* frame = g_new0(Frame, 1);
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
    frame->window = XCreateWindow(gDisplay, gRoot,
                        42, 42, 42, 42, frame_border_width,
                        DefaultDepth(gDisplay, gScreen), CopyFromParent,
                        DefaultVisual(gDisplay, gScreen),
                        CWOverrideRedirect|CWBackPixmap|CWEventMask, &at);

    XDefineCursor(gDisplay, frame->window, cursor[CurNormal]);

    return frame;
}

void frame_insert_window(Frame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        // insert it here
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        count++;

        Client *c = get_client_from_window(window);
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
        Layout* layout = &frame->content.layout;
        frame_insert_window((layout->selection == 0)? layout->a : layout->b, window);
    }
}

bool frame_remove_window(Frame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        Client *c = get_client_from_window(window);
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

                if(count == 0)   frame_remove_function(frame);
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

void frame_destroy(Frame* frame, Window** buf, size_t* count) {
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
    XDestroyWindow(gDisplay, frame->window);
    g_free(frame);
}

void monitor_apply_layout(Monitor* monitor) {
    if (monitor) {
        XRectangle rect = monitor->rect;
        // apply pad
        rect.y += bar_height;
        rect.height -= bar_height;
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

void frame_apply_client_layout(Frame* frame, XRectangle rect, int layout) {
   Window* buf = frame->content.clients.buf;
   size_t count = frame->content.clients.count;
   size_t count_wo_floats = count - frame->content.clients.floatcount; 
   int selection = frame->content.clients.selection;
   XRectangle cur = rect;
   int last_step_y, last_step_x;
   int step_y, step_x;

   if(layout == LAYOUT_MAX){
      for (int i = 0; i < count; i++) {
         Client* client = get_client_from_window(buf[i]);
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
         Client* client = get_client_from_window(buf[i]);
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

void frame_apply_layout(Frame* frame, XRectangle rect) {
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
      if (rect.width <= WIN_MIN_WIDTH || rect.height <= WIN_MIN_HEIGHT) 
         return;

      XSetWindowBorderWidth(gDisplay, frame->window, frame_border_width);
      // set indicator frame
      unsigned long border_color = dc.colors[0][ColFrameBorder];
      if (g_cur_frame == frame) 
         border_color = dc.colors[1][ColFrameBorder];

      XSetWindowBorder(gDisplay, frame->window, border_color);
      XMoveResizeWindow(gDisplay, frame->window,
            rect.x-frame_border_width, rect.y-frame_border_width,
            rect.width, rect.height);
      XSetWindowBackgroundPixmap(gDisplay, frame->window, ParentRelative);
      XClearWindow(gDisplay, frame->window);
      XLowerWindow(gDisplay, frame->window);
      frame_set_visible(frame, (count != 0) || (g_cur_frame == frame));
      if (count == 0) return;

      frame_apply_client_layout(frame, rect, frame->content.clients.layout);
   } else { /* frame->type == TYPE_FRAMES */
      Layout* layout = &frame->content.layout;
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

void add_monitor(XRectangle rect, Tag* tag, int primary) {
    assert(tag != NULL);
    Monitor m;
    memset(&m, 0, sizeof(m));
    m.rect = rect;
    m.tag = tag;
    m.mouse.x = 0;
    m.mouse.y = 0;
    m.primary = primary;
    update_bar(&m);
    g_array_append_val(g_monitors, m);
}

Monitor* find_monitor_with_tag(Tag* tag) {
    for (int i=0; i<g_monitors->len; i++) {
        Monitor* m = &g_array_index(g_monitors, Monitor, i);
        if (m->tag == tag) 
            return m;
    }
    return NULL;
}

Tag* find_tag(const char* name) {
    for (int i=0; i<g_tags->len; i++) {
        if (!strcmp(g_array_index(g_tags, Tag*, i)->name, name)) 
            return g_array_index(g_tags, Tag*, i);
    }
    return NULL;
}

void add_tag(const char* name) {
    Tag* find_result = find_tag(name);
    if (find_result)  return;
    
    Tag* tag = g_new(Tag, 1);
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

void update_monitors() {
#ifdef XINERAMA
   if(XineramaIsActive(gDisplay)){
      int i, j, nn;
      XineramaScreenInfo *info = XineramaQueryScreens(gDisplay, &nn);
      XineramaScreenInfo *unique = NULL;

      if(!(unique = (XineramaScreenInfo *)malloc(sizeof(XineramaScreenInfo)*nn)))
         die("FATAL", "Could not malloc() %u bytes", sizeof(XineramaScreenInfo)*nn);
      for(i=0, j=0; i<nn; i++)
         if(is_unique_geometry(unique, j, &info[i]))
            memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
      XFree(info);
      nn = j;

      if(g_monitors->len <= nn ){ 
         for(i=0; i<nn; i++){ 
            XRectangle rect = {
               .x = unique[i].x_org,
               .y = unique[i].y_org,
               .width =  unique[i].width,
               .height = unique[i].height,
            }; 
            Tag *cur_tag = g_array_index(g_tags, Tag*, i);
            if((i==0) && (g_monitors->len==0)){ // first one is primary monitor
               add_monitor(rect, cur_tag, 1);
               g_cur_monitor = 0;
               g_cur_frame = cur_tag->frame;
            } else if(i>=g_monitors->len) { // new monitors available
               add_monitor(rect, cur_tag, 0);
            } 
            Monitor* new_mon = &g_array_index(g_monitors, Monitor, i);
            if(rect.x != new_mon->rect.x || rect.y != new_mon->rect.y ||
                  rect.width != new_mon->rect.width || rect.height != new_mon->rect.height){
               new_mon->rect = rect;
               //resizebarwin(new_mon);
               monitor_apply_layout(new_mon);
            }
         }
      } else {// less monitors available
         for(i=nn; i < g_monitors->len; i++)
            g_array_remove_index(g_monitors, i);
      }
   } else 
#endif //XINERAMA //
   {  // Default monitor setup
      XRectangle rect = {
         .x = 0, 
         .y = 0,
         .width = DisplayWidth(gDisplay, gScreen),
         .height = DisplayHeight(gDisplay, gScreen),
      };
      // add monitor with first tag
      Tag *cur_tag = g_array_index(g_tags, Tag*, 0);
      add_monitor(rect, cur_tag, 1);
      g_cur_monitor = 0;
      g_cur_frame = cur_tag->frame;
   }
}

Frame* frame_descend(Frame* frame){
   while (frame->type == TYPE_FRAMES) {
      frame = (frame->content.layout.selection == 0) ?
               frame->content.layout.a : frame->content.layout.b;
   }
   return frame;
}

void cycle(const Arg *arg) {
    // find current selection
    Frame* frame = get_current_monitor()->tag->frame;
    frame = frame_descend(frame);
    if (frame->content.clients.count == 0)  return;
    
    int index = frame->content.clients.selection;
    int count = (int) frame->content.clients.count;
    if(index == count-1) index = 0;
    else                 index++;
    frame->content.clients.selection = index;
    Window window = frame->content.clients.buf[index];
    window_focus(window);
    XRaiseWindow(gDisplay, window);
}

void frame_split(Frame* frame, int align, int fraction) {
    // ensure fraction is allowed
    fraction = CLAMP(fraction, FRACTION_UNIT * (0.0 + FRAME_MIN_FRACTION),
                               FRACTION_UNIT * (1.0 - FRAME_MIN_FRACTION));

    Frame* first = frame_create_empty();
    first->content = frame->content;
    first->type = TYPE_CLIENTS;
    first->parent = frame;

    Frame* second = frame_create_empty();
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
   Frame* frame = get_current_monitor()->tag->frame;
   frame = frame_descend(frame);
   frame_split(frame, ALIGN_VERTICAL, fraction);
}

void split_h(const Arg *arg){
   int fraction = FRACTION_UNIT* CLAMP(arg->f, 0.0 + FRAME_MIN_FRACTION,
                                               1.0 - FRAME_MIN_FRACTION);
   Frame* frame = get_current_monitor()->tag->frame;
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
    Frame* neighbour = frame_neighbour(g_cur_frame, direction);
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
    Frame* parent = neighbour->parent;
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

Frame* frame_neighbour(Frame* frame, char direction) {
    Frame* other;
    bool found = false;
    while (frame->parent) {
        // find frame, where we can change the
        // selection in the desired direction
        Layout* layout = &frame->parent->content.layout;
        switch(direction) {
            case 'r':
                if (layout->align==ALIGN_HORIZONTAL && layout->a==frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'l':
                if (layout->align==ALIGN_HORIZONTAL && layout->b==frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            case 'd':
                if (layout->align==ALIGN_VERTICAL && layout->a==frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'u':
                if (layout->align==ALIGN_VERTICAL && layout->b==frame) {
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
int frame_inner_neighbour_index(Frame* frame, char direction) {
    int index = -1;
    if (frame->type != TYPE_CLIENTS) {
        say("WARNING", "Frame has invalid type");
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
      Frame* neighbour = frame_neighbour(g_cur_frame, direction);
      if (neighbour != NULL) { // if neighbour was found
         Frame* parent = neighbour->parent;
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
      Frame* neighbour = frame_neighbour(g_cur_frame, direction);
      Window win = frame_focused_window(g_cur_frame);
      if (win && neighbour != NULL) { // if neighbour was found
         // change selection in parrent
         Frame* parent = neighbour->parent;
         assert(parent);
         parent->content.layout.selection = ! parent->content.layout.selection;
         // move window to neighbour
         frame_remove_window(g_cur_frame, win);
         frame_insert_window(parent, win);
         if(focus_follows_shift)    frame_focus_recursive(parent);
         else                       frame_focus_recursive(g_cur_frame);

         // layout was changed, so update it
         monitor_apply_layout(get_current_monitor());
      }
   }
}

Window frame_focused_window(Frame* frame) {
    if (!frame) return (Window)0;
    
    // follow the selection to a leave
    frame = frame_descend(frame);
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        return frame->content.clients.buf[selection];
    } // else, if there are no windows
    return (Window)0;
}

bool frame_focus_window(Frame* frame, Window win){
   if(!frame) return false;

   if(frame->type == TYPE_CLIENTS){
      size_t count = frame->content.clients.count;
      Window* buf = frame->content.clients.buf;
      // search for win in buf
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
    Client* client = get_client_from_window(win);
    if (!client) return;
    
    Tag* tag = client->tag;
    assert(client->tag);
    Monitor* monitor = find_monitor_with_tag(tag);
    Monitor* cur_mon = get_current_monitor();
    // if we are not allowed to switch tag and tag is not on 
    // current monitor (or on no monitor) then we cannot focus the window
    if (monitor != cur_mon && !switch_monitor)  return;
    if (monitor == NULL && !switch_tag)         return;
    
    if (monitor != cur_mon && monitor != NULL) {
        if (!switch_monitor) {
            return;
        } else { // switch monitor
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

int frame_focus_recursive(Frame* frame) {
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
void frame_do_recursive(Frame* frame, void (*action)(Frame*), int order) {
    if (!frame) return;
    
    if (frame->type == TYPE_FRAMES) {
        // clients and subframes
        Layout* layout = &(frame->content.layout);
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

void frame_hide(Frame* frame) {
    frame_set_visible(frame, false);
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (int i=0; i<count; i++) 
            window_set_visible(buf[i], false);
    }
}

void frame_show_clients(Frame* frame) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (int i=0; i<count; i++) 
            window_set_visible(buf[i], true);
    }
}

void frame_remove(const Arg *arg){
    frame_remove_function(g_cur_frame);
}

void frame_remove_function(Frame* frame){
   if(!frame->parent)   return;

    assert(g_cur_frame->type == TYPE_CLIENTS);
    Frame* parent = g_cur_frame->parent;
    Frame* first = g_cur_frame;
    Frame* second;
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
    XDestroyWindow(gDisplay, parent->window);
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

Monitor* get_current_monitor() {
    return &g_array_index(g_monitors, Monitor, g_cur_monitor);
}

Monitor* get_primary_monitor() {
   return &g_array_index(g_monitors, Monitor, 0);
}

void frame_set_visible(Frame* frame, bool visible) {
    if (!frame)                           return;
    if (frame->window_visible == visible) return;
    
    window_set_visible(frame->window, visible);
    frame->window_visible = visible;
}

// executes action for each client within frame and its subframes
// if action fails (i.e. returns something != 0), then it abborts with this code
int frame_foreach_client(Frame* frame, ClientAction action, void* data) {
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
        Client* client;
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
        Monitor* m = &g_array_index(g_monitors, Monitor, i);
        monitor_apply_layout(m);
    }
}

void monitor_set_tag(Monitor* monitor, Tag* tag) {
    Monitor* other = find_monitor_with_tag(tag);
    if (monitor == other) return;
    if (other != NULL) return;
    
    Tag* old_tag = monitor->tag;
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
   Monitor* monitor = get_current_monitor();
   Tag*  tag = find_tag(tags[tagindex]);

   if (monitor && tag)
      monitor_set_tag(get_current_monitor(), tag);
}

void move_tag(const Arg *arg) {
   int tagindex = arg->i;
   Tag* target = find_tag(tags[tagindex]);
   
   if (target)
      tag_move_window(target);
}

void tag_move_window(Tag* target) {
    Frame*  frame = g_cur_frame;
    Window window = frame_focused_window(frame);
    if (!g_cur_frame || !window) return;
    
    Monitor* monitor = get_current_monitor();
    if (monitor->tag == target)  return;
    
    Monitor* monitor_target = find_monitor_with_tag(target);
    frame_remove_window(frame, window);
    // insert window into target
    frame_insert_window(target->frame, window);
    Client* client = get_client_from_window(window);
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

int monitor_index_of(Monitor* monitor) {
    return monitor - (Monitor*)g_monitors->data;
}

void monitor_focus_by_index(int new_selection) {
    new_selection = CLAMP(new_selection, 0, g_monitors->len - 1);
    Monitor* old = &g_array_index(g_monitors, Monitor, g_cur_monitor);
    Monitor* monitor = &g_array_index(g_monitors, Monitor, new_selection);
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
    if (True == XQueryPointer(gDisplay, gRoot, &win, &child,
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
        XWarpPointer(gDisplay, None, gRoot, 0, 0, 0, 0, new_x, new_y);
    }
}

void update_bar(Monitor* mon) {
   XSetWindowAttributes wa = {
      .override_redirect = True,
      .background_pixel = dc.colors[0][ColBG],
      .background_pixmap = ParentRelative,
      .event_mask = ButtonPressMask|ExposureMask
   };

   int width = mon->rect.width;
   if(systray_visible && mon->primary==1)   width -= get_systray_width();
   mon->barwin = XCreateWindow(gDisplay, gRoot,
         mon->rect.x, mon->rect.y, width, bar_height, 0,
         DefaultDepth(gDisplay, gScreen), CopyFromParent,
         DefaultVisual(gDisplay, gScreen),
         CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);

   XDefineCursor(gDisplay, mon->barwin, cursor[CurNormal]);
   XMapWindow(gDisplay, mon->barwin);
}

void drawborder(unsigned long col[ColLast]){
   XGCValues gcv;
   XRectangle r = {dc.x, dc.y, dc.w, 2 };

   gcv.foreground = col[ColWindowBorder];
   XChangeGC(gDisplay, dc.gc, GCForeground, &gcv);
   XFillRectangles(gDisplay, dc.drawable, dc.gc, &r, 1);
}

void drawcoloredtext(char *text, Monitor* mon){
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

   XSetForeground(gDisplay, dc.gc, col[ColBG]);
   XFillRectangle(gDisplay, dc.drawable, dc.gc, dc.x, dc.y+2, dc.w, dc.h);
   if(!text)   return;

   olen = strlen(text);
   h = dc.font.ascent + dc.font.descent;
   y = dc.y + (dc.h / 2) - (h/2) + dc.font.ascent;
   x = dc.x + (h/2);

   // shorten text if necessary
   for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w-h; len--);
   if(!len) return;
   memcpy(buf, text, len);
   if(len < olen)
      for(i = len; i && i > len-3; buf[--i] = '.');
   XSetForeground(gDisplay, dc.gc, col[ColFG]);
   if(dc.font.set)
      XmbDrawString(gDisplay, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
   else
      XDrawString(gDisplay, dc.drawable, dc.gc, x, y, buf, len);
}

void initfont(const char *fontstr) {
   char *def, **missing;
   int n;

   dc.font.set = XCreateFontSet(gDisplay, fontstr, &missing, &n, &def);
   if(missing) {
      while(n--)
         say("ERROR", "Missing fontset: %s", missing[n]);
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
      if(!(dc.font.xfont = XLoadQueryFont(gDisplay, fontstr))
            && !(dc.font.xfont = XLoadQueryFont(gDisplay, "fixed")))
         die("ERROR", "Cannot load font: '%s'", fontstr);
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

Monitor* wintomon(Window w){
   int x, y;
   Client *c;
   Monitor *m, *r;
   int di;
   unsigned int dui;
   Window dummy;
   int a, area = 0;

   if(w == gRoot && XQueryPointer(gDisplay, gRoot, &dummy, &dummy, &x, &y, &di, &di, &dui)){
      m = get_current_monitor();
      
      for(int i=0; i<g_monitors->len; i++){
         r = &g_array_index(g_monitors, Monitor, i);
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
      m = &g_array_index(g_monitors, Monitor, i);
      if(w == m->barwin) return m;
   }
   if((c = get_client_from_window(w))){
      m = find_monitor_with_tag(c->tag);
      return m;
   }
   return get_current_monitor();
}

void update_status(void) {
   if(!gettextprop(gRoot, XA_WM_NAME, stext, sizeof(stext)))
      strcpy(stext, "FusionWM-"VERSION);
   draw_bar(get_current_monitor());
}

void draw_bars(){
   for(int i=0; i<g_monitors->len; i++){
      Monitor*m = &g_array_index(g_monitors, Monitor, i);
      draw_bar(m);
   }

   updatesystray();
}

void draw_bar(Monitor* mon){

   resizebarwin(mon);

   unsigned long *col, *bordercol;
   char separator[] = "|";
   dc.x = 0;
   dc.w = mon->rect.width;
   drawborder(dc.colors[2]);
   int barwidth = mon->rect.width;
   Tag* thistag;

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
      if(systray_visible) 
         dc.x -= get_systray_width();

      if(dc.x < x) {
         dc.x = x;
         dc.w = mon->rect.width - x;
      }
      //drawtext(stext, dc.colors[0]);
      drawcoloredtext(stext, mon);
   } else
      dc.x = mon->rect.width;

   // window title
   if((dc.w = dc.x - x) > bar_height) {
         dc.x = x;
         Window win = frame_focused_window(mon->tag->frame);
         char* client_title;
         if(!win || mon != get_current_monitor())    
            client_title = "";
         else        
            client_title = get_client_from_window(win)->title;
         drawtext(client_title, dc.colors[0]);
   }

   XCopyArea(gDisplay, dc.drawable, mon->barwin, dc.gc, 0, 0, barwidth, bar_height, 0, 0);
   XSync(gDisplay, False);
}

void resizebarwin(Monitor *m) {
	unsigned int w = m->rect.width;
	if(systray_visible && m->primary==1 )
		w -= get_systray_width();

	XMoveResizeWindow(gDisplay, m->barwin, m->rect.x, m->rect.y, w, bar_height);
}

