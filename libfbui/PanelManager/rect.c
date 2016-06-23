
/*=========================================================================
 *
 * fbpm, a panel-based window manager for FBUI (in-kernel framebuffer UI)
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

#include <stdio.h>
#include <linux/fb.h>
#include <libfbui.h>
#include <jpeglib.h>

extern int window_count;
extern struct fbui_wininfo info[];

JSAMPLE * image_buffer;
int image_height;
int image_width;
int image_ncomponents;

extern short win_w, win_h;

char grayscale=0; /* 1 forces bg image to grayscale */




/* -----------------Rectangle subtraction code----------------- */

typedef struct rect {
	short x0,y0,x1,y1;
	struct rect *next;
} Rect;

static Rect* Rect_new(short x0, short y0, short x1, short y1)
{
	Rect *nu = (Rect*) malloc (sizeof(Rect));
	if (!nu) return NULL;

	nu->x0 = x0;
	nu->y0 = y0;
	nu->x1 = x1;
	nu->y1 = y1;
	nu->next = NULL;

	return nu;
}

static void Rect_sort (Rect *r)
{
	if (!r) return;
	if (r->x0 > r->x1) {
		short tmp = r->x0;
		r->x0 = r->x1;
		r->x1 = tmp;
	}
	if (r->y0 > r->y1) {
		short tmp = r->y0;
		r->y0 = r->y1;
		r->y1 = tmp;
	}
}

static Rect* Rect_dup (Rect *r)
{
	if (!r) return NULL;
	return Rect_new (r->x0,r->y0,r->x1,r->y1);
}

static int Rect_subtract (Rect *a, Rect *b, Rect *rects[])
{
	int count=0;
	if (!a || !b || !rects) return;

	if (a->x0==b->x0 && a->x1==b->x1 && a->y0==b->y0 && a->y1==b->y1)
		return 0;

	Rect_sort(a);
	Rect_sort(b);

#define between(N,M,O) (N>=M && N<=O)

	int i=-1, j=-1;
	if (b->x1 < a->x0)
		i=0;
	else if (b->x0 <= a->x0 && between(b->x1,a->x0,a->x1))
		i=1;
	else if (between(b->x0,a->x0,a->x1) && b->x1 >= a->x1)
		i=3;
	else if (between(b->x0,a->x0,a->x1) && between(b->x1,a->x0,a->x1))
		i=2;
	else
		i=4;

	if (b->y1 < a->y0)
		j=0;
	else if (b->y0 <= a->y0 && between(b->y1,a->y0,a->y1))
		j=1;
	else if (between(b->y0,a->y0,a->y1) && b->y1 >= a->y1)
		j=3;
	else if (between(b->y0,a->y0,a->y1) && between(b->y1,a->y0,a->y1))
		j=2;
	else
		j=4;

/*
                                          j values
                                                 +-------+
                                                 |   0   |
                                                 +-------+
                                      +-------+
                  +--------------+    |       |
                  |              |    |   1   |  +-------+
                  |              |    |       |  |       |
                  |      a       |    +-------+  |   2   |
                  |              |               |       |
                  |              |               +-------+
                  |              |    +-------+
                  +--------------+    |   3   |
                                      +-------+  +-------+
   i values:                                     |   4   |
      +-------+      +--------+     +-------+    +-------+
      |   0   |      |    2   |     |   4   |
      +-------+      +--------+     +-------+
            +---------+     +--------+
            |    1    |     |    3   |
            +---------+     +--------+
 */
	// totally outside
	if (i==0 || i==4 || j==0 || j==4) {
		rects[count++] = Rect_dup(a);
		return count;
	}
	else
	// totally inside
	if (i==2 && j==2) {
		if (a->x0 != b->x0)
		 rects[count++] = Rect_new (a->x0,a->y0,b->x0-1,a->y1); //left
		if (a->y0 != b->y0)
		 rects[count++]  = Rect_new (b->x0,a->y0,b->x1,b->y0-1); //top
		if (a->y1 != b->y1)
		 rects[count++]  = Rect_new (b->x0,b->y1+1,b->x1,a->y1); //bottom
		if (a->x1 != b->x1)
		 rects[count++]  = Rect_new (b->x0+1,a->y0,a->x1,a->y1); //right
		return count;
	}
	else
	if (i==1 && j==2) {
		if (a->y0 != b->y0)
		 rects[count++] = Rect_new (a->x0,a->y0,b->x1,b->y0-1); // topleft
		if (a->y1 != b->y1)
		 rects[count++] = Rect_new (a->x0,b->y1+1,b->x1,a->y1); // bottomleft
		if (a->x1 != b->x1)
		 rects[count++] = Rect_new (b->x1+1,a->y0,a->x1,a->y1); // right
		return count;
	}
	else
	if (i==3 && j==2) {
		if (a->y0 != b->y0)
		 rects[count++] = Rect_new (a->x0,a->y0,a->x1,b->y0-1); // top
		if (a->x0 != b->x0)
		 rects[count++] = Rect_new (a->x0,b->y0,b->x0-1,b->y1); // middle
		if (a->y1 != b->y1)
		 rects[count++] = Rect_new (a->x0,b->y1+1,a->x1,a->y1); // bottom
		return count;
	}
	else
	if (i==2 && j==1) {
		if (a->x0 != b->x0)
		 rects[count++] = Rect_new (a->x0,a->y0,b->x0-1,b->y1); // topleft
		if (a->x1 != b->x1)
 		 rects[count++] = Rect_new (b->x1+1,a->y0,a->x1,b->y1); // topright
		if (a->y1 != b->y1)
		 rects[count++] = Rect_new (a->x0,b->y1+1,a->x1,a->y1); //bottom
		return count;
	}
	else
	if (i==2 && j==3) {
		if (a->y0 != b->y0)
		 rects[count++] = Rect_new (a->x0,a->y0,a->x1,b->y0-1); // top
		if (a->x0 != b->x0)
		 rects[count++] = Rect_new (a->x0,b->y0,b->x0-1,a->y1); // bottomleft
		if (a->x1 != b->x1)
		 rects[count++] = Rect_new (b->x1+1,b->y0,a->x1,a->y1); // bottomright
		return count;
	}
	else
	if (i==3 && j==1) {
		if (a->x0 != b->x0)
		 rects[count++] = Rect_new (a->x0,a->y0,b->x0-1,a->y1); // left
		if (a->y1 > b->y1)
		 rects[count++] = Rect_new (b->x0,b->y1+1,a->x1,a->y1); // bottomright
		return count;
	}
	else
	if (i==3 && j==3) {
		if (a->x0 != b->x0)
		 rects[count++]  = Rect_new (a->x0,a->y0,b->x0-1,a->y1); // left
		if (a->y0 < b->y0)
		 rects[count++]  = Rect_new (b->x0,a->y0,a->x1,b->y0-1); //topright
		return count;
	}
	else
	if (i==1 && j==1) {
		if (a->x1 != b->x1)
		 rects[count++] = Rect_new (b->x1+1,a->y0,a->x1,a->y1); // right
		if (a->y1 != b->y1)
		 rects[count++] = Rect_new (a->x0,b->y1+1,b->x1,a->y1); // bottomleft
		return count;
	}
	else
	if (i==1 && j==3) {
		if (a->x1 != b->x1)
		 rects[count++] = Rect_new (b->x1+1,a->y0,a->x1,a->y1); // right
		if (a->y0 < b->y0) 
		 rects[count++] = Rect_new (a->x0,a->y0,b->x1,b->y0-1); // topleft
		return count;
	}
	else
		printf ("combo %d %d\n",i,j);
}

static void Rect_delete(Rect *r)
{
	if (r)
		free((void*)r);
}


/* Draws a portion of the background
 * If an image has been loaded, it draws that.
 * Otherwise, a green gradient.
 */
void
draw_image (Display *dpy, Window *win, short x0, short y0, short x1, short y1)
{
	int i,j;
	if (x0 > x1) {
		short tmp=x0;x0=x1;x1=tmp;
	}
	if (y0 > y1) {
		short tmp=y0;y0=y1;y1=tmp;
	}
	if (x1<0) return;
	if (y1<0) return;
	if (x0<0) x0=0;
	if (y0<0) y0=0;

	/* If we have no background image, don't sweat it, just draw
	 * a gradient
	 */
	if (!image_buffer) {
		for (i=x0; i<=x1; i++) {
			unsigned long k = (191 * i) / win_w;
			unsigned long color = (35 + k) + (90 << 8); 
			fbui_draw_vline (dpy, win, i,y0,y1, color);
		}
		fbui_flush (dpy, win);
		return;
	}

	if (x0>=image_width) return;
	if (y0>=image_height) return;
	if (x1>=image_width) x1=image_width-1;
	if (y1>=image_height) y1=image_height-1;
	
	for (j=y0; j<=y1; j++) {
		if (image_ncomponents == 1 || grayscale) {
			for(i=x0; i<x1; i++) {
				unsigned long pix=0;
				unsigned char *p = image_buffer +
					image_ncomponents*(i + j*image_width);

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

				fbui_draw_point (dpy, win, i, j, pix);
			}
		}
		else
		{
			fbui_put_rgb3 (dpy, win, x0, j, x1-x0+1,
				image_buffer + 3*(j*image_width+x0));
		}
	}

	fbui_flush(dpy,win);
}


void
draw_background (Display *dpy, Window *win)
{
	Rect *list1 = Rect_new (0,0,win_w-1,win_h-1);
	Rect *list2 = NULL;
	Rect *ptr;
	int mypid = getpid();
	int i;

	for (i=0; i<window_count; i++) {
		Rect *r0,*r1,*r2,*r3;
		struct fbui_wininfo *wi = &info[i];
		Rect *windowrect = Rect_new(wi->x,wi->y,
			wi->x+wi->width-1,wi->y+wi->height-1);
//printf ("subtracting %s\n", wi->name);
		if (wi->pid != mypid) {
			ptr = list1;
			while(ptr) {
				Rect *rects[4];
				int count = Rect_subtract (ptr,windowrect,rects);
				int j=0;
				while (j < count) {
					rects[j]->next = list2; 
					list2 = rects[j];
					j++;
				}
				ptr = ptr->next;
			}
			ptr = list1;
			Rect *next;
			while (ptr) {
				next = ptr->next;
				Rect_delete(ptr);
				ptr=next;
			}
			list1 = list2;
			list2 = NULL;

#if 0
int k=0;
Rect *n = list1;
while(n) { k++; 
printf ("   rect: %d %d - %d %d\n", n->x0,n->y0,n->x1,n->y1);
n=n->next; 
}
printf ("result list now has %d rectangles\n", k);
#endif
		}

		Rect_delete (windowrect);
	}

	/* draw the background */
	ptr=list1;
	while(ptr) {
		draw_image (dpy,win, ptr->x0,ptr->y0,ptr->x1,ptr->y1);
		ptr=ptr->next;
	}
	fbui_flush (dpy, win);

	/* remove the rectangle list */
	ptr = list1;
	while (ptr) {
		Rect *next = ptr->next;
		Rect_delete(ptr);
		ptr=next;
	}
}

