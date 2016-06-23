
/*=========================================================================
 *
 * fbview, an image viewer for FBUI (in-kernel framebuffer UI)
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
 * Sept 26, 2004: Updated for grayscale images.
 * Dec 18, 2004: Updated for TIFFs.
 * Jan 11, 2004: Fixed memory allocation bugs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "libfbui.h"

#include <jpeglib.h>
#include <tiffio.h>


typedef struct item {
	struct item *next;
	char *str;
} Item;



static unsigned char *buffer = NULL;
static unsigned long buflen=0;

unsigned char *get_buffer (unsigned long size)
{
	if (!buffer) {
		buffer = (JSAMPLE*) malloc (size);
		if (!buffer)
			FATAL ("failed to malloc image buffer\n");

		buflen = size;
		return buffer;
	}
	else
	{
		if (size < buflen)
			return buffer;
		else {
			buffer = realloc (buffer, size);
			return buffer;
		}
	}
}


static Item *file_list = NULL;

void append_item (char *str)
{
	Item *nu = (Item*)malloc(sizeof(Item));
	memset ((void*)nu,0,sizeof(Item));
	nu->str = strdup (str);
	
	Item *last=NULL;
	last=file_list;
	while(last) {
		if (last->next)
			last = last->next;
		else
			break;
	}
	if (!last)
		file_list = nu;
	else
		last->next = nu;
}

char *nth_item (int n)
{
	Item *item= file_list;

	while (n--) {
		if (item)
			item = item->next;
		if (!item)
			break;
	}

	return item? item->str : NULL;
}


Font *pcf;
int target_w, target_h;
int available_height, text_height;

typedef struct
{
        int dx, dy;
        int e;
        int j;
        int sum;
}
BresenhamInfo;

void
bresenham_init (BresenhamInfo *bi, int target_size, int original_size)
{
	if (!bi) 
		FATAL("null ptr param");
	if (!target_size || !original_size)
		FATAL("zero numeric param");
	if (target_size > original_size)
		FATAL("expansion requested");

	bi->dx = original_size;
	bi->dy = target_size;
	bi->e = (bi->dy<<1) - bi->dx;
	bi->j = 0;
	bi->sum = 0; // diag
}


int
bresenham_get (BresenhamInfo *bi)
{
	char done = 0;

	if (!bi) 
		FATAL("null ptr param");

	done = false;

	int initial_j = bi->j;

	while (bi->j <= bi->dx && !done)
	{
		if (bi->e >= 0)
		{
			done = true;
			bi->e -= (bi->dx<<1);
		}
		bi->e += (bi->dy<<1);
		++bi->j;
	}

	int count = bi->j - initial_j;
	bi->sum += count;
	if (bi->sum > bi->dx)
	{
		int *i=0;
		*i++;
	}
	return count;
}


JSAMPLE * image_buffer;
unsigned long *image_buffer_long;
unsigned char *shrunken_image_buffer;
int image_height;       
int image_width; 
int image_ncomponents;
int image_orig_width, image_orig_height;

static int grayscale=0;

static int fileNum = 0;
static char *path = NULL;

extern int read_JPEG_file (char*);

static unsigned char bres_x_ary [3000];
static unsigned char bres_y_ary [3000];

static short win_w, win_h;
static char do_shrink=0;

void shrink_image (short target_w, short target_h);

void
rescale()
{
printf ("fbview: entered rescale()\n");
	do_shrink=0;
	target_w = image_width;
	target_h = image_height > available_height ? available_height : image_height;

	if (win_w < image_width || available_height < image_height) 
	{
		do_shrink = 1;

		double ratio = 0;
		if (win_w < image_width) {
			ratio = win_w / (double) image_width;
		}
		if (available_height < image_height) {
			double r = available_height / (double) image_height;
			if (ratio == 0 || r < ratio)
				ratio = r;
		}
		target_w = image_width * ratio;
		target_h = image_height * ratio;
	}

	if (do_shrink) {
		shrink_image (target_w, target_h);
		image_width = target_w;
		image_height= target_h;
		do_shrink = 0;
		image_ncomponents = 3;
	}
}

int
readnext (Display *dpy, Window *win)
{
	path=NULL;
	int result=0;

	while (!result) {
		image_width=0;
		image_height=0;
		image_ncomponents=0;

		path = nth_item (fileNum++);
		if (!path)
			break;

		if (image_buffer) {
			image_buffer = NULL;
		}
		if (image_buffer_long) {
			image_buffer_long = NULL;
		}
		if (shrunken_image_buffer) {
			shrunken_image_buffer = NULL;
		}

		char *extension = path;
		char *s = NULL;
		while ((s = strchr (extension, '.'))) {
			extension = s;
			if (strchr (s+1, '.')) {
				extension = s+1;
			} else 
				break;
		}
		if (extension == path) {
			fprintf (stderr, "fbview: invalid filename %s\n", path);
			continue;
		}

		if (!strcmp (extension, ".jpg") ||
		    !strcmp (extension, ".JPG") ||
		    !strcmp (extension, ".jpeg") ||
		    !strcmp (extension, ".JPEG"))
		{
			result = read_JPEG_file (path);
			if (result) {
				printf ("JPEG information: %s, width %d height %d depth %d\n",
					path,image_width,image_height,image_ncomponents*8);
				if (image_ncomponents != 3 && image_ncomponents != 1)
					result=0;
				image_buffer_long = NULL;
				image_orig_width = image_width;
				image_orig_height = image_height;
			}

		}
		else
		if (!strcmp (extension, ".tif") ||
		    !strcmp (extension, ".TIF") ||
		    !strcmp (extension, ".tiff") ||
		    !strcmp (extension, ".TIFF")) 
		{
			TIFF *tiff = TIFFOpen (path, "r");
			if (tiff) {
				unsigned long w, h;
				unsigned short d, samples;
				char ok = 1;

				if (!TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &w)) 
					ok = 0;
				if (!TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &h)) 
					ok = 0;
				if (!TIFFGetField (tiff, TIFFTAG_BITSPERSAMPLE, &d)) 
					ok = 0;
				if (TIFFGetField (tiff, TIFFTAG_SAMPLESPERPIXEL, &samples))
					d *= samples;

				if (!ok) {
					TIFFClose (tiff);
					fprintf (stderr, "TIFF %s: missing dimensions or depth\n", path);
					continue;
				}

				printf ("TIFF information: %s, width %lu height %lu depth %u\n",
					path, w,h,d);

				image_width = w;
				image_height = h;
				image_orig_width = image_width;
				image_orig_height = image_height;
				image_ncomponents = d / 8; // RGBA

				image_buffer_long = (unsigned long*) get_buffer (w * h * 4);
				if (!image_buffer_long) {
					TIFFClose (tiff);
					FATAL ("out of memory");
				}

				if (!TIFFReadRGBAImage (tiff, w, h, image_buffer_long, 1)) {
					TIFFClose (tiff);
					fprintf (stderr, "fbview: error reading %s\n", path);
				}

				image_buffer = NULL;

				TIFFClose (tiff);
				result = 1;
			}
			else
				result = 0;
		}
		else
		if (!strcmp (extension, ".nef") ||
		    !strcmp (extension, ".NEF")) 
		{
			fprintf (stderr, "fbview: not supporting raw images yet: %s\n", path);
			continue;
		}
		else
		{
			fprintf (stderr, "fbview: unsupported image extension in %s\n", path);
			continue;
		}
	}

	if (!path)
		return 0;

	if (result)
		fbui_set_subtitle (dpy, win, path);

	rescale();

	return 1;
}


void printloading (Display *dpy, Window *win)
{
	fbui_draw_string (dpy, win, pcf, 10, 10, "Loading...", RGB_WHITE);
	fbui_flush (dpy, win);
}


void shrink_image (short target_w, short target_h)
{
	BresenhamInfo b;

	image_width = image_orig_width;
	image_height = image_orig_height;

	bresenham_init (&b, target_w, image_width);
	int n=0, i, j, inc;
	for (i=0; i<image_width; i+=inc) {
		inc = bres_x_ary[n++] = bresenham_get(&b);
	}
	bresenham_init (&b, target_h, image_height);
	n=0;
	for (i=0; i<image_height; i+=inc) {
		inc = bres_y_ary[n++] = bresenham_get(&b);
	}

	shrunken_image_buffer = malloc (target_w * target_h * 3);
	if (!shrunken_image_buffer)
		FATAL("out of memory");

	/* shrink */
	int yinc = 0;
	int x, y;
	for (y=0, j=0; j<image_height; j+=yinc, y++) 
	{
		yinc = bres_y_ary [y];

		int xinc = 0;

		for(x=0, i=0; i<image_width; i+=xinc, x++) 
		{
			xinc = bres_x_ary [x];

			unsigned long r,g,b;
			r=g=b=0;

			/* collect a pixel average */
			int k,l;
			for (k=0; k<xinc; k++) {
				for (l=0; l<yinc; l++) {
					unsigned char *p = NULL;
					unsigned long *p2 = NULL;

					if (image_buffer)  {
						p = image_buffer + image_ncomponents * (i+k + (j+l)*image_width);
						unsigned char pix=0;
						if (image_ncomponents == 1) {
							pix = *p;
							r += pix;			
							g += pix;			
							b += pix;			
						} else {
							/* 3 */
							r += *p++;
							g += *p++;
							b += *p;
						}
					} 
					else if (image_buffer_long) {
						// TIFFs are upside down
						int yy = j + l;
						if (yy >= image_height-1)
							yy = image_height-1;

						yy = (image_height-1) - yy;

						p2 = image_buffer_long + i+k + yy*image_width;

						unsigned long pix = *p2;
						unsigned long r0, g0, b0;
						register unsigned long m;
						b0 = pix >> 16;
						g0 = pix >> 8;
						r0 = pix;
						m = 0xff;
						r0 &= m;
						b0 &= m;
						g0 &= m;
						r += r0;
						g += g0;
						b += b0;
					}
				}
			}
			short factor = xinc * yinc;
			r /= factor;
			g /= factor;
			b /= factor;

			unsigned long ix = 3 * (x + y * target_w);

			if (grayscale) {
				short n = (r + g + b) / 3;
				r = g = b = n;
			}

			shrunken_image_buffer [ix++] = r;
			shrunken_image_buffer [ix++] = g;
			shrunken_image_buffer [ix] = b;
		} 
	}
}


int
main(int argc, char** argv)
{
	int i,j;
	Display *dpy;
	Window *win;
	char this_image_shrunken = 0;

	path=NULL;
	image_buffer=NULL;
	image_buffer_long=NULL;
	image_height=0;
	image_width=0;
	shrunken_image_buffer = NULL;

	if (argc==1) 
		return 0;

        pcf = font_new ();

        if (!pcf_read (pcf, "timR12.pcf")) {
                font_free (pcf);
                pcf = NULL;
		FATAL ("cannot load font");
        }

	text_height = pcf->ascent + pcf->descent;

/* XX
 * To fix: If only one image file, we should open
 * with dimensions of that file, plus room for text
 */
	long fg,bg;
	fg = RGB_NOCOLOR;
	bg = RGB_BLACK;

	dpy = fbui_display_open ();
	if (!dpy)
		FATAL ("cannot open display");

	/* get the maximal window */
	image_width=dpy->width-1;
	image_height=dpy->height-1;

	win = fbui_window_open (dpy, image_width, image_height + 5 + text_height, 
		&win_w, &win_h,
		9999,9999, // max wid/ht
		0, 0, 
		&fg, &bg, 
		"fbview", "", 
		FBUI_PROGTYPE_APP, 
		false, // not requesting control
		false, // therefore not autoplacing anything
		-1,
		true, // need keys
		false, // not all motion
		false, // not hidden
		argc,argv);
	if (!win)
		FATAL ("cannot create window");

	/* Parse file list */
	i=1;
	while (i<argc) {
		char ch = *argv[i];
		/* params used by FBUI lib will be truncated to zero length */
		if (ch && ch != '-') {
			printf ("param '%s'\n", argv[i]);
			append_item (argv[i]);
		}
		i++;
	}

	fileNum = 0;

	available_height = win_h - 5 - text_height;

	printloading (dpy, win);

	int result = readnext (dpy,win);

	if (!result) {
		fbui_display_close (dpy);
		exit(0);
	}

	fbui_clear (dpy, win);
	fbui_flush (dpy, win);

	/* event loop */
	char done=0;
	while(!done) {
		Event ev;
		int err;
		if ((err = fbui_wait_event (dpy, &ev, FBUI_EVENTMASK_ALL))) {
			fbui_print_error (err);
			continue;
		}
printf ("%s got event %s\n", argv[0], fbui_get_event_name (ev.type));

		int num = ev.type;

		if (ev.win != win)
			FATAL ("got event for another window");

		switch (num) {
		case FBUI_EVENT_MOVE_RESIZE:
printf ("fbview got MR: %d %d\n", win_w, win_h);
			if (win_w == ev.width && win_h && ev.height)
				continue;
			win_w = ev.width;
			win_h = ev.height;
			available_height = win_h - 5 - text_height;
			if (image_buffer)
				rescale ();
			break;
		
		case FBUI_EVENT_ENTER:
			printf ("fbview got Enter\n");
			continue;
		
		case FBUI_EVENT_LEAVE:
			printf ("fbview got Leave\n");
			continue;
		
		case FBUI_EVENT_MOTION: {
			short x, y;
			
			x = ev.x;
			y = ev.y;

			/* not used */
			continue;
		}
		
		case FBUI_EVENT_KEY: {
			short ch = fbui_convert_key (dpy, ev.key);

#if 0
			if (ch)
				printf ("viewer got key '%c' (0x%02x)\n", ch,ch);
#endif

			if (ch == 'q' || ch == 'Q') {
				done=1;
				continue;
			} 
			else if (ch == 'g' || ch == 'G') {
				grayscale = !grayscale;
			}
			else
			if (ch == ' ') {
				if (this_image_shrunken)
					free (shrunken_image_buffer);

				shrunken_image_buffer = NULL;
				this_image_shrunken = 0;

				fbui_clear (dpy, win);
				printloading (dpy, win);

				int result= readnext (dpy,win);

				if (!result) {
					fbui_window_close (dpy, win);
					exit(0);
				}
			}
			else
				continue;
		}
		
		case FBUI_EVENT_EXPOSE:
			break;
			
		default:
			continue;
		}

		char expr [100];
		char *tmp = path;
		char *tmp2;
		while ((tmp2 = strchr(tmp, '/'))) {
			tmp = tmp2 + 1;
		}
		sprintf (expr, "%s (%d x %d, depth %d)", tmp, 
			image_orig_width, image_orig_height, 
			image_ncomponents * 8);
		short w,a,d;
		font_string_dims (pcf, expr, &w,&a,&d);
		fbui_clear_area (dpy, win, 0, target_h+5, w, target_h+5 + text_height);
		fbui_draw_string (dpy, win, pcf, 0, target_h + 5, expr, RGB_WHITE);

		for (j=0; j<image_height; j++) {
			if (image_ncomponents == 1 || grayscale) {
				for(i=0; i<image_width; i++) {
					unsigned long pix=0;
					unsigned char *p = NULL;
					unsigned long *p2 = NULL;

					if (shrunken_image_buffer)  {
						p = shrunken_image_buffer + image_ncomponents*(i + j*image_width);
						pix = *p++;
						pix += *p++;
						pix += *p;
						pix /= 3;
						pix |= (pix<<8) | (pix<<16);
					} 
					else if (image_buffer) {
						p = image_buffer + image_ncomponents*(i + j*image_width);
						/* pixels are grayscale */
						if (image_ncomponents == 1) {
							pix = *p;
							pix <<= 8;
							pix |= *p;
							pix <<= 8;
							pix |= *p;
						} else {
							pix = *p++;
							pix += *p++;
							pix += *p;
							pix /= 3;
							pix |= (pix<<8) | (pix<<16);
						}
					}
					else if (image_buffer_long) {
						int yy = (image_height-1) - j;
						p2= image_buffer_long + i + yy*image_width;
						pix = *p2;
					}

					fbui_draw_point (dpy, win, i, j, pix);
				}
			} 
			else 
			{
				if (shrunken_image_buffer)
					fbui_put_rgb3 (dpy, win, 0, j, image_width, 
						shrunken_image_buffer + 3*j*image_width);
				else if (image_buffer)
					fbui_put_rgb3 (dpy, win, 0, j, image_width, 
						image_buffer + 3*j*image_width);
				else if (image_buffer_long)
					fbui_put_rgb (dpy, win, 0, j, image_width, 
						image_buffer_long + 4*j*image_width);
			}
		}

		fbui_flush (dpy, win);
	} /* while */

	fbui_flush (dpy, win);
	fbui_window_close (dpy, win);
	fbui_display_close (dpy);
	
	if (buffer)
		free (buffer);
	return 0;
}
