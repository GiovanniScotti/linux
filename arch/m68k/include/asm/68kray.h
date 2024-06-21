/* include/asm/68kray.h
 * 68Kray board
 */

#ifndef _68KRAY_H_
#define _68KRAY_H_

#include <linux/platform_device.h>
#include <linux/types.h>

#define BYTE_REF(addr) (*((volatile unsigned char*)(addr)))
#define WORD_REF(addr) (*((volatile unsigned short*)(addr)))
#define LONG_REF(addr) (*((volatile unsigned long*)(addr)))

#define UART_BASE_ADDR (0x180000)
#define UART_END_ADDR  (0x1FFFFF)
#define UART_CTRL_REG  BYTE_REF(UART_BASE_ADDR+0) // Write only
#define UART_STAT_REG  BYTE_REF(UART_BASE_ADDR+0) // Read only
#define UART_TX_REG    BYTE_REF(UART_BASE_ADDR+2) // Write only
#define UART_RX_REG    BYTE_REF(UART_BASE_ADDR+2) // Read only

// Default control reg. At 115200@1.8432MHz, divisor is 16x.
//                        CR4        CR3        CR2        CR1        CR0
//                     | ----------- 8N1 ------------- | ------- 16x ------ |
#define UART_CTRL_DEF  ((1 << 4) | (0 << 3) | (1 << 2) | (0 << 1) | (1 << 0))
#define UART_TX_IRQ_EN (1 << 5)	// CR5 = 1, CR6 = 0
#define UART_RX_IRQ_EN (1 << 7)

// UART status bits.
#define UART_RX_FULL   (1 << 0)
#define UART_TX_EMPTY  (1 << 1)
#define UART_INT_REQ   (1 << 7)

#define VIA_BASE_ADDR  (0x200000)
#define VIA_ORA_REG    BYTE_REF(VIA_BASE_ADDR+2)
#define VIA_DDRA_REG   BYTE_REF(VIA_BASE_ADDR+6)

// Clocksource timer.
#define FREQ_HZ        100    // 100 Hz
#define TIMER_IRQ      4

// UART miscellanea.
#define UART_CLOCK     1843200 // 1.8432 MHz
#define UART_BAUD      115200
#define UART_IRQ       2

// 68Kray has a 33-entry jump table for vector indirection.
#define VECTOR_ENTRIES 33


struct jtable_entry {
    uint16_t opcode;
    uint32_t address;
};

struct platform_uart_68kray {
	unsigned long	mapbase;	/* Physical address base */
	void __iomem	*membase;	/* Virtual address if mapped */
	unsigned int	irq;		/* Interrupt vector */
};

#endif
