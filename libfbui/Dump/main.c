
/*=========================================================================
 *
 * fbdump, a screen dumper for FBUI (in-kernel framebuffer UI)
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


/* Copyright (C) 2004 by Zack T Smith */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <linux/fb.h>
#include <jpeglib.h>


JSAMPLE * image_buffer;
int image_height;       
int image_width; 

static struct fb_fix_screeninfo fi;
static struct fb_var_screeninfo vi;


extern int read_JPEG_file (char*);

int
main(int argc, char** argv)
{
	int i,j;
	char *path="dump.jpg";
	int fd;
	unsigned char *buf = NULL;
	unsigned long buflen = 0;
	int bpp;
	int bytes_per_pixel;

	image_buffer=NULL;
	image_height=0;
	image_width=0;

	i=1;
	while(i<argc) {
		if ('-' != *argv[i]) {
			path=argv[i];
			break;
		}
		i++;
	}

        fd = open ("/dev/fb0", O_RDWR );
        if (fd == -1)
                fd = open ("/dev/fb/0", O_RDWR );
        if (fd == -1)
        {
                perror("open");
                exit(1);
        }
        else
        {
                if (ioctl (fd, FBIOGET_FSCREENINFO, &fi))
                {
                        perror("ioctl");
                	exit(1);
                }
                else
                if (ioctl (fd, FBIOGET_VSCREENINFO, &vi))
                {
                        perror("ioctl");
                	exit(1);
                }
                else
                {
                        if (fi.visual != FB_VISUAL_TRUECOLOR &&
                            fi.visual != FB_VISUAL_DIRECTCOLOR )
                        {
				fprintf (stderr, "error: need direct/truecolor display\n");
                		exit(1);
                        }
                        else
                        {
                                buf = (unsigned char*) fi.smem_start;
                                buflen = fi.smem_len;
                                buf = mmap (buf, buflen,
                                        PROT_WRITE | PROT_READ,
                                        MAP_SHARED, fd, 0);
                                if (buf == MAP_FAILED)
                                {
					perror ("mmap");
					exit(1);
                                }
			}
		}
	}

	printf ("Framebuffer resolution: %dx%d, %dbpp\n",
		vi.xres, vi.yres, vi.bits_per_pixel);

	image_width = vi.xres;
	image_height = vi.yres;
	bpp = vi.bits_per_pixel;
	bytes_per_pixel = (bpp+7)>>3;

	image_buffer = (JSAMPLE*) malloc (image_width * image_height * 3);
	if (!image_buffer) {
		fprintf (stderr,"error: cannot allocate pixel buffer\n");
		exit(1);
	}

	unsigned char ro = vi.red.offset;
	unsigned char go = vi.green.offset;
	unsigned char bo = vi.blue.offset;
	unsigned char rl = vi.red.length;
	unsigned char gl = vi.green.length;
	unsigned char bl = vi.blue.length;

	unsigned long ix = 0;

	for (j=0; j<image_height; j++) {
		unsigned char *ptr = buf + fi.line_length * j;
		unsigned long value=0;
		int x=0;

		while (x < image_width) {
			unsigned long value=0;
			unsigned long a0,a1=0,a2=0,a3=0;
			switch (bytes_per_pixel) {
			case 4:
				a0 = *ptr++;
				a1 = *ptr++;
				a2 = *ptr++;
				a3 = *ptr++;
				break;
			case 3:
				a0 = *ptr++;
				a1 = *ptr++;
				a2 = *ptr++;
				break;
			case 2:
				a0 = *ptr++;
				a1 = *ptr++;
				break;
			case 1:
				a0 = *ptr++;
				break;
			}
			a1 <<= 8;
			a2 <<= 16;
			a3 <<= 24;
			value = a0|a1|a2|a3;

			unsigned long r,g,b;
			r = value >> ro;
			g = value >> go;
			b = value >> bo;
			r <<= (8 - rl);
			g <<= (8 - gl);
			b <<= (8 - bl);

			image_buffer[ix++] = r;
			image_buffer[ix++] = g;
			image_buffer[ix++] = b;
			x++;
		}
	}

	write_JPEG_file (path,999);
	close(fd);

	return 0;
}
