// SPDX-License-Identifier: GPL-2.0
void init_dragen2(char *command, int size);
void init_ucsimm(char *command, int size);
struct rtc_time;
int m68328_hwclk(int set, struct rtc_time *t);

#if defined(CONFIG_68KRAY)
void init_68kray(char *command, int size);
#endif
