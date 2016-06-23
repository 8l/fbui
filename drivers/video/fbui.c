
/*=========================================================================
 *
 * Module name:		fbui
 * Module purpose:	Provides in-kernel drawing routines to userland,
 *			supporting 8/16/24/32 bit packed pixels.
 * Module originator:	Zachary T Smith (fbui@comcast.net)
 *
 *=========================================================================
 *
 * FBUI, an in-kernel graphical user interface for applications
 * Copyright (C) 2003-2005 Zachary T Smith, fbui@comcast.net
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
 *=========================================================================
 *
 * Changes:
 *
 * Sep 12, 2004, fbui@comcast.net: added fbui_copy_area
 * Sep 13, 2004, fbui@comcast.net: added fbui_draw_string
 * Sep 13, 2004, fbui@comcast.net: added sharing of fbui between processes
 * Sep 14, 2004, fbui@comcast.net: added fbui_switch
 * Sep 16, 2004, fbui@comcast.net: added fbui_open/close
 * Sep 16, 2004, fbui@comcast.net: use of KD_GRAPHICS
 * Sep 16, 2004, fbui@comcast.net: return to KD_TEXT after all closed
 * Sep 18, 2004, fbui@comcast.net: added event types
 * Sep 20, 2004, fbui@comcast.net: added window-data semaphore
 * Sep 21, 2004, fbui@comcast.net: added keyboard focus concept
 * Sep 22, 2004, fbui@comcast.net: keyboard reading via tty->read_buf
 * Sep 22, 2004, fbui@comcast.net: keyboard mode set to XLATE
 * Sep 22, 2004, fbui@comcast.net: simple keyboard behavior achieved
 * Sep 23, 2004, fbui@comcast.net: added implicit keyfocus when only 1 win
 * Sep 24, 2004, fbui@comcast.net: fbui_open() now accepts target console#
 * Sep 25, 2004, fbui@comcast.net: added read_point
 * Sep 25, 2004, fbui@comcast.net: expanded role of window manager
 * Sep 25, 2004, fbui@comcast.net: added fbui_put_rgb3
 * Sep 25, 2004, fbui@comcast.net: coordinates now truly window-relative
 * Sep 26, 2004, fbui@comcast.net: added per-console winlists
 * Sep 27, 2004, fbui@comcast.net: keyfocus_stack
 * Sep 28, 2004, fbui@comcast.net: control ioctl
 * Sep 29, 2004, fbui@comcast.net: window semaphore; window resize, move
 * Oct 01, 2004, fbui@comcast.net: all drawing commands now atomic
 * Oct 01, 2004, fbui@comcast.net: fixed put_rgb bug
 * Oct 02, 2004, fbui@comcast.net: added fbui_window_info()
 * Oct 04, 2004, fbui@comcast.net: structural changes
 * Oct 05, 2004, fbui@comcast.net: keyboard focus working
 * Oct 05, 2004, fbui@comcast.net: added fbui_winptrs_change()
 * Oct 05, 2004, fbui@comcast.net: added keyboard accelerators
 * Oct 06, 2004, fbui@comcast.net: moved code to pixel_from_rgb
 * Oct 06, 2004, fbui@comcast.net: removed read_point (mmap replaces it)
 * Oct 06, 2004, fbui@comcast.net: created fb_hline
 * Oct 06, 2004, fbui@comcast.net: fb_clear now supports info->bgcolor
 * Oct 10, 2004, fbui@comcast.net: added support for retrieving mouse info
 * Oct 12, 2004, fbui@comcast.net: resolved bug to do with multiple down's
 * Oct 12, 2004, fbui@comcast.net: switched to rw_semaphores
 * Oct 14, 2004, fbui@comcast.net: got fbui_input module working
 * Oct 14, 2004, fbui@comcast.net: mouse data now available as events
 * Oct 14, 2004, fbui@comcast.net: key data now available as events
 * Oct 15, 2004, fbui@comcast.net: put in basic mouse-pointer logic
 * Oct 16, 2004, fbui@comcast.net: fbui-inp now calls fbui to provide events
 * Oct 17, 2004, fbui@comcast.net: perfected software mouse-pointer logic
 * Oct 17, 2004, fbui@comcast.net: restructuring: now only fb_* in fbops.
 * Oct 17, 2004, fbui@comcast.net: added window-moved event.
 * Oct 18, 2004, fbui@comcast.net: fixed keyboard accelerators post inp-hand
 * Oct 20, 2004, fbui@comcast.net: wait-for-event working
 * Oct 20, 2004, fbui@comcast.net: accelerators are now for chars [32,128)
 * Oct 21, 2004, fbui@comcast.net: revisions to support panel manager
 * Oct 22, 2004, fbui@comcast.net: removed keyfocus stack
 * Oct 23, 2004, fbui@comcast.net: fixed tty: NULL is now used
 * Oct 25, 2004, fbui@comcast.net: key queue is now per-console
 * Oct 26, 2004, fbui@comcast.net: putpixels now takes in-kernel data
 * Oct 26, 2004, fbui@comcast.net: putpixels_rgb takes 0% & 100% transparency
 * Oct 26, 2004, fbui@comcast.net: added fb_getpixels_rgb
 * Oct 26, 2004, fbui@comcast.net: mouse pointer now drawn using putpixels
 * Oct 26, 2004, fbui@comcast.net: conversion to multiple windows per process
 * Oct 26, 2004, fbui@comcast.net: added cut/paste logic
 * Nov 11, 2004, fbui@comcast.net: left-bearing for drawn characters!
 * Nov 18, 2004, fbui@comcast.net: added mouse-motion event
 * Nov 24, 2004, fbui@comcast.net: added fbui-specific logfile
 * Nov 29, 2004, fbui@comcast.net: added proper event queues for windows.
 * Dec 12, 2004, fbui@comcast.net: added ability to open hidden window
 * Dec 13, 2004, fbui@comcast.net: event queues are now per-process
 * Dec 13, 2004, fbui@comcast.net: Enter event now generated after switch
 * Dec 13, 2004, fbui@comcast.net: added fbui_tinyblit
 * Dec 14, 2004, fbui@comcast.net: Enter/Leave after ptr hide/unhide
 * Dec 15, 2004, fbui@comcast.net: wait-for-event checks all pid's windows
 * Dec 16, 2004, fbui@comcast.net: processentry nwindows now correct
 * Dec 18, 2004, fbui@comcast.net: processentry now has event mask
 * Dec 21, 2004, fbui@comcast.net: optimization for tinyblit
 * Dec 28, 2004, fbui@comcast.net: added fbui_clean to remove zombie data
 * Dec 31, 2004, fbui@comcast.net: added process_exists
 * Jan 01, 2005, fbui@comcast.net: added PrtSc accelerator
 * Jan 01, 2005, fbui@comcast.net: added SUBTITLE and SETFONT cmds
 * Jan 02, 2005, fbui@comcast.net: unhide now performs overlap check
 * Jan 03, 2005, fbui@comcast.net: added mode24 for slight speedup
 * Jan 03, 2005, fbui@comcast.net: added pointerfocus and receive_all_motion
 * Jan 04, 2005, fbui@comcast.net: fixed tinyblit bug
 * Jan 05, 2005, fbui@comcast.net: separated button events from keys
 * Jan 13, 2005, fbui@comcast.net: minor tweaks
 */


/*
 * Note:
 *
 * This code currently works for 8/16/24/32 little endian packed pixels only.
 * I have some 4-bit VGA code which I may add later.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/ctype.h>
#include <linux/input.h>	/* struct input_event */
#include <linux/sem.h>
#include <linux/delay.h>
#include <linux/pid.h>	/* find_pid */

/* Variables for input_handler */
static char fbui_handler_regd = 0;
static char got_rel_x = 0;
static char got_rel_y = 0;
static short incoming_x = 0;
static short incoming_y = 0;
static char altdown = 0;
static char intercepting_accel = 0;



#define FBUI_VERSION "0.9.14"



/* Mouse-pointer */
#define PTRWID 10
#define PTRHT 16
static unsigned long pointer_saveunder[256]; /* max 16x16 */


static void fbui_enable_pointer (struct fb_info *info);
static int fbui_clear (struct fb_info *info, struct fbui_window *win);
static int fbui_clear_area (struct fb_info *info, struct fbui_window *win,
	short x0, short y0, short x1, short y1);
static int fbui_fill_area (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color);
static int fbui_draw_point (struct fb_info *info, struct fbui_window *win, 
	short x, short y, u32 color);
static int fbui_draw_line (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color);
static int fbui_copy_area (struct fb_info *info, struct fbui_window *win, 
	short xsrc,short ysrc,short w, short h, short xdest,short ydest);
static int fbui_draw_vline (struct fb_info *info, struct fbui_window *win, 
	short x, short y0, short y1, u32 color);
static int fbui_draw_hline (struct fb_info *info, struct fbui_window *win, 
	short x0, short x1, short y, u32 color);
static int fbui_draw_rect (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color);
static int fbui_put_rgb (struct fb_info *info, struct fbui_window *win, 
	short x, short y,short n, unsigned long *src);
static int fbui_put_rgb3 (struct fb_info *info, struct fbui_window *win, 
	short x,short y, short n, unsigned char *src);
static int fbui_put (struct fb_info *info, struct fbui_window *win, 
	short x,short y, short n, unsigned char *src);
static int fbui_copy_area (struct fb_info *info, struct fbui_window *win,
		short xsrc,short ysrc,short w, short h, 
		short xdest,short ydest);
static int fbui_draw_string (struct fb_info *info, struct fbui_window *win,
	struct fbui_font *font,
	short x, short y, unsigned char *str, u32 color);
static int fbui_tinyblit (struct fb_info *info, struct fbui_window *win, 
	short x, short y, short width, u32 color, u32 bgcolor, u32 bitmap);
static struct fbui_processentry *alloc_processentry (struct fb_info *info, int pid, int cons);
static void free_processentry (struct fb_info *info, struct fbui_processentry *pre);
static struct fbui_window *get_pointer_window (struct fb_info *info);
static struct fbui_window *
   accelerator_test (struct fb_info *info, int cons, unsigned char);
static int fbui_clean (struct fb_info *info, int cons);
static int fbui_remove_win (struct fb_info *info, short win_id, int);




static int process_exists (int pid)
{
	struct pid *pidptr;
	read_lock_irq(&tasklist_lock);
	pidptr = find_pid (PIDTYPE_PID, pid);
	read_unlock_irq(&tasklist_lock);
	return pidptr != NULL;
}


#if 0
static struct fbui_processentry *lookup_processentry_by_pid (struct fb_info *info,int pid)
{
	int i=0;
	struct fbui_processentry *pre;

	if (!info)
		return NULL;
	/*----------*/

        while (i < FBUI_MAXCONSOLES * FBUI_MAXWINDOWSPERVC) {
                pre = &info->processentries[i];
                if (pre->in_use && pid == pre->pid)
                        return pre;
		i++;
	}
	return NULL;
}
#endif


static void fbui_enqueue_event (struct fb_info *info, struct fbui_window *win, 
                           struct fbui_event *ev, int inside_IH)
{
	struct fbui_processentry *pre;
	unsigned long flags = 0;
	short head;
	spinlock_t mylock = SPIN_LOCK_UNLOCKED;

	if (!info || !win || !ev)
		return;
	pre = win->processentry;
	if (!pre) {
		printk (KERN_INFO "fbui_enqueue_event: no processentry\n");
		return;
	}
	if (!pre->in_use) {
		printk (KERN_INFO "fbui_enqueue_event: processentry not in use\n");
		return;
	}
	if (pre->events_pending >= FBUI_MAXEVENTSPERPROCESS) {
		/*printk (KERN_INFO "fbui_enqueue_event: event buffer overflow for process %d, event type %d\n", pre->pid, ev->type);*/
		return;
	}
	/*----------*/

	if (!inside_IH) {
		down (&pre->queuesem);
		spin_lock_irqsave(&mylock, flags); 
	}

	ev->id = win->id;
	ev->pid = win->pid;
	head = pre->events_head;
	memcpy (&pre->events[head], ev, sizeof (struct fbui_event));
	pre->events_head = (head + 1) % FBUI_MAXEVENTSPERPROCESS;
	pre->events_pending++;

/*printk(KERN_INFO "enqueue: window %d event %d pending=%d, enqueue at head %d\n", win->id, ev->type, pre->events_pending, head); */

	if (!inside_IH) {
		spin_unlock_irqrestore(&mylock, flags); 
		up (&pre->queuesem);
	}

	if (pre->waiting) {
		pre->waiting = 0;
		wake_up_interruptible (&pre->waitqueue);
	}
}


static int fbui_dequeue_event (struct fb_info *info, struct fbui_processentry *pre,
                               struct fbui_event *ev)
{
	short tail;
	unsigned long flags;

	if (!info || !pre || !ev)
		return 0;

	if (pre->events_pending <= 0) {
/* printk(KERN_INFO "fbui_dequeue_event: no events pending\n");  */
		return 0;
	}
	/*----------*/

	/* Remove an event from the event queue for this process,
	 * locking out input_handler which writes to the queue,
	 * and locking out via a semaphore any other process
	 * either reading or writing the queue.
	 */
	down (&pre->queuesem);
	spin_lock_irqsave(&pre->queuelock, flags); 

	tail = pre->events_tail;
	memcpy (ev, &pre->events[tail], sizeof (struct fbui_event));
/* printk(KERN_INFO "dequeue for window %d: pending %d, tail %d\n", ev->id,pre->events_pending, tail); */
	pre->events_tail = (tail + 1) % FBUI_MAXEVENTSPERPROCESS;
	--pre->events_pending;

	spin_unlock_irqrestore (&pre->queuelock, flags);
	up (&pre->queuesem);

/* printk (KERN_INFO "dequeue: ev { type %d key %d pid %d id %d } current->pid %d pre { pid %d ix %d}\n",ev->type,ev->key,ev->pid,ev->id,current->pid,pre->pid,pre->index); */

	return 1;
}


u32 pixel_from_rgb (struct fb_info *info, u32 value)
{
	u32 r,g,b;
	if (!info->mode24) {
		unsigned char tmp = 0xff;
		b = tmp & value;
		value >>= 8;
		g = tmp & value;
		value >>= 8;
		r = tmp & value;
		tmp = 8;
		r >>= (tmp - info->redsize);
		g >>= (tmp - info->greensize);
		b >>= (tmp - info->bluesize);
		r <<= info->redshift;
		g <<= info->greenshift;
		b <<= info->blueshift;
		return r | g | b;
	} else {
		return value & 0xffffff;
	}
}

u32 pixel_to_rgb (struct fb_info *info, u32 value)
{
	u32 r,g,b;
	if (!info->mode24) {
		unsigned char tmp = 8;
		r = value >> info->redshift;
		g = value >> info->greenshift;
		b = value >> info->blueshift;
		r &= (1 << info->redsize) - 1;
		g &= (1 << info->bluesize) - 1;
		b &= (1 << info->greensize) - 1;
		r <<= (tmp - info->redsize);
		g <<= (tmp - info->greensize);
		b <<= (tmp - info->bluesize);
		r <<= 16;
		g <<= 8;
		return r | g | b;
	} else {
		return value & 0xffffff;
	}
}


void fb_point (struct fb_info *info, short x, short y, u32 color, char do_invert)
{
	short xres, yres;
	u32 bytes_per_pixel, offset;
	unsigned char *ptr;

	if (!info)
		return;
	xres = info->var.xres;
	yres = info->var.yres;
	if (x < 0 || y < 0 || x >= xres || y >= yres)
		return;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        offset = y * info->fix.line_length + x * bytes_per_pixel;
        ptr = ((unsigned char*)info->screen_base);
	if (!ptr)
		return;

        ptr += offset;

	if (do_invert)
	{
		switch (bytes_per_pixel)
		{
		case 1: fb_writeb(0xff ^ fb_readb (ptr), ptr); break;
		case 2: fb_writew(0xffff ^ fb_readw (ptr), ptr); break;
		case 4: fb_writel(0xffffffff ^ fb_readl (ptr), ptr); break;
		case 3:
			fb_writeb(0xff ^ fb_readb (ptr), ptr); ptr++;
			fb_writeb(0xff ^ fb_readb (ptr), ptr); ptr++;
			fb_writeb(0xff ^ fb_readb (ptr), ptr);
			break;
		}
	}
	else
	{
		u32 c = pixel_from_rgb (info, color);
		switch (bytes_per_pixel)
		{
		case 1:	fb_writeb (c, ptr); break;
		case 2:	fb_writew (c, ptr); break;
		case 4:	fb_writel (c, ptr); break;
		case 3: 
			fb_writeb (c, ptr); ptr++; c >>= 8;
			fb_writeb (c, ptr); ptr++; c >>= 8;
			fb_writeb (c, ptr); 
			break;
		}
        }
}


void fb_hline (struct fb_info *info, short x0, short x1, short y, u32 color)
{
	u32 pixel, bytes_per_pixel, offset;
	unsigned char *ptr;
	int i;

	if (!info)
		return;

	if (x0 > x1) {
		short tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
	offset = y * info->fix.line_length + x0 * bytes_per_pixel;
	ptr = ((unsigned char*)info->screen_base);
	if (!ptr) 
		return;
	ptr += offset;

	i= x1-x0+1;
	pixel = pixel_from_rgb (info, color);
	switch (bytes_per_pixel) {
	case 4: {
		register unsigned long* ptr2 = (unsigned long*) ptr;
		register u32 pix = pixel;
		while (i) {
			if (i > 8) {
				/* useful on older processors */
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				fb_writel (pix, ptr2); ptr2++;
				i -= 8;
			} else {
				fb_writel (pix, ptr2); ptr2++;
				i--;
			}
		}
		break;
	}

	case 3: {
		u32 r = 0xff & pixel; 
		u32 g = 0xff & (pixel >> 8); 
		u32 b = 0xff & (pixel >> 16);
		register u32 long1,long2,long3;
		register unsigned char third = x0 % 3;

		long1 = r | (g<<8) | (b<<16) | (r<<24);
		long2 = g | (b<<8) | (r<<16) | (g<<24);
		long3 = b | (r<<8) | (g<<16) | (b<<24);
		while (i) {
			if (!third && i>=4) {
				unsigned long *ptr2 = (unsigned long*) ptr;
				while (i>=12) {
					/* useful on older processors */
					fb_writel (long1, ptr2); ptr2++;
					fb_writel (long2, ptr2); ptr2++;
					fb_writel (long3, ptr2); ptr2++;
					fb_writel (long1, ptr2); ptr2++;
					fb_writel (long2, ptr2); ptr2++;
					fb_writel (long3, ptr2); ptr2++;
					fb_writel (long1, ptr2); ptr2++;
					fb_writel (long2, ptr2); ptr2++;
					fb_writel (long3, ptr2); ptr2++;
					i -= 12;
					ptr += 36;
				}
				while (i>=4) {
					fb_writel (long1, ptr2); ptr2++;
					fb_writel (long2, ptr2); ptr2++;
					fb_writel (long3, ptr2); ptr2++;
					i -= 4;
					ptr += 12;
				}
			} else {
				u32 c = pixel;
				fb_writeb (c, ptr); ptr++; c >>= 8;
				fb_writeb (c, ptr); ptr++; c >>= 8;
				fb_writeb (c, ptr); ptr++;
				i--;
				third++;
				if (third==3) third=0;
			}
		}
		break;
	}

	case 2: {
		u32 c32 = (pixel & 0xffff) | (pixel << 16);
		while (i) {
			if (i>=2 && !(3 & (u32)ptr)) {
				if (i>=16) {
					u32 *ptr2 = (u32*)ptr;
					/* useful on older processors */
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					ptr += 32;
					i -= 16;
				}
				if (i>=2) {
					fb_writel (c32, ptr);
					ptr += 4;
					i -= 2;
				}
			} else {
				u32 c = pixel;
				fb_writeb (c, ptr); ptr++; c >>= 8;
				fb_writeb (c, ptr); ptr++; 
				i--;
			}
		}
		break;
	}

	case 1: {
		u32 c32;
		pixel &= 0xff;
		c32 = pixel | (pixel<<8) | (pixel<<16) || (pixel<<24);
		while (i) {
			if (!(3 & (u32)ptr)) {
				if (i>=32) {
					u32 *ptr2 = (u32 *)ptr;
					/* useful on older processors */
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					fb_writel (c32, ptr2); ptr2++;
					ptr += 32;
					i -= 32;
				}
				if (i>=4) {
					fb_writel (c32, ptr);
					ptr += 4;
					i -= 4;
				}
			} else {
				fb_writel (pixel, ptr); ptr++;
				i--;
			}
		}
		break;
	}

	}/*switch*/
}


void fb_vline (struct fb_info *info, short x, short y0, short y1, u32 color)
{
	u32 pixel, linelen, bytes_per_pixel, offset;
	unsigned char *ptr;
	short i;
	short xres,yres;

	if (!info)
		return;
	xres = info->var.xres;
	if (x < 0 || x >= xres)
		return;
	if (y0 > y1) {
		short tmp = y0;
		y0 = y1;
		y1 = tmp;
	}
	if (y1 < 0)
		return;
	yres = info->var.yres;
	if (y0 >= yres)
		return;
	if (y0 < 0)
		y0 = 0;
	if (y1 >= yres)
		y1 = yres-1;
	/*----------*/

	linelen = info->fix.line_length;
	bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
	offset = y0 * linelen + x * bytes_per_pixel;
	ptr = info->screen_base;
	ptr += offset;

	i = y1-y0+1;
	pixel = pixel_from_rgb (info, color);

	switch (bytes_per_pixel) {
	case 4:
		while (i--) {
			fb_writel (pixel, ptr);
			ptr += linelen;
		}
		break;

	case 3:
		while (i--) {
			register u32 c = pixel;
			fb_writeb (c, ptr); ptr++; c >>= 8;
			fb_writeb (c, ptr); ptr++; c >>= 8;
			fb_writeb (c, ptr); 
			ptr += linelen;
			ptr -= 2;
		}
		break;

	case 2:
		while (i--) {
			fb_writew (pixel, ptr);
			ptr += linelen;
		}
		break;

	case 1:
		while (i--) {
			fb_writeb (pixel, ptr);
			ptr += linelen;
		}
		break;
	}

	return;
}


void fb_clear (struct fb_info *info, u32 color_)
{
	u32 color = color_;
	short y, ylim, xlim;

	if (!info) 
		return;
	if (info->state != FBINFO_STATE_RUNNING) 
		return;
	/*----------*/

	y = 0;
	ylim = info->var.yres;
	xlim = info->var.xres - 1;

	while (y < ylim)
		fb_hline (info, 0, xlim, y++, color);
}


/* Parameter cons is the VC we are switching to;
 * info->currcon is the VC we are switching from.
 */
int fbui_switch (struct fb_info *info, int cons)
{
	struct fbui_window *win;
	struct fbui_event ev;
	int i,lim;

	if (!info)
		return 0;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADVC;
	if (!vc_cons_allocated (cons))
		return 0;
	/*----------*/

	info->pointer_active = 0;
	intercepting_accel = 0;
	altdown = 0;

	/* Clean up the new console
	 */
	fbui_clean (info, cons);

	/* Send Leave event to whichever window had gotten an Enter.
	 */
	down_read (&info->winptrSem);
	win = info->pointer_window [info->currcon];
	up_read (&info->winptrSem);
	if (win) {
		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_LEAVE;

		win->pointer_inside = 0;
		down_write (&info->winptrSem);
		info->pointer_window [info->currcon] = NULL;
		up_write (&info->winptrSem);

		fbui_enqueue_event (info, win, &ev, 0);
	}

	info->currcon = cons;

	/* This routine is called whenever there's a switch,
	 * not just when we're switching to a graphics console.
	 */
	if (vt_cons[cons]->vc_mode != KD_GRAPHICS)
		return 1;

	/* Former value no longer valid */
	down_write (&info->winptrSem);
	info->pointer_window [cons] = NULL;
	up_write (&info->winptrSem);

	fb_clear (info, info->bgcolor[cons]);

	/* Firstly perform the clears
	 */
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	down_read (&info->winptrSem);
	for (; i < lim; i++) {
		struct fbui_window *ptr = 
			info->windows [i];

		if (ptr && !ptr->is_wm && !ptr->is_hidden)
			fbui_clear (info, ptr);
	}
	up_read (&info->winptrSem);

	/* Secondly sent the expose events
	 */
	memset (&ev, 0, sizeof (struct fbui_event));
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	down_read (&info->winptrSem);
	for (; i < lim; i++) {
		struct fbui_window *ptr = 
			info->windows [i];

		if (ptr) {
			ev.type = FBUI_EVENT_EXPOSE;

			ptr->pointer_inside = 0;

			/* No need to expose if hidden */
			if (!ptr->is_hidden)
				fbui_enqueue_event (info, ptr, &ev, 0);
		}
	}
	up_read (&info->winptrSem);

	info->pointer_active = 0;
	fbui_enable_pointer (info);

	/* If the pointer is on top of a window, 
	 * send that window an Enter event.
	 */
	down_read (&info->winptrSem);
	win = get_pointer_window (info);
	up_read (&info->winptrSem);
	if (win) {
		struct fbui_event ev;
		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_ENTER;
		fbui_enqueue_event (info, win, &ev, 0);
	}

	return 1;
}


static int pointer_in_window (struct fb_info *info, struct fbui_window *win, char just_tip)
{
	register short mx,my,mx1,my1;
	short x0,y0,x1,y1;

	if (!info || !win)
		return 0;
	if (!info->pointer_active || info->pointer_hidden)
		return 0;
	/*----------*/
	if (win->console != info->currcon)
		return 0;
	if (win->is_hidden)
		return 0;
	mx = info->curr_mouse_x;
	my = info->curr_mouse_y;
	mx1 = mx + PTRWID - 1;
	my1 = my + PTRHT - 1;
	x0 = win->x0;
	y0 = win->y0;
	x1 = win->x1;
	y1 = win->y1;
	if (!just_tip) {
		if ((mx >= x0 && mx <= x1) || (mx1 >= x0 && mx1 <= x1))
			if ((my >= y0 && my <= y1) || (my1 >= y0 && my1 <= y1))
				return 1;
	} else {
		if ((mx >= x0 && mx <= x1) && (my >= y0 && my <= y1))
				return 1;
	}

	return 0;
}


/* XX -- Currently the mouse pointer is a fixed pattern */
static unsigned long ptrpixels [] = {
#define T___ 0xff000000
#define BORD RGB_BLACK
#define X___ RGB_WHITE
BORD,BORD,T___,T___,T___,T___,T___,T___,T___,T___,
BORD,X___,BORD,T___,T___,T___,T___,T___,T___,T___,
BORD,X___,X___,BORD,T___,T___,T___,T___,T___,T___,
BORD,X___,X___,X___,BORD,T___,T___,T___,T___,T___,
BORD,X___,X___,X___,X___,BORD,T___,T___,T___,T___,
BORD,X___,X___,X___,X___,X___,BORD,T___,T___,T___,
BORD,X___,X___,X___,X___,X___,X___,BORD,T___,T___,
BORD,X___,X___,X___,X___,X___,X___,X___,BORD,T___,
BORD,X___,X___,X___,X___,X___,X___,X___,X___,BORD,
BORD,X___,X___,X___,X___,X___,BORD,BORD,BORD,BORD,
BORD,X___,X___,BORD,X___,X___,BORD,T___,T___,T___,
BORD,X___,BORD,T___,BORD,X___,X___,BORD,T___,T___,
BORD,BORD,T___,T___,BORD,X___,X___,BORD,T___,T___,
T___,T___,T___,T___,T___,BORD,X___,X___,BORD,T___,
T___,T___,T___,T___,T___,BORD,X___,X___,BORD,T___,
T___,T___,T___,T___,T___,T___,BORD,BORD,T___,T___
};


static void fbui_pointer_save (struct fb_info *info)
{
	short j,x,y;
	unsigned long *p = pointer_saveunder;

	if (!info)
		return;
	/*----------*/

	x = info->mouse_x0;
	y = info->mouse_y0;
	j = PTRHT;
	while (j--) {
		if (info->fbops->fb_getpixels_rgb) 
			info->fbops->fb_getpixels_rgb (info, x, y++, PTRWID, p, 1);
		p += PTRWID;
	}
}

static void fbui_pointer_restore (struct fb_info *info)
{
	short j,x,y;
	unsigned long *p = pointer_saveunder;

	if (!info)
		return;
	/*----------*/

	x = info->mouse_x0;
	y = info->mouse_y0;
	j = PTRHT;
	while (j--) {
		if (info->fbops->fb_putpixels_rgb) 
			info->fbops->fb_putpixels_rgb (info, x, y++, PTRWID, p, 1);
		p += PTRWID;
	}
}

static void fbui_pointer_draw (struct fb_info *info)
{
	short j, x, y;
	unsigned long *p = ptrpixels;

	if (!info)
		return;
	/*----------*/

	x = info->mouse_x0;
	y = info->mouse_y0;
	j = PTRHT;
	while (j--) {
		if (info->fbops->fb_putpixels_rgb) 
			info->fbops->fb_putpixels_rgb (info, x, y++, PTRWID, p, 1);
		p += PTRWID;
	}
}

static void fbui_enable_pointer (struct fb_info *info)
{
	if (!info) 
		return;
	if (info->pointer_active)
		return;
	/*----------*/

	fbui_pointer_save (info);
	fbui_pointer_draw (info);

	info->pointer_active = 1;
	info->pointer_hidden = 0;
}

#if 0
static void fbui_disable_pointer (struct fb_info *info)
{
	if (!info) 
		return;
	if (!info->pointer_active)
		return;
	/*----------*/

	// fbui_pointer_restore (info, win);
	info->pointer_active = 0;
	info->pointer_hidden = 0;
}
#endif

static void fbui_hide_pointer (struct fb_info *info)
{
	if (!info) 
		return;
	if (!info->pointer_active)
		return;
	if (info->pointer_hidden)
		return;
	/*----------*/

	if (!info->have_hardware_pointer) {
		fbui_pointer_restore (info);
	}
	info->pointer_hidden = 1;
}

static void fbui_unhide_pointer (struct fb_info *info)
{
	if (!info) 
		return;
	if (!info->pointer_active)
		return;
	if (!info->pointer_hidden)
		return;
	/*----------*/

	if (!info->have_hardware_pointer) {
		fbui_pointer_save (info);
		fbui_pointer_draw (info);
	}
	info->pointer_hidden = 0;
}


static struct fbui_window *get_pointer_window (struct fb_info *info)
{
	struct fbui_window *win=NULL;
	int i,lim;
	int cons;

	if (!info) 
		return NULL;
	/*----------*/

	cons = info->currcon;
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	while (i < lim) {
		win = info->windows [i];
		if (win && !win->is_wm && pointer_in_window (info,win,1))
			break;

		i++;
	}
	if (i >= lim)
		return NULL;
	return win;
}



static struct fbui_window *fbui_lookup_wm (struct fb_info *info, int cons)
{
	struct fbui_window *ptr;
        struct rw_semaphore *sem;

	if (!info)
		return NULL;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return NULL;
	/*----------*/

	sem = &info->winptrSem;
	down_read (sem);
	ptr = info->window_managers [cons];
	up_read (sem);

	/* Since the wm is a critical window, let's
	 * just make sure that its process still exists.
	 */
	if (ptr) {
		if (!process_exists (ptr->pid)) {
			fbui_remove_win (info, ptr->id, 0);
			ptr = NULL;
			down_write (sem);
			ptr = info->window_managers [cons];
			up_write (sem);
		}
	}

	return ptr;
}

void input_handler (u32 param, struct input_event *ev)
{
	struct fb_info *info;
	int type, code, value;
	short xlim, ylim;
	int cons;
	char event_is_altkey=0;

	if (!fbui_handler_regd)
		return;

	info = (struct fb_info*) param;
	if (!info || !ev)
		return;

	cons = info->currcon;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return;
	if (vt_cons[cons]->vc_mode != KD_GRAPHICS)
		return;
	/*----------*/

	type = ev->type;
	code = ev->code;
	value = ev->value;

	/* Intercept Alt-keys at all times.
	 */
	if (type == EV_KEY && (code == KEY_LEFTALT || code == KEY_RIGHTALT)) {
		altdown = value > 0;
		event_is_altkey=1;
	}

	switch (type) {
	case EV_SYN:
		return;

	case EV_KEY:
		if (altdown && !event_is_altkey) {
			if (value==1) {
				unsigned char ia = 0;
				struct fbui_window *match;

				switch (code) {
				case KEY_1: ia = '1'; break;
				case KEY_2: ia = '2'; break;
				case KEY_3: ia = '3'; break;
				case KEY_4:ia = '4'; break;
				case KEY_5: ia = '5'; break;
				case KEY_6: ia = '6'; break;
				case KEY_7: ia = '7'; break;
				case KEY_8:ia = '8'; break;
				case KEY_9: ia = '9'; break;
				case KEY_0: ia = '0'; break;
				case KEY_TAB:ia = '\t'; break;
				case KEY_ENTER:ia = '\n'; break;
				case KEY_SYSRQ:ia = FBUI_ACCEL_PRTSC; break;
				case KEY_HOME:ia = FBUI_ACCEL_HOME; break;
				case KEY_END:ia = FBUI_ACCEL_END; break;
				case KEY_PAGEUP:ia = FBUI_ACCEL_PGUP; break;
				case KEY_PAGEDOWN:ia = FBUI_ACCEL_PGDN; break;
				case KEY_A:  ia = 'a'; break;
				case KEY_B:  ia = 'b'; break;
				case KEY_C:  ia = 'c'; break;
				case KEY_D: ia = 'd'; break;
				case KEY_E:  ia = 'e'; break;
				case KEY_F:  ia = 'f'; break;
				case KEY_G: ia = 'g'; break; 
				case KEY_H: ia = 'h'; break;
				case KEY_I:  ia = 'i'; break;
				case KEY_J: ia = 'j'; break; 
				case KEY_K: ia = 'k'; break; 
				case KEY_L: ia = 'l'; break;
				case KEY_M: ia = 'm'; break; 
				case KEY_N:  ia = 'n'; break;
				case KEY_O: ia = 'o'; break; 
				case KEY_P: ia = 'p'; break;
				case KEY_Q:  ia = 'q'; break;
				case KEY_R:  ia = 'r'; break;
				case KEY_S: ia = 's'; break; 
				case KEY_T: ia = 't'; break;
				case KEY_U: ia = 'u'; break; 
				case KEY_V:  ia = 'v'; break;
				case KEY_W: ia = 'w'; break; 
				case KEY_X: ia = 'x'; break;
				case KEY_Y:  ia = 'y'; break;
				case KEY_Z: ia = 'z'; break;
				case KEY_BACKSPACE: ia = '\b'; break;
				}

				match = accelerator_test (info, info->currcon, ia);
				if (match) {
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_ACCEL;
					ev.key = ia;
					fbui_enqueue_event (info, match, &ev, 1);
				}

				intercepting_accel = 1;
				return;
			}
		}

		if (intercepting_accel && code!=KEY_LEFTALT && code!=KEY_RIGHTALT) {
			intercepting_accel = 0;
			return;
		}

		if (!intercepting_accel) {
			if (down_read_trylock (&info->winptrSem)) {
				if ((code & 0xfff0) == BTN_MOUSE) {
					struct fbui_window *win;

					win = get_pointer_window (info);
					up_read (&info->winptrSem);

					if (win) {
						struct fbui_event ev;
						short tmp=0;

						memset (&ev, 0, sizeof (struct fbui_event));
						ev.type = FBUI_EVENT_BUTTON;
						ev.key = value ? 1 : 0;

						switch (code) {
						case BTN_LEFT:
							tmp = FBUI_BUTTON_LEFT;
							break;
						case BTN_MIDDLE:
							tmp = FBUI_BUTTON_MIDDLE;
							break;
						case BTN_RIGHT:
							tmp = FBUI_BUTTON_RIGHT;
							break;
						}
						ev.key |= tmp;
						fbui_enqueue_event (info, win, &ev, 1);
					}
				}
				else
				{
					struct fbui_window *recipient=info->keyfocus_window [cons];
					up_read (&info->winptrSem);

					if (recipient) {
						struct fbui_event ev;
						memset (&ev, 0, sizeof (struct fbui_event));

						ev.type = FBUI_EVENT_KEY;
						ev.key = (code << 2) | (value & 3);
						fbui_enqueue_event (info, recipient, &ev, 1);
					} 
#if 0
					else
						printk (KERN_INFO "key %d,%d discarded\n",code,value);
#endif
				}
			}
			else /* XX need workaround */
				printk (KERN_INFO "key %d,%d INADVERTENTLY discarded\n",code,value);
		}
		break;

	case EV_REL:
		/* We don't receive both x and y at once;
		 * we must wait for both to arrive.
		 */
		xlim = info->var.xres;
		ylim = info->var.yres;
		if (!got_rel_x && !got_rel_y) {
			incoming_x = info->curr_mouse_x;
			incoming_y = info->curr_mouse_y;
		}
		if (code == REL_X) {
			short x = incoming_x;
			if (got_rel_x && !got_rel_y)
				got_rel_y = 1;
			xlim--;
			x += value;
			if (x<0)
				x = 0;
			if (x>xlim)
				x = xlim;
			incoming_x = x;
			got_rel_x = 1;
		} else
		if (code == REL_Y) {
			short y = incoming_y;
			if (got_rel_y && !got_rel_x)
				got_rel_x = 1;
			ylim--;
			y += value;
			if (y<0)
				y = 0;
			if (y>ylim)
				y = ylim;
			incoming_y = y;
			got_rel_y = 1;
		}

		if (got_rel_x && got_rel_y) {
			int cons = info->currcon;

			if (!info->pointer_active || info->pointer_hidden)
				return;

			/* Even if the new coords cannot affect the
			 * display due to drawing that is happening, 
			 * we at least need to record them.
			 */
			info->curr_mouse_x = incoming_x;
			info->curr_mouse_y = incoming_y;

			if (down_read_trylock (&info->winptrSem)) {
				struct fbui_window *win = NULL;
				struct fbui_window *wm = NULL;
				struct fbui_window *pf = NULL;
				struct fbui_window *oldwin = 
					info->pointer_window [cons];
				char drawing=0;
				struct fbui_processentry *pre = NULL;
				struct fbui_processentry *oldpre = NULL;
				
				win = get_pointer_window (info);
				if (win) {
					drawing = win->drawing;
					pre = win->processentry;
				}

				if (oldwin)
					oldpre = oldwin->processentry;

				/* generate Leave */
				if (oldwin && oldpre && win != oldwin &&
				   (oldpre->wait_event_mask & FBUI_EVENTMASK_LEAVE)) {
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_LEAVE;
					fbui_enqueue_event (info, oldwin, &ev, 1);

					oldwin->pointer_inside = 0;
					info->pointer_window [cons] = NULL;
				}

				/* generate Enter */
				if (win && pre && !win->pointer_inside &&
				   (pre->wait_event_mask & FBUI_EVENTMASK_ENTER)) {
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_ENTER;
					fbui_enqueue_event (info, win, &ev, 1);

					win->pointer_inside = 1;
					info->pointer_window [cons] = win;
				}

				/* If possible draw the pointer */
				if (!win || !drawing) {
					fbui_pointer_restore (info);
					info->mouse_x0 = incoming_x;
					info->mouse_y0 = incoming_y;
					info->mouse_x1 = info->mouse_x0 + PTRWID - 1;
					info->mouse_y1 = info->mouse_y0 + PTRHT - 1;
					fbui_pointer_save (info);
					fbui_pointer_draw (info);
				}

				/* generate Motion for appropriate window */
				if (win && pre &&
				   (pre->wait_event_mask & FBUI_EVENTMASK_MOTION))
				{
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_MOTION;
					ev.x = info->mouse_x0 - win->x0;
					ev.y = info->mouse_y0 - win->y0;
					fbui_enqueue_event (info, win, &ev, 1);
				}

				/* generate Motion for the window that has pointer focus*/
				pf = info->pointerfocus_window[cons];
				if (pf && pf->processentry) {
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_MOTION;
					ev.x = info->mouse_x0 - pf->x0;
					ev.y = info->mouse_y0 - pf->y0;
					fbui_enqueue_event (info, pf, &ev, 1);
				}

				wm = info->window_managers [cons];
				if (wm && wm->receive_all_motion) {
					struct fbui_event ev;
					memset (&ev, 0, sizeof (struct fbui_event));
					ev.type = FBUI_EVENT_MOTION;
					ev.x = info->mouse_x0;
					ev.y = info->mouse_y0;
					fbui_enqueue_event (info, wm, &ev, 1);
				}
				up_read (&info->winptrSem);
			} 

			got_rel_x = 0;
			got_rel_y = 0;
		}
		break;
	}
}


int fbui_init (struct fb_info *info)
{
	int i;

	if (!info) 
		return 0;
	/*----------*/

#ifdef FBUI_DEBUG
	init_MUTEX (&logsem);
#endif

	for (i=0; i < FBUI_TOTALACCELS * FBUI_MAXCONSOLES; i++)
		info->accelerators [i] = NULL;

	for (i=0; i < (FBUI_MAXCONSOLES * FBUI_MAXWINDOWSPERVC); i++) {
		info->windows [i] = NULL;
		init_MUTEX (&info->windowSems [i]);
	}

	init_MUTEX (&info->preSem);

	for (i=0; i<FBUI_MAXCONSOLES; i++) {
		info->force_placement [i] = 0;
		info->bgcolor[i] = 0;
		info->pointer_window [i] = NULL;
		info->keyfocus_window [i] = NULL;
		info->pointerfocus_window [i] = NULL;
		info->window_managers [i] = NULL;
	}

	init_rwsem (&info->winptrSem);
	info->mouse_x0 = info->var.xres >> 1;
	info->mouse_y0 = info->var.yres >> 1;
	info->curr_mouse_x = info->mouse_x0;
	info->curr_mouse_y = info->mouse_y0;
	info->mouse_x1 = info->mouse_x0 + PTRWID - 1;
	info->mouse_y1 = info->mouse_y0 + PTRHT - 1;
	info->pointer_active = 0;

	info->have_hardware_pointer = 0;

	init_rwsem (&info->cutpaste_sem);
	info->cutpaste_buffer = NULL;

	info->redsize = info->var.red.length;
	info->greensize = info->var.green.length;
	info->bluesize = info->var.blue.length;
	info->redshift = info->var.red.offset;
	info->greenshift = info->var.green.offset;
	info->blueshift = info->var.blue.offset;

	info->mode24 = (info->redsize == 8 && info->greensize == 8 &&
	    info->bluesize == 8 && info->redshift == 16 &&
	    info->greenshift == 8 && !info->blueshift);

	intercepting_accel = 0;
	altdown = 0;

	printk(KERN_INFO "FrameBuffer UI (%s) by Zack T Smith: operational on fb%d\n", FBUI_VERSION,info->node);

/*
	printk(KERN_INFO "sizeof (struct fbui_window)=%d\n", sizeof (struct fbui_window));
	printk(KERN_INFO "sizeof (struct fbui_processentry)=%d\n",sizeof(struct fbui_processentry));
	printk(KERN_INFO "sizeof (struct fbui_font)=%d\n", sizeof (struct fbui_font));
*/

	return 1;
}


static inline struct fbui_window *fbui_lookup_win (struct fb_info *info, int win_id)
{
	struct fbui_window *win;

	if (!info) 
		return NULL;
	if (win_id < 0 || win_id >= (FBUI_MAXWINDOWSPERVC * FBUI_MAXCONSOLES))
		return NULL;
	/*----------*/

	down_read (&info->winptrSem);
	win = info->windows [win_id];
	up_read (&info->winptrSem);

	return win;
}




static int fbui_overlap_check (struct fb_info *info, 
                               short x0, short y0, short x1, short y1,
                               int cons, struct fbui_window *self)
{
	struct fbui_window *ptr;
	struct rw_semaphore *sem;
	int i,lim;

	if (!info) 
		return -1;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return -1;
	/*----------*/

	sem = &info->winptrSem;
	down_read (sem);
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	while (i < lim) {
		ptr = info->windows [i];
	
		if (ptr && !ptr->is_hidden && ptr->console == cons && !ptr->is_wm
		    && ptr != self)
		{
			short x2, y2, x3, y3;

			x2 = ptr->x0;
			x3 = ptr->x1;
			y2 = ptr->y0;
			y3 = ptr->y1;

			if ((x2 >= x0 && x2 <= x1) ||
			    (x3 >= x0 && x3 <= x1)) {
				if ((y2 >= y0 && y2 <= y1) ||
				    (y3 >= y0 && y3 <= y1)) {
					up_read (sem);
					return 1;
				}
			}
		}
		i++;
	}
	up_read (sem);

	return 0;
}


static void restore_vc (struct fb_info *info, int cons)
{
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return;
	/*----------*/

	if (cons != info->currcon) {
		vc_cons[cons].d->vc_tty = info->ttysave[cons];
		vt_cons[cons]->vc_mode = KD_TEXT;
	} else {
		acquire_console_sem();
		vc_cons[cons].d->vc_tty = info->ttysave[cons];
		vt_cons[cons]->vc_mode = KD_TEXT;
		do_unblank_screen(1);
		redraw_screen (cons,0);
		release_console_sem();
	}
}




static void fbui_winptrs_change (struct fb_info *info, int cons)
{
	struct fbui_window *ptr;

	if (!info) 
		return;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return;
	/*----------*/

	ptr = fbui_lookup_wm (info, cons);
	if (ptr) {
		struct fbui_event ev;
		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_WINCHANGE;
		fbui_enqueue_event (info, ptr, &ev, 0);
	}
}


static int get_total_windows (struct fb_info *info, int cons)
{
	int i,lim,total;

	if (!info)
		return 0;

	total=0;
	i = FBUI_MAXWINDOWSPERVC * cons;
	lim = i + FBUI_MAXWINDOWSPERVC;
	down_read (&info->winptrSem);
	for ( ; i < lim; i++) {
		if (NULL != info->windows [i])
			total++;
	}
	up_read (&info->winptrSem);
	return total;
}

static int fbui_remove_win (struct fb_info *info, short win_id, int force)
{
	struct fbui_window *win;
	struct fbui_processentry *pre;
	struct rw_semaphore *sem;
	int cons = 0;
	int i, lim;

	if (!info)
		return FBUI_ERR_NULLPTR;
	if (win_id < 0 || win_id >= (FBUI_MAXWINDOWSPERVC * FBUI_MAXCONSOLES))
		return FBUI_ERR_BADWIN;
	/*----------*/

	sem = &info->winptrSem;
	down_read (sem);
	win = info->windows [win_id];
	up_read (sem);

	if (!win)
		return FBUI_ERR_BADWIN;

	/* Verify that we're allowed to remove this window */
	if (!force && current->pid != win->pid) {
		struct fbui_window *wm = fbui_lookup_wm (info, win->console);
		if (!wm) 
			return FBUI_ERR_BADWIN;

		if (current->pid != wm->pid)
			return FBUI_ERR_BADWIN;
	}

	/* XX */
	if (win->drawing)
		printk(KERN_INFO "DRAWING DURING REMOVEWIN\n");

	down_write (sem);
	cons = win->console;
	if (win == info->windows[win_id]) 
		info->windows [win_id] = NULL;
	else {
		printk (KERN_INFO "fbui_remove_win: window[win->id] != win\n");
	}
	if (win->is_wm)
		info->window_managers [cons] = NULL;

	if (info->pointerfocus_window[cons] == win)
		info->pointerfocus_window[cons] = NULL;

	/* XX need to search window list for another window that wants
	 * focus
	 */
	if (info->keyfocus_window[cons] == win)
		info->keyfocus_window[cons] = NULL;

	if (info->pointer_window [cons] == win)
		info->pointer_window[cons] = NULL;

	/* Clear any accelerators tied to this window */
	i = FBUI_TOTALACCELS * cons;
	lim = i + FBUI_TOTALACCELS;
	for ( ; i < lim; i++) {
		if (info->accelerators[i] == win)
			info->accelerators[i] = NULL;
	}
	up_write (sem);

	/* Reduce the window count for this pid */
	pre = win->processentry;
	if (!pre)
		printk (KERN_INFO "fbui_remove_win: window is missing processentry\n");
	else {
		if (pre->nwindows <= 0)
			printk (KERN_INFO "fbui_remove_win: processentry has invalid #windows=%d\n",
				pre->nwindows);
		else
			pre->nwindows--;

		if (pre->nwindows <= 0)
			free_processentry (info,pre);
		else
			printk(KERN_INFO "fbui_remove_win: processentry %d has #wins=%d\n",pre->index,
				pre->nwindows);
	}

	/* Clear window to display's bgcolor */
	win->bgcolor = info->bgcolor[cons];
	if (!win->is_wm)
		fbui_clear (info, win);
	kfree(win);

	/* console empty? if so, restore textmode
	 */
	if (!get_total_windows (info, cons)) {
		printk (KERN_INFO "fbui: restoring to text mode on VC %d\n", cons);
		restore_vc (info,cons);
	} else
	if (!force) {
		fbui_winptrs_change (info,cons);
	}

	return FBUI_SUCCESS;
}

static inline struct fbui_window *
		fbui_add_win (struct fb_info *info, 
                              int cons, 
                              char autoplacement,
                              char hidden)
{
	struct fbui_window *nu = NULL;
	struct fbui_processentry *pre = NULL;
	struct semaphore *sem = NULL;
	int pid = current->pid;
	short win_id;
	short i, lim;

	if (!info)
		return NULL;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return NULL;
	/*----------*/

	/* Find an empty window array entry
	 */
	down_write (&info->winptrSem);
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	win_id = -1;
	while (i < lim) {
		if (!info->windows [i]) {
			nu = kmalloc (sizeof(struct fbui_window), GFP_KERNEL);
			if (!nu) {
				/* XX kprint ... */
				up_write (&info->winptrSem);
				return NULL;
			}
			win_id = i;
			info->windows [i] = nu;
			break;
		}
		i++;
	}
	up_write (&info->winptrSem);

	if (i >= lim)
		return NULL;

	memset ((void*) nu, 0, sizeof(struct fbui_window));

	nu->id = win_id;

	nu->pid = pid;
	nu->console = cons;

	nu->mouse_x = -1;
	nu->mouse_y = -1;

	nu->font_valid = 0;

	/* Lookup the appropriate processentry, or allocate one
	 * if necessary.
	 */
	sem = &info->preSem;
	down (sem);
	pre = NULL;
	i=0;
	while (i < FBUI_MAXCONSOLES * FBUI_MAXWINDOWSPERVC) {
		pre = &info->processentries[i];
		if (pre->in_use && pre->pid == pid) {
			nu->processentry = pre;

			/* Rule: each process' windows are limited to one VC
			 */
			if (pre->console != cons)
				nu->console = cons = pre->console;
			break;
		}
		pre = NULL;
		i++;
	}
	up (sem);

	if (!pre) {
		pre = alloc_processentry (info, pid, cons);
		if (pre)
			nu->processentry = pre;
	}

	if (!pre) {
		printk (KERN_INFO "fbui_add_win: cannot even allocate a processentry\n");
		info->windows [nu->id] = NULL;
		kfree (nu);
		return NULL;
	}
	else
		++pre->nwindows;

	nu->is_hidden = hidden;
	nu->need_placement = autoplacement;

	if (!autoplacement && !hidden) {
		struct fbui_event ev;
		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_EXPOSE;
		fbui_enqueue_event (info, nu, &ev, 0);
	}

	fbui_winptrs_change (info,cons);

	return nu;
}


static inline void
backup_vc (struct fb_info *info, int cons)
{
	struct tty_struct *tty;

	if (!info || cons < 0 || cons >= FBUI_MAXCONSOLES)
		return;
	/*----------*/

        acquire_console_sem();
	vt_cons[cons]->vc_mode = KD_GRAPHICS;
	tty = vc_cons[cons].d->vc_tty;
	info->ttysave[cons] = tty;
	vc_cons[cons].d->vc_tty = NULL;
        info->cursor.enable = 0;
	do_blank_screen(1);
        release_console_sem();

	if (cons == info->currcon) {
		fb_clear (info, info->bgcolor[cons]);
		info->pointer_active = 0;
		fbui_enable_pointer(info);
	}
}


static int fbui_set_geometry (struct fb_info *info, struct fbui_window *win,
                              short x0, short y0, short x1, short y1);

int fbui_open (struct fb_info *info, struct fbui_openparams *p)
{
	struct fbui_window *win;
	struct fbui_window *wm;
	short cons;
	char auto_placed=0;
	char initially_hidden;
	short tmp1, tmp2, max;

	/* Extra bulletproofing! */
	if (!info || !p)
		return FBUI_ERR_NULLPTR;
	if (p->need_keys & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->need_motion & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->desired_vc < -1 || p->desired_vc >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADPARAM;
	if (p->req_control & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->receive_all_motion & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->doing_autoposition & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->initially_hidden & 0xfe)
		return FBUI_ERR_BADPARAM;
	if (p->program_type < 0 || p->program_type >= FBUI_PROGTYPE_LAST)
		return FBUI_ERR_BADPARAM;
	tmp1 = p->x0;
	tmp2 = p->x1;
	max = info->var.xres;
	if (tmp1 < 0)
		tmp1 = 0;
	if (tmp1 >= max)
		tmp1 = max-1;
	if (tmp2 < 0)
		tmp2 = 0;
	if (tmp2 >= max)
		tmp2 = max-1;
	if (tmp1 > tmp2) {
		p->x0 = tmp2;
		p->x1 = tmp1;
	}
	tmp1 = p->y0;
	tmp2 = p->y1;
	max = info->var.yres;
	if (tmp1 < 0)
		tmp1 = 0;
	if (tmp1 >= max)
		tmp1 = max-1;
	if (tmp2 < 0)
		tmp2 = 0;
	if (tmp2 >= max)
		tmp2 = max-1;
	if (tmp1 > tmp2) {
		p->y0 = tmp2;
		p->y1 = tmp1;
	}
	/*----------*/

	if (!fbui_handler_regd) {
		fbui_input_register_handler (&input_handler, (u32) info);
		fbui_handler_regd = 1;
	}

	initially_hidden = p->initially_hidden;

	cons = p->desired_vc;
	if (cons >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADVC;
	if (!vc_cons_allocated (cons)) {
		if (!vc_allocate (cons))
			return FBUI_ERR_BADVC;
	}
	if (cons < 0)
		cons = info->currcon;

	wm = fbui_lookup_wm (info, cons);

	if (p->req_control) {
		if (wm)
			return FBUI_ERR_HAVEWM;

		/* the window-manager process gets the full screen area */
		p->x0 = 0;
		p->y0 = 0;
		p->x1 = info->var.xres - 1;
		p->y1 = info->var.yres - 1;
		info->bgcolor[cons] = p->bgcolor;
	} else {
		if (p->program_type)
			auto_placed = 1;
		if (info->force_placement[cons])
			auto_placed = 1;
		if (!wm)
			auto_placed = 0;
		if (wm && !wm->doing_autopos)
			auto_placed = 0;

		if (!auto_placed && !initially_hidden) {
			if (fbui_overlap_check (info, p->x0, p->y0, p->x1, p->y1, 
						cons, NULL))
				return FBUI_ERR_OVERLAP;
		}
	} 

	win = fbui_add_win (info, cons, auto_placed, 
			    initially_hidden | auto_placed);
	if (!win)
		return FBUI_ERR_NOMEM;

/* printk(KERN_INFO "fbui_open: window %s/%d successfully added, location %d %d, %dx%d\n", win->name,win->id,win->x0,win->y0,win->width,win->height); */
	win->program_type = p->program_type;
	win->need_keys = p->need_keys;
	win->need_motion = p->need_motion;
	win->receive_all_motion = p->receive_all_motion;
	win->need_placement = auto_placed;

	strncpy (win->name, p->name, FBUI_NAMELEN);
	strncpy (win->subtitle, p->subtitle, FBUI_NAMELEN);

	win->max_width = p->max_width;
	win->max_height = p->max_height;

	if (p->req_control) {
		win->is_wm = 1;
		if (p->doing_autoposition)
			win->doing_autopos = 1;

		down_write (&info->winptrSem);
		info->window_managers [cons] = win;
		up_write (&info->winptrSem);
	}

	if (vt_cons[cons]->vc_mode != KD_GRAPHICS) {
		backup_vc (info, cons);
		intercepting_accel = 0;
	}

	if (!auto_placed && !initially_hidden && !p->req_control)
		fbui_set_geometry (info, win, p->x0,p->y0,p->x1,p->y1);
	else {
		/* Copy the numbers over verbatim since the wm may find them useful*/
		win->x0 = p->x0;
		win->y0 = p->y0;
		win->x1 = p->x1;
		win->y1 = p->y1;
		win->width = p->x1 - p->x0 + 1;
		win->height = p->y1 - p->y0 + 1;
	}

	win->bgcolor = p->bgcolor;

	if (!win->is_wm && !auto_placed && !initially_hidden)
		fbui_clear (info, win);

	down_write (&info->winptrSem);
{struct fbui_window *p = info->keyfocus_window[cons];
printk(KERN_INFO "fbui_open: cons=%d needkeys=%d keyfocus=%08lx %s %d\n", cons, win->need_keys, (unsigned long)p, p?p->name:"(none)", p?p->id:-1);
}
	if (!info->keyfocus_window[cons] && win->need_keys)
		info->keyfocus_window[cons] = win;
	if (!info->pointerfocus_window[cons] && win->need_motion)
		info->pointerfocus_window[cons] = win;
	up_write (&info->winptrSem);

	return win->id;
}


int fbui_close (struct fb_info *info, short id)
{
	struct fbui_window *win;

	if (!info)
		return FBUI_ERR_NULLPTR;
	if (id < 0 || id >= (FBUI_MAXWINDOWSPERVC * FBUI_MAXCONSOLES))
		return FBUI_ERR_BADWIN;
	/*----------*/

	win = fbui_lookup_win (info, id);
	if (!win)
		return FBUI_ERR_NOTOPEN;

	return fbui_remove_win (info, id, 0);
}


/* "redraw" forced by window-manager process
 */
int fbui_redraw (struct fb_info *info, struct fbui_window *self, 
		 struct fbui_window *win)
{
	struct fbui_event ev;

	if (!info || !self || !win) 
		return 0;
	if (!self->is_wm)
		return FBUI_ERR_NOTWM;
	/*----------*/

	fbui_clear (info, win);

	memset (&ev, 0, sizeof (struct fbui_event));
	ev.type = FBUI_EVENT_EXPOSE;
	fbui_enqueue_event (info, win, &ev, 0);

	return FBUI_SUCCESS;
}


/* Verify that the process entry array doesn't contain killed processes,
 *   remove any such entries.
 * Verify that the windows on the system are for processes that exist.
 *   remove any such windows.
 */
static int fbui_clean (struct fb_info *info, int cons)
{
	struct semaphore *sem;
        struct rw_semaphore *rwsem;
	int i, lim;

	if (!fbui_handler_regd)
		return FBUI_SUCCESS;
	if (!info)
		return FBUI_ERR_NULLPTR;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADPARAM;
	/*----------*/

	/* Process entry check */
	sem = &info->preSem;
	down(sem);
	i=0;
	while (i < FBUI_MAXCONSOLES * FBUI_MAXWINDOWSPERVC) {
		struct fbui_processentry *pre = &info->processentries[i];

		if (pre->in_use) {
			if (!process_exists (pre->pid)) {
				printk (KERN_INFO "fbui_clean: removing zombie process entry %d\n", i);
				pre->in_use = 0;
				pre->waiting = 0;
				pre->pid = 0;
				pre->nwindows = 0;
			}
		}
		i++;
	}
	up(sem);

	/* Window struct check */
	rwsem = &info->winptrSem;
	i = cons * FBUI_MAXWINDOWSPERVC;
	lim = i + FBUI_MAXWINDOWSPERVC;
	while (i < lim) {
		struct fbui_window *win;

		down_read (rwsem);
		win = info->windows [i];
		up_read (rwsem);

		if (win) {
			if (!process_exists (win->pid)) {
				short id = win->id;

				if (id == i) {
					printk (KERN_INFO "fbui_clean: removing zombie window %d\n", i);
					fbui_remove_win (info, i, 1);
				} else
					printk (KERN_INFO "fbui_clean: invalid window entry for window %d\n", id);
			}
		}
		i++;
	}

	return FBUI_SUCCESS;
}


/* Return the process ids of all windows for this console
 * npids initially tells the maximum we can return.
 */
int fbui_window_info (struct fb_info *info, int cons, 
	/*user*/ struct fbui_wininfo *ary, int ninfo)
{
	struct fbui_window *win;
	int i, j, lim;

	if (!info || !ary || ninfo <= 0) 
		return FBUI_ERR_NULLPTR;
	if (!access_ok (VERIFY_WRITE, (void*)ary, 
	    ninfo * sizeof(struct fbui_wininfo)))
		return FBUI_ERR_BADADDR;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADPARAM;
	/*----------*/

	/* Clean up the console */
	fbui_clean (info, cons);

/*printk(KERN_INFO "fbui_window_info: cons=%d\n", cons);*/
	i = 0;
	j = cons * FBUI_MAXWINDOWSPERVC;
	lim = j + FBUI_MAXWINDOWSPERVC;
	down_read (&info->winptrSem);
	while (i < ninfo && j < lim) {
		win = info->windows [j];
		if (win && !win->is_wm) {
			struct fbui_wininfo wi;
			char *ptr;
#if 0
printk(KERN_INFO "            : window %d/%s ", win->id,win->name);
printk(KERN_INFO "            : %s %s type=%d\n", win->is_hidden?"hidden":"",win->need_keys?"need_keys","",win->program_type);
#endif
			wi.id = win->id;
			wi.pid = win->pid;
			wi.program_type = win->program_type;
			wi.hidden = win->is_hidden;
			wi.need_placement = win->need_placement;
			wi.need_keys = win->need_keys;
			wi.need_motion = win->need_motion;
			wi.x = win->x0;
			wi.y = win->y0;
			wi.width = win->width;
			wi.height = win->height;
			wi.max_width = win->max_width;
			wi.max_height = win->max_height;
/* printk(KERN_INFO "inside wininfo: x %d y %d w %d h %d\n",(int)wi.x,(int)wi.y,(int)wi.width, (int)wi.height); */
			strcpy (wi.name, win->name);
			strcpy (wi.subtitle, win->subtitle);

			ptr = (char*) &ary[i];

			if (copy_to_user (ptr, &wi, sizeof(struct fbui_wininfo))) {
				up_read (&info->winptrSem);
				return FBUI_ERR_BADADDR;
			}

			i++;
		}
		j++;
	}
	up_read (&info->winptrSem);

	return i;
}


/* Delete the window belonging to a given process,
 * as identified by its process id.
 */
int fbui_delete (struct fb_info *info, struct fbui_window *self, 
		 struct fbui_window *win)
{
	int rv;
	short id;
	struct semaphore *sem;

	if (!info || !self || !win) 
		return 0;
	if (!self->is_wm)
		return FBUI_ERR_NOTWM;
	/*----------*/

	id = win->id;
	sem = &info->windowSems [id];
	down (sem);
	rv = fbui_remove_win (info, win->id, 0);
	up (sem);
	return FBUI_SUCCESS;
}



static struct fbui_window *
   accelerator_test (struct fb_info *info, int cons, unsigned char ch)
{
	if (!info)
		return NULL;
	if (ch >= FBUI_TOTALACCELS)
		return NULL;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return NULL;
	/*----------*/

	return info->accelerators [ch + FBUI_TOTALACCELS*cons];
}


/* op==1 to register an accelerator (Alt-key sequence), 0 to unregister
 */
static int fbui_accel (struct fb_info *info, struct fbui_window *win,
	short which, short op)
{
	int ix;

	if (!info || !win)
		return FBUI_ERR_NULLPTR;
	if (which < 0 || which > 127 || op < 0 || op > 1)
		return FBUI_ERR_BADPARAM;
	/*----------*/

	ix = which + FBUI_TOTALACCELS * win->console;

	if (op==1) {
		if (accelerator_test (info, win->console, which))
			return FBUI_ERR_ACCELBUSY;

		info->accelerators[ix] = win;
	} else
		info->accelerators[ix] = NULL;

	return FBUI_SUCCESS;
}


/* Does not generate WinChange event */
int fbui_move_resize (struct fb_info *info, struct fbui_window *self,
	struct fbui_window *win,  
	short x, short y, short width, short height)
{
	char result=FBUI_SUCCESS;
	short x1, y1;
	struct semaphore *sem;

	if (!info || !self || !win)
		return 0;
	if (!self->is_wm)
		return FBUI_ERR_NOTWM;
	/*----------*/

	x1 = x+width-1;
	y1 = y+height-1;

	sem = &info->windowSems [win->id];
	down (sem);

	/* If the window is not hidden, verify no overlap.
 	 * If hidden, we don't care.
	 */
	if (!win->is_hidden)
		result = fbui_overlap_check (info, x, y, x1, y1, self->console, win);

	if (!result) {
		result = fbui_set_geometry (info, win, x, y, x1, y1);

		if (!result) {
			struct fbui_event ev;

			fbui_clear (info, win);

			memset (&ev, 0, sizeof (struct fbui_event));
			ev.type = FBUI_EVENT_MOVE_RESIZE;
			ev.x = x;
			ev.y = y;
			ev.width = x1-x+1;
			ev.height = y1-y+1;
			fbui_enqueue_event (info, win, &ev, 0);

			/*ev.type = FBUI_EVENT_EXPOSE;
			fbui_enqueue_event (info, win, &ev, 0);
			*/

			win->need_placement = 0;
		}
		else
			printk (KERN_INFO "set geometry failed -- %s\n",win->name);
	} else
		printk (KERN_INFO "overlap check failed -- %s\n",win->name);
	up (sem);
	return result;
}



/* Does not generate WinChange event */
int fbui_hide (struct fb_info *info, struct fbui_window *self, 
               struct fbui_window *win)
{
	struct semaphore *sem;
	int cons;

	if (!info || !self || !win) 
		return FBUI_ERR_NULLPTR;
	if (!self->is_wm)
		return FBUI_ERR_NOTWM;
	if (self == win)
		return FBUI_ERR_BADPARAM;
	cons = self->console;
	if (cons != win->console)
		return FBUI_ERR_BADPARAM;
	/*----------*/

	if (!win->is_hidden) {
		struct fbui_event ev;
		struct fbui_window *win2, *wm;

		wm = fbui_lookup_wm (info, cons);

/* printk(KERN_INFO "Entered fbui_hide: hiding unhidden window %s/%d\n", win->name,win->id);  */
		sem = &info->windowSems [win->id];
		down (sem);
		if (!wm) {
			/* Rule: if there is a window manager running, 
			 * let it decide whether to perform a window clear.
			 */
			u32 c = win->bgcolor;
			win->bgcolor = info->bgcolor [cons];
			fbui_clear (info, win); /* clear to display bgcolor */
			win->bgcolor = c;
		}
		win->is_hidden = 1;
		up (sem);

		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_HIDE;
		fbui_enqueue_event (info, win, &ev, 0);

		down_read (&info->winptrSem);
		win2 = info->pointer_window [info->currcon];
		up_read (&info->winptrSem);

		if (win2 == win) {
			memset (&ev, 0, sizeof (struct fbui_event));
			ev.type = FBUI_EVENT_LEAVE;
			fbui_enqueue_event (info, win, &ev, 0);
		}
	}

	return FBUI_SUCCESS;
}


/* Does not generate WinChange event */
int fbui_unhide (struct fb_info *info, struct fbui_window *self, 
                 struct fbui_window *win)
{
	struct semaphore *sem;
	struct fbui_event ev;
	struct fbui_window *win2;
	int cons;

	if (!info || !self || !win)
		return 0;
	if (!self->is_wm)
		return 0;
	if (self == win)
		return FBUI_ERR_BADPARAM;
	cons = self->console;
	if (cons != win->console)
		return FBUI_ERR_BADPARAM;
	if (!win->is_hidden)
		return FBUI_SUCCESS;
	/*----------*/

	/* Rule: Don't allow unhide if the window will overlap.
	 */
	if (fbui_overlap_check (info, win->x0, win->y0, win->x1, win->y1, 
				cons, win))
		return FBUI_ERR_OVERLAP;

	sem = &info->windowSems [win->id];
	down (sem);
	win->is_hidden = 0;
	fbui_clear (info, win);
	up (sem);

	memset (&ev, 0, sizeof (struct fbui_event));
	ev.type = FBUI_EVENT_UNHIDE;
	fbui_enqueue_event (info, win, &ev, 0);
	ev.type = FBUI_EVENT_EXPOSE;
	fbui_enqueue_event (info, win, &ev, 0);
	
	down_read (&info->winptrSem);
	win2 = get_pointer_window (info);
	up_read (&info->winptrSem);
	if (win2 == win) {
		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_ENTER;
		fbui_enqueue_event (info, win, &ev, 0);
	}

	return FBUI_SUCCESS;
}


static struct fbui_processentry *alloc_processentry (struct fb_info *info, 
						     int pid, int cons)
{
	int i;
	struct fbui_processentry *pre;
	struct semaphore *sem;

	if (!info)
		return NULL;
	/*----------*/

	sem = &info->preSem;
	down(sem);
	i=0;
	while (i < FBUI_MAXCONSOLES * FBUI_MAXWINDOWSPERVC) {
		pre = &info->processentries[i];
		if (!pre->in_use) {
			pre->index = i;
			pre->waiting = 0;
			pre->pid = pid;
			pre->console = cons;
			pre->nwindows = 0;
			pre->events_head = 0;
			pre->events_tail = 0;
			pre->events_pending = 0;
			init_waitqueue_head(&pre->waitqueue);
			pre->window_num = -1;
			pre->queuelock = SPIN_LOCK_UNLOCKED;
			init_MUTEX (&pre->queuesem);
			pre->in_use = 1;
			up (sem);
			return pre;
		}
		i++;
	}
	up (sem);
	return NULL;
}

static void free_processentry (struct fb_info *info, struct fbui_processentry *pre)
{
	struct semaphore *sem;
	if (!pre)
		return;
	/*----------*/

	sem = &info->preSem;
	down (sem);
	pre->in_use = 0;
	pre->waiting = 0;
	pre->pid = 0;
	pre->nwindows = 0;
	pre->events_head = 0;
	pre->events_tail = 0;
	pre->events_pending = 0;
	up (sem);
}




int fbui_control (struct fb_info *info, struct fbui_ctrlparams *ctl)
{
	struct fbui_window *self=NULL;
	struct fbui_window *win=NULL;
	struct fbui_processentry *pre=NULL;
	int cons = -1;
	char cmd;
	short id;
	short x, y;
	short width, height;
	struct fbui_event *event;
	unsigned char *pointer;
	u32 cutlen;

	if (!info || !ctl)
		return FBUI_ERR_NULLPTR;
	/*----------*/

	cmd = ctl->op;
	x = ctl->x;
	y = ctl->y;
	width = ctl->width;
	height = ctl->height;
	id = ctl->id;
	event = ctl->event;
	pointer = ctl->pointer;
	cutlen = ctl->cutpaste_length;

	if (id < 0)
		return FBUI_ERR_BADPARAM;

	if (!(self = fbui_lookup_win (info, id)))
		return FBUI_ERR_MISSINGWIN;

	pre = self->processentry;
	if (pre->pid != current->pid)
		return FBUI_ERR_BADPID;

	if (!pre)
		return FBUI_ERR_MISSINGPROCENT;

	cons = pre->console;
	if (cons < 0 || cons >= FBUI_MAXCONSOLES)
		return FBUI_ERR_BADVC;
	if (cons != id / FBUI_MAXWINDOWSPERVC)
		return FBUI_ERR_BADVC;

	/* Only poll and wait do not take a window param
	 */
	if (!self && (cmd != FBUI_POLLEVENT && cmd != FBUI_WAITEVENT))
		return FBUI_ERR_INVALIDCMD;

	if (cmd >= FBUI_CTL_TAKESWIN) {
		if (!self->is_wm)
			return FBUI_ERR_NOTWM;
		if (!(win = fbui_lookup_win (info, ctl->id2)))
			return FBUI_ERR_BADPID;
		if (win->console != cons)
			return FBUI_ERR_WRONGWM;
	} 

	switch (cmd) {
	case FBUI_REDRAW:
		return fbui_redraw (info, self, win);

	case FBUI_DELETE:
		return fbui_delete (info, self, win);

	case FBUI_MOVE_RESIZE:
		return fbui_move_resize (info, self, win, x, y, width, height);

	case FBUI_HIDE:
		return fbui_hide (info, self, win);

	case FBUI_UNHIDE:
		return fbui_unhide (info, self, win);

	case FBUI_WININFO:
		return fbui_window_info (info, cons, ctl->info, ctl->ninfo);

	case FBUI_ACCEL:
		return fbui_accel (info, self, ctl->x, ctl->y);

	case FBUI_GETDIMS: {
		long value;
		if (self->need_placement)
			value = FBUI_ERR_NOTPLACED;
		else {
			value = self->width;
			value <<= 16;
			value |= self->height;
		}
		return value;
	}

	case FBUI_READMOUSE: {
		long value;
		value = self->mouse_x;
		value <<= 16;
		value |= self->mouse_y;
		return value;
	}

	case FBUI_GETPOSN: {
		u32 value;
		if (self->need_placement) {
			value = FBUI_ERR_NOTPLACED;
		} else {
			value = self->x0;
			value <<= 16;
			value |= self->y0;
		}
		return value;
	}

	case FBUI_ASSIGN_KEYFOCUS:
		if (!win->is_hidden && win->need_keys && !win->is_wm) {
			down_write (&info->winptrSem);
			info->keyfocus_window [cons] = win;
			up_write (&info->winptrSem);
		}
		return FBUI_SUCCESS;

	case FBUI_ASSIGN_PTRFOCUS:
		if (!win->is_hidden && win->need_motion && !win->is_wm) {
			down_write (&info->winptrSem);
			info->pointerfocus_window [cons] = win;
			up_write (&info->winptrSem);
		}
		return FBUI_SUCCESS;

	case FBUI_SUBTITLE:
		strncpy (self->subtitle, ctl->string, FBUI_NAMELEN);
		fbui_winptrs_change (info, self->console);
		return FBUI_SUCCESS;

	case FBUI_SETFONT:
		if (!access_ok (VERIFY_READ, pointer, FBUI_FONTSIZE))
			return FBUI_ERR_BADADDR;
		
		if (copy_from_user ((char*) &self->font, pointer, FBUI_FONTSIZE))
			return FBUI_ERR_BADADDR;

		self->font_valid = 1;
		return FBUI_SUCCESS;

	case FBUI_POLLEVENT:
	case FBUI_WAITEVENT: {
		struct fbui_event ev;

		if (!event)
			return FBUI_ERR_NULLPTR;

		if (!access_ok (VERIFY_WRITE, (void*)event, sizeof(struct fbui_event)))
			return -EFAULT;

		/* Rule: all windows for a process use the same event mask */
		pre->wait_event_mask = x;

		memset (&ev, 0, sizeof (struct fbui_event));
		ev.type = FBUI_EVENT_NONE;

		if (fbui_dequeue_event (info, pre, &ev)) {
			if (copy_to_user (event, &ev, sizeof(struct fbui_event)))
				return -EFAULT;
			else
				return FBUI_SUCCESS;
		}

		/* No event & polling means exit here */
		if (cmd == FBUI_POLLEVENT) {
			pre->waiting = 0;
			return FBUI_ERR_NOEVENT;
		}

		pre->waiting = 1;
		wait_event_interruptible (pre->waitqueue, 
					  pre->events_pending > 0);

		if (fbui_dequeue_event (info, pre, &ev)) {
			if (copy_to_user (event, &ev, sizeof(struct fbui_event)))
				return -EFAULT;
			else
				return FBUI_SUCCESS;
		} else {
printk (KERN_INFO "XXXXXXXXXX process (pid %d) woken up but no event pending! (pre->nwindows=%d)\n",pre->pid,pre->nwindows);
			return FBUI_ERR_NOEVENT;
		}
	}

	case FBUI_READPOINT:
		if (!self->is_wm)
			return RGB_NOCOLOR;
		else
			return info->fbops->fb_read_point ? 
				info->fbops->fb_read_point (info, x, y) :
				RGB_NOCOLOR;

	case FBUI_PLACEMENT:
		if (self->is_wm) {
			info->force_placement[cons] = x?1:0;
			return FBUI_SUCCESS;
		}
		else
			return FBUI_ERR_NOTWM;

	case FBUI_CUT: { /* write to kernel buf */
		unsigned char *ptr;
		char err=0;
		if (!pointer)
			return FBUI_ERR_NULLPTR;

		if (cutlen <= 0)
			return FBUI_ERR_BADPARAM;

		if (!access_ok (VERIFY_READ, pointer, cutlen))
			return FBUI_ERR_BADADDR;

		down_write (&info->cutpaste_sem);
		ptr = kmalloc (cutlen, GFP_KERNEL);
		if (!ptr)
			err = 1;
		else {
			if (copy_from_user (ptr, pointer, cutlen)) {
				err = 1;
				kfree (ptr);
			} else {
				/* success */
				if (info->cutpaste_buffer)
					kfree (info->cutpaste_buffer);
				info->cutpaste_buffer = ptr;
				info->cutpaste_length = cutlen;
			}
		}
		up_write (&info->cutpaste_sem);
		if (err)
			return FBUI_ERR_CUTFAIL;
		else
			return FBUI_SUCCESS;
	}

	case FBUI_CUTLENGTH:
		return info->cutpaste_length;

	case FBUI_PASTE: { /* read from kernel buf */
		unsigned char *ptr;
		char err = 0;

		if (!pointer)
			return FBUI_ERR_NULLPTR;

		if (cutlen <= 0)
			return FBUI_ERR_BADPARAM;

		if (!access_ok (VERIFY_WRITE, pointer, cutlen))
			return FBUI_ERR_BADADDR;

		if (info->cutpaste_buffer)
			kfree (info->cutpaste_buffer);
		down_read (&info->cutpaste_sem);
		ptr = info->cutpaste_buffer;
		if (ptr) {
			u32 len = sizeof(struct fbui_wininfo);
			if (cutlen < len)
				len = cutlen;
			if (copy_to_user (pointer, ptr, len))
				err = 1;
		} else 
			err = 1;
		up_read (&info->cutpaste_sem);
		if (err)
			return FBUI_ERR_PASTEFAIL;

		/* return byte count */
		if (!ptr) 
			return 0;
		else
			return cutlen;
	 } /* paste */

	default:
		return FBUI_ERR_INVALIDCMD;
	}
}


static char cmdinfo[] = 
{
        0,      /* none */
192+    6,      /* copy area    x0,y0,x1,y1,w,h*/
32+128+ 4,      /* point        x,y,color lo,hi*/
32+192+ 6,      /* line         x0,y0,x1,y1,color lo,hi*/
32+128+ 5,      /* hline        x0,x1,color lo,hi,y*/
32+128+ 5,      /* vline        y0,y1,color lo,hi,x*/
32+192+ 6,      /* rect         x0,y0,x1,y1,color lo,hi*/
32+192+ 6,      /* fill_area    x0,y0,x1,y1,color lo,hi*/
        0,      /* clear */
192+    4,      /* invertline   x0,y0,x1,y1*/
32+128+ 9,      /* string       x,y,font lo,hi,len, string lo,hi, fgcolor lo,hi*/
32+128+ 5,      /* put pixels   x,y,ptr lo,hi,len*/
32+128+ 5,      /* put pixels RGB       x,y,ptr lo,hi,len */
32+128+ 5,      /* put pixels RGB 3-byte        x,y,ptr lo,hi,len */
192+    4,      /* clear area   x0,y0,x1,y1*/
32+128+ 9,      /* tinyblit	x,y,color lo,hi,bgcolor lo,hi,width, bitmap lo,hi */
};


/* This routine executes commands which can be 
 * safely ignored when a window is hidden, suspended, or
 * not in the foreground console.
 */
int fbui_exec (struct fb_info *info, short win_id, short n, unsigned char *arg)
{
	struct fbui_window *win=NULL;
	unsigned char *argmax = arg + n * 2;
	int result = FBUI_SUCCESS;
	char initial_hide=0;

	if (!info || !arg)
		return FBUI_ERR_NULLPTR;
	if (win_id < 0 || win_id >= (FBUI_MAXWINDOWSPERVC * FBUI_MAXCONSOLES))
		return FBUI_ERR_BADWIN;
	if (info->fix.visual != FB_VISUAL_TRUECOLOR && 
	    info->fix.visual != FB_VISUAL_DIRECTCOLOR) 
		return FBUI_ERR_WRONGVISUAL;
	if (n < 0)
		return FBUI_ERR_INVALIDCMD; /* XX */
	/*----------*/

	if (!(win = fbui_lookup_win (info, win_id)))
		return FBUI_ERR_BADWIN;
	if (win->pid != current->pid)
                return FBUI_ERR_BADWIN;
	if (win->is_hidden)
		return FBUI_SUCCESS;

	down (&info->windowSems [win->id]);
	win->drawing = 1;

	initial_hide = info->pointer_hidden;

	while (!result && arg < argmax && !win->is_hidden)
	{
		unsigned short ary [20];
		short len;
		short a=0, b=0, c=0, d=0, wid=0, ht=0;
		u32 ptr=0;
		unsigned short cmd;
		unsigned char flags;
		unsigned short ix;
		u32 param32=0;

		if (get_user (cmd, arg)) {
			result = FBUI_ERR_BADADDR;
			break;
		}
		arg += 2;

		if (cmd > sizeof (cmdinfo)) {
			result = FBUI_ERR_INVALIDCMD;
			break;
		}
		
		flags = cmdinfo[cmd];
		if ((len = 2 * (flags & 31))) {
			if (copy_from_user (ary, arg, len)) {
				result = FBUI_ERR_BADADDR;
				break;
			}
		}
		arg += len;

		ix = 0;
		if (flags & 128) {
			a = ary[ix++];
			b = ary[ix++];
		}
		if (flags & 64) {
			c = ary[ix++];
			d = ary[ix++];
		}
		if (flags & 32) {
			param32 = ary[ix+1];
			param32 <<= 16;
			param32 |= ary[ix];
			ix+=2;
		}

		switch(cmd) {
		case FBUI_CLEAR:
			result = fbui_clear (info,win);
			if (result)
				goto finished;
			break;

		case FBUI_POINT: 
			result = fbui_draw_point (info,win, a,b,param32);
			if (result)
				goto finished;
			break;

		case FBUI_INVERTLINE: 
			win->do_invert = 1;
			result = fbui_draw_line (info,win, a,b,c,d,0);
			win->do_invert = 0;
			if (result)
				goto finished;
			break;

		case FBUI_LINE: 
			result = fbui_draw_line (info,win, a,b,c,d,param32);
			if (result)
				goto finished;
			break;

		case FBUI_HLINE: 
			c = ary[ix++];
			result = fbui_draw_hline (info,win,a,b,c,param32);
			if (result)
				goto finished;
			break;

		case FBUI_TINYBLIT: {
			u32 bits;
			u32 bgcolor;
			short width;

			/* a = x
			 * b = y
			 * param32 = fgcolor
		 	 */
			bgcolor = ary[ix+1];
			bgcolor <<= 16;
			bgcolor |= ary[ix];
			ix += 2;

			width = ary[ix++];

			bits = ary[ix+1];
			bits <<= 16;
			bits |= ary[ix];
			ix += 2;

/* printk(KERN_INFO "FBUI_TINYBLIT: x=%d y=%d width=%d fgcolor=%08lx bgcolor=%08lx bits=%08lx\n", a,b,width,param32,bgcolor,bits); */

			result = fbui_tinyblit (info,win,a,b,width,param32,bgcolor,bits);
			if (result)
				goto finished;
			break;
		}

		case FBUI_VLINE: 
			c = ary[ix++];
			result = fbui_draw_vline (info,win,c,a,b,param32);
			if (result)
				goto finished;
			break;

		case FBUI_RECT: 
			result = fbui_draw_rect (info,win,a,b,c,d,param32);
			if (result)
				goto finished;
			break;

		case FBUI_FILLAREA: 
			result = fbui_fill_area (info,win,a,b,c,d,param32);
			if (result)
				goto finished;
			break;

		case FBUI_CLEARAREA:
			result = fbui_clear_area (info,win,a,b,c,d);
			if (result)
				goto finished;
			break;

		case FBUI_PUT:
			wid = ary[ix++];
			result= fbui_put (info,win, a,b,wid, (unsigned char*)param32);
			if (result)
				goto finished;
			break;

		case FBUI_PUTRGB: 
			wid = ary[ix++];
			result= fbui_put_rgb (info,win,a,b,wid, (unsigned long*)param32);
			if (result)
				goto finished;
			break;

		case FBUI_PUTRGB3:
			wid = ary[ix++];
			result= fbui_put_rgb3 (info,win, a,b,wid, (unsigned char*)param32);
			if (result)
				goto finished;
			break;

		case FBUI_COPYAREA:
			wid = ary[ix++];
			ht = ary[ix++];
			result = fbui_copy_area (info,win,a,b, wid,ht,c,d);
			if (result)
				goto finished;
			break;

		case FBUI_STRING: {
			struct fbui_font *font = &win->font;
			u32 color;
			wid = ary[ix++];

			ptr = ary[ix+1];
			ptr <<= 16;
			ptr |= ary[ix];
			ix += 2;

			if (!ptr) {
				result = FBUI_ERR_NULLPTR;
				goto finished;
			}

			color = ary[ix+1];
			color <<= 16;
			color |= ary[ix];
			ix += 2;

			if (!param32) {
				if (!win->font_valid) {
					result = FBUI_ERR_NOFONT;
					goto finished;
				}
			} else {
				if (!access_ok (VERIFY_READ, (void*)param32, FBUI_FONTSIZE)) {
					result = FBUI_ERR_BADADDR;
					goto finished;
				}
				if (copy_from_user ((char*)font,(char*)param32,FBUI_FONTSIZE)) {
					result = FBUI_ERR_BADADDR;
					goto finished;
				}

				win->font_valid = 1;
			}

			if (!access_ok (VERIFY_READ, (void*)ptr, wid)) {
				result = FBUI_ERR_BADADDR;
				goto finished;
			}
			result = fbui_draw_string (info,win,font,a,b,
				(unsigned char*) ptr, color);
			if (result)
				goto finished;
			break;
		  }

		} /* switch */
	}
finished:
	if (!initial_hide && info->pointer_hidden)
		fbui_unhide_pointer (info);

	win->drawing = 0;
	up (&info->windowSems [win->id]);
	return result;
}


/* This is provided for completeness sake.
 * The current fbdump actually uses mmap().
 */
u32 fb_read_point (struct fb_info *info, short x, short y)
{
	u32 bytes_per_pixel, offset, value;
	unsigned char *ptr;

	if (!info || info->state != FBINFO_STATE_RUNNING) 
		return RGB_NOCOLOR;
	if (x < 0 || y < 0 || x >= info->var.xres || y >= info->var.yres)
		return RGB_NOCOLOR;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        offset = y * info->fix.line_length + x * bytes_per_pixel;
        ptr = offset + ((unsigned char*)info->screen_base);

	value = 0;
	switch (bytes_per_pixel) {
	case 4:
		value = fb_readl (ptr);
		break;
	case 3: {
		u32 tmp;
		value = 0xff & fb_readb (ptr); 
		ptr++;
		tmp = 0xff & fb_readb (ptr); 
		ptr++;
		tmp <<= 8;
		value |= tmp;
		tmp = 0xff & fb_readb (ptr);
		tmp <<= 16;
		value |= tmp;
		break;
	}
	case 2:
		value = fb_readw (ptr);
		break;
	case 1:
		value = fb_readb (ptr);
		break;
	}

	return pixel_to_rgb (info, value);
}


short fb_getpixels_rgb (struct fb_info *info, short x, short y, short n,
	unsigned long *dest, char in_kernel)
{
	short xres, yres;
	short i;

	if (!info || !dest)
		return FBUI_ERR_NULLPTR;
	if (n <= 0 || x < 0 || y < 0)
		return 0;
	xres = info->var.xres;
	yres = info->var.yres;
	if (x >= xres)
		return 0;
	if (y >= yres)
		return 0;
	if (x+n >= xres)
		n = xres-x;
	/*----------*/

	i=n;
	while (i > 0) {
		u32 rgb = fb_read_point (info, x, y);
		if (in_kernel) {
			*dest++ = rgb;
		} else {
			/* not needed? */
		}
		x++;
		i--;
	}
	return n;
}


static int fbui_draw_point (struct fb_info *info, struct fbui_window *win, 
		     short x, short y, u32 color)
{
	short mx,my;
	if (!info || !win)
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (x < 0 || y < 0 || x >= win->width || y >= win->height)
		return 0;
	/*----------*/
	x += win->x0;
	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		short mx1;
		short my1;
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;

		if (x >= mx && y >= my && x <= mx1 && y <= my1)
			fbui_hide_pointer (info);
	}

	if (info->fbops->fb_point)
		info->fbops->fb_point (info, x, y, color, win->do_invert);

	return FBUI_SUCCESS;
}


static int fbui_draw_line (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color)
{
	int dx, dy, e, j, xchange;
	char s1, s2;
	short x, y;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (x0 < 0 && x1 < 0) 
		return 0;
	if (y0 < 0 && y1 < 0) 
		return 0;
	j = win->width;
	if (x0 >= j && x1 >= j)
		return 0;
	j = win->height;
	if (y0 >= j && y1 >= j)
		return 0;
	/*----------*/

	if (!win->do_invert) {
		if (x0==x1) {
			if (y0==y1)
				return fbui_draw_point (info,win,x0,y0,color);
			else
				return fbui_draw_vline (info,win,x0,y0,y1,color);
		}
		else
		if (y0==y1) {
			return fbui_draw_hline (info,win,x0,x1,y0,color);
		}
	}

	/* Bresenham */
	s1 = 1;
	s2 = 1;
	x = x0;
	y = y0;

	dx = x1-x0;
	if (dx < 0) {
		dx = -dx;
		s1 = -1;
	}

	dy = y1-y0;
	if (dy < 0) {
		dy = -dy;
		s2 = -1;
	}

	xchange = 0;

	if (dy>dx) {
		int tmp = dx;
		dx = dy;
		dy = tmp;
		xchange = 1;
	}

	e = (dy<<1) - dx;
	for (j=0; j <= dx; j++) 
	{
		fbui_draw_point (info,win,x,y,color);
		
		if (e >= 0) {
			if (xchange)
				x += s1;
			else
				y += s2;
			e -= (dx << 1);
		}
		if (xchange) 
			y += s2;
		else
			x += s1;
		e += (dy << 1);
	}

	return FBUI_SUCCESS;
}


static int fbui_tinyblit (struct fb_info *info, struct fbui_window *win, 
	short x_, short y, short width_, u32 fg, u32 bg, u32 bitmap_)
{
	u32 native_fg=0;
	u32 native_bg=0;
	u32 bytes_per_pixel;
	u32 offset;
	u32 bitmap = bitmap_;
	unsigned char *ptr;
	unsigned char do_bg, width=width_;
	short x1;
	short x, x0;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (x_ >= win->width) 
		return 0;
	if (y < 0 || y >= win->height) 
		return 0;
	if (width > 32)
		width = 32;
	x0 = win->x0;
	x1 = x0 + win->width;
	x = x0 + x_;
	if (x0 + width <= 0)
		return 0;
	/*----------*/

	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		short x2 = x + width - 1;
		short mx = info->mouse_x0;
		short my = info->mouse_y0;
		short mx1 = info->mouse_x1;
		short my1 = info->mouse_y1;
		if (y >= my && y <= my1) {
			if ((mx >= x && mx <= x2) || (mx1 >= x && mx1 <= x2) ||
			    (x >= mx && x <= mx1))
				fbui_hide_pointer (info);
		}
	}

	bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
	offset = y * info->fix.line_length + x * bytes_per_pixel;
	ptr = ((unsigned char*)info->screen_base);
	ptr += offset;

	bitmap <<= (32 - width);

	native_fg = pixel_from_rgb (info, fg);
	do_bg = !(bg & 0xff000000);
	if (do_bg)
		native_bg = pixel_from_rgb (info, bg);

	while (width--) {
		if (x >= x1)
			break;
		if (x >= x0) {
			u32 c = native_fg;
			u32 bit = bitmap >> 31;
			if (!bit)
				c = native_bg;

			if (bit || (!bit && do_bg))
			{
				switch (bytes_per_pixel)
				{
				case 1:	fb_writeb (c, ptr); break;
				case 2:	fb_writew (c, ptr); break;
				case 4:	fb_writel (c, ptr); break;
				case 3: 
					fb_writeb (c, ptr); ptr++; c >>= 8;
					fb_writeb (c, ptr); ptr++; c >>= 8;
					fb_writeb (c, ptr);
					ptr -= 2;
					break;
				}
			}
		}

		bitmap <<= 1;
		ptr += bytes_per_pixel;
		x++;

		if (!do_bg && !bitmap)
			break;
	}

	return FBUI_SUCCESS;
}


static int fbui_draw_hline (struct fb_info *info, struct fbui_window *win, 
	short x0, short x1, short y, u32 color)
{
	int i;
	short mx,my;
	short mx1,my1;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (x0>x1) { 
		short tmp=x0; x0=x1; x1=tmp; 
	}
	if (y < 0 || y >= win->height) 
		return 0;
	i = win->width;
	if (x0 >= i || x1 < 0) 
		return 0;
	if (x0 < 0)
		x0 = 0;
	if (x1 >= i)
		x1 = i-1;
	/*----------*/

	x0 += win->x0;
	x1 += win->x0;
	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;
		if (y >= my && y <= my1) {
			if ((mx >= x0 && mx <= x1) || (mx1 >= x0 && mx1 <= x1) ||
			    (x0 >= mx && x0 <= mx1))
				fbui_hide_pointer (info);
		}
	}

	if (info->fbops->fb_hline)
		info->fbops->fb_hline (info, x0, x1, y, color);

	return FBUI_SUCCESS;
}


static int fbui_draw_vline (struct fb_info *info, struct fbui_window *win, 
	short x, short y0, short y1, u32 color)
{
	short i;
	short mx,my;
	short mx1,my1;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (y0>y1) { 
		short tmp=y0; y0=y1; y1=tmp; 
	}
	if (x < 0 || x >= win->width) 
		return 0;
	i = win->height;
	if (y1 < 0 || y0 >= i)
		return 0;
	if (y0 < 0) 
		y0 = 0;
	if (y1 >= i)
		y1 = i-1;
	/*----------*/

	x += win->x0;
	y0 += win->y0;
	y1 += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;
		if (x >= mx && x < mx1) {
			if ((my >= y0 && my <= y1) || (my1 >= y0 && my1 <= y1) ||
			    (y0 >= my && y0 <= my1))
				fbui_hide_pointer (info);
		}
	}

	if (info->fbops->fb_vline)
		info->fbops->fb_vline (info, x, y0, y1, color);

	return FBUI_SUCCESS;
}


static int fbui_draw_rect (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color)
{
	int retval;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	/*----------*/

	if ((retval = fbui_draw_hline (info, win, x0, x1, y0, color))) 
		return retval;
	if ((retval = fbui_draw_hline (info, win, x0, x1, y1, color))) 
		return retval;
	if ((retval = fbui_draw_vline (info, win, x0, y0, y1, color))) 
		return retval;
	if ((retval = fbui_draw_vline (info, win, x1, y0, y1, color))) 
		return retval;

	return FBUI_SUCCESS;
}


static int fbui_clear_area (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1)
{
	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	/*----------*/

	return fbui_fill_area (info,win,x0,y0,x1,y1, win->bgcolor);
}


static int fbui_fill_area (struct fb_info *info, struct fbui_window *win, 
	short x0, short y0, short x1, short y1, u32 color)
{
	int i;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (x0>x1) { 
		short tmp=x0; x0=x1; x1=tmp; 
	}
	if (y0>y1) { 
		short tmp=y0; y0=y1; y1=tmp; 
	}
	/*----------*/

	i = y0;
	while (i <= y1)
		fbui_draw_hline (info, win, x0, x1, i++, color);

	return FBUI_SUCCESS;
}


int fbui_clear (struct fb_info *info, struct fbui_window *win)
{
	if (!info || !win)
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (win->is_wm)
		return FBUI_SUCCESS;
	/*----------*/

/* printk(KERN_INFO "Entered fbui_clear, window %s %d\n", win->name,win->id); */

	fbui_clear_area (info, win, 0,0, win->width-1,win->height-1);

	/* Following is needed since fbui_clear called from many places,
	 * not just fbui_exec which does its own unhide
	 */
	fbui_unhide_pointer (info);

	return FBUI_SUCCESS;
}


static unsigned char leftshift[4] = {
	24, 16, 8, 0
};



/* Returns width of string if > 0, else an error code
 */
static int fbui_draw_string (struct fb_info *info, struct fbui_window *win,
	struct fbui_font *font,
	short x, short y, unsigned char *str, u32 color)
{
	unsigned char *bitmap, bitwidth, bitheight;
	short ytop, total_width = 0;
	short j;

	if (!info || !win || !font || !str)
		return FBUI_ERR_NULLPTR;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (x >= win->width) 
		return 0;
	if (y + (font->ascent + font->descent) <= 0)
		return 0;
	if (y >= win->height) 
		return 0;
	/*----------*/

	while (x < win->width)
	{
		char left, descent;
		unsigned char ch;
		unsigned char width;

		if (get_user (ch, str))
			return FBUI_ERR_BADADDR;
		if (!ch)
			break;
		str++;

		if (ch < font->first_char || ch > font->last_char)
			continue;

		ch -= font->first_char;
		if (ch >= 128 && ch < 160) {	
			/* our PCF Fonts move [128,159] to end */
			ch &= 31;
			ch |= 240;
		} 
		else if (ch >= 160)
			ch -= 32;

		if (get_user (bitmap, &font->bitmaps[ch])
		 || get_user (bitwidth, &font->bitwidths[ch])
		 || get_user (bitheight, &font->heights[ch])
		 || get_user (width, &font->widths[ch])
		 || get_user (left, &font->lefts[ch])
		 || get_user (descent, &font->descents[ch]))
			return FBUI_ERR_BADADDR;

		if (!bitmap || !bitwidth || !bitheight || !width)
			continue;

		ytop = font->ascent + descent;
		ytop -= bitheight;

		/* Draw loop */
		j = 0;
		while (j < bitheight) 
		{
			unsigned long bits;
			short current_x;
			short current_y = y + j + ytop;
			unsigned char datum;
			unsigned char bytes_per_row;

			bytes_per_row = (bitwidth+7)/8;
			if (current_y < 0) {
				bitmap += bytes_per_row;
				goto ds_inner_skip;
			}
			if (current_y >= win->height)
				break;

			current_x = x + left;
			bits = 0;

			switch (bytes_per_row) {
			case 4:
				if (get_user (datum, bitmap))
					return FBUI_ERR_BADADDR;
				bitmap++;
				bits |= datum;
			case 3:
				bits <<= 8;
				if (get_user (datum, bitmap))
					return FBUI_ERR_BADADDR;
				bitmap++;
				bits |= datum;
			case 2:
				bits <<= 8;
				if (get_user (datum, bitmap))
					return FBUI_ERR_BADADDR;
				bitmap++;
				bits |= datum;
			case 1:
				bits <<= 8;
				if (get_user (datum, bitmap))
					return FBUI_ERR_BADADDR;
				bitmap++;
				bits |= datum;
			}

			bits <<= leftshift [bytes_per_row-1];

			fbui_tinyblit (info, win, current_x, current_y, bitwidth, 
				color, RGB_NOCOLOR, 
				bits);

ds_inner_skip:
			j++;
		}

		x += width;
		total_width += width;

	} /* char loop */

	return total_width;
}



static int fbui_set_geometry (struct fb_info *info, struct fbui_window *win,
                              short x0, short y0, short x1, short y1)
{
	short xres, yres;
	short w,h;

	if (!info || !win) 
		return FBUI_ERR_NULLPTR;
	xres=info->var.xres;
	yres=info->var.yres;
	if (x0>x1) { 
		int tmp=x0; x0=x1; x1=tmp; 
	}
	if (y0>y1) { 
		int tmp=y0; y0=y1; y1=tmp; 
	}
	if (x1<0 || x0>=xres) 
		return FBUI_ERR_OFFSCREEN;
	if (y1<0 || y0>=yres) 
		return FBUI_ERR_OFFSCREEN;
	if (x0<0) 
		x0=0;
	if (y0<0) 
		y0=0;
	if (x1>=xres) 
		x1=xres-1;
	if (y1>=yres) 
		y1=yres-1;
	/*----------*/

	w = x1-x0+1;
	h = y1-y0+1;
	if (win->max_width && w > win->max_width)
		w = win->max_width;
	if (win->max_height && h > win->max_height)
		h = win->max_height;

	win->x0 = x0;
	win->y0 = y0;
	win->x1 = x0 + w - 1;
	win->y1 = y0 + h - 1;
	win->width = w;
	win->height = h;
/* printk (KERN_INFO "fbui_set_geometry: %s (id=%d) is now at %d %d %d %d , wh = %d %d\n",win->name,win->id,x0,y0,x0+w-1,y0+h-1,w,h); */

	return FBUI_SUCCESS;
}


/* Each pixel is 4 bytes, with 4th being transparency (!=0 => 100% transparent)
 */
void fb_putpixels_rgb (struct fb_info *info, short x, short y,
	short n, unsigned long *src, char in_kernel)
{
        int offset;
        u32 bytes_per_pixel;
        unsigned char *ptr;
	int i;
	u32 *src2;
	short xres, yres;

	if (!info || !src || n<=0) 
		return;
	if (n <= 0 || y < 0)
		return;
	yres = info->var.yres;
	if (y >= yres)
		return;
	xres = info->var.xres;
	if (x < 0) {
		n += x;
		x = 0;
	}
	if (x+n < 0)
		return;
	if (x >= xres)
		return;
	if ((x+n) >= xres)
		n = xres - x;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        offset = y * info->fix.line_length + x * bytes_per_pixel;
        ptr = info->screen_base;
	if (!ptr) {
		return;
	}
        ptr += offset;
	
	src2 = (u32*) src;
	i=n;
	while (i > 0) {
		u32 value;
		if (in_kernel) {
			value = *src2++;
		} else {
			if (get_user(value, src2)) 
				return;
			src2++;
		}

		/* XX real transparency is for later */
		if (!(0xff000000 & value)) {
			value = pixel_from_rgb (info, value);

			switch (bytes_per_pixel)
			{
			case 4:
				fb_writel (value, ptr);
				ptr += 4;
				break;

			case 3:
				fb_writeb (value, ptr); ptr++; value >>= 8;
				fb_writeb (value, ptr); ptr++; value >>= 8;
				fb_writeb (value, ptr); ptr++;
				break;

			case 2:
				fb_writew (value, ptr); 
				ptr += 2;
				break;

			case 1:
				fb_writeb (value, ptr); 
				*ptr++ = value;
				break;

			default:
				break;
			}
		} else {
			ptr += bytes_per_pixel;
		}
		i--;
	}
}

void fb_putpixels_native (struct fb_info *info, short x,short y,
	short n, unsigned char *src, char in_kernel)
{
        short bytes_per_pixel;
	u32 offset;
        unsigned char *ptr;
	int i;

	if (!info || !src || n<=0) 
		return;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        offset = y * info->fix.line_length + x * bytes_per_pixel;
        ptr = info->screen_base;
	if (!ptr) 
		return;
        ptr += offset;

	n *= bytes_per_pixel;
	i=n;
	while (i > 0) {
		unsigned char value;
		if (in_kernel) {
			value = *src++;
		} else {
			if (get_user(value, src)) 
				return;
			src++;
		}
		fb_writeb (value, ptr); 
		ptr++;
		i--;
	}
}

void fb_putpixels_rgb3 (struct fb_info *info, short x,short y,
	short n, unsigned char *src, char in_kernel)
{
        u32 bytes_per_pixel;
	u8 *src2;
        int offset;
        unsigned char *ptr;
	int i;

	if (!info || !src || n<=0) 
		return;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        offset = y * info->fix.line_length + x * bytes_per_pixel;
        ptr = info->screen_base;
	if (!ptr)
		return;

        ptr += offset;
	
	src2 = (u8*) src;
	i=n;
	while (i > 0) {
		u32 value;
		u32 r,g,b;
		u8 byte;
		if (in_kernel) {
			r = *src2++;
			g = *src2++;
			b = *src2++;
		} else {
			if (get_user(byte, src2)) {
				return;
			}
			src2++;
			r = byte;
			if (get_user(byte, src2)) {
				return;
			}
			g = byte;
			src2++;
			if (get_user(byte, src2)) {
				return;
			}
			b = byte;
			src2++;
		}

		r >>= (8 - info->var.red.length);
		g >>= (8 - info->var.green.length);
		b >>= (8 - info->var.blue.length);
		r <<= info->var.red.offset;
		g <<= info->var.green.offset;
		b <<= info->var.blue.offset;
		value = r | g | b;

		switch (bytes_per_pixel)
		{
		case 4:
			fb_writel (value, ptr);
			ptr += 4;
			break;

		case 3:
			fb_writeb (value, ptr); ptr++; value >>= 8;
			fb_writeb (value, ptr); ptr++; value >>= 8;
			fb_writeb (value, ptr); ptr++;
			break;

		case 2:
			fb_writew (value, ptr); 
			ptr += 2;
			break;

		case 1:
			fb_writeb (value, ptr); 
			*ptr++ = value;
			break;

		default:
			break;
		}
		i--;
	}
}


static int fbui_put (struct fb_info *info, struct fbui_window *win, 
	short x,short y, short n, unsigned char *src)
{
	u32 length;
	int bytes_per_pixel;
	short mx,my;
	int i;

	if (!info || !win || !src) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
	length = bytes_per_pixel * n;
	if (!access_ok (VERIFY_READ,(void*)src, length)) 
		return FBUI_ERR_BADADDR;
	if (x >= win->width || y<0 || y >= win->height || (x+n-1) < 0)
		return 0;
	if (x < 0) { 
		x = -x;
		n -= x;
		src += x * bytes_per_pixel;
		x = 0;
	}
	i = x+n-1;
	if (i >= win->width) { 
		short dif = i - win->width; 
		n -= dif; 
		n--;
	}
	if (info->var.red.length > 8 ||
	    info->var.green.length > 8 ||
	    info->var.blue.length > 8) 
		return FBUI_ERR_BIGENDIAN;
	/*----------*/

	x += win->x0;
	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		short mx1,my1;
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;
		if (y >= my && y <= my1) {
			if ((mx >= x && mx < x+n) || (mx1 >= x && mx1 < x+n)) 
				fbui_hide_pointer (info);
		}
	}

	if (info->fbops->fb_putpixels_native)
		info->fbops->fb_putpixels_native (info, x, y, n, src, 0);

	return FBUI_SUCCESS;
}


/* Source data are supposed to be ulong aligned, 
 * and stored as RGBxRGBx... where x is unused.
 *
 * XX perhaps later, transparency would be nice.
 */
static int fbui_put_rgb (struct fb_info *info, struct fbui_window *win, 
	short x, short y,short n, unsigned long *src)
{
	u32 length;
	short mx,my;
	short mx1,my1;
	int i;

	if (!info || !win || !src) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	length = n << 2; 
	if (!access_ok (VERIFY_READ,(void*)src, length)) 
		return FBUI_ERR_BADADDR;
	if (x >= win->width || y<0 || y >= win->height || (x+n-1) < 0)
		return 0;
	if (x < 0) { 
		n += x;
		src -= x;
		x = 0;
	}
	i = x+n-1;
	if (i >= win->width) { 
		short dif = i - win->width; 
		n -= dif; 
		n--;
	}
	if (info->var.red.length > 8 ||
	    info->var.green.length > 8 ||
	    info->var.blue.length > 8) 
		return FBUI_ERR_BIGENDIAN;
	/*----------*/

	x += win->x0;
	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;
		if (y >= my && y <= my1) {
			if ((mx >= x && mx < x+n) || (mx1 >= x && mx1 <= x+n))
				fbui_hide_pointer (info);
		}
	}

	if (info->fbops->fb_putpixels_rgb)
		info->fbops->fb_putpixels_rgb (info, x, y, n, src, 0);

	return FBUI_SUCCESS;
}




/* same as fbui_put_rgb, except the source data are RGBRGBRGB... */

static int fbui_put_rgb3 (struct fb_info *info, struct fbui_window *win, 
	short x,short y, short n, unsigned char *src)
{
        u32 length;
	int i;

	if (!info || !win || !src) 
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (!src) 
		return FBUI_ERR_NULLPTR;
	length = n * 3;
	if (!access_ok (VERIFY_READ,(void*)src, length)) 
		return FBUI_ERR_BADADDR;
	if (x >= win->width || y<0 || y >= win->height || (x+n-1) < 0)
		return 0;
	if (x < 0) { 
		x = -x;
		n -= x;
		src += x * 3;
		x = 0;
	}
	i = x+n-1;
	if (i >= win->width) { 
		short dif = i - win->width; 
		n -= dif; 
		n--;
	}
	if (info->var.red.length > 8 ||
	    info->var.green.length > 8 ||
	    info->var.blue.length > 8) 
		return FBUI_ERR_BIGENDIAN;
	/*----------*/

	x += win->x0;
	y += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && !info->pointer_hidden) {
		short mx,my;
		short mx1, my1;
		mx = info->mouse_x0;
		my = info->mouse_y0;
		mx1 = info->mouse_x1;
		my1 = info->mouse_y1;
		if (y >= my && y <= my1) {
			if ((mx >= x && mx < x+n) || (mx1 >= x && mx1 <= x+n))
				fbui_hide_pointer (info);
		}
	}

	if (info->fbops->fb_putpixels_rgb3)
		info->fbops->fb_putpixels_rgb3 (info, x, y, n, src, 0);

	return FBUI_SUCCESS;
}



static void fbui_copy_within (unsigned char *dest, unsigned char *src, u32 n)
{
	int dif = dest < src ? src - dest : dest - src;
	while (n) {
		if (dif >= 4 && n >= 64 && !(3 & (u32)src) && !(3 & (u32)dest)){
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			fb_writel (fb_readl (src), dest); src+=4; dest+=4;
			n -= 64;
		} else {
			fb_writeb (fb_readb (src), dest); src++; dest++;
			n--;
		}
	}
}


/* XX Would be nice to be able to halt the copy prematurely when window gets hidden.
 */
void fb_copyarea (struct fb_info *info, 
		short xsrc,short ysrc,short w, short h, 
		short xdest,short ydest)
{
        u32 bytes_per_pixel, offset, linelen;
        unsigned char *src;
        unsigned char *dest;
	int n;

	if (!info || w<=0 || h<=0)
		return;
	/*----------*/

        bytes_per_pixel = (info->var.bits_per_pixel + 7) >> 3;
        linelen = info->fix.line_length;
	offset = ysrc * linelen + xsrc * bytes_per_pixel;
        src = info->screen_base;
	if (!src) 
		return;

        dest = src;
        src += offset;
	offset = ydest * linelen + xdest * bytes_per_pixel;
	dest += offset;
	n = w * bytes_per_pixel;

	if (ydest < ysrc) {
		int j=0;
		while (j < h) {
			fbui_copy_within (dest,src,n);
			dest += linelen;
			src += linelen;
			j++;
		}
	} else
	if (ydest > ysrc) {
		int j=h-1;
		dest += linelen * h;
		src += linelen * h;
		while (j >= 0) {
			dest -= linelen;
			src -= linelen;
			fbui_copy_within (dest,src,n);
			j--;
		}
	} else { 
		/* y equal */
		int dif = xsrc > xdest ? xsrc-xdest : xdest-xsrc;

		/* leftgoing */
		if (xdest < xsrc) {
			int j=0;
			while (j < h) {
				register unsigned char *d = dest;
				register unsigned char *s = src;
				register int i = w * bytes_per_pixel;
				while (i) {
					if (dif>3 && !(3 & (u32) s) && i>3) {
						u32 v = fb_readl (s); s+= 4;
						if (!(3 & (u32) d)) {
							fb_writel (v, d); 
							d += 4;
							i -= 4;

							while (i >= 4) {
								v= fb_readl (s);
								s += 4;
								fb_writel(v, d);
								d += 4;
								i -= 4;
							}
						} else {
							fb_writeb (v, d); 
							d++; v >>= 8;
							fb_writeb (v, d); 
							d++; v >>= 8;
							fb_writeb (v, d); 
							d++; v >>= 8;
							fb_writeb (v, d); 
							d++;
							i -= 4;
						}
					} else {
						fb_writeb (fb_readb (s), d);
						s++;
						d++;
						i--;
					}
				}
				dest += linelen;
				src += linelen;
				j++;
			}
			
		} else {
		/* rightgoing */
			int j=0;
			while (j < h) {
				int i = w * bytes_per_pixel;
				unsigned char *d = dest + i;
				unsigned char *s = src + i;
				while (i) {
					if (dif>3 && !(3 & (u32) s) && i>3) 
					{
						u32 v;
						s -= 4;
						v = fb_readl (s);
						if (!(3 & (u32) d)) {
							d -= 4;
							fb_writel (v, d);
							i -= 4;

							while (i >= 4) {
								v=fb_readl(s-3);
								s -= 4;
								fb_writel(v,d-3); 
								d -= 4;
								i -= 4;
							}
						} else {
							d--;
							fb_writeb (v>>24, d); 
							d--;
							fb_writeb (v>>16, d); 
							d--;
							fb_writeb (v>>8, d); 
							d--;
							fb_writeb (v, d); 
							i -= 4;
						}
					} else {
						s--;
						d--;
						fb_writeb (fb_readb (s), d);
						i--;
					}
				}
				dest += linelen;
				src += linelen;
				j++;
			}
		}
	}
}

int fbui_copy_area (struct fb_info *info, struct fbui_window *win, 
	short xsrc,short ysrc,short w, short h, short xdest,short ydest)
{
	short win_w, win_h;

	if (!info || !win)
		return FBUI_ERR_NULLPTR;
	if (info->state != FBINFO_STATE_RUNNING) 
		return FBUI_ERR_NOTRUNNING;
	if (win->console != info->currcon)
		return FBUI_SUCCESS;
	if (win->is_hidden)
		return FBUI_SUCCESS;
	if (xsrc==xdest && ysrc==ydest)
		return 0;
	win_w = win->width;
	win_h = win->height;
	if ((xsrc+w-1) < 0 || (ysrc+h-1) < 0) 
		return 0;
	if (xsrc >= win_w || ysrc >= win_h) 
		return 0;
	if ((xdest+w-1) < 0 || (ydest+h-1) < 0) 
		return 0;
	if (xdest >= win_w || ydest >= win_h) 
		return 0;
	if (xsrc < 0) { 
		xdest -= xsrc; 
		w += xsrc; 
		xsrc=0;
	}
	if (ysrc < 0) { 
		ydest -= ysrc;
		h += ysrc;
		ysrc=0;
	}
	if ((xsrc+w-1) >= win_w) { 
		short dif=(xsrc+w) - win_w;
		w -= dif; 
	}
	if ((ysrc+h-1) >= win_h) { 
		short dif=(ysrc+h) - win_h;
		h -= dif; 
	}
	if ((xdest+w-1) > win_w) { 
		short dif=(xdest+w) - win_w;
		w -= dif; 
	}
	if ((ydest+h-1) > win_h) { 
		short dif=(ydest+h) - win_h;
		h -= dif; 
	}
	/*----------*/

	xsrc += win->x0;
	ysrc += win->y0;
	xdest += win->x0;
	ydest += win->y0;

	if (!info->have_hardware_pointer && info->pointer_active && 
	    !info->pointer_hidden && pointer_in_window (info, win, 0))
		fbui_hide_pointer (info);

	if (info->fbops->fb_copyarea2)
		info->fbops->fb_copyarea2 (info, xsrc,ysrc,w,h,xdest,ydest);

	return FBUI_SUCCESS;
}


int fbui_release (struct fb_info *info, int user)
{
	if (!info)
		return 0;

	return 0;
}

EXPORT_SYMBOL(fbui_init);
EXPORT_SYMBOL(fbui_release);
EXPORT_SYMBOL(fbui_control);
EXPORT_SYMBOL(fbui_open);
EXPORT_SYMBOL(fbui_switch);
EXPORT_SYMBOL(fbui_close);
EXPORT_SYMBOL(fbui_exec);

EXPORT_SYMBOL(fb_clear);
EXPORT_SYMBOL(fb_hline);
EXPORT_SYMBOL(fb_vline);
EXPORT_SYMBOL(fb_point);
EXPORT_SYMBOL(fb_read_point);
EXPORT_SYMBOL(fb_copyarea);
EXPORT_SYMBOL(fb_putpixels_native);
EXPORT_SYMBOL(fb_putpixels_rgb);
EXPORT_SYMBOL(fb_getpixels_rgb);
EXPORT_SYMBOL(fb_putpixels_rgb3);
EXPORT_SYMBOL(pixel_from_rgb);
EXPORT_SYMBOL(pixel_to_rgb);

MODULE_AUTHOR("Zachary T Smith <fbui@comcast.net>")
MODULE_DESCRIPTION("In-kernel graphical user interface for applications");
MODULE_LICENSE("GPL");

