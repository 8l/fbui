
/*=========================================================================
 *
 * libfbui, a library for accessing FBUI (in-kernel framebuffer GUI).
 * Copyright (C) 2003-2004 Zachary T Smith, fbui@comcast.net
 *
 * This module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this module; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * (See the file COPYING in the main directory of this archive for
 * more details.)
 *
 *=======================================================================*/


#ifndef _FBUI_H
#define _FBUI_H

#include <linux/fb.h>


typedef unsigned char uchar;
typedef unsigned char bool;

enum { true=1, false=0 };

#define LIBFBUI_COMMANDBUFLEN (4096)



typedef struct win {
	int id;

	unsigned short command [LIBFBUI_COMMANDBUFLEN + 1];
	unsigned short command_ix;

	int width, height;

	struct win *next;
} Window;

typedef struct {
	int fd;

	Window *list;

	unsigned char shift,ctrl,alt;
	short width, height, depth;

	/* needed for creating native-format pixmaps */
	short red_offset, green_offset, blue_offset;
	short red_length, green_length, blue_length;
} Display;

typedef struct {
	Window *win;
	short id;
	char type;
	short key;
	short x,y,width,height;
} Event;



extern int fbui_poll_event (Display *dpy, Event *, unsigned short mask); /* returns <0 when error */
extern int fbui_wait_event (Display *dpy, Event *, unsigned short mask); /* returns <0 when error */

extern int fbui_read_mouse (Display *dpy, Window*, short*,short*);

extern int fbui_get_dims (Display *dpy, Window*, short*,short*);
extern int fbui_get_position (Display *dpy, Window*, short*,short*);

/* raw to ascii conversion: */
extern int fbui_convert_key (Display *, long);

extern int fbui_flush (Display *, Window *);

extern int fbui_cut (Display*,Window *wm, unsigned char *data, unsigned long length);
extern int fbui_paste (Display*,Window *wm, unsigned char *data, unsigned long max_length);
extern long fbui_cut_length (Display*,Window *wm);

extern int fbui_hide(Display*,Window *wm,short id);
extern int fbui_unhide(Display*,Window *wm,short id);
extern int fbui_delete(Display*,Window *wm,short id);
extern int fbui_redraw(Display*,Window *wm,short id);
extern int fbui_move_resize(Display*,Window *wm,short id,short,short,short,short);
extern int fbui_window_info (Display*,Window *wm,struct fbui_wininfo*,int);
extern int fbui_accelerator (Display* dpy, Window *wm,short key, short op);

extern int fbui_assign_keyfocus (Display*,Window *wm,short);
extern int fbui_assign_ptrfocus (Display*,Window *wm,short);

extern int fbui_placement (Display* dpy,Window *wm, int yes);

extern int fbui_draw_point (Display*,Window*, short x, short y,unsigned long);
extern unsigned long fbui_read_point (Display*,Window*, short x, short y);
extern int fbui_draw_vline (Display*,Window*, short x, short y0, short y1,unsigned long);
extern int fbui_draw_hline (Display*,Window*, short x0, short x1, short y,unsigned long);

extern int fbui_set_subtitle (Display*,Window*, char *);

extern int
fbui_tinyblit (Display *dpy, Window *win, short x, short y,
                unsigned long color,
                unsigned long bgcolor,
                short width,
                unsigned long bits);

extern int fbui_draw_line (Display*,Window*, short x0, short y0, short x1, short y1,unsigned long);
extern int fbui_invert_line (Display*,Window*, short x0, short y0, short x1, short y1);
extern int fbui_draw_string (Display*,Window*, struct fbui_font*,short, short, char *,unsigned long);
extern int fbui_set_font (Display *dpy, Window *win, struct fbui_font *font);
extern int fbui_clear (Display *, Window*);
extern int fbui_draw_rect (Display*,Window*, short x0, short y0, short x1, short y1,unsigned long);
extern int fbui_fill_area (Display*,Window*, short x0, short y0, short x1, short y1,unsigned long);
extern int fbui_clear_area (Display*,Window*, short x0, short y0, short x1, short y1);
extern int fbui_copy_area (Display*,Window*, short xsrc, short ysrc, short xdest, short ydest, short w, short h);
extern int fbui_put (Display*,Window*, short x, short y, short n, unsigned char *p);
extern int fbui_put_rgb (Display*,Window*, short x, short y, short n, unsigned long *p);
extern int fbui_put_rgb3 (Display*,Window*, short x, short y, short n, unsigned char *p);

extern Display *fbui_display_open ();
extern void fbui_display_close (Display *);

extern int fbui_window_close (Display *, Window*);

extern Window *fbui_window_open (Display *dpy, short width, short height, 
	short *width_return, short *height_return,
	short max_width, short max_height,
	short xrel, short yrel, 
	unsigned long *fgcolor_inout, 
	unsigned long *bgcolor_inout,
	char*, char*,
	char progtype, 
	char req_ctl,
	char doing_autopositioning,
	char vc, 
	char need_keys,
	char receive_all_motion,
	char initially_hidden,
	int argc,char** argv);

/* special keys */
#define FBUI_DEL	(127)

#define FBUISPECIALKEYSBASE 1024

#define FBUI_UP		(FBUISPECIALKEYSBASE+0)
#define FBUI_DOWN 	(FBUISPECIALKEYSBASE+1)
#define FBUI_LEFT	(FBUISPECIALKEYSBASE+2)
#define FBUI_RIGHT	(FBUISPECIALKEYSBASE+3)
#define FBUI_F1		(FBUISPECIALKEYSBASE+4)
#define FBUI_F2		(FBUISPECIALKEYSBASE+5)
#define FBUI_F3		(FBUISPECIALKEYSBASE+6)
#define FBUI_F4		(FBUISPECIALKEYSBASE+7)
#define FBUI_F5		(FBUISPECIALKEYSBASE+8)
#define FBUI_F6		(FBUISPECIALKEYSBASE+9)
#define FBUI_F7		(FBUISPECIALKEYSBASE+10)
#define FBUI_F8		(FBUISPECIALKEYSBASE+11)
#define FBUI_F9		(FBUISPECIALKEYSBASE+12)
#define FBUI_F10	(FBUISPECIALKEYSBASE+13)
#define FBUI_F11	(FBUISPECIALKEYSBASE+14)
#define FBUI_F12	(FBUISPECIALKEYSBASE+15)
#define FBUI_INS	(FBUISPECIALKEYSBASE+16)
#define FBUI_HOME	(FBUISPECIALKEYSBASE+17)
#define FBUI_END	(FBUISPECIALKEYSBASE+18)
#define FBUI_PGUP	(FBUISPECIALKEYSBASE+19)
#define FBUI_PGDN	(FBUISPECIALKEYSBASE+20)
#define FBUI_SCRLK	(FBUISPECIALKEYSBASE+21)
#define FBUI_NUMLK	(FBUISPECIALKEYSBASE+22)
#define FBUI_LEFTTAB	(FBUISPECIALKEYSBASE+23)
#define FBUI_PRTSC	(FBUISPECIALKEYSBASE+24)
#define FBUI_CAPSLK	(FBUISPECIALKEYSBASE+25)


#define FONT_WEIGHT_LIGHT (0)
#define FONT_WEIGHT_MEDIUM (1)
#define FONT_WEIGHT_BOLD (2)
#define FONT_WEIGHT_BLACK (3)

typedef struct fbui_font Font;

extern void font_string_dims (Font *font, unsigned char*, short *w, short *ascent, short *descent);
extern void font_char_dims (Font *font, uchar ch, short *w, short *asc, short *desc);

extern Font* font_new (void);
extern void font_free (Font*);

extern char pcf_read (Font* pcf, char *path);

extern char *fbui_get_event_name (int type);

extern int display_fd;
extern Display *my_dpy;

void fbui_print_error (int value);
char *fbui_error_name (int value);


#define FATAL(s) { fbui_display_close (my_dpy); fprintf(stderr,"Error in %s(): %s\n",__FUNCTION__,s); exit(1); }
#define WARNING(s) { fprintf(stderr,"Warning in %s(): %s\n",__FUNCTION__,s); }




#endif
