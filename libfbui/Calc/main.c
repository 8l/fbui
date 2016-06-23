
/*=========================================================================
 *
 * fbcalc, a simple calculator for FBUI (in-kernel framebuffer UI)
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


#define VERSION "0.2"


#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "libfbui.h"


static short win_w;
static short win_h;

static short button_width;
static short button_height;

static int rows = 6;
static int cols = 4;

static Display *dpy = NULL;
static Window *win = NULL;
static Font *font = NULL;

static unsigned long fgcolor = RGB_BLACK;
static unsigned long bgcolor = 0xC0C0C0;

static char diverr = false;

static char *keystring[] = {
	"STO", "RCL", "CLR", "CE",
	"1/x", "sqrt", "x^2", "/",
	"7", "8", "9", "*",
	"4", "5", "6", "-",
	"1", "2", "3", "+",
	"0", ".", "-", "=",
	NULL

};

static char begin_new_value = false;

static double stored = 0.0;
static double result = 0.0;
static double value = 0.0;

static char got_point = 0;
static char rightgoing_place = 0;

static int operation = 0;


int whichbutton (short x, short y)
{
	int i,j;

	for (j=0; j<rows; j++) {
		for (i=0; i<cols; i++) {
			int x0 = i*button_width + (i+1) * button_width/2;
			int y0 = (j+1)*button_height + (j+2) * button_height/2;
			int x1 = x0 + button_width - 1;
			int y1 = y0 + button_height - 1;

			if (x >= x0 && x <= x1 && y >= y0 && y <= y1)
				return i + j*cols;
		}
	}
	return -1;
}


void draw_button (Display *dpy, Window *win, int i, int j, char highlight)
{
	if (!dpy || !win)
		return;
	if (i<0 || j<0 || i>=cols || j>=rows)
		return;

	int x0 = i*button_width + (i+1) * button_width/2;
	int y0 = (j+1)*button_height + (j+2) * button_height/2;
	int x1 = x0 + button_width - 1;
	int y1 = y0 + button_height - 1;

	if (highlight)
		fbui_fill_area (dpy, win, x0,y0,x1,y1, RGB_CYAN);

	fbui_draw_rect (dpy, win, x0,y0,x1,y1, RGB_BLACK);

	char *str = keystring [i + j*cols];

	short w,a,d,h;
	font_string_dims (font,str,&w,&a,&d);
	h = a+d;
	w = (button_width-w)/2;
	h = (button_height-h)/2;

	fbui_draw_string (dpy,win,NULL,
		w+x0+1,h+y0+1, str, fgcolor);
}


void update_value ()
{
	int x0 = button_width/2;
	int y0 = button_height/2;
	int x1 = win_w - 1 - x0;
	int y1 = y0 + button_height - 1;
	fbui_fill_area (dpy,win,x0,y0,x1,y1,RGB_WHITE);
	fbui_draw_rect (dpy,win,x0,y0,x1,y1,fgcolor);
	char valstr[100];

	if (diverr)
		sprintf (valstr, "Divide Error");
	else
		sprintf (valstr, "%.12g", value);

	short w,a,d,h;
	font_string_dims (font,valstr,&w,&a,&d);
	h = a+d;
	w = (win_w - w - button_width);
	h = (button_height-h)/2;
	fbui_draw_string (dpy,win,NULL,
		w+x0,h+y0+1, valstr, fgcolor);
}


void
insert_digit (char ch)
{
	if (ch < '0' || ch > '9')
		return;

	if (begin_new_value)
		value = 0.0;
	begin_new_value = false;

	ch -= '0';

	if (!got_point) {
		value *= 10.0;
		value += ch;
		update_value ();
	} else {
		int i = rightgoing_place;
		double tmp = ch;
		while(i--)
			tmp /= 10.0;
		tmp /= 10.0;
		++rightgoing_place;
		value += tmp;
	}
	update_value();
}

enum {

	ADD = 1,
	SUBTRACT = 2,
	MULTIPLY = 3,
	DIVIDE = 4

};


void perform_equals ()
{
printf ("____________\n");
printf ("value=%g\n", value);
printf ("result=%g\n", result);
printf ("operation=%d\n", operation);
	switch (operation) {
	case ADD: 
		result += value; 
		break;
	case SUBTRACT: 
		result -= value; 
		break;
	case MULTIPLY: 
		result *= value; 
		break;
	case DIVIDE: 
		if (value != 0)
			result /= value; 
		else
			diverr = true;
		break;
	}
	operation = 0;
	value = result; 
	update_value(); 
	begin_new_value = true;
}

void perform_neg()
{
	value = -value;
	update_value();
}

void perform_ix()
{
	if (!value)
		diverr=true;
	else
		value = 1.0/value;
	update_value();
}

void perform_sqrt()
{
	value = sqrt(value);
	update_value();
}

void perform_square()
{
	value = value*value;
	update_value();
}

void perform_point()
{
	if (!got_point) {
		got_point = true;
		rightgoing_place = 0;
	}
}

void perform_store()
{
	stored = value; 
	got_point = false;
	begin_new_value = true; 
}

void perform_recall()
{
	value = stored; 
	got_point = false;
	begin_new_value = true; 
	update_value(); 
}

void perform_clear()
{
	value = 0.0; 
	got_point = false;
	diverr = false;
	update_value(); 
}
void perform_clearall()
{
	value = result = stored = 0.0; 
	got_point = false;
	diverr = false;
	update_value(); 
}

void perform_div()
{
	operation = DIVIDE; 
	got_point = false;
	result = value;
	begin_new_value = true; 
}

void perform_sub()
{
	operation = SUBTRACT; 
	got_point = false;
	result = value;
	begin_new_value = true; 
}

void perform_mult()
{
	operation = MULTIPLY; 
	got_point = false;
	result = value;
	begin_new_value = true; 
}

void perform_add()
{
	operation = ADD; 
	got_point = false;
	result = value;
	begin_new_value = true; 
}

int
main(int argc, char** argv)
{
	int i;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL("cannot open display");

	win_w = 200;
	win_h = 180;
	win = fbui_window_open (dpy, win_w,win_h, &win_w, &win_h, 9999,9999, 0, -1, 
		&fgcolor, 
		&bgcolor,
		"fbcalc", VERSION, 
		FBUI_PROGTYPE_EPHEMERAL, 
		false, false, 
		-1, 
		true, // need keys
		false,
		false, 
		argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	font = font_new ();

	if (win_w < 200) {
		if (!pcf_read (font, "courR10.pcf")) {
			font_free (font);
			FATAL ("cannot load font");
		}
	} else {
		if (!pcf_read (font, "courR12.pcf")) {
			font_free (font);
			FATAL ("cannot load font");
		}
	}

	fbui_set_font (dpy,win,font);

	double tmp1,tmp2;
	tmp1 = cols + (cols + 1.0)/2.0;
	tmp2 = 1.0 + rows + (rows + 2.0)/2.0;
	button_width = win_w / tmp1;
	button_height = win_h / tmp2;

	got_point = false;

	short current_x = -1, current_y = -1;

	while(1) {
		char need_draw=0;
		char full_draw=0;

		Event ev;
		int err;
		if (err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL ))
		{
			if (err != FBUI_ERR_NOEVENT)
				fbui_print_error (err);
			continue;
		}

		Window *win2;

		int num= ev.type;

		win2 = ev.win;
		if (win2 != win)
			FATAL ("event's window is not ours");

		if (num == FBUI_EVENT_EXPOSE) {
			need_draw=1;
			full_draw=1;
		} else
		if (num == FBUI_EVENT_MOTION) {
			current_x = ev.x;
			current_y = ev.y;
		} else
		if (num == FBUI_EVENT_ENTER) {
			fbui_draw_rect (dpy, win, 0,0, win_w-1, win_h-1, RGB_RED);
			fbui_draw_rect (dpy, win, 1,1, win_w-2, win_h-2, RGB_RED);
			fbui_flush (dpy, win);
			continue;
		}
		else
		if (num == FBUI_EVENT_LEAVE) {
			fbui_draw_rect (dpy, win, 0,0, win_w-1, win_h-1, bgcolor);
			fbui_draw_rect (dpy, win, 1,1, win_w-2, win_h-2, bgcolor);
			fbui_flush (dpy, win);
			continue;
		}
		else
		if (num == FBUI_EVENT_MOVE_RESIZE) {
			win_w = ev.width;
			win_h = ev.height;
			need_draw=1;
		}
		else
		if (num == FBUI_EVENT_KEY) {
			short key = fbui_convert_key (dpy, ev.key);
			if (!key)
				continue;

			if (key=='q')
				break;

			switch (key) {
			case '.': perform_point(); break;
			case '/': perform_div(); break;
			case '=': perform_equals(); break;
			case '*': perform_mult(); break;
			case '+': perform_add(); break;
			case '-': perform_sub(); break;
			case 'c': perform_clear(); break;
			case 's': perform_store(); break;
			case 'r': perform_recall(); break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				insert_digit (key);
				continue;
			}

		}
		else
		if (num == FBUI_EVENT_BUTTON) {
			if (ev.key & FBUI_BUTTON_LEFT) {
				if (ev.key & 1) {
					int n = whichbutton (current_x, current_y);
					switch (n) {
					case 0: perform_store(); break;
					case 1: perform_recall(); break;
					case 2: perform_clearall(); break;
					case 3: perform_clear(); break;

					case 4: perform_ix(); break;
					case 5: perform_sqrt(); break;
					case 6: perform_square(); break;
					case 7: perform_div(); break;

					case 8: insert_digit ('7'); break;
					case 9: insert_digit ('8'); break;
					case 10: insert_digit ('9'); break;
					case 11: perform_mult(); break;

					case 12: insert_digit ('4'); break;
					case 13: insert_digit ('5'); break;
					case 14: insert_digit ('6'); break;
					case 15: perform_sub(); break;

					case 16: insert_digit ('1'); break;
					case 17: insert_digit ('2'); break;
					case 18: insert_digit ('3'); break;
					case 19: perform_add(); break;

					case 20: insert_digit ('0'); break;
					case 21: perform_point(); break;
					case 22: perform_neg(); break;
					case 23: perform_equals (); break;

					}
				}
			}
		}

		if (!need_draw)
			continue;

		if (need_draw) {
			update_value ();

			if (full_draw) {
				int i,j;
				for (j=0; j<rows; j++)
					for (i=0; i<cols; i++)
						draw_button (dpy,win,i,j, false);
			}
		}

		fbui_flush (dpy, win);
	}

	fbui_window_close(dpy, win);
	fbui_display_close(dpy);
	return 0;
}
