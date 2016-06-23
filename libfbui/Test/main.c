
/*=========================================================================
 *
 * fbtest, a test for FBUI (in-kernel framebuffer UI)
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




void
random_rect_test(Display* dpy, Window *win)
{
	int i=0;		
	srand(1243+time(NULL));
	printf ("50000 random rectangles\n");
	while (i++ < 50000)
	{
		int x0 = rand();
		int y0 = rand();
		int x1 = rand();
		int y1= rand();
		x0 &= 2047;
		x1 &= 2047;
		y0 &= 2047;
		y1 &= 2047;
		x0 -= 800;
		y0 -= 800;
		x1 -= 800;
		y1 -= 800;
		fbui_draw_rect (dpy,win,x0,y0,x1,y1,rand());
	}
}



int
main(int argc, char** argv)
{
	int i;
	int vc=-1;

	Display *dpy;
	Window *win;
	Font *pcf = font_new ();

	if (!pcf_read (pcf, "timR12.pcf")) {
		font_free (pcf);
		pcf = NULL;
		FATAL ("cannot load font");
	}

	short win_w, win_h;
	long fg,bg;
	bg=0x303030;

	dpy = fbui_display_open ();
	if (!dpy) 
		FATAL ("cannot open display");

	win = fbui_window_open (dpy, 300,200, &win_w, &win_h, 9999,9999, 370, 15, 
		&fg, &bg, "fbtest", "", 
		FBUI_PROGTYPE_APP, false,false, vc,
		false, false,false,
		argc,argv);
	if (!win) 
		FATAL ("cannot create window");

	int whichtest = argc >= 2 ? atoi(argv[1]) : 0;

	if (!whichtest || whichtest==1)
	{
		printf ("100000 lines\n");
		for (i=0;i<100000; i++)
		{
			short x0 = (rand() & 2047) - 800;
			short y0 = (rand() & 2047) - 800;
			short x1 = (rand() & 2047) - 800;
			short y1 = (rand() & 2047) - 800;
			fbui_draw_line (dpy, win, x0,y0,x1,y1,rand());
		}
	}

	if (!whichtest || whichtest==2)
	{
		printf ("500000 hlines\n");
		int j;
		for(j=0; j<1000; j++)
		{
			i=0;
			unsigned long color=rand();
			while(i < 500)
			{
				int x1 = rand() % 800;
				int x2 = rand() % 800;
				int y = rand() % 600;
				fbui_draw_hline (dpy, win, x1,x2,y,color);
				i++;
			}
			fbui_flush (dpy, win);
			i=0;
			while(i < 500)
			{
				fbui_draw_hline (dpy, win, 0, i, i,color); 
				i++;
			}
			fbui_flush (dpy, win);
		}
	}


	if (!whichtest || whichtest==3)
	{
                if(pcf)
                {
			fbui_draw_line(dpy, win, 0,0,799,599,RGB_RED);
			fbui_draw_line(dpy, win, 799,0,0,599,RGB_RED);

			int n=0;
			while(n<200) {
                        	int y = 0;

				while (y<600) {
#define TESTSTR "01234 ? ABC_EFGHI ~` JKLMNOPQRSTItest m,yYqpaafgjklz<>?!@#$%^&*()~"
					int rv = fbui_draw_string (dpy, win, pcf,
						-100,y, TESTSTR, rand() & 0xffffff);

					y += pcf->ascent + pcf->descent;
				}
				n++;
			}
                }
                if (pcf) font_free (pcf);

	}


	if (!whichtest || whichtest==4) 
	{
printf ("rects\n");
		random_rect_test(dpy, win);
	}

	if (!whichtest || whichtest==5)
	{
		printf ("10000 random filled rectangles\n");
		for (i=0; i<10000; i++)
		{
			fbui_fill_area (dpy,win, rand() % 1000,rand()%700,rand()%900,rand()%500,
				rand());
		}
	}

	if (!whichtest || whichtest==6)
	{
		fbui_fill_area (dpy, win, 0,0,799,599,RGB_ORANGE);

		static unsigned char drawthis[256*3];
		for (i=0; i<256; i++) {
			drawthis[i*3] = i;
			drawthis[i*3+1] = 255-i;
			drawthis[i*3+2] = i/2;
		}

		printf ("10000 256x16 put-native operations\n");

		i=0;
		while(i<10000)
		{
			int j=0;
			short x = rand() % 800;
			short y = rand() % 600;
			while (j<16) {
				int tmp = fbui_put (dpy, win, x,j+y,256, drawthis );
				if (tmp != FBUI_SUCCESS) {
					fbui_window_close(dpy, win);
					printf ("error %d from fbui_put\n", tmp);
					exit(0);
				}
				j++;
			}
			i++;
		}
	}

	if (!whichtest || whichtest==7)
	{
		printf ("5000 fullscreen filled rectangles\n");
		for (i=0; i<5000; i++)
		{
			fbui_fill_area (dpy, win, -20,-20,1000,999,rand());
		}
	}

	if (!whichtest || whichtest==8)
	{
		// draw 100,000 pixels
		i=0;
		while (i < 100000)
		{
			fbui_draw_point (dpy, win, i/2000,i&511, i & 0xff);
			i++;
		}
	}

	if (!whichtest || whichtest==9)
	{
		fbui_fill_area (dpy, win, 0,0,398,298, RGB_RED);
		fbui_fill_area (dpy, win, 400,0,799,298, RGB_GREEN);
		fbui_fill_area (dpy, win, 0,300,398,599, RGB_BLUE);
		fbui_fill_area (dpy, win, 400,300,799,599, RGB_YELLOW);

		printf ("10000 inverted lines\n");
		for (i=0;i<10000; i++)
		{
			short x0 = (rand() & 2047) - 800;
			short y0 = (rand() & 2047) - 800;
			short x1 = (rand() & 2047) - 800;
			short y1 = (rand() & 2047) - 800;

			fbui_invert_line (dpy, win, x0,y0,x1,y1);
		}
	}

	if (!whichtest || whichtest==10)
	{
		unsigned long drawthis[256];
		for (i=0; i<256; i++) {
			unsigned long value = i/2;
			value <<= 8;
			value |= 255-i;
			value <<= 8;
			value |= 192;
			drawthis[i] = value;
		}

		printf ("50000 256x16 put-rgb operations\n");

		i=0;
		while(i<50000)
		{
			int j=0;
			int x = (rand() & 2047) - 800;
			int y = (rand() & 2047) - 800;
			while(j<16) {
				fbui_put_rgb (dpy, win, x,y+j,256, drawthis );
				j++;
			}
			i++;
		}
	}

	if (!whichtest || whichtest==11) 
	{
		fbui_draw_line (dpy, win, 0,0,799,599, RGB_BLUE);
		fbui_draw_line (dpy, win, 799,0,0,599, RGB_GREEN);

		for (i=0; i<6; i++)
			fbui_copy_area (dpy, win, 0,10,0,0,800,600);
	}

	if (!whichtest || whichtest==12) 
	{
		fbui_draw_line (dpy, win, 0,0,799,599, RGB_BLUE);
		fbui_draw_line (dpy, win, 799,0,0,599, RGB_BLUE);

		for (i=0; i<6; i++)
			fbui_copy_area (dpy, win, 0,0,0,10,800,800);
	}

	if (!whichtest || whichtest==13) 
	{
		fbui_draw_line (dpy, win, 0,0,799,599, RGB_YELLOW);
		fbui_draw_line (dpy, win, 799,0,0,599, RGB_ORANGE);

		for (i=0; i<6; i++)
			fbui_copy_area (dpy, win, 0,0,10,0,800,800);
	}

	if (!whichtest || whichtest==14) 
	{
		fbui_draw_line (dpy, win, 0,0,799,599, RGB_BLUE);
		fbui_draw_line (dpy, win, 799,0,0,599, RGB_BLUE);

		for (i=0; i<6; i++)
			fbui_copy_area (dpy, win, 10,0,0,0,800,800);
	}

	if (!whichtest || whichtest==15)
	{
		static unsigned char drawthis[768];
		for (i=0; i<768; ) {
			drawthis[i++] = i/6;
			drawthis[i++] = 255-i/3;
			drawthis[i++] = 0xff;
		}

		printf ("50000 256x16 put-rgb operations\n");

		i=0;
		while(i<50000)
		{
			int j=0;
			int x = (rand() & 2047) - 800;
			int y = (rand() & 2047) - 800;
			while(j<16) {
				fbui_put_rgb3 (dpy, win, x,y+j,256, drawthis );
				j++;
			}
			i++;
		}
	}

	if (!whichtest || whichtest==16)
	{
		int j=256;

		printf ("vlines\n");

#if 1
		for (j=0; j<256; j++) {
			i=0;
			unsigned long color = (rand() & 0xffff00) | (j & 255);
			while(i<5000)
			{
				fbui_draw_vline (dpy, win, i-1000,i-2000,i-1000,color);
				i++;
			}
			fbui_flush (dpy, win);
		}
#endif
		fbui_clear (dpy, win);
		fbui_flush (dpy, win);
		for (j=0 ; j<640; j++)
		{
			double r;
			int factor = j / 160;
			i=0;
			unsigned long color=rand();
			while(i<win_w)
			{
				r = i;
				r /= win_w/4;
				r *= 3.1415926536;
				r *= factor;
				r = sin(r);
				int y = r*(win_h/2) + (win_h/2);
				fbui_draw_vline (dpy, win, i, (j&1) ? 0 : win_h, y,color);
				i++;
			}
			fbui_flush (dpy, win);
		}
		fbui_draw_vline (dpy, win, 100, 0, 100, RGB_WHITE);
		fbui_draw_vline (dpy, win, 110, 100, 0, RGB_WHITE);
		fbui_flush (dpy, win);
	}

	fbui_window_close(dpy, win);
	fbui_display_close (dpy);

	return 0;
}
