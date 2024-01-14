#ifndef _CLOCK_H
#define _CLOCK_H

#include "sbi.h"

unsigned long get_cycles();
void clock_set_next_event();

#endif