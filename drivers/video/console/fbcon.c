/*
 *  linux/drivers/video/fbcon.c -- Low level frame buffer based console driver
 *
 *	Copyright (C) 1995 Geert Uytterhoeven
 *
 *
 *  This file is based on the original Amiga console driver (amicon.c):
 *
 *	Copyright (C) 1993 Hamish Macdonald
 *			   Greg Harp
 *	Copyright (C) 1994 David Carter [carter@compsci.bristol.ac.uk]
 *
 *	      with work by William Rucklidge (wjr@cs.cornell.edu)
 *			   Geert Uytterhoeven
 *			   Jes Sorensen (jds@kom.auc.dk)
 *			   Martin Apel
 *
 *  and on the original Atari console driver (atacon.c):
 *
 *	Copyright (C) 1993 Bjoern Brauel
 *			   Roman Hodek
 *
 *	      with work by Guenther Kelleter
 *			   Martin Schaller
 *			   Andreas Schwab
 *
 *  Hardware cursor support added by Emmanuel Marty (core@ggi-project.org)
 *  Smart redraw scrolling, arbitrary font width support, 512char font support
 *  and software scrollback added by 
 *                         Jakub Jelinek (jj@ultra.linux.cz)
 *
 *  Random hacking by Martin Mares <mj@ucw.cz>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 *  The low level operations for the various display memory organizations are
 *  now in separate source files.
 *
 *  Currently the following organizations are supported:
 *
 *    o afb			Amiga bitplanes
 *    o cfb{2,4,8,16,24,32}	Packed pixels
 *    o ilbm			Amiga interleaved bitplanes
 *    o iplan2p[248]		Atari interleaved bitplanes
 *    o mfb			Monochrome
 *    o vga			VGA characters/attributes
 *
 *  To do:
 *
 *    - Implement 16 plane mode (iplan2p16)
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#undef FBCONDEBUG

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>	/* MSch: for IRQ probe */
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/font.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#ifdef CONFIG_ATARI
#include <asm/atariints.h>
#endif
#ifdef CONFIG_MAC
#include <asm/macints.h>
#endif
#if defined(__mc68000__) || defined(CONFIG_APUS)
#include <asm/machdep.h>
#include <asm/setup.h>
#endif

#include "fbcon.h"

#ifdef FBCONDEBUG
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

struct display fb_display[MAX_NR_CONSOLES];
signed char con2fb_map[MAX_NR_CONSOLES];
static int logo_height;
static int logo_lines;
static int logo_shown = -1;
/* Software scrollback */
int fbcon_softback_size = 32768;
static unsigned long softback_buf, softback_curr;
static unsigned long softback_in;
static unsigned long softback_top, softback_end;
static int softback_lines;
/* console mappings */
static int first_fb_vc;
static int last_fb_vc = MAX_NR_CONSOLES - 1;
static int fbcon_is_default = 1; 
/* font data */
static char fontname[40];

/* current fb_info */
static int info_idx =  -1;

#define REFCOUNT(fd)	(((int *)(fd))[-1])
#define FNTSIZE(fd)	(((int *)(fd))[-2])
#define FNTCHARCNT(fd)	(((int *)(fd))[-3])
#define FNTSUM(fd)	(((int *)(fd))[-4])
#define FONT_EXTRA_WORDS 4

#define CM_SOFTBACK	(8)

#define advance_row(p, delta) (unsigned short *)((unsigned long)(p) + (delta) * vc->vc_size_row)

static void fbcon_free_font(struct display *);
static int fbcon_set_origin(struct vc_data *);

#define CURSOR_DRAW_DELAY		(1)

/* # VBL ints between cursor state changes */
#define ARM_CURSOR_BLINK_RATE		(10)
#define ATARI_CURSOR_BLINK_RATE		(42)
#define MAC_CURSOR_BLINK_RATE		(32)
#define DEFAULT_CURSOR_BLINK_RATE	(20)

static int vbl_cursor_cnt;

#define divides(a, b)	((!(a) || (b)%(a)) ? 0 : 1)

/*
 *  Interface used by the world
 */

static const char *fbcon_startup(void);
static void fbcon_init(struct vc_data *vc, int init);
static void fbcon_deinit(struct vc_data *vc);
static void fbcon_clear(struct vc_data *vc, int sy, int sx, int height,
			int width);
static void fbcon_putc(struct vc_data *vc, int c, int ypos, int xpos);
static void fbcon_putcs(struct vc_data *vc, const unsigned short *s,
			int count, int ypos, int xpos);
static void fbcon_cursor(struct vc_data *vc, int mode);
static int fbcon_scroll(struct vc_data *vc, int t, int b, int dir,
			int count);
static void fbcon_bmove(struct vc_data *vc, int sy, int sx, int dy, int dx,
			int height, int width);
static int fbcon_switch(struct vc_data *vc);
static int fbcon_blank(struct vc_data *vc, int blank, int mode_switch);
static int fbcon_set_palette(struct vc_data *vc, unsigned char *table);
static int fbcon_scrolldelta(struct vc_data *vc, int lines);
void accel_clear_margins(struct vc_data *vc, struct fb_info *info,
			 int bottom_only);


/*
 *  Internal routines
 */
static __inline__ int real_y(struct display *p, int ypos);
static __inline__ void ywrap_up(struct vc_data *vc, int count);
static __inline__ void ywrap_down(struct vc_data *vc, int count);
static __inline__ void ypan_up(struct vc_data *vc, int count);
static __inline__ void ypan_down(struct vc_data *vc, int count);
static void fbcon_bmove_rec(struct vc_data *vc, struct display *p, int sy, int sx,
			    int dy, int dx, int height, int width, u_int y_break);
static void fbcon_set_disp(struct fb_info *info, struct vc_data *vc);
static void fbcon_redraw_move(struct vc_data *vc, struct display *p,
			      int line, int count, int dy);

#ifdef CONFIG_MAC
/*
 * On the Macintoy, there may or may not be a working VBL int. We need to probe
 */
static int vbl_detected;

static irqreturn_t fb_vbl_detect(int irq, void *dummy, struct pt_regs *fp)
{
	vbl_detected++;
	return IRQ_HANDLED;
}
#endif

static void fb_flashcursor(void *private)
{
	struct fb_info *info = (struct fb_info *) private;
	struct vc_data *vc = NULL;

	if (info->currcon != -1)
		vc = vc_cons[info->currcon].d;

	if (info->state != FBINFO_STATE_RUNNING ||
	    info->cursor.rop == ROP_COPY || !vc || !CON_IS_VISIBLE(vc)
	    || registered_fb[(int) con2fb_map[vc->vc_num]] != info)
		return;
	acquire_console_sem();
	info->cursor.enable ^= 1;
	info->fbops->fb_cursor(info, &info->cursor);
	release_console_sem();
}

#if (defined(__arm__) && defined(IRQ_VSYNCPULSE)) || defined(CONFIG_ATARI) || defined(CONFIG_MAC)
static int cursor_blink_rate;
static irqreturn_t fb_vbl_handler(int irq, void *dev_id, struct pt_regs *fp)
{
	struct fb_info *info = dev_id;

	if (vbl_cursor_cnt && --vbl_cursor_cnt == 0) {
		schedule_work(&info->queue);	
		vbl_cursor_cnt = cursor_blink_rate; 
	}
	return IRQ_HANDLED;
}
#endif
	
static void cursor_timer_handler(unsigned long dev_addr)
{
	struct fb_info *info = (struct fb_info *) dev_addr;
	
	schedule_work(&info->queue);
	mod_timer(&info->cursor_timer, jiffies + HZ/5);
}

int __init fb_console_setup(char *this_opt)
{
	char *options;
	int i, j;

	if (!this_opt || !*this_opt)
		return 0;

	while ((options = strsep(&this_opt, ",")) != NULL) {
		if (!strncmp(options, "font:", 5))
			strcpy(fontname, options + 5);
		
		if (!strncmp(options, "scrollback:", 11)) {
			options += 11;
			if (*options) {
				fbcon_softback_size = simple_strtoul(options, &options, 0);
				if (*options == 'k' || *options == 'K') {
					fbcon_softback_size *= 1024;
					options++;
				}
				if (*options != ',')
					return 0;
				options++;
			} else
				return 0;
		}
		
		if (!strncmp(options, "map:", 4)) {
			options += 4;
			if (*options)
				for (i = 0, j = 0; i < MAX_NR_CONSOLES; i++) {
					if (!options[j])
						j = 0;
					con2fb_map[i] = (options[j++]-'0') % FB_MAX;
				}
			return 0;
		}

		if (!strncmp(options, "vc:", 3)) {
			options += 3;
			if (*options)
				first_fb_vc = simple_strtoul(options, &options, 10) - 1;
			if (first_fb_vc < 0)
				first_fb_vc = 0;
			if (*options++ == '-')
				last_fb_vc = simple_strtoul(options, &options, 10) - 1;
			fbcon_is_default = 0; 
		}	
	}
	return 0;
}

__setup("fbcon=", fb_console_setup);

static int search_fb_in_map(int idx)
{
	int i;

	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (con2fb_map[i] == idx)
			return 1;
	}
	return 0;
}

static int search_for_mapped_con(void)
{
	int i;

	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (con2fb_map[i] != -1)
			return 1;
	}
	return 0;
}

static int fbcon_takeover(void)
{
	int err, i;

	if (!num_registered_fb)
		return -ENODEV;

	for (i = first_fb_vc; i <= last_fb_vc; i++)
		con2fb_map[i] = info_idx;

	err = take_over_console(&fb_con, first_fb_vc, last_fb_vc,
				fbcon_is_default);
	if (err) {
		for (i = first_fb_vc; i <= last_fb_vc; i++) {
			con2fb_map[i] = -1;
		}
		info_idx = -1;
	}

	return err;
}

/**
 *	set_con2fb_map - map console to frame buffer device
 *	@unit: virtual console number to map
 *	@newidx: frame buffer index to map virtual console to
 *
 *	Maps a virtual console @unit to a frame buffer device
 *	@newidx.
 */
static int set_con2fb_map(int unit, int newidx)
{
	struct vc_data *vc = vc_cons[unit].d;
	int oldidx = con2fb_map[unit];
	struct fb_info *info = registered_fb[newidx];
	struct fb_info *oldinfo = NULL;
	int found;

	if (oldidx == newidx)
		return 0;

	if (!vc)
	    return -ENODEV;

	if (!search_for_mapped_con()) {
		info_idx = newidx;
		return fbcon_takeover();
	}

	if (oldidx != -1)
		oldinfo = registered_fb[oldidx];

	found = search_fb_in_map(newidx);

	acquire_console_sem();
	con2fb_map[unit] = newidx;
	if (!found) {
		if (!try_module_get(info->fbops->owner)) {
			con2fb_map[unit] = oldidx;
			release_console_sem();
			return -ENODEV;
		}
		if (info->fbops->fb_open && info->fbops->fb_open(info, 0)) {
			module_put(info->fbops->owner);
			con2fb_map[unit] = oldidx;
			release_console_sem();
			return -ENODEV;
		}
	}

	/*
	 * If old fb is not mapped to any of the consoles,
	 * fbcon should release it.
	 */
	if (oldinfo && !search_fb_in_map(oldidx)) {
		if (oldinfo->fbops->fb_release &&
		    oldinfo->fbops->fb_release(oldinfo, 0)) {
			con2fb_map[unit] = oldidx;
			if (!found && info->fbops->fb_release)
				info->fbops->fb_release(info, 0);
			if (!found)
				module_put(info->fbops->owner);
			release_console_sem();
			return -ENODEV;
		}
		if (oldinfo->queue.func == fb_flashcursor)
			del_timer_sync(&oldinfo->cursor_timer);
		module_put(oldinfo->fbops->owner);
	}
	info->currcon = -1;
	if (!found) {
		if (!info->queue.func || info->queue.func == fb_flashcursor) {
			if (!info->queue.func)
				INIT_WORK(&info->queue, fb_flashcursor, info);

			init_timer(&info->cursor_timer);
			info->cursor_timer.function = cursor_timer_handler;
			info->cursor_timer.expires = jiffies + HZ / 5;
			info->cursor_timer.data = (unsigned long ) info;
			add_timer(&info->cursor_timer);
		}
	}
	if (info->fbops->fb_set_par)
		info->fbops->fb_set_par(info);
	fbcon_set_disp(info, vc);
	release_console_sem();
	return 0;
}

/*
 * Accelerated handlers.
 */
static inline int get_color(struct vc_data *vc, struct fb_info *info,
			    u16 c, int is_fg)
{
	int depth = fb_get_color_depth(info);
	int color = 0;

	if (depth != 1)
		color = (is_fg) ? attr_fgcol((vc->vc_hi_font_mask) ? 9 : 8, c)
			: attr_bgcol((vc->vc_hi_font_mask) ? 13 : 12, c);

	switch (depth) {
	case 1:
	{
		/* 0 or 1 */
		int fg = (info->fix.visual != FB_VISUAL_MONO01) ? 1 : 0;
		int bg = (info->fix.visual != FB_VISUAL_MONO01) ? 0 : 1;

		color = (is_fg) ? fg : bg;
		break;
	}
	case 2:
		/*
		 * Scale down 16-colors to 4 colors. Default 4-color palette
		 * is grayscale.
		 */
		color /= 4;
		break;
	case 3:
		/*
		 * Last 8 entries of default 16-color palette is a more intense
		 * version of the first 8 (i.e., same chrominance, different
		 * luminance).
		 */
		color &= 7;
		break;
	}

	return color;
}

#define FBCON_ATTRIBUTE_UNDERLINE 1
#define FBCON_ATTRIBUTE_REVERSE   2
#define FBCON_ATTRIBUTE_BOLD      4

static inline int get_attribute(struct fb_info *info, u16 c)
{
	int attribute = 0;

	if (fb_get_color_depth(info) == 1) {
		if (attr_underline(c))
			attribute |= FBCON_ATTRIBUTE_UNDERLINE;
		if (attr_reverse(c))
			attribute |= FBCON_ATTRIBUTE_REVERSE;
		if (attr_bold(c))
			attribute |= FBCON_ATTRIBUTE_BOLD;
	}

	return attribute;
}

static inline void update_attr(u8 *dst, u8 *src, int attribute,
			       struct vc_data *vc)
{
	int i, offset = (vc->vc_font.height < 10) ? 1 : 2;
	int width = (vc->vc_font.width + 7) >> 3;
	unsigned int cellsize = vc->vc_font.height * width;
	u8 c;

	offset = cellsize - (offset * width);
	for (i = 0; i < cellsize; i++) {
		c = src[i];
		if (attribute & FBCON_ATTRIBUTE_UNDERLINE && i >= offset)
			c = 0xff;
		if (attribute & FBCON_ATTRIBUTE_BOLD)
			c |= c >> 1;
		if (attribute & FBCON_ATTRIBUTE_REVERSE)
			c = ~c;
		dst[i] = c;
	}
}

void accel_bmove(struct vc_data *vc, struct fb_info *info, int sy, 
		int sx, int dy, int dx, int height, int width)
{
	struct fb_copyarea area;

	area.sx = sx * vc->vc_font.width;
	area.sy = sy * vc->vc_font.height;
	area.dx = dx * vc->vc_font.width;
	area.dy = dy * vc->vc_font.height;
	area.height = height * vc->vc_font.height;
	area.width = width * vc->vc_font.width;

	info->fbops->fb_copyarea(info, &area);
}

void accel_clear(struct vc_data *vc, struct fb_info *info, int sy,
			int sx, int height, int width)
{
	int bgshift = (vc->vc_hi_font_mask) ? 13 : 12;
	struct fb_fillrect region;

	region.color = attr_bgcol_ec(bgshift, vc);
	region.dx = sx * vc->vc_font.width;
	region.dy = sy * vc->vc_font.height;
	region.width = width * vc->vc_font.width;
	region.height = height * vc->vc_font.height;
	region.rop = ROP_COPY;

	info->fbops->fb_fillrect(info, &region);
}	

void accel_putcs(struct vc_data *vc, struct fb_info *info,
		 const unsigned short *s, int count, int yy, int xx)
{
	void (*move_unaligned)(struct fb_info *info, struct fb_pixmap *buf,
			       u8 *dst, u32 d_pitch, u8 *src, u32 idx,
			       u32 height, u32 shift_high, u32 shift_low,
			       u32 mod);
	void (*move_aligned)(struct fb_info *info, struct fb_pixmap *buf,
			     u8 *dst, u32 d_pitch, u8 *src, u32 s_pitch,
			     u32 height);
	unsigned short charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	unsigned int width = (vc->vc_font.width + 7) >> 3;
	unsigned int cellsize = vc->vc_font.height * width;
	unsigned int maxcnt = info->pixmap.size/cellsize;
	unsigned int scan_align = info->pixmap.scan_align - 1;
	unsigned int buf_align = info->pixmap.buf_align - 1;
	unsigned int shift_low = 0, mod = vc->vc_font.width % 8;
	unsigned int shift_high = 8, pitch, cnt, size, k;
	unsigned int idx = vc->vc_font.width >> 3;
	unsigned int attribute = get_attribute(info, scr_readw(s));
	struct fb_image image;
	u8 *src, *dst, *buf = NULL;

	if (attribute) {
		buf = kmalloc(cellsize, GFP_KERNEL);
		if (!buf)
			return;
	}

	image.fg_color = get_color(vc, info, scr_readw(s), 1);
	image.bg_color = get_color(vc, info, scr_readw(s), 0);

	image.dx = xx * vc->vc_font.width;
	image.dy = yy * vc->vc_font.height;
	image.height = vc->vc_font.height;
	image.depth = 1;

	if (info->pixmap.outbuf && info->pixmap.inbuf) {
		move_aligned = fb_iomove_buf_aligned;
		move_unaligned = fb_iomove_buf_unaligned;
	} else {
		move_aligned = fb_sysmove_buf_aligned;
		move_unaligned = fb_sysmove_buf_unaligned;
	}
	while (count) {
		if (count > maxcnt)
			cnt = k = maxcnt;
		else
			cnt = k = count;

		image.width = vc->vc_font.width * cnt;
		pitch = ((image.width + 7) >> 3) + scan_align;
		pitch &= ~scan_align;
		size = pitch * image.height + buf_align;
		size &= ~buf_align;
		dst = fb_get_buffer_offset(info, &info->pixmap, size);
		image.data = dst;
		if (mod) {
			while (k--) {
				src = vc->vc_font.data + (scr_readw(s++)&
							  charmask)*cellsize;

				if (attribute) {
					update_attr(buf, src, attribute, vc);
					src = buf;
				}

				move_unaligned(info, &info->pixmap, dst, pitch,
					       src, idx, image.height,
					       shift_high, shift_low, mod);
				shift_low += mod;
				dst += (shift_low >= 8) ? width : width - 1;
				shift_low &= 7;
				shift_high = 8 - shift_low;
			}
		} else {
			while (k--) {
				src = vc->vc_font.data + (scr_readw(s++)&
							  charmask)*cellsize;

				if (attribute) {
					update_attr(buf, src, attribute, vc);
					src = buf;
				}

				move_aligned(info, &info->pixmap, dst, pitch,
					     src, idx, image.height);
				dst += width;
			}
		}
		info->fbops->fb_imageblit(info, &image);
		image.dx += cnt * vc->vc_font.width;
		count -= cnt;
	}

	if (buf)
		kfree(buf);
}

void accel_clear_margins(struct vc_data *vc, struct fb_info *info,
				int bottom_only)
{
	int bgshift = (vc->vc_hi_font_mask) ? 13 : 12;
	unsigned int cw = vc->vc_font.width;
	unsigned int ch = vc->vc_font.height;
	unsigned int rw = info->var.xres - (vc->vc_cols*cw);
	unsigned int bh = info->var.yres - (vc->vc_rows*ch);
	unsigned int rs = info->var.xres - rw;
	unsigned int bs = info->var.yres - bh;
	struct fb_fillrect region;

	region.color = attr_bgcol_ec(bgshift, vc);
	region.rop = ROP_COPY;

	if (rw && !bottom_only) {
		region.dx = info->var.xoffset + rs;
		region.dy = 0;
		region.width = rw;
		region.height = info->var.yres_virtual;
		info->fbops->fb_fillrect(info, &region);
	}

	if (bh) {
		region.dx = info->var.xoffset;
		region.dy = info->var.yoffset + bs;
		region.width = rs;
		region.height = bh;
		info->fbops->fb_fillrect(info, &region);
	}	
}	

/*
 *  Low Level Operations
 */
/* NOTE: fbcon cannot be __init: it may be called from take_over_console later */
static int var_to_display(struct display *disp,
			  struct fb_var_screeninfo *var,
			  struct fb_info *info)
{
	disp->xres_virtual = var->xres_virtual;
	disp->yres_virtual = var->yres_virtual;
	disp->bits_per_pixel = var->bits_per_pixel;
	disp->grayscale = var->grayscale;
	disp->nonstd = var->nonstd;
	disp->accel_flags = var->accel_flags;
	disp->height = var->height;
	disp->width = var->width;
	disp->red = var->red;
	disp->green = var->green;
	disp->blue = var->blue;
	disp->transp = var->transp;
	disp->mode = fb_match_mode(var, &info->modelist);
	if (disp->mode == NULL)
		/* This should not happen */
		return -EINVAL;
	return 0;
}

static void display_to_var(struct fb_var_screeninfo *var,
			   struct display *disp)
{
	fb_videomode_to_var(var, disp->mode);
	var->xres_virtual = disp->xres_virtual;
	var->yres_virtual = disp->yres_virtual;
	var->bits_per_pixel = disp->bits_per_pixel;
	var->grayscale = disp->grayscale;
	var->nonstd = disp->nonstd;
	var->accel_flags = disp->accel_flags;
	var->height = disp->height;
	var->width = disp->width;
	var->red = disp->red;
	var->green = disp->green;
	var->blue = disp->blue;
	var->transp = disp->transp;
}

static const char *fbcon_startup(void)
{
	const char *display_desc = "frame buffer device";
	struct display *p = &fb_display[fg_console];
	struct vc_data *vc = vc_cons[fg_console].d;
	struct font_desc *font = NULL;
	struct module *owner;
	struct fb_info *info = NULL;
	int rows, cols;
	int irqres;

	irqres = 1;
	/*
	 *  If num_registered_fb is zero, this is a call for the dummy part.
	 *  The frame buffer devices weren't initialized yet.
	 */
	if (!num_registered_fb || info_idx == -1)
		return display_desc;
	/*
	 * Instead of blindly using registered_fb[0], we use info_idx, set by
	 * fb_console_init();
	 */
	info = registered_fb[info_idx];
	if (!info)
		return NULL;
	info->currcon = -1;
	
	owner = info->fbops->owner;
	if (!try_module_get(owner))
		return NULL;
	if (info->fbops->fb_open && info->fbops->fb_open(info, 0)) {
		module_put(owner);
		return NULL;
	}
	if (info->fix.type != FB_TYPE_TEXT) {
		if (fbcon_softback_size) {
			if (!softback_buf) {
				softback_buf =
				    (unsigned long)
				    kmalloc(fbcon_softback_size,
					    GFP_KERNEL);
				if (!softback_buf) {
					fbcon_softback_size = 0;
					softback_top = 0;
				}
			}
		} else {
			if (softback_buf) {
				kfree((void *) softback_buf);
				softback_buf = 0;
				softback_top = 0;
			}
		}
		if (softback_buf)
			softback_in = softback_top = softback_curr =
			    softback_buf;
		softback_lines = 0;
	}

	/* Setup default font */
	if (!p->fontdata) {
		if (!fontname[0] || !(font = find_font(fontname)))
			font = get_default_font(info->var.xres,
						   info->var.yres);
		vc->vc_font.width = font->width;
		vc->vc_font.height = font->height;
		vc->vc_font.data = p->fontdata = font->data;
		vc->vc_font.charcount = 256; /* FIXME  Need to support more fonts */
	}

	cols = info->var.xres / vc->vc_font.width;
	rows = info->var.yres / vc->vc_font.height;
	vc_resize(vc->vc_num, cols, rows);

	DPRINTK("mode:   %s\n", info->fix.id);
	DPRINTK("visual: %d\n", info->fix.visual);
	DPRINTK("res:    %dx%d-%d\n", info->var.xres,
		info->var.yres,
		info->var.bits_per_pixel);
	con_set_default_unimap(vc->vc_num);

#ifdef CONFIG_ATARI
	if (MACH_IS_ATARI) {
		cursor_blink_rate = ATARI_CURSOR_BLINK_RATE;
		irqres =
		    request_irq(IRQ_AUTO_4, fb_vbl_handler,
				IRQ_TYPE_PRIO, "framebuffer vbl",
				info);
	}
#endif				/* CONFIG_ATARI */

#ifdef CONFIG_MAC
	/*
	 * On a Macintoy, the VBL interrupt may or may not be active. 
	 * As interrupt based cursor is more reliable and race free, we 
	 * probe for VBL interrupts.
	 */
	if (MACH_IS_MAC) {
		int ct = 0;
		/*
		 * Probe for VBL: set temp. handler ...
		 */
		irqres = request_irq(IRQ_MAC_VBL, fb_vbl_detect, 0,
				     "framebuffer vbl", info);
		vbl_detected = 0;

		/*
		 * ... and spin for 20 ms ...
		 */
		while (!vbl_detected && ++ct < 1000)
			udelay(20);

		if (ct == 1000)
			printk
			    ("fbcon_startup: No VBL detected, using timer based cursor.\n");

		free_irq(IRQ_MAC_VBL, fb_vbl_detect);

		if (vbl_detected) {
			/*
			 * interrupt based cursor ok
			 */
			cursor_blink_rate = MAC_CURSOR_BLINK_RATE;
			irqres =
			    request_irq(IRQ_MAC_VBL, fb_vbl_handler, 0,
					"framebuffer vbl", info);
		} else {
			/*
			 * VBL not detected: fall through, use timer based cursor
			 */
			irqres = 1;
		}
	}
#endif				/* CONFIG_MAC */

#if defined(__arm__) && defined(IRQ_VSYNCPULSE)
	cursor_blink_rate = ARM_CURSOR_BLINK_RATE;
	irqres = request_irq(IRQ_VSYNCPULSE, fb_vbl_handler, SA_SHIRQ,
			     "framebuffer vbl", info);
#endif
	/* Initialize the work queue. If the driver provides its
	 * own work queue this means it will use something besides 
	 * default timer to flash the cursor. */
	if (!info->queue.func) {
		INIT_WORK(&info->queue, fb_flashcursor, info);

		init_timer(&info->cursor_timer);
		info->cursor_timer.function = cursor_timer_handler;
		info->cursor_timer.expires = jiffies + HZ / 5;
		info->cursor_timer.data = (unsigned long ) info;
		add_timer(&info->cursor_timer);
	}
	return display_desc;
}

static void fbcon_init(struct vc_data *vc, int init)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct vc_data **default_mode = vc->vc_display_fg;
	struct display *t, *p = &fb_display[vc->vc_num];
	int display_fg = (*default_mode)->vc_num;
	int logo = 1, new_rows, new_cols, rows, cols, charcnt = 256;
	unsigned short *save = NULL, *r, *q;
	int cap = info->flags;

	if (info_idx == -1 || info == NULL)
	    return;
	if (vc->vc_num != display_fg || (info->flags & FBINFO_MODULE) ||
	    (info->fix.type == FB_TYPE_TEXT))
		logo = 0;

	info->var.xoffset = info->var.yoffset = p->yscroll = 0;	/* reset wrap/pan */

	if (var_to_display(p, &info->var, info))
		return;

	/* If we are not the first console on this
	   fb, copy the font from that console */
	t = &fb_display[display_fg];
	if (!vc->vc_font.data) {
		vc->vc_font.data = p->fontdata = t->fontdata;
		vc->vc_font.width = (*default_mode)->vc_font.width;
		vc->vc_font.height = (*default_mode)->vc_font.height;
		p->userfont = t->userfont;
		if (p->userfont)
			REFCOUNT(p->fontdata)++;
		con_copy_unimap(vc->vc_num, display_fg);
	}
	if (p->userfont)
		charcnt = FNTCHARCNT(p->fontdata);
	vc->vc_can_do_color = (fb_get_color_depth(info) != 1);
	vc->vc_complement_mask = vc->vc_can_do_color ? 0x7700 : 0x0800;
	if (charcnt == 256) {
		vc->vc_hi_font_mask = 0;
	} else {
		vc->vc_hi_font_mask = 0x100;
		if (vc->vc_can_do_color)
			vc->vc_complement_mask <<= 1;
	}
	cols = vc->vc_cols;
	rows = vc->vc_rows;
	new_cols = info->var.xres / vc->vc_font.width;
	new_rows = info->var.yres / vc->vc_font.height;
	vc_resize(vc->vc_num, new_cols, new_rows);
	/*
	 * We must always set the mode. The mode of the previous console
	 * driver could be in the same resolution but we are using different
	 * hardware so we have to initialize the hardware.
	 *
	 * We need to do it in fbcon_init() to prevent screen corruption.
	 */
	if (CON_IS_VISIBLE(vc) && info->fbops->fb_set_par)
		info->fbops->fb_set_par(info);


	if ((cap & FBINFO_HWACCEL_COPYAREA) &&
	    !(cap & FBINFO_HWACCEL_DISABLED))
		p->scrollmode = SCROLL_MOVE;
	else /* default to something safe */
		p->scrollmode = SCROLL_REDRAW;

	/*
	 *  ++guenther: console.c:vc_allocate() relies on initializing
	 *  vc_{cols,rows}, but we must not set those if we are only
	 *  resizing the console.
	 */
	if (!init) {
		vc->vc_cols = new_cols;
		vc->vc_rows = new_rows;
	}

	if (logo) {
		/* Need to make room for the logo */
		int cnt, erase = vc->vc_video_erase_char;
		int step;

		/*
		 * remove underline attribute from erase character
		 * if black and white framebuffer.
		 */
		if (fb_get_color_depth(info) == 1)
			erase &= ~0x400;
		logo_height = fb_prepare_logo(info);
		logo_lines = (logo_height + vc->vc_font.height - 1) /
			     vc->vc_font.height;
		q = (unsigned short *) (vc->vc_origin +
					vc->vc_size_row * rows);
		step = logo_lines * cols;
		for (r = q - logo_lines * cols; r < q; r++)
			if (scr_readw(r) != vc->vc_video_erase_char)
				break;
		if (r != q && new_rows >= rows + logo_lines) {
			save = kmalloc(logo_lines * new_cols * 2, GFP_KERNEL);
			if (save) {
				int i = cols < new_cols ? cols : new_cols;
				scr_memsetw(save, erase, logo_lines * new_cols * 2);
				r = q - step;
				for (cnt = 0; cnt < logo_lines; cnt++, r += i)
					scr_memcpyw(save + cnt * new_cols, r, 2 * i);
				r = q;
			}
		}
		if (r == q) {
			/* We can scroll screen down */
			r = q - step - cols;
			for (cnt = rows - logo_lines; cnt > 0; cnt--) {
				scr_memcpyw(r + step, r, vc->vc_size_row);
				r -= cols;
			}
			if (!save) {
				vc->vc_y += logo_lines;
				vc->vc_pos += logo_lines * vc->vc_size_row;
			}
		}
		scr_memsetw((unsigned short *) vc->vc_origin,
			    erase,
			    vc->vc_size_row * logo_lines);

		if (CON_IS_VISIBLE(vc) && vt_cons[vc->vc_num]->vc_mode == KD_TEXT) {
			accel_clear_margins(vc, info, 0);
			update_screen(vc->vc_num);
		}
		if (save) {
			q = (unsigned short *) (vc->vc_origin +
						vc->vc_size_row *
						rows);
			scr_memcpyw(q, save, logo_lines * new_cols * 2);
			vc->vc_y += logo_lines;
			vc->vc_pos += logo_lines * vc->vc_size_row;
			kfree(save);
		}
		if (logo_lines > vc->vc_bottom) {
			logo_shown = -1;
			printk(KERN_INFO
			       "fbcon_init: disable boot-logo (boot-logo bigger than screen).\n");
		} else {
			logo_shown = -2;
			vc->vc_top = logo_lines;
		}
	}

	if (vc->vc_num == display_fg && softback_buf) {
		int l = fbcon_softback_size / vc->vc_size_row;
		if (l > 5)
			softback_end = softback_buf + l * vc->vc_size_row;
		else {
			/* Smaller scrollback makes no sense, and 0 would screw
			   the operation totally */
			softback_top = 0;
		}
	}
}

static void fbcon_deinit(struct vc_data *vc)
{
	struct display *p = &fb_display[vc->vc_num];

	if (info_idx != -1)
	    return;
	fbcon_free_font(p);
}

/* ====================================================================== */

/*  fbcon_XXX routines - interface used by the world
 *
 *  This system is now divided into two levels because of complications
 *  caused by hardware scrolling. Top level functions:
 *
 *	fbcon_bmove(), fbcon_clear(), fbcon_putc()
 *
 *  handles y values in range [0, scr_height-1] that correspond to real
 *  screen positions. y_wrap shift means that first line of bitmap may be
 *  anywhere on this display. These functions convert lineoffsets to
 *  bitmap offsets and deal with the wrap-around case by splitting blits.
 *
 *	fbcon_bmove_physical_8()    -- These functions fast implementations
 *	fbcon_clear_physical_8()    -- of original fbcon_XXX fns.
 *	fbcon_putc_physical_8()	    -- (font width != 8) may be added later
 *
 *  WARNING:
 *
 *  At the moment fbcon_putc() cannot blit across vertical wrap boundary
 *  Implies should only really hardware scroll in rows. Only reason for
 *  restriction is simplicity & efficiency at the moment.
 */

static __inline__ int real_y(struct display *p, int ypos)
{
	int rows = p->vrows;

	ypos += p->yscroll;
	return ypos < rows ? ypos : ypos - rows;
}


static void fbcon_clear(struct vc_data *vc, int sy, int sx, int height,
			int width)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	
	struct display *p = &fb_display[vc->vc_num];
	u_int y_break;

	if (!info->fbops->fb_blank && console_blanked)
		return;
	if (info->state != FBINFO_STATE_RUNNING)
		return;

	if (!height || !width)
		return;

	/* Split blits that cross physical y_wrap boundary */

	y_break = p->vrows - p->yscroll;
	if (sy < y_break && sy + height - 1 >= y_break) {
		u_int b = y_break - sy;
		accel_clear(vc, info, real_y(p, sy), sx, b, width);
		accel_clear(vc, info, real_y(p, sy + b), sx, height - b,
				 width);
	} else
		accel_clear(vc, info, real_y(p, sy), sx, height, width);
}

static void fbcon_putcs(struct vc_data *vc, const unsigned short *s,
			int count, int ypos, int xpos)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];

	if (!info->fbops->fb_blank && console_blanked)
		return;
	if (info->state != FBINFO_STATE_RUNNING)
		return;

	if (vt_cons[vc->vc_num]->vc_mode != KD_TEXT)
		return;

	accel_putcs(vc, info, s, count, real_y(p, ypos), xpos);
}

static void fbcon_putc(struct vc_data *vc, int c, int ypos, int xpos)
{
	fbcon_putcs(vc, (const unsigned short *) &c, 1, ypos, xpos);
}

static void fbcon_cursor(struct vc_data *vc, int mode)
{
	struct fb_cursor cursor;
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	unsigned short charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	struct display *p = &fb_display[vc->vc_num];
	int w = (vc->vc_font.width + 7) >> 3, c;
	int y = real_y(p, vc->vc_y), fg, bg;
	int attribute;
	u8 *src;

	if (mode & CM_SOFTBACK) {
		mode &= ~CM_SOFTBACK;
		if (softback_lines) {
			if (y + softback_lines >= vc->vc_rows)
				mode = CM_ERASE;
			else
				y += softback_lines;
		}
	} else if (softback_lines)
		fbcon_set_origin(vc);

 	c = scr_readw((u16 *) vc->vc_pos);
	attribute = get_attribute(info, c);
	src = vc->vc_font.data + ((c & charmask) * (w * vc->vc_font.height));
	if (attribute) {
		u8 *dst;

		dst = kmalloc(w * vc->vc_font.height, GFP_ATOMIC);
		if (!dst)
			return;
		if (info->cursor.data)
			kfree(info->cursor.data);
		info->cursor.data = dst;
		update_attr(dst, src, attribute, vc);
		src = dst;
	}

	cursor.image.data = src;
	cursor.set = FB_CUR_SETCUR;
	cursor.image.depth = 1;
	
	switch (mode) {
	case CM_ERASE:
		if (info->cursor.rop == ROP_XOR) {
			info->cursor.enable = 0;
			info->cursor.rop = ROP_COPY;
			info->fbops->fb_cursor(info, &cursor);
		}	
		break;
	case CM_MOVE:
	case CM_DRAW:
		info->cursor.enable = 1;
		fg = get_color(vc, info, c, 1);
		bg = get_color(vc, info, c, 0);

		if (info->cursor.image.fg_color != fg ||
		    info->cursor.image.bg_color != bg) {
			cursor.image.fg_color = fg;
			cursor.image.bg_color = bg;
			cursor.set |= FB_CUR_SETCMAP;
		}
		
		if ((info->cursor.image.dx != (vc->vc_font.width * vc->vc_x)) ||
		    (info->cursor.image.dy != (vc->vc_font.height * y))) {
			cursor.image.dx = vc->vc_font.width * vc->vc_x;
			cursor.image.dy = vc->vc_font.height * y;
			cursor.set |= FB_CUR_SETPOS;
		}

		if (info->cursor.image.height != vc->vc_font.height ||
		    info->cursor.image.width != vc->vc_font.width) {
			cursor.image.height = vc->vc_font.height;
			cursor.image.width = vc->vc_font.width;
			cursor.set |= FB_CUR_SETSIZE;
		}

		if (info->cursor.hot.x || info->cursor.hot.y) {
			cursor.hot.x = cursor.hot.y = 0;
			cursor.set |= FB_CUR_SETHOT;
		}

		if ((cursor.set & FB_CUR_SETSIZE) ||
		    ((vc->vc_cursor_type & 0x0f) != p->cursor_shape)
		    || info->cursor.mask == NULL) {
			char *mask = kmalloc(w*vc->vc_font.height, GFP_ATOMIC);
			int cur_height, size, i = 0;
			u8 msk = 0xff;

			if (!mask)
				return;
		
			if (info->cursor.mask)
				kfree(info->cursor.mask);
			info->cursor.mask = mask;
			p->cursor_shape = vc->vc_cursor_type & 0x0f;
			cursor.set |= FB_CUR_SETSHAPE;

			switch (vc->vc_cursor_type & 0x0f) {
			case CUR_NONE:
				cur_height = 0;
				break;
			case CUR_UNDERLINE:
				cur_height = (vc->vc_font.height < 10) ? 1 : 2;
				break;
			case CUR_LOWER_THIRD:
				cur_height = vc->vc_font.height/3;
				break;
			case CUR_LOWER_HALF:
				cur_height = vc->vc_font.height >> 1;
				break;
			case CUR_TWO_THIRDS:
				cur_height = (vc->vc_font.height << 1)/3;
				break;
			case CUR_BLOCK:
			default:
				cur_height = vc->vc_font.height;
				break;
			}
			size = (vc->vc_font.height - cur_height) * w;
			while (size--)
				mask[i++] = ~msk;
			size = cur_height * w;
			while (size--)
				mask[i++] = msk;
		}
        	info->cursor.rop = ROP_XOR;
		info->fbops->fb_cursor(info, &cursor);
		vbl_cursor_cnt = CURSOR_DRAW_DELAY;
		break;
	}
}

static int scrollback_phys_max = 0;
static int scrollback_max = 0;
static int scrollback_current = 0;

int update_var(int con, struct fb_info *info)
{
	if (con == info->currcon) 
		return fb_pan_display(info, &info->var);
	return 0;
}

static void fbcon_set_disp(struct fb_info *info, struct vc_data *vc)
{
	struct display *p = &fb_display[vc->vc_num], *t;
	struct vc_data **default_mode = vc->vc_display_fg;
	int display_fg = (*default_mode)->vc_num;
	int rows, cols, charcnt = 256;

	info->var.xoffset = info->var.yoffset = p->yscroll = 0;
	if (var_to_display(p, &info->var, info))
		return;
	t = &fb_display[display_fg];
	if (!vc->vc_font.data) {
		vc->vc_font.data = p->fontdata = t->fontdata;
		vc->vc_font.width = (*default_mode)->vc_font.width;
		vc->vc_font.height = (*default_mode)->vc_font.height;
		p->userfont = t->userfont;
		if (p->userfont)
			REFCOUNT(p->fontdata)++;
		con_copy_unimap(vc->vc_num, display_fg);
	}
	if (p->userfont)
		charcnt = FNTCHARCNT(p->fontdata);

	vc->vc_can_do_color = (fb_get_color_depth(info) != 1);
	vc->vc_complement_mask = vc->vc_can_do_color ? 0x7700 : 0x0800;
	if (charcnt == 256) {
		vc->vc_hi_font_mask = 0;
	} else {
		vc->vc_hi_font_mask = 0x100;
		if (vc->vc_can_do_color)
			vc->vc_complement_mask <<= 1;
	}
	cols = info->var.xres / vc->vc_font.width;
	rows = info->var.yres / vc->vc_font.height;
	vc_resize(vc->vc_num, cols, rows);
	if (CON_IS_VISIBLE(vc)) {
		update_screen(vc->vc_num);
		if (softback_buf) {
			int l = fbcon_softback_size / vc->vc_size_row;

			if (l > 5)
				softback_end = softback_buf + l *
					vc->vc_size_row;
			else {
				/* Smaller scrollback makes no sense, and 0
				   would screw the operation totally */
				softback_top = 0;
			}
		}
	}
	switch_screen(fg_console);
}

static __inline__ void ywrap_up(struct vc_data *vc, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	
	p->yscroll += count;
	if (p->yscroll >= p->vrows)	/* Deal with wrap */
		p->yscroll -= p->vrows;
	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode |= FB_VMODE_YWRAP;
	update_var(vc->vc_num, info);
	scrollback_max += count;
	if (scrollback_max > scrollback_phys_max)
		scrollback_max = scrollback_phys_max;
	scrollback_current = 0;
}

static __inline__ void ywrap_down(struct vc_data *vc, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	
	p->yscroll -= count;
	if (p->yscroll < 0)	/* Deal with wrap */
		p->yscroll += p->vrows;
	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode |= FB_VMODE_YWRAP;
	update_var(vc->vc_num, info);
	scrollback_max -= count;
	if (scrollback_max < 0)
		scrollback_max = 0;
	scrollback_current = 0;
}

static __inline__ void ypan_up(struct vc_data *vc, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	
	p->yscroll += count;
	if (p->yscroll > p->vrows - vc->vc_rows) {
		accel_bmove(vc, info, p->vrows - vc->vc_rows, 
			 	0, 0, 0, vc->vc_rows, vc->vc_cols);
		p->yscroll -= p->vrows - vc->vc_rows;
	}
	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode &= ~FB_VMODE_YWRAP;
	update_var(vc->vc_num, info);
	accel_clear_margins(vc, info, 1);
	scrollback_max += count;
	if (scrollback_max > scrollback_phys_max)
		scrollback_max = scrollback_phys_max;
	scrollback_current = 0;
}

static __inline__ void ypan_up_redraw(struct vc_data *vc, int t, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	int redraw = 0;

	p->yscroll += count;
	if (p->yscroll > p->vrows - vc->vc_rows) {
		p->yscroll -= p->vrows - vc->vc_rows;
		redraw = 1;
	}

	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode &= ~FB_VMODE_YWRAP;
	if (redraw)
		fbcon_redraw_move(vc, p, t + count, vc->vc_rows - count, t);
	update_var(vc->vc_num, info);
	accel_clear_margins(vc, info, 1);
	scrollback_max += count;
	if (scrollback_max > scrollback_phys_max)
		scrollback_max = scrollback_phys_max;
	scrollback_current = 0;
}

static __inline__ void ypan_down(struct vc_data *vc, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	
	p->yscroll -= count;
	if (p->yscroll < 0) {
		accel_bmove(vc, info, 0, 0, p->vrows - vc->vc_rows,
			 	0, vc->vc_rows, vc->vc_cols);
		p->yscroll += p->vrows - vc->vc_rows;
	}
	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode &= ~FB_VMODE_YWRAP;
	update_var(vc->vc_num, info);
	accel_clear_margins(vc, info, 1);
	scrollback_max -= count;
	if (scrollback_max < 0)
		scrollback_max = 0;
	scrollback_current = 0;
}

static __inline__ void ypan_down_redraw(struct vc_data *vc, int t, int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	int redraw = 0;

	p->yscroll -= count;
	if (p->yscroll < 0) {
		p->yscroll += p->vrows - vc->vc_rows;
		redraw = 1;
	}
	info->var.xoffset = 0;
	info->var.yoffset = p->yscroll * vc->vc_font.height;
	info->var.vmode &= ~FB_VMODE_YWRAP;
	if (redraw)
		fbcon_redraw_move(vc, p, t, vc->vc_rows - count, t + count);
	update_var(vc->vc_num, info);
	accel_clear_margins(vc, info, 1);
	scrollback_max -= count;
	if (scrollback_max < 0)
		scrollback_max = 0;
	scrollback_current = 0;
}

static void fbcon_redraw_softback(struct vc_data *vc, struct display *p,
				  long delta)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	int count = vc->vc_rows;
	unsigned short *d, *s;
	unsigned long n;
	int line = 0;

	d = (u16 *) softback_curr;
	if (d == (u16 *) softback_in)
		d = (u16 *) vc->vc_origin;
	n = softback_curr + delta * vc->vc_size_row;
	softback_lines -= delta;
	if (delta < 0) {
		if (softback_curr < softback_top && n < softback_buf) {
			n += softback_end - softback_buf;
			if (n < softback_top) {
				softback_lines -=
				    (softback_top - n) / vc->vc_size_row;
				n = softback_top;
			}
		} else if (softback_curr >= softback_top
			   && n < softback_top) {
			softback_lines -=
			    (softback_top - n) / vc->vc_size_row;
			n = softback_top;
		}
	} else {
		if (softback_curr > softback_in && n >= softback_end) {
			n += softback_buf - softback_end;
			if (n > softback_in) {
				n = softback_in;
				softback_lines = 0;
			}
		} else if (softback_curr <= softback_in && n > softback_in) {
			n = softback_in;
			softback_lines = 0;
		}
	}
	if (n == softback_curr)
		return;
	softback_curr = n;
	s = (u16 *) softback_curr;
	if (s == (u16 *) softback_in)
		s = (u16 *) vc->vc_origin;
	while (count--) {
		unsigned short *start;
		unsigned short *le;
		unsigned short c;
		int x = 0;
		unsigned short attr = 1;

		start = s;
		le = advance_row(s, 1);
		do {
			c = scr_readw(s);
			if (attr != (c & 0xff00)) {
				attr = c & 0xff00;
				if (s > start) {
					accel_putcs(vc, info, start, s - start,
						    real_y(p, line), x);
					x += s - start;
					start = s;
				}
			}
			if (c == scr_readw(d)) {
				if (s > start) {
					accel_putcs(vc, info, start, s - start,
						    real_y(p, line), x);
					x += s - start + 1;
					start = s + 1;
				} else {
					x++;
					start++;
				}
			}
			s++;
			d++;
		} while (s < le);
		if (s > start)
			accel_putcs(vc, info, start, s - start,
				    real_y(p, line), x);
		line++;
		if (d == (u16 *) softback_end)
			d = (u16 *) softback_buf;
		if (d == (u16 *) softback_in)
			d = (u16 *) vc->vc_origin;
		if (s == (u16 *) softback_end)
			s = (u16 *) softback_buf;
		if (s == (u16 *) softback_in)
			s = (u16 *) vc->vc_origin;
	}
}

static void fbcon_redraw_move(struct vc_data *vc, struct display *p,
			      int line, int count, int dy)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	unsigned short *s = (unsigned short *)
		(vc->vc_origin + vc->vc_size_row * line);

	while (count--) {
		unsigned short *start = s;
		unsigned short *le = advance_row(s, 1);
		unsigned short c;
		int x = 0;
		unsigned short attr = 1;

		do {
			c = scr_readw(s);
			if (attr != (c & 0xff00)) {
				attr = c & 0xff00;
				if (s > start) {
					accel_putcs(vc, info, start, s - start,
						    real_y(p, dy), x);
					x += s - start;
					start = s;
				}
			}
			console_conditional_schedule();
			s++;
		} while (s < le);
		if (s > start)
			accel_putcs(vc, info, start, s - start,
				    real_y(p, dy), x);
		console_conditional_schedule();
		dy++;
	}
}

static void fbcon_redraw(struct vc_data *vc, struct display *p,
			 int line, int count, int offset)
{
	unsigned short *d = (unsigned short *)
	    (vc->vc_origin + vc->vc_size_row * line);
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	unsigned short *s = d + offset;

	while (count--) {
		unsigned short *start = s;
		unsigned short *le = advance_row(s, 1);
		unsigned short c;
		int x = 0;
		unsigned short attr = 1;

		do {
			c = scr_readw(s);
			if (attr != (c & 0xff00)) {
				attr = c & 0xff00;
				if (s > start) {
					accel_putcs(vc, info, start, s - start,
						    real_y(p, line), x);
					x += s - start;
					start = s;
				}
			}
			if (c == scr_readw(d)) {
				if (s > start) {
					accel_putcs(vc, info, start, s - start,
						    real_y(p, line), x);
					x += s - start + 1;
					start = s + 1;
				} else {
					x++;
					start++;
				}
			}
			scr_writew(c, d);
			console_conditional_schedule();
			s++;
			d++;
		} while (s < le);
		if (s > start)
			accel_putcs(vc, info, start, s - start,
				    real_y(p, line), x);
		console_conditional_schedule();
		if (offset > 0)
			line++;
		else {
			line--;
			/* NOTE: We subtract two lines from these pointers */
			s -= vc->vc_size_row;
			d -= vc->vc_size_row;
		}
	}
}

static inline void fbcon_softback_note(struct vc_data *vc, int t,
				       int count)
{
	unsigned short *p;

	if (vc->vc_num != fg_console)
		return;
	p = (unsigned short *) (vc->vc_origin + t * vc->vc_size_row);

	while (count) {
		scr_memcpyw((u16 *) softback_in, p, vc->vc_size_row);
		count--;
		p = advance_row(p, 1);
		softback_in += vc->vc_size_row;
		if (softback_in == softback_end)
			softback_in = softback_buf;
		if (softback_in == softback_top) {
			softback_top += vc->vc_size_row;
			if (softback_top == softback_end)
				softback_top = softback_buf;
		}
	}
	softback_curr = softback_in;
}

static int fbcon_scroll(struct vc_data *vc, int t, int b, int dir,
			int count)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	int scroll_partial = info->flags & FBINFO_PARTIAL_PAN_OK;

	if (!info->fbops->fb_blank && console_blanked)
		return 0;

	if (!count || vt_cons[vc->vc_num]->vc_mode != KD_TEXT)
		return 0;

	fbcon_cursor(vc, CM_ERASE);

	/*
	 * ++Geert: Only use ywrap/ypan if the console is in text mode
	 * ++Andrew: Only use ypan on hardware text mode when scrolling the
	 *           whole screen (prevents flicker).
	 */

	switch (dir) {
	case SM_UP:
		if (count > vc->vc_rows)	/* Maximum realistic size */
			count = vc->vc_rows;
		if (softback_top)
			fbcon_softback_note(vc, t, count);
		if (logo_shown >= 0)
			goto redraw_up;
		switch (p->scrollmode) {
		case SCROLL_MOVE:
			accel_bmove(vc, info, t + count, 0, t, 0,
					 b - t - count, vc->vc_cols);
			accel_clear(vc, info, b - count, 0, count,
					 vc->vc_cols);
			break;

		case SCROLL_WRAP_MOVE:
			if (b - t - count > 3 * vc->vc_rows >> 2) {
				if (t > 0)
					fbcon_bmove(vc, 0, 0, count, 0, t,
						    vc->vc_cols);
				ywrap_up(vc, count);
				if (vc->vc_rows - b > 0)
					fbcon_bmove(vc, b - count, 0, b, 0,
						    vc->vc_rows - b,
						    vc->vc_cols);
			} else if (info->flags & FBINFO_READS_FAST)
				fbcon_bmove(vc, t + count, 0, t, 0,
					    b - t - count, vc->vc_cols);
			else
				goto redraw_up;
			fbcon_clear(vc, b - count, 0, count, vc->vc_cols);
			break;

		case SCROLL_PAN_REDRAW:
			if ((p->yscroll + count <=
			     2 * (p->vrows - vc->vc_rows))
			    && ((!scroll_partial && (b - t == vc->vc_rows))
				|| (scroll_partial
				    && (b - t - count >
					3 * vc->vc_rows >> 2)))) {
				if (t > 0)
					fbcon_redraw_move(vc, p, 0, t, count);
				ypan_up_redraw(vc, t, count);
				if (vc->vc_rows - b > 0)
					fbcon_redraw_move(vc, p, b - count,
							  vc->vc_rows - b, b);
			} else
				fbcon_redraw_move(vc, p, t + count, b - t - count, t);
			fbcon_clear(vc, b - count, 0, count, vc->vc_cols);
			break;

		case SCROLL_PAN_MOVE:
			if ((p->yscroll + count <=
			     2 * (p->vrows - vc->vc_rows))
			    && ((!scroll_partial && (b - t == vc->vc_rows))
				|| (scroll_partial
				    && (b - t - count >
					3 * vc->vc_rows >> 2)))) {
				if (t > 0)
					fbcon_bmove(vc, 0, 0, count, 0, t,
						    vc->vc_cols);
				ypan_up(vc, count);
				if (vc->vc_rows - b > 0)
					fbcon_bmove(vc, b - count, 0, b, 0,
						    vc->vc_rows - b,
						    vc->vc_cols);
			} else if (info->flags & FBINFO_READS_FAST)
				fbcon_bmove(vc, t + count, 0, t, 0,
					    b - t - count, vc->vc_cols);
			else
				goto redraw_up;
			fbcon_clear(vc, b - count, 0, count, vc->vc_cols);
			break;

		case SCROLL_REDRAW:
		      redraw_up:
			fbcon_redraw(vc, p, t, b - t - count,
				     count * vc->vc_cols);
			accel_clear(vc, info, real_y(p, b - count), 0,
					 count, vc->vc_cols);
			scr_memsetw((unsigned short *) (vc->vc_origin +
							vc->vc_size_row *
							(b - count)),
				    vc->vc_video_erase_char,
				    vc->vc_size_row * count);
			return 1;
		}
		break;

	case SM_DOWN:
		if (count > vc->vc_rows)	/* Maximum realistic size */
			count = vc->vc_rows;
		switch (p->scrollmode) {
		case SCROLL_MOVE:
			accel_bmove(vc, info, t, 0, t + count, 0,
					 b - t - count, vc->vc_cols);
			accel_clear(vc, info, t, 0, count, vc->vc_cols);
			break;

		case SCROLL_WRAP_MOVE:
			if (b - t - count > 3 * vc->vc_rows >> 2) {
				if (vc->vc_rows - b > 0)
					fbcon_bmove(vc, b, 0, b - count, 0,
						    vc->vc_rows - b,
						    vc->vc_cols);
				ywrap_down(vc, count);
				if (t > 0)
					fbcon_bmove(vc, count, 0, 0, 0, t,
						    vc->vc_cols);
			} else if (info->flags & FBINFO_READS_FAST)
				fbcon_bmove(vc, t, 0, t + count, 0,
					    b - t - count, vc->vc_cols);
			else
				goto redraw_down;
			fbcon_clear(vc, t, 0, count, vc->vc_cols);
			break;

		case SCROLL_PAN_MOVE:
			if ((count - p->yscroll <= p->vrows - vc->vc_rows)
			    && ((!scroll_partial && (b - t == vc->vc_rows))
				|| (scroll_partial
				    && (b - t - count >
					3 * vc->vc_rows >> 2)))) {
				if (vc->vc_rows - b > 0)
					fbcon_bmove(vc, b, 0, b - count, 0,
						    vc->vc_rows - b,
						    vc->vc_cols);
				ypan_down(vc, count);
				if (t > 0)
					fbcon_bmove(vc, count, 0, 0, 0, t,
						    vc->vc_cols);
			} else if (info->flags & FBINFO_READS_FAST)
				fbcon_bmove(vc, t, 0, t + count, 0,
					    b - t - count, vc->vc_cols);
			else
				goto redraw_down;
			fbcon_clear(vc, t, 0, count, vc->vc_cols);
			break;

		case SCROLL_PAN_REDRAW:
			if ((count - p->yscroll <= p->vrows - vc->vc_rows)
			    && ((!scroll_partial && (b - t == vc->vc_rows))
				|| (scroll_partial
				    && (b - t - count >
					3 * vc->vc_rows >> 2)))) {
				if (vc->vc_rows - b > 0)
					fbcon_redraw_move(vc, p, b, vc->vc_rows - b,
							  b - count);
				ypan_down_redraw(vc, t, count);
				if (t > 0)
					fbcon_redraw_move(vc, p, count, t, 0);
			} else
				fbcon_redraw_move(vc, p, t, b - t - count, t + count);
			fbcon_clear(vc, t, 0, count, vc->vc_cols);
			break;

		case SCROLL_REDRAW:
		      redraw_down:
			fbcon_redraw(vc, p, b - 1, b - t - count,
				     -count * vc->vc_cols);
			accel_clear(vc, info, real_y(p, t), 0, count,
					 vc->vc_cols);
			scr_memsetw((unsigned short *) (vc->vc_origin +
							vc->vc_size_row *
							t),
				    vc->vc_video_erase_char,
				    vc->vc_size_row * count);
			return 1;
		}
	}
	return 0;
}


static void fbcon_bmove(struct vc_data *vc, int sy, int sx, int dy, int dx,
			int height, int width)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	
	if (!info->fbops->fb_blank && console_blanked)
		return;

	if (!width || !height)
		return;

	/*  Split blits that cross physical y_wrap case.
	 *  Pathological case involves 4 blits, better to use recursive
	 *  code rather than unrolled case
	 *
	 *  Recursive invocations don't need to erase the cursor over and
	 *  over again, so we use fbcon_bmove_rec()
	 */
	fbcon_bmove_rec(vc, p, sy, sx, dy, dx, height, width,
			p->vrows - p->yscroll);
}

static void fbcon_bmove_rec(struct vc_data *vc, struct display *p, int sy, int sx, 
			    int dy, int dx, int height, int width, u_int y_break)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	u_int b;

	if (sy < y_break && sy + height > y_break) {
		b = y_break - sy;
		if (dy < sy) {	/* Avoid trashing self */
			fbcon_bmove_rec(vc, p, sy, sx, dy, dx, b, width,
					y_break);
			fbcon_bmove_rec(vc, p, sy + b, sx, dy + b, dx,
					height - b, width, y_break);
		} else {
			fbcon_bmove_rec(vc, p, sy + b, sx, dy + b, dx,
					height - b, width, y_break);
			fbcon_bmove_rec(vc, p, sy, sx, dy, dx, b, width,
					y_break);
		}
		return;
	}

	if (dy < y_break && dy + height > y_break) {
		b = y_break - dy;
		if (dy < sy) {	/* Avoid trashing self */
			fbcon_bmove_rec(vc, p, sy, sx, dy, dx, b, width,
					y_break);
			fbcon_bmove_rec(vc, p, sy + b, sx, dy + b, dx,
					height - b, width, y_break);
		} else {
			fbcon_bmove_rec(vc, p, sy + b, sx, dy + b, dx,
					height - b, width, y_break);
			fbcon_bmove_rec(vc, p, sy, sx, dy, dx, b, width,
					y_break);
		}
		return;
	}
	accel_bmove(vc, info, real_y(p, sy), sx, real_y(p, dy), dx,
			height, width);
}

static __inline__ void updatescrollmode(struct display *p, struct fb_info *info,
					struct vc_data *vc)
{
	int fh = vc->vc_font.height;
	int cap = info->flags;
	int good_pan = (cap & FBINFO_HWACCEL_YPAN)
		 && divides(info->fix.ypanstep, vc->vc_font.height)
		 && info->var.yres_virtual > info->var.yres;
	int good_wrap = (cap & FBINFO_HWACCEL_YWRAP)
		 && divides(info->fix.ywrapstep, vc->vc_font.height)
		 && divides(vc->vc_font.height, info->var.yres_virtual);
	int reading_fast = cap & FBINFO_READS_FAST;
	int fast_copyarea = (cap & FBINFO_HWACCEL_COPYAREA) && !(cap & FBINFO_HWACCEL_DISABLED);
	int fast_imageblit = (cap & FBINFO_HWACCEL_IMAGEBLIT) && !(cap & FBINFO_HWACCEL_DISABLED);

	p->vrows = info->var.yres_virtual/fh;
	if (info->var.yres > (fh * (vc->vc_rows + 1)))
		p->vrows -= (info->var.yres - (fh * vc->vc_rows)) / fh;
	if ((info->var.yres % fh) && (info->var.yres_virtual % fh <
				      info->var.yres % fh))
		p->vrows--;

	if (good_wrap || good_pan) {
		if (reading_fast || fast_copyarea)
			p->scrollmode = good_wrap ? SCROLL_WRAP_MOVE : SCROLL_PAN_MOVE;
		else
			p->scrollmode = good_wrap ? SCROLL_REDRAW :
				SCROLL_PAN_REDRAW;
	} else {
		if (reading_fast || (fast_copyarea && !fast_imageblit))
			p->scrollmode = SCROLL_MOVE;
		else
			p->scrollmode = SCROLL_REDRAW;
	}
}

static int fbcon_resize(struct vc_data *vc, unsigned int width, 
			unsigned int height)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	struct fb_var_screeninfo var = info->var;
	int x_diff, y_diff;
	int fw = vc->vc_font.width;
	int fh = vc->vc_font.height;

	var.xres = width * fw;
	var.yres = height * fh;
	x_diff = info->var.xres - var.xres;
	y_diff = info->var.yres - var.yres;
	if (x_diff < 0 || x_diff > fw || (y_diff < 0 || y_diff > fh)) {
		struct fb_videomode *mode;

		DPRINTK("attempting resize %ix%i\n", var.xres, var.yres);
		mode = fb_find_best_mode(&var, &info->modelist);
		if (mode == NULL)
			return -EINVAL;
		fb_videomode_to_var(&var, mode);
		if (width > var.xres/fw || height > var.yres/fh)
			return -EINVAL;
		/*
		 * The following can probably have any value... Do we need to
		 * set all of them?
		 */
		var.bits_per_pixel = p->bits_per_pixel;
		var.xres_virtual = p->xres_virtual;
		var.yres_virtual = p->yres_virtual;
		var.accel_flags = p->accel_flags;
		var.width = p->width;
		var.height = p->height;
		var.red = p->red;
		var.green = p->green;
		var.blue = p->blue;
		var.transp = p->transp;
		var.nonstd = p->nonstd;

		DPRINTK("resize now %ix%i\n", var.xres, var.yres);
		if (CON_IS_VISIBLE(vc)) {
			var.activate = FB_ACTIVATE_NOW |
				FB_ACTIVATE_FORCE;
			fb_set_var(info, &var);
			info->flags &= ~FBINFO_MISC_MODESWITCH;
		}
		var_to_display(p, &info->var, info);
	}
	updatescrollmode(p, info, vc);
	return 0;
}

static int fbcon_switch(struct vc_data *vc)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	struct fb_var_screeninfo var;
	int i;

	if (softback_top) {
		int l = fbcon_softback_size / vc->vc_size_row;
		if (softback_lines)
			fbcon_set_origin(vc);
		softback_top = softback_curr = softback_in = softback_buf;
		softback_lines = 0;

		if (l > 5)
			softback_end = softback_buf + l * vc->vc_size_row;
		else {
			/* Smaller scrollback makes no sense, and 0 would screw
			   the operation totally */
			softback_top = 0;
		}
	}
	if (logo_shown >= 0) {
		struct vc_data *conp2 = vc_cons[logo_shown].d;

		if (conp2->vc_top == logo_lines
		    && conp2->vc_bottom == conp2->vc_rows)
			conp2->vc_top = 0;
		logo_shown = -1;
	}

#ifdef CONFIG_FB_UI
        fbui_switch (info, vc->vc_num);
#endif

	/*
	 * FIXME: If we have multiple fbdev's loaded, we need to
	 * update all info->currcon.  Perhaps, we can place this
	 * in a centralized structure, but this might break some
	 * drivers.
	 *
	 * info->currcon = vc->vc_num;
	 */
	for (i = 0; i < FB_MAX; i++) {
		if (registered_fb[i] != NULL)
			registered_fb[i]->currcon = vc->vc_num;
	}

	memset(&var, 0, sizeof(struct fb_var_screeninfo));
	fb_videomode_to_var(&var, p->mode);
	display_to_var(&var, p);
	var.activate = FB_ACTIVATE_NOW;

	/*
	 * make sure we don't unnecessarily trip the memcmp()
	 * in fb_set_var()
	 */
	info->var.activate = var.activate;
	info->var.yoffset = info->var.xoffset = p->yscroll = 0;
	fb_set_var(info, &var);

	if (info->flags & FBINFO_MISC_MODESWITCH) {
		if (info->fbops->fb_set_par)
			info->fbops->fb_set_par(info);
		info->flags &= ~FBINFO_MISC_MODESWITCH;
	}

	vc->vc_can_do_color = (fb_get_color_depth(info) != 1);
	vc->vc_complement_mask = vc->vc_can_do_color ? 0x7700 : 0x0800;
	updatescrollmode(p, info, vc);

	switch (p->scrollmode) {
	case SCROLL_WRAP_MOVE:
		scrollback_phys_max = p->vrows - vc->vc_rows;
		break;
	case SCROLL_PAN_MOVE:
	case SCROLL_PAN_REDRAW:
		scrollback_phys_max = p->vrows - 2 * vc->vc_rows;
		if (scrollback_phys_max < 0)
			scrollback_phys_max = 0;
		break;
	default:
		scrollback_phys_max = 0;
		break;
	}
	scrollback_max = 0;
	scrollback_current = 0;

	update_var(vc->vc_num, info);
	fbcon_set_palette(vc, color_table); 	

	if (vt_cons[vc->vc_num]->vc_mode == KD_TEXT)
		accel_clear_margins(vc, info, 0);
	if (logo_shown == -2) {
		logo_shown = fg_console;
		/* This is protected above by initmem_freed */
		fb_show_logo(info);
		update_region(fg_console,
			      vc->vc_origin + vc->vc_size_row * vc->vc_top,
			      vc->vc_size_row * (vc->vc_bottom -
						 vc->vc_top) / 2);
		return 0;
	}
	return 1;
}

static int fbcon_blank(struct vc_data *vc, int blank, int mode_switch)
{
	unsigned short charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];

	if (mode_switch) {
		struct fb_var_screeninfo var = info->var;

/*
 * HACK ALERT: Some hardware will require reinitializion at this stage,
 *             others will require it to be done as late as possible.
 *             For now, we differentiate this with the
 *             FBINFO_MISC_MODESWITCHLATE bitflag.  Worst case will be
 *             hardware that requires it here and another one later.
 *             A definitive solution may require fixing X or the VT
 *             system.
 */
		if (info->flags & FBINFO_MISC_MODESWITCHLATE)
			info->flags |= FBINFO_MISC_MODESWITCH;

		if (blank) {
			fbcon_cursor(vc, CM_ERASE);
			return 0;
		}

		if (!(info->flags & FBINFO_MISC_MODESWITCHLATE)) {
			var.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
			fb_set_var(info, &var);
		}
	}

	fbcon_cursor(vc, blank ? CM_ERASE : CM_DRAW);

	if (!info->fbops->fb_blank) {
		if (blank) {
			unsigned short oldc;
			u_int height;
			u_int y_break;

			oldc = vc->vc_video_erase_char;
			vc->vc_video_erase_char &= charmask;
			height = vc->vc_rows;
			y_break = p->vrows - p->yscroll;
			if (height > y_break) {
				accel_clear(vc, info, real_y(p, 0),
					    0, y_break, vc->vc_cols);
				accel_clear(vc, info, real_y(p, y_break),
					    0, height - y_break, 
					    vc->vc_cols);
			} else
				accel_clear(vc, info, real_y(p, 0),
					    0, height, vc->vc_cols);
			vc->vc_video_erase_char = oldc;
		} else
			update_screen(vc->vc_num);
		return 0;
	} else
		return fb_blank(info, blank);
}

static void fbcon_free_font(struct display *p)
{
	if (p->userfont && p->fontdata && (--REFCOUNT(p->fontdata) == 0))
		kfree(p->fontdata - FONT_EXTRA_WORDS * sizeof(int));
	p->fontdata = NULL;
	p->userfont = 0;
}

static int fbcon_get_font(struct vc_data *vc, struct console_font *font)
{
	u8 *fontdata = vc->vc_font.data;
	u8 *data = font->data;
	int i, j;

	font->width = vc->vc_font.width;
	font->height = vc->vc_font.height;
	font->charcount = vc->vc_hi_font_mask ? 512 : 256;
	if (!font->data)
		return 0;

	if (font->width <= 8) {
		j = vc->vc_font.height;
		for (i = 0; i < font->charcount; i++) {
			memcpy(data, fontdata, j);
			memset(data + j, 0, 32 - j);
			data += 32;
			fontdata += j;
		}
	} else if (font->width <= 16) {
		j = vc->vc_font.height * 2;
		for (i = 0; i < font->charcount; i++) {
			memcpy(data, fontdata, j);
			memset(data + j, 0, 64 - j);
			data += 64;
			fontdata += j;
		}
	} else if (font->width <= 24) {
		for (i = 0; i < font->charcount; i++) {
			for (j = 0; j < vc->vc_font.height; j++) {
				*data++ = fontdata[0];
				*data++ = fontdata[1];
				*data++ = fontdata[2];
				fontdata += sizeof(u32);
			}
			memset(data, 0, 3 * (32 - j));
			data += 3 * (32 - j);
		}
	} else {
		j = vc->vc_font.height * 4;
		for (i = 0; i < font->charcount; i++) {
			memcpy(data, fontdata, j);
			memset(data + j, 0, 128 - j);
			data += 128;
			fontdata += j;
		}
	}
	return 0;
}

static int fbcon_do_set_font(struct vc_data *vc, int w, int h,
			     u8 * data, int userfont)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct display *p = &fb_display[vc->vc_num];
	int resize;
	int cnt;
	char *old_data = NULL;

	if (CON_IS_VISIBLE(vc) && softback_lines)
		fbcon_set_origin(vc);

	resize = (w != vc->vc_font.width) || (h != vc->vc_font.height);
	if (p->userfont)
		old_data = vc->vc_font.data;
	if (userfont)
		cnt = FNTCHARCNT(data);
	else
		cnt = 256;
	vc->vc_font.data = p->fontdata = data;
	if ((p->userfont = userfont))
		REFCOUNT(data)++;
	vc->vc_font.width = w;
	vc->vc_font.height = h;
	if (vc->vc_hi_font_mask && cnt == 256) {
		vc->vc_hi_font_mask = 0;
		if (vc->vc_can_do_color) {
			vc->vc_complement_mask >>= 1;
			vc->vc_s_complement_mask >>= 1;
		}
			
		/* ++Edmund: reorder the attribute bits */
		if (vc->vc_can_do_color) {
			unsigned short *cp =
			    (unsigned short *) vc->vc_origin;
			int count = vc->vc_screenbuf_size / 2;
			unsigned short c;
			for (; count > 0; count--, cp++) {
				c = scr_readw(cp);
				scr_writew(((c & 0xfe00) >> 1) |
					   (c & 0xff), cp);
			}
			c = vc->vc_video_erase_char;
			vc->vc_video_erase_char =
			    ((c & 0xfe00) >> 1) | (c & 0xff);
			vc->vc_attr >>= 1;
		}
	} else if (!vc->vc_hi_font_mask && cnt == 512) {
		vc->vc_hi_font_mask = 0x100;
		if (vc->vc_can_do_color) {
			vc->vc_complement_mask <<= 1;
			vc->vc_s_complement_mask <<= 1;
		}
			
		/* ++Edmund: reorder the attribute bits */
		{
			unsigned short *cp =
			    (unsigned short *) vc->vc_origin;
			int count = vc->vc_screenbuf_size / 2;
			unsigned short c;
			for (; count > 0; count--, cp++) {
				unsigned short newc;
				c = scr_readw(cp);
				if (vc->vc_can_do_color)
					newc =
					    ((c & 0xff00) << 1) | (c &
								   0xff);
				else
					newc = c & ~0x100;
				scr_writew(newc, cp);
			}
			c = vc->vc_video_erase_char;
			if (vc->vc_can_do_color) {
				vc->vc_video_erase_char =
				    ((c & 0xff00) << 1) | (c & 0xff);
				vc->vc_attr <<= 1;
			} else
				vc->vc_video_erase_char = c & ~0x100;
		}

	}

	if (resize) {
		/* reset wrap/pan */
		info->var.xoffset = info->var.yoffset = p->yscroll = 0;
		vc_resize(vc->vc_num, info->var.xres / w, info->var.yres / h);
		if (CON_IS_VISIBLE(vc) && softback_buf) {
			int l = fbcon_softback_size / vc->vc_size_row;
			if (l > 5)
				softback_end =
				    softback_buf + l * vc->vc_size_row;
			else {
				/* Smaller scrollback makes no sense, and 0 would screw
				   the operation totally */
				softback_top = 0;
			}
		}
	} else if (CON_IS_VISIBLE(vc)
		   && vt_cons[vc->vc_num]->vc_mode == KD_TEXT) {
		accel_clear_margins(vc, info, 0);
		update_screen(vc->vc_num);
	}

	if (old_data && (--REFCOUNT(old_data) == 0))
		kfree(old_data - FONT_EXTRA_WORDS * sizeof(int));
	return 0;
}

static int fbcon_copy_font(struct vc_data *vc, int con)
{
	struct display *od = &fb_display[con];
	struct console_font *f = &vc->vc_font;

	if (od->fontdata == f->data)
		return 0;	/* already the same font... */
	return fbcon_do_set_font(vc, f->width, f->height, od->fontdata, od->userfont);
}

/*
 *  User asked to set font; we are guaranteed that
 *	a) width and height are in range 1..32
 *	b) charcount does not exceed 512
 */

static int fbcon_set_font(struct vc_data *vc, struct console_font *font, unsigned flags)
{
	unsigned charcount = font->charcount;
	int w = font->width;
	int h = font->height;
	int size = h;
	int i, k;
	u8 *new_data, *data = font->data, *p;

	if (charcount != 256 && charcount != 512)
		return -EINVAL;

	if (w > 8) {
		if (w <= 16)
			size *= 2;
		else
			size *= 4;
	}
	size *= charcount;

	new_data = kmalloc(FONT_EXTRA_WORDS * sizeof(int) + size, GFP_USER);

	if (!new_data)
		return -ENOMEM;

	new_data += FONT_EXTRA_WORDS * sizeof(int);
	FNTSIZE(new_data) = size;
	FNTCHARCNT(new_data) = charcount;
	REFCOUNT(new_data) = 0;	/* usage counter */
	p = new_data;
	if (w <= 8) {
		for (i = 0; i < charcount; i++) {
			memcpy(p, data, h);
			data += 32;
			p += h;
		}
	} else if (w <= 16) {
		h *= 2;
		for (i = 0; i < charcount; i++) {
			memcpy(p, data, h);
			data += 64;
			p += h;
		}
	} else if (w <= 24) {
		for (i = 0; i < charcount; i++) {
			int j;
			for (j = 0; j < h; j++) {
				memcpy(p, data, 3);
				p[3] = 0;
				data += 3;
				p += sizeof(u32);
			}
			data += 3 * (32 - h);
		}
	} else {
		h *= 4;
		for (i = 0; i < charcount; i++) {
			memcpy(p, data, h);
			data += 128;
			p += h;
		}
	}
	/* we can do it in u32 chunks because of charcount is 256 or 512, so
	   font length must be multiple of 256, at least. And 256 is multiple
	   of 4 */
	k = 0;
	while (p > new_data) {
		p = (u8 *)((u32 *)p - 1);
		k += *(u32 *) p;
	}
	FNTSUM(new_data) = k;
	/* Check if the same font is on some other console already */
	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		struct vc_data *tmp = vc_cons[i].d;
		
		if (fb_display[i].userfont &&
		    fb_display[i].fontdata &&
		    FNTSUM(fb_display[i].fontdata) == k &&
		    FNTSIZE(fb_display[i].fontdata) == size &&
		    tmp->vc_font.width == w &&
		    !memcmp(fb_display[i].fontdata, new_data, size)) {
			kfree(new_data - FONT_EXTRA_WORDS * sizeof(int));
			new_data = fb_display[i].fontdata;
			break;
		}
	}
	return fbcon_do_set_font(vc, font->width, font->height, new_data, 1);
}

static int fbcon_set_def_font(struct vc_data *vc, struct console_font *font, char *name)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	struct font_desc *f;

	if (!name)
		f = get_default_font(info->var.xres, info->var.yres);
	else if (!(f = find_font(name)))
		return -ENOENT;

	font->width = f->width;
	font->height = f->height;
	return fbcon_do_set_font(vc, f->width, f->height, f->data, 0);
}

static u16 palette_red[16];
static u16 palette_green[16];
static u16 palette_blue[16];

static struct fb_cmap palette_cmap = {
	0, 16, palette_red, palette_green, palette_blue, NULL
};

static int fbcon_set_palette(struct vc_data *vc, unsigned char *table)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[vc->vc_num]];
	int i, j, k, depth;
	u8 val;

	if (!info->fbops->fb_blank && console_blanked)
		return -EINVAL;
	depth = fb_get_color_depth(info);
	if (depth > 3) {
		for (i = j = 0; i < 16; i++) {
			k = table[i];
			val = vc->vc_palette[j++];
			palette_red[k] = (val << 8) | val;
			val = vc->vc_palette[j++];
			palette_green[k] = (val << 8) | val;
			val = vc->vc_palette[j++];
			palette_blue[k] = (val << 8) | val;
		}
		palette_cmap.len = 16;
		palette_cmap.start = 0;
	/*
	 * If framebuffer is capable of less than 16 colors,
	 * use default palette of fbcon.
	 */
	} else
		fb_copy_cmap(fb_default_cmap(1 << depth), &palette_cmap);

	return fb_set_cmap(&palette_cmap, info);
}

static u16 *fbcon_screen_pos(struct vc_data *vc, int offset)
{
	unsigned long p;
	int line;
	
	if (vc->vc_num != fg_console || !softback_lines)
		return (u16 *) (vc->vc_origin + offset);
	line = offset / vc->vc_size_row;
	if (line >= softback_lines)
		return (u16 *) (vc->vc_origin + offset -
				softback_lines * vc->vc_size_row);
	p = softback_curr + offset;
	if (p >= softback_end)
		p += softback_buf - softback_end;
	return (u16 *) p;
}

static unsigned long fbcon_getxy(struct vc_data *vc, unsigned long pos,
				 int *px, int *py)
{
	unsigned long ret;
	int x, y;

	if (pos >= vc->vc_origin && pos < vc->vc_scr_end) {
		unsigned long offset = (pos - vc->vc_origin) / 2;

		x = offset % vc->vc_cols;
		y = offset / vc->vc_cols;
		if (vc->vc_num == fg_console)
			y += softback_lines;
		ret = pos + (vc->vc_cols - x) * 2;
	} else if (vc->vc_num == fg_console && softback_lines) {
		unsigned long offset = pos - softback_curr;

		if (pos < softback_curr)
			offset += softback_end - softback_buf;
		offset /= 2;
		x = offset % vc->vc_cols;
		y = offset / vc->vc_cols;
		ret = pos + (vc->vc_cols - x) * 2;
		if (ret == softback_end)
			ret = softback_buf;
		if (ret == softback_in)
			ret = vc->vc_origin;
	} else {
		/* Should not happen */
		x = y = 0;
		ret = vc->vc_origin;
	}
	if (px)
		*px = x;
	if (py)
		*py = y;
	return ret;
}

/* As we might be inside of softback, we may work with non-contiguous buffer,
   that's why we have to use a separate routine. */
static void fbcon_invert_region(struct vc_data *vc, u16 * p, int cnt)
{
	while (cnt--) {
		u16 a = scr_readw(p);
		if (!vc->vc_can_do_color)
			a ^= 0x0800;
		else if (vc->vc_hi_font_mask == 0x100)
			a = ((a) & 0x11ff) | (((a) & 0xe000) >> 4) |
			    (((a) & 0x0e00) << 4);
		else
			a = ((a) & 0x88ff) | (((a) & 0x7000) >> 4) |
			    (((a) & 0x0700) << 4);
		scr_writew(a, p++);
		if (p == (u16 *) softback_end)
			p = (u16 *) softback_buf;
		if (p == (u16 *) softback_in)
			p = (u16 *) vc->vc_origin;
	}
}

static int fbcon_scrolldelta(struct vc_data *vc, int lines)
{
	struct fb_info *info = registered_fb[(int) con2fb_map[fg_console]];
	struct display *p = &fb_display[fg_console];
	int offset, limit, scrollback_old;

	if (softback_top) {
		if (vc->vc_num != fg_console)
			return 0;
		if (vt_cons[vc->vc_num]->vc_mode != KD_TEXT || !lines)
			return 0;
		if (logo_shown >= 0) {
			struct vc_data *conp2 = vc_cons[logo_shown].d;

			if (conp2->vc_top == logo_lines
			    && conp2->vc_bottom == conp2->vc_rows)
				conp2->vc_top = 0;
			if (logo_shown == vc->vc_num) {
				unsigned long p, q;
				int i;

				p = softback_in;
				q = vc->vc_origin +
				    logo_lines * vc->vc_size_row;
				for (i = 0; i < logo_lines; i++) {
					if (p == softback_top)
						break;
					if (p == softback_buf)
						p = softback_end;
					p -= vc->vc_size_row;
					q -= vc->vc_size_row;
					scr_memcpyw((u16 *) q, (u16 *) p,
						    vc->vc_size_row);
				}
				softback_in = p;
				update_region(vc->vc_num, vc->vc_origin,
					      logo_lines * vc->vc_cols);
			}
			logo_shown = -1;
		}
		fbcon_cursor(vc, CM_ERASE | CM_SOFTBACK);
		fbcon_redraw_softback(vc, p, lines);
		fbcon_cursor(vc, CM_DRAW | CM_SOFTBACK);
		return 0;
	}

	if (!scrollback_phys_max)
		return -ENOSYS;

	scrollback_old = scrollback_current;
	scrollback_current -= lines;
	if (scrollback_current < 0)
		scrollback_current = 0;
	else if (scrollback_current > scrollback_max)
		scrollback_current = scrollback_max;
	if (scrollback_current == scrollback_old)
		return 0;

	if (!info->fbops->fb_blank &&
	    (console_blanked || vt_cons[vc->vc_num]->vc_mode != KD_TEXT
	     || !lines))
		return 0;
	fbcon_cursor(vc, CM_ERASE);

	offset = p->yscroll - scrollback_current;
	limit = p->vrows;
	switch (p->scrollmode) {
	case SCROLL_WRAP_MOVE:
		info->var.vmode |= FB_VMODE_YWRAP;
		break;
	case SCROLL_PAN_MOVE:
	case SCROLL_PAN_REDRAW:
		limit -= vc->vc_rows;
		info->var.vmode &= ~FB_VMODE_YWRAP;
		break;
	}
	if (offset < 0)
		offset += limit;
	else if (offset >= limit)
		offset -= limit;
	info->var.xoffset = 0;
	info->var.yoffset = offset * vc->vc_font.height;
	update_var(vc->vc_num, info);
	if (!scrollback_current)
		fbcon_cursor(vc, CM_DRAW);
	return 0;
}

static int fbcon_set_origin(struct vc_data *vc)
{
	if (softback_lines && !console_blanked)
		fbcon_scrolldelta(vc, softback_lines);
	return 0;
}

static void fbcon_suspended(struct fb_info *info)
{
	/* Clear cursor, restore saved data */
	info->cursor.enable = 0;
	info->fbops->fb_cursor(info, &info->cursor);
}

static void fbcon_resumed(struct fb_info *info)
{
	struct vc_data *vc;

	if (info->currcon < 0)
		return;
	vc = vc_cons[info->currcon].d;

	update_screen(vc->vc_num);
}

static void fbcon_modechanged(struct fb_info *info)
{
	struct vc_data *vc = vc_cons[info->currcon].d;
	struct display *p;
	int rows, cols;

	if (info->currcon < 0 || vt_cons[info->currcon]->vc_mode !=
	    KD_TEXT)
		return;
	p = &fb_display[vc->vc_num];

	info->var.xoffset = info->var.yoffset = p->yscroll = 0;

	if (CON_IS_VISIBLE(vc)) {
		var_to_display(p, &info->var, info);
		cols = info->var.xres / vc->vc_font.width;
		rows = info->var.yres / vc->vc_font.height;
		vc_resize(vc->vc_num, cols, rows);
		updatescrollmode(p, info, vc);
		scrollback_max = 0;
		scrollback_current = 0;
		update_var(vc->vc_num, info);
		fbcon_set_palette(vc, color_table);
		update_screen(vc->vc_num);
		if (softback_buf) {
			int l = fbcon_softback_size / vc->vc_size_row;
			if (l > 5)
				softback_end = softback_buf + l * vc->vc_size_row;
			else {
				/* Smaller scrollback makes no sense, and 0
				   would screw the operation totally */
				softback_top = 0;
			}
		}
	}
}

static int fbcon_mode_deleted(struct fb_info *info,
			      struct fb_videomode *mode)
{
	struct fb_info *fb_info;
	struct display *p;
	int i, j, found = 0;

	/* before deletion, ensure that mode is not in use */
	for (i = first_fb_vc; i <= last_fb_vc; i++) {
		j = (int) con2fb_map[i];
		if (j == -1)
			continue;
		fb_info = registered_fb[j];
		if (fb_info != info)
			continue;
		p = &fb_display[i];
		if (!p || !p->mode)
			continue;
		if (fb_mode_is_equal(p->mode, mode)) {
			found = 1;
			break;
		}
	}
	return found;
}

static int fbcon_fb_registered(int idx)
{
	int ret = 0;

	if (info_idx == -1) {
		info_idx = idx;
		ret = fbcon_takeover();
	}

	return ret;
}

static int fbcon_event_notify(struct notifier_block *self, 
			      unsigned long action, void *data)
{
	struct fb_event *event = (struct fb_event *) data;
	struct fb_info *info = event->info;
	struct fb_videomode *mode;
	struct fb_con2fbmap *con2fb;
	int ret = 0;

	switch(action) {
	case FB_EVENT_SUSPEND:
		fbcon_suspended(info);
		break;
	case FB_EVENT_RESUME:
		fbcon_resumed(info);
		break;
	case FB_EVENT_MODE_CHANGE:
		fbcon_modechanged(info);
		break;
	case FB_EVENT_MODE_DELETE:
		mode = (struct fb_videomode *) event->data;
		ret = fbcon_mode_deleted(info, mode);
		break;
	case FB_EVENT_FB_REGISTERED:
		ret = fbcon_fb_registered(info->node);
		break;
	case FB_EVENT_SET_CONSOLE_MAP:
		con2fb = (struct fb_con2fbmap *) event->data;
		ret = set_con2fb_map(con2fb->console - 1, con2fb->framebuffer);
		break;
	case FB_EVENT_GET_CONSOLE_MAP:
		con2fb = (struct fb_con2fbmap *) event->data;
		con2fb->framebuffer = con2fb_map[con2fb->console - 1];
		break;
	}

	return ret;
}

/*
 *  The console `switch' structure for the frame buffer based console
 */

const struct consw fb_con = {
	.owner			= THIS_MODULE,
	.con_startup 		= fbcon_startup,
	.con_init 		= fbcon_init,
	.con_deinit 		= fbcon_deinit,
	.con_clear 		= fbcon_clear,
	.con_putc 		= fbcon_putc,
	.con_putcs 		= fbcon_putcs,
	.con_cursor 		= fbcon_cursor,
	.con_scroll 		= fbcon_scroll,
	.con_bmove 		= fbcon_bmove,
	.con_switch 		= fbcon_switch,
	.con_blank 		= fbcon_blank,
	.con_font_set 		= fbcon_set_font,
	.con_font_get 		= fbcon_get_font,
	.con_font_default	= fbcon_set_def_font,
	.con_font_copy 		= fbcon_copy_font,
	.con_set_palette 	= fbcon_set_palette,
	.con_scrolldelta 	= fbcon_scrolldelta,
	.con_set_origin 	= fbcon_set_origin,
	.con_invert_region 	= fbcon_invert_region,
	.con_screen_pos 	= fbcon_screen_pos,
	.con_getxy 		= fbcon_getxy,
	.con_resize             = fbcon_resize,
};

static struct notifier_block fbcon_event_notifier = {
	.notifier_call	= fbcon_event_notify,
};

int __init fb_console_init(void)
{
	int i;

	acquire_console_sem();
	fb_register_client(&fbcon_event_notifier);
	release_console_sem();

	for (i = 0; i < MAX_NR_CONSOLES; i++)
		con2fb_map[i] = -1;

	if (num_registered_fb) {
		for (i = 0; i < FB_MAX; i++) {
			if (registered_fb[i] != NULL) {
				info_idx = i;
				break;
			}
		}
		fbcon_takeover();
	}

	return 0;
}

module_init(fb_console_init);

#ifdef MODULE

void __exit fb_console_exit(void)
{
	acquire_console_sem();
	fb_unregister_client(&fbcon_event_notifier);
	release_console_sem();
	give_up_console(&fb_con);
}	

module_exit(fb_console_exit);

#endif

/*
 *  Visible symbols for modules
 */

EXPORT_SYMBOL(fb_display);
EXPORT_SYMBOL(fb_con);

MODULE_LICENSE("GPL");
