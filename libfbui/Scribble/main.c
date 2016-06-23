
/*=========================================================================
 *
 * fbscribble, a simple drawing program
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
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <string.h>

#include <linux/vt.h>
#include <linux/input.h>

#include "libfbui.h"


typedef struct {
	short width, height;
	short depth; // allowed: 24 or 48
	char *path;
	char *title;
	char *creator;
	char *comment;

	unsigned char *pixels;
} Canvas;


Canvas *Canvas_create (short w, short h, short d)
{
	Canvas *nu;
	unsigned char *pixels;

	if (w<0 || h<0)
		return NULL;
	if (d!=24 && d!=48)
		return NULL;

	int size = w * h * (d==24 ? 3 : 6);
	pixels = (unsigned char*) malloc(size);
	if (!pixels)
		FATAL ("out of memory");
	memset ((void*)pixels, 0xffffff, size);

	nu = (Canvas*) malloc(sizeof(Canvas));
	if (!nu)
		FATAL ("out of memory");

	memset ((void*)nu, 0, sizeof(Canvas));
	nu->width = w;
	nu->height = w;
	nu->depth = d;
	nu->pixels = pixels;
	return nu;
}

void Canvas_put_pixel (Canvas *canvas, short x, short y, unsigned long pixel)
{
	if (!canvas)
		return;
	if (canvas->depth != 24)
		return;
	if (x < 0 || y < 0 || x>=canvas->width || y>=canvas->height)
		return;
	if (!canvas->pixels)
		FATAL ("Canvas lacks pixmap");
	/*----------------*/

	unsigned long ix = (x + y*canvas->width) * (canvas->depth/8);
	canvas->pixels [ix++] = pixel >> 16;
	canvas->pixels [ix++] = pixel >> 8;
	canvas->pixels [ix] = pixel;
}

void Canvas_clear (Canvas *canvas)
{
	if (!canvas)
		return;
	if (!canvas->pixels)
		return;
	/*----------------*/

	int size = canvas->width * canvas->height * (canvas->depth==24 ? 3 : 6);
	memset ((void*)canvas->pixels, 0xffffff, size);
}

void Canvas_draw (Canvas *canvas, Display* dpy, Window *win, int zoom)
{
	if (!canvas || !dpy || !win)
		return;

	if (zoom != 1)
		return;

	fbui_fill_area (dpy, win, 0,0, win->width-1, win->height-1, 0xB0B0B0);

	int x,y;
	int bytes_per_pixel = canvas->depth / 8;

	int x0 = (win->width - canvas->width) >> 1;
	int y0 = (win->height - canvas->height) >> 1;

	for (y=0; y<canvas->height; y++) {
		for (x=0; x<canvas->width; x++) {
			unsigned long ix = (x + y*canvas->width) * bytes_per_pixel;
			unsigned long pixel;
			pixel = canvas->pixels [ix++];
			pixel <<= 8;
			pixel |= canvas->pixels [ix++];
			pixel <<= 8;
			pixel |= canvas->pixels [ix];

			fbui_draw_point (dpy, win, x0 + x, y0 + y, pixel);
		}
	}
}

int Canvas_deserialize (Canvas *canvas, char *path)
{
	FILE *f;

	if (!canvas || !path)
		FATAL ("null ptr param");
	/*----------------*/

	f = fopen (path, "r");
	if (!f)
		return false;

	int a,b,c;
	if (3 != fscanf (f, "%d %d %d\n", &a,&b,&c)) {
		fclose (f);
		return false;
	}

	if (a < 0 || b < 0 || (c != 24 && c != 48))
		return false;

	if (canvas->pixels)
		free (canvas->pixels);

	int bytes_per_pixel = c/8;

	int size = a * b * bytes_per_pixel;
	unsigned char *pixels = (unsigned char*) malloc(size);
	if (!pixels)
		FATAL ("out of memory");
	memset ((void*)pixels, 0xffffff, size);

	canvas->pixels = pixels;
	canvas->width = a;
	canvas->height = b;
	canvas->depth = c;

	int x,y;
	for (y=0; y<canvas->height; y++) {
		for (x=0; x<canvas->width; x++) {
			unsigned long value;

			if (1 != fscanf (f, "%06lx", &value)) {
				fclose(f);
				return false;
			}

			unsigned long r,b,g;
			r = 0xff & (value >> 16);
			g = 0xff & (value >> 8);
			b = 0xff & value;

			unsigned long ix = (x + y*canvas->width) * bytes_per_pixel;
			pixels [ix++] = r;
			pixels [ix++] = g;
			pixels [ix] = b;
		}
	}

	fclose(f);
	return true;
}

int Canvas_serialize (Canvas *canvas, char *path)
{
	FILE *f;

	if (!canvas || !path)
		FATAL ("null ptr param");
	/*----------------*/

	f = fopen (path, "w");
	if (!f)
		return false;

	fprintf (f, "%d %d %d\n", canvas->width, canvas->height, canvas->depth);

	int x,y;
	int bytes_per_pixel = canvas->depth / 8;

	for (y=0; y<canvas->height; y++) {
		for (x=0; x<canvas->width; x++) {
			unsigned long ix = (x + y*canvas->width) * bytes_per_pixel;
			unsigned long pixel;
			pixel = canvas->pixels [ix++];
			pixel <<= 8;
			pixel |= canvas->pixels [ix++];
			pixel <<= 8;
			pixel |= canvas->pixels [ix];

			fprintf (f, "%06lx ", pixel);
		}
		fprintf (f,"\n");
	}
	fclose(f);
}

int
main(int argc, char** argv)
{
	Display *dpy;
	Window *win;
	u32 fg,bg;
	int i;
	int vc=-1;
	char button_down = 0;

	short canvas_w, canvas_h;

	Canvas *canvas = Canvas_create (240,240,24);

	if (Canvas_deserialize (canvas, "image"))
		printf ("fbscribble: successfully read image\n");
	else {
		printf ("fbscribble: failed to read image\n");
		Canvas_clear (canvas);
	}

	fg = RGB_BLACK;
	bg = RGB_WHITE;

	short win_w, win_h;
	dpy = fbui_display_open ();
	if (!dpy) 
		FATAL ("cannot open display");

	win_w = 320;
	win_h = 320;

	canvas_w = win_w;
	canvas_h = win_h;

	win = fbui_window_open (dpy, win_w, win_h, 
		&win_w, &win_h, 999,999,
		0, 0,
		&fg, &bg, 
		"fbscribble", "", 
		FBUI_PROGTYPE_APP, 
		false,false, 
		vc,
		true,  // need keys
		false, // all motion
		false, // not hidden
		argc, argv);

	int x0 = (win->width - canvas->width) >> 1;
	int y0 = (win->height - canvas->height) >> 1;

	while (1) {
		int total;
		int need=0;
		Event ev;
		Window *win;
		int x=0, y=0;
		int type;
		int err;

		if (err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL)) {
			fbui_print_error (err);
			continue;
		}

printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

		win = ev.win;
		if (!win)
			FATAL("null window");

		type = ev.type;

		switch (type) {
		case FBUI_EVENT_EXPOSE:
			Canvas_draw (canvas, dpy, win, 1);
			continue;

		case FBUI_EVENT_KEY: {
			short key = ev.key >> 2;
			short state = ev.key & 3;
printf ("key=%d\n", key);

			if (state && key == KEY_Q)
				goto finit;

			if (state && key == KEY_S)
				Canvas_serialize (canvas, "image");

			if (state && key == KEY_L)
				Canvas_deserialize (canvas, "image");
			break;
		 }

		case FBUI_EVENT_BUTTON:
printf ("got button event, value=%d\n", ev.key);
			if (ev.key & FBUI_BUTTON_LEFT)
				button_down = ev.key & 1;
			break;
		
		case FBUI_EVENT_MOTION: {
			short x= ev.x - x0;
			short y= ev.y - y0;

			if (button_down && x >= 0 && y >= 0 && 
			    x < canvas->width && y < canvas->height) 
			{
				Canvas_put_pixel (canvas, x, y, RGB_RED);
				fbui_draw_point (dpy, win, ev.x, ev.y, RGB_RED);
				fbui_flush (dpy, win);
			}
			break;
		 }
		
		case FBUI_MOVE_RESIZE:
			x0 = (ev.width - canvas->width) >> 1;
			y0 = (ev.height - canvas->height) >> 1;
			break;
		}
	}
finit:

	fbui_display_close (dpy);
}
