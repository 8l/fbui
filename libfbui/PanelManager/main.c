
/*=========================================================================
 *
 * fbpm, a window manager for FBUI (in-kernel framebuffer UI)
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
 * Oct 21, 2004, fbui@comcast.net: began conversion to panel manager
 * Dec 12, 2004, fbui@comcast.net: implemented alt-key switching between apps
 * Dec 31, 2004, fbui@comcast.net: added drawing of background areas
 */



#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <jpeglib.h>
#include <signal.h>
#include <errno.h>

#include "libfbui.h"


#define min(AA,BB) ( (AA)<(BB) ? (AA) : (BB) )
#define max(AA,BB) ( (AA)>(BB) ? (AA) : (BB) )


extern void draw_background (Display *dpy, Window *win);

extern int read_JPEG_file (char*);

void draw_titlebar (Display *dpy, Window *self, short width, char *title, char*);


#define circle1_width 16
#define circle1_height 16
static unsigned char circle1_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0x84, 0x10, 0xc2, 0x20, 0xc2, 0x20,
   0xa1, 0x40, 0x81, 0x40, 0x81, 0x40, 0x82, 0x20, 0x82, 0x20, 0x84, 0x10,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle2_width 16
#define circle2_height 16
static unsigned char circle2_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xc4, 0x11, 0x22, 0x22, 0x02, 0x22,
   0x01, 0x42, 0x01, 0x41, 0x81, 0x40, 0x42, 0x20, 0x22, 0x20, 0xe4, 0x13,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle3_width 16
#define circle3_height 16
static unsigned char circle3_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xe4, 0x13, 0x02, 0x21, 0x82, 0x20,
   0xc1, 0x41, 0x01, 0x42, 0x01, 0x42, 0x02, 0x22, 0x22, 0x22, 0xc4, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle8_width 16
#define circle8_height 16
static unsigned char circle8_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xc4, 0x11, 0x22, 0x22, 0x22, 0x22,
   0xc1, 0x41, 0x21, 0x42, 0x21, 0x42, 0x22, 0x22, 0x22, 0x22, 0xc4, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle4_width 16
#define circle4_height 16
static unsigned char circle4_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0x04, 0x11, 0x22, 0x21, 0x22, 0x21,
   0x21, 0x41, 0xe1, 0x43, 0x01, 0x41, 0x02, 0x21, 0x02, 0x21, 0x04, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle5_width 16
#define circle5_height 16
static unsigned char circle5_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xe4, 0x13, 0x22, 0x20, 0x22, 0x20,
   0xe1, 0x41, 0x01, 0x42, 0x01, 0x42, 0x02, 0x22, 0x22, 0x22, 0xc4, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle6_width 16
#define circle6_height 16
static unsigned char circle6_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xc4, 0x11, 0x22, 0x22, 0x22, 0x20,
   0x21, 0x40, 0xe1, 0x41, 0x21, 0x42, 0x22, 0x22, 0x22, 0x22, 0xc4, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle7_width 16
#define circle7_height 16
static unsigned char circle7_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xe4, 0x13, 0x02, 0x22, 0x02, 0x22,
   0x01, 0x41, 0x81, 0x40, 0x81, 0x40, 0x82, 0x20, 0x82, 0x20, 0x84, 0x10,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle9_width 16
#define circle9_height 16
static unsigned char circle9_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xc4, 0x11, 0x22, 0x22, 0x22, 0x22,
   0x21, 0x42, 0xc1, 0x43, 0x01, 0x41, 0x02, 0x21, 0x82, 0x20, 0x84, 0x10,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};

#define circle0_width 16
#define circle0_height 16
static unsigned char circle0_bits[] = {
   0xc0, 0x01, 0x30, 0x06, 0x0c, 0x18, 0xc4, 0x11, 0x22, 0x22, 0x22, 0x22,
   0x21, 0x42, 0x21, 0x42, 0x21, 0x42, 0x22, 0x22, 0x22, 0x22, 0xc4, 0x11,
   0x0c, 0x18, 0x30, 0x06, 0xc0, 0x01, 0x00, 0x00};


static short icon_height = circle0_height;


static unsigned char *bitmaps[10] = {
	circle1_bits,
	circle2_bits,
	circle3_bits,
	circle4_bits,
	circle5_bits,
	circle6_bits,
	circle7_bits,
	circle8_bits,
	circle9_bits,
	circle0_bits,
};

unsigned short reverse16 (unsigned short value)
{
	int i;
	unsigned short result = 0;
	for (i=0; i<16; i++) {
		unsigned short j = 1 << i;
		unsigned short j2 = 1 << (15-i);
		if (j & value)
			result |= j2;
	}
	return result;
}

short win_w, win_h;
#define STEELBLUE 0x4682B4

extern JSAMPLE * image_buffer;
extern int image_height;
extern int image_width;
extern int image_ncomponents;
extern int read_JPEG_file (char*);

int foreground_id = -1;

Font *font1;
Font *font2;
short line_height; // font1's

static short separator_size = 2;

static char grayscale=0; /* 1 forces bg image to grayscale */


int window_count;

#define MAXWINS (40)
struct fbui_wininfo info[MAXWINS];



static short mainarea_width;
static short mainarea_height;

static short bottomarea_width;
static short bottomarea_height;

static short rightarea_width;
static short rightarea_height;

static short listarea_width;
static short listarea_height;

static short rightarea_height_remaining;
static short bottomarea_width_remaining;


void
draw_window_list (Display *dpy, Window *self)
{
	int i;
	short icon_width;
	short lx, ly;
	short key=1;
	short line_height = font1->ascent + font1->descent;

	// The (#) icons
	icon_width = 16;
	icon_height = 16;

	lx = win_w - listarea_width;
	ly = win_h - listarea_height;
	fbui_fill_area (dpy, self, lx,ly, win_w,win_h, RGB_CYAN);

	int appcount = 0;
	for (i=0; i < window_count; i++) {
		int t = info[i].program_type;
		unsigned char *bits_source = bitmaps [appcount < 10 ? appcount : 9];

		if (t == FBUI_PROGTYPE_APP || 
		    t == FBUI_PROGTYPE_EPHEMERAL || !t) 
		{
			int j;
			for (j=0; j<icon_height; j++) {
				unsigned short bits = 0;
				int k = j << 1;
				bits = bits_source [k+1]; 
				bits <<= 8;
				bits |= bits_source [k];
				bits = reverse16 (bits);

				fbui_tinyblit (dpy, self, lx+2, ly+j,
					RGB_BLACK, RGB_NOCOLOR, 
					icon_width, bits);
			}

			fbui_draw_string (dpy, self, font1,
				lx+3+icon_width,ly, 
				info[i].name, RGB_BLUE);

			appcount++;
			ly += max(line_height,icon_height);
		}
	}

	// No apps? Clear out the title bar.
	if (!appcount) {
		// fbui_clear_area (dpy, self, 0, 0, mainarea_width-1, line_height-1);
		fbui_draw_string (dpy, self, font1, lx+3, ly, "No apps.", RGB_BLUE);
	}
}


void
flip (Display *dpy, Window *self, int which /* 0...8 */ )
{
	int i, appcounter;
	int count;

	if (which < 0 || which > 8) {
		return;
	}

	/* Firstly count the number of apps and ephemerals */
	count=0;
	i = 0;
	while (i < window_count) {
		char type = info[i].program_type;
		if (type == FBUI_PROGTYPE_APP || type == FBUI_PROGTYPE_EPHEMERAL || !type)
			count++;
		i++;
	}
	/* Rule: overlarge # switches to last */
	if (which >= count)
		which = count - 1;

	/* Secondly hide all */
	i = 0;
	while (i < window_count) {
		char type = info[i].program_type;
		if (type == FBUI_PROGTYPE_APP || type == FBUI_PROGTYPE_EPHEMERAL || !type) {
			fbui_hide (dpy, self, info[i].id);
		}
		i++;
	}

	/* Thirdly bring the flipped-to window to the fore */
	appcounter = 0; 
	i = 0;
	struct fbui_wininfo *wi = NULL;
	while (i < window_count) {
		char type = info[i].program_type;
		if (type == FBUI_PROGTYPE_APP || type == FBUI_PROGTYPE_EPHEMERAL || !type) {
			if (appcounter == which) {
				int retval;
				short id = info[i].id;
				wi = &info[i];
				foreground_id = id;
				if (retval = fbui_unhide (dpy, self, id))
					printf ("fbpm: unhide failed %d\n", retval);
				fbui_assign_keyfocus (dpy, self, id);
				break;
			}
			appcounter++;
		}
		i++;
	}

	if (wi)
		draw_titlebar (dpy,self,wi->width, wi->name, wi->subtitle);
}


void
flip_to_next (Display *dpy, Window *self)
{
	int i;
	char found_current = 0;
	char appcounter = 0;

	if (foreground_id < 0)
		return;

	i = 0;
	while (i < window_count) {
		char type = info[i].program_type;
		short id = info[i].id;

		if (type == FBUI_PROGTYPE_APP || type == FBUI_PROGTYPE_EPHEMERAL || !type) {
			if (found_current) {
				flip (dpy, self, appcounter);
				return;
			}

			if (id == foreground_id)
				found_current = 1;

			appcounter++;
		}
		i++;
	}

	// Not found, which means it was the last one.
	flip (dpy, self, 0);
}


void draw_titlebar (Display *dpy, Window *self, short width, char *title, char *subtitle)
{
	if (!window_count)
		return;

	// draw gradient behind text
	int j=0;
	while (j < width) {
		unsigned long r,g,b;
		int n = 255 - j/2;
		int k = n > 1 ? n : 0;
		r = 30;
		g = 32 + (7*k)/8;
		b = k/3;
		r <<= 16;
		b <<= 8;
		fbui_draw_vline (dpy, self, 
				 j, 0, line_height-1, r|g|b);
		j++;
	}

	// draw program name/pid
	char tmp[200];
	if (!subtitle || !*subtitle)
		sprintf(tmp,"%s", title,subtitle);
	else
		sprintf(tmp,"%s (%s)", title,subtitle);
	fbui_draw_string (dpy, self, font1,0,0,tmp, RGB_YELLOW);
}


int
main(int argc, char** argv)
{
	int i,j;
	Display *dpy;
	Window *self;

	window_count = 0;

        font1 = font_new ();  // main font
        // font2 = font_new ();  // subtitle font

        if (!pcf_read (font1, "timR12.pcf")) {
                font_free (font1);
                font1 = NULL;
		FATAL ("cannot load font");
        }

	/* Get text height...will determine size of list area.
 	 */
	line_height = font1->ascent + font1->descent;

	long fgcolor = RGB_NOCOLOR; // not used
	long bgcolor = STEELBLUE;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL ("cannot open display");

	self = fbui_window_open (dpy, 1,1, &win_w, &win_h,
		9999,9999, // no max wid/ht
		0, 0, 
		&fgcolor, &bgcolor, 
		"fbpm", "", 
		FBUI_PROGTYPE_WM, 
		true,
		true, 
		-1,
		false,
		true, // all motion
		false,// not hidden
		argc,argv);
	if (!self) {
		fbui_display_close (dpy);
		FATAL ("cannot open manager window");
	}

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
                if (!result) {
                        WARNING ("cannot read background image");
			if (image_buffer)
				free(image_buffer);
			image_buffer=NULL;
			image_width=0;
			image_height=0;
			image_ncomponents=0;
		}

        }

	/* If the wm was started after windows appeared,
	 * we need to hide those windows to prevent fbui_overlap_check
	 * from complaining.
	 */
	window_count = fbui_window_info (dpy, self, &info[0], MAXWINS);
	for (i=0; i<window_count; i++) {
		int rv = fbui_hide (dpy, self, info[i].id);
		if (rv) {
			printf ("hide of %s failed! (%d)\n", info[i].name,rv);
		}
	}

	listarea_height = icon_height * 10;
	rightarea_width = 78;
	listarea_width = rightarea_width;
	bottomarea_width = win_w - rightarea_width;
	mainarea_width = bottomarea_width;
	bottomarea_height = 25;
	mainarea_height = win_h - bottomarea_height;
	rightarea_height = mainarea_height - listarea_height;
	bottomarea_width_remaining = bottomarea_width;
	rightarea_height_remaining = rightarea_height;

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

	/* Alt-PrtSc is to perform a screendump
	 */
	if (FBUI_SUCCESS != fbui_accelerator (dpy, self, FBUI_ACCEL_PRTSC, 1))
		FATAL ("cannot register accelerator");

	if (FBUI_SUCCESS != fbui_placement (dpy, self, 1))
		FATAL ("cannot force auto-placement");

	int need_list = 1;
	int need_redraw = 0;
	goto getlist;

	while(1) {
		Event ev;
		int err;
		if (err = fbui_wait_event (dpy, &ev,
                                     FBUI_EVENTMASK_ALL - FBUI_EVENTMASK_KEY))
		{
			fbui_print_error (err);
			continue;
		}
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

		int num = ev.type;

		if (ev.win != self)
			FATAL ("event not for our window");

		if (num == FBUI_EVENT_EXPOSE) {
			need_redraw = 1;
		}
		else if (num == FBUI_EVENT_WINCHANGE) {
			need_redraw = 1;
			need_list = 1;
		}
		else if (num == FBUI_EVENT_ENTER) {
			printf ("fbpm got Enter (it shouldnt)\n");
			continue;
		}
		else if (num == FBUI_EVENT_LEAVE) {
			printf ("fbpm got Leave (it shouldnt)\n");
			continue;
		}
		else if (num == FBUI_EVENT_ACCEL) {
			short key = ev.key;

printf ("ACCEL key %d\n", key);

			switch (key) {
			case '\b':
				goto finit;

			case '\t':
				flip_to_next (dpy, self);
				continue;

			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				flip (dpy, self, key - '1');
				continue;

			case FBUI_ACCEL_PRTSC:
				printf ("Dumping screen image to dump.jpg\n");
				system ("fbdump");
				continue;
			}
		}

getlist:
		if (need_list) {
			int i;
			short new_x0[MAXWINS];
			short new_y0[MAXWINS];
			short new_x1[MAXWINS];
			short new_y1[MAXWINS];
			short zone[MAXWINS];

			short new_application_window = -1;

			int total_existing_application_windows = 0;
			short existing_application_windows[MAXWINS];

			for (i=0; i<MAXWINS; i++)
				existing_application_windows[i] = -1;

			char foreground_is_in_foreground = 0;
			char foreground_found = 0;

			/* Get a list of existing application windows,
			 * since we need to bring any new app window
			 * to the fore.	
			 */
			for (i=0; i<window_count; i++) {
				if (info[i].program_type == FBUI_PROGTYPE_APP) {
					existing_application_windows [total_existing_application_windows++] = info[i].id;

					if (info[i].id == foreground_id) {
						if (!info[i].hidden)
							foreground_is_in_foreground = 1;
					}
				}
			}

printf ("----------------------------------------------\n");
printf ("fbpm: Getting window list...");
			window_count = fbui_window_info (dpy, self, &info[0], MAXWINS);
printf ("%d total\n", window_count);

			/* Perform a window census...
			 */
			int total_bottom = 0;
			int total_right = 0;
			int total_main = 0;
			for (i=0; i<window_count; i++) {
				int id = info[i].id;

				switch (info[i].program_type) {
				case FBUI_PROGTYPE_WM:
printf ("  %d wm %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].hidden?"hidden":"");
					break;
				case FBUI_PROGTYPE_APP:
					total_main++; 
printf ("  %d app %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					j=0;
					char got_it_already = 0;
					while (j < total_existing_application_windows) {
						if (id == existing_application_windows[j]) {
							got_it_already = 1;
							break;
						}
						j++;
					}

					if (!got_it_already)
						new_application_window = id;

					if (foreground_id == id)
						foreground_found = 1;

					break;
				case FBUI_PROGTYPE_LAUNCHER:
					total_bottom++; 
printf ("  %d launcher %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					break;
				case FBUI_PROGTYPE_TOOL:
					total_right++; 
printf ("  %d tool %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					break;
				case FBUI_PROGTYPE_EPHEMERAL:
					total_main++; 
printf ("  %d ephemeral %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					break;
				case FBUI_PROGTYPE_NONE:
					total_main++; 
printf ("  %d (no type) %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					break;
				default:
					total_main++; 
printf ("  %d (unknown type) %s id=%d pid=%d %s\n", i, info[i].name, id,info[i].pid,info[i].hidden?"hidden":"");
					break;
				}
			}

			/* Determine what the new positions of all of 
			 * the windows will be.
			 */
			int bottom_ration = total_bottom ? bottomarea_width / total_bottom : 0;
			int right_ration = total_right ? rightarea_height / total_right : 0;
			int bottom_x = 0;
			int right_y = 0;

			if (!foreground_found)
				foreground_id = -1;

			for (i=0; i<window_count; i++) {
				char whichzone=0;  /* 1=main, 2=bottom, 3=right */
				int x0=-1,y0,x1,y1;
				short x = info[i].x;
				short y = info[i].y;
				short w = info[i].width;
				short h = info[i].height;
				short max_w = info[i].max_width;
				short max_h = info[i].max_height;
//printf ("from info[%d] x %d y %d w %d h %d\n",(int)i,(int)x,(int)y,(int)w,(int)h);

				switch (info[i].program_type) {
				case FBUI_PROGTYPE_WM:
					break;

				case FBUI_PROGTYPE_APP:
				case FBUI_PROGTYPE_EPHEMERAL:
				default:
					whichzone=1;
					x0 = 0;
					y0 = line_height; // leave space for title
					x1 = mainarea_width - 1 - separator_size;
					y1 = mainarea_height - 1 - separator_size;

					if (new_application_window < 0 && foreground_id == -1)
						foreground_id = info[i].id;

					break;

				case FBUI_PROGTYPE_LAUNCHER:
					whichzone=2;
					x0 = bottom_x;
					if (w < bottom_ration)
						bottom_x += (w + separator_size);
					else if (max_w > 1 && max_w < bottom_ration) 
						bottom_x += max_w;
					else
						bottom_x += bottom_ration;
					x1 = bottom_x - 1 - separator_size;
					y0 = mainarea_height;
					y1 = win_h - 1;
					break;

				case FBUI_PROGTYPE_TOOL:
					whichzone=3;
					x0 = mainarea_width;
					x1 = win_w - 1;
					y0 = right_y;
					if (h < right_ration)
						right_y += (h + separator_size);
					else if (max_h > 1 && max_h < right_ration)
						right_y += max_h;
					else
						right_y += right_ration;
					y1 = right_y - 1 - separator_size;
					break;
				}

				new_x0 [i] = x0;
				new_y0 [i] = y0;
				new_x1 [i] = x1;
				new_y1 [i] = y1;
				zone [i] = whichzone;
printf ("  new pos '%s' = %d %d, %d %d\n", info[i].name,x0,y0,x1,y1);
			}

			/* Determine which zones changed.
			 */
			char bottom_changed=0;
			char right_changed=0;
			char main_changed=0;
			for (i=0; i<window_count; i++) {
				if (info[i].need_placement ||
				    new_x0[i] != info[i].x || 
				    new_y0[i] != info[i].y || 
				    new_x1[i] != info[i].x+info[i].width-1 ||
				    new_y1[i] != info[i].y+info[i].height-1)
				{
					switch (zone[i]) {
					case 1:
						main_changed=1;
						break;
					case 2:
						bottom_changed=1;
						break;
					case 3:
						right_changed=1;
						break;
					}
				}
			}

			if (!foreground_found)
				main_changed = 1;
			if (new_application_window >= 0)
				main_changed = 1;

			if (main_changed)
				printf ("-main changed-\n");
			if (bottom_changed)
				printf ("-bottom changed-\n");
			if (right_changed)
				printf ("-right changed-\n");

			/* For each zone that changed, hide all
			 * windows.
			 */
			if (main_changed) {
				for (i=0; i<window_count; i++) {
					if (zone[i]==1)
						fbui_hide (dpy, self, info[i].id);
				}
			}
			if (bottom_changed) {
				for (i=0; i<window_count; i++) {
					if (zone[i]==2)
						fbui_hide (dpy, self, info[i].id);
				}
			}
			if (right_changed) {
				for (i=0; i<window_count; i++) {
					if (zone[i]==3)
						fbui_hide (dpy, self, info[i].id);
				}
			}

			/* Now make the size/position changes,
			 * while windows are hidden.
			 */
			for (i=0; i<window_count; i++) 
			{
				int domove=0;
				if ((zone[i]==1 && main_changed) ||
				    (zone[i]==2 && bottom_changed) ||
				    (zone[i]==3 && right_changed))
					domove=1;

				if (info[i].need_placement)
					domove=1;

				if (domove)
					fbui_move_resize (dpy, self, info[i].id,
						new_x0[i], 
						new_y0[i], 
						new_x1[i],
						new_y1[i]);
			}

			/* Unhide windows that need it.
			 */
			for (i=0; i<window_count; i++) 
			{
				if ((zone[i]==2 && bottom_changed) ||
				    (zone[i]==3 && right_changed))
				{
					int retval;
					if (retval = fbui_unhide (dpy, self, info[i].id))
						printf ("fbpm: unhide failed %d\n", retval);
				}
			}

			/* Only one main window gets focus
			 */
			if (new_application_window != -1) {
				int retval;
				if (retval = fbui_unhide (dpy, self, new_application_window))
					printf ("fbpm: unhide failed %d\n", retval);
				fbui_assign_keyfocus (dpy, self, new_application_window);
				foreground_id = new_application_window;
printf ("unhid new app %d\n", new_application_window);
			}
			else if (foreground_id != -1) {
				int retval;
printf ("unhid fg win %d\n", foreground_id);
				if (retval = fbui_unhide (dpy, self, foreground_id))
					printf ("fbpm: unhide failed %d\n", retval);
				fbui_assign_keyfocus (dpy, self, foreground_id);
			}

printf ("After moves...\n");
			window_count = fbui_window_info (dpy, self, &info[0], MAXWINS);
			for (i=0; i<window_count; i++) {
printf ("   program %s.... %s\n", info[i].name,info[i].hidden?"hidden":"");
			}

			need_list=0;

			need_redraw = 1;
		} 
		
		if (need_redraw) {
			int i;

			need_redraw = 0;

			draw_background (dpy,self);
			draw_window_list (dpy, self);

			i=0; 
			while (i < window_count) {
				struct fbui_wininfo *wi = &info[i];
				int x,y;

				if (wi->id == foreground_id) {
					draw_titlebar (dpy,self,wi->width, wi->name,wi->subtitle);
					break;
				}

				i++;
			}
		}
	}
finit:
	;

	int mypid = getpid();
	for (i=0; i < window_count; i++) {
		if (info[i].pid != mypid)
			kill (info[i].pid, SIGTERM);
	}

	for (i=0; i < window_count; i++) {
#if 0
		draw_background (dpy, self,
		  info[i].x,
		  info[i].y - line_height,
		  info[i].x + info[i].width - 1,
		  info[i].y - 1);
		fbui_flush (dpy, self);
#endif
	}

	fbui_window_close (dpy, self);
	fbui_display_close (dpy);

	return 0;
}
