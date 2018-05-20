// mz8023yt@163.com 20180520 begin >>> [1/2] realize the mini2440 x35 lcd driver

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/types.h>
#include <asm/mach/map.h>

#define u32    unsigned int

struct lcd_regs
{
        unsigned long lcdcon1;
        unsigned long lcdcon2;
        unsigned long lcdcon3;
        unsigned long lcdcon4;
        unsigned long lcdcon5;
        unsigned long lcdsaddr1;
        unsigned long lcdsaddr2;
        unsigned long lcdsaddr3;
        unsigned long redlut;
        unsigned long greenlut;
        unsigned long bluelut;
        unsigned long reserved[9];
        unsigned long dithmode;
        unsigned long tpal;
        unsigned long lcdintpnd;
        unsigned long lcdsrcpnd;
        unsigned long lcdintmsk;
        unsigned long lpcsel;
};

static struct fb_info *mini2440_x35_fb;
static u32 pseudo_palette[16];

static volatile struct lcd_regs *lcd_regs_base;
static volatile unsigned long *gpbcon;
static volatile unsigned long *gpbdat;
static volatile unsigned long *gpccon;
static volatile unsigned long *gpdcon;
static volatile unsigned long *gpgcon;

static inline u32 chan_to_field(u32 chan, struct fb_bitfield* bf)
{
        chan &= 0xffff;
        chan >>= 16 - bf->length;
        return chan << bf->offset;
}

/* 设置颜色 */
static int mini2440_x35_setcolreg(u32 regno, u32 red, u32 green, u32 blue, u32 transp, struct fb_info *info)
{
        u32 val;
        if(regno > 16)
                return 1;
        val = chan_to_field(red, &info->var.red);
        val |= chan_to_field(green, &info->var.green);
        val |= chan_to_field(blue, &info->var.blue);
        pseudo_palette[regno] = val;
        return 0;
}

static struct fb_ops mini2440_x35_ops = {
.       owner           = THIS_MODULE,
        .fb_setcolreg   = mini2440_x35_setcolreg,
        .fb_fillrect    = cfb_fillrect,
        .fb_copyarea    = cfb_copyarea,
        .fb_imageblit   = cfb_imageblit,
};

static int mini2440_x35_lcd_init(void)
{
        /* 分配一个fb_info */
        mini2440_x35_fb = framebuffer_alloc(0, NULL);

        /* 2 填充fb_info结构 */
        /* 2.1 设置固定参数 */
        strcpy(mini2440_x35_fb->fix.id, "mini2440_x35_lcd");

        /* 计算 frambuffer 的大小 */
        mini2440_x35_fb->fix.smem_len = 240 * 320 * 32 / 8;

        /* 设置扫描方式为：以像素点扫描，非隔行扫描 */
        mini2440_x35_fb->fix.type = FB_TYPE_PACKED_PIXELS;

        /* 设置色阶为：真彩色 */
        mini2440_x35_fb->fix.visual = FB_VISUAL_TRUECOLOR;
        mini2440_x35_fb->fix.line_length = 240 * 4;

        /* 2.2 设置可变参数 */
        mini2440_x35_fb->var.xres = 240;
        mini2440_x35_fb->var.yres = 320;
        mini2440_x35_fb->var.xres_virtual = 240;
        mini2440_x35_fb->var.yres_virtual = 320;

        /* 设置每个像素点占有的字节数为：32个字节 */
        mini2440_x35_fb->var.bits_per_pixel = 32;

        /* 颜色放置的方式，这里是真彩色为 8:8:8，每种颜色占用8位 */
        mini2440_x35_fb->var.red.offset = 16;
        mini2440_x35_fb->var.red.length = 8;
        mini2440_x35_fb->var.blue.offset = 8;
        mini2440_x35_fb->var.blue.length = 8;
        mini2440_x35_fb->var.green.offset = 0;
        mini2440_x35_fb->var.green.length = 8;

        /* 设置颜色立即生效 */
        mini2440_x35_fb->var.activate = FB_ACTIVATE_NOW;

        /* 2.3 设置操作函数 */
        mini2440_x35_fb->fbops = &mini2440_x35_ops;

        /* 2.4 其他的设置 */
        /* 调色板 */
        mini2440_x35_fb->pseudo_palette = pseudo_palette;
        /* 屏幕尺寸的大小 */
        mini2440_x35_fb->screen_size = 240 * 320 * 32 / 8;

        /* 3. 设置硬件，驱动lcd */
        /* 设置 lcd 引脚可用 */
        gpbcon = ioremap(0x56000010, 8);
        gpbdat = gpbcon + 1;
        gpccon = ioremap(0x5600020, 4);
        gpdcon = ioremap(0x5600030, 4);
        gpgcon = ioremap(0x5600060, 4);
        *gpccon = 0xaaaaaaaa;
        *gpdcon = 0xaaaaaaaa;

        /* lcd_pwren lcd电源引脚 */
        *gpgcon |= (0x3 << (4 * 2));

        /* 设置lcd控制器寄存器 */
        lcd_regs_base = ioremap(0x4d000000, sizeof(struct lcd_regs));
        lcd_regs_base->lcdcon1 = (9 << 8) | (3 << 5) | (0xd << 1);
        lcd_regs_base->lcdcon2 = (8 << 24) | (319 << 14) | (4 << 6) | (9 << 0);
        lcd_regs_base->lcdcon3 = (15 << 19) | (239 << 8) | (16 << 0);
        lcd_regs_base->lcdcon4 = 5;
        lcd_regs_base->lcdcon5 = (0 << 12) | (0 << 10) | (1 << 6) | (0 << 1) | (0 << 0);

        /* 分配显存，将显存物理地址 告诉 framebuffer mini2440_x35_fb->fix.smem_start = xxx; */
        mini2440_x35_fb->screen_base = dma_alloc_writecombine(NULL, mini2440_x35_fb->fix.smem_len, &(mini2440_x35_fb->fix.smem_start), GFP_KERNEL);
        lcd_regs_base->lcdsaddr1 = (mini2440_x35_fb->fix.smem_start >> 1) & (~(3 << 30));
        lcd_regs_base->lcdsaddr2 = ((mini2440_x35_fb->fix.smem_start + mini2440_x35_fb->fix.smem_len) >> 1) & 0x1fffff;
        lcd_regs_base->lcdsaddr3 = (0 << 11) | ((240 * 32 / 16) << 0);

        /* 4. 启动 lcd */
        lcd_regs_base->lcdcon1 |= (1 << 0);
        lcd_regs_base->lcdcon5 |= (1 << 3);
        /* MINI2440的背光电路也是通过 LCD_PWREN 来控制的, 不需要单独的背光引脚 */

        /* 5. 注册framebuffer */
        register_framebuffer(mini2440_x35_fb);

        return 0;
}

static void mini2440_x35_lcd_exit(void)
{
        /* 注销framebuffer结构，释放分配的 frambuffer 显存 */
        unregister_framebuffer(mini2440_x35_fb);

        /* 关闭lcd控制器以及电源 */
        lcd_regs_base->lcdcon1 &= ~(1 << 0);
        lcd_regs_base->lcdcon5 &= ~(1 << 3);

        dma_free_writecombine(NULL, mini2440_x35_fb->fix.smem_len, mini2440_x35_fb->screen_base, mini2440_x35_fb->fix.smem_start);

        /* 取消映射地址 */
        iounmap(lcd_regs_base);
        iounmap(gpccon);
        iounmap(gpdcon);
        iounmap(gpgcon);

        /* 释放framebuffer结构 */
        framebuffer_release(mini2440_x35_fb);
}

module_init(mini2440_x35_lcd_init);
module_exit(mini2440_x35_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LCL");

// mz8023yt@163.com 20180520 end   <<< [1/2] realize the mini2440 x35 lcd driver
