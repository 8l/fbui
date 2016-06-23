
/*=========================================================================
 *
 * fbstart, a simple button FBUI program to launch another FBUI program
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
#include <stdlib.h>
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


int
main(int argc, char** argv)
{
	Display *dpy;
	Window *win;
	Font *pcf = font_new ();
	u32 fg,bg;
	char *program = "fblauncher";

	if (!pcf_read (pcf, "ncenBI14.pcf")) {
		font_free (pcf);
		FATAL ("cannot load font");
	}

	short line_height = pcf->ascent + pcf->descent;

	fg = 0x5000;
	bg = 0x40ff40;

	short maxwidth, a,d;
	font_string_dims (pcf, "~Start~", &maxwidth, &a, &d);

	short win_w, win_h;
	dpy = fbui_display_open ();
	if (!dpy) 
		FATAL ("cannot open display");

	win = fbui_window_open (dpy, maxwidth,50, &win_w, &win_h, maxwidth, line_height*2,
		0, -1, 
		&fg, &bg, 
		"fbstart", "", 
		FBUI_PROGTYPE_LAUNCHER, 
		false,false, 
		-1,
		false, // doesn't need keys: buttons only
		false, 
		false, 
		argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	while (1) {
		int need_redraw=0;
		Event ev;

		int err;
		if (err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL))
		{
			if (err != FBUI_ERR_NOEVENT)
				fbui_print_error (err);
			continue;
		}

		int num = ev.type;

		switch (num) {
		case FBUI_EVENT_EXPOSE:
			need_redraw=1;
			break;
		
		case FBUI_EVENT_MOVE_RESIZE:
			win_w = ev.width;
			win_h = ev.height;
			need_redraw=1;
			break;
		
		case FBUI_EVENT_BUTTON:
			if (ev.key & FBUI_BUTTON_LEFT) {
				if (ev.key & 1) {
					if (!fork()) {
						system (program);
						goto done;
					}
				}
			}
			break;
		}

		if (!need_redraw)
			continue;

		char text[100];
		sprintf (text, "Start");

		short w,a,d;
		font_string_dims(pcf,text,&w,&a,&d);

		int x, y;
		x = (win_w - w) / 2;
		if (d < 0)
			d = -d;
		y = (win_h - (a+d)) / 2;

		fbui_clear (dpy, win);
		fbui_draw_string (dpy, win, pcf, x,y, text, fg);
	}

done:
	fbui_window_close(dpy, win);
	fbui_display_close (dpy);
}
