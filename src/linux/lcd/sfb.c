// -----------------------------------------------------------------------------
// Simple Frame Buffer driver                                             sfb.c 
// This is a simple frame buffer driver thatn can be used with an FPGA type
// LCD controller where the display size and other variables are basically 
// fixed, so no control registers are needed. 
// -----------------------------------------------------------------------------
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include "sfb.h"

// -----------------------------------------------------------------------------


// Global Variables ------------------------------------------------------------
static unsigned long videomemory     = SFB_VIDEOMEMSTART;
static unsigned long videomemorysize = SFB_VIDEOMEMSIZE;
static struct fb_info fb_info;
static u32 pseudo_palette[16];

#define SFB_MAX_PALLETE_REG 16

// -----------------------------------------------------------------------------
// Set up "variable" screeninfo
// -----------------------------------------------------------------------------
static struct fb_var_screeninfo sfb_var __initdata = {
    .xres           = SFB_MAX_X,        // Visual  resolution
    .yres           = SFB_MAX_Y,        // Visual  resolution
    .xres_virtual   = SFB_MAX_X,        // Virtual resolution
    .yres_virtual   = SFB_MAX_Y,        // Virtual resolution
    .xoffset        = 0,		// offset from virtual to visible 
    .yoffset        = 0,		// resolution			
    .bits_per_pixel = 16,
    .red            = {  0, 5, 0 },     // offset, length, transp offset
    .green          = {  5, 6, 0 },
    .blue           = { 11, 5, 0 },
    .nonstd         = 0,		// != 0 Non standard pixel format 
    .grayscale      = 0,
    .activate       = FB_ACTIVATE_NOW,  // set values immediately (or vbl)
    .height         = LCD_HEIGHT,
    .width          = LCD_WIDTH,
    .left_margin    = 0,
    .right_margin   = 384,
    .upper_margin   = 0,
    .lower_margin   = 0,
    .vmode          = FB_VMODE_NONINTERLACED,
};

// -----------------------------------------------------------------------------
// Set up "fixed" screeninfo
// -----------------------------------------------------------------------------
static struct fb_fix_screeninfo sfb_fix __initdata = {
    .id          = "SimpleFB",
    .type        = FB_TYPE_PACKED_PIXELS,
    .visual      = FB_VISUAL_TRUECOLOR,
    .smem_start  = SFB_VIDEOMEMSTART, // Start of frame buffer mem, (physical address) 
    .smem_len    = SFB_VIDEOMEMSIZE,  // Length of frame buffer mem
    .mmio_start  = SFB_VIDEOMEMSTART, // Start of Memory Mapped I/O, (physical address)
    .mmio_len    = SFB_VIDEOMEMSIZE,  // Length of Memory Mapped I/O
    .line_length = LCD_WIDTH*2,
    .xpanstep    = 0,
    .ypanstep    = 0,
    .ywrapstep   = 0,
    .accel       = FB_ACCEL_NONE,
};

// -----------------------------------------------------------------------------
// Function prototypes 
// -----------------------------------------------------------------------------
int            sfb_init(void);
int            sfb_setup(char *);
static int     sfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int     sfb_set_par(struct fb_info *info);
static ssize_t sfb_read(struct fb_info *info, char *buf, size_t count, loff_t * ppos);
static ssize_t sfb_write(struct fb_info *info, const char *buf, size_t count, loff_t * ppos);
static int     sfb_setcolreg(unsigned regno, unsigned r, unsigned g, unsigned b, unsigned transp, struct fb_info *info);
static int     sfb_blank(int blank_mode, struct fb_info *info);

// -----------------------------------------------------------------------------
// Define fb_ops structure that is registered with the kernel.
// Note: The cfb_xxx routines, these are generic routines implemented in kernel.
// You can override them in case your hardware provides accelerated graphics function
// -----------------------------------------------------------------------------
static struct fb_ops sfb_ops = {
    .owner        = THIS_MODULE,

    .fb_check_var = sfb_check_var,     // Check variables
    .fb_set_par   = sfb_set_par,

    .fb_fillrect  = cfb_fillrect,      // use standard functions
    .fb_copyarea  = cfb_copyarea,      // use standard functions
    .fb_imageblit = cfb_imageblit,     // use standard functions

    .fb_read      = sfb_read,          // Special read function
    .fb_write     = sfb_write,         // Special read function

    .fb_setcolreg = sfb_setcolreg,     // Set color register
    .fb_blank     = sfb_blank,         // Blank Display
};

// -----------------------------------------------------------------------------
// For now this is a stub. Later will need to put in code to toggle a
// GPIO line tied to the LCD controller enable line.
// -----------------------------------------------------------------------------
#define GPIO_PIN        11                  // Set Pin Number here
#define LCD_ENABLE_PIN (1 << GPIO_PIN)     // Compute bit mask
static void lcd_enable(int enable)
{
    unsigned long *portptr;
    portptr = ioremap(PIOB_BASE, 1024);    // Initialize AT91 GPIO REG
    if(enable) portptr[PIO_SODR] = LCD_ENABLE_PIN;   // Set bit
    else       portptr[PIO_CODR] = LCD_ENABLE_PIN;   // clear bit
}

// -----------------------------------------------------------------------------
// sfb_blank - blanks the display.
//      @blank_mode: the blank mode we want.
//      @info: frame buffer structure that represents a single frame buffer
//
//  Blank the screen if blank_mode != 0, else unblank. Return 0 if
//    blanking succeeded, != 0 if un-/blanking failed due to e.g. a
//    video mode which doesn't support it. Implements VESA suspend
//    and powerdown modes on hardware that supports disabling hsync/vsync:
//    blank_mode == 2: suspend vsync
//    blank_mode == 3: suspend hsync
//    blank_mode == 4: powerdown
//
//  Returns negative errno on error, or zero on success.
// -----------------------------------------------------------------------------
static int sfb_blank(int blank_mode, struct fb_info *info)
{
    switch(blank_mode) {
        case FB_BLANK_UNBLANK:
        case FB_BLANK_NORMAL:
            lcd_enable(1);
            break;
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_HSYNC_SUSPEND:
            lcd_enable(0);
            break;
        case FB_BLANK_POWERDOWN:
            lcd_enable(0);
            break;
        default:
            return(-EINVAL);
    }
    // let fbcon do a soft blank for us
    return(blank_mode == FB_BLANK_NORMAL) ? 1 : 0;
}

// -----------------------------------------------------------------------------
// sfb_setcolreg - sets a color register.
//     regno:  Which register in the CLUT we are programming
//     red:    The red value which can be up to 16 bits wide
//     green:  The green value which can be up to 16 bits wide
//     blue:   The blue value which can be up to 16 bits wide.
//     transp: If supported the alpha value which can be up to 16 bits wide.
//     info:   frame buffer info structure
//
// Returns negative errno on error, or zero on success.
// -----------------------------------------------------------------------------
static int sfb_setcolreg(unsigned regno, unsigned r, unsigned g,unsigned b, unsigned transp, struct fb_info *info)
{
    if(regno >= SFB_MAX_PALLETE_REG) return(1); //no. of hw registers

//  ((u32 *) info->pseudo_palette)[regno]  = (r  << info->var.red.offset) | (g      << info->var.green.offset) |
//                                           (b << info->var.blue.offset) | (transp << info->var.transp.offset);

    ((u32 *) info->pseudo_palette)[regno] = (r & 0xf800) | (g & 0xfc00) >> 5 | (b & 0xf800) >> 11;
    return(0);
}

// -----------------------------------------------------------------------------
// By reading and writing along the video DRAM row and column boundaries
// it speeds up the graphics by 50%
// -----------------------------------------------------------------------------
#define TRANSLATE_ADDRESS  1
#if TRANSLATE_ADDRESS
#define TRANSLATE(a)       (((((unsigned long)a)/1280)&0x01FF)<<11) | ((((unsigned long)a)%1280)&0x07FF); 
#else
#define TRANSLATE(a)       (unsigned long)(a)
#endif
// -----------------------------------------------------------------------------
static inline void sfb_fb_writew(u16 v, void *base, void *a)
{
    u16 *p;
#if TRANSLATE_ADDRESS
    unsigned long m = ((unsigned long)a);
    m = (((m/1280)&0x01FF)<<11) | ((m%1280)&0x07FF);
    p = (u16 *)(m + (unsigned long)base);
#else
    p = (u16 *)(TRANSLATE(a) + (unsigned long)base);
#endif
    fb_writew(v, p);
}
static inline void sfb_fb_writeb(u8 v, void *base, void *a)
{
    u16 *p;

#if TRANSLATE_ADDRESS
    unsigned long m = (unsigned long)a;
    m = (((m/1280)&0x01FF)<<11) | ((m%1280)&0x07FF);
    p = (u16 *)(m + (unsigned long)base);
#else
    p = (u16 *)(TRANSLATE(a) + (unsigned long)base);
#endif

    fb_writeb(v, p);
}
static inline u16 sfb_fb_readw(void *base, void *a)
{
    u16 *p;

#if TRANSLATE_ADDRESS
    unsigned long m = (unsigned long)a;
    m = (((m/1280)&0x01FF)<<11) | ((m%1280)&0x07FF);
    p = (u16 *)(m + (unsigned long)base);
#else
    p = (u16 *)(TRANSLATE(a) + (unsigned long)base);
#endif

    return(fb_readw(p));
}
static inline u8 sfb_fb_readb(void *base, void *a)
{
    u16 *p;

#if TRANSLATE_ADDRESS
    unsigned long m = (unsigned long)a;
    m = (((m/1280)&0x01FF)<<11) | ((m%1280)&0x07FF);
    p = (u16 *)(m + (unsigned long)base);
#else
    p = (u16 *)(TRANSLATE(a) + (unsigned long)base);
#endif
    return(fb_readb(p));
}

static inline unsigned long copy_from_user16(void *base, void *to, void *from, unsigned long n)
{
    u16 *dst = (u16 *) to;
    u16 *src = (u16 *) from;

    if(!access_ok(VERIFY_READ, from, n)) return(n);

    while(n > 1) {
       u16 v;
       if(__get_user(v, src)) return(n);
       sfb_fb_writew(v, base, dst);
       src++, dst++;
       n -= 2;
    }
    if(n) {
        u8 v;
        if(__get_user(v, ((u8 *) src))) return(n);
        sfb_fb_writeb(v, base, dst);
    }
    return(0);
}

static inline unsigned long copy_to_user16(void *base, void *to, void *from, unsigned long n)
{
    u16 *dst = (u16 *) to;
    u16 *src = (u16 *) from;

    if(!access_ok(VERIFY_WRITE, to, n)) return(n);

    while(n > 1) {
        u16 v = sfb_fb_readw(base, src);
        if(__put_user(v, dst)) return(n);
        src++, dst++;
        n -= 2;
    }
    if(n) {
        u8 v = sfb_fb_readb(base, src);
        if(__put_user(v, ((u8 *) dst))) return(n);
    }
    return(0);
}


// -----------------------------------------------------------------------------
// By reading and writing along the video DRAM row and column boundaries
// it speeds up the graphics by 50%
// -----------------------------------------------------------------------------
static ssize_t sfb_read(struct fb_info *info, char *buf, size_t count, loff_t * ppos)
{
    unsigned long p = *ppos;

    if(p >= info->fix.smem_len)        return 0;
    if(count >= info->fix.smem_len)    count = info->fix.smem_len;
    if(count + p > info->fix.smem_len) count = info->fix.smem_len - p;

    if(count) {
        char *base_addr;
        base_addr = info->screen_base;
        count -= copy_to_user16((void *)base_addr, (void *)buf, (void *)p, count);
        if(!count) return -EFAULT;
        *ppos += count;
    }
    return count;
}

// -----------------------------------------------------------------------------
// By reading and writing along the video DRAM row and column boundaries
// it speeds up the graphics by 50%
// -----------------------------------------------------------------------------
static ssize_t sfb_write(struct fb_info *info, const char *buf, size_t count, loff_t * ppos)
{
    unsigned long p = *ppos;   // offset pointer
    int err;

    if(p     >  info->fix.smem_len) return -ENOSPC;
    if(count >= info->fix.smem_len) count = info->fix.smem_len;
    err = 0;
    if(count + p > info->fix.smem_len) {
        count = info->fix.smem_len - p;
        err = -ENOSPC;
    }

    if(count) {
        char *base_addr;
        base_addr = info->screen_base;

        count -= copy_from_user16((void *)base_addr, (void *)p, (void *)buf, count);
        *ppos += count;
        err = -EFAULT;
    }
    if(count) return count;
    return err;
}

// -----------------------------------------------------------------------------
// Recollect the discussion on line_length under the Graphics Hardware section. 
// Line length expressed in bytes, denotes the number of bytes in each line.
// -----------------------------------------------------------------------------
static u_long get_line_length(int xres_virtual, int bpp)
{
    u_long line_length;

    line_length = xres_virtual * bpp;
    line_length = (line_length + 31) & ~31;
    line_length >>= 3;
    return(line_length);
}

// -----------------------------------------------------------------------------
// We only support 16 bits_per_pixel mode since that is hardware specific. 
// -----------------------------------------------------------------------------
static inline int sfb_check_bpp(struct fb_var_screeninfo *var)
{
    if(var->bits_per_pixel == 16) return(0);
    else                          return(-EINVAL);
    
}

// -----------------------------------------------------------------------------
static inline void sfb_fixup_var_modes(struct fb_var_screeninfo *var)
{
    switch(var->bits_per_pixel) {

        case 16:  // RGB565 Mode
            var->red.offset    =  0;
            var->red.length    =  5;
            var->green.offset  =  5;
            var->green.length  =  6;
            var->blue.offset   = 11;
            var->blue.length   =  5;
            var->transp.offset =  0;
            var->transp.length =  0;
            break;

    }
    var->red.msb_right    = 0;
    var->green.msb_right  = 0;
    var->blue.msb_right   = 0;
    var->transp.msb_right = 0;
    var->grayscale        = 0;
    var->height           = LCD_HEIGHT;
    var->width            = LCD_WIDTH;
}

// -----------------------------------------------------------------------------
// sfb_check_var, does not write anything to hardware, only
// verify based on hardware data for validity
// -----------------------------------------------------------------------------
static int sfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    u_long line_length;
    
    if(!var->xres) var->xres = SFB_MIN_X;  // Check for the resolution validity 
    if(!var->yres) var->yres = SFB_MIN_Y;

    if(var->xres > var->xres_virtual) var->xres_virtual = var->xres;
    if(var->yres > var->yres_virtual) var->yres_virtual = var->yres;

    if(sfb_check_bpp(var)) return(-EINVAL);

    if(var->xres_virtual < var->xoffset + var->xres) var->xres_virtual = var->xoffset + var->xres;
    if(var->yres_virtual < var->yoffset + var->yres) var->yres_virtual = var->yoffset + var->yres;

    // ------------------------------------------------------------------------
    // Make sure the card has enough video memory in this mode
    // Recall the formula
    // Fb Memory = Display Width * Display Height * Bytes-per-pixel
    // ------------------------------------------------------------------------
    line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
    if(line_length * var->yres_virtual > videomemorysize) return(-ENOMEM);
    
    sfb_fixup_var_modes(var);
    return(0);
}

// -----------------------------------------------------------------------------
// This routine actually sets the video mode. All validation has
// been done already. In our case, this is a stub
// -----------------------------------------------------------------------------
static int sfb_set_par(struct fb_info *info)
{
    info->fix.line_length = get_line_length(info->var.xres_virtual,info->var.bits_per_pixel);
    return(0);
}

// -----------------------------------------------------------------------------
// Driver Entry point 
// -----------------------------------------------------------------------------
int __init sfb_init(void)
{
    unsigned long *regptr;

    // Initialize AT91 SMC REG--------------------------------------------------
    regptr = ioremap(EBI_BASE, 64);
    regptr[SMC_CSR2] = SMC_BITDEF;     // Register bit definitions Set up NCS2

    if(!request_mem_region(SFB_VIDEOMEMSTART, SFB_VIDEOMEMSIZE, "SFB framebuffer")) {
        printk(KERN_ERR "sfb: unable to reserve framebuffer at 0x%0x\n", (unsigned int)SFB_VIDEOMEMSTART);
        return(-EBUSY);
    }

    // Fill fb_info structure --------------------------------------------------
    fb_info.screen_base = ioremap(videomemory, videomemorysize);
    fb_info.screen_size = SFB_VIDEOMEMSIZE;
    fb_info.fbops       = &sfb_ops;
    fb_info.var         = sfb_var;
    fb_info.fix         = sfb_fix;
//  fb_info.flags       = FBINFO_FLAG_DEFAULT;
    fb_info.flags       = 0x0000;       // Default, no special flags

    fb_info.pseudo_palette = pseudo_palette;

    fb_alloc_cmap(&fb_info.cmap, 256, 0);

    // Register the driver -----------------------------------------------------
    if(register_framebuffer(&fb_info) < 0) return(-EINVAL);

//  printk(KERN_INFO "sfb: getlen=%p\n", get_line_length(fb_info.var.xres_virtual, fb_info.var.bits_per_pixel));

    printk(KERN_INFO "fb%d: Simple frame buffer device initialized \n", fb_info.node);
    return(0);
}

// -----------------------------------------------------------------------------
static void __exit sfb_cleanup(void)
{
    unregister_framebuffer(&fb_info);
}

// -----------------------------------------------------------------------------
module_init(sfb_init);
module_exit(sfb_cleanup);
MODULE_AUTHOR("Donnaware International LLC");
MODULE_DESCRIPTION("Simple Framebuffer driver for FPGA");
MODULE_LICENSE("GPL");

// -----------------------------------------------------------------------------
// end sfb.c 
// -----------------------------------------------------------------------------

