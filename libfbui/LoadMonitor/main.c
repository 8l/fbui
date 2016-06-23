
/*=========================================================================
 *
 * fbload, a load monitor for FBUI (in-kernel framebuffer UI)
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
 * Sept 26, 2004: responds to geo changes.
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "libfbui.h"



float getload ()
{
	float a,b,c;
	FILE *f = fopen ("/proc/loadavg", "r");
	if (3 != fscanf (f, "%f %f %f", &a, &b, &c))
		a = 0.0;
	fclose (f);
	return a;
}


int
main(int argc, char** argv)
{
	int i;
	Display *dpy;
	Window *win;
	int seconds_per = 5;

	if (argc >= 2 && isdigit(*argv[1]) && '0'!=isdigit(*argv[1]))
		seconds_per = atoi (argv[1]);

	short w = 50;
	short h = 120;
	float *values;
	unsigned long fgcolor = RGB_GREEN;
	unsigned long bgcolor = 0x2000;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL ("cannot open display");

	win = fbui_window_open (dpy, w,h, &w,&h, 9999,9999, -1, -1, 
		&fgcolor, &bgcolor, "fbload", "", 
		FBUI_PROGTYPE_TOOL,false,false, -1,false, 
		false, false, argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	int last_x = w - 1;
	int last_y = h - 1;

	values = (float*) malloc (w * sizeof(float));
	if (!values)
		FATAL("out of memory");

	for (i=0; i<w; i++)
		values[i] = 0.0;

	unsigned long barcolor = fgcolor;

	int range=0;
	time_t t0 = time(NULL);
	while(1) {
		time_t t;
		int need=0;
		int need_redraw=0;
		usleep (50000);

		t = time(NULL);
		int tdiff = t - t0;
		if (tdiff >= seconds_per) {
			need=1;
			t0 = t;
		}

		if (!need) {
			Event ev;
			int err;
			if (err = fbui_poll_event (dpy, &ev,
					     FBUI_EVENTMASK_ALL & ~FBUI_EVENTMASK_KEY))
			{
				if (err != FBUI_ERR_NOEVENT)
					fbui_print_error (err);
				continue;
			}
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

			int num = ev.type;
			char key = ev.key;

			if (ev.win != win)
				FATAL ("event for wrong window");

			if (num == FBUI_EVENT_EXPOSE)
				need_redraw = need = 1;
			else if (num == FBUI_EVENT_MOVE_RESIZE) {
				w = ev.width;
				h = ev.height;
				need = 1;
			}
			if (num == FBUI_EVENT_ENTER) {
				printf ("fbload got Enter\n");
				continue;
			}
			if (num == FBUI_EVENT_LEAVE) {
				printf ("fbload got Leave\n");
				continue;
			}

		}

		if (!need) 
			continue;

		/* shift values to left */
		for (i=1; i<w; i++)
			values[i-1] = values[i];

		int nu = values[w-1] = getload();
		nu++;
		if (nu > range)
			need_redraw = 1;

		/* get max value */
		float max = 0;
		for (i=0; i<w; i++) {
			if (max < values[i])
				max = values[i];
		}

		range = max;
		range++;
		if (!range)
			range = 1;

		fbui_clear (dpy, win);
		for (i=0; i<w; i++)
			fbui_draw_vline (dpy, win, i, 
				h - ((float)h/range) * values[i], h, barcolor);

		if (range > 1) {
			for (i=0; i<range; i++) {
				int y = h/range;
				y *= i;
				y = h - y;
				fbui_draw_hline (dpy, win, 0, last_x, y, RGB_YELLOW);
			}
		}
		fbui_flush(dpy, win);
	}

	fbui_window_close (dpy, win);
	fbui_display_close (dpy);
	return 0;
}
