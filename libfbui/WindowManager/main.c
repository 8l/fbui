
/*=========================================================================
 *
 * fbwm, a window manager for FBUI (in-kernel framebuffer UI)
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

/*
 * Changes:
 *
 * Oct 19, 2004, fbui@comcast.net: got rectangle subtraction working
 * Oct 19, 2004, fbui@comcast.net: got background image drawing working
 * Dec 31, 2004, fbui@comcast.net: moved rectangle code to separate file
 */



#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <jpeglib.h>
#include <signal.h>


#include "libfbui.h"


short win_w, win_h;
#define STEELBLUE 0x4682B4

extern JSAMPLE * image_buffer;
extern int image_height;
extern int image_width;
extern int image_ncomponents;

extern int read_JPEG_file (char*);

extern char grayscale; /* 1 forces bg image to grayscale */

struct fbui_wininfo info[200];
struct fbui_wininfo prev[200];
int window_count;


extern void
draw_image (Display *dpy, Window *win, short x0, short y0, short x1, short y1);

extern void
draw_background (Display *dpy, Window *win, struct fbui_wininfo *, int);


void
update_focus (Display *dpy, Window *self)
{
	int i;
	for (i=0; i<window_count; i++) {
		if (info[i].program_type == FBUI_PROGTYPE_APP)
			fbui_assign_keyfocus (dpy, self, info[i].id);
	}
}


int
main(int argc, char** argv)
{
	int mypid,i,j;
	Display *dpy;
	Window *self;
        Font *pcf;
	short line_height;

	grayscale=0;

        pcf = font_new ();
	if (!pcf)
		FATAL ("out o' memory");

        if (!pcf_read (pcf, "timR12.pcf")) {
                font_free (pcf);
        	FATAL ("cannot read Times 12");
        }

	line_height = pcf->ascent + pcf->descent;

	long fg,bg=0x404040;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL ("request for control denied");

	self = fbui_window_open (dpy, 1,1, &win_w, &win_h, 9999,9999,
		0, 0, 
		&fg, &bg, 
		"fbwm", "", 
		FBUI_PROGTYPE_WM,
		true,  // we are a window manager
		false, // but we are not autopositioning anything
		-1,
		false, // we don't need keys
		false, // don't need all motion
		false, // not hidden
		argc,argv);
        if (!self)
                FATAL ("cannot open manager window");

	/* Check for a background image */
	image_width=0;
	image_height=0;
	image_ncomponents=0;
	image_buffer=NULL;
	char *image_path=NULL;
	i=1;
	while (i<argc) {
		char *s = argv[i];
		int ch = s ? s[0] : 0;
		if (ch && '-' != ch) {
			image_path = argv[i];
			break;
		}
		i++;
	}
	if (image_path) {
		int result;
		result = read_JPEG_file (image_path);
                if (image_ncomponents != 3)
			result=0;
		if (!result)
			FATAL ("cannot read background image");

	}

	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '1', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '2', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '3', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '4', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '5', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '6', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '7', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '8', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '9', 1))
		FATAL ("cannot register accelerator");
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '0', 1))
		FATAL ("cannot register accelerator");

	/* Alt-tab switches between apps
	 */
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '\t', 1))
		FATAL ("cannot register accelerator");

	/* Alt-backsp shuts down FBUI on the current VC
	 */
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, '\b', 1))
		FATAL ("cannot register accelerator");

	int need_list = 0;
	int need_redraw = 0;

	while(1) {
		Event ev;
		int err;
		if (err = fbui_wait_event (dpy, &ev,
                                FBUI_EVENTMASK_ALL - FBUI_EVENTMASK_KEY))
		{
			fbui_print_error (err);
			continue;
		}
		int num = ev.type;
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

		if (ev.win != self) {
printf ("ev.win=%08x\n", (unsigned long) ev.win);
			FATAL ("got event for another win");
		}

		if (num == FBUI_EVENT_EXPOSE) {
			need_redraw = 1;
			need_list = 1;
		}
		else if (num == FBUI_EVENT_WINCHANGE) {
			need_list = 1;
		}
		else if (num == FBUI_EVENT_ACCEL) {
			short key = ev.key;
printf ("FBWM accel is %d (0x%04x)\n", key,key);
			if (key == '\b') {
printf ("FBWM got alt-backspace\n");
				goto done;
			}

			if (key == '\t') {
				continue;
			}
		}

		if (need_list) {
			int i;
			int prev_count = window_count;

			need_redraw = 1;

			for (i=0; i<window_count; i++)
				prev[i] = info[i];

			window_count = fbui_window_info (dpy, self, &info[0], 200);

			/* determine which windows have disappeared */
			for (i=0; i<window_count; i++) {
				int missing=1;
				int j=0;
printf ("%d: window id %d, %s width %d height %d\n", 
	i, info[i].id, info[i].name, info[i].width, info[i].height);
				while (j < window_count) {
					if (prev[i].pid == info[j].pid) {
						missing=0;
						break;
					}
					j++;
				}
				if (missing) {
					draw_image (dpy, self,
					  prev[i].x,
					  prev[i].y - line_height,
					  prev[i].x + prev[i].width - 1,
					  prev[i].y - 1);
					fbui_flush(dpy, self);
				}
			}

			update_focus (dpy, self);

			need_list=0;
		} 
		
		if (need_redraw) {
			int i;

			need_redraw = 0;

			// Fill in the gaps
			draw_background (dpy, self, info, window_count);

			i=0; 
			while (i < window_count) {
				char tmp[100];
				struct fbui_wininfo *wi = &info[i];
				int x,y;

				// locate window
				x = wi->x;
				y = wi->y;

				// draw gradient behind text
				int j=0;
				while (j < wi->width) {
					unsigned long r,g,b;
					int k = (255 - j/2) > 1 ? (255 - j/2) : 0;
					r = 30;
					g = 32 + (7*k)/8;
					b = k/3;
					r <<= 16;
					b <<= 8;
					fbui_draw_vline (dpy, self, x+j, y-1, y-line_height,
						r|g|b);
					j++;
				}

				// draw program name/pid
				sprintf(tmp,"%s (pid=%d)", wi->name,wi->pid);
				fbui_draw_string (dpy, self, pcf,x,y-line_height,tmp, RGB_YELLOW);

				i++;
			}
		}
	}

done:
	mypid = getpid();
	for (i=0; i<window_count; i++) {
		if (info[i].pid != mypid)
			kill (info[i].pid, SIGTERM);
	}

	fbui_display_close (dpy);
	return 0;
}
