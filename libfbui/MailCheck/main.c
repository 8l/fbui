
/*=========================================================================
 *
 * fbcheck, a POP3 mail checker for FBUI (in-kernel framebuffer UI)
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


int
main(int argc, char** argv)
{
	Display *dpy;
	Window *win;
	Font *pcf = font_new ();
	u32 fg,bg;
	char *server, *user, *pass;

	char tmp[300];
	sprintf (tmp, "%s/.fbcheckrc", getenv("HOME"));
	FILE *f = fopen (tmp,"r");
	if (!f)
		FATAL("cannot open configuration file (~/.fbcheckrc)");
	if (!fgets (tmp, 300, f))
		FATAL("cannot read configuration file (~/.fbcheckrc)");
	server = strdup(tmp);
	if (!fgets (tmp, 300, f))
		FATAL("cannot read configuration file (~/.fbcheckrc)");
	user = strdup(tmp);
	if (!fgets (tmp, 300, f))
		FATAL("cannot read configuration file (~/.fbcheckrc)");
	pass = strdup(tmp);

	if (!pcf_read (pcf, "timR10.pcf")) {
		font_free (pcf);
		FATAL ("cannot load font");
	}

	short line_height = pcf->ascent + pcf->descent;

	fg = RGB_YELLOW;
	bg = RGB_BROWN;

	short win_w, win_h;
	dpy = fbui_display_open ();
	if (!dpy) 
		FATAL ("cannot open display");

	win = fbui_window_open (dpy, 150,50, &win_w, &win_h, 250, 50,
		300, -1, 
		&fg, &bg, 
		"fbcheck", "", 
		FBUI_PROGTYPE_LAUNCHER, false,false, -1,false, 
		false, false, argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	time_t t0 = time(NULL);
	time_t t = t0 + 999;
	while (1) {
		int total;
		int need_redraw=0;
		Event ev;

		usleep(50000);

		if ((t - t0) >= 60) {
			total = checkmail (server,user,pass);
			t0 = t = time(NULL);
			need_redraw=1;
		}

		int err;
		if (err = fbui_poll_event (dpy, &ev,
                                FBUI_EVENTMASK_ALL - FBUI_EVENTMASK_KEY))
		{
			if (err != FBUI_ERR_NOEVENT)
				fbui_print_error (err);
			continue;
		}
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

		int num = ev.type;

		switch (num) {
		case FBUI_EVENT_EXPOSE:
			need_redraw=1;
			break;
		
		case FBUI_EVENT_MOVE_RESIZE:
			win_w = ev.width;
			win_h = ev.height;
			break;
		
		}

		if (!need_redraw)
			continue;

		int underline = 0;
		char text[200];
		if (total < 0) {
			sprintf (text, "Mailserver unreachable");
		} else if (total > 0) {
			sprintf (text, "Messages: <<%d>>", total);
			underline = 1;
		} else {
			sprintf (text, "~ No messages ~");
		}

		short w,a,d;
		font_string_dims(pcf,text,&w,&a,&d);
printf ("mc str dims %d %d %d\n",w,a,d);

		int x, y;
		x = (win_w - w) / 2;
		if (d < 0)
			d = -d;
		y = (win_h - (a+d)) / 2;

		fbui_clear (dpy, win);
		fbui_draw_string (dpy, win, pcf, x,y, text, fg);
		if (underline)
			fbui_draw_hline (dpy, win, x, x+w-1, 2 + y + pcf->ascent, fg);
		fbui_flush (dpy, win);
	}

	fbui_window_close(dpy, win);
	fbui_display_close (dpy);

	free(server);
	free(user);
	free(pass);
}
