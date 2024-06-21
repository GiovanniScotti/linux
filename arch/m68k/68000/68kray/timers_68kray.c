#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clocksource.h>
#include <asm/setup.h>
#include <asm/machdep.h>

#include <asm/68kray.h>


static u32 tick_cnt_68kray;

void hw_timer_init_68kray(void);


static u64 read_clk_68kray(struct clocksource *cs)
{
	unsigned long flags;
	u32 cycles;

	local_irq_save(flags);
	cycles = tick_cnt_68kray;
	local_irq_restore(flags);

	return cycles;
}

static struct clocksource clk_68kray = {
	.name	= "timer",
	.rating	= 250,
	.read	= read_clk_68kray,
	.mask	= CLOCKSOURCE_MASK(32),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static irqreturn_t hw_tick(int irq, void *dummy)
{
	/* Reset timer */
    u8 ora = VIA_ORA_REG;

	VIA_ORA_REG = ora & 0x7F; // PA7 = 0 reset timer
	tick_cnt_68kray += 1;
	legacy_timer_tick(1);
	VIA_ORA_REG = ora | 0x80; // PA7 = 1 restart timer

	return IRQ_HANDLED;
}

void hw_timer_init_68kray(void)
{
    u8 ora;
    int ret;

    /* Initialize VIA */
    ora = VIA_ORA_REG;
    VIA_DDRA_REG = (u8)0xFF;  // PA0-7 outputs
    VIA_ORA_REG = ora & 0x7F; // PA7 keeps timer in reset state

    /* Set ISR */
    pr_info("Requested IRQ %d for timer\n", TIMER_IRQ);
    ret = request_irq(TIMER_IRQ, hw_tick, IRQF_TIMER, "timer", NULL);
    if (ret) {
        pr_err("Failed to request IRQ %d (timer): %pe\n", TIMER_IRQ,
            ERR_PTR(ret));
    }

	/* Enable timer by deasserting timer reset */
    VIA_ORA_REG = ora | 0x80; // PA7 = 1
	clocksource_register_hz(&clk_68kray, FREQ_HZ);
}
