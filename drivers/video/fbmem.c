/*
 *  linux/drivers/video/fbmem.c
 *
 *  Copyright (C) 1994 Martin Schaller
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *      2004 - Updated for fbui
 *      - Zack T Smith <fbui@comcast.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/linux_logo.h>
#include <linux/proc_fs.h>
#include <linux/console.h>
#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif
#include <linux/devfs_fs_kernel.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/device.h>

#if defined(__mc68000__) || defined(CONFIG_APUS)
#include <asm/setup.h>
#endif

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <linux/fb.h>

    /*
     *  Frame buffer device initialization and setup routines
     */

#define FBPIXMAPSIZE	16384

static struct notifier_block *fb_notifier_list;
struct fb_info *registered_fb[FB_MAX];
int num_registered_fb;

/*
 * Helpers
 */

int fb_get_color_depth(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;

	if (var->green.length == var->blue.length &&
	    var->green.length == var->red.length &&
	    !var->green.offset && !var->blue.offset &&
	    !var->red.offset)
		return var->green.length;
	else
		return (var->green.length + var->red.length +
			var->blue.length);
}
EXPORT_SYMBOL(fb_get_color_depth);

/*
 * Drawing helpers.
 */
void fb_iomove_buf_aligned(struct fb_info *info, struct fb_pixmap *buf,
			   u8 *dst, u32 d_pitch, u8 *src, u32 s_pitch,
			   u32 height)
{
	int i;

	for (i = height; i--; ) {
		buf->outbuf(info, dst, src, s_pitch);
		src += s_pitch;
		dst += d_pitch;
	}
}

void fb_sysmove_buf_aligned(struct fb_info *info, struct fb_pixmap *buf,
			    u8 *dst, u32 d_pitch, u8 *src, u32 s_pitch,
			    u32 height)
{
	int i, j;

	for (i = height; i--; ) {
		for (j = 0; j < s_pitch; j++)
			dst[j] = src[j];
		src += s_pitch;
		dst += d_pitch;
	}
}

void fb_iomove_buf_unaligned(struct fb_info *info, struct fb_pixmap *buf,
			     u8 *dst, u32 d_pitch, u8 *src, u32 idx,
			     u32 height, u32 shift_high, u32 shift_low,
			     u32 mod)
{
	u8 mask = (u8) (0xfff << shift_high), tmp;
	int i, j;

	for (i = height; i--; ) {
		for (j = 0; j < idx; j++) {
			tmp = buf->inbuf(info, dst+j);
			tmp &= mask;
			tmp |= *src >> shift_low;
			buf->outbuf(info, dst+j, &tmp, 1);
			tmp = *src << shift_high;
			buf->outbuf(info, dst+j+1, &tmp, 1);
			src++;
		}
		tmp = buf->inbuf(info, dst+idx);
		tmp &= mask;
		tmp |= *src >> shift_low;
		buf->outbuf(info, dst+idx, &tmp, 1);
		if (shift_high < mod) {
			tmp = *src << shift_high;
			buf->outbuf(info, dst+idx+1, &tmp, 1);
		}	
		src++;
		dst += d_pitch;
	}
}

void fb_sysmove_buf_unaligned(struct fb_info *info, struct fb_pixmap *buf,
			      u8 *dst, u32 d_pitch, u8 *src, u32 idx,
			      u32 height, u32 shift_high, u32 shift_low,
			      u32 mod)
{
	u8 mask = (u8) (0xfff << shift_high), tmp;
	int i, j;

	for (i = height; i--; ) {
		for (j = 0; j < idx; j++) {
			tmp = dst[j];
			tmp &= mask;
			tmp |= *src >> shift_low;
			dst[j] = tmp;
			tmp = *src << shift_high;
			dst[j+1] = tmp;
			src++;
		}
		tmp = dst[idx];
		tmp &= mask;
		tmp |= *src >> shift_low;
		dst[idx] = tmp;
		if (shift_high < mod) {
			tmp = *src << shift_high;
			dst[idx+1] = tmp;
		}
		src++;
		dst += d_pitch;
	}
}

/*
 * we need to lock this section since fb_cursor
 * may use fb_imageblit()
 */
char* fb_get_buffer_offset(struct fb_info *info, struct fb_pixmap *buf, u32 size)
{
	u32 align = buf->buf_align - 1, offset;
	char *addr = buf->addr;

	/* If IO mapped, we need to sync before access, no sharing of
	 * the pixmap is done
	 */
	if (buf->flags & FB_PIXMAP_IO) {
		if (info->fbops->fb_sync && (buf->flags & FB_PIXMAP_SYNC))
			info->fbops->fb_sync(info);
		return addr;
	}

	/* See if we fit in the remaining pixmap space */
	offset = buf->offset + align;
	offset &= ~align;
	if (offset + size > buf->size) {
		/* We do not fit. In order to be able to re-use the buffer,
		 * we must ensure no asynchronous DMA'ing or whatever operation
		 * is in progress, we sync for that.
		 */
		if (info->fbops->fb_sync && (buf->flags & FB_PIXMAP_SYNC))
			info->fbops->fb_sync(info);
		offset = 0;
	}
	buf->offset = offset + size;
	addr += offset;

	return addr;
}

#ifdef CONFIG_LOGO
#include <linux/linux_logo.h>

static inline unsigned safe_shift(unsigned d, int n)
{
	return n < 0 ? d >> -n : d << n;
}

static void fb_set_logocmap(struct fb_info *info,
				   const struct linux_logo *logo)
{
	struct fb_cmap palette_cmap;
	u16 palette_green[16];
	u16 palette_blue[16];
	u16 palette_red[16];
	int i, j, n;
	const unsigned char *clut = logo->clut;

	palette_cmap.start = 0;
	palette_cmap.len = 16;
	palette_cmap.red = palette_red;
	palette_cmap.green = palette_green;
	palette_cmap.blue = palette_blue;
	palette_cmap.transp = NULL;

	for (i = 0; i < logo->clutsize; i += n) {
		n = logo->clutsize - i;
		/* palette_cmap provides space for only 16 colors at once */
		if (n > 16)
			n = 16;
		palette_cmap.start = 32 + i;
		palette_cmap.len = n;
		for (j = 0; j < n; ++j) {
			palette_cmap.red[j] = clut[0] << 8 | clut[0];
			palette_cmap.green[j] = clut[1] << 8 | clut[1];
			palette_cmap.blue[j] = clut[2] << 8 | clut[2];
			clut += 3;
		}
		fb_set_cmap(&palette_cmap, info);
	}
}

static void  fb_set_logo_truepalette(struct fb_info *info,
					    const struct linux_logo *logo,
					    u32 *palette)
{
	unsigned char mask[9] = { 0,0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff };
	unsigned char redmask, greenmask, bluemask;
	int redshift, greenshift, blueshift;
	int i;
	const unsigned char *clut = logo->clut;

	/*
	 * We have to create a temporary palette since console palette is only
	 * 16 colors long.
	 */
	/* Bug: Doesn't obey msb_right ... (who needs that?) */
	redmask   = mask[info->var.red.length   < 8 ? info->var.red.length   : 8];
	greenmask = mask[info->var.green.length < 8 ? info->var.green.length : 8];
	bluemask  = mask[info->var.blue.length  < 8 ? info->var.blue.length  : 8];
	redshift   = info->var.red.offset   - (8 - info->var.red.length);
	greenshift = info->var.green.offset - (8 - info->var.green.length);
	blueshift  = info->var.blue.offset  - (8 - info->var.blue.length);

	for ( i = 0; i < logo->clutsize; i++) {
		palette[i+32] = (safe_shift((clut[0] & redmask), redshift) |
				 safe_shift((clut[1] & greenmask), greenshift) |
				 safe_shift((clut[2] & bluemask), blueshift));
		clut += 3;
	}
}

static void fb_set_logo_directpalette(struct fb_info *info,
					     const struct linux_logo *logo,
					     u32 *palette)
{
	int redshift, greenshift, blueshift;
	int i;

	redshift = info->var.red.offset;
	greenshift = info->var.green.offset;
	blueshift = info->var.blue.offset;

	for (i = 32; i < logo->clutsize; i++)
		palette[i] = i << redshift | i << greenshift | i << blueshift;
}

static void fb_set_logo(struct fb_info *info,
			       const struct linux_logo *logo, u8 *dst,
			       int depth)
{
	int i, j, k, fg = 1;
	const u8 *src = logo->data;
	u8 d, xor = (info->fix.visual == FB_VISUAL_MONO01) ? 0xff : 0;

	if (fb_get_color_depth(info) == 3)
		fg = 7;

	switch (depth) {
	case 4:
		for (i = 0; i < logo->height; i++)
			for (j = 0; j < logo->width; src++) {
				*dst++ = *src >> 4;
				j++;
				if (j < logo->width) {
					*dst++ = *src & 0x0f;
					j++;
				}
			}
		break;
	case 1:
		for (i = 0; i < logo->height; i++) {
			for (j = 0; j < logo->width; src++) {
				d = *src ^ xor;
				for (k = 7; k >= 0; k--) {
					*dst++ = ((d >> k) & 1) ? fg : 0;
					j++;
				}
			}
		}
		break;
	}
}

/*
 * Three (3) kinds of logo maps exist.  linux_logo_clut224 (>16 colors),
 * linux_logo_vga16 (16 colors) and linux_logo_mono (2 colors).  Depending on
 * the visual format and color depth of the framebuffer, the DAC, the
 * pseudo_palette, and the logo data will be adjusted accordingly.
 *
 * Case 1 - linux_logo_clut224:
 * Color exceeds the number of console colors (16), thus we set the hardware DAC
 * using fb_set_cmap() appropriately.  The "needs_cmapreset"  flag will be set.
 *
 * For visuals that require color info from the pseudo_palette, we also construct
 * one for temporary use. The "needs_directpalette" or "needs_truepalette" flags
 * will be set.
 *
 * Case 2 - linux_logo_vga16:
 * The number of colors just matches the console colors, thus there is no need
 * to set the DAC or the pseudo_palette.  However, the bitmap is packed, ie,
 * each byte contains color information for two pixels (upper and lower nibble).
 * To be consistent with fb_imageblit() usage, we therefore separate the two
 * nibbles into separate bytes. The "depth" flag will be set to 4.
 *
 * Case 3 - linux_logo_mono:
 * This is similar with Case 2.  Each byte contains information for 8 pixels.
 * We isolate each bit and expand each into a byte. The "depth" flag will
 * be set to 1.
 */
static struct logo_data {
	int depth;
	int needs_directpalette;
	int needs_truepalette;
	int needs_cmapreset;
	const struct linux_logo *logo;
} fb_logo;

int fb_prepare_logo(struct fb_info *info)
{
	int depth = fb_get_color_depth(info);

	memset(&fb_logo, 0, sizeof(struct logo_data));

	if (info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		depth = info->var.blue.length;
		if (info->var.red.length < depth)
			depth = info->var.red.length;
		if (info->var.green.length < depth)
			depth = info->var.green.length;
	}

	if (depth >= 8) {
		switch (info->fix.visual) {
		case FB_VISUAL_TRUECOLOR:
			fb_logo.needs_truepalette = 1;
			break;
		case FB_VISUAL_DIRECTCOLOR:
			fb_logo.needs_directpalette = 1;
			fb_logo.needs_cmapreset = 1;
			break;
		case FB_VISUAL_PSEUDOCOLOR:
			fb_logo.needs_cmapreset = 1;
			break;
		}
	}

	/* Return if no suitable logo was found */
	fb_logo.logo = fb_find_logo(depth);
	
	if (!fb_logo.logo || fb_logo.logo->height > info->var.yres) {
		fb_logo.logo = NULL;
		return 0;
	}
	/* What depth we asked for might be different from what we get */
	if (fb_logo.logo->type == LINUX_LOGO_CLUT224)
		fb_logo.depth = 8;
	else if (fb_logo.logo->type == LINUX_LOGO_VGA16)
		fb_logo.depth = 4;
	else
		fb_logo.depth = 1;		
	return fb_logo.logo->height;
}

int fb_show_logo(struct fb_info *info)
{
	u32 *palette = NULL, *saved_pseudo_palette = NULL;
	unsigned char *logo_new = NULL;
	struct fb_image image;
	int x;

	/* Return if the frame buffer is not mapped or suspended */
	if (fb_logo.logo == NULL || info->state != FBINFO_STATE_RUNNING)
		return 0;

	image.depth = 8;
	image.data = fb_logo.logo->data;

	if (fb_logo.needs_cmapreset)
		fb_set_logocmap(info, fb_logo.logo);

	if (fb_logo.needs_truepalette || 
	    fb_logo.needs_directpalette) {
		palette = kmalloc(256 * 4, GFP_KERNEL);
		if (palette == NULL)
			return 0;

		if (fb_logo.needs_truepalette)
			fb_set_logo_truepalette(info, fb_logo.logo, palette);
		else
			fb_set_logo_directpalette(info, fb_logo.logo, palette);

		saved_pseudo_palette = info->pseudo_palette;
		info->pseudo_palette = palette;
	}

	if (fb_logo.depth <= 4) {
		logo_new = kmalloc(fb_logo.logo->width * fb_logo.logo->height, 
				   GFP_KERNEL);
		if (logo_new == NULL) {
			if (palette)
				kfree(palette);
			if (saved_pseudo_palette)
				info->pseudo_palette = saved_pseudo_palette;
			return 0;
		}
		image.data = logo_new;
		fb_set_logo(info, fb_logo.logo, logo_new, fb_logo.depth);
	}

	image.width = fb_logo.logo->width;
	image.height = fb_logo.logo->height;
	image.dy = 0;

	for (x = 0; x < num_online_cpus() * (fb_logo.logo->width + 8) &&
	     x <= info->var.xres-fb_logo.logo->width; x += (fb_logo.logo->width + 8)) {
		image.dx = x;
		info->fbops->fb_imageblit(info, &image);
	}
	
	if (palette != NULL)
		kfree(palette);
	if (saved_pseudo_palette != NULL)
		info->pseudo_palette = saved_pseudo_palette;
	if (logo_new != NULL)
		kfree(logo_new);
	return fb_logo.logo->height;
}
#else
int fb_prepare_logo(struct fb_info *info) { return 0; }
int fb_show_logo(struct fb_info *info) { return 0; }
#endif /* CONFIG_LOGO */

static int fbmem_read_proc(char *buf, char **start, off_t offset,
			   int len, int *eof, void *private)
{
	struct fb_info **fi;
	int clen;

	clen = 0;
	for (fi = registered_fb; fi < &registered_fb[FB_MAX] && len < 4000; fi++)
		if (*fi)
			clen += sprintf(buf + clen, "%d %s\n",
				        (*fi)->node,
				        (*fi)->fix.id);
	*start = buf + offset;
	if (clen > offset)
		clen -= offset;
	else
		clen = 0;
	return clen < len ? clen : len;
}

static ssize_t
fb_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_dentry->d_inode;
	int fbidx = iminor(inode);
	struct fb_info *info = registered_fb[fbidx];
	u32 *buffer, *dst, *src;
	int c, i, cnt = 0, err = 0;
	unsigned long total_size;

	if (!info || ! info->screen_base)
		return -ENODEV;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	if (info->fbops->fb_read)
		return info->fbops->fb_read(file, buf, count, ppos);
	
	total_size = info->screen_size;
	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p >= total_size)
	    return 0;
	if (count >= total_size)
	    count = total_size;
	if (count + p > total_size)
		count = total_size - p;

	cnt = 0;
	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	src = (u32 *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	while (count) {
		c  = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		dst = buffer;
		for (i = c >> 2; i--; )
			*dst++ = fb_readl(src++);
		if (c & 3) {
			u8 *dst8 = (u8 *) dst;
			u8 *src8 = (u8 *) src;

			for (i = c & 3; i--;)
				*dst8++ = fb_readb(src8++);

			src = (u32 *) src8;
		}

		if (copy_to_user(buf, buffer, c)) {
			err = -EFAULT;
			break;
		}
		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}

	kfree(buffer);
	return (err) ? err : cnt;
}

static ssize_t
fb_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_dentry->d_inode;
	int fbidx = iminor(inode);
	struct fb_info *info = registered_fb[fbidx];
	u32 *buffer, *dst, *src;
	int c, i, cnt = 0, err;
	unsigned long total_size;

	if (!info || !info->screen_base)
		return -ENODEV;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	if (info->fbops->fb_write)
		return info->fbops->fb_write(file, buf, count, ppos);
	
	total_size = info->screen_size;
	if (total_size == 0)
		total_size = info->fix.smem_len;

	if (p > total_size)
	    return -ENOSPC;
	if (count >= total_size)
	    count = total_size;
	err = 0;
	if (count + p > total_size) {
	    count = total_size - p;
	    err = -ENOSPC;
	}
	cnt = 0;
	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	dst = (u32 *) (info->screen_base + p);

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	while (count) {
		c = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		src = buffer;
		if (copy_from_user(src, buf, c)) {
			err = -EFAULT;
			break;
		}
		for (i = c >> 2; i--; )
			fb_writel(*src++, dst++);
		if (c & 3) {
			u8 *src8 = (u8 *) src;
			u8 *dst8 = (u8 *) dst;

			for (i = c & 3; i--; )
				fb_writeb(*src8++, dst8++);

			dst = (u32 *) dst8;
		}
		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}
	kfree(buffer);

	return (err) ? err : cnt;
}

#ifdef CONFIG_KMOD
static void try_to_load(int fb)
{
	request_module("fb%d", fb);
}
#endif /* CONFIG_KMOD */

void
fb_load_cursor_image(struct fb_info *info)
{
	unsigned int width = (info->cursor.image.width + 7) >> 3;
	u8 *data = (u8 *) info->cursor.image.data;

	if (info->sprite.outbuf)
	    info->sprite.outbuf(info, info->sprite.addr, data, width);
	else
	    memcpy(info->sprite.addr, data, width);
}

int
fb_cursor(struct fb_info *info, struct fb_cursor_user __user *sprite)
{
	struct fb_cursor_user cursor_user;
	struct fb_cursor cursor;
	char *data = NULL, *mask = NULL, *info_mask = NULL;
	u16 *red = NULL, *green = NULL, *blue = NULL, *transp = NULL;
	int err = -EINVAL;
	
	if (copy_from_user(&cursor_user, sprite, sizeof(struct fb_cursor_user)))
		return -EFAULT;

	memcpy(&cursor, &cursor_user, sizeof(cursor_user));
	cursor.mask = info->cursor.mask;
	cursor.image.data = info->cursor.image.data;
	cursor.image.cmap.red = info->cursor.image.cmap.red;
	cursor.image.cmap.green = info->cursor.image.cmap.green;
	cursor.image.cmap.blue = info->cursor.image.cmap.blue;
	cursor.image.cmap.transp = info->cursor.image.cmap.transp;
	cursor.data = NULL;

	if (cursor.set & FB_CUR_SETCUR)
		info->cursor.enable = 1;
	
	if (cursor.set & FB_CUR_SETCMAP) {
		unsigned len = cursor.image.cmap.len;
		if ((int)len <= 0)
			goto out;
		len *= 2;
		err = -ENOMEM;
		red = kmalloc(len, GFP_USER);
		green = kmalloc(len, GFP_USER);
		blue = kmalloc(len, GFP_USER);
		if (!red || !green || !blue)
			goto out;
		if (cursor_user.image.cmap.transp) {
			transp = kmalloc(len, GFP_USER);
			if (!transp)
				goto out;
		}
		err = -EFAULT;
		if (copy_from_user(red, cursor_user.image.cmap.red, len))
			goto out;
		if (copy_from_user(green, cursor_user.image.cmap.green, len))
			goto out;
		if (copy_from_user(blue, cursor_user.image.cmap.blue, len))
			goto out;
		if (transp) {
			if (copy_from_user(transp,
					   cursor_user.image.cmap.transp, len))
				goto out;
		}
		cursor.image.cmap.red = red;
		cursor.image.cmap.green = green;
		cursor.image.cmap.blue = blue;
		cursor.image.cmap.transp = transp;
	}
	
	if (cursor.set & FB_CUR_SETSHAPE) {
		int size = ((cursor.image.width + 7) >> 3) * cursor.image.height;		

		if ((cursor.image.height != info->cursor.image.height) ||
		    (cursor.image.width != info->cursor.image.width))
			cursor.set |= FB_CUR_SETSIZE;
		
		err = -ENOMEM;
		data = kmalloc(size, GFP_USER);
		mask = kmalloc(size, GFP_USER);
		if (!mask || !data)
			goto out;
		
		err = -EFAULT;
		if (copy_from_user(data, cursor_user.image.data, size) ||
		    copy_from_user(mask, cursor_user.mask, size))
			goto out;
		
		cursor.image.data = data;
		cursor.mask = mask;
		info_mask = (char *) info->cursor.mask;
		info->cursor.mask = mask;
	}
	info->cursor.set = cursor.set;
	info->cursor.rop = cursor.rop;
	err = info->fbops->fb_cursor(info, &cursor);
out:
	kfree(data);
	kfree(mask);
	kfree(red);
	kfree(green);
	kfree(blue);
	kfree(transp);
	if (info_mask)
		info->cursor.mask = info_mask;
	return err;
}

int
fb_pan_display(struct fb_info *info, struct fb_var_screeninfo *var)
{
        int xoffset = var->xoffset;
        int yoffset = var->yoffset;
        int err;

        if (xoffset < 0 || yoffset < 0 || !info->fbops->fb_pan_display ||
            xoffset + info->var.xres > info->var.xres_virtual ||
            yoffset + info->var.yres > info->var.yres_virtual)
                return -EINVAL;
	if ((err = info->fbops->fb_pan_display(var, info)))
		return err;
        info->var.xoffset = var->xoffset;
        info->var.yoffset = var->yoffset;
        if (var->vmode & FB_VMODE_YWRAP)
                info->var.vmode |= FB_VMODE_YWRAP;
        else
                info->var.vmode &= ~FB_VMODE_YWRAP;
        return 0;
}

int
fb_set_var(struct fb_info *info, struct fb_var_screeninfo *var)
{
	int err;

	if (var->activate & FB_ACTIVATE_INV_MODE) {
		struct fb_videomode mode1, mode2;
		int ret = 0;

		fb_var_to_videomode(&mode1, var);
		fb_var_to_videomode(&mode2, &info->var);
		/* make sure we don't delete the videomode of current var */
		ret = fb_mode_is_equal(&mode1, &mode2);

		if (!ret) {
		    struct fb_event event;

		    event.info = info;
		    event.data = &mode1;
		    ret = notifier_call_chain(&fb_notifier_list,
					      FB_EVENT_MODE_DELETE, &event);
		}

		if (!ret)
		    fb_delete_videomode(&mode1, &info->modelist);

		return ret;
	}

	if ((var->activate & FB_ACTIVATE_FORCE) ||
	    memcmp(&info->var, var, sizeof(struct fb_var_screeninfo))) {
		if (!info->fbops->fb_check_var) {
			*var = info->var;
			return 0;
		}

		if ((err = info->fbops->fb_check_var(var, info)))
			return err;

		if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
			struct fb_videomode mode;
			info->var = *var;

			if (info->fbops->fb_set_par)
				info->fbops->fb_set_par(info);

			fb_pan_display(info, &info->var);

			fb_set_cmap(&info->cmap, info);

			fb_var_to_videomode(&mode, &info->var);
			fb_add_videomode(&mode, &info->modelist);

			if (info->flags & FBINFO_MISC_MODECHANGEUSER) {
				struct fb_event event;

				info->flags &= ~FBINFO_MISC_MODECHANGEUSER;
				event.info = info;
				notifier_call_chain(&fb_notifier_list,
						    FB_EVENT_MODE_CHANGE, &event);
			}
		}
	}
	return 0;
}

int
fb_blank(struct fb_info *info, int blank)
{	
	/* ??? Variable sized stack allocation.  */
	u16 black[info->cmap.len];
	struct fb_cmap cmap;
	
	if (info->fbops->fb_blank && !info->fbops->fb_blank(blank, info))
		return 0;
	if (blank) { 
		memset(black, 0, info->cmap.len * sizeof(u16));
		cmap.red = cmap.green = cmap.blue = black;
		cmap.transp = info->cmap.transp ? black : NULL;
		cmap.start = info->cmap.start;
		cmap.len = info->cmap.len;
	} else
		cmap = info->cmap;
	return fb_set_cmap(&cmap, info);
}

static int 
fb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	 unsigned long arg)
{
	int fbidx = iminor(inode);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct fb_con2fbmap con2fb;
	struct fb_cmap_user cmap;
	struct fb_event event;
	void __user *argp = (void __user *)arg;
	int i;
	
	if (!fb)
		return -ENODEV;
	switch (cmd) {
	case FBIOGET_VSCREENINFO:
		return copy_to_user(argp, &info->var,
				    sizeof(var)) ? -EFAULT : 0;
	case FBIOPUT_VSCREENINFO:
		if (copy_from_user(&var, argp, sizeof(var)))
			return -EFAULT;
		acquire_console_sem();
		info->flags |= FBINFO_MISC_MODECHANGEUSER;
		i = fb_set_var(info, &var);
		info->flags &= ~FBINFO_MISC_MODECHANGEUSER;
		release_console_sem();
		if (i) return i;
		if (copy_to_user(argp, &var, sizeof(var)))
			return -EFAULT;
		return 0;
	case FBIOGET_FSCREENINFO:
		return copy_to_user(argp, &info->fix,
				    sizeof(fix)) ? -EFAULT : 0;
	case FBIOPUTCMAP:
		if (copy_from_user(&cmap, argp, sizeof(cmap)))
			return -EFAULT;
		return (fb_set_user_cmap(&cmap, info));
	case FBIOGETCMAP:
		if (copy_from_user(&cmap, argp, sizeof(cmap)))
			return -EFAULT;
		return fb_cmap_to_user(&info->cmap, &cmap);
	case FBIOPAN_DISPLAY:
		if (copy_from_user(&var, argp, sizeof(var)))
			return -EFAULT;
		acquire_console_sem();
		i = fb_pan_display(info, &var);
		release_console_sem();
		if (i)
			return i;
		if (copy_to_user(argp, &var, sizeof(var)))
			return -EFAULT;
		return 0;
	case FBIO_CURSOR:
		acquire_console_sem();
		i = fb_cursor(info, argp);
		release_console_sem();
		return i;
	case FBIOGET_CON2FBMAP:
		if (copy_from_user(&con2fb, argp, sizeof(con2fb)))
			return -EFAULT;
		if (con2fb.console < 1 || con2fb.console > MAX_NR_CONSOLES)
		    return -EINVAL;
		con2fb.framebuffer = -1;
		event.info = info;
		event.data = &con2fb;
		notifier_call_chain(&fb_notifier_list,
				    FB_EVENT_GET_CONSOLE_MAP, &event);
		return copy_to_user(argp, &con2fb,
				    sizeof(con2fb)) ? -EFAULT : 0;
	case FBIOPUT_CON2FBMAP:
		if (copy_from_user(&con2fb, argp, sizeof(con2fb)))
			return - EFAULT;
		if (con2fb.console < 0 || con2fb.console > MAX_NR_CONSOLES)
		    return -EINVAL;
		if (con2fb.framebuffer < 0 || con2fb.framebuffer >= FB_MAX)
		    return -EINVAL;
#ifdef CONFIG_KMOD
		if (!registered_fb[con2fb.framebuffer])
		    try_to_load(con2fb.framebuffer);
#endif /* CONFIG_KMOD */
		if (!registered_fb[con2fb.framebuffer])
		    return -EINVAL;
		if (con2fb.console > 0 && con2fb.console < MAX_NR_CONSOLES) {
			event.info = info;
			event.data = &con2fb;
			return notifier_call_chain(&fb_notifier_list,
						   FB_EVENT_SET_CONSOLE_MAP,
						   &event);
		}
		return -EINVAL;
	case FBIOBLANK:
		acquire_console_sem();
		i = fb_blank(info, arg);
		release_console_sem();
		return i;

#ifdef CONFIG_FB_UI
	case FBIO_UI_OPEN: {
                if (access_ok (VERIFY_READ, (char*) arg, sizeof(struct fbui_openparams))) 
			return fbui_open (info, (struct fbui_openparams*) arg);
                else
			return -EFAULT;
	}

	case FBIO_UI_CLOSE:
		return fbui_close (info, arg);

        case FBIO_UI_EXEC: {
                short win_id=-1;
                short nwords=0;
                short *ptr=(short*) arg;
                if (access_ok (VERIFY_READ, (char*) arg, 4)) {
                        if (get_user (win_id, ptr))
				return -EFAULT;
			if (win_id < 0 || win_id >= FBUI_MAXWINDOWSPERVC*FBUI_MAXCONSOLES)
				return FBUI_ERR_BADWIN;
			ptr++;
                        if (get_user (nwords, ptr))
				return -EFAULT;
			arg += 4;
			if (access_ok (VERIFY_READ, (char*) (arg), 2*nwords)) 
				return fbui_exec (info, win_id, nwords, 
					(unsigned char*) (arg));
			else
				return -EFAULT;
                }
                else
			return -EFAULT;
        }

	case FBIO_UI_CONTROL:
                if (access_ok (VERIFY_READ, (char*) arg, sizeof(struct fbui_ctrlparams)))
		{
			struct fbui_ctrlparams ctl;
			if (!copy_from_user (&ctl, (void*)arg, sizeof(struct fbui_ctrlparams))){
				return fbui_control (info, &ctl);
			} else {
				return -EFAULT;
			}
		} else {
			return -EFAULT;
}
#endif

	default:
		if (fb->fb_ioctl == NULL)
			return -EINVAL;
		return fb->fb_ioctl(inode, file, cmd, arg, info);
	}
}

static int 
fb_mmap(struct file *file, struct vm_area_struct * vma)
{
	int fbidx = iminor(file->f_dentry->d_inode);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	unsigned long off;
#if !defined(__sparc__) || defined(__sparc_v9__)
	unsigned long start;
	u32 len;
#endif

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;
	if (!fb)
		return -ENODEV;
	if (fb->fb_mmap) {
		int res;
		lock_kernel();
		res = fb->fb_mmap(info, file, vma);
		unlock_kernel();
		return res;
	}

#if defined(__sparc__) && !defined(__sparc_v9__)
	/* Should never get here, all fb drivers should have their own
	   mmap routines */
	return -EINVAL;
#else
	/* !sparc32... */
	lock_kernel();

	/* frame buffer memory */
	start = info->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);
	if (off >= len) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags) {
			unlock_kernel();
			return -EINVAL;
		}
		start = info->fix.mmio_start;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.mmio_len);
	}
	unlock_kernel();
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;
#if defined(__sparc_v9__)
	vma->vm_flags |= (VM_SHM | VM_LOCKED);
	if (io_remap_page_range(vma, vma->vm_start, off,
				vma->vm_end - vma->vm_start, vma->vm_page_prot, 0))
		return -EAGAIN;
#else
#if defined(__mc68000__)
#if defined(CONFIG_SUN3)
	pgprot_val(vma->vm_page_prot) |= SUN3_PAGE_NOCACHE;
#elif defined(CONFIG_MMU)
	if (CPU_IS_020_OR_030)
		pgprot_val(vma->vm_page_prot) |= _PAGE_NOCACHE030;
	if (CPU_IS_040_OR_060) {
		pgprot_val(vma->vm_page_prot) &= _CACHEMASK040;
		/* Use no-cache mode, serialized */
		pgprot_val(vma->vm_page_prot) |= _PAGE_NOCACHE_S;
	}
#endif
#elif defined(__powerpc__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE|_PAGE_GUARDED;
#elif defined(__alpha__)
	/* Caching is off in the I/O space quadrant by design.  */
#elif defined(__i386__) || defined(__x86_64__)
	if (boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#elif defined(__mips__)
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#elif defined(__hppa__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE;
#elif defined(__ia64__) || defined(__arm__) || defined(__sh__)
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#else
#warning What do we have to do here??
#endif
	if (io_remap_page_range(vma, vma->vm_start, off,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
#endif /* !__sparc_v9__ */
	return 0;
#endif /* !sparc32 */
}

static int
fb_open(struct inode *inode, struct file *file)
{
	int fbidx = iminor(inode);
	struct fb_info *info;
	int res = 0;

	if (fbidx >= FB_MAX)
		return -ENODEV;
#ifdef CONFIG_KMOD
	if (!(info = registered_fb[fbidx]))
		try_to_load(fbidx);
#endif /* CONFIG_KMOD */
	if (!(info = registered_fb[fbidx]))
		return -ENODEV;
	if (!try_module_get(info->fbops->owner))
		return -ENODEV;
	if (info->fbops->fb_open) {
		res = info->fbops->fb_open(info,1);
		if (res)
			module_put(info->fbops->owner);
	}
	return res;
}

static int 
fb_release(struct inode *inode, struct file *file)
{
	int fbidx = iminor(inode);
	struct fb_info *info;

	lock_kernel();
	info = registered_fb[fbidx];
	if (info->fbops->fb_release)
		info->fbops->fb_release(info,1);
	module_put(info->fbops->owner);
	unlock_kernel();
	return 0;
}

static struct file_operations fb_fops = {
	.owner =	THIS_MODULE,
	.read =		fb_read,
	.write =	fb_write,
	.ioctl =	fb_ioctl,
	.mmap =		fb_mmap,
	.open =		fb_open,
	.release =	fb_release,
#ifdef HAVE_ARCH_FB_UNMAPPED_AREA
	.get_unmapped_area = get_fb_unmapped_area,
#endif
};

static struct class_simple *fb_class;

/**
 *	register_framebuffer - registers a frame buffer device
 *	@fb_info: frame buffer info structure
 *
 *	Registers a frame buffer device @fb_info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int
register_framebuffer(struct fb_info *fb_info)
{
	int i;
	struct class_device *c;
	struct fb_event event;

	if (num_registered_fb == FB_MAX)
		return -ENXIO;
	num_registered_fb++;
	for (i = 0 ; i < FB_MAX; i++)
		if (!registered_fb[i])
			break;
	fb_info->node = i;

	c = class_simple_device_add(fb_class, MKDEV(FB_MAJOR, i), NULL, "fb%d", i);
	if (IS_ERR(c)) {
		/* Not fatal */
		printk(KERN_WARNING "Unable to create class_device for framebuffer %d; errno = %ld\n", i, PTR_ERR(c));
	}
	
	if (fb_info->pixmap.addr == NULL) {
		fb_info->pixmap.addr = kmalloc(FBPIXMAPSIZE, GFP_KERNEL);
		if (fb_info->pixmap.addr) {
			fb_info->pixmap.size = FBPIXMAPSIZE;
			fb_info->pixmap.buf_align = 1;
			fb_info->pixmap.scan_align = 1;
			fb_info->pixmap.access_align = 4;
			fb_info->pixmap.flags = FB_PIXMAP_DEFAULT;
		}
	}	
	fb_info->pixmap.offset = 0;

	if (fb_info->sprite.addr == NULL) {
		fb_info->sprite.addr = kmalloc(FBPIXMAPSIZE, GFP_KERNEL);
		if (fb_info->sprite.addr) {
			fb_info->sprite.size = FBPIXMAPSIZE;
			fb_info->sprite.buf_align = 1;
			fb_info->sprite.scan_align = 1;
			fb_info->sprite.access_align = 4;
			fb_info->sprite.flags = FB_PIXMAP_DEFAULT;
		}
	}
	fb_info->sprite.offset = 0;

	if (!fb_info->modelist.prev ||
	    !fb_info->modelist.next ||
	    list_empty(&fb_info->modelist)) {
	        struct fb_videomode mode;

		INIT_LIST_HEAD(&fb_info->modelist);
		fb_var_to_videomode(&mode, &fb_info->var);
		fb_add_videomode(&mode, &fb_info->modelist);
	}

	registered_fb[i] = fb_info;

	devfs_mk_cdev(MKDEV(FB_MAJOR, i),
			S_IFCHR | S_IRUGO | S_IWUGO, "fb/%d", i);
	event.info = fb_info;
	notifier_call_chain(&fb_notifier_list,
			    FB_EVENT_FB_REGISTERED, &event);
	return 0;
}


/**
 *	unregister_framebuffer - releases a frame buffer device
 *	@fb_info: frame buffer info structure
 *
 *	Unregisters a frame buffer device @fb_info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int
unregister_framebuffer(struct fb_info *fb_info)
{
	int i;

	i = fb_info->node;
	if (!registered_fb[i])
		return -EINVAL;
	devfs_remove("fb/%d", i);

	if (fb_info->pixmap.addr && (fb_info->pixmap.flags & FB_PIXMAP_DEFAULT))
		kfree(fb_info->pixmap.addr);
	if (fb_info->sprite.addr && (fb_info->sprite.flags & FB_PIXMAP_DEFAULT))
		kfree(fb_info->sprite.addr);
	fb_destroy_modelist(&fb_info->modelist);
	registered_fb[i]=NULL;
	num_registered_fb--;
	class_simple_device_remove(MKDEV(FB_MAJOR, i));
	return 0;
}

/**
 *	fb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_register_client(struct notifier_block *nb)
{
	return notifier_chain_register(&fb_notifier_list, nb);
}

/**
 *	fb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_unregister_client(struct notifier_block *nb)
{
	return notifier_chain_unregister(&fb_notifier_list, nb);
}

/**
 *	fb_set_suspend - low level driver signals suspend
 *	@info: framebuffer affected
 *	@state: 0 = resuming, !=0 = suspending
 *
 *	This is meant to be used by low level drivers to
 * 	signal suspend/resume to the core & clients.
 *	It must be called with the console semaphore held
 */
void fb_set_suspend(struct fb_info *info, int state)
{
	struct fb_event event;

	event.info = info;
	if (state) {
		notifier_call_chain(&fb_notifier_list, FB_EVENT_SUSPEND, &event);
		info->state = FBINFO_STATE_SUSPENDED;
	} else {
		info->state = FBINFO_STATE_RUNNING;
		notifier_call_chain(&fb_notifier_list, FB_EVENT_RESUME, &event);
	}
}

/**
 *	fbmem_init - init frame buffer subsystem
 *
 *	Initialize the frame buffer subsystem.
 *
 *	NOTE: This function is _only_ to be called by drivers/char/mem.c.
 *
 */

int __init
fbmem_init(void)
{
	create_proc_read_entry("fb", 0, NULL, fbmem_read_proc, NULL);

	devfs_mk_dir("fb");
	if (register_chrdev(FB_MAJOR,"fb",&fb_fops))
		printk("unable to get major %d for fb devs\n", FB_MAJOR);

	fb_class = class_simple_create(THIS_MODULE, "graphics");
	if (IS_ERR(fb_class)) {
		printk(KERN_WARNING "Unable to create fb class; errno = %ld\n", PTR_ERR(fb_class));
		fb_class = NULL;
	}
	return 0;
}
module_init(fbmem_init);

#define NR_FB_DRIVERS 64
static char *video_options[NR_FB_DRIVERS];
static int ofonly;

/**
 * fb_get_options - get kernel boot parameters
 * @name - framebuffer name as it would appear in
 *         the boot parameter line
 *         (video=<name>:<options>)
 *
 * NOTE: Needed to maintain backwards compatibility
 */
int fb_get_options(char *name, char **option)
{
	char *opt, *options = NULL;
	int opt_len, retval = 0;
	int name_len = strlen(name), i;

	if (name_len && ofonly && strncmp(name, "offb", 4))
		retval = 1;

	if (name_len && !retval) {
		for (i = 0; i < NR_FB_DRIVERS; i++) {
			if (video_options[i] == NULL)
				continue;
			opt_len = strlen(video_options[i]);
			if (!opt_len)
				continue;
			opt = video_options[i];
			if (!strncmp(name, opt, name_len) &&
			    opt[name_len] == ':')
				options = opt + name_len + 1;
		}
	}
	if (options && !strncmp(options, "off", 3))
		retval = 1;

	if (option)
		*option = options;

	return retval;
}

/**
 *	video_setup - process command line options
 *	@options: string of options
 *
 *	Process command line options for frame buffer subsystem.
 *
 *	NOTE: This function is a __setup and __init function.
 *            It only stores the options.  Drivers have to call
 *            fb_get_options() as necessary.
 *
 *	Returns zero.
 *
 */

int __init video_setup(char *options)
{
	int i;

	if (!options || !*options)
		return 0;

	for (i = 0; i < NR_FB_DRIVERS; i++) {
		if (!strncmp(options, "ofonly", 6))
			ofonly = 1;
		if (video_options[i] == NULL) {
			video_options[i] = options;
			break;
		}
	}

	return 0;
}
__setup("video=", video_setup);

    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(register_framebuffer);
EXPORT_SYMBOL(unregister_framebuffer);
EXPORT_SYMBOL(num_registered_fb);
EXPORT_SYMBOL(registered_fb);
EXPORT_SYMBOL(fb_prepare_logo);
EXPORT_SYMBOL(fb_show_logo);
EXPORT_SYMBOL(fb_set_var);
EXPORT_SYMBOL(fb_blank);
EXPORT_SYMBOL(fb_pan_display);
EXPORT_SYMBOL(fb_get_buffer_offset);
EXPORT_SYMBOL(fb_iomove_buf_unaligned);
EXPORT_SYMBOL(fb_iomove_buf_aligned);
EXPORT_SYMBOL(fb_sysmove_buf_unaligned);
EXPORT_SYMBOL(fb_sysmove_buf_aligned);
EXPORT_SYMBOL(fb_load_cursor_image);
EXPORT_SYMBOL(fb_set_suspend);
EXPORT_SYMBOL(fb_register_client);
EXPORT_SYMBOL(fb_unregister_client);
EXPORT_SYMBOL(fb_get_options);

MODULE_LICENSE("GPL");
