
/*=========================================================================
 *
 * fbclock, an analog clock for FBUI (in-kernel framebuffer UI)
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


/* Changes
 *
 * Sept 26, 2004: responds to geometry changes.
 * Sept 30, 2004: lib changes.
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "libfbui.h"


static short w = 200;
static short h = 200;

int getpos (int deg, int *xp, int *yp, int border)
{
	double radians;
	double x,y,xradius,yradius;

	radians = deg;
	radians /= 360.0;
	radians *= 2.0 * 3.14159265;

	xradius = w / 2 - border;
	yradius = h / 2 - border;
	x = sin (radians) * xradius;
	y = cos (radians) * yradius;
	x += w/2;
	y = h/2 - y;

	*xp = x;
	*yp = y;
}

int
main(int argc, char** argv)
{
	int i;
	Display *dpy;
	Window *win;

	unsigned long markercolor = 0x00ff00;
	unsigned long centercolor = 0xffff00;
	unsigned long handcolor = 0x00ffff;
	unsigned long gemcolor = 0xffffff;
	unsigned long bgcolor = 0x2000;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL("cannot open display");

	w = 120;
	h = 120;
	win = fbui_window_open (dpy, w,h, &w, &h, 9999,9999, 0, -1, 
		&handcolor, 
		&bgcolor,
		"fbclock", "", 
		FBUI_PROGTYPE_TOOL, false, false, -1, false,false,false, argc,argv);
	if (!win) 
		FATAL ("cannot create window");

printf ("our window is id %d\n", win->id);

	time_t t0 = time(0);
	while(1) {
		time_t t;
		int need=0, mustclear=0;
		int size;
		usleep (500000);

		t = time(0);
		int tdiff = t - t0;
		if (tdiff >= 1) {
			need=1;
			mustclear=1;
			t0 = t;
		}

		if (!need) {
			Event ev;
			Window *win2;
			int err;

			if (err = fbui_poll_event (dpy, &ev,
			     		     FBUI_EVENTMASK_ALL & 
					     ~(FBUI_EVENTMASK_KEY | FBUI_EVENTMASK_MOTION)))
			{
				if (err != FBUI_ERR_NOEVENT)
					fbui_print_error (err);
				continue;
			}

			int num= ev.type;
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

			win2 = ev.win;
			if (win2 != win) {
printf ("ev.win=%08lx\n", (unsigned long)win2);
				FATAL ("event's window is not ours");
			}

			if (num == FBUI_EVENT_EXPOSE)
				need=1;
			else
			if (num == FBUI_EVENT_ENTER) {
				printf ("fbclock got Enter\n");
				continue;
			}
			else
			if (num == FBUI_EVENT_LEAVE) {
				printf ("fbclock got Leave\n");
				continue;
			}
			else
			if (num == FBUI_EVENT_MOVE_RESIZE) {
				w = ev.width;
				h = ev.height;
				need=1;
			}
		}

		if (!need) 
			continue;

		struct tm *tmp = localtime (&t);
		int hour = tmp->tm_hour;
		int minute = tmp->tm_min;
		int second = tmp->tm_sec;

		if (mustclear)
			fbui_clear (dpy, win);

		int shorter_side = w < h ? w : h;

		int deg;
		for (deg=0; deg < 360; deg += 6) {
			int x,y;
			getpos (deg, &x,&y, 5);
			
			int size=0;
			if ((deg % 90) == 0)
				size=2;
			else
			if ((deg % 30) == 0)
				size=1;
			fbui_fill_area (dpy, win, x-size, y-size, x+size, y+size, markercolor);
		}

		int x,y;
		float p = minute;
		p /= 60.0;
		p *= 30;
		getpos ((hour%12)*30 + (int)p, &x,&y, shorter_side/4);
		fbui_draw_line (dpy, win, w/2,h/2,x,y, handcolor);
		fbui_draw_line (dpy, win, w/2+1,h/2,x+1,y, handcolor);
		fbui_draw_line (dpy, win, w/2,h/2+1,x+1,y+1, handcolor);
		
		getpos (minute*6, &x,&y, 15);
		fbui_draw_line (dpy, win, w/2,h/2,x,y, handcolor);
		
		getpos (second*6, &x,&y, 10);
		fbui_draw_point (dpy, win, x,y-2, gemcolor);
		fbui_draw_hline (dpy, win, x-1,x+1,y-1, gemcolor);
		fbui_draw_hline (dpy, win, x-2,x+2,y, gemcolor);
		fbui_draw_hline (dpy, win, x-1,x+1,y+1, gemcolor);
		fbui_draw_point (dpy, win, x,y+2, gemcolor);
		
		size=3;
		fbui_fill_area (dpy, win, w/2-size, h/2-size, w/2+size, h/2+size, centercolor);

		fbui_flush (dpy, win);
	}

	fbui_window_close(dpy, win);
	fbui_display_close(dpy);
	return 0;
}
