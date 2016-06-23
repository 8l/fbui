
/*=========================================================================
 *
 * fblauncher, a launcher for FBUI (in-kernel framebuffer UI)
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
 */

#include <stdio.h>
#include <sys/param.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#include "libfbui.h"


#define VERSION "0.1"


Font *titlefont, *listfont;
short titlefontheight, listfontheight;


short total_items;
typedef struct item {
	char *s1, *s2;
	struct item *next;
} Item;

Item *items;

Item *Item_new(char *s1, char *s2) 
{
	if (!s1 || !s2) return NULL;

	struct item *nu = (struct item*)malloc(sizeof(struct item));
	if (!nu)
		FATAL("cannot allocate list item");

	nu->next=NULL;
	nu->s1 = strdup(s1);
	nu->s2 = strdup(s2);
	if (!nu->s1 || !nu->s2)
		FATAL("cannot allocate string for item");
	return nu;
}

Item *Item_append_pair (char *s1, char *s2) 
{
	struct item *nu = Item_new (s1,s2);
	if (nu) {
		struct item *i = items;
		if (!i) {
			items = nu;
		} else {
			while (i->next)
				i = i->next;
			i->next = nu;
		}
		++total_items;
	}
	return nu;
}

int readline (FILE *f, char *line)
{
	int ix=0;
	short ch;
	while (EOF != (ch = fgetc (f))) {
		if (ch == '\r')
			continue;
		if (ch == '\n')
			break;
		line[ix++] = ch;
		if (ix==249) break;
	}
	line[ix]=0;
	if (!ix && ch == EOF)
		return -1;
	else
		return ix;
}


int readfile (char *path)
{
	short ch;
	char line[250];
	if (!path) 
		return 0;
	FILE *f = fopen (path, "r");
	if (!f) 
		return 0;

	int count;
	if (readline (f, line) < 1)
		return 0;

	count = atoi (line);

	while(count--) {
		if (-1 == readline (f, line))
			break;

		char *s = strchr (line, ' ');
		if (s) {
			*s = 0;
			s++;

			if (line[0])
				Item_append_pair (line, s);
		}
	}

	fclose (f);
	return 1;
}

char *Item_lookup_n (int n)
{
	Item *im = items;

	while (im && n--)
		im = im->next;

	return im? im->s2 : NULL;
}


static short w = 200;
static short h = 200;

int
main(int argc, char** argv)
{
	int i;
	Display *dpy;
	Window *win;
	char path[MAXPATHLEN];
	struct stat st;
	FILE *f;
	int highlight_which = 0;

	total_items = 0;
	items=NULL;

	strcpy (path, getenv ("HOME"));
	strcat (path, "/.launcher");
	i = stat (path, &st);
	if (i) {
		WARNING ("can't open ~/.launcher; creating...");
		f = fopen (path, "w");
		fprintf (f, "3\n");
		fprintf (f, "Terminal fbterm\n");
		fprintf (f, "Calculator fbcalc\n");
		fprintf (f, "Scribble fbscribble\n");
		fclose (f);
	}

	f = fopen (path, "r");
	if (!f)
		FATAL ("can't open ~/.launcher");
	fclose (f);

	if (!readfile (path))
		FATAL ("error reading ~/.launcher");

        titlefont = font_new ();
        listfont = font_new ();
	if (!titlefont || !listfont)
		FATAL ("cannot allocate Font");

        if (!pcf_read (titlefont, "helvR12.pcf"))
		FATAL ("cannot read Helv bold 12 point");
        if (!pcf_read (listfont, "timR10.pcf"))
		FATAL ("cannot read Times 12 point");

	short indent,a,d;
	font_string_dims (listfont, "88. ", &indent, &a, &d);

	listfontheight = listfont->ascent + listfont->descent;
	titlefontheight = titlefont->ascent + titlefont->descent;

	unsigned long titlecolor = RGB_RED;
	unsigned long listcolor = RGB_BROWN;
	unsigned long titlefontcolor = RGB_YELLOW;
	unsigned long listfontcolor = RGB_ORANGE;
	unsigned long sepcolor = RGB_WHITE;
	unsigned long listhighlightcolor = RGB_LTBROWN;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL("cannot open display");

	w = 150;
	h = 150;
	win = fbui_window_open (dpy, w,h, &w, &h, 9999,9999, 0, -50, 
		&listfontcolor, 
		&listcolor,
		"fblauncher", VERSION, 
		FBUI_PROGTYPE_EPHEMERAL, 
		false,false, 
		-1, 
                true, // need keys
		false,
		false, 
		argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	short x=-1, y=-1;
	short whichlaunch = -1;

	char done=0;
	while(!done) {
		char needdraw=0;
		char fulldraw=0;
		Event ev;
		int err;

		if (err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL)) {
			fbui_print_error (err);
			continue;
		}

		int type = ev.type;
		Window *win2 = ev.win;

		if (win2 != win)
			FATAL ("event's window is not ours");

		switch(type) {
		case FBUI_EVENT_MOTION: 
			x = ev.x;
			y = ev.y;
			if (y >= (titlefontheight + 1)) {
				int which = (y - titlefontheight - 1) / listfontheight;
				if (which > total_items)
					which = total_items;
				if (which != highlight_which) {
					highlight_which = which;
					needdraw = 1;
				}
			}
			break;
	
		case FBUI_EVENT_EXPOSE:
			needdraw=1;
			fulldraw=1;
			break;

		case FBUI_EVENT_ENTER:
			break;

		case FBUI_EVENT_BUTTON:
			if (y < titlefontheight) {
				if (x >= (w - 10))
					done=true;
				continue;
			}

			if ((ev.key & FBUI_BUTTON_LEFT) && (ev.key & 1)) {
				whichlaunch = (y - titlefontheight) / listfontheight;
				done=true;
				continue;
			}
			break;

		case FBUI_EVENT_LEAVE:
			if (highlight_which != -1) {
				highlight_which = -1;
				needdraw=1;
			}
			break;
		case FBUI_EVENT_MOVE_RESIZE:
			w = ev.width;
			h = ev.height;
			break;
		}
		
		if (!needdraw) 
			continue;

		if (fulldraw) {
			char title[200];
			sprintf (title, "Launcher %s",VERSION);

			short wid,a,d;
			font_string_dims (titlefont, title, &wid,&a,&d);
			short title_pos = (w - wid) / 2;

			fbui_fill_area (dpy, win, 0, 0, w-1, titlefontheight, titlecolor);
			fbui_draw_hline (dpy, win, 0, w-1, titlefontheight, sepcolor);
			fbui_draw_string (dpy, win, titlefont, title_pos, 0, 
				title, titlefontcolor);
		}

		fbui_fill_area (dpy, win, 0, titlefontheight+1, w-1, h, listcolor);

		fbui_draw_line (dpy, win, w-10, 2, w-3, 9, RGB_WHITE);
		fbui_draw_line (dpy, win, w-10, 9, w-3, 2, RGB_WHITE);

		struct item *im = items;
		int ix=0;
		while (im) {
			char numstr[5];
			sprintf (numstr, "%d.", ix+1);

			short y = titlefontheight+1+ix*listfontheight;

			if (ix == highlight_which) 
				fbui_fill_area (dpy, win, 0, y, w, y + listfontheight,
					listhighlightcolor);

			fbui_draw_string (dpy, win, listfont, 
				0, titlefontheight+1+ix*listfontheight, 
				numstr, listfontcolor);
	
			fbui_draw_string (dpy, win, listfont, 
				indent, y, im->s1, listfontcolor);

			im = im->next;
			ix++;
		}

		fbui_flush (dpy, win);
	}

	fbui_window_close (dpy, win);
	fbui_display_close (dpy);

	if (-1 != whichlaunch) {
		char *launch = Item_lookup_n (whichlaunch);
		if (launch)
			system (launch);
	}

	return 0;
}
