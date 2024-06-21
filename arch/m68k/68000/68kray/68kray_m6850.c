#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <asm/68kray.h>


#define DRV_NAME				"m6850uart"
#define M6850_SERIAL_DEV_NAME	"ttyS"
#define M6850_SERIAL_MINOR		64

#define PORT_M6850				2000

struct m6850_uart_port {
	struct uart_port port;
	u8 ctrl_reg;
};

static struct m6850_uart_port m6850_uart_ports[1];
#define	M6850_MAXPORTS ARRAY_SIZE(m6850_uart_ports)


#if defined(CONFIG_SERIAL_68KRAY_CONSOLE)
static struct console m6850_console;
#define	M6850_CONSOLE (&m6850_console)
#else
#define	M6850_CONSOLE NULL
#endif


/* UART driver structure. */
static struct uart_driver m6850_driver = {
	.owner			= THIS_MODULE,
	.driver_name	= DRV_NAME,
	.dev_name		= M6850_SERIAL_DEV_NAME,
	.major			= TTY_MAJOR,
	.minor			= M6850_SERIAL_MINOR,
	.nr				= M6850_MAXPORTS,
	.cons			= M6850_CONSOLE,
};


static void m6850_tx_chars(struct m6850_uart_port *pp)
{
	struct uart_port *port = &pp->port;
	struct circ_buf *xmit = &port->state->xmit;

	if (port->x_char) {
		/* Send special char - probably flow control */
		UART_TX_REG = port->x_char;
		port->x_char = 0;
		port->icount.tx++;
		return;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		pp->ctrl_reg &= ~UART_TX_IRQ_EN;
		UART_CTRL_REG = pp->ctrl_reg;
		return;
	}

	while (UART_STAT_REG & UART_TX_EMPTY) {
		UART_TX_REG = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE -1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit)) {
		pp->ctrl_reg &= ~UART_TX_IRQ_EN;
		UART_CTRL_REG = pp->ctrl_reg;
	}

}

static void m6850_rx_chars(struct m6850_uart_port *pp)
{
	struct uart_port *port = &pp->port;
	unsigned char ch, flag;

	while (UART_STAT_REG & (1 << 0)) {	// RX data reg. full
		ch = UART_RX_REG;
		flag = TTY_NORMAL;
		port->icount.rx++;

		if (uart_handle_sysrq_char(port, ch))
			continue;
		//uart_insert_char(&port, status, MCFUART_USR_RXOVERRUN, ch, flag);
		tty_insert_flip_char(&port->state->port, ch, flag);
		//uart_insert_char(&port, status, 0, ch, flag);
	}

	tty_flip_buffer_push(&port->state->port);
}

// Interrupt handler
static irqreturn_t m6850_interrupt(int irq, void *data)
{
	struct uart_port *port = data;
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);
	irqreturn_t ret = IRQ_NONE;

	if (UART_STAT_REG & UART_RX_FULL) {
		m6850_rx_chars(pp);
		ret = IRQ_HANDLED;
	}
	else if (UART_STAT_REG & UART_TX_EMPTY) {
		m6850_tx_chars(pp);
		ret = IRQ_HANDLED;
	}

	return ret;
}


static unsigned int m6850_tx_empty(struct uart_port *port)
{
	return ((UART_STAT_REG & UART_TX_EMPTY)) ? TIOCSER_TEMT : 0;
}

static unsigned int m6850_get_mctrl(struct uart_port *port)
{
/*
 *	- %TIOCM_CAR	state of DCD signal
 *	- %TIOCM_CTS	state of CTS signal
 *	- %TIOCM_DSR	state of DSR signal
 *	- %TIOCM_RI		state of RI signal
 *
 *	The bit is set if the signal is currently driven active.  If
 *	the port does not support CTS, DCD or DSR, the driver should
 *	indicate that the signal is permanently active. If RI is
 *	not available, the signal should not be indicated as active.
 */
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void m6850_set_mctrl(struct uart_port *port, unsigned int sigs) {}

static void m6850_start_tx(struct uart_port *port)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);

	/* Enable UART TX interrupt */
	pp->ctrl_reg |= UART_TX_IRQ_EN;
	UART_CTRL_REG = pp->ctrl_reg;
}

static void m6850_stop_tx(struct uart_port *port)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);

	/* Disable UART TX interrupt */
	pp->ctrl_reg &= ~UART_TX_IRQ_EN;
	UART_CTRL_REG = pp->ctrl_reg;
}

static void m6850_stop_rx(struct uart_port *port)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);

	/* Disable UART RX interrupt */
	pp->ctrl_reg &= ~UART_RX_IRQ_EN;
	UART_CTRL_REG = pp->ctrl_reg;
}

static void m6850_enable_ms(struct uart_port *port) {}

static void m6850_break_ctl(struct uart_port *port, int break_state) {}

static int m6850_startup(struct uart_port *port)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);

	/* Disable UART interrupts */
	pp->ctrl_reg = UART_CTRL_DEF;
	UART_CTRL_REG = pp->ctrl_reg;


	pr_info("Requested IRQ %d for UART\n", port->irq);
	if (request_irq(port->irq, m6850_interrupt, 0, "M6850UART", port))
		pr_err("M6850UART: unable to attach line %d "
			"interrupt vector=%d\n", port->line, port->irq);

	/* Enable RX interrupts now */
	pp->ctrl_reg |= UART_RX_IRQ_EN;
	UART_CTRL_REG = pp->ctrl_reg;

	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static void m6850_shutdown(struct uart_port *port)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);
	unsigned long flags;

 	spin_lock_irqsave(&port->lock, flags);

	/* Disable UART interrupts */
	pp->ctrl_reg = UART_CTRL_DEF;
	UART_CTRL_REG = pp->ctrl_reg;

	free_irq(port->irq, port);

 	spin_unlock_irqrestore(&port->lock, flags);
}

static void m6850_set_termios(struct uart_port *port, struct ktermios *termios,
	const struct ktermios *old)
{
	/* Nothing to do */
}

static const char * m6850_type(struct uart_port *port)
{
	return (port->type == PORT_M6850) ? "M6850 UART" : NULL;
}

static void m6850_release_port(struct uart_port *port)
{
	/* Nothing to release */
}

static int m6850_request_port(struct uart_port *port)
{
	/* UART always present */
	return 0;
}

static void m6850_config_port(struct uart_port *port, int flags)
{
	struct m6850_uart_port *pp = container_of(port, struct m6850_uart_port, port);

	port->type = PORT_M6850;
	port->fifosize = 1; // No FIFO

	/* Disable UART interrupts */
	pp->ctrl_reg = UART_CTRL_DEF;
	UART_CTRL_REG = pp->ctrl_reg;

}

static int m6850_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if ((ser->type != PORT_UNKNOWN) && (ser->type != PORT_M6850))
		return -EINVAL;
	return 0;
}


#ifdef CONFIG_CONSOLE_POLL

static void m6850_poll_putchar(struct uart_port *port, unsigned char chr)
{
	/*while (!(UART_GET_STATUS(uart) & TXEMPTY))
		cpu_relax();*/

	UART_TX_REG = chr;
}

static int m6850_poll_getchar(struct uart_port *port)
{
	unsigned char chr;

	while ((UART_STAT_REG & UART_RX_FULL))
		cpu_relax();

	chr = UART_RX_REG;
	return chr;
}

#endif


/*
 *	Define the basic serial functions we support.
 */
static const struct uart_ops m6850_uart_ops = {
	.tx_empty		= m6850_tx_empty,
	.get_mctrl		= m6850_get_mctrl,
	.set_mctrl		= m6850_set_mctrl,
	.start_tx		= m6850_start_tx,
	.stop_tx		= m6850_stop_tx,
	.stop_rx		= m6850_stop_rx,
	.enable_ms		= m6850_enable_ms,
	.break_ctl		= m6850_break_ctl,
	.startup		= m6850_startup,
	.shutdown		= m6850_shutdown,
	.set_termios	= m6850_set_termios,
	.type			= m6850_type,
	.release_port	= m6850_release_port,
	.request_port	= m6850_request_port,
	.config_port	= m6850_config_port,
	.verify_port	= m6850_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_put_char	= m6850_poll_putchar,
	.poll_get_char	= m6850_poll_getchar,
#endif
};


#if defined(CONFIG_SERIAL_68KRAY_CONSOLE)

static int m6850_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = UART_BAUD;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if ((co->index < 0) || (co->index >= M6850_MAXPORTS))
		co->index = 0;
	port = &m6850_uart_ports[co->index].port;
	if (port->membase == 0)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static void m6850_console_out(char c)
{
    while (!(UART_STAT_REG & UART_TX_EMPTY));
    UART_TX_REG = (u8)c;
}

static void m6850_console_write(struct console *co, const char *s, unsigned int count)
{
	while (count--) {
		if (*s == '\n')
			m6850_console_out('\r');
		m6850_console_out(*s++);
	}
}

static struct console m6850_console = {
	.name		= M6850_SERIAL_DEV_NAME,
	.write		= m6850_console_write,
	.device		= uart_console_device,
	.setup		= m6850_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &m6850_driver,
};

#endif // CONFIG_SERIAL_68KRAY_CONSOLE


static int m6850_probe(struct platform_device *pdev)
{
	struct platform_uart_68kray *platp = pdev->dev.platform_data;
	struct uart_port *port;
	int i;

	for (i = 0; ((i < M6850_MAXPORTS) && (platp[i].mapbase)); i++) {
		port = &m6850_uart_ports[i].port;
		m6850_uart_ports[i].ctrl_reg = UART_CTRL_DEF;

		port->line = i;
		port->type = PORT_M6850;
		port->fifosize = 1; // No FIFO
		port->mapbase = platp[i].mapbase;
		port->membase = (platp[i].membase) ? platp[i].membase :
			(unsigned char __iomem *) platp[i].mapbase;
		port->iotype = SERIAL_IO_MEM;
		port->irq = platp[i].irq;
		port->uartclk = UART_CLOCK;
		port->ops = &m6850_uart_ops;
		port->flags = UPF_BOOT_AUTOCONF;
		port->dev = &pdev->dev;

		uart_add_one_port(&m6850_driver, port);
	}

	return 0;
}

static int m6850_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver m6850_platform_driver = {
	.probe		= m6850_probe,
	.remove		= m6850_remove,
	.driver		=
	{
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};


static int __init m6850_init(void)
{
	int rc;

	pr_info("M6850 UART serial driver init\n");

	rc = uart_register_driver(&m6850_driver);
	if (rc)
		return rc;

	rc = platform_driver_register(&m6850_platform_driver);
	if (rc)
		return rc;

	return 0;
}

static void __exit m6850_exit(void)
{
	platform_driver_unregister(&m6850_platform_driver);
	uart_unregister_driver(&m6850_driver);
}


module_init(m6850_init);
module_exit(m6850_exit);

MODULE_AUTHOR("Giovanni Scotti");
MODULE_DESCRIPTION("M6850 UART driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:m6850uart");

/****************************************************************************/