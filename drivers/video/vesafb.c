/*
 * framebuffer driver for VBE 2.0 compliant graphic boards
 *
 * switching to graphics mode happens at boot time (while
 * running in real mode, see arch/i386/boot/video.S).
 *
 * (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/ioport.h>
#include <linux/init.h>
#ifdef __i386__
#include <video/edid.h>
#endif
#include <asm/io.h>
#include <asm/mtrr.h>

#define dac_reg	(0x3c8)
#define dac_val	(0x3c9)

/* --------------------------------------------------------------------- */

static struct fb_var_screeninfo vesafb_defined __initdata = {
	.activate	= FB_ACTIVATE_NOW,
	.height		= -1,
	.width		= -1,
	.right_margin	= 32,
	.upper_margin	= 16,
	.lower_margin	= 4,
	.vsync_len	= 4,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo vesafb_fix __initdata = {
	.id	= "VESA VGA",
	.type	= FB_TYPE_PACKED_PIXELS,
	.accel	= FB_ACCEL_NONE,
};

static int             inverse   = 0;
static int             mtrr      = 1;
static int	       vram __initdata = 0; /* Set amount of memory to be used */
static int             pmi_setpal = 0;	/* pmi for palette changes ??? */
static int             ypan       = 0;  /* 0..nothing, 1..ypan, 2..ywrap */
static unsigned short  *pmi_base  = NULL;
static void            (*pmi_start)(void);
static void            (*pmi_pal)(void);

/* --------------------------------------------------------------------- */

static int vesafb_pan_display(struct fb_var_screeninfo *var,
                              struct fb_info *info)
{
#ifdef __i386__
	int offset;

	if (!ypan)
		return -EINVAL;
	if (var->xoffset)
		return -EINVAL;
	if (var->yoffset > var->yres_virtual)
		return -EINVAL;
	if ((ypan==1) && var->yoffset+var->yres > var->yres_virtual)
		return -EINVAL;

	offset = (var->yoffset * info->fix.line_length + var->xoffset) / 4;

        __asm__ __volatile__(
                "call *(%%edi)"
                : /* no return value */
                : "a" (0x4f07),         /* EAX */
                  "b" (0),              /* EBX */
                  "c" (offset),         /* ECX */
                  "d" (offset >> 16),   /* EDX */
                  "D" (&pmi_start));    /* EDI */
#endif
	return 0;
}

static void vesa_setpalette(int regno, unsigned red, unsigned green,
			    unsigned blue, struct fb_var_screeninfo *var)
{
#ifdef __i386__
	struct { u_char blue, green, red, pad; } entry;
	int shift = 16 - var->green.length;

	if (pmi_setpal) {
		entry.red   = red   >> shift;
		entry.green = green >> shift;
		entry.blue  = blue  >> shift;
		entry.pad   = 0;
	        __asm__ __volatile__(
                "call *(%%esi)"
                : /* no return value */
                : "a" (0x4f09),         /* EAX */
                  "b" (0),              /* EBX */
                  "c" (1),              /* ECX */
                  "d" (regno),          /* EDX */
                  "D" (&entry),         /* EDI */
                  "S" (&pmi_pal));      /* ESI */
	} else {
		/* without protected mode interface, try VGA registers... */
		outb_p(regno,       dac_reg);
		outb_p(red   >> shift, dac_val);
		outb_p(green >> shift, dac_val);
		outb_p(blue  >> shift, dac_val);
	}
#endif
}

static int vesafb_setcolreg(unsigned regno, unsigned red, unsigned green,
			    unsigned blue, unsigned transp,
			    struct fb_info *info)
{
	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */
	
	if (regno >= info->cmap.len)
		return 1;

	switch (info->var.bits_per_pixel) {
	case 8:
		vesa_setpalette(regno,red,green,blue, &info->var);
		break;
	case 16:
		if (info->var.red.offset == 10) {
			/* 1:5:5:5 */
			((u32*) (info->pseudo_palette))[regno] =	
					((red   & 0xf800) >>  1) |
					((green & 0xf800) >>  6) |
					((blue  & 0xf800) >> 11);
		} else {
			/* 0:5:6:5 */
			((u32*) (info->pseudo_palette))[regno] =	
					((red   & 0xf800)      ) |
					((green & 0xfc00) >>  5) |
					((blue  & 0xf800) >> 11);
		}
		break;
	case 24:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		((u32 *)(info->pseudo_palette))[regno] =
			(red   << info->var.red.offset)   |
			(green << info->var.green.offset) |
			(blue  << info->var.blue.offset);
		break;
	case 32:
		red   >>= 8;
		green >>= 8;
		blue  >>= 8;
		((u32 *)(info->pseudo_palette))[regno] =
			(red   << info->var.red.offset)   |
			(green << info->var.green.offset) |
			(blue  << info->var.blue.offset);
		break;
    }
    return 0;
}

static struct fb_ops vesafb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= vesafb_setcolreg,
	.fb_pan_display	= vesafb_pan_display,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_cursor	= soft_cursor,

#ifdef CONFIG_FB_UI
        /* Assign routines from fbui.c */
        .fb_release             = fbui_release,
        .fb_point               = fb_point,
        .fb_hline               = fb_hline,
        .fb_vline               = fb_vline,
        .fb_clear               = fb_clear,
        .fb_read_point          = fb_read_point,
        .fb_putpixels_native    = fb_putpixels_native,
        .fb_putpixels_rgb       = fb_putpixels_rgb,
        .fb_putpixels_rgb3      = fb_putpixels_rgb3,
        .fb_getpixels_rgb       = fb_getpixels_rgb,
        .fb_copyarea2           = fb_copyarea,
#endif
};

int __init vesafb_setup(char *options)
{
	char *this_opt;
	
	if (!options || !*options)
		return 0;
	
	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt) continue;
		
		if (! strcmp(this_opt, "inverse"))
			inverse=1;
		else if (! strcmp(this_opt, "redraw"))
			ypan=0;
		else if (! strcmp(this_opt, "ypan"))
			ypan=1;
		else if (! strcmp(this_opt, "ywrap"))
			ypan=2;
		else if (! strcmp(this_opt, "vgapal"))
			pmi_setpal=0;
		else if (! strcmp(this_opt, "pmipal"))
			pmi_setpal=1;
		else if (! strcmp(this_opt, "mtrr"))
			mtrr=1;
		else if (! strcmp(this_opt, "nomtrr"))
			mtrr=0;
		else if (! strncmp(this_opt, "vram:", 5))
			vram = simple_strtoul(this_opt+5, NULL, 0);
	}
	return 0;
}

static int __init vesafb_probe(struct device *device)
{
	struct platform_device *dev = to_platform_device(device);
	struct fb_info *info;
	int i, err;

	if (screen_info.orig_video_isVGA != VIDEO_TYPE_VLFB)
		return -ENXIO;

	vesafb_fix.smem_start = screen_info.lfb_base;
	vesafb_defined.bits_per_pixel = screen_info.lfb_depth;
	if (15 == vesafb_defined.bits_per_pixel)
		vesafb_defined.bits_per_pixel = 16;
	vesafb_defined.xres = screen_info.lfb_width;
	vesafb_defined.yres = screen_info.lfb_height;
	vesafb_fix.line_length = screen_info.lfb_linelength;

	/* Allocate enough memory for double buffering */
	vesafb_fix.smem_len = screen_info.lfb_width * screen_info.lfb_height * vesafb_defined.bits_per_pixel >> 2;

	/* check that we don't remap more memory than old cards have */
	if (vesafb_fix.smem_len > (screen_info.lfb_size * 65536))
		vesafb_fix.smem_len = screen_info.lfb_size * 65536;

	/* Set video size according to vram boot option */
	if (vram)
		vesafb_fix.smem_len = vram * 1024 * 1024;

	vesafb_fix.visual   = (vesafb_defined.bits_per_pixel == 8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;

	/* limit framebuffer size to 16 MB.  Otherwise we'll eat tons of
	 * kernel address space for nothing if the gfx card has alot of
	 * memory (>= 128 MB isn't uncommon these days ...) */
	if (vesafb_fix.smem_len > 16 * 1024 * 1024)
		vesafb_fix.smem_len = 16 * 1024 * 1024;

#ifndef __i386__
	screen_info.vesapm_seg = 0;
#endif

	if (!request_mem_region(vesafb_fix.smem_start, vesafb_fix.smem_len, "vesafb")) {
		printk(KERN_WARNING
		       "vesafb: abort, cannot reserve video memory at 0x%lx\n",
			vesafb_fix.smem_start);
		/* We cannot make this fatal. Sometimes this comes from magic
		   spaces our resource handlers simply don't know about */
	}

	info = framebuffer_alloc(sizeof(u32) * 256, &dev->dev);
	if (!info) {
		release_mem_region(vesafb_fix.smem_start, vesafb_fix.smem_len);
		return -ENOMEM;
	}
	info->pseudo_palette = info->par;
	info->par = NULL;

        info->screen_base = ioremap(vesafb_fix.smem_start, vesafb_fix.smem_len);
	if (!info->screen_base) {
		printk(KERN_ERR
		       "vesafb: abort, cannot ioremap video memory 0x%x @ 0x%lx\n",
			vesafb_fix.smem_len, vesafb_fix.smem_start);
		err = -EIO;
		goto err;
	}

	printk(KERN_INFO "vesafb: framebuffer at 0x%lx, mapped to 0x%p, size %dk\n",
	       vesafb_fix.smem_start, info->screen_base, vesafb_fix.smem_len/1024);
	printk(KERN_INFO "vesafb: mode is %dx%dx%d, linelength=%d, pages=%d\n",
	       vesafb_defined.xres, vesafb_defined.yres, vesafb_defined.bits_per_pixel, vesafb_fix.line_length, screen_info.pages);

	if (screen_info.vesapm_seg) {
		printk(KERN_INFO "vesafb: protected mode interface info at %04x:%04x\n",
		       screen_info.vesapm_seg,screen_info.vesapm_off);
	}

	if (screen_info.vesapm_seg < 0xc000)
		ypan = pmi_setpal = 0; /* not available or some DOS TSR ... */

	if (ypan || pmi_setpal) {
		pmi_base  = (unsigned short*)phys_to_virt(((unsigned long)screen_info.vesapm_seg << 4) + screen_info.vesapm_off);
		pmi_start = (void*)((char*)pmi_base + pmi_base[1]);
		pmi_pal   = (void*)((char*)pmi_base + pmi_base[2]);
		printk(KERN_INFO "vesafb: pmi: set display start = %p, set palette = %p\n",pmi_start,pmi_pal);
		if (pmi_base[3]) {
			printk(KERN_INFO "vesafb: pmi: ports = ");
				for (i = pmi_base[3]/2; pmi_base[i] != 0xffff; i++)
					printk("%x ",pmi_base[i]);
			printk("\n");
			if (pmi_base[i] != 0xffff) {
				/*
				 * memory areas not supported (yet?)
				 *
				 * Rules are: we have to set up a descriptor for the requested
				 * memory area and pass it in the ES register to the BIOS function.
				 */
				printk(KERN_INFO "vesafb: can't handle memory requests, pmi disabled\n");
				ypan = pmi_setpal = 0;
			}
		}
	}

	vesafb_defined.xres_virtual = vesafb_defined.xres;
	vesafb_defined.yres_virtual = vesafb_fix.smem_len / vesafb_fix.line_length;
	if (ypan && vesafb_defined.yres_virtual > vesafb_defined.yres) {
		printk(KERN_INFO "vesafb: scrolling: %s using protected mode interface, yres_virtual=%d\n",
		       (ypan > 1) ? "ywrap" : "ypan",vesafb_defined.yres_virtual);
	} else {
		printk(KERN_INFO "vesafb: scrolling: redraw\n");
		vesafb_defined.yres_virtual = vesafb_defined.yres;
		ypan = 0;
	}

	/* some dummy values for timing to make fbset happy */
	vesafb_defined.pixclock     = 10000000 / vesafb_defined.xres * 1000 / vesafb_defined.yres;
	vesafb_defined.left_margin  = (vesafb_defined.xres / 8) & 0xf8;
	vesafb_defined.hsync_len    = (vesafb_defined.xres / 8) & 0xf8;
	
	vesafb_defined.red.offset    = screen_info.red_pos;
	vesafb_defined.red.length    = screen_info.red_size;
	vesafb_defined.green.offset  = screen_info.green_pos;
	vesafb_defined.green.length  = screen_info.green_size;
	vesafb_defined.blue.offset   = screen_info.blue_pos;
	vesafb_defined.blue.length   = screen_info.blue_size;
	vesafb_defined.transp.offset = screen_info.rsvd_pos;
	vesafb_defined.transp.length = screen_info.rsvd_size;
	printk(KERN_INFO "vesafb: %s: "
	       "size=%d:%d:%d:%d, shift=%d:%d:%d:%d\n",
	       (vesafb_defined.bits_per_pixel > 8) ?
	       "Truecolor" : "Pseudocolor",
	       screen_info.rsvd_size,
	       screen_info.red_size,
	       screen_info.green_size,
	       screen_info.blue_size,
	       screen_info.rsvd_pos,
	       screen_info.red_pos,
	       screen_info.green_pos,
	       screen_info.blue_pos);

	vesafb_fix.ypanstep  = ypan     ? 1 : 0;
	vesafb_fix.ywrapstep = (ypan>1) ? 1 : 0;

	/* request failure does not faze us, as vgacon probably has this
	 * region already (FIXME) */
	request_region(0x3c0, 32, "vesafb");

	if (mtrr) {
		int temp_size = vesafb_fix.smem_len;
		/* Find the largest power-of-two */
		while (temp_size & (temp_size - 1))
                	temp_size &= (temp_size - 1);
                        
                /* Try and find a power of two to add */
		while (temp_size && mtrr_add(vesafb_fix.smem_start, temp_size, MTRR_TYPE_WRCOMB, 1)==-EINVAL) {
			temp_size >>= 1;
		}
	}
	
	info->fbops = &vesafb_ops;
	info->var = vesafb_defined;
	info->fix = vesafb_fix;
	info->flags = FBINFO_FLAG_DEFAULT |
		(ypan) ? FBINFO_HWACCEL_YPAN : 0;

	if (fb_alloc_cmap(&info->cmap, 256, 0) < 0) {
		err = -ENXIO;
		goto err;
	}
	if (register_framebuffer(info)<0) {
		err = -EINVAL;
		fb_dealloc_cmap(&info->cmap);
		goto err;
	}
	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       info->node, info->fix.id);

#ifdef CONFIG_FB_UI
        fbui_init (info);
#endif

	return 0;
err:
	framebuffer_release(info);
	release_mem_region(vesafb_fix.smem_start, vesafb_fix.smem_len);
	return err;
}

static struct device_driver vesafb_driver = {
	.name	= "vesafb",
	.bus	= &platform_bus_type,
	.probe	= vesafb_probe,
};

static struct platform_device vesafb_device = {
	.name	= "vesafb",
};

int __init vesafb_init(void)
{
	int ret;
	char *option = NULL;

	/* ignore error return of fb_get_options */
	fb_get_options("vesafb", &option);
	vesafb_setup(option);
	ret = driver_register(&vesafb_driver);

	if (!ret) {
		ret = platform_device_register(&vesafb_device);
		if (ret)
			driver_unregister(&vesafb_driver);
	}
	return ret;
}
module_init(vesafb_init);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

MODULE_LICENSE("GPL");
