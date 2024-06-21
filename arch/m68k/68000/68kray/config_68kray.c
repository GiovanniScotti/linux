#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/machdep.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/rtc.h>

#include <asm/68kray.h>


void __init platform_early_init_68kray(void);
void __init init_68kray(char *command, int len);


static struct resource m6850uart_res[] = {
	{
		.start	= UART_BASE_ADDR,
		.end	= UART_END_ADDR,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= UART_IRQ,
		.end	= UART_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_uart_68kray m6850uart_platform_data[] = {
	{
		.mapbase	= UART_BASE_ADDR,
		.irq		= UART_IRQ,
	},
};

static struct platform_device m6850_uart = {
	.name				= "m6850uart",
	.id					= -1,
	.num_resources		= 1,
	.resource			= m6850uart_res,
	.dev.platform_data	= m6850uart_platform_data,
};

static struct platform_device *devices_68kray[] __initdata = {
	&m6850_uart,
};


static void boot_console_out(char c)
{
    while (!(UART_STAT_REG & UART_TX_EMPTY));
    UART_TX_REG = (u8)c;
}

static void boot_console_write(struct console *co, const char *s, unsigned int count)
{
	while (count--) {
		if (*s == '\n')
			boot_console_out('\r');
		boot_console_out(*s++);
	}
}

static struct __initdata console early_boot_console = {
	.name = "bootuart",
	.write = boot_console_write,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1
};

void __init platform_early_init_68kray(void)
{
#if defined(CONFIG_SERIAL_68KRAY_CONSOLE)
	register_console(&early_boot_console);
	add_preferred_console("ttyS", 0, "115200");
#endif
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

static int hwclk_68kray(int set, struct rtc_time *t)
{
    if (!set) {
        t->tm_year = 1;
        t->tm_mon = 0;
        t->tm_mday = 1;
        t->tm_hour = 12;
        t->tm_min = 0;
        t->tm_sec = 0;
    }

    return 0;
}

static void reset_68kray(void)
{
	local_irq_disable();

	asm volatile(
		"moveal #0x00000000, %a0;\n\t"
		"moveal 0(%a0), %sp;\n\t"
		"moveal 4(%a0), %a0;\n\t"
		"jmp (%a0);"
	);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

extern void hw_timer_init_68kray(void);

// Called by 68000 config_BSP function.
void __init init_68kray(char *command, int len)
{
	pr_info("68Kray modular computer support by G. Scotti\n");

	mach_sched_init = hw_timer_init_68kray;
	mach_hwclk = hwclk_68kray;
	mach_reset = reset_68kray;

	platform_early_init_68kray();
}

static int __init init_BSP(void)
{
	platform_add_devices(devices_68kray, ARRAY_SIZE(devices_68kray));
	return 0;
}

arch_initcall(init_BSP);
