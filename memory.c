#include <stdio.h>
#include "gb.h"

/* Read one byte from memory at `addr` */
uint8_t gb_memory_readb(struct gb *gb, uint16_t addr) {
     printf("Unsupported read at address 0x%04x\n", addr);
     die();

     return 0xff;
}
