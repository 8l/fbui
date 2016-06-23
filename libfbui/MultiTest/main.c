
/*=========================================================================
 *
 * fbmtest, a multiple-window-per-application test program for FBUI
 * Copyright (C) 2004 Zachary T Smith, fbui@comcast.net
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


typedef unsigned long u32;
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include <linux/vt.h>
#include <linux/input.h>

#include "libfbui.h"


#define NUMWINS 5
int
main(int argc, char** argv)
{
	Display *dpy;
	Window *windows[NUMWINS];
	int inside[NUMWINS];
	Font *pcf = font_new ();
	u32 fg,bg;
	int i;
	int vc=-1;

	i=1;
	while(i<argc) {
		if (!strncmp("-c",argv[i],2)) {
			if (argv[i][2])
				vc = atoi (2 + argv[i]);
		}
		i++;
	}

	if (!pcf_read (pcf, "timR12.pcf")) {
		font_free (pcf);
		pcf = NULL;
		FATAL ("cannot load font");
	}

	short line_height = pcf->ascent + pcf->descent;

	fg = RGB_YELLOW;
	bg = RGB_BROWN;

	short win_w, win_h;
	dpy = fbui_display_open ();
	if (!dpy) 
		FATAL ("cannot open display");

	srand (time(NULL));

	for (i=0; i< NUMWINS; i++) {
		Window *win;
		char subtitle[10];

		inside[i] = 0;

		sprintf (subtitle, "%d", i);

		do {
			short win_w = 150;
			short win_h = 150;
			int x = rand () % (dpy->width - win_w);
			int y = rand () & (dpy->height - win_h);
			win = fbui_window_open (dpy, win_w, win_h, 
				&win_w, &win_h, 999,999,
				x, y,
				&fg, &bg, 
				"fbmtest", subtitle, 
				FBUI_PROGTYPE_APP, 
				false,false, vc,
				false, 
				false, 
				i == 3 ? true : false, // hide the 4th window
				0,NULL);
		} while (!win);

		windows[i] = win;
	}

	printf ("MultiTest: All windows created.\n");

	while (1) {
		int total;
		int need=0;
		Event ev;
		Window *win;
		int x=0, y=0;
		int type;
		int err;

		/* Get any event for ANY of our windows */

		if (err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL))
		{
			fbui_print_error (err);
			continue;
		}

		win = ev.win;
		if (!win)
			FATAL("null window");

		type = ev.type;

printf ("%s (subwindow %d) got event %s\n", argv[0], win->id, 
	fbui_get_event_name (type));

		if (type == FBUI_EVENT_ENTER)
			inside [win->id] = 1;
		else if (type == FBUI_EVENT_LEAVE)
			inside [win->id] = 0;

		fbui_clear (dpy, win);

		char expr[100];
		sprintf (expr, "This is window %d", win->id);

		i=0;
		int in = 0;
		while(i < NUMWINS) {
			if (win->id == windows[i]->id) {
				in = inside[i];
				break;
			}
			i++;
		}

		if (in)
			fbui_draw_rect (dpy, win, 0, 0, 149,149, RGB_YELLOW);

		fbui_draw_string (dpy, win, pcf, 6,6, expr, fg);

		if (type == FBUI_EVENT_MOTION) {
			fbui_draw_line (dpy, win, 0,0, ev.x, ev.y, RGB_WHITE);
			fbui_flush (dpy, win);
		}
	}

	fbui_display_close (dpy);
}
