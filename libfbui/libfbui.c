
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/vt.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/param.h>



#include "libfbui.h"


static struct fb_fix_screeninfo fi;
static struct fb_var_screeninfo vi;
static struct fbui_openparams oi;

#define ERRLOG "/tmp/fbui.log"


static int dpi = 75;


/*-----------------------------------------------------------------------------
 * Changes:
 *
 * Sep 23, ZS: added SIGTERM handler
 * Sep 24, ZS: added commandline processing of console# (-c)
 * Sep 25, ZS: added read_point
 * Sep 25, ZS: added errlog
 * Sep 26, ZS: added commandline processing of geometry (-geo)
 * Sep 26, ZS: more signals supported
 * Oct 01, ZS: updated for new use of control ioctl
 * Oct 14, ZS: added support for fbui-input events 
 * Oct 14, ZS: added keycode->char conversion
 * Oct 22, ZS: fbui_open now ensures dims are available
 * Oct 26, ZS: added -fg and -bg
 *
 *----------------------------------------------------------------------------*/


Display *my_dpy = NULL;
int display_fd;


static void
errlog (char *s, char *s2)
{
	FILE *f=fopen(ERRLOG,"a"); 
	if (!f) 
		return;
	fprintf(f, "%s(): %s\n", s,s2); 
	fprintf(f, "\tsystem error(%d): %s\n",  errno,strerror(errno));
	fclose(f);
}


int
fbui_flush (Display *dpy, Window *win)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/

	if (win->command_ix <= 2)
		return 0;

	win->command[0] = win->id;
	win->command[1] = win->command_ix-2;
	if (win->command[1])
		result = ioctl (dpy->fd, FBIO_UI_EXEC, (void*) win->command);
	win->command[0] = win->id;
	win->command[1] = 0;
	win->command_ix = 2;
	return result;
}

static int
check_flush (Display *dpy, Window *win, int need)
{
	unsigned short nwords;
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	nwords = win->command_ix-2;
	if (nwords + need >= LIBFBUI_COMMANDBUFLEN) {
		result = fbui_flush (dpy,win);
	}
	return result;
}



/* called only by wm */
int 
fbui_window_info (Display* dpy, Window *wm, struct fbui_wininfo* info, int ninfo)
{
	int result=0;

	if (!dpy || !info || ninfo<=0) return -1011;
	/*---------------*/

	static struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_WININFO;
	ctl.id = wm->id;
	ctl.info = info;
	ctl.ninfo = ninfo;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl);
}

int 
fbui_accelerator (Display* dpy, Window *wm, short key, short op)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_ACCEL;
	ctl.id = wm->id;
	ctl.x = key;
	ctl.y = op;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}


int 
fbui_cut (Display* dpy, Window *win, unsigned char *data, unsigned long length)
{
	if (!dpy || !win || !data || !length) 
		return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_CUT;
	ctl.id = win->id;
	ctl.pointer = data;
	ctl.cutpaste_length = length;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}

int 
fbui_paste (Display* dpy, Window *win, unsigned char *data, unsigned long max)
{
	if (!dpy || !win || !data || !max) 
		return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_PASTE;
	ctl.id = win->id;
	ctl.pointer = data;
	ctl.cutpaste_length = max;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}


long
fbui_cut_length (Display* dpy, Window *win)
{
	if (!dpy || !win)
		return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_CUTLENGTH;
	ctl.id = win->id;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}


/* 1 => force all windows to be auto placed
 */
/* called only by wm */
int 
fbui_placement (Display* dpy, Window *wm, int yes)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_PLACEMENT;
	ctl.id = wm->id;
	ctl.x = yes;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}


/* called only by wm */
int
fbui_redraw (Display *dpy, Window *wm, short win)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_REDRAW;
	ctl.id = wm->id;
	ctl.id2 = win;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl) < 0 ? -errno : 0;
}



/* called only by wm */
int
fbui_move_resize (Display *dpy, Window *wm, short id, short x0, short y0, short x1, short y1)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	if (x0>x1) { short tmp=x0;x0=x1;x1=tmp; }
	if (y0>y1) { short tmp=y0;y0=y1;y1=tmp; }

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_MOVE_RESIZE;
	ctl.id = wm->id;
	ctl.id2 = id;
	ctl.x = x0;
	ctl.y = y0;
	ctl.width = x1-x0+1;
	ctl.height = y1-y0+1;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl) < 0 ? -errno : 0;
}

/* called only by wm */
int
fbui_delete (Display *dpy, Window *wm, short id)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_DELETE;
	ctl.id = wm->id;
	ctl.id2 = id;

	int rv=1;
	do {
		rv = ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl);
		if (rv < 0 && -errno != FBUI_ERR_DRAWING)
			FATAL ("error when deleting window");
	}
	while (rv);

	return 0;
}

/* called only by wm */
int
fbui_assign_keyfocus (Display *dpy, Window *wm, short id)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_ASSIGN_KEYFOCUS;
	ctl.id = wm->id;
	ctl.id2 = id;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl) < 0 ? -errno : 0;
}

/* called only by wm */
int
fbui_assign_pointerfocus (Display *dpy, Window *wm, short id)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_ASSIGN_PTRFOCUS;
	ctl.id = wm->id;
	ctl.id2 = id;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl) < 0 ? -errno : 0;
}

/* called only by wm */
int
fbui_hide (Display *dpy, Window *wm, short id)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_HIDE;
	ctl.id = wm->id;
	ctl.id2 = id;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl) < 0 ? -errno : 0;
}

/* called only by wm */
int
fbui_unhide (Display *dpy, Window *wm, short id)
{
	int result=0;

	if (!dpy) return -1;
	/*---------------*/

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_UNHIDE;
	ctl.id = wm->id;
	ctl.id2 = id;

	result = ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long)&ctl);
	if (result < 0)
		result = -errno;
	return result;
}

int
fbui_draw_point (Display *dpy, Window *win, short x, short y, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,5))
		return result;

	win->command [win->command_ix++] = FBUI_POINT;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;

	return 0;
}

unsigned long
fbui_read_point (Display *dpy, Window *win, short x, short y)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,3))
		return result;

	win->command [win->command_ix++] = FBUI_READPOINT;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;

	return fbui_flush (dpy, win);
}

int
fbui_draw_vline (Display *dpy, Window *win, short x, short y0, short y1, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,6))
		return result;

	win->command [win->command_ix++] = FBUI_VLINE;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = y1;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;
	win->command [win->command_ix++] = x;

	return 0;
}

int
fbui_tinyblit (Display *dpy, Window *win, short x, short y, 
		unsigned long color,
		unsigned long bgcolor,
		short width,
		unsigned long bits)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,10))
		return result;

	win->command [win->command_ix++] = FBUI_TINYBLIT;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;
	win->command [win->command_ix++] = bgcolor;
	win->command [win->command_ix++] = bgcolor>>16;
	win->command [win->command_ix++] = width;
	win->command [win->command_ix++] = bits;
	win->command [win->command_ix++] = bits>>16;

fbui_flush(dpy,win);

	return 0;
}

int
fbui_draw_hline (Display *dpy, Window *win, short x0, short x1, short y, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,6))
		return result;

	win->command [win->command_ix++] = FBUI_HLINE;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;
	win->command [win->command_ix++] = y;

	return 0;
}

int
fbui_draw_line (Display *dpy, Window *win, short x0, short y0, short x1, short y1, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,7))
		return result;
	win->command [win->command_ix++] = FBUI_LINE;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = y1;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;

	return 0;
}


int
fbui_invert_line (Display *dpy, Window *win, short x0, short y0, short x1, short y1)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,5))
		return result;

	win->command [win->command_ix++] = FBUI_INVERTLINE;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = y1;

	return 0;
}


int
fbui_draw_string (Display *dpy, Window *win, struct fbui_font *font,
	short x0, short y0, char *str_, 
	unsigned long color)
{
	int result=0;
        unsigned long n;
        unsigned long n2;
	unsigned char *str = (unsigned char*) str_;

	if (!dpy || !win || !str)
		return -1;
	/*---------------*/

	if (result = check_flush (dpy, win,10))
		return result;

        if (!str) FATAL("null param3");

	n = (unsigned long) str;
	n2 = (unsigned long)font;

	win->command [win->command_ix++] = FBUI_STRING;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = n2;
	win->command [win->command_ix++] = n2>>16;
	win->command [win->command_ix++] = strlen (str);
	win->command [win->command_ix++] = n;
	win->command [win->command_ix++] = n>>16;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;

	/* gotta flush, string may not last */
	return fbui_flush (dpy, win);
}

int 
fbui_set_subtitle (Display *dpy, Window *win, char *str)
{
	struct fbui_ctrlparams ctl;

	if (!dpy || !win || !str)
		return FBUI_ERR_NULLPTR;

	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_SUBTITLE;
	ctl.id = win->id;
	strncpy (ctl.string, str, FBUI_NAMELEN);

	return ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl) < 0 ? -errno : 0;
}

int
fbui_set_font (Display *dpy, Window *win, struct fbui_font *font)
{
	if (!dpy || !win || !font)
		return -1;

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_SETFONT;
	ctl.id = win->id;
	ctl.pointer = (unsigned char*) font;

	return ioctl (dpy->fd, FBIO_UI_CONTROL, (unsigned long) &ctl) < 0 ? -errno : 0;
}


int
fbui_clear (Display *dpy, Window *win)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,1))
		return result;

	win->command [win->command_ix++] = FBUI_CLEAR;

	return 0;
}

int
fbui_draw_rect (Display *dpy, Window *win, short x0, short y0, short x1, short y1, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,7))
		return result;

	win->command [win->command_ix++] = FBUI_RECT;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = y1;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;

	return 0;
}

int
fbui_fill_area (Display *dpy, Window *win, short x0, short y0, short x1, short y1, unsigned long color)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,7))
		return result;

	win->command [win->command_ix++] = FBUI_FILLAREA;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = y1;
	win->command [win->command_ix++] = color;
	win->command [win->command_ix++] = color>>16;

	return 0;
}

int
fbui_clear_area (Display *dpy, Window *win, short x0, short y0, short x1, short y1)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,5))
		return result;

	win->command [win->command_ix++] = FBUI_CLEARAREA;
	win->command [win->command_ix++] = x0;
	win->command [win->command_ix++] = y0;
	win->command [win->command_ix++] = x1;
	win->command [win->command_ix++] = y1;

	return 0;
}

int
fbui_copy_area (Display *dpy, Window *win, short xsrc, short ysrc, short xdest, short ydest, short w, short h)
{
	int result=0;

	if (!dpy || !win) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,7))
		return result;

	win->command [win->command_ix++] = FBUI_COPYAREA;
	win->command [win->command_ix++] = xsrc;
	win->command [win->command_ix++] = ysrc;
	win->command [win->command_ix++] = xdest;
	win->command [win->command_ix++] = ydest;
	win->command [win->command_ix++] = w;
	win->command [win->command_ix++] = h;

	return 0;
}

int
fbui_put (Display *dpy, Window *win, short x, short y, short n, unsigned char *p)
{
	int result=0;

	if (!dpy || !win || !p) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,6))
		return result;

	win->command [win->command_ix++] = FBUI_PUT;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;
	win->command [win->command_ix++] = (unsigned long)p;
	win->command [win->command_ix++] = ((unsigned long)p)>>16;
	win->command [win->command_ix++] = n;

	return 0;
}

int
fbui_put_rgb (Display *dpy, Window *win, short x, short y, short n, unsigned long *p)
{
	int result=0;

	if (!dpy || !win || !p) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,6))
		return result;

	win->command [win->command_ix++] = FBUI_PUTRGB;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;
	win->command [win->command_ix++] = (unsigned long) p;
	win->command [win->command_ix++] = ((unsigned long) p) >>16;
	win->command [win->command_ix++] = n;

	return 0;
}

int
fbui_put_rgb3 (Display *dpy, Window *win, short x, short y, short n, unsigned char *p)
{
	int result=0;

	if (!dpy || !win || !p) return -1;
	/*---------------*/
	if (result = check_flush (dpy, win,6))
		return result;

	win->command [win->command_ix++] = FBUI_PUTRGB3;
	win->command [win->command_ix++] = x;
	win->command [win->command_ix++] = y;
	win->command [win->command_ix++] = (unsigned long) p;
	win->command [win->command_ix++] = ((unsigned long) p) >>16;
	win->command [win->command_ix++] = n;

	return 0;
}


int
fbui_window_close (Display *dpy, Window *win)
{
	int r;

	if (!dpy || !win) return -1;
	/*---------------*/
	// fbui_flush (dpy, win);

	r = ioctl (dpy->fd, FBIO_UI_CLOSE, win->id);

	Window *prev = NULL;
	Window *ptr = dpy->list;
	while (ptr) {
		if (win == ptr)
			break;
		prev = ptr;
		ptr = ptr->next;
	}
	if (ptr) {
		if (!prev)
			dpy->list = win->next;
		else
			prev->next = win->next;
	}

	free (win);
	return r;
}



// key info that comes from FBUI : bits 0-1= state 2-15=keycode
int
fbui_convert_key (Display *dpy, long keyinfo)
{
	int key,state;
	unsigned short ch=0;

	if (!dpy) return -1;
	/*---------------*/

	state = 3 & keyinfo;
	key = keyinfo >> 2;

	switch (key) {
	case KEY_RIGHTSHIFT:
	case KEY_LEFTSHIFT: 
		dpy->shift = state>0;
		return 0;

	case KEY_RIGHTCTRL:
	case KEY_LEFTCTRL: 
		dpy->ctrl = state>0;
		return 0;

	case KEY_RIGHTALT:
	case KEY_LEFTALT: 
		dpy->alt = state>0;
		return 0;
	}

	if (state==0)
		return 0;

	switch (key) {
	case KEY_ENTER: ch = '\n'; break;
	case KEY_ESC: ch = 27; break;
	case KEY_SPACE: ch = ' '; break;
	case KEY_MINUS: ch = !dpy->shift ? '-' : '_'; break;
	case KEY_EQUAL: ch = !dpy->shift ? '=' : '+'; break;
	case KEY_BACKSPACE: ch = 8; break;
	case KEY_SEMICOLON: ch = !dpy->shift ? ';' : ':'; break;
	case KEY_COMMA: ch = !dpy->shift ? ',' : '<'; break;
	case KEY_DOT: ch = !dpy->shift ? '.' : '>'; break;
	case KEY_GRAVE: ch = !dpy->shift ? '`' : '~'; break;
	case KEY_BACKSLASH: ch = !dpy->shift ? '\\' : '|'; break;
	case KEY_LEFTBRACE: ch = !dpy->shift ? '[' : '{'; break;
	case KEY_RIGHTBRACE: ch = !dpy->shift ? ']' : '}'; break;
	case KEY_APOSTROPHE: ch = !dpy->shift ? '\'' : '"'; break;
	case KEY_SLASH: ch = !dpy->shift ? '/' : '?'; break;
	case KEY_TAB: ch = !dpy->shift ? '\t' : FBUI_LEFTTAB; break;
	case KEY_A: ch = 'a'; break;
	case KEY_B: ch = 'b'; break;
	case KEY_C: ch = 'c'; break;
	case KEY_D: ch = 'd'; break;
	case KEY_E: ch = 'e'; break;
	case KEY_F: ch = 'f'; break;
	case KEY_G: ch = 'g'; break;
	case KEY_H: ch = 'h'; break;
	case KEY_I: ch = 'i'; break;
	case KEY_J: ch = 'j'; break;
	case KEY_K: ch = 'k'; break;
	case KEY_L: ch = 'l'; break;
	case KEY_M: ch = 'm'; break;
	case KEY_N: ch = 'n'; break;
	case KEY_O: ch = 'o'; break;
	case KEY_P: ch = 'p'; break;
	case KEY_Q: ch = 'q'; break;
	case KEY_R: ch = 'r'; break;
	case KEY_S: ch = 's'; break;
	case KEY_T: ch = 't'; break;
	case KEY_U: ch = 'u'; break;
	case KEY_V: ch = 'v'; break;
	case KEY_W: ch = 'w'; break;
	case KEY_X: ch = 'x'; break;
	case KEY_Y: ch = 'y'; break;
	case KEY_Z: ch = 'z'; break;
	case KEY_0: ch = '0'; break;
	case KEY_1: ch = '1'; break;
	case KEY_2: ch = '2'; break;
	case KEY_3: ch = '3'; break;
	case KEY_4: ch = '4'; break;
	case KEY_5: ch = '5'; break;
	case KEY_6: ch = '6'; break;
	case KEY_7: ch = '7'; break;
	case KEY_8: ch = '8'; break;
	case KEY_9: ch = '9'; break;

	/* special chars section */
	case KEY_UP: ch = FBUI_UP; break;
	case KEY_DOWN: ch = FBUI_DOWN; break;
	case KEY_LEFT: ch = FBUI_LEFT; break;
	case KEY_RIGHT: ch = FBUI_RIGHT; break;

	case KEY_INSERT: ch = FBUI_INS; break;
	case KEY_DELETE: ch = FBUI_DEL; break;
	case KEY_HOME: ch = FBUI_HOME; break;
	case KEY_END: ch = FBUI_END; break;
	case KEY_PAGEUP: ch = FBUI_PGUP; break;
	case KEY_PAGEDOWN: ch = FBUI_PGDN; break;
	case KEY_SCROLLLOCK: ch = FBUI_SCRLK; break;
	case KEY_NUMLOCK: ch = FBUI_NUMLK; break;
	case KEY_CAPSLOCK: ch = FBUI_CAPSLK; break;

	case KEY_PRINT: ch = FBUI_PRTSC; break;

	case KEY_F1: ch = FBUI_F1; break;
	case KEY_F2: ch = FBUI_F2; break;
	case KEY_F3: ch = FBUI_F3; break;
	case KEY_F4: ch = FBUI_F4; break;
	case KEY_F5: ch = FBUI_F5; break;
	case KEY_F6: ch = FBUI_F6; break;
	case KEY_F7: ch = FBUI_F7; break;
	case KEY_F8: ch = FBUI_F8; break;
	case KEY_F9: ch = FBUI_F9; break;
	case KEY_F10: ch = FBUI_F10; break;
	case KEY_F11: ch = FBUI_F11; break;
	case KEY_F12: ch = FBUI_F12; break;

	/* mouse buttons */
	case BTN_LEFT: ch = FBUI_BUTTON_LEFT; break;
	case BTN_RIGHT: ch = FBUI_BUTTON_RIGHT; break;
	case BTN_MIDDLE: ch = FBUI_BUTTON_MIDDLE; break;
	}

	if (dpy->ctrl && isalpha(ch))
		ch &= 31;
	else
	if (dpy->shift && isalpha(ch))
		ch = toupper(ch);

	if (dpy->shift && isdigit(ch)) {
		switch (ch) {
		case '0': ch = ')'; break;
		case '1': ch = '!'; break;
		case '2': ch = '@'; break;
		case '3': ch = '#'; break;
		case '4': ch = '$'; break;
		case '5': ch = '%'; break;
		case '6': ch = '^'; break;
		case '7': ch = '&'; break;
		case '8': ch = '*'; break;
		case '9': ch = '('; break;
		}
	}

	return ch;
}


int
fbui_poll_event (Display *dpy, Event *e, unsigned short mask)
{
	Window *win;
	short win_id;
	long retval;

	if (!dpy || !e) 
		return -1;
	/*---------------*/
	win = dpy->list;
	while (win) {
		fbui_flush (dpy, win);
		win = win->next;
	}

	memset (e, 0, sizeof(Event));

	struct fbui_event event;
	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_POLLEVENT;
	ctl.id = dpy->list ? dpy->list->id : -1; /* window id not required but faster */
	ctl.x = (short)mask;
	ctl.event = &event;
	retval = ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl);

	if (retval < 0)
		return -errno;

	e->type = event.type;
	e->id = win_id = event.id;
	e->key = event.key;
	e->x = event.x;
	e->y = event.y;
	e->width = event.width;
	e->height = event.height;

	win = dpy->list;
	while (win) {
		if (win->id == win_id)
			break;
		win = win->next;
	}
	e->win = win;

	if (!win)
		fprintf(stderr, "libfbui: cannot identify window for id %d\n", win_id);
	else
	if (event.type == FBUI_MOVE_RESIZE) {
		win->width = event.width;
		win->height = event.height;
	}

	return 0;
}

int
fbui_wait_event (Display *dpy, Event *e, unsigned short mask)
{
	Window *win;

	if (!dpy || !e) 
		return -1;
	/*---------------*/

	win = dpy->list;
	while (win) {
		fbui_flush (dpy, win);
		win = win->next;
	}

	memset (e, 0, sizeof(Event));

	struct fbui_event event;
	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_WAITEVENT;
	ctl.id = dpy->list ? dpy->list->id : -1; /* window id not required but faster */
	ctl.x = (short)mask;
	ctl.event = &event;
	int retval = ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl);

	if (retval < 0)
		return -errno;

	e->type = event.type;
	short win_id = event.id;
	e->id = win_id;
	e->key = event.key;
	e->x = event.x;
	e->y = event.y;
	e->width = event.width;
	e->height = event.height;

	win = dpy->list;
	while (win) {
		if (win->id == win_id)
			break;
		win = win->next;
	}
	e->win = win;

	if (!win)
		fprintf(stderr, "libfbui: cannot identify window for id %d\n", win_id);
	else
	if (event.type == FBUI_MOVE_RESIZE) {
		win->width = event.width;
		win->height = event.height;
	}


	return 0;
}

int
fbui_get_dims (Display *dpy, Window *win, short *width, short *height)
{
	long result;

	if (!dpy || !win || !width || !height) 
		return -1;
	/*---------------*/

	fbui_flush (dpy, win);

	*width = 0;
	*height = 0;

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_GETDIMS;
	ctl.id = win->id;

	result = ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl);
//printf ("getdims retval %d (%d)\n",result,-errno);
	if (result < 0) 
		return -errno;

	*width = result >> 16;
	*height = result & 0xffff;

	win->width = *width;
	win->height = *height;

	return 0;
}


/* note: mouse buttons appear as keystrokes */
int
fbui_read_mouse (Display *dpy, Window *win, short *x, short *y)
{
	long result;

	if (!dpy || !win || !x || !y) 
		return -1;
	/*---------------*/

	fbui_flush (dpy, win);

	*x = 0;
	*y = 0;

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_READMOUSE;
	ctl.id = win->id;

	result = ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl);
	if (result < 0) 
		return result;

	*x = result >> 16;
	*y = result & 0xffff;

	return 0;
}


int
fbui_get_position (Display *dpy, Window *win, short *x, short *y)
{
	unsigned long result;

	if (!dpy || !win || !x || !y) 
		return -1;
	/*---------------*/

	fbui_flush (dpy, win);

	*x = -1;
	*y = -1;

	struct fbui_ctrlparams ctl;
	memset (&ctl, 0, sizeof (struct fbui_ctrlparams));
	ctl.op = FBUI_GETPOSN;

	result = ioctl (dpy->fd, FBIO_UI_CONTROL, &ctl);
	if (result < 0)
		return 0;

	*x = result >> 16;
	*y = result & 0xffff;

	return 1;
}

static void kill_handler (int foo)
{
	if (my_dpy) {
		Window *win = my_dpy->list;
		while (win) {
			Window *next = win->next;
			fbui_window_close (my_dpy, win);
			win = next;
		}
		my_dpy->list = NULL;
		fbui_display_close (my_dpy);
	}
	exit(-1);
}

static int hex_to_int (char h)
{
	h = tolower(h);
	if (h >= '0' && h <= '9')
		return h - '0';
	else
	if (h >= 'a' && h <= 'f')
		return 10 + h - 'a';
	else
		return 0;
}

static long parse_rgb (char *s)
{
	int len;
	long c;
	if (!s) return -1;
	len = strlen (s);
	if (len == 3) {
		c = hex_to_int (*s++); 
		c <<= 8;
		c |= hex_to_int (*s++); 
		c <<= 8;
		c |= hex_to_int (*s++); 
		c <<= 4;
		return c;
	}
	if (len == 6) {
		c = hex_to_int (*s++); 
		c <<= 4;
		c |= hex_to_int (*s++); 
		c <<= 4;
		c |= hex_to_int (*s++); 
		c <<= 4;
		c |= hex_to_int (*s++); 
		c <<= 4;
		c |= hex_to_int (*s++); 
		c <<= 4;
		c |= hex_to_int (*s++); 
		return c;
	}
	return -1;
}


static int
getline (FILE *f, char *buf, int buflen)
{
        int got;
        char ch;
        int ix=0;

        buf[0] = 0;

        while (ix < (buflen-1)) {
                got = fread (&ch, 1, 1, f);

                if (!got)
                        break;

                if (ch == '\n')
                        break;
                if (ch == '\r')
                        continue;

                buf [ix++] = ch;
        }

        buf [ix] = 0;
        return ix;
}

long
parse_colorname (char *s)
{
	if (!strcmp(s, "red"))
		return RGB_RED;
	if (!strcmp(s, "green"))
		return RGB_GREEN;
	if (!strcmp(s, "blue"))
		return RGB_BLUE;
	if (!strcmp(s, "black"))
		return 0;
	if (!strcmp(s, "white"))
		return 0xffffff;
	if (!strcmp(s, "steelblue"))
		return RGB_STEELBLUE;
	if (!strcmp(s, "sienna"))
		return RGB_SIENNA;
	if (!strcmp(s, "cyan"))
		return RGB_CYAN;
	if (!strcmp(s, "orange"))
		return RGB_ORANGE;
	if (!strcmp(s, "yellow"))
		return RGB_YELLOW;
	if (!strcmp(s, "magenta"))
		return RGB_MAGENTA;
	if (!strcmp(s, "purple"))
		return RGB_PURPLE;
	if (!strcmp(s, "brown"))
		return RGB_BROWN;
	if (!strcmp(s, "gray"))
		return RGB_GRAY;

	FILE *f = fopen ("/usr/X11/lib/X11/rgb.txt","r");
	char buffer [200];
	if (!f) return -1;
	while (getline (f, buffer, 199)) {
		int r,g,b;
		char name[100];
		if (4 == sscanf(buffer,"%u %u %u %s", &r, &g, &b, name)) {
			if (!strcmp (s, name)) {
				long color;
				r &= 0xff;
				g &= 0xff;
				b &= 0xff;
				color = r;
				color <<= 8;
				color |= g;
				color <<= 8;
				color |= b;
				fclose(f);
				return color;
			}
		}
	}
	fclose(f);

	return -1;
}


/* not static since it's used by fbterm */

int fbui_parse_geom (char *s1, short *w, short *h, short *xr, short *yr)
{
	short width=0,height=0,xrel=-1,yrel=-1;
	char *s2 = NULL;
	char *s3 = NULL;
	char *s4 = NULL;

	if (!s1 || !w || !h || !xr || !yr)
		return 0;
	/*---------------*/

	if (*s1 && isdigit(*s1)) {
		char *s2 = strchr (s1, 'x');
		if (!s2)
			return 0;
		*s2++ = 0;

		s3 = s2;
		while (*s3 && isdigit(*s3))
			s3++;

		if (*s3) {
			xrel = *s3=='-' ? -1 : 1;
			*s3++ = 0;

			s4 = s3;
			while (*s4 && isdigit(*s4))
				s4++;

			if (s4) {
				yrel = *s4=='-' ? -1 : 1;
				*s4++ = 0;
			}
		} else {
			xrel = 9999; /* no position given */
			s3 = NULL;
		}

		width = atoi(s1);
		height = atoi(s2);

		if (s3 && s4) {
			int a = atoi(s3);
			int b = atoi(s4);
			if (xrel == 1) {
				xrel = a;
			} else {
				xrel = -a - 1;
			}
			if (yrel == 1) {
				yrel = b;
			} else {
				yrel = -b - 1;
			}
		} else {
			xrel = 9999;
		}
		*w = width;
		*h = height;
		*xr = xrel;
		*yr = yrel;
		return 1;
	} else
		return 0;
}


void
fbui_display_close (Display* dpy)
{
	if (dpy) {
		Window *win = dpy->list;
		while (win) {
			fbui_window_close (dpy, win);
			win = win->next;
		}
		close (dpy->fd);
		free (dpy);
	}
}

Display*
fbui_display_open ()
{
	Display *dpy = NULL;
	int success=0;
	int fd;

	if (my_dpy)
		FATAL ("attempt to re-init");

	fd = open ("/dev/fb0", O_RDWR );
	if (fd < 0)
		fd = open ("/dev/fb/0", O_RDWR );
	display_fd = fd;
	if (fd < 0)
	{
		success= 0;
		perror("open");
		errlog(__FUNCTION__, "Open failed");
	}
	else
	{
		if (ioctl (fd, FBIOGET_FSCREENINFO, &fi))
		{
			perror("ioctl");
			success= 0;
			errlog(__FUNCTION__, "ioctl1 failed");
		}
		else
		if (ioctl (fd, FBIOGET_VSCREENINFO, &vi))
		{
			perror("ioctl");
			success= 0;
			errlog(__FUNCTION__, "ioctl2 failed");
		}
		else
		{
			if (fi.visual != FB_VISUAL_TRUECOLOR &&
			    fi.visual != FB_VISUAL_DIRECTCOLOR )
			{
				FATAL ("Oops, we need a direct/truecolor display");
				success= 0;
				errlog(__FUNCTION__, "wrong visual");
			}
			else
			{
				printf ("Framebuffer resolution: %dx%d, %dbpp\n", vi.xres, vi.yres, vi.bits_per_pixel);

				success = 1;
			}
		}
	}

	if (success) {
		dpy = (Display*) malloc (sizeof(Display));
		if (!dpy) {
			fprintf (stderr, "out of memory\n");
			return NULL;
		} 

		my_dpy = dpy;
		memset ((void*)dpy, 0, sizeof(Display));
		dpy->fd = fd;

		dpy->width = vi.xres;
		dpy->height = vi.yres;
		dpy->depth = vi.bits_per_pixel;

		dpy->red_offset = vi.red.offset;
		dpy->green_offset = vi.green.offset;
		dpy->blue_offset = vi.blue.offset;
		dpy->red_length = vi.red.length;
		dpy->green_length = vi.green.length;
		dpy->blue_length = vi.blue.length;

		signal (SIGTERM, kill_handler);
		signal (SIGINT, kill_handler);
		signal (SIGSEGV, kill_handler);
	}
	return dpy;
}

Window*
fbui_window_open (Display *dpy,
	   short width, short height, 
	   short *width_return, short *height_return,
	   short max_width, short max_height,
	   short xrel, short yrel, 
	   unsigned long *fgcolor_inout, 
	   unsigned long *bgcolor_inout, 
	   char *name, char *subtitle, 
	   char program_type, 
	   char request_control, 
	   char doing_autopositioning,
	   char vc_, 
	   char need_keys,
	   char receive_all_motion,
	   char initially_hidden,
	   int argc, char **argv)
{
	int i;
	char vc=-1;
	long fgcolor;
	long bgcolor;
	Window *win = NULL;
	char force_type = -1;

	if (!dpy || !name || !width_return || !height_return || 
	    !fgcolor_inout || !bgcolor_inout)
	{
		WARNING ("null param");
		return NULL;
	}
	/*---------------*/
	fgcolor = *fgcolor_inout;
	bgcolor = *bgcolor_inout;

        i=1;
	if (argv)
        while(i < argc) {
		char *str = argv[i];
                if (!strncmp(str, "-type=",6)) {
			char *tstr = str+6;
			if (!strcmp (tstr, "app"))
				force_type = FBUI_PROGTYPE_APP;
			else if (strcmp (tstr, "launcher"))
				force_type = FBUI_PROGTYPE_LAUNCHER;
			else if (strcmp (tstr, "tool"))
				force_type = FBUI_PROGTYPE_TOOL;
			else if (strcmp (tstr, "emphemeral"))
				force_type = FBUI_PROGTYPE_EPHEMERAL;
			else if (strcmp (tstr, "none"))
				force_type = FBUI_PROGTYPE_NONE;
		}
                else if (!strncmp(str, "-geo",4)) {
			int ch = str[4];
			char *values=NULL;
			*str = 0;
			if (ch && isdigit(ch)) {
				values = str+4;
			} else {
				if (i != (argc-1)) {
					values = argv[++i];
					argv[i][0] = 0;
				}
			}
				
			if (values) {
				short w,h,xr,yr;
				if (fbui_parse_geom (values,&w,&h,&xr,&yr)) {
					width = w;
					height = h;
					if (xr != 9999) {
						xrel = xr;
						yrel = yr;
					}
				}
                        }
		}
		else
                if (!strncmp (str, "-bg", 3) || !strncmp(str, "-fg",3)) {
			char *s = 3 + str;
			char which = str[1];
			char ch = *s;
			long color = -1;
			*str = 0;
			if (ch == '=')
				ch = *++s;
			if (!ch) {
				s = argv[++i];
				ch = s ? *s : 0;
			}
			if (ch == '#') {
				color = parse_rgb (s+1);
			} 
			else if (ch) {
				color = parse_colorname (s);
			}
			if (color != -1) {
				if (which == 'f')
					fgcolor = color;
				else
					bgcolor = color;
			}
			argv[i][0] = 0;
		}
		else
                if (!strncmp(str, "-c",2)) {
                        char *s = 2 + str;
                        if (*s && isdigit(*s)) {
                                vc = atoi(s);
				*str = 0;
                        } else {
				*str = 0;
                                if (i != (argc-1)) {
                                        vc = atoi(argv[++i]);
					*argv[i] = 0;
                                } else { 
					fprintf(stderr, "Usage: %s [-cCONSOLE#] [-fgCOLOR] [-bgCOLOR]\n", argv[0]); 
				}
                        }
                }
                i++;
        }
	if (vc_ != -1 && vc == -1)
		vc = vc_;

	if (width > max_width)
		width = max_width;
	if (height > max_height)
		height = max_height;

	int result;

	oi.req_control = request_control ? 1 : 0;
	oi.x0 = xrel >= 0 ? xrel : vi.xres + xrel + 1 - width;
	oi.y0 = yrel >= 0 ? yrel : vi.yres + yrel + 1 - height;
	oi.x1= oi.x0 + width - 1;
	oi.y1= oi.y0 + height - 1;
	oi.max_width = 	max_width;
	oi.max_height = max_height;
	oi.bgcolor =	bgcolor;
	oi.initially_hidden = initially_hidden;
	*fgcolor_inout = fgcolor;
	*bgcolor_inout = bgcolor;
printf ("vc=%d\n",vc);
	oi.desired_vc =	vc;
	oi.doing_autoposition =	doing_autopositioning ? 1 : 0;
	oi.need_keys =	need_keys ? 1 : 0;
	oi.receive_all_motion =	receive_all_motion ? 1 : 0;
	oi.program_type = force_type >= 0 ? force_type : program_type;
	strncpy (oi.name, name, FBUI_NAMELEN);
	strncpy (oi.subtitle, subtitle, FBUI_NAMELEN);
	result = ioctl (dpy->fd, FBIO_UI_OPEN, &oi);
	if (result < 0)
	{
		char expr[100];
		sprintf (expr, "errcode %d\n",result);
		errlog(__FUNCTION__, expr);
		return NULL;
	}

	win = (Window*) malloc (sizeof(Window));
	if (!win)
		FATAL ("out of memory");
	memset (win, 0, sizeof (Window));

	win->id = result;
	win->command_ix = 2;
	win->next = dpy->list;
	dpy->list = win;

	short w,h;

	while (fbui_get_dims (dpy, win, &w, &h)) {
		printf ("%s: waiting for window dimensions\n", __FUNCTION__);
		sleep (1);
	}

	*width_return = w;
	*height_return = h;

	return win;
}


Font *
font_new (void)
{
	Font *nu;
	nu = (Font*) malloc(sizeof (Font));
	if (!nu)
		FATAL("out of memory")
	else
	{
		memset ((void*) nu, 0, sizeof (Font));
		nu->bitmap_buffer = NULL;
		// nu->dpi = 0;
		nu->ascent = 0;
		nu->descent = 0;
	}
	return nu;
}

void
font_free (Font* font)
{
	int i;
	if (!font)
		return; // error

	if (font->lefts) 
		free ((void*) font->lefts);
	if (font->widths) 
		free ((void*) font->widths);
	if (font->descents) 
		free ((void*) font->descents);
	if (font->widths) 
		free ((void*) font->widths);
	if (font->heights) 
		free ((void*) font->heights);
	if (font->bitmaps) 
		free ((void*) font->bitmaps);

	free ((void*)font);
}


void 
font_char_dims (Font *font, uchar ch, short *w, short *asc, short *desc)
{
	if (!font || !w || !asc || !desc)
		return; // error

	ch -= font->first_char;
	*w = font->widths[ch];
	*asc = font->ascent;
	*desc = font->descent;
}


void
font_string_dims (Font *font, unsigned char *str, short *w, short *a, short *d)
{
	if (!font || !str || !w || !a || !d)
		return; // error

	short w0 = 0;
	int ch;

	while ((ch = *str++)) {
		ch -= font->first_char;
		if (ch >= 0)
			w0 += font->widths [ch];
	}

	*w = w0;
	*a = font->ascent;
	*d = font->descent;
}



static uchar bit_reversal_array [256] =
{
0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 
0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff, 
};


#define ZZ(a,b,c,d) ((((unsigned long)d)<<24)|(((unsigned long)c)<<16)|(((unsigned short)b)<<8)|a)


inline unsigned long ULONG(char endian, unsigned char* pp) 
{
	if (endian) 
		return (((unsigned long)pp[0]) << 24) | (((unsigned long)pp[1]) << 16) | (((unsigned short)pp[2]) << 8) | pp[3];
	else
		return (((unsigned long)pp[3]) << 24) | (((unsigned long)pp[2]) << 16) | (((unsigned short)pp[1]) << 8) | pp[0];
}


inline unsigned short USHORT(char endian, unsigned char* pp)
{
	unsigned short i;

	if (endian) 
		i = (((unsigned short)pp[0]) << 8) | pp[1];
	else
		i = (((unsigned short)pp[1]) << 8) | pp[0];

	return i;
}


enum {
	PCF_PROPS = 1,
	PCF_ACCEL = 2,
	PCF_METRICS = 4,
	PCF_BITMAPS = 8,
	PCF_INK_METRICS = 16,
	PCF_BDF_ENCODINGS = 32,
	PCF_SWIDTHS = 64,
	PCF_GLYPH_NAMES = 128,
	PCF_BDF_ACCEL = 256,

};

enum {
	PCF_BIG_ENDIAN = (1<<2),
	PCF_COMPRESSED = 256,	
};

// A pcf that is "compressed" has smaller metrics info
// but the bitmap data is the same.


static unsigned char *read_buffer = NULL;

char
pcf_read_encodings (Font* pcf, unsigned char* orig)
{
	unsigned char *ptr = orig;
	unsigned long format;
	char endian;

	format = ULONG(0,ptr); ptr += 4;
	endian = (format & PCF_BIG_ENDIAN) ? true : false;

	ptr += 2; // skip: first column
	ptr += 2; // skip: last column

	ptr += 2; // skip: first row
	ptr += 2; // skip: last row

	// read default char

	unsigned short defaultChar = USHORT(endian,ptr); ptr += 2;
	unsigned short numChars = USHORT(endian,ptr); ptr += 2;

#if 0
	printf ("pcf_read_encodings(): default char = %d, num chars = %d\n",
		pcf->first_char, numChars);
#endif

	/* kludge */
	pcf->first_char = 31;
	pcf->last_char = 255;

	return true;
}


char
pcf_read_bitmaps (Font* pcf, unsigned char* orig)
{
	unsigned char *ptr = orig;
	unsigned long format;
	char endian;
	char compressed;
	unsigned long nChars;
	int i, j;
	unsigned char *offsets;

	format = ULONG(0,ptr); ptr += 4;
	endian = (format & PCF_BIG_ENDIAN) ? true : false;
	compressed = (format & PCF_COMPRESSED) ? true : false;

	int storage_unit = (format >> 4) & 3;
	int row_unit = format & 3;
	int bit_order = format & 8;

	nChars = ULONG(endian,ptr); ptr += 4;
	if (pcf->nchars)
	{
		if (pcf->nchars != nChars)
			FATAL ("#metrics != #chars");
	}
	else
	{
		pcf->nchars = nChars;
	}

	offsets = ptr;
	ptr += 4 * nChars;

	unsigned char *ptr2 = ptr + 4 * (3 & format);
	unsigned long data_size = ULONG(endian,ptr2);

	ptr += 16;

	pcf->bitmap_buffer = malloc (data_size);
	if (!pcf->widths)
	{
		pcf->lefts = malloc (nChars);
		pcf->widths = malloc (nChars);
		pcf->bitwidths = malloc (nChars);
		pcf->descents = malloc (nChars);
		pcf->heights = malloc (nChars);

		if (!pcf->lefts || !pcf->heights)
			FATAL ("out of memory");
	}

	pcf->bitmaps = malloc (nChars * sizeof(char*));

	if (!pcf->bitmap_buffer || !pcf->lefts || !pcf->widths || !pcf->heights 
	    || !pcf->bitmaps)
		FATAL ("unable to allocate font data");

	// Copy over bitmap data.
	for (i=0; i < data_size; i++)
	{
		pcf->bitmap_buffer[i] = ptr[i];
	}

	// Need to ensure that the leftmost font bit is bit 7.
	//
	if (!bit_order)
	{
		unsigned char *p = pcf->bitmap_buffer;
		i = data_size;
		while (i--) {
			*p++ = bit_reversal_array[*p];
		}
	}

	// Need to re-order the bytes to make them
	// little-endian, if they were written in big-endian format.
	//
	unsigned char *p;
	switch (row_unit) {
	default:
	case 0: // row = 1 byte
		break;

	case 1: // row = 1 short
		if (storage_unit == 0 && endian) {
			unsigned char *p = pcf->bitmap_buffer;
			i = nChars / 2;
			while (i)
			{
				int tmp = *p;
				*p = *(p+1);
				*++p = tmp;
				i--;
			}
		}
		break;

	case 2: // row = 1 long
		if (endian) {
			switch (storage_unit) {
			case 0:
				p = pcf->bitmap_buffer;
				i = nChars / 4;
				while (i)
				{
					int tmp = *p;
					*p = *(p+3);
					*(p+3) = tmp;

					p++;
					tmp = *p;
					*p = *(p+1);
					*(p+1) = tmp;

					p += 3;

					i--;
				}
				break;
			case 1:
				p = pcf->bitmap_buffer;
				i = nChars / 4;
				while (i)
				{
					int tmp = *p;
					int tmp2 = *(p+1);
					*p = *(p+2);
					*(p+1) = *(p+3);
					*(p+2) = tmp;
					*(p+3) = tmp2;

					p += 4;

					i--;
				}
				break;
			default:
				break;
			}
		}
		break;
	}

	// Now generate pointers for character data.
	//
	j = 8 << row_unit;

	for (i=0; i < nChars; i++)
	{
		unsigned long offset = ULONG(endian,offsets); offsets += 4;
		pcf->bitmaps [i] = pcf->bitmap_buffer + offset;
		pcf->bitwidths [i] = j;
	}

	return true;
}


char
pcf_read_metrics (Font* pcf, unsigned char* orig)
{
	unsigned char *ptr = orig;
	unsigned long format;
	char endian;
	char compressed;
	unsigned long nMetrics;
	int i;

	format = ULONG(0,ptr); ptr += 4;
	endian = (format & PCF_BIG_ENDIAN) ? true : false;
	compressed = (format & PCF_COMPRESSED) ? true : false;

	if (compressed)
	{
		nMetrics = USHORT(endian,ptr);
		ptr += 2;
	} 
	else
	{
		nMetrics = ULONG(endian,ptr);
		ptr += 4;
	}

	if (!pcf->nchars && !pcf->widths)
	{
		pcf->lefts = malloc (nMetrics);
		pcf->widths = malloc (nMetrics);
		pcf->bitwidths = malloc (nMetrics);
		pcf->descents = malloc (nMetrics);
		pcf->heights = malloc (nMetrics);

		if (!pcf->lefts || !pcf->heights)
			FATAL ("out of memory");
	}

	if (compressed)
	{
		for (i=0; i < nMetrics; i++)
		{
			pcf->lefts[i] = ((short)*ptr++) - 0x80; 
			ptr++; // skip right-bearing
			pcf->widths[i] = ((short)*ptr++) - 0x80;
			pcf->heights [i] = ((short)*ptr++) - 0x80; // get ascent
			pcf->descents[i] = ((short)*ptr++) - 0x80;
			pcf->heights [i] += pcf->descents[i]; // height=ascent+descent
			// There is no attr byte in compressed version.

			pcf->bitwidths[i] = pcf->widths[i];

#if 0
printf ("char %c(%d): w=%d ht=%d desc=%d\n", i+32,i+32, pcf->widths[i], pcf->heights[i], pcf->descents[i]);
#endif
		}
	}
	else
	{
		for (i=0; i < nMetrics; i++)
		{
			pcf->lefts [i] = USHORT (endian,ptr); ptr += 2;
			ptr += 2;
			pcf->widths[i] = USHORT (endian,ptr); ptr += 2;
			pcf->heights [i] = USHORT (endian,ptr); ptr += 2;
			pcf->descents[i] = USHORT (endian,ptr); ptr += 2;
			ptr += 2; // skip attr word

			pcf->bitwidths[i] = pcf->widths[i];
		}
	}

	return true;
}

char
pcf_read_accelerator (Font* pcf, unsigned char* orig)
{
	unsigned char *ptr = orig;
	unsigned long format;
	char endian;
	int i;

	format = ULONG(0,ptr); ptr += 4;
	endian= (format & PCF_BIG_ENDIAN) ? 1 : 0;

	ptr += 8;

	pcf->ascent = ULONG(endian,ptr); ptr += 4;
	pcf->descent = ULONG(endian,ptr); ptr += 4;

// printf ("ascent = %u , descent = %d\n", ascent, descent);

	return true;
}



char
pcf_read_properties (Font* pcf, unsigned char* orig)
{
	unsigned char *ptr = orig;
	unsigned long format, nprops;
	char endian;
	int i;

	format = ULONG(0,ptr); ptr += 4;
	endian = (format & PCF_BIG_ENDIAN) ? 1 : 0;
	nprops = ULONG(endian,ptr); ptr += 4;

	unsigned char* str_buffer = ptr + nprops*9 + 4;

// printf ("format=%08lx\n", format);
// printf ("nprops=%08lx\n", nprops);

	i = ((unsigned long)str_buffer) & 3;
	if (i) str_buffer += (4-i);

	for (i = 0; i < nprops; i++)
	{
		unsigned long name = ULONG(endian,ptr); ptr += 4;
		unsigned char is_string = *ptr++;
		unsigned long value = ULONG(endian,ptr); ptr += 4;

		char *str = ((char*)str_buffer) + name;
		if (is_string)
		{
			char *str2 = ((char*)str_buffer) + value;

			switch (*str) {

			case 'F':

			if (!strcmp ("FAMILY_NAME", str)) 
			{
//printf ("family-name=%s\n", str2);
#if 0
				if (strlen (str2) < 63)
					strcpy (pcf->family, str2);
				else
					strncpy (pcf->family, str2, 63);
#endif
			} 
			else
			if (!strcmp ("FULL_NAME", str)) 
			{
//printf ("full-name=%s\n", str2);
#if 0
				if (strlen (str2) < 63)
					strcpy (pcf->fullname, str2);
				else
					strncpy (pcf->fullname, str2, 63);
#endif
			} 
			break;

			case 'C':

			if (!strcmp ("CHARSET_REGISTRY", str)) 
			{
// printf ("charset registry=%s\n", str2);
#if 0
				if (strcmp ("ISO8859", str2))
					return false;
#endif
			}
			break;

			case 'S':

			if (!strcmp ("SLANT", str)) 
			{
//printf ("slant=%s\n", str2);
#if 0
				char angle=*str2;
				if (angle=='R')
					pcf->italic=0;
				else
					pcf->italic=1;
#endif
			} 
			break;
			
			case 'W':

			if (!strcmp ("WEIGHT_NAME", str)) 
			{
//prf ("weight-name=%s\n", str2);
#if 0
				if (!strcmp(str2, "Bold")) {
					pcf->weight = FONT_WEIGHT_BOLD;
				}
				else
				if (!strcmp(str2, "Medium")) {
					pcf->weight = FONT_WEIGHT_MEDIUM;
				}
				else
					pcf->weight = FONT_WEIGHT_MEDIUM;
#endif
			}
			break;
			
			default:
				break;

			//printf ("\tunused property name(%ld) %s value(%ld) %s\n",
			 //	name, str, value, str2);
			}
		}
		else
		{
			switch (*str) {

			case 'P':

			if (!strcmp ("POINT_SIZE", str)) 
			{
//printf ("point size=%lu\n", value);
#if 0
				double sz = value;
				pcf->size = sz / 10.0;
#endif
			}
			break;

			case 'R':

			if (!strcmp ("RESOLUTION_X", str)) 
			{
//printf ("resolution x=%lu\n", value);
#if 0
				pcf->dpi = value;
#endif
			} 
			break;

			case 'C':

			// We're using only iso8859-1 fonts.
			//
			if (!strcmp ("CHARSET_ENCODING", str))
			{
// printf ("encoding=%d\n", value);
				if (value != 1)
					return false;
			}
			default:
				break;

			// printf ("\tunused property name %s value %lu\n", str, value);
			}
		}
	}
	return true;
}


char
pcf_read (Font* pcf, char *path)
{
	int i;
	FILE *f;
	static char *line;
	char *param;
	char *param_end;
	short param_length;
	unsigned char *us;
	char *s;
	char *s2;
	char *s3;
	int char_num = 0;
	static char got_start = false;
	static char got_encoding = false;
	static char got_end = false;
	static char got_bitmap = false;
	struct stat statbuf;
	char path2[PATH_MAX];

	if (*path != '/') {
		char *dir = getenv("PCFFONTDIR");
		if (dir) {
			strcpy (path2, dir);
			if ('/' != dir[strlen(dir)-1])
				strcat (path2, "/");
		} else
			sprintf (path2, "/usr/X11R6/lib/fonts/%d%s", dpi, "dpi/");

		strcat (path2, path);
	}
	else
		strcpy (path2, path);

// printf ("path='%s'\n", path2);
	if (stat (path2, &statbuf))
		return false;

	unsigned long size = statbuf.st_size;

#if 0
	pcf->fullname[0]=0;
	pcf->family[0]=0;
#endif

	f = fopen (path2, "rb");
	if (!f)
		return false;

        read_buffer = malloc (size);
        if (!read_buffer)
		FATAL ("unable to allocate font read buffer!");
        i = fread (read_buffer,1,size,f);
        if (i != size)
		FATAL ("cannot read entire font file");
        fclose (f);
        f = NULL;
        uchar *ptr = read_buffer;

	// Read the TOC
	unsigned long toc_type;
	unsigned long toc_total;

	toc_type = ULONG(0,ptr); ptr += 4; 
	toc_total = ULONG(0,ptr); ptr += 4;
	if (toc_type != ZZ(1, 'f','c','p'))
		FATAL ("pcf file has bad header");

	for (i=0; i < toc_total; i++)
	{
		unsigned long j;
		unsigned short type;
		unsigned char *table;

// printf ("toc entry %d:\n", i);
		
		type = ULONG(0,ptr); ptr += 4;
//		printf ("  type=%u, ", type);

		j = ULONG(0,ptr); ptr += 4;
		// printf (" format=%lu, ", j);

		j = ULONG(0,ptr); ptr += 4;
		// printf (" size=%lu, ", j);

		j = ULONG(0,ptr); ptr += 4;
   //		printf (" offset=%lu\n", j);

		table = read_buffer + j;

		switch (type)
		{
		case PCF_PROPS:
			if (!pcf_read_properties (pcf, table))
				return false;
			break;

		case PCF_BDF_ENCODINGS :
			if (!pcf_read_encodings (pcf, table))
				return false;
			break;

		case PCF_ACCEL:
			if (!pcf_read_accelerator (pcf, table))
				return false;
			break;

		case PCF_METRICS:
			if (!pcf_read_metrics (pcf, table))
				return false;
			break;

		case PCF_BITMAPS:
			if (!pcf_read_bitmaps (pcf, table))
				return false;
			break;

		default:
			;
		}
	}

	if (read_buffer)
	{
		free (read_buffer);
		read_buffer = NULL;
	}

	return true;
}


char *fbui_get_event_name (int type)
{
	char *s="(unknown)";
	switch (type) {
	case FBUI_EVENT_NONE: s="(none)"; break;
	case FBUI_EVENT_EXPOSE: s="Expose"; break;
	case FBUI_EVENT_HIDE: s="Hide"; break;
	case FBUI_EVENT_UNHIDE: s="Unhide"; break;
	case FBUI_EVENT_ENTER: s="Enter"; break;
	case FBUI_EVENT_LEAVE: s="Leave"; break;
	case FBUI_EVENT_KEY: s="Key"; break;
	case FBUI_EVENT_MOVE_RESIZE: s="MoveResize"; break;
	case FBUI_EVENT_ACCEL: s="Accel"; break;
	case FBUI_EVENT_WINCHANGE: s="WinChange"; break;
	case FBUI_EVENT_MOTION: s="Motion"; break;
	case FBUI_EVENT_BUTTON: s="Button"; break;
	}
	return s;
}


void fbui_print_error (int value)
{
	printf ("FBUI error: %s\n", fbui_error_name(value));
}

char *fbui_error_name (int value)
{
	char *s = "(none)";

	if (value >= 0)
		value = -value;

	switch (value) {
	case FBUI_ERR_BADADDR: s = "bad address"; break;
	case FBUI_ERR_NULLPTR: s = "null pointer"; break;
	case FBUI_ERR_OFFSCREEN: s = "offscreen"; break;
	case FBUI_ERR_NOTRUNNING: s = "not running"; break;
	case FBUI_ERR_WRONGVISUAL: s = "wrong visual"; break;
	case FBUI_ERR_NOTPLACED: s = "not placed"; break;
	case FBUI_ERR_BIGENDIAN: s = "big endian"; break;
	case FBUI_ERR_INVALIDCMD: s = "invalid command"; break;
	case FBUI_ERR_BADPID: s = "bad process id"; break;
	case FBUI_ERR_ACCELBUSY: s = "accelerator key in use"; break;
	case FBUI_ERR_NOFONT: s = "no font"; break;
	case FBUI_ERR_NOMEM: s = "out of memory"; break;
	case FBUI_ERR_NOTOPEN: s = "not open"; break;
	case FBUI_ERR_OVERLAP: s = "window overlap"; break;
	case FBUI_ERR_ALREADYOPEN: s = "already open"; break;
	case FBUI_ERR_MISSINGWIN: s = "missing window"; break;
	case FBUI_ERR_NOTWM: s = "not a window manager"; break;
	case FBUI_ERR_WRONGWM: s = "wrong window manager"; break;
	case FBUI_ERR_HAVEWM: s = "already have window manager"; break;
	case FBUI_ERR_KEYFOCUSDENIED: s = "key focus request denied"; break;
	case FBUI_ERR_KEYFOCUSERR: s = "key focus error"; break;
	case FBUI_ERR_BADPARAM: s = "bad parameter"; break;
	case FBUI_ERR_NOMOUSE: s = "no mouse"; break;
	case FBUI_ERR_MOUSEREAD: s = "mouse read error"; break;
	case FBUI_ERR_OVERLARGECUT: s = "overlarge cut"; break;
	case FBUI_ERR_BADWIN: s = "bad window"; break;
	case FBUI_ERR_PASTEFAIL: s = "paste failed"; break;
	case FBUI_ERR_CUTFAIL: s = "cut failed"; break;
	case FBUI_ERR_NOEVENT: s = "no events"; break;
	case FBUI_ERR_DRAWING: s = "busy drawing"; break;
	case FBUI_ERR_MISSINGPROCENT: s = "missing process entry"; break;
	case FBUI_ERR_BADVC: s = "bad virtual console number"; break;
	}
	return s;
}
